[CmdletBinding()]
param(
	[switch]$StaticOnly,
	[switch]$SkipDevelopmentBuild,
	[switch]$SkipMapCheck,
	[switch]$SkipRuntimeValidation,
	[switch]$SkipShippingPackage,
	[ValidateRange(30, 1800)]
	[int]$RuntimeTimeoutSeconds = 240,
	[string]$ArchiveDirectory
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$projectRoot = Split-Path -Parent $PSScriptRoot
$projectFile = Join-Path $projectRoot 'IndieGame.uproject'
$resolverScript = Join-Path $PSScriptRoot 'Resolve-UnrealEditor.ps1'
$validationScript = Join-Path $PSScriptRoot 'Validate-Project.ps1'
$resultDirectory = Join-Path $projectRoot 'Saved\Validation\RebirthRelease'
$runId = '{0}_{1}' -f (
	[DateTime]::UtcNow.ToString('yyyyMMddTHHmmssfffZ')),
	$PID
$runDirectory = Join-Path $resultDirectory $runId
$summaryPath = Join-Path $runDirectory 'summary.json'
$latestSummaryPath = Join-Path $resultDirectory 'Latest.json'
$startedAtUtc = [DateTime]::UtcNow
$stepResults = [System.Collections.Generic.List[object]]::new()
$allStepNames = @(
	'static_contracts',
	'engine_resolution',
	'source_state_pre',
	'development_editor_build',
	'development_game_build',
	'map_check',
	'runtime_ending_a',
	'runtime_ending_b',
	'shipping_package',
	'source_state_post'
)
$activeStepName = $null
$activeStepLogPath = $null
$engineAssociation = $null
$engineBuildVersion = $null
$resolvedEditorForSummary = $null
$initialSourceState = $null
$shippingArchiveManifestPath = $null
if ([string]::IsNullOrWhiteSpace($ArchiveDirectory)) {
	$ArchiveDirectory = Join-Path (
		Join-Path $projectRoot 'Saved\StagedBuilds\RebirthShipping') $runId
}

function Add-StepResult {
	param(
		[Parameter(Mandatory = $true)]
		[string]$Name,
		[Parameter(Mandatory = $true)]
		[ValidateSet('PASS', 'FAIL', 'BLOCKED', 'NOT_RUN')]
		[string]$Status,
		[string]$Detail,
		[string]$LogPath
	)

	$logHash = $null
	$logHashError = $null
	if (-not [string]::IsNullOrWhiteSpace($LogPath) -and
		(Test-Path -LiteralPath $LogPath -PathType Leaf)) {
		try {
			$logHash = (
				Get-FileHash -Algorithm SHA256 -LiteralPath $LogPath
			).Hash
		}
		catch {
			$logHashError = $_.Exception.Message
		}
	}
	if ($Status -eq 'PASS' -and
		-not [string]::IsNullOrWhiteSpace($LogPath) -and
		[string]::IsNullOrWhiteSpace($logHash)) {
		throw "PASS evidence log could not be hashed: $LogPath ($logHashError)"
	}
	$effectiveDetail = $Detail
	if (-not [string]::IsNullOrWhiteSpace($logHashError)) {
		$effectiveDetail += " Log hash unavailable: $logHashError"
	}
	[void]$stepResults.Add([pscustomobject]@{
		name = $Name
		status = $Status
		detail = $effectiveDetail
		logPath = $LogPath
		logSha256 = $logHash
	})
}

function Set-ActiveStep {
	param(
		[Parameter(Mandatory = $true)][string]$Name,
		[string]$LogPath
	)
	$script:activeStepName = $Name
	$script:activeStepLogPath = $LogPath
}

function Complete-ActiveStep {
	param(
		[Parameter(Mandatory = $true)]
		[ValidateSet('PASS', 'FAIL', 'BLOCKED', 'NOT_RUN')]
		[string]$Status,
		[string]$Detail
	)
	Add-StepResult `
		-Name $script:activeStepName `
		-Status $Status `
		-Detail $Detail `
		-LogPath $script:activeStepLogPath
	$script:activeStepName = $null
	$script:activeStepLogPath = $null
}

function Add-MissingStepResults {
	param([Parameter(Mandatory = $true)][string]$Detail)

	$recordedNames = @(
		$script:stepResults | ForEach-Object { $_.name }
	)
	foreach ($stepName in $script:allStepNames) {
		if ($recordedNames -notcontains $stepName) {
			Add-StepResult `
				-Name $stepName `
				-Status 'NOT_RUN' `
				-Detail $Detail `
				-LogPath $null
		}
	}
}

function Get-SourceState {
	if (-not (Get-Command git -ErrorAction SilentlyContinue)) {
		return [pscustomobject]@{
			available = $false
			commitSha = $null
			worktreeClean = $false
			worktreeStatus = @('git executable was not found')
		}
	}

	$headOutput = @(
		& git -C $projectRoot rev-parse --verify HEAD 2>$null
	)
	$headExitCode = $LASTEXITCODE
	$statusOutput = @(
		& git -C $projectRoot status --porcelain=v1 --untracked-files=all 2>$null
	)
	$statusExitCode = $LASTEXITCODE
	$commitSha = if ($headExitCode -eq 0 -and $headOutput.Count -gt 0) {
		([string]$headOutput[-1]).Trim()
	}
	else {
		$null
	}
	$statusLines = @(
		$statusOutput |
			ForEach-Object { ([string]$_).TrimEnd() } |
			Where-Object { -not [string]::IsNullOrWhiteSpace($_) }
	)
	return [pscustomobject]@{
		available = $headExitCode -eq 0 -and $statusExitCode -eq 0
		commitSha = $commitSha
		worktreeClean =
			$headExitCode -eq 0 -and
			$statusExitCode -eq 0 -and
			$statusLines.Count -eq 0
		worktreeStatus = $statusLines
	}
}

function Test-SourceStateUnchanged {
	param(
		[Parameter(Mandatory = $true)]$Initial,
		[Parameter(Mandatory = $true)]$Current
	)

	$initialStatus = @($Initial.worktreeStatus) -join "`n"
	$currentStatus = @($Current.worktreeStatus) -join "`n"
	return $Initial.available `
		-and $Current.available `
		-and -not [string]::IsNullOrWhiteSpace($Initial.commitSha) `
		-and $Initial.commitSha -eq $Current.commitSha `
		-and $initialStatus -ceq $currentStatus
}

function Test-CleanSourceStateLocked {
	param(
		[Parameter(Mandatory = $true)]$Initial,
		[Parameter(Mandatory = $true)]$Current
	)

	return $Initial.worktreeClean `
		-and $Current.worktreeClean `
		-and (Test-SourceStateUnchanged -Initial $Initial -Current $Current)
}

function Write-ShippingArchiveManifest {
	if (-not (Test-Path -LiteralPath $ArchiveDirectory -PathType Container)) {
		throw "Shipping archive directory was not created: $ArchiveDirectory"
	}
	$resolvedArchive = (Resolve-Path -LiteralPath $ArchiveDirectory).Path.TrimEnd(
		[char[]]@('\', '/'))
	$archiveFiles = @(
		Get-ChildItem -LiteralPath $resolvedArchive -Recurse -File |
			Sort-Object FullName
	)
	if ($archiveFiles.Count -eq 0) {
		throw "Shipping archive is empty: $resolvedArchive"
	}
	$productExecutables = @(
		$archiveFiles |
			Where-Object {
				$_.Name -ieq 'IndieGame.exe' -and $_.Length -gt 0
			}
	)
	if ($productExecutables.Count -eq 0) {
		throw (
			'Shipping archive has no non-empty IndieGame.exe product ' +
			"executable: $resolvedArchive")
	}
	foreach ($requiredExtension in @('.pak', '.utoc', '.ucas')) {
		$requiredArtifacts = @(
			$archiveFiles |
				Where-Object {
					$_.Extension -ieq $requiredExtension -and $_.Length -gt 0
				}
		)
		if ($requiredArtifacts.Count -eq 0) {
			throw (
				"Shipping archive has no non-empty $requiredExtension " +
				"artifact: $resolvedArchive")
		}
	}

	$artifacts = @(
		foreach ($archiveFile in $archiveFiles) {
			$relativePath = $archiveFile.FullName.Substring(
				$resolvedArchive.Length).TrimStart([char[]]@('\', '/'))
			[pscustomobject]@{
				path = $relativePath
				sizeBytes = $archiveFile.Length
				sha256 = (
					Get-FileHash -Algorithm SHA256 -LiteralPath $archiveFile.FullName
				).Hash
			}
		}
	)
	$script:shippingArchiveManifestPath = Join-Path (
		$script:runDirectory) 'ShippingArchiveManifest.json'
	$manifest = [pscustomobject]@{
		schemaVersion = 1
		generatedAtUtc = [DateTime]::UtcNow.ToString('o')
		commitSha = $initialSourceState.commitSha
		engineBuildVersion = $engineBuildVersion
		projectFile = $projectFile
		archiveDirectory = $resolvedArchive
		fileCount = $artifacts.Count
		artifacts = $artifacts
	}
	$manifest |
		ConvertTo-Json -Depth 6 |
		Set-Content `
			-LiteralPath $script:shippingArchiveManifestPath `
			-Encoding UTF8
}

function Write-RunSummary {
	param(
		[Parameter(Mandatory = $true)]
		[ValidateSet('PASS', 'PARTIAL', 'FAIL', 'BLOCKED')]
		[string]$Status,
		[string]$Detail
	)

	if (-not (Test-Path -LiteralPath $runDirectory)) {
		New-Item -ItemType Directory -Path $runDirectory -Force | Out-Null
	}
	$currentSourceState = Get-SourceState
	$initialCommitSha = if ($null -ne $initialSourceState) {
		$initialSourceState.commitSha
	}
	else {
		$null
	}
	$initialWorktreeClean = $null -ne $initialSourceState `
		-and $initialSourceState.worktreeClean
	$initialWorktreeStatus = if ($null -ne $initialSourceState) {
		@($initialSourceState.worktreeStatus)
	}
	else {
		@()
	}
	$shippingManifestHash = $null
	if (-not [string]::IsNullOrWhiteSpace($shippingArchiveManifestPath) -and
		(Test-Path -LiteralPath $shippingArchiveManifestPath -PathType Leaf)) {
		$shippingManifestHash = (
			Get-FileHash `
				-Algorithm SHA256 `
				-LiteralPath $shippingArchiveManifestPath
			).Hash
	}
	$sourceStateUnchanged = $null -ne $initialSourceState `
		-and (Test-SourceStateUnchanged `
			-Initial $initialSourceState `
			-Current $currentSourceState)
	$cleanSourceStateLocked = $null -ne $initialSourceState `
		-and (Test-CleanSourceStateLocked `
			-Initial $initialSourceState `
			-Current $currentSourceState)
	if ($Status -eq 'PASS' -and -not $cleanSourceStateLocked) {
		throw (
			'Refusing to write PASS because the final Git source state is not ' +
			'the clean commit locked at startup.')
	}
	if ($Status -eq 'PARTIAL' -and
		-not $StaticOnly -and
		-not $cleanSourceStateLocked) {
		throw (
			'Refusing to write a Full/PARTIAL result because the final Git ' +
			'source state is not the clean commit locked at startup.')
	}
	$summary = [pscustomobject]@{
		schemaVersion = 2
		runId = $runId
		status = $Status
		mode = if ($StaticOnly) { 'StaticOnly' } else { 'Full' }
		detail = $Detail
		startedAtUtc = $startedAtUtc.ToString('o')
		finishedAtUtc = [DateTime]::UtcNow.ToString('o')
		commandLine = [Environment]::CommandLine
		commitSha = $initialCommitSha
		worktreeClean = $initialWorktreeClean
		worktreeStatus = $initialWorktreeStatus
		finalCommitSha = $currentSourceState.commitSha
		finalWorktreeClean = $currentSourceState.worktreeClean
		sourceStateUnchanged = $sourceStateUnchanged
		sourceChangedDuringRun = -not $sourceStateUnchanged
		cleanSourceStateLocked = $cleanSourceStateLocked
		projectFile = $projectFile
		engineAssociation = $engineAssociation
		engineBuildVersion = $engineBuildVersion
		resolvedEditor = $resolvedEditorForSummary
		archiveDirectory = $ArchiveDirectory
		shippingArchiveManifest = $shippingArchiveManifestPath
		shippingArchiveManifestSha256 = $shippingManifestHash
		automatedReleaseCandidateEligible =
			$Status -eq 'PASS' `
			-and $cleanSourceStateLocked `
			-and -not (
				$stepResults | Where-Object { $_.status -ne 'PASS' })
		releaseEligible = $false
		releaseEligibilityDetail = (
			'Human G3 paths, first-time playtests, final content/accessibility, ' +
			'and clean-machine package execution remain separate gates.')
		steps = @($stepResults)
	}
	$json = $summary | ConvertTo-Json -Depth 8
	$summaryTempPath = "$summaryPath.tmp-$PID"
	$latestTempPath = "$latestSummaryPath.tmp-$runId"
	Set-Content -LiteralPath $summaryTempPath -Value $json -Encoding UTF8
	Move-Item -LiteralPath $summaryTempPath -Destination $summaryPath -Force
	Set-Content -LiteralPath $latestTempPath -Value $json -Encoding UTF8
	Move-Item `
		-LiteralPath $latestTempPath `
		-Destination $latestSummaryPath `
		-Force
	Write-Host "REBIRTH_RELEASE_HARNESS SUMMARY status=$Status path=$summaryPath"
}

function Invoke-NativeChecked {
	param(
		[Parameter(Mandatory = $true)]
		[string]$FilePath,
		[Parameter(Mandatory = $true)]
		[string[]]$Arguments,
		[Parameter(Mandatory = $true)]
		[string]$Label,
		[string]$LogPath
	)

	Write-Host "[$Label] $FilePath $($Arguments -join ' ')"
	$previousNativeErrorPreference = $ErrorActionPreference
	$ErrorActionPreference = 'Continue'
	$exitCode = $null
	try {
		if ([string]::IsNullOrWhiteSpace($LogPath)) {
			& $FilePath @Arguments
		}
		else {
			& $FilePath @Arguments 2>&1 |
				Tee-Object -FilePath $LogPath
		}
		$exitCode = $LASTEXITCODE
	}
	finally {
		$ErrorActionPreference = $previousNativeErrorPreference
	}
	if ($null -eq $exitCode) {
		throw "$Label did not report a native process exit code."
	}
	if ($exitCode -ne 0) {
		throw "$Label failed with exit code $exitCode."
	}
	if (-not [string]::IsNullOrWhiteSpace($LogPath)) {
		if (-not (Test-Path -LiteralPath $LogPath -PathType Leaf)) {
			throw "$Label did not create its evidence log: $LogPath"
		}
		$logFile = Get-Item -LiteralPath $LogPath
		if ($logFile.Length -le 0) {
			throw "$Label created an empty evidence log: $LogPath"
		}
		if ($logFile.LastWriteTimeUtc -lt $startedAtUtc.AddSeconds(-2)) {
			throw "$Label evidence log is stale: $LogPath"
		}
	}
}

function ConvertTo-ProcessArgument {
	param([Parameter(Mandatory = $true)][string]$Value)

	if ($Value -notmatch '[\s"]') {
		return $Value
	}
	return '"' + $Value.Replace('"', '\"') + '"'
}

function Assert-ReleaseLog {
	param(
		[Parameter(Mandatory = $true)]
		[string]$LogPath,
		[Parameter(Mandatory = $true)]
		[ValidateSet('A', 'B')]
		[string]$Ending
	)

	if (-not (Test-Path -LiteralPath $LogPath -PathType Leaf)) {
		throw "Runtime validation did not create its log: $LogPath"
	}
	$logText = Get-Content -Raw -Encoding UTF8 -LiteralPath $LogPath
	if ($logText.Contains('REBIRTH_RELEASE FAIL') -or
		$logText.Contains('REBIRTH_GREYBOX FAIL')) {
		throw "Runtime validation reported FAIL. Check $LogPath."
	}
	foreach ($requiredMarker in @(
		'REBIRTH_GREYBOX PASS',
		'REBIRTH_RELEASE PASS collision_route',
		'REBIRTH_RELEASE PASS audio_queue',
		'REBIRTH_RELEASE PASS p3_p5',
		'REBIRTH_RELEASE PASS savegame_v3',
		"REBIRTH_RELEASE PASS ending=$Ending",
		"REBIRTH_RELEASE PASS complete ending=$Ending"
	)) {
		if (-not $logText.Contains($requiredMarker)) {
			throw "Runtime validation marker is missing: $requiredMarker. Check $LogPath."
		}
	}
}

function Invoke-RebirthRuntimeCase {
	param(
		[Parameter(Mandatory = $true)]
		[string]$EditorCommand,
		[Parameter(Mandatory = $true)]
		[ValidateSet('A', 'B')]
		[string]$Ending
	)

	if (-not (Test-Path -LiteralPath $runDirectory)) {
		New-Item -ItemType Directory -Path $runDirectory -Force | Out-Null
	}
	$logPath = Join-Path $runDirectory "RebirthRelease_Ending$Ending.log"
	if (Test-Path -LiteralPath $logPath) {
		Remove-Item -LiteralPath $logPath -Force
	}

	$runtimeArguments = @(
		$projectFile,
		'-game',
		'-unattended',
		'-nosplash',
		'-nullrhi',
		'-stdout',
		'-FullStdOutLogOutput',
		"-abslog=$logPath",
		'-IGChapterThree',
		'-IGRebirthGreybox',
		'-IGRebirthReleaseValidation',
		"-IGRebirthEnding=$Ending"
	)
	$processArguments = @(
		$runtimeArguments |
			ForEach-Object { ConvertTo-ProcessArgument -Value $_ }
	)
	Write-Host "[Runtime ending $Ending] $EditorCommand $($runtimeArguments -join ' ')"
	$process = Start-Process `
		-FilePath $EditorCommand `
		-ArgumentList $processArguments `
		-PassThru `
		-NoNewWindow
	try {
		if (-not $process.WaitForExit($RuntimeTimeoutSeconds * 1000)) {
			$process.Kill()
			[void]$process.WaitForExit(5000)
			throw "Runtime ending $Ending timed out after $RuntimeTimeoutSeconds seconds."
		}
		$process.Refresh()
		if ($process.ExitCode -ne 0) {
			throw "Runtime ending $Ending exited with code $($process.ExitCode). Check $logPath."
		}
	}
	finally {
		$process.Dispose()
	}
	Assert-ReleaseLog -LogPath $logPath -Ending $Ending
	Write-Host "REBIRTH_RELEASE_HARNESS PASS runtime ending=$Ending log=$logPath"
}

function Invoke-RebirthMapCheck {
	param(
		[Parameter(Mandatory = $true)]
		[string]$EditorCommand,
		[Parameter(Mandatory = $true)]
		[string]$LogPath
	)

	if (Test-Path -LiteralPath $LogPath) {
		Remove-Item -LiteralPath $LogPath -Force
	}
	$mapCheckArguments = @(
		$projectFile,
		'/Game/Maps/Prologue_Morning',
		'-unattended',
		'-nosplash',
		'-nullrhi',
		'-stdout',
		'-FullStdOutLogOutput',
		"-abslog=$LogPath",
		'-ExecCmds=MAP CHECK,QUIT_EDITOR'
	)
	$processArguments = @(
		$mapCheckArguments |
			ForEach-Object { ConvertTo-ProcessArgument -Value $_ }
	)
	Write-Host "[Map Check] $EditorCommand $($mapCheckArguments -join ' ')"
	$process = Start-Process `
		-FilePath $EditorCommand `
		-ArgumentList $processArguments `
		-PassThru `
		-NoNewWindow
	try {
		if (-not $process.WaitForExit($RuntimeTimeoutSeconds * 1000)) {
			$process.Kill()
			[void]$process.WaitForExit(5000)
			throw "Map Check timed out after $RuntimeTimeoutSeconds seconds."
		}
		$process.Refresh()
		if ($process.ExitCode -ne 0) {
			throw "Map Check exited with code $($process.ExitCode). Check $LogPath."
		}
	}
	finally {
		$process.Dispose()
	}

	if (-not (Test-Path -LiteralPath $LogPath -PathType Leaf)) {
		throw "Map Check did not create its log: $LogPath"
	}
	$logText = Get-Content -Raw -Encoding UTF8 -LiteralPath $LogPath
	if ($logText -match '(?im)MapCheck:.*Error:' -or
		$logText -match '(?im)Map check failed' -or
		$logText -notmatch '(?im)Map check complete:\s*0 Error') {
		throw "Map Check did not complete with zero errors. Check $LogPath."
	}
	Write-Host (
		'REBIRTH_RELEASE_HARNESS PASS map_check ' +
		'map=/Game/Maps/Prologue_Morning')
}

trap {
	$failureMessage = $_.Exception.Message
	if (-not [string]::IsNullOrWhiteSpace($script:activeStepName)) {
		Complete-ActiveStep -Status 'FAIL' -Detail $failureMessage
	}
	try {
		Add-MissingStepResults `
			-Detail "Stopped after failure: $failureMessage"
		Write-RunSummary -Status 'FAIL' -Detail $failureMessage
	}
	catch {
		[Console]::Error.WriteLine(
			"Failed to write release summary: $($_.Exception.Message)")
	}
	[Console]::Error.WriteLine(
		"REBIRTH_RELEASE_HARNESS FAIL $failureMessage")
	exit 1
}

if (-not (Test-Path -LiteralPath $runDirectory)) {
	New-Item -ItemType Directory -Path $runDirectory -Force | Out-Null
}
if (-not (Test-Path -LiteralPath $projectFile -PathType Leaf)) {
	throw "Project descriptor was not found: $projectFile"
}
if (-not (Test-Path -LiteralPath $resolverScript -PathType Leaf)) {
	throw "Unreal resolver was not found: $resolverScript"
}
if (-not (Test-Path -LiteralPath $validationScript -PathType Leaf)) {
	throw "Static project validator was not found: $validationScript"
}
$projectDescriptor =
	Get-Content -Raw -Encoding UTF8 -LiteralPath $projectFile |
	ConvertFrom-Json
$engineAssociation = [string]$projectDescriptor.EngineAssociation
$initialSourceState = Get-SourceState
if (-not $StaticOnly `
	-and $SkipDevelopmentBuild `
	-and (-not $SkipMapCheck -or -not $SkipRuntimeValidation)) {
	throw (
		'SkipDevelopmentBuild cannot be combined with Map Check or runtime ' +
		'validation without a commit-bound prebuilt manifest. Run the ' +
		'Development builds or skip both dependent stages.')
}

$staticContractsLog = Join-Path $runDirectory 'StaticContracts.log'
Set-ActiveStep -Name 'static_contracts' -LogPath $staticContractsLog
Invoke-NativeChecked `
	-FilePath 'powershell.exe' `
	-Arguments @(
		'-NoProfile',
		'-ExecutionPolicy',
		'Bypass',
		'-File',
		$validationScript
	) `
	-Label 'Static contracts' `
	-LogPath $staticContractsLog
Complete-ActiveStep `
	-Status 'PASS' `
	-Detail 'PowerShell and REBIRTH static contracts passed.'
if ($StaticOnly) {
	Add-MissingStepResults -Detail 'StaticOnly requested.'
	Write-RunSummary `
		-Status 'PARTIAL' `
		-Detail 'Static contracts passed; Unreal stages were intentionally not run.'
	Write-Host (
		'REBIRTH_RELEASE_HARNESS STATIC_ONLY COMPLETE status=PARTIAL ' +
		'(Unreal build/map/runtime/package not executed).')
	exit 0
}

$engineResolutionLog = Join-Path $runDirectory 'EngineResolution.log'
Set-ActiveStep `
	-Name 'engine_resolution' `
	-LogPath $engineResolutionLog
$previousErrorActionPreference = $ErrorActionPreference
$ErrorActionPreference = 'Continue'
$resolvedEditorOutput = @(
	& powershell.exe `
		-NoProfile `
		-ExecutionPolicy Bypass `
		-File $resolverScript `
		-ProjectPath $projectFile 2>&1
)
$resolverExitCode = $LASTEXITCODE
$ErrorActionPreference = $previousErrorActionPreference
$resolvedEditorOutput |
	ForEach-Object { [string]$_ } |
	Set-Content -LiteralPath $engineResolutionLog -Encoding UTF8
if ($resolverExitCode -ne 0 -or $resolvedEditorOutput.Count -eq 0) {
	$blockedDetail =
		"Unreal Engine $engineAssociation was not found. " +
		'Install it or set IG_UNREAL_EDITOR to UnrealEditor.exe.'
	Complete-ActiveStep -Status 'BLOCKED' -Detail $blockedDetail
	Add-MissingStepResults -Detail 'Engine resolution was blocked.'
	Write-RunSummary -Status 'BLOCKED' -Detail $blockedDetail
	[Console]::Error.WriteLine(
		"REBIRTH_RELEASE_HARNESS BLOCKED $blockedDetail")
	exit 2
}
$editorExecutable = ([string]$resolvedEditorOutput[-1]).Trim()
if (-not (Test-Path -LiteralPath $editorExecutable -PathType Leaf)) {
	throw "Resolved Unreal Editor does not exist: $editorExecutable"
}
$resolvedEditorForSummary = $editorExecutable

$win64Directory = Split-Path -Parent $editorExecutable
$binariesDirectory = Split-Path -Parent $win64Directory
$engineDirectory = Split-Path -Parent $binariesDirectory
$buildVersionPath = Join-Path $engineDirectory 'Build\Build.version'
if (Test-Path -LiteralPath $buildVersionPath -PathType Leaf) {
	$buildVersion =
		Get-Content -Raw -Encoding UTF8 -LiteralPath $buildVersionPath |
		ConvertFrom-Json
	$engineBuildVersion = '{0}.{1}.{2}-{3}' -f
		$buildVersion.MajorVersion,
		$buildVersion.MinorVersion,
		$buildVersion.PatchVersion,
		$buildVersion.Changelist
}
Complete-ActiveStep `
	-Status 'PASS' `
	-Detail "Resolved $editorExecutable ($engineBuildVersion)."

Set-ActiveStep -Name 'source_state_pre'
$sourceStateBeforeBuild = Get-SourceState
if (-not (Test-CleanSourceStateLocked `
		-Initial $initialSourceState `
		-Current $sourceStateBeforeBuild)) {
	$sourceBlockedDetail =
		'Release validation requires one clean, unchanged Git commit. ' +
		'Commit or revert all tracked and untracked changes, then rerun.'
	Complete-ActiveStep -Status 'BLOCKED' -Detail $sourceBlockedDetail
	Add-MissingStepResults -Detail 'Source-state gate was blocked.'
	Write-RunSummary -Status 'BLOCKED' -Detail $sourceBlockedDetail
	[Console]::Error.WriteLine(
		"REBIRTH_RELEASE_HARNESS BLOCKED $sourceBlockedDetail")
	exit 2
}
Complete-ActiveStep `
	-Status 'PASS' `
	-Detail "Clean source state locked to $($initialSourceState.commitSha)."

$buildScript = Join-Path $engineDirectory 'Build\BatchFiles\Build.bat'
$automationScript = Join-Path $engineDirectory 'Build\BatchFiles\RunUAT.bat'
$editorCommand = Join-Path $win64Directory 'UnrealEditor-Cmd.exe'
if (-not (Test-Path -LiteralPath $editorCommand -PathType Leaf)) {
	$editorCommand = $editorExecutable
}

if (-not $SkipDevelopmentBuild) {
	$editorBuildLog = Join-Path $runDirectory 'DevelopmentEditorBuild.log'
	Set-ActiveStep `
		-Name 'development_editor_build' `
		-LogPath $editorBuildLog
	if (-not (Test-Path -LiteralPath $buildScript -PathType Leaf)) {
		throw "Unreal Build.bat was not found: $buildScript"
	}
	Invoke-NativeChecked `
		-FilePath $buildScript `
		-Arguments @(
			'IndieGameEditor',
			'Win64',
			'Development',
			$projectFile,
			'-WaitMutex',
			'-NoHotReloadFromIDE'
		) `
		-Label 'Development editor build' `
		-LogPath $editorBuildLog
	Complete-ActiveStep -Status 'PASS' -Detail 'IndieGameEditor Development built.'

	$gameBuildLog = Join-Path $runDirectory 'DevelopmentGameBuild.log'
	Set-ActiveStep `
		-Name 'development_game_build' `
		-LogPath $gameBuildLog
	Invoke-NativeChecked `
		-FilePath $buildScript `
		-Arguments @(
			'IndieGame',
			'Win64',
			'Development',
			$projectFile,
			'-WaitMutex'
		) `
		-Label 'Development game build' `
		-LogPath $gameBuildLog
	Complete-ActiveStep -Status 'PASS' -Detail 'IndieGame Development built.'
	Write-Host 'REBIRTH_RELEASE_HARNESS PASS development_build'
}
else {
	Add-StepResult `
		-Name 'development_editor_build' `
		-Status 'NOT_RUN' `
		-Detail 'SkipDevelopmentBuild requested.' `
		-LogPath $null
	Add-StepResult `
		-Name 'development_game_build' `
		-Status 'NOT_RUN' `
		-Detail 'SkipDevelopmentBuild requested.' `
		-LogPath $null
}

if (-not $SkipMapCheck) {
	$mapCheckLog = Join-Path $runDirectory 'MapCheck_Prologue_Morning.log'
	Set-ActiveStep -Name 'map_check' -LogPath $mapCheckLog
	Invoke-RebirthMapCheck `
		-EditorCommand $editorCommand `
		-LogPath $mapCheckLog
	Complete-ActiveStep `
		-Status 'PASS' `
		-Detail '/Game/Maps/Prologue_Morning completed Map Check with zero errors.'
}
else {
	Add-StepResult `
		-Name 'map_check' `
		-Status 'NOT_RUN' `
		-Detail 'SkipMapCheck requested.' `
		-LogPath $null
}

if (-not $SkipRuntimeValidation) {
	$endingALog = Join-Path $runDirectory 'RebirthRelease_EndingA.log'
	Set-ActiveStep -Name 'runtime_ending_a' -LogPath $endingALog
	Invoke-RebirthRuntimeCase -EditorCommand $editorCommand -Ending 'A'
	Complete-ActiveStep `
		-Status 'PASS' `
		-Detail (
			'Sampled collision, synthetic PCM/component lifecycle, in-process ' +
			'v3 disk round-trip, P3/P5, and ending A passed.')

	$endingBLog = Join-Path $runDirectory 'RebirthRelease_EndingB.log'
	Set-ActiveStep -Name 'runtime_ending_b' -LogPath $endingBLog
	Invoke-RebirthRuntimeCase -EditorCommand $editorCommand -Ending 'B'
	Complete-ActiveStep `
		-Status 'PASS' `
		-Detail (
			'Sampled collision, synthetic PCM/component lifecycle, in-process ' +
			'v3 disk round-trip, P3/P5, and ending B passed.')
}
else {
	foreach ($notRunStep in @('runtime_ending_a', 'runtime_ending_b')) {
		Add-StepResult `
			-Name $notRunStep `
			-Status 'NOT_RUN' `
			-Detail 'SkipRuntimeValidation requested.' `
			-LogPath $null
	}
}

if (-not $SkipShippingPackage) {
	$shippingLog = Join-Path $runDirectory 'ShippingPackage.log'
	Set-ActiveStep -Name 'shipping_package' -LogPath $shippingLog
	if (-not (Test-Path -LiteralPath $automationScript -PathType Leaf)) {
		throw "Unreal RunUAT.bat was not found: $automationScript"
	}
	if (Test-Path -LiteralPath $ArchiveDirectory -PathType Container) {
		$existingArchiveEntries = @(
			Get-ChildItem -LiteralPath $ArchiveDirectory -Force
		)
		if ($existingArchiveEntries.Count -gt 0) {
			throw (
				'Shipping archive directory must be absent or empty so stale ' +
				"files cannot be reused: $ArchiveDirectory")
		}
	}
	else {
		New-Item -ItemType Directory -Path $ArchiveDirectory -Force | Out-Null
	}
	Invoke-NativeChecked `
		-FilePath $automationScript `
		-Arguments @(
			'BuildCookRun',
			"-project=$projectFile",
			'-target=IndieGame',
			'-noP4',
			'-unattended',
			'-utf8output',
			'-platform=Win64',
			'-clientconfig=Shipping',
			'-build',
			'-cook',
			'-allmaps',
			'-stage',
			'-pak',
			'-package',
			'-prereqs',
			'-archive',
			"-archivedirectory=$ArchiveDirectory"
		) `
		-Label 'Shipping package' `
		-LogPath $shippingLog
	Write-ShippingArchiveManifest
	Complete-ActiveStep `
		-Status 'PASS' `
		-Detail (
			"Shipping archive and SHA-256 manifest created at $ArchiveDirectory.")
	Write-Host (
		"REBIRTH_RELEASE_HARNESS PASS shipping_package " +
		"archive=$ArchiveDirectory manifest=$shippingArchiveManifestPath")
}
else {
	Add-StepResult `
		-Name 'shipping_package' `
		-Status 'NOT_RUN' `
		-Detail 'SkipShippingPackage requested.' `
		-LogPath $null
}

Set-ActiveStep -Name 'source_state_post'
$sourceStateAfterValidation = Get-SourceState
if (-not (Test-CleanSourceStateLocked `
		-Initial $initialSourceState `
		-Current $sourceStateAfterValidation)) {
	throw (
		'Git source state changed during release validation. ' +
		'Discard this run and rerun from one clean commit.')
}
Complete-ActiveStep `
	-Status 'PASS' `
	-Detail "Source state remained $($initialSourceState.commitSha)."

$hasSkippedStages =
	$SkipDevelopmentBuild `
	-or $SkipMapCheck `
	-or $SkipRuntimeValidation `
	-or $SkipShippingPackage
if ($hasSkippedStages) {
	Write-RunSummary `
		-Status 'PARTIAL' `
		-Detail 'All requested stages passed; one or more release stages were skipped.'
	Write-Host 'REBIRTH_RELEASE_HARNESS PARTIAL complete (one or more stages skipped)'
}
else {
	Write-RunSummary `
		-Status 'PASS' `
		-Detail 'All automated release stages completed from one clean commit.'
	Write-Host 'REBIRTH_RELEASE_HARNESS PASS complete'
}
