[CmdletBinding()]
param(
	[ValidateRange(30, 600)]
	[int]$TimeoutSeconds = 180,
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
		Join-Path $projectRoot 'Saved\Validation\RebirthSpikes') $runId
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
New-Item -ItemType Directory -Path $userDirectory -Force | Out-Null
$snapshotDirectory = Join-Path $EvidenceDirectory 'SaveSnapshots'
New-Item -ItemType Directory -Path $snapshotDirectory -Force | Out-Null

$editorOutput = @(
	& powershell.exe `
		-NoProfile `
		-ExecutionPolicy Bypass `
		-File $resolver `
		-ProjectPath $projectFile 2>&1
)
if ($LASTEXITCODE -ne 0 -or $editorOutput.Count -eq 0) {
	throw 'Unreal Engine resolution failed.'
}
$editor = ([string]$editorOutput[-1]).Trim()
$editorDirectory = Split-Path -Parent $editor
$editorCommand = Join-Path $editorDirectory 'UnrealEditor-Cmd.exe'
if (-not (Test-Path -LiteralPath $editorCommand -PathType Leaf)) {
	$editorCommand = $editor
}

function ConvertTo-ProcessArgument {
	param([Parameter(Mandatory = $true)][string]$Value)
	if ($Value -notmatch '[\s"]') {
		return $Value
	}
	return '"' + $Value.Replace('"', '\"') + '"'
}

$results = [System.Collections.Generic.List[object]]::new()
function Invoke-PersistenceProbe {
	param(
		[Parameter(Mandatory = $true)][string]$Mode,
		[Parameter(Mandatory = $true)][string]$Slot,
		[Parameter(Mandatory = $true)][string]$ExpectedMarker,
		[int]$P3Checkpoint = -1,
		[string]$Ending
	)

	$caseName = if ($P3Checkpoint -ge 0) {
		'{0}_{1}' -f $Mode, $P3Checkpoint
	}
	elseif (-not [string]::IsNullOrWhiteSpace($Ending)) {
		'{0}_{1}' -f $Mode, $Ending
	}
	else {
		$Mode
	}
	$logPath = Join-Path $EvidenceDirectory "$caseName.log"
	$arguments = @(
		$projectFile,
		'-game',
		'-unattended',
		'-nosplash',
		'-nullrhi',
		'-stdout',
		'-FullStdOutLogOutput',
		"-abslog=$logPath",
		"-UserDir=$userDirectory",
		"-IGRebirthPersistenceProbe=$Mode",
		"-IGRebirthValidationSlot=$Slot"
	)
	if ($P3Checkpoint -ge 0) {
		$arguments += "-IGRebirthP3Checkpoint=$P3Checkpoint"
	}
	if (-not [string]::IsNullOrWhiteSpace($Ending)) {
		$arguments += "-IGRebirthEnding=$Ending"
	}
	$processArguments = @(
		$arguments |
			ForEach-Object { ConvertTo-ProcessArgument -Value $_ }
	)
	$process = Start-Process `
		-FilePath $editorCommand `
		-ArgumentList $processArguments `
		-PassThru `
		-NoNewWindow
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
	$hash = (Get-FileHash -Algorithm SHA256 -LiteralPath $logPath).Hash
	$saveFiles = @(
		Get-ChildItem `
			-LiteralPath $userDirectory `
			-Recurse `
			-File `
			-Filter "$Slot.sav"
	)
	$preservesSaveSnapshot =
		$Mode -in @('P3Write', 'EndingWrite', 'EndingCommit')
	$expectsSaveDeletion =
		$Mode -in @('P3Read', 'EndingVerify')
	$saveSnapshotPath = $null
	$saveSnapshotHash = $null
	$saveDeleted = $false
	if ($preservesSaveSnapshot) {
		if ($saveFiles.Count -ne 1 -or $saveFiles[0].Length -le 0) {
			throw (
				"$caseName expected one non-empty '$Slot.sav' below " +
				"$userDirectory, found $($saveFiles.Count).")
		}
		$saveSnapshotPath =
			Join-Path $snapshotDirectory "$caseName.sav"
		Copy-Item `
			-LiteralPath $saveFiles[0].FullName `
			-Destination $saveSnapshotPath
		$saveSnapshotHash = (
			Get-FileHash `
				-Algorithm SHA256 `
				-LiteralPath $saveSnapshotPath
		).Hash
	}
	elseif ($expectsSaveDeletion) {
		$saveDeleted = $saveFiles.Count -eq 0
		if (-not $saveDeleted) {
			throw "$caseName left '$Slot.sav' behind after verification."
		}
	}
	[void]$results.Add([pscustomobject]@{
		case = $caseName
		status = 'PASS'
		logPath = $logPath
		logSha256 = $hash
		saveSnapshotPath = $saveSnapshotPath
		saveSnapshotSha256 = $saveSnapshotHash
		saveDeleted = $saveDeleted
	})
	Write-Host "REBIRTH_SPIKE_HARNESS PASS case=$caseName"
}

for ($checkpoint = 0; $checkpoint -le 6; ++$checkpoint) {
	$slot = "RebirthP3_${runId}_$checkpoint"
	Invoke-PersistenceProbe `
		-Mode 'P3Write' `
		-Slot $slot `
		-P3Checkpoint $checkpoint `
		-ExpectedMarker (
			"REBIRTH_SPIKE PASS s3_p3_write checkpoint=$checkpoint")
	Invoke-PersistenceProbe `
		-Mode 'P3Read' `
		-Slot $slot `
		-P3Checkpoint $checkpoint `
		-ExpectedMarker (
			"REBIRTH_SPIKE PASS s3_p3_resume checkpoint=$checkpoint exact=1")
}

foreach ($ending in @('A', 'B')) {
	$slot = "RebirthEnding_${runId}_$ending"
	Invoke-PersistenceProbe `
		-Mode 'EndingWrite' `
		-Slot $slot `
		-Ending $ending `
		-ExpectedMarker (
			"REBIRTH_SPIKE PASS s4_ending_write ending=$ending")
	Invoke-PersistenceProbe `
		-Mode 'EndingCommit' `
		-Slot $slot `
		-Ending $ending `
		-ExpectedMarker (
			"REBIRTH_SPIKE PASS s4_common_commit ending=$ending once=1")
	Invoke-PersistenceProbe `
		-Mode 'EndingVerify' `
		-Slot $slot `
		-Ending $ending `
		-ExpectedMarker (
			"REBIRTH_SPIKE PASS s4_common_restore ending=$ending " +
			'common_once=1 branch_exclusive=1')
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
	p3ProcessRestarts = 7
	endingProcessRestarts = 4
	results = @($results)
}
$summaryPath = Join-Path $EvidenceDirectory 'summary.json'
$summary |
	ConvertTo-Json -Depth 6 |
	Set-Content -LiteralPath $summaryPath -Encoding UTF8
Write-Host (
	"REBIRTH_SPIKE_HARNESS PASS complete p3=7 endings=2 " +
	"summary=$summaryPath")
