[CmdletBinding()]
param(
	[ValidateRange(30, 1800)]
	[int]$TimeoutSeconds = 240,
	[string]$EvidenceDirectory
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$projectRoot = Split-Path -Parent $PSScriptRoot
$projectFile = Join-Path $projectRoot 'IndieGame.uproject'
$resolver = Join-Path $PSScriptRoot 'Resolve-UnrealEditor.ps1'
$runId = '{0}_{1}' -f (
	[DateTime]::UtcNow.ToString('yyyyMMddTHHmmssfffZ')),
	$PID
if ([string]::IsNullOrWhiteSpace($EvidenceDirectory)) {
	$EvidenceDirectory = Join-Path (
		Join-Path $projectRoot 'Saved\Validation\CheckpointAnchors') $runId
}
if (Test-Path -LiteralPath $EvidenceDirectory -PathType Container) {
	if (@(Get-ChildItem -LiteralPath $EvidenceDirectory -Force).Count -gt 0) {
		throw "Evidence directory must be absent or empty: $EvidenceDirectory"
	}
}
else {
	New-Item -ItemType Directory -Path $EvidenceDirectory -Force | Out-Null
}
$userDirectory = Join-Path $EvidenceDirectory 'UserData'
$snapshotDirectory = Join-Path $EvidenceDirectory 'SaveSnapshots'
New-Item -ItemType Directory -Path $userDirectory -Force | Out-Null
New-Item -ItemType Directory -Path $snapshotDirectory -Force | Out-Null

$editorOutput = @(
	& powershell.exe `
		-NoProfile `
		-ExecutionPolicy Bypass `
		-File $resolver `
		-ProjectPath $projectFile `
		-Commandlet 2>&1
)
if ($LASTEXITCODE -ne 0 -or $editorOutput.Count -eq 0) {
	throw 'Unreal Engine resolution failed.'
}
$editorCommand = ([string]$editorOutput[-1]).Trim()
if (-not $editorCommand.EndsWith(
	'UnrealEditor-Cmd.exe',
	[StringComparison]::OrdinalIgnoreCase)) {
	throw "Headless anchor validation requires UnrealEditor-Cmd.exe: $editorCommand"
}

function ConvertTo-ProcessArgument {
	param([Parameter(Mandatory = $true)][string]$Value)
	if ($Value -notmatch '[\s"]') {
		return $Value
	}
	return '"' + $Value.Replace('"', '\"') + '"'
}

$results = [System.Collections.Generic.List[object]]::new()
function Invoke-AnchorProbe {
	param(
		[Parameter(Mandatory = $true)][string]$Mode,
		[Parameter(Mandatory = $true)][string]$AnchorCase,
		[Parameter(Mandatory = $true)][string]$Slot,
		[Parameter(Mandatory = $true)][string]$ExpectedMarker
	)

	$caseName = '{0}_{1}' -f $Mode, $AnchorCase
	$logPath = Join-Path $EvidenceDirectory "$caseName.log"
	$arguments = @(
		$projectFile,
		'-game',
		'-unattended',
		'-nosplash',
		'-nullrhi',
		'-nosound',
		'-RenderOffscreen',
		'-stdout',
		'-FullStdOutLogOutput',
		"-abslog=$logPath",
		"-UserDir=$userDirectory",
		"-IGRebirthPersistenceProbe=$Mode",
		"-IGRebirthValidationSlot=$Slot",
		"-IGRebirthAnchorCase=$AnchorCase"
	)
	$processArguments = @(
		$arguments |
			ForEach-Object { ConvertTo-ProcessArgument -Value $_ }
	)
	$process = Start-Process `
		-FilePath $editorCommand `
		-ArgumentList $processArguments `
		-PassThru `
		-WindowStyle Hidden
	try {
		if (-not $process.WaitForExit($TimeoutSeconds * 1000)) {
			$process.Kill()
			[void]$process.WaitForExit(5000)
			throw "$caseName timed out after $TimeoutSeconds seconds."
		}
		$process.Refresh()
		if ($process.ExitCode -ne 0) {
			throw "$caseName exited with code $($process.ExitCode)."
		}
	}
	finally {
		$process.Dispose()
	}
	if (-not (Test-Path -LiteralPath $logPath -PathType Leaf)) {
		throw "$caseName did not create a log."
	}
	$logText = Get-Content -Raw -Encoding UTF8 -LiteralPath $logPath
	if ($logText.Contains('REBIRTH_SPIKE FAIL') -or
		-not $logText.Contains($ExpectedMarker)) {
		throw "$caseName did not report '$ExpectedMarker'."
	}
	$logHash = (Get-FileHash -Algorithm SHA256 -LiteralPath $logPath).Hash
	$saveFiles = @(
		Get-ChildItem `
			-LiteralPath $userDirectory `
			-Recurse `
			-File `
			-Filter "$Slot.sav"
	)
	$saveSnapshotPath = $null
	$saveSnapshotHash = $null
	$saveDeleted = $false
	if ($Mode -eq 'AnchorWrite') {
		if ($saveFiles.Count -ne 1 -or $saveFiles[0].Length -le 0) {
			throw "$caseName expected one non-empty '$Slot.sav'."
		}
		$saveSnapshotPath = Join-Path $snapshotDirectory "$caseName.sav"
		Copy-Item `
			-LiteralPath $saveFiles[0].FullName `
			-Destination $saveSnapshotPath
		$saveSnapshotHash = (
			Get-FileHash -Algorithm SHA256 -LiteralPath $saveSnapshotPath
		).Hash
	}
	else {
		$saveDeleted = $saveFiles.Count -eq 0
		if (-not $saveDeleted) {
			throw "$caseName left '$Slot.sav' behind after verification."
		}
	}
	[void]$results.Add([pscustomobject]@{
		case = $caseName
		anchor = $AnchorCase
		mode = $Mode
		status = 'PASS'
		logPath = $logPath
		logSha256 = $logHash
		saveSnapshotPath = $saveSnapshotPath
		saveSnapshotSha256 = $saveSnapshotHash
		saveDeleted = $saveDeleted
	})
	Write-Host "REBIRTH_ANCHOR_HARNESS PASS case=$caseName"
}

$anchors = @(
	'CH02Corridor',
	'CH02Store',
	'CH03Apartment',
	'CH03Flood',
	'CH03Roof'
)
foreach ($anchor in $anchors) {
	$slot = "RebirthAnchor_${runId}_$anchor"
	Invoke-AnchorProbe `
		-Mode 'AnchorWrite' `
		-AnchorCase $anchor `
		-Slot $slot `
		-ExpectedMarker (
			"REBIRTH_SPIKE PASS s7_anchor_write case=$anchor map_saved=1")
	Invoke-AnchorProbe `
		-Mode 'AnchorRead' `
		-AnchorCase $anchor `
		-Slot $slot `
		-ExpectedMarker (
			"REBIRTH_SPIKE PASS s7_anchor_resume case=$anchor")
}

$commitSha = @(
	& git -C $projectRoot rev-parse --verify HEAD 2>$null
)[-1].Trim()
$summary = [pscustomobject]@{
	schemaVersion = 1
	runId = $runId
	status = 'PASS'
	commitSha = $commitSha
	editor = $editorCommand
	userDirectory = $userDirectory
	finishedAtUtc = [DateTime]::UtcNow.ToString('o')
	anchorCount = $anchors.Count
	processCount = $results.Count
	mapReentryCount = @($results | Where-Object mode -eq 'AnchorRead').Count
	capsuleClearCount = @($results | Where-Object mode -eq 'AnchorRead').Count
	floorContactCount = @($results | Where-Object mode -eq 'AnchorRead').Count
	results = @($results)
}
$summaryPath = Join-Path $EvidenceDirectory 'summary.json'
$summary |
	ConvertTo-Json -Depth 6 |
	Set-Content -LiteralPath $summaryPath -Encoding UTF8
Write-Host (
	"REBIRTH_ANCHOR_HARNESS PASS complete anchors=$($anchors.Count) " +
	"processes=$($results.Count) summary=$summaryPath")
