[CmdletBinding()]
param(
	[ValidateRange(30, 1800)]
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
	throw "Headless persistence validation requires UnrealEditor-Cmd.exe: $editorCommand"
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
		[int]$CH02TimeCheckpoint = -1,
		[int]$P5Checkpoint = -1,
		[int]$P3Checkpoint = -1,
		[string]$CatChoice,
		[string]$Ending
	)

	$caseName = if (-not [string]::IsNullOrWhiteSpace($CatChoice)) {
		'{0}_{1}' -f $Mode, $CatChoice
	}
	elseif ($CH02TimeCheckpoint -ge 0) {
		'{0}_{1}' -f $Mode, $CH02TimeCheckpoint
	}
	elseif ($P5Checkpoint -ge 0) {
		'{0}_{1}' -f $Mode, $P5Checkpoint
	}
	elseif ($P3Checkpoint -ge 0) {
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
		'-nosound',
		'-RenderOffscreen',
		'-stdout',
		'-FullStdOutLogOutput',
		"-abslog=$logPath",
		"-UserDir=$userDirectory",
		"-IGRebirthPersistenceProbe=$Mode",
		"-IGRebirthValidationSlot=$Slot"
	)
	if ($Mode -eq 'CatChoiceRead') {
		$arguments += '-IGChapterTwo'
	}
	if ($P3Checkpoint -ge 0) {
		$arguments += "-IGRebirthP3Checkpoint=$P3Checkpoint"
	}
	if ($CH02TimeCheckpoint -ge 0) {
		$arguments +=
			"-IGRebirthCH02TimeCheckpoint=$CH02TimeCheckpoint"
	}
	if ($P5Checkpoint -ge 0) {
		$arguments += "-IGRebirthP5Checkpoint=$P5Checkpoint"
	}
	if (-not [string]::IsNullOrWhiteSpace($CatChoice)) {
		$arguments += "-IGRebirthCatChoiceCase=$CatChoice"
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
	$hash = (Get-FileHash -Algorithm SHA256 -LiteralPath $logPath).Hash
	$saveFiles = @(
		Get-ChildItem `
			-LiteralPath $userDirectory `
			-Recurse `
			-File `
			-Filter "$Slot.sav"
	)
	$preservesSaveSnapshot =
		$Mode -in @(
			'BoundaryBeforeWrite',
			'BoundaryAfterWrite',
			'CatChoiceWrite',
			'CH02TimeWrite',
			'P5Write',
			'P3Write',
			'EndingWrite',
			'EndingCommit')
	$expectsSaveDeletion =
		$Mode -in @(
			'BoundaryBeforeRead',
			'BoundaryAfterRead',
			'CatChoiceRead',
			'CH02TimeRead',
			'P5Read',
			'P3Read',
			'EndingVerify')
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

foreach ($phase in @('Before', 'After')) {
	$slot = "RebirthBoundary_${runId}_$phase"
	Invoke-PersistenceProbe `
		-Mode "Boundary${phase}Write" `
		-Slot $slot `
		-ExpectedMarker (
			"REBIRTH_SPIKE PASS s5_boundary_write phase=$($phase.ToLowerInvariant()) immutable=1")
	$expectedBeatCount = if ($phase -eq 'After') { 1 } else { 0 }
	$expectedSafeStateCount = if ($phase -eq 'After') { 3 } else { 1 }
	Invoke-PersistenceProbe `
		-Mode "Boundary${phase}Read" `
		-Slot $slot `
		-ExpectedMarker (
			"REBIRTH_SPIKE PASS s5_boundary_resume phase=$($phase.ToLowerInvariant()) " +
			"transient_tags=0 safe_states=$expectedSafeStateCount " +
			"beat_count=$expectedBeatCount slot_deleted=1")
}

foreach ($catChoice in @(
	'CapLeft',
	'CapWaited',
	'CupLeft',
	'CupWaited',
	'PassedBy'
)) {
	$slot = "RebirthCatChoice_${runId}_$catChoice"
	Invoke-PersistenceProbe `
		-Mode 'CatChoiceWrite' `
		-Slot $slot `
		-CatChoice $catChoice `
		-ExpectedMarker (
			"REBIRTH_SPIKE PASS s5_cat_write case=$catChoice")
	Invoke-PersistenceProbe `
		-Mode 'CatChoiceRead' `
		-Slot $slot `
		-CatChoice $catChoice `
		-ExpectedMarker (
			"REBIRTH_SPIKE PASS s5_cat_resume case=$catChoice " +
			'ch01_physical=1 ch02_physical=1 slot_deleted=1')
}

for ($checkpoint = 0; $checkpoint -le 1; ++$checkpoint) {
	$slot = "RebirthCH02Time_${runId}_$checkpoint"
	Invoke-PersistenceProbe `
		-Mode 'CH02TimeWrite' `
		-Slot $slot `
		-CH02TimeCheckpoint $checkpoint `
		-ExpectedMarker (
			"REBIRTH_SPIKE PASS s6_ch02_time_write checkpoint=$checkpoint")
	Invoke-PersistenceProbe `
		-Mode 'CH02TimeRead' `
		-Slot $slot `
		-CH02TimeCheckpoint $checkpoint `
		-ExpectedMarker (
			"REBIRTH_SPIKE PASS s6_ch02_time_resume checkpoint=$checkpoint exact=1")
}

for ($checkpoint = 0; $checkpoint -le 3; ++$checkpoint) {
	$slot = "RebirthP5_${runId}_$checkpoint"
	Invoke-PersistenceProbe `
		-Mode 'P5Write' `
		-Slot $slot `
		-P5Checkpoint $checkpoint `
		-ExpectedMarker (
			"REBIRTH_SPIKE PASS s3_p5_write checkpoint=$checkpoint")
	Invoke-PersistenceProbe `
		-Mode 'P5Read' `
		-Slot $slot `
		-P5Checkpoint $checkpoint `
		-ExpectedMarker (
			"REBIRTH_SPIKE PASS s3_p5_resume checkpoint=$checkpoint exact=1")
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
	memoryBoundaryProcessRestarts = 2
	catChoiceProcessRestarts = 5
	ch02TimeProcessRestarts = 2
	p5ProcessRestarts = 4
	p3ProcessRestarts = 7
	endingProcessRestarts = 4
	results = @($results)
}
$summaryPath = Join-Path $EvidenceDirectory 'summary.json'
$summary |
	ConvertTo-Json -Depth 6 |
	Set-Content -LiteralPath $summaryPath -Encoding UTF8
Write-Host (
	"REBIRTH_SPIKE_HARNESS PASS complete boundary=2 cat_choices=5 ch02_time=2 p5=4 p3=7 endings=2 " +
	"summary=$summaryPath")
