[CmdletBinding()]
param(
	[switch]$StaticOnly,
	[switch]$SkipDevelopmentBuild,
	[switch]$SkipMapCheck,
	[switch]$SkipEditorRuntimeValidation,
	[switch]$SkipRuntimeValidation,
	[switch]$SkipShippingPackage,
	[switch]$AllowDirtyWorktree,
	[ValidateRange(30, 1800)]
	[int]$RuntimeTimeoutSeconds = 240,
	[string]$ArchiveDirectory
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$skipEditorRuntimeValidation =
	$SkipRuntimeValidation -or $SkipEditorRuntimeValidation
$editorRuntimeSkipDetail = if ($SkipRuntimeValidation) {
	'SkipRuntimeValidation requested.'
}
elseif ($SkipEditorRuntimeValidation) {
	'SkipEditorRuntimeValidation requested; packaged Shipping runtime remains enabled.'
}
else {
	''
}

$projectRoot = Split-Path -Parent $PSScriptRoot
$projectFile = Join-Path $projectRoot 'IndieGame.uproject'
$buildProjectRoot = $projectRoot
$buildProjectFile = $projectFile
$usingAsciiBuildMirror = $false
$resolverScript = Join-Path $PSScriptRoot 'Resolve-UnrealEditor.ps1'
$validationScript = Join-Path $PSScriptRoot 'Validate-Project.ps1'
$persistenceScript =
	Join-Path $PSScriptRoot 'Run-Rebirth-PersistenceSpikes.ps1'
$ch02FreedomScript =
	Join-Path $PSScriptRoot 'Run-Rebirth-CH02FreedomSpikes.ps1'
$checkpointAnchorScript =
	Join-Path $PSScriptRoot 'Run-Rebirth-CheckpointAnchorSpikes.ps1'
$frontendShippingProbeScript =
	Join-Path $PSScriptRoot 'Run-Rebirth-FrontendShippingProbe.ps1'
$applicationIconValidationScript =
	Join-Path $PSScriptRoot 'Test-Windows-ExecutableIcon.ps1'
$executableMetadataSyncScript =
	Join-Path $PSScriptRoot 'Copy-Windows-ExecutableVersionResource.ps1'
$executableMetadataValidationScript =
	Join-Path $PSScriptRoot 'Test-Windows-ExecutableMetadata.ps1'
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
	'vc_runtime_prerequisite',
	'source_state_pre',
	'development_editor_build',
	'development_game_build',
	'persistence_spikes',
	'ch02_freedom_spikes',
	'checkpoint_anchor_spikes',
	'map_check',
	'runtime_ending_a',
	'runtime_ending_b',
	'shipping_package',
	'shipping_runtime_ending_a',
	'shipping_runtime_ending_b',
	'shipping_persistence_spikes',
	'shipping_frontend_input_hud',
	'shipping_archive_post_runtime',
	'source_state_post'
)
$activeStepName = $null
$activeStepLogPath = $null
$engineAssociation = $null
$engineBuildVersion = $null
$resolvedEditorForSummary = $null
$vcRuntimePrerequisite = $null
$initialSourceState = $null
$shippingArchiveManifestPath = $null
$shippingApplicationIconEvidencePath = $null
$shippingApplicationIconLogPath = $null
$shippingExecutableMetadataSyncLogPath = $null
$shippingExecutableMetadataLogPath = $null
$shippingRuntimeResults = [ordered]@{}
$shippingPersistenceSummaryPath = $null
$shippingFrontendSummaryPath = $null
$unrealDiagnosticPatterns = @(
	'(?i)\bFatal error\b',
	'(?i)\bCritical error:',
	'(?i)\bUnhandled Exception\b',
	'(?i)\bEnsure condition failed\b',
	'(?i)\bAssertion failed:',
	'(?i)\bLog[A-Za-z0-9_]+:\s*Error:'
)
$unrealDiagnosticAllowlist = @(
	# No Ensure/Error/Fatal diagnostic is approved for the current G3
	# automatic runtime. Future entries must provide exact pattern and reason.
	# [pscustomobject]@{ pattern = '<exact regex>'; reason = '<approval reason>' }
)
if ([string]::IsNullOrWhiteSpace($ArchiveDirectory)) {
	$ArchiveDirectory = Join-Path (
		Join-Path $projectRoot 'Saved\StagedBuilds\RebirthShipping') $runId
}

function Invoke-ReleaseRobocopy {
	param(
		[Parameter(Mandatory = $true)][string]$Source,
		[Parameter(Mandatory = $true)][string]$Destination,
		[switch]$Mirror
	)

	New-Item -ItemType Directory -Force -Path $Destination | Out-Null
	$arguments = @($Source, $Destination)
	$arguments += if ($Mirror) { '/MIR' } else { '/E' }
	$arguments += @(
		'/COPY:DAT',
		'/DCOPY:DAT',
		'/R:2',
		'/W:1',
		'/XJ',
		'/NFL',
		'/NDL',
		'/NP',
		'/NJH',
		'/NJS'
	)
	if ($Mirror) {
		$arguments += @(
			'/XD',
			'.git',
			'Saved',
			'Intermediate',
			'Binaries',
			'DerivedDataCache',
			'.vs'
		)
	}

	& robocopy.exe @arguments | Out-Null
	$robocopyExit = $LASTEXITCODE
	if ($robocopyExit -ge 8) {
		throw "Release workspace sync failed ($robocopyExit): $Source -> $Destination"
	}
}

function Get-AsciiReleaseBuildRoot {
	# Keep one deterministic ASCII workspace per source path. Enterprise
	# Application Control can approve this stable BuildRules assembly, whereas
	# a new run-id directory creates a new unsigned DLL path on every run. The
	# source snapshot and all evidence still remain isolated by runId/archive.
	$hashAlgorithm = [Security.Cryptography.SHA256]::Create()
	try {
		$pathBytes = [Text.Encoding]::UTF8.GetBytes(
			$projectRoot.ToLowerInvariant())
		$hashBytes = $hashAlgorithm.ComputeHash($pathBytes)
	}
	finally {
		$hashAlgorithm.Dispose()
	}
	$shortHash = -join ($hashBytes[0..3] | ForEach-Object {
		$_.ToString('x2')
	})
	$mirrorBase = Join-Path $env:LOCALAPPDATA 'IndieGame\AsciiBuild'
	$mirrorRoot = Join-Path $mirrorBase "Art_$shortHash"
	$fullBase = [IO.Path]::GetFullPath($mirrorBase).TrimEnd('\', '/')
	$fullRoot = [IO.Path]::GetFullPath($mirrorRoot)
	if (-not $fullRoot.StartsWith(
			$fullBase + [IO.Path]::DirectorySeparatorChar,
			[StringComparison]::OrdinalIgnoreCase)) {
		throw "Unsafe release build mirror path: $fullRoot"
	}
	return $fullRoot
}

function Get-VcRuntimePrerequisite {
	param([Parameter(Mandatory = $true)][string]$EngineRoot)

	$requiredVersion = [Version]'14.50.35719.0'
	$systemRoot = [Environment]::GetEnvironmentVariable('SystemRoot')
	$systemDirectory = Join-Path $systemRoot 'System32'
	$runtimeFiles = @('msvcp140_2.dll', 'vcruntime140_1.dll')
	$versions = [ordered]@{}
	$isValid = $true
	foreach ($runtimeFile in $runtimeFiles) {
		$runtimePath = Join-Path $systemDirectory $runtimeFile
		$parsedVersion = $null
		if (Test-Path -LiteralPath $runtimePath -PathType Leaf) {
			$rawVersion = (Get-Item -LiteralPath $runtimePath).VersionInfo.FileVersion
			$parsed = [Version]'0.0.0.0'
			if ([Version]::TryParse($rawVersion, [ref]$parsed)) {
				$parsedVersion = $parsed
			}
		}
		$versions[$runtimeFile] = if ($null -ne $parsedVersion) {
			$parsedVersion.ToString()
		}
		else {
			$null
		}
		if ($null -eq $parsedVersion -or $parsedVersion -lt $requiredVersion) {
			$isValid = $false
		}
	}
	$installerPath = Join-Path $EngineRoot 'Extras\Redist\en-us\vc_redist.x64.exe'
	$appLocalDirectory = Join-Path $EngineRoot (
		'Binaries\ThirdParty\AppLocalDependencies\Win64\x64\Microsoft.VC.CRT')
	$appLocalVersions = [ordered]@{}
	$appLocalHashes = [ordered]@{}
	$appLocalValid = $true
	foreach ($runtimeFile in $runtimeFiles) {
		$runtimePath = Join-Path $appLocalDirectory $runtimeFile
		$parsedVersion = $null
		if (Test-Path -LiteralPath $runtimePath -PathType Leaf) {
			$rawVersion = (Get-Item -LiteralPath $runtimePath).VersionInfo.FileVersion
			$parsed = [Version]'0.0.0.0'
			if ([Version]::TryParse($rawVersion, [ref]$parsed)) {
				$parsedVersion = $parsed
			}
		}
		$appLocalVersions[$runtimeFile] = if ($null -ne $parsedVersion) {
			$parsedVersion.ToString()
		}
		else {
			$null
		}
		if ($null -eq $parsedVersion -or $parsedVersion -lt $requiredVersion) {
			$appLocalValid = $false
		}
		$appLocalHashes[$runtimeFile] = if (
			Test-Path -LiteralPath $runtimePath -PathType Leaf) {
			(Get-FileHash -LiteralPath $runtimePath -Algorithm SHA256).Hash
		}
		else {
			$null
		}
	}

	# UE resolves imports beside UnrealEditor-Cmd.exe before System32. Epic's
	# own AppLocal CRT can therefore support a locked-down workstation without
	# silently installing a machine-wide redistributable. Accept that route
	# only when both editor-local DLLs are new enough and byte-identical to the
	# official files shipped with this exact engine installation.
	$editorLocalDirectory = Join-Path $EngineRoot 'Binaries\Win64'
	$editorLocalVersions = [ordered]@{}
	$editorLocalHashes = [ordered]@{}
	$editorLocalValid = $appLocalValid
	foreach ($runtimeFile in $runtimeFiles) {
		$runtimePath = Join-Path $editorLocalDirectory $runtimeFile
		$parsedVersion = $null
		if (Test-Path -LiteralPath $runtimePath -PathType Leaf) {
			$rawVersion = (Get-Item -LiteralPath $runtimePath).VersionInfo.FileVersion
			$parsed = [Version]'0.0.0.0'
			if ([Version]::TryParse($rawVersion, [ref]$parsed)) {
				$parsedVersion = $parsed
			}
			$editorLocalHashes[$runtimeFile] = (
				Get-FileHash -LiteralPath $runtimePath -Algorithm SHA256).Hash
		}
		else {
			$editorLocalHashes[$runtimeFile] = $null
		}
		$editorLocalVersions[$runtimeFile] = if ($null -ne $parsedVersion) {
			$parsedVersion.ToString()
		}
		else {
			$null
		}
		if ($null -eq $parsedVersion -or
			$parsedVersion -lt $requiredVersion -or
			$null -eq $appLocalHashes[$runtimeFile] -or
			$editorLocalHashes[$runtimeFile] -ne $appLocalHashes[$runtimeFile]) {
			$editorLocalValid = $false
		}
	}
	$runtimeSource = if ($isValid) {
		'System32'
	}
	elseif ($editorLocalValid) {
		'EngineAppLocal'
	}
	else {
		'Unavailable'
	}
	return [pscustomobject]@{
		valid = $isValid -or $editorLocalValid
		systemValid = $isValid
		runtimeSource = $runtimeSource
		requiredVersion = $requiredVersion.ToString()
		versions = [pscustomobject]$versions
		installerPath = $installerPath
		installerAvailable = Test-Path -LiteralPath $installerPath -PathType Leaf
		appLocalDirectory = $appLocalDirectory
		appLocalVersions = [pscustomobject]$appLocalVersions
		appLocalHashes = [pscustomobject]$appLocalHashes
		appLocalValid = $appLocalValid
		editorLocalDirectory = $editorLocalDirectory
		editorLocalVersions = [pscustomobject]$editorLocalVersions
		editorLocalHashes = [pscustomobject]$editorLocalHashes
		editorLocalValid = $editorLocalValid
	}
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

function Test-RequestedSourceStateLocked {
	param(
		[Parameter(Mandatory = $true)]$Initial,
		[Parameter(Mandatory = $true)]$Current
	)

	if ($AllowDirtyWorktree) {
		return Test-SourceStateUnchanged -Initial $Initial -Current $Current
	}
	return Test-CleanSourceStateLocked -Initial $Initial -Current $Current
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
	if ($productExecutables.Count -ne 1) {
		throw (
			'Shipping archive must contain exactly one non-empty ' +
			'IndieGame.exe product executable: ' +
			"found=$($productExecutables.Count) archive=$resolvedArchive")
	}
	$runtimeExecutables = @(
		$archiveFiles |
			Where-Object {
				$_.Name -ieq 'IndieGame-Win64-Shipping.exe' -and
				$_.Length -gt 0
			}
	)
	if ($runtimeExecutables.Count -ne 1) {
		throw (
			'Shipping archive must contain exactly one non-empty ' +
			'IndieGame-Win64-Shipping.exe runtime executable: ' +
			"found=$($runtimeExecutables.Count) archive=$resolvedArchive")
	}
	$script:shippingExecutableMetadataSyncLogPath =
		Join-Path $script:runDirectory 'ShippingExecutableMetadataSync.log'
	Invoke-NativeChecked `
		-FilePath 'powershell.exe' `
		-Arguments @(
			'-NoProfile',
			'-ExecutionPolicy',
			'Bypass',
			'-File',
			$executableMetadataSyncScript,
			'-SourceExecutable',
			$runtimeExecutables[0].FullName,
			'-DestinationExecutable',
			$productExecutables[0].FullName
		) `
		-Label 'Shipping launcher executable metadata sync' `
		-LogPath $script:shippingExecutableMetadataSyncLogPath
	$metadataSyncText = Get-Content `
		-Raw `
		-Encoding UTF8 `
		-LiteralPath $script:shippingExecutableMetadataSyncLogPath
	if (-not $metadataSyncText.Contains(
		'WINDOWS_EXECUTABLE_METADATA_SYNC PASS')) {
		throw 'Shipping launcher executable metadata sync did not report PASS.'
	}
	$script:shippingExecutableMetadataLogPath =
		Join-Path $script:runDirectory 'ShippingExecutableMetadata.log'
	Invoke-NativeChecked `
		-FilePath 'powershell.exe' `
		-Arguments @(
			'-NoProfile',
			'-ExecutionPolicy',
			'Bypass',
			'-File',
			$executableMetadataValidationScript,
			'-Executable',
			$productExecutables[0].FullName
		) `
		-Label 'Shipping launcher executable metadata' `
		-LogPath $script:shippingExecutableMetadataLogPath
	$metadataValidationText = Get-Content `
		-Raw `
		-Encoding UTF8 `
		-LiteralPath $script:shippingExecutableMetadataLogPath
	if (-not $metadataValidationText.Contains(
		'WINDOWS_EXECUTABLE_METADATA PASS')) {
		throw 'Shipping launcher executable metadata did not report PASS.'
	}
	$expectedApplicationIcon =
		Join-Path $buildProjectRoot 'Build\Windows\Application.ico'
	$script:shippingApplicationIconEvidencePath =
		Join-Path $script:runDirectory 'ShippingApplicationIcon.png'
	$script:shippingApplicationIconLogPath =
		Join-Path $script:runDirectory 'ShippingApplicationIcon.log'
	Invoke-NativeChecked `
		-FilePath 'powershell.exe' `
		-Arguments @(
			'-NoProfile',
			'-ExecutionPolicy',
			'Bypass',
			'-File',
			$applicationIconValidationScript,
			'-Executable',
			$productExecutables[0].FullName,
			'-ExpectedIco',
			$expectedApplicationIcon,
			'-EvidencePng',
			$script:shippingApplicationIconEvidencePath
		) `
		-Label 'Shipping executable application icon' `
		-LogPath $script:shippingApplicationIconLogPath
	$iconValidationText = Get-Content `
		-Raw `
		-Encoding UTF8 `
		-LiteralPath $script:shippingApplicationIconLogPath
	if (-not $iconValidationText.Contains(
			'WINDOWS_EXECUTABLE_ICON PASS size=32 matched_pixels=1024')) {
		throw 'Shipping executable application icon did not report exact pixel parity.'
	}
	# VERSIONINFO synchronization mutates the launcher after UAT staging. Refresh
	# FileInfo instances so manifest sizes describe the exact hashed artifact.
	$archiveFiles = @(
		Get-ChildItem -LiteralPath $resolvedArchive -Recurse -File |
			Sort-Object FullName
	)
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
	$requiredAppLocalVersion = [Version]'14.50.35719.0'
	$appLocalRuntimeFiles = @(
		foreach ($runtimeName in @('msvcp140_2.dll', 'vcruntime140_1.dll')) {
			$runtimePath = Join-Path $runtimeExecutables[0].DirectoryName $runtimeName
			if (-not (Test-Path -LiteralPath $runtimePath -PathType Leaf)) {
				throw (
					"Shipping runtime is missing AppLocal dependency $runtimeName " +
					"beside $($runtimeExecutables[0].FullName)")
			}
			$rawVersion = (Get-Item -LiteralPath $runtimePath).VersionInfo.FileVersion
			$parsedVersion = [Version]'0.0.0.0'
			$versionParsed = [Version]::TryParse(
				$rawVersion,
				[ref]$parsedVersion)
			if (-not $versionParsed -or
				$parsedVersion -lt $requiredAppLocalVersion) {
				throw (
					"Shipping AppLocal dependency is outdated: name=$runtimeName " +
					"required=$requiredAppLocalVersion actual=$rawVersion")
			}
			[pscustomobject]@{
				path = $runtimePath.Substring(
					$resolvedArchive.Length).TrimStart([char[]]@('\', '/'))
				version = $parsedVersion.ToString()
				sha256 = (
					Get-FileHash -Algorithm SHA256 -LiteralPath $runtimePath
				).Hash
			}
		}
	)

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
		applicationIcon = [pscustomobject]@{
			executable = $productExecutables[0].FullName
			expectedIco = $expectedApplicationIcon
			evidencePng = $script:shippingApplicationIconEvidencePath
			evidenceSha256 = (
				Get-FileHash `
					-Algorithm SHA256 `
					-LiteralPath $script:shippingApplicationIconEvidencePath
			).Hash
			validationLog = $script:shippingApplicationIconLogPath
			validationLogSha256 = (
				Get-FileHash `
					-Algorithm SHA256 `
					-LiteralPath $script:shippingApplicationIconLogPath
			).Hash
		}
		executableMetadata = [pscustomobject]@{
			launcherExecutable = $productExecutables[0].FullName
			runtimeExecutable = $runtimeExecutables[0].FullName
			expectedProductName = '4:44 AM'
			expectedProjectVersion = '1.0.0'
			expectedCompanyName = 'easygap'
			syncLog = $script:shippingExecutableMetadataSyncLogPath
			syncLogSha256 = (
				Get-FileHash `
					-Algorithm SHA256 `
					-LiteralPath $script:shippingExecutableMetadataSyncLogPath
			).Hash
			validationLog = $script:shippingExecutableMetadataLogPath
			validationLogSha256 = (
				Get-FileHash `
					-Algorithm SHA256 `
					-LiteralPath $script:shippingExecutableMetadataLogPath
			).Hash
		}
		appLocalRuntime = [pscustomobject]@{
			minimumVersion = $requiredAppLocalVersion.ToString()
			files = $appLocalRuntimeFiles
		}
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
	$shippingIconEvidenceHash = $null
	if (-not [string]::IsNullOrWhiteSpace(
			$shippingApplicationIconEvidencePath) -and
		(Test-Path `
			-LiteralPath $shippingApplicationIconEvidencePath `
			-PathType Leaf)) {
		$shippingIconEvidenceHash = (
			Get-FileHash `
				-Algorithm SHA256 `
				-LiteralPath $shippingApplicationIconEvidencePath
		).Hash
	}
	$shippingMetadataLogHash = $null
	if (-not [string]::IsNullOrWhiteSpace(
			$shippingExecutableMetadataLogPath) -and
		(Test-Path `
			-LiteralPath $shippingExecutableMetadataLogPath `
			-PathType Leaf)) {
		$shippingMetadataLogHash = (
			Get-FileHash `
				-Algorithm SHA256 `
				-LiteralPath $shippingExecutableMetadataLogPath
		).Hash
	}
	$shippingPersistenceSummaryHash = $null
	if (-not [string]::IsNullOrWhiteSpace(
			$shippingPersistenceSummaryPath) -and
		(Test-Path `
			-LiteralPath $shippingPersistenceSummaryPath `
			-PathType Leaf)) {
		$shippingPersistenceSummaryHash = (
			Get-FileHash `
				-Algorithm SHA256 `
				-LiteralPath $shippingPersistenceSummaryPath
		).Hash
	}
	$shippingFrontendSummaryHash = $null
	if (-not [string]::IsNullOrWhiteSpace($shippingFrontendSummaryPath) -and
		(Test-Path `
			-LiteralPath $shippingFrontendSummaryPath `
			-PathType Leaf)) {
		$shippingFrontendSummaryHash = (
			Get-FileHash `
				-Algorithm SHA256 `
				-LiteralPath $shippingFrontendSummaryPath
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
	$requestedSourceStateLocked = $null -ne $initialSourceState `
		-and (Test-RequestedSourceStateLocked `
			-Initial $initialSourceState `
			-Current $currentSourceState)
	if ($Status -eq 'PASS' -and -not $cleanSourceStateLocked) {
		throw (
			'Refusing to write PASS because the final Git source state is not ' +
			'the clean commit locked at startup.')
	}
	if ($Status -eq 'PARTIAL' -and -not $requestedSourceStateLocked) {
		throw (
			'Refusing to write PARTIAL because the final Git ' +
			'source state is not the clean commit locked at startup.')
	}
	$summary = [pscustomobject]@{
		schemaVersion = 3
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
		allowDirtyWorktree = $AllowDirtyWorktree.IsPresent
		requestedSourceStateLocked = $requestedSourceStateLocked
		projectFile = $projectFile
		buildProjectFile = $buildProjectFile
		usingAsciiBuildMirror = $usingAsciiBuildMirror
		engineAssociation = $engineAssociation
		engineBuildVersion = $engineBuildVersion
		resolvedEditor = $resolvedEditorForSummary
		vcRuntimePrerequisite = $vcRuntimePrerequisite
		archiveDirectory = $ArchiveDirectory
		shippingArchiveManifest = $shippingArchiveManifestPath
		shippingArchiveManifestSha256 = $shippingManifestHash
		shippingApplicationIconEvidence =
			$shippingApplicationIconEvidencePath
		shippingApplicationIconEvidenceSha256 =
			$shippingIconEvidenceHash
		shippingApplicationIconLog = $shippingApplicationIconLogPath
		shippingExecutableMetadataSyncLog =
			$shippingExecutableMetadataSyncLogPath
		shippingExecutableMetadataLog =
			$shippingExecutableMetadataLogPath
		shippingExecutableMetadataLogSha256 =
			$shippingMetadataLogHash
		shippingRuntimeResults = [pscustomobject]$shippingRuntimeResults
		shippingPersistenceSummary = $shippingPersistenceSummaryPath
		shippingPersistenceSummarySha256 = $shippingPersistenceSummaryHash
		shippingFrontendSummary = $shippingFrontendSummaryPath
		shippingFrontendSummarySha256 = $shippingFrontendSummaryHash
		unrealDiagnosticAllowlist = @($unrealDiagnosticAllowlist)
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

function Assert-NoUnexpectedUnrealDiagnostics {
	param(
		[Parameter(Mandatory = $true)]
		[string]$LogPath,
		[Parameter(Mandatory = $true)]
		[string]$LogText,
		[switch]$AllowUE58UnifiedErrorStartupNoise
	)

	$logLines = @($LogText -split '\r?\n')
	$knownStartupDiagnostics =
		[System.Collections.Generic.HashSet[int]]::new()
	if ($AllowUE58UnifiedErrorStartupNoise -and
		$engineBuildVersion -match '^5\.8\.1-') {
		$unifiedErrorStart = -1
		$engineInitialize = -1
		for ($lineIndex = 0; $lineIndex -lt $logLines.Count; $lineIndex++) {
			if ($unifiedErrorStart -lt 0 -and
				$logLines[$lineIndex].Contains(
					'LogTemp: Error test: UE::UnifiedErrorTest::Empty:')) {
				$unifiedErrorStart = $lineIndex
			}
			if ($unifiedErrorStart -ge 0 -and
				$logLines[$lineIndex].Contains(
					'LogEngine: Initializing Engine...')) {
				$engineInitialize = $lineIndex
				break
			}
		}

		if ($unifiedErrorStart -ge 0 -and
			$engineInitialize -gt $unifiedErrorStart -and
			$engineInitialize - $unifiedErrorStart -le 32) {
			$startupWindow = @(
				$logLines[$unifiedErrorStart..$engineInitialize]) -join "`n"
			$requiredEngineMarkers = @(
				'LogTemp: Error with param: UE::UnifiedErrorTest::WithInt:',
				'LogTemp: Error with context: UE::UnifiedErrorTest::Empty:',
				'LogTemp: FError that has been invalidated:',
				'LogTemp: FError that has been moved from:'
			)
			$hasCompleteEngineSignature = $true
			foreach ($requiredEngineMarker in $requiredEngineMarkers) {
				if (-not $startupWindow.Contains($requiredEngineMarker)) {
					$hasCompleteEngineSignature = $false
					break
				}
			}
			if ($hasCompleteEngineSignature) {
				$conditionIndexes = @(
					for ($lineIndex = $unifiedErrorStart;
						$lineIndex -lt $engineInitialize;
						$lineIndex++) {
						if ($logLines[$lineIndex] -match
							'LogAutomationTest:\s*Error:\s*Condition failed\s*$') {
							$lineIndex
						}
					}
				)
				if ($conditionIndexes.Count -eq 15) {
					foreach ($conditionIndex in $conditionIndexes) {
						[void]$knownStartupDiagnostics.Add($conditionIndex)
					}
					Write-Host (
						'REBIRTH_RELEASE_HARNESS INFO ignored known UE 5.8.1 ' +
						'UnifiedError startup diagnostics count=15 scope=map_check')
				}
			}
		}
	}

	$unexpectedDiagnostics = [System.Collections.Generic.List[string]]::new()
	for ($lineIndex = 0; $lineIndex -lt $logLines.Count; $lineIndex++) {
		$line = $logLines[$lineIndex]
		if ([string]::IsNullOrWhiteSpace($line)) {
			continue
		}
		$isDiagnostic = $false
		foreach ($diagnosticPattern in $unrealDiagnosticPatterns) {
			if ($line -match $diagnosticPattern) {
				$isDiagnostic = $true
				break
			}
		}
		if (-not $isDiagnostic) {
			continue
		}
		if ($knownStartupDiagnostics.Contains($lineIndex)) {
			continue
		}

		$isAllowed = $false
		foreach ($allowEntry in $unrealDiagnosticAllowlist) {
			if ($line -match [string]$allowEntry.pattern) {
				$isAllowed = $true
				break
			}
		}
		if (-not $isAllowed) {
			[void]$unexpectedDiagnostics.Add($line.Trim())
		}
	}
	if ($unexpectedDiagnostics.Count -gt 0) {
		$preview = @(
			$unexpectedDiagnostics |
				Select-Object -First 8
		) -join ' | '
		throw (
			'Unapproved Unreal Ensure/Error/Fatal diagnostic was found ' +
			"in $LogPath (count=$($unexpectedDiagnostics.Count), " +
			"allowlist=$($unrealDiagnosticAllowlist.Count)): $preview")
	}
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
	Assert-NoUnexpectedUnrealDiagnostics `
		-LogPath $LogPath `
		-LogText $logText
	foreach ($requiredMarker in @(
		'REBIRTH_GREYBOX PASS',
		'REBIRTH_RELEASE PASS s2_roof_door',
		'REBIRTH_RELEASE PASS collision_route',
		'REBIRTH_RELEASE PASS store_instancing instances=1122',
		'REBIRTH_RELEASE PASS audio_synthesis tracks=8 invalid=0 clipped=0',
		'REBIRTH_RELEASE PASS audio_queue',
		'REBIRTH_RELEASE PASS s5_item_continuity profiles=3 closures=2 presentations=2 cases=12 duplicates=0',
		'REBIRTH_RELEASE PASS p3_p5',
		'REBIRTH_RELEASE PASS savegame_v3',
		"REBIRTH_SPIKE PASS s4_common_prop ending=$Ending duplicates=0 " +
			'actual_state=1 safety_cues=5',
		"REBIRTH_RELEASE PASS ending=$Ending",
		"REBIRTH_RELEASE PASS complete ending=$Ending"
	)) {
		if (-not $logText.Contains($requiredMarker)) {
			throw "Runtime validation marker is missing: $requiredMarker. Check $LogPath."
		}
	}
	$expectedRouteOrder = if ($Ending -eq 'A') { 'p1_p2' } else { 'p2_p1' }
	foreach ($endToEndMarker in @(
			'REBIRTH_E2E PASS ch01_router',
			'REBIRTH_E2E PASS ch02_router',
			'approval_0431=1',
			'approval_screen=1',
			'cat_aftermath=1',
			'authored_housings=2',
			'layered_displays=2',
			'pressure_caps=2',
			'time_entry_physical=2',
			"route_order=$expectedRouteOrder",
			'REBIRTH_E2E PASS ch03_handoff',
			'REBIRTH_SPIKE PASS s1_outfit_sleeve chapters=3 duplicates=0 stitches=3'
		)) {
			if (-not $logText.Contains($endToEndMarker)) {
				throw (
					"End-to-end runtime marker is missing: $endToEndMarker. " +
					"Check $LogPath.")
			}
		}
}

function Assert-PersistenceSpikeEvidence {
	param(
		[Parameter(Mandatory = $true)]
		[string]$LogPath,
		[Parameter(Mandatory = $true)]
		[string]$EvidenceDirectory,
		[Parameter(Mandatory = $true)]
		[string]$ExpectedCommitSha
	)

	if (-not (Test-Path -LiteralPath $LogPath -PathType Leaf)) {
		throw "Persistence spike harness log is missing: $LogPath"
	}
	$harnessLog = Get-Content -Raw -Encoding UTF8 -LiteralPath $LogPath
	if ($harnessLog.Contains('REBIRTH_SPIKE FAIL') -or
		-not $harnessLog.Contains(
			'REBIRTH_SPIKE_HARNESS PASS complete boundary=2 cat_choices=5 ch02_time=2 p5=4 p3=7 endings=2')) {
		throw "Persistence spike harness did not report a complete PASS: $LogPath"
	}

	$nestedSummaryPath = Join-Path $EvidenceDirectory 'summary.json'
	if (-not (Test-Path -LiteralPath $nestedSummaryPath -PathType Leaf)) {
		throw "Persistence spike summary is missing: $nestedSummaryPath"
	}
	try {
		$nestedSummary =
			Get-Content -Raw -Encoding UTF8 -LiteralPath $nestedSummaryPath |
			ConvertFrom-Json
	}
	catch {
		throw (
			"Persistence spike summary is invalid JSON: " +
			"$nestedSummaryPath ($($_.Exception.Message))")
	}
	$packagedShipping =
		$null -ne $nestedSummary.PSObject.Properties['packagedShipping'] `
		-and [bool]$nestedSummary.packagedShipping
	if ($nestedSummary.status -ne 'PASS' -or
		[string]$nestedSummary.commitSha -ne $ExpectedCommitSha -or
		[int]$nestedSummary.memoryBoundaryProcessRestarts -ne 2 -or
		[int]$nestedSummary.catChoiceProcessRestarts -ne 5 -or
		[int]$nestedSummary.ch02TimeProcessRestarts -ne 2 -or
		[int]$nestedSummary.p5ProcessRestarts -ne 4 -or
		[int]$nestedSummary.p3ProcessRestarts -ne 7 -or
		[int]$nestedSummary.endingProcessRestarts -ne 4) {
		throw (
			'Persistence spike summary metadata does not match the locked ' +
			"release source state: $nestedSummaryPath")
	}
	if ($packagedShipping -and (
			[int]$nestedSummary.processCount -ne 46 -or
			[int]$nestedSummary.archiveFileCount -le 0 -or
			-not [bool]$nestedSummary.archiveUnchanged)) {
		throw (
			'Packaged Shipping persistence metadata is incomplete: ' +
			$nestedSummaryPath)
	}

	$expectedCases = [System.Collections.Generic.HashSet[string]]::new(
		[System.StringComparer]::Ordinal)
	foreach ($phase in @('Before', 'After')) {
		[void]$expectedCases.Add("Boundary${phase}Write")
		[void]$expectedCases.Add("Boundary${phase}Read")
	}
	foreach ($catChoice in @(
		'CapLeft',
		'CapWaited',
		'CupLeft',
		'CupWaited',
		'PassedBy'
	)) {
		[void]$expectedCases.Add("CatChoiceWrite_$catChoice")
		[void]$expectedCases.Add("CatChoiceRead_$catChoice")
	}
	for ($checkpoint = 0; $checkpoint -le 1; ++$checkpoint) {
		[void]$expectedCases.Add("CH02TimeWrite_$checkpoint")
		[void]$expectedCases.Add("CH02TimeRead_$checkpoint")
	}
	for ($checkpoint = 0; $checkpoint -le 3; ++$checkpoint) {
		[void]$expectedCases.Add("P5Write_$checkpoint")
		[void]$expectedCases.Add("P5Read_$checkpoint")
	}
	for ($checkpoint = 0; $checkpoint -le 6; ++$checkpoint) {
		[void]$expectedCases.Add("P3Write_$checkpoint")
		[void]$expectedCases.Add("P3Read_$checkpoint")
	}
	foreach ($ending in @('A', 'B')) {
		[void]$expectedCases.Add("EndingWrite_$ending")
		[void]$expectedCases.Add("EndingCommit_$ending")
		[void]$expectedCases.Add("EndingVerify_$ending")
	}
	$results = @($nestedSummary.results)
	if ($results.Count -ne $expectedCases.Count) {
		throw (
			"Persistence spike result count was $($results.Count); " +
			"expected $($expectedCases.Count).")
	}

	$evidenceRoot = (
		Resolve-Path -LiteralPath $EvidenceDirectory
	).Path.TrimEnd('\') + '\'
	$seenCases = [System.Collections.Generic.HashSet[string]]::new(
		[System.StringComparer]::Ordinal)
	foreach ($result in $results) {
		$caseName = [string]$result.case
		if (-not $expectedCases.Contains($caseName) -or
			-not $seenCases.Add($caseName) -or
			$result.status -ne 'PASS') {
			throw "Persistence spike result is missing, duplicated, or failed: $caseName"
		}
		if ($packagedShipping) {
			if (-not (Test-Path `
					-LiteralPath $result.receiptPath `
					-PathType Leaf)) {
				throw "Packaged persistence receipt is missing: $caseName"
			}
			$resolvedReceipt = (
				Resolve-Path -LiteralPath $result.receiptPath
			).Path
			if (-not $resolvedReceipt.StartsWith(
					$evidenceRoot,
					[System.StringComparison]::OrdinalIgnoreCase)) {
				throw (
					'Packaged persistence receipt escaped its evidence ' +
					"directory: $resolvedReceipt")
			}
			$actualReceiptHash = (
				Get-FileHash -Algorithm SHA256 -LiteralPath $resolvedReceipt
			).Hash
			if ($actualReceiptHash -ne [string]$result.receiptSha256) {
				throw "Packaged persistence receipt hash mismatch: $resolvedReceipt"
			}
			$receiptText = (
				Get-Content -Raw -Encoding UTF8 -LiteralPath $resolvedReceipt
			).Trim()
			if (-not $receiptText.StartsWith(
					'REBIRTH_SPIKE PASS ',
					[StringComparison]::Ordinal)) {
				throw "Packaged persistence receipt reported failure: $resolvedReceipt"
			}
		}
		else {
			if (-not (Test-Path -LiteralPath $result.logPath -PathType Leaf)) {
				throw "Persistence spike case log is missing: $($result.logPath)"
			}
			$resolvedCaseLog = (
				Resolve-Path -LiteralPath $result.logPath
			).Path
			if (-not $resolvedCaseLog.StartsWith(
					$evidenceRoot,
					[System.StringComparison]::OrdinalIgnoreCase)) {
				throw "Persistence spike log escaped its evidence directory: $resolvedCaseLog"
			}
			$actualHash = (
				Get-FileHash -Algorithm SHA256 -LiteralPath $resolvedCaseLog
			).Hash
			if ($actualHash -ne [string]$result.logSha256) {
				throw "Persistence spike log hash mismatch: $resolvedCaseLog"
			}
			$caseLogText =
				Get-Content -Raw -Encoding UTF8 -LiteralPath $resolvedCaseLog
			if ($caseLogText.Contains('REBIRTH_SPIKE FAIL')) {
				throw "Persistence spike case reported FAIL: $resolvedCaseLog"
			}
			Assert-NoUnexpectedUnrealDiagnostics `
				-LogPath $resolvedCaseLog `
				-LogText $caseLogText
		}

		$requiresSaveSnapshot =
			$caseName -like 'Boundary*Write' -or
			$caseName -like 'CatChoiceWrite_*' -or
			$caseName -like 'CH02TimeWrite_*' -or
			$caseName -like 'P5Write_*' -or
			$caseName -like 'P3Write_*' -or
			$caseName -like 'EndingWrite_*' -or
			$caseName -like 'EndingCommit_*'
		$requiresSaveDeletion =
			$caseName -like 'Boundary*Read' -or
			$caseName -like 'CatChoiceRead_*' -or
			$caseName -like 'CH02TimeRead_*' -or
			$caseName -like 'P5Read_*' -or
			$caseName -like 'P3Read_*' -or
			$caseName -like 'EndingVerify_*'
		if ($requiresSaveSnapshot) {
			if ([string]::IsNullOrWhiteSpace(
					[string]$result.saveSnapshotPath) -or
				-not (Test-Path `
					-LiteralPath $result.saveSnapshotPath `
					-PathType Leaf)) {
				throw "Persistence save snapshot is missing: $caseName"
			}
			$resolvedSaveSnapshot = (
				Resolve-Path -LiteralPath $result.saveSnapshotPath
			).Path
			if (-not $resolvedSaveSnapshot.StartsWith(
					$evidenceRoot,
					[System.StringComparison]::OrdinalIgnoreCase)) {
				throw (
					'Persistence save snapshot escaped its evidence ' +
					"directory: $resolvedSaveSnapshot")
			}
			$actualSaveHash = (
				Get-FileHash `
					-Algorithm SHA256 `
					-LiteralPath $resolvedSaveSnapshot
			).Hash
			if ($actualSaveHash -ne [string]$result.saveSnapshotSha256) {
				throw (
					"Persistence save snapshot hash mismatch: " +
					$resolvedSaveSnapshot)
			}
		}
		elseif ($requiresSaveDeletion -and -not [bool]$result.saveDeleted) {
			throw "Persistence save slot was not deleted: $caseName"
		}
	}
	if ($seenCases.Count -ne $expectedCases.Count) {
		throw 'Persistence spike result set is incomplete.'
	}
}

function Assert-CH02FreedomSpikeEvidence {
	param(
		[Parameter(Mandatory = $true)][string]$LogPath,
		[Parameter(Mandatory = $true)][string]$EvidenceDirectory,
		[Parameter(Mandatory = $true)][string]$ExpectedCommitSha
	)

	if (-not (Test-Path -LiteralPath $LogPath -PathType Leaf)) {
		throw "CH02 freedom harness log is missing: $LogPath"
	}
	$harnessLog = Get-Content -Raw -Encoding UTF8 -LiteralPath $LogPath
	if ($harnessLog.Contains('REBIRTH_E2E FAIL') -or
		-not $harnessLog.Contains(
			'REBIRTH_CH02_FREEDOM_HARNESS PASS complete routes=5')) {
		throw "CH02 freedom harness did not report a complete PASS: $LogPath"
	}

	$summaryPath = Join-Path $EvidenceDirectory 'summary.json'
	if (-not (Test-Path -LiteralPath $summaryPath -PathType Leaf)) {
		throw "CH02 freedom summary is missing: $summaryPath"
	}
	$summary = Get-Content -Raw -Encoding UTF8 -LiteralPath $summaryPath |
		ConvertFrom-Json
	if ($summary.status -ne 'PASS' -or
		[string]$summary.commitSha -ne $ExpectedCommitSha -or
		[int]$summary.routeCount -ne 5) {
		throw "CH02 freedom summary metadata mismatch: $summaryPath"
	}

	$expected = @{
		P1ThenP2 = @(1, 1, 3, 2)
		P2ThenP1 = @(1, 1, 3, 2)
		SkipP1 = @(0, 1, 2, 1)
		SkipP2 = @(1, 0, 2, 1)
		SkipBoth = @(0, 0, 2, 0)
	}
	$results = @($summary.results)
	if ($results.Count -ne $expected.Count) {
		throw "CH02 freedom result count mismatch: $($results.Count)"
	}
	$evidenceRoot = (
		Resolve-Path -LiteralPath $EvidenceDirectory
	).Path.TrimEnd('\') + '\'
	$seen = [System.Collections.Generic.HashSet[string]]::new(
		[System.StringComparer]::Ordinal)
	foreach ($result in $results) {
		$route = [string]$result.route
		if (-not $expected.ContainsKey($route) -or
			-not $seen.Add($route) -or
			$result.status -ne 'PASS') {
			throw "CH02 freedom route is missing, duplicated, or failed: $route"
		}
		$contract = $expected[$route]
		if ([int][bool]$result.p1Resolved -ne $contract[0] -or
			[int][bool]$result.p2Resolved -ne $contract[1] -or
			[int]$result.truthCount -ne $contract[2] -or
			[int]$result.pressureCaps -ne $contract[3]) {
			throw "CH02 freedom state mismatch: $route"
		}
		$resolvedLog = (Resolve-Path -LiteralPath $result.logPath).Path
		if (-not $resolvedLog.StartsWith(
				$evidenceRoot,
				[System.StringComparison]::OrdinalIgnoreCase)) {
			throw "CH02 freedom log escaped evidence directory: $resolvedLog"
		}
		$actualHash = (
			Get-FileHash -Algorithm SHA256 -LiteralPath $resolvedLog
		).Hash
		if ($actualHash -ne [string]$result.logSha256) {
			throw "CH02 freedom log hash mismatch: $resolvedLog"
		}
		$caseLog = Get-Content -Raw -Encoding UTF8 -LiteralPath $resolvedLog
		Assert-NoUnexpectedUnrealDiagnostics `
			-LogPath $resolvedLog `
			-LogText $caseLog
	}
	if ($seen.Count -ne $expected.Count) {
		throw 'CH02 freedom route set is incomplete.'
	}
}

function Assert-CheckpointAnchorSpikeEvidence {
	param(
		[Parameter(Mandatory = $true)][string]$LogPath,
		[Parameter(Mandatory = $true)][string]$EvidenceDirectory,
		[Parameter(Mandatory = $true)][string]$ExpectedCommitSha
	)

	if (-not (Test-Path -LiteralPath $LogPath -PathType Leaf)) {
		throw "Checkpoint anchor harness log is missing: $LogPath"
	}
	$harnessLog = Get-Content -Raw -Encoding UTF8 -LiteralPath $LogPath
	if ($harnessLog.Contains('REBIRTH_SPIKE FAIL') -or
		-not $harnessLog.Contains(
			'REBIRTH_ANCHOR_HARNESS PASS complete anchors=5 processes=10')) {
		throw "Checkpoint anchor harness did not report a complete PASS: $LogPath"
	}

	$summaryPath = Join-Path $EvidenceDirectory 'summary.json'
	if (-not (Test-Path -LiteralPath $summaryPath -PathType Leaf)) {
		throw "Checkpoint anchor summary is missing: $summaryPath"
	}
	$summary = Get-Content -Raw -Encoding UTF8 -LiteralPath $summaryPath |
		ConvertFrom-Json
	if ($summary.status -ne 'PASS' -or
		[string]$summary.commitSha -ne $ExpectedCommitSha -or
		[int]$summary.anchorCount -ne 5 -or
		[int]$summary.processCount -ne 10 -or
		[int]$summary.mapReentryCount -ne 5 -or
		[int]$summary.capsuleClearCount -ne 5 -or
		[int]$summary.floorContactCount -ne 5) {
		throw "Checkpoint anchor summary metadata mismatch: $summaryPath"
	}

	$anchors = @(
		'CH02Corridor',
		'CH02Store',
		'CH03Apartment',
		'CH03Flood',
		'CH03Roof'
	)
	$expectedCases = [System.Collections.Generic.HashSet[string]]::new(
		[System.StringComparer]::Ordinal)
	foreach ($anchor in $anchors) {
		[void]$expectedCases.Add("AnchorWrite_$anchor")
		[void]$expectedCases.Add("AnchorRead_$anchor")
	}
	$results = @($summary.results)
	if ($results.Count -ne $expectedCases.Count) {
		throw "Checkpoint anchor result count mismatch: $($results.Count)"
	}
	$evidenceRoot = (
		Resolve-Path -LiteralPath $EvidenceDirectory
	).Path.TrimEnd('\') + '\'
	$seen = [System.Collections.Generic.HashSet[string]]::new(
		[System.StringComparer]::Ordinal)
	foreach ($result in $results) {
		$caseName = [string]$result.case
		if (-not $expectedCases.Contains($caseName) -or
			-not $seen.Add($caseName) -or
			$result.status -ne 'PASS') {
			throw "Checkpoint anchor case is missing, duplicated, or failed: $caseName"
		}
		$resolvedLog = (Resolve-Path -LiteralPath $result.logPath).Path
		if (-not $resolvedLog.StartsWith(
				$evidenceRoot,
				[System.StringComparison]::OrdinalIgnoreCase)) {
			throw "Checkpoint anchor log escaped evidence directory: $resolvedLog"
		}
		$actualHash = (
			Get-FileHash -Algorithm SHA256 -LiteralPath $resolvedLog
		).Hash
		if ($actualHash -ne [string]$result.logSha256) {
			throw "Checkpoint anchor log hash mismatch: $resolvedLog"
		}
		$caseLog = Get-Content -Raw -Encoding UTF8 -LiteralPath $resolvedLog
		if ($caseLog.Contains('REBIRTH_SPIKE FAIL')) {
			throw "Checkpoint anchor case reported FAIL: $resolvedLog"
		}
		Assert-NoUnexpectedUnrealDiagnostics `
			-LogPath $resolvedLog `
			-LogText $caseLog
		if ($caseName -like 'AnchorWrite_*') {
			if ([string]::IsNullOrWhiteSpace(
					[string]$result.saveSnapshotPath) -or
				-not (Test-Path `
					-LiteralPath $result.saveSnapshotPath `
					-PathType Leaf)) {
				throw "Checkpoint anchor save snapshot is missing: $caseName"
			}
			$resolvedSaveSnapshot = (
				Resolve-Path -LiteralPath $result.saveSnapshotPath
			).Path
			if (-not $resolvedSaveSnapshot.StartsWith(
					$evidenceRoot,
					[System.StringComparison]::OrdinalIgnoreCase)) {
				throw (
					'Checkpoint anchor save snapshot escaped its evidence ' +
					"directory: $resolvedSaveSnapshot")
			}
			$actualSaveHash = (
				Get-FileHash `
					-Algorithm SHA256 `
					-LiteralPath $resolvedSaveSnapshot
			).Hash
			if ($actualSaveHash -ne [string]$result.saveSnapshotSha256) {
				throw "Checkpoint anchor save hash mismatch: $caseName"
			}
		}
		elseif (-not [bool]$result.saveDeleted -or
			-not $caseLog.Contains('capsule_clear=1 map_reentered=1')) {
			throw "Checkpoint anchor physical restore contract failed: $caseName"
		}
	}
	if ($seen.Count -ne $expectedCases.Count) {
		throw 'Checkpoint anchor result set is incomplete.'
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
		'-nosound',
		'-RenderOffscreen',
		'-stdout',
		'-FullStdOutLogOutput',
		"-abslog=$logPath",
		'-IGRebirthGreybox',
		'-IGRebirthReleaseValidation',
		"-IGRebirthEnding=$Ending"
	)
	$runtimeArguments += '-IGRebirthEndToEndValidation'
	$processArguments = @(
		$runtimeArguments |
			ForEach-Object { ConvertTo-ProcessArgument -Value $_ }
	)
	Write-Host "[Runtime ending $Ending] $EditorCommand $($runtimeArguments -join ' ')"
	$process = Start-Process `
		-FilePath $EditorCommand `
		-ArgumentList $processArguments `
		-PassThru `
		-WindowStyle Hidden
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

# UAT success proves that files were staged, not that the monolithic Shipping
# executable can boot and reach an ending. Use a game-written receipt because
# Shipping logging can be compiled out, redirected or unavailable to stdout.
function Invoke-RebirthShippingRuntimeCase {
	param(
		[Parameter(Mandatory = $true)]
		[string]$ArchiveRoot,
		[Parameter(Mandatory = $true)]
		[ValidateSet('A', 'B')]
		[string]$Ending
	)

	$runtimeExecutables = @(
		Get-ChildItem -LiteralPath $ArchiveRoot -Recurse -File |
			Where-Object {
				$_.Name -ieq 'IndieGame-Win64-Shipping.exe' -and
				$_.Length -gt 0
			}
	)
	if ($runtimeExecutables.Count -ne 1) {
		throw (
			'Shipping runtime validation requires exactly one non-empty ' +
			'IndieGame-Win64-Shipping.exe: ' +
			"found=$($runtimeExecutables.Count) archive=$ArchiveRoot")
	}

	$runtimeExecutable = $runtimeExecutables[0]
	$resultPath = Join-Path $runDirectory "ShippingRuntime_Ending$Ending.txt"
	$userDirectory = Join-Path $runDirectory "ShippingRuntimeUser_$Ending"
	if (Test-Path -LiteralPath $resultPath -PathType Leaf) {
		throw (
			"Shipping runtime receipt path is not fresh for ending ${Ending}: " +
			$resultPath)
	}
	if (Test-Path -LiteralPath $userDirectory) {
		throw (
			"Shipping runtime user directory is not fresh for ending ${Ending}: " +
			$userDirectory)
	}
	New-Item -ItemType Directory -Path $userDirectory | Out-Null

	$runtimeArguments = @(
		'/Game/Maps/Prologue_Morning',
		'-game',
		'-unattended',
		'-nosplash',
		'-NoLoadingScreen',
		'-nullrhi',
		'-nosound',
		'-RenderOffscreen',
		'-IGRebirthGreybox',
		'-IGRebirthReleaseValidation',
		'-IGRebirthEndToEndValidation',
		"-IGRebirthEnding=$Ending",
		"-IGRebirthResultPath=$resultPath",
		"-UserDir=$userDirectory"
	)
	$processArguments = @(
		$runtimeArguments |
			ForEach-Object { ConvertTo-ProcessArgument -Value $_ }
	)
	Write-Host (
		"[Shipping runtime ending $Ending] $($runtimeExecutable.FullName) " +
		($runtimeArguments -join ' '))
	$process = Start-Process `
		-FilePath $runtimeExecutable.FullName `
		-ArgumentList $processArguments `
		-WorkingDirectory $runtimeExecutable.DirectoryName `
		-PassThru `
		-WindowStyle Hidden
	try {
		if (-not $process.WaitForExit($RuntimeTimeoutSeconds * 1000)) {
			$process.Kill()
			[void]$process.WaitForExit(5000)
			throw (
				"Shipping runtime ending $Ending timed out after " +
				"$RuntimeTimeoutSeconds seconds.")
		}
		$process.Refresh()
		if ($process.ExitCode -ne 0) {
			throw (
				"Shipping runtime ending $Ending exited with code " +
				"$($process.ExitCode).")
		}
	}
	finally {
		$process.Dispose()
	}

	if (-not (Test-Path -LiteralPath $resultPath -PathType Leaf)) {
		throw "Shipping runtime ending $Ending did not create $resultPath"
	}
	$resultText = (
		Get-Content -Raw -Encoding UTF8 -LiteralPath $resultPath
	).Trim()
	$expectedResult =
		"REBIRTH_PACKAGED_RUNTIME PASS contract=1 ending=$Ending " +
		'common_discovery=1 strong_cue=1 c6=1'
	if ($resultText -cne $expectedResult) {
		throw (
			"Shipping runtime ending $Ending returned an invalid receipt: " +
			$resultText)
	}
	$resultHash = (
		Get-FileHash -Algorithm SHA256 -LiteralPath $resultPath
	).Hash
	$script:shippingRuntimeResults[$Ending] = [pscustomobject]@{
		executable = $runtimeExecutable.FullName
		executableSha256 = (
			Get-FileHash `
				-Algorithm SHA256 `
				-LiteralPath $runtimeExecutable.FullName
		).Hash
		resultPath = $resultPath
		resultSha256 = $resultHash
		userDirectory = $userDirectory
	}
	Write-Host (
		"REBIRTH_RELEASE_HARNESS PASS shipping_runtime ending=$Ending " +
		"receipt=$resultPath sha256=$resultHash")
}

function Assert-FrontendShippingProbeEvidence {
	param(
		[Parameter(Mandatory = $true)][string]$LogPath,
		[Parameter(Mandatory = $true)][string]$EvidenceDirectory,
		[Parameter(Mandatory = $true)][string]$ArchiveRoot
	)

	if (-not (Test-Path -LiteralPath $LogPath -PathType Leaf)) {
		throw "Frontend Shipping harness log is missing: $LogPath"
	}
	$harnessLog = Get-Content -Raw -Encoding UTF8 -LiteralPath $LogPath
	if (-not $harnessLog.Contains(
			'REBIRTH_FRONTEND_SHIPPING PASS resolutions=4 input_events=44')) {
		throw "Frontend Shipping harness did not report a complete PASS: $LogPath"
	}
	$summaryPath = Join-Path $EvidenceDirectory 'summary.json'
	if (-not (Test-Path -LiteralPath $summaryPath -PathType Leaf)) {
		throw "Frontend Shipping summary is missing: $summaryPath"
	}
	$summary = Get-Content -Raw -Encoding UTF8 -LiteralPath $summaryPath |
		ConvertFrom-Json
	$resolvedArchive = (
		Resolve-Path -LiteralPath $ArchiveRoot
	).Path.TrimEnd([char[]]@('\', '/'))
	$summaryArchive = [IO.Path]::GetFullPath(
		[string]$summary.archiveDirectory).TrimEnd([char[]]@('\', '/'))
	$summaryResults = @($summary.results)
	if ([int]$summary.schemaVersion -ne 3 -or
		-not $summaryArchive.Equals(
			$resolvedArchive,
			[StringComparison]::OrdinalIgnoreCase) -or
		[int]$summary.archiveFileCount -le 0 -or
		[int]$summary.resolutionCount -ne 4 -or
		[int]$summary.dialogueCaseCount -ne 8 -or
		[int]$summary.inputEventCount -ne 44 -or
		[int]$summary.layoutSampleCount -ne 40 -or
		$summaryResults.Count -ne 4 -or
		-not [bool]$summary.archiveUnchanged) {
		throw "Frontend Shipping summary metadata mismatch: $summaryPath"
	}
	$archivePrefix = $resolvedArchive + [IO.Path]::DirectorySeparatorChar
	$summaryShippingExecutable = (
		Resolve-Path -LiteralPath ([string]$summary.shippingExecutable)
	).Path
	if (-not $summaryShippingExecutable.StartsWith(
			$archivePrefix,
			[StringComparison]::OrdinalIgnoreCase)) {
		throw (
			'Frontend Shipping executable escaped the archive: ' +
			$summaryShippingExecutable)
	}
	$actualShippingExecutableHash = (
		Get-FileHash -Algorithm SHA256 -LiteralPath $summaryShippingExecutable
	).Hash
	if ($actualShippingExecutableHash -cne
		[string]$summary.shippingExecutableSha256) {
		throw (
			'Frontend Shipping executable hash mismatch: ' +
			$summaryShippingExecutable)
	}
	$expectedResolutions = [System.Collections.Generic.HashSet[string]]::new(
		[StringComparer]::Ordinal)
	foreach ($resolution in @(
		'1280x720',
		'1600x900',
		'1920x1080',
		'2560x1440')) {
		[void]$expectedResolutions.Add($resolution)
	}
	$evidenceRoot = (
		Resolve-Path -LiteralPath $EvidenceDirectory
	).Path.TrimEnd('\') + '\'
	foreach ($result in $summaryResults) {
		$resolution = [string]$result.resolution
		if (-not $expectedResolutions.Remove($resolution) -or
			[int]$result.inputEvents -ne 11 -or
			[int]$result.layoutSamples -ne 10 -or
			[int]$result.minimumElements -lt 8) {
			throw "Frontend Shipping result contract failed: $resolution"
		}
		$resolvedReceipt = (
			Resolve-Path -LiteralPath $result.receiptPath
		).Path
		if (-not $resolvedReceipt.StartsWith(
				$evidenceRoot,
				[StringComparison]::OrdinalIgnoreCase)) {
			throw "Frontend Shipping receipt escaped evidence: $resolvedReceipt"
		}
		$actualReceiptHash = (
			Get-FileHash -Algorithm SHA256 -LiteralPath $resolvedReceipt
		).Hash
		if ($actualReceiptHash -ne [string]$result.receiptSha256) {
			throw "Frontend Shipping receipt hash mismatch: $resolvedReceipt"
		}
		$resolvedDialogueScreenshot = (
			Resolve-Path -LiteralPath $result.dialogueScreenshotPath
		).Path
		if (-not $resolvedDialogueScreenshot.StartsWith(
			$evidenceRoot,
			[StringComparison]::OrdinalIgnoreCase)) {
			throw (
				'Frontend Shipping dialogue screenshot escaped evidence: ' +
				$resolvedDialogueScreenshot)
		}
		$actualDialogueScreenshotHash = (
			Get-FileHash -Algorithm SHA256 -LiteralPath $resolvedDialogueScreenshot
		).Hash
		if ($actualDialogueScreenshotHash -ne
			[string]$result.dialogueScreenshotSha256) {
			throw (
				'Frontend Shipping dialogue screenshot hash mismatch: ' +
				$resolvedDialogueScreenshot)
		}
		$resolvedDefaultDialogueScreenshot = (
			Resolve-Path -LiteralPath $result.defaultDialogueScreenshotPath
		).Path
		if (-not $resolvedDefaultDialogueScreenshot.StartsWith(
			$evidenceRoot,
			[StringComparison]::OrdinalIgnoreCase)) {
			throw (
				'Frontend Shipping default dialogue screenshot escaped evidence: ' +
				$resolvedDefaultDialogueScreenshot)
		}
		$actualDefaultDialogueScreenshotHash = (
			Get-FileHash `
				-Algorithm SHA256 `
				-LiteralPath $resolvedDefaultDialogueScreenshot
		).Hash
		if ($actualDefaultDialogueScreenshotHash -ne
			[string]$result.defaultDialogueScreenshotSha256) {
			throw (
				'Frontend Shipping default dialogue screenshot hash mismatch: ' +
				$resolvedDefaultDialogueScreenshot)
		}
		$receiptText = (
			Get-Content -Raw -Encoding UTF8 -LiteralPath $resolvedReceipt
		).Trim()
		$receiptPattern = (
			'^REBIRTH_FRONTEND PASS contract=3 resolution=' +
			[regex]::Escape($resolution) +
			' keyboard_access=1 gamepad_access=1 dpad_down=1 ' +
			'keyboard_up=1 gamepad_close=1 keyboard_pause=1 ' +
			'gamepad_pause=1 display=1 dialogue=1 dialogue_default=1 ' +
			'speaker=1 continuation=1 default_scale=100 max_scale=200 ' +
			'sound_lane=1 samples=10 elements_min=(?<elements>[0-9]+) ' +
			'input_events=11 bounds=(?<minX>-?[0-9]+),(?<minY>-?[0-9]+),' +
			'(?<maxX>-?[0-9]+),(?<maxY>-?[0-9]+)$')
		$receiptMatch = [regex]::Match($receiptText, $receiptPattern)
		$resolutionMatch = [regex]::Match(
			$resolution,
			'^(?<width>[0-9]+)x(?<height>[0-9]+)$')
		if (-not $receiptMatch.Success -or -not $resolutionMatch.Success) {
			throw "Frontend Shipping receipt contract failed: $resolvedReceipt"
		}
		$minimumElements = [int]$receiptMatch.Groups['elements'].Value
		$minimumX = [int]$receiptMatch.Groups['minX'].Value
		$minimumY = [int]$receiptMatch.Groups['minY'].Value
		$maximumX = [int]$receiptMatch.Groups['maxX'].Value
		$maximumY = [int]$receiptMatch.Groups['maxY'].Value
		$expectedWidth = [int]$resolutionMatch.Groups['width'].Value
		$expectedHeight = [int]$resolutionMatch.Groups['height'].Value
		if ($minimumElements -ne [int]$result.minimumElements -or
			[int]$result.dialogueScreenshotWidth -ne $expectedWidth -or
			[int]$result.dialogueScreenshotHeight -ne $expectedHeight -or
			[int]$result.defaultDialogueScreenshotWidth -ne $expectedWidth -or
			[int]$result.defaultDialogueScreenshotHeight -ne $expectedHeight -or
			$minimumX -ne [int]$result.bounds.minimumX -or
			$minimumY -ne [int]$result.bounds.minimumY -or
			$maximumX -ne [int]$result.bounds.maximumX -or
			$maximumY -ne [int]$result.bounds.maximumY -or
			$minimumX -lt -1 -or $minimumY -lt -1 -or
			$maximumX -gt ($expectedWidth + 1) -or
			$maximumY -gt ($expectedHeight + 1) -or
			[string]$result.receipt -cne $receiptText) {
			throw "Frontend Shipping receipt evidence mismatch: $resolvedReceipt"
		}
	}
	if ($expectedResolutions.Count -ne 0) {
		throw 'Frontend Shipping result set is incomplete.'
	}
}

function Assert-ShippingArchiveManifestUnchanged {
	param(
		[Parameter(Mandatory = $true)]
		[string]$ArchiveRoot,
		[Parameter(Mandatory = $true)]
		[string]$ManifestPath
	)

	if (-not (Test-Path -LiteralPath $ManifestPath -PathType Leaf)) {
		throw "Shipping archive manifest is missing: $ManifestPath"
	}
	$resolvedArchive = (
		Resolve-Path -LiteralPath $ArchiveRoot
	).Path.TrimEnd([char[]]@('\', '/'))
	$manifest = Get-Content `
		-Raw `
		-Encoding UTF8 `
		-LiteralPath $ManifestPath | ConvertFrom-Json
	$manifestArchive = [IO.Path]::GetFullPath(
		[string]$manifest.archiveDirectory).TrimEnd([char[]]@('\', '/'))
	if (-not $manifestArchive.Equals(
			$resolvedArchive,
			[System.StringComparison]::OrdinalIgnoreCase)) {
		throw (
			'Shipping archive manifest root mismatch: ' +
			"expected=$resolvedArchive actual=$manifestArchive")
	}

	$manifestArtifacts = @($manifest.artifacts)
	$actualFiles = @(
		Get-ChildItem -LiteralPath $resolvedArchive -Recurse -File
	)
	if ($manifestArtifacts.Count -ne [int]$manifest.fileCount -or
		$actualFiles.Count -ne $manifestArtifacts.Count) {
		throw (
			'Shipping archive file count changed after runtime validation: ' +
			"manifest=$($manifestArtifacts.Count) actual=$($actualFiles.Count)")
	}

	$archivePrefix = $resolvedArchive + [IO.Path]::DirectorySeparatorChar
	$seenPaths = [System.Collections.Generic.HashSet[string]]::new(
		[System.StringComparer]::OrdinalIgnoreCase)
	foreach ($artifact in $manifestArtifacts) {
		$relativePath = [string]$artifact.path
		if ([string]::IsNullOrWhiteSpace($relativePath) -or
			[IO.Path]::IsPathRooted($relativePath)) {
			throw "Shipping manifest has an invalid relative path: $relativePath"
		}
		$joinedArtifactPath = Join-Path $resolvedArchive $relativePath
		$artifactPath = [IO.Path]::GetFullPath($joinedArtifactPath)
		if (-not $artifactPath.StartsWith(
				$archivePrefix,
				[System.StringComparison]::OrdinalIgnoreCase)) {
			throw "Shipping manifest path escaped the archive: $relativePath"
		}
		if (-not $seenPaths.Add($artifactPath)) {
			throw "Shipping manifest contains a duplicate path: $relativePath"
		}
		if (-not (Test-Path -LiteralPath $artifactPath -PathType Leaf)) {
			throw "Shipping archive file disappeared after runtime: $relativePath"
		}
		$file = Get-Item -LiteralPath $artifactPath
		if ($file.Length -ne [long]$artifact.sizeBytes) {
			throw "Shipping archive file size changed after runtime: $relativePath"
		}
		$actualHash = (
			Get-FileHash -Algorithm SHA256 -LiteralPath $artifactPath
		).Hash
		if ($actualHash -cne [string]$artifact.sha256) {
			throw "Shipping archive file hash changed after runtime: $relativePath"
		}
	}

	foreach ($actualFile in $actualFiles) {
		if (-not $seenPaths.Contains($actualFile.FullName)) {
			throw (
				'Shipping runtime created an unmanifested archive file: ' +
				$actualFile.FullName)
		}
	}
	Write-Host (
		'REBIRTH_RELEASE_HARNESS PASS shipping_archive_post_runtime ' +
		"files=$($actualFiles.Count) manifest=$ManifestPath")
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
		'-nosound',
		'-RenderOffscreen',
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
		-WindowStyle Hidden
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
	Assert-NoUnexpectedUnrealDiagnostics `
		-LogPath $LogPath `
		-LogText $logText `
		-AllowUE58UnifiedErrorStartupNoise
	$mapPassPattern =
		'(?im)MapCheck:.*(?:Map check complete:\s*0 Error|' +
		'맵 체크 완료:\s*오류 0 회,\s*경고 0 회)'
	if ($logText -match '(?im)MapCheck:.*Error:' -or
		$logText -match '(?im)Map check failed|맵 체크 실패' -or
		$logText -notmatch $mapPassPattern) {
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
if (-not (Test-Path -LiteralPath $persistenceScript -PathType Leaf)) {
	throw "Persistence spike harness was not found: $persistenceScript"
}
$projectDescriptor =
	Get-Content -Raw -Encoding UTF8 -LiteralPath $projectFile |
	ConvertFrom-Json
$engineAssociation = [string]$projectDescriptor.EngineAssociation
$initialSourceState = Get-SourceState
$sourceLockLabel = if ($AllowDirtyWorktree) {
	'unchanged dirty regression snapshot'
}
else {
	'clean commit'
}
if ($StaticOnly) {
	Set-ActiveStep -Name 'source_state_pre'
	$sourceStateBeforeStatic = Get-SourceState
	if (-not (Test-RequestedSourceStateLocked `
			-Initial $initialSourceState `
			-Current $sourceStateBeforeStatic)) {
		$sourceBlockedDetail = if ($AllowDirtyWorktree) {
			'Static regression source state changed before it could be locked. Rerun.'
		}
		else {
			'Static validation requires one clean, unchanged Git commit. ' +
			'Commit or revert all tracked and untracked changes, then rerun.'
		}
		Complete-ActiveStep -Status 'BLOCKED' -Detail $sourceBlockedDetail
		Add-MissingStepResults -Detail 'Static source-state gate was blocked.'
		Write-RunSummary -Status 'BLOCKED' -Detail $sourceBlockedDetail
		[Console]::Error.WriteLine(
			"REBIRTH_RELEASE_HARNESS BLOCKED $sourceBlockedDetail")
		exit 2
	}
	Complete-ActiveStep `
		-Status 'PASS' `
		-Detail "Clean static source state locked to $($initialSourceState.commitSha)."
}
if (-not $StaticOnly `
	-and $SkipDevelopmentBuild `
	-and (-not $SkipMapCheck -or -not $skipEditorRuntimeValidation)) {
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
	Set-ActiveStep -Name 'source_state_post'
	$sourceStateAfterStatic = Get-SourceState
	if (-not (Test-RequestedSourceStateLocked `
			-Initial $initialSourceState `
			-Current $sourceStateAfterStatic)) {
		throw (
			'Git source state changed during static release validation. ' +
			'Discard this run and rerun from one clean commit.')
	}
	Complete-ActiveStep `
		-Status 'PASS' `
		-Detail "Static source state locked as $sourceLockLabel at $($initialSourceState.commitSha)."
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
		-ProjectPath $projectFile `
		-Commandlet 2>&1
)
$resolverExitCode = $LASTEXITCODE
$ErrorActionPreference = $previousErrorActionPreference
$resolvedEditorOutput |
	ForEach-Object { [string]$_ } |
	Set-Content -LiteralPath $engineResolutionLog -Encoding UTF8
if ($resolverExitCode -ne 0 -or $resolvedEditorOutput.Count -eq 0) {
	$blockedDetail =
		"Unreal Engine $engineAssociation was not found. " +
		'Install it or set IG_UNREAL_EDITOR to its editor binary.'
	Complete-ActiveStep -Status 'BLOCKED' -Detail $blockedDetail
	Add-MissingStepResults -Detail 'Engine resolution was blocked.'
	Write-RunSummary -Status 'BLOCKED' -Detail $blockedDetail
	[Console]::Error.WriteLine(
		"REBIRTH_RELEASE_HARNESS BLOCKED $blockedDetail")
	exit 2
}
$editorCommand = ([string]$resolvedEditorOutput[-1]).Trim()
if (-not (Test-Path -LiteralPath $editorCommand -PathType Leaf)) {
	throw "Resolved Unreal commandlet does not exist: $editorCommand"
}
if (-not $editorCommand.EndsWith(
		'UnrealEditor-Cmd.exe',
		[StringComparison]::OrdinalIgnoreCase)) {
	throw "Headless release validation requires UnrealEditor-Cmd.exe: $editorCommand"
}
$resolvedEditorForSummary = $editorCommand

$win64Directory = Split-Path -Parent $editorCommand
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
	-Detail "Resolved $editorCommand ($engineBuildVersion)."

$requiresEditorRuntime =
	-not $SkipMapCheck -or
	-not $SkipRuntimeValidation -or
	-not $SkipShippingPackage
if ($requiresEditorRuntime) {
	Set-ActiveStep -Name 'vc_runtime_prerequisite'
	$vcRuntimePrerequisite = Get-VcRuntimePrerequisite -EngineRoot $engineDirectory
	if (-not $vcRuntimePrerequisite.valid) {
		$installedVersions = @(
			$vcRuntimePrerequisite.versions.psobject.Properties |
				ForEach-Object {
					$versionLabel = if ($null -ne $_.Value) {
						$_.Value
					}
					else {
						'missing'
					}
					'{0}={1}' -f $_.Name, $versionLabel
				}
		) -join ', '
		$editorLocalVersions = @(
			$vcRuntimePrerequisite.editorLocalVersions.psobject.Properties |
				ForEach-Object {
					$versionLabel = if ($null -ne $_.Value) {
						$_.Value
					}
					else {
						'missing'
					}
					'{0}={1}' -f $_.Name, $versionLabel
				}
		) -join ', '
		$runtimeBlockedDetail =
			'Unreal Engine editor execution requires Microsoft Visual C++ ' +
			"Redistributable $($vcRuntimePrerequisite.requiredVersion) or newer. " +
			"Detected System32 [$installedVersions], engine-local " +
			"[$editorLocalVersions]. Install $($vcRuntimePrerequisite.installerPath) " +
			'or restore the official UE AppLocal files outside this unattended run, ' +
			'then rerun the release harness.'
		Complete-ActiveStep -Status 'BLOCKED' -Detail $runtimeBlockedDetail
		Add-MissingStepResults -Detail 'VC++ runtime prerequisite was blocked.'
		Write-RunSummary -Status 'BLOCKED' -Detail $runtimeBlockedDetail
		[Console]::Error.WriteLine(
			"REBIRTH_RELEASE_HARNESS BLOCKED $runtimeBlockedDetail")
		exit 2
	}
	if (-not $SkipShippingPackage -and
		-not $vcRuntimePrerequisite.appLocalValid) {
		$appLocalBlockedDetail =
			'Shipping requires the UE AppLocal CRT source at ' +
			"$($vcRuntimePrerequisite.appLocalDirectory) with version " +
			"$($vcRuntimePrerequisite.requiredVersion) or newer. Repair the " +
			'UE 5.8 installation, then rerun the release harness.'
		Complete-ActiveStep -Status 'BLOCKED' -Detail $appLocalBlockedDetail
		Add-MissingStepResults -Detail 'Shipping AppLocal CRT source was blocked.'
		Write-RunSummary -Status 'BLOCKED' -Detail $appLocalBlockedDetail
		[Console]::Error.WriteLine(
			"REBIRTH_RELEASE_HARNESS BLOCKED $appLocalBlockedDetail")
		exit 2
	}
	Complete-ActiveStep `
		-Status 'PASS' `
		-Detail (
			'Microsoft Visual C++ runtime satisfies Unreal Engine minimum ' +
			"$($vcRuntimePrerequisite.requiredVersion) via " +
			"$($vcRuntimePrerequisite.runtimeSource); AppLocal " +
			"source valid=$($vcRuntimePrerequisite.appLocalValid).")
}
else {
	Add-StepResult `
		-Name 'vc_runtime_prerequisite' `
		-Status 'NOT_RUN' `
		-Detail 'No Unreal editor runtime stage was requested.' `
		-LogPath $null
}

Set-ActiveStep -Name 'source_state_pre'
$sourceStateBeforeBuild = Get-SourceState
if (-not (Test-RequestedSourceStateLocked `
		-Initial $initialSourceState `
		-Current $sourceStateBeforeBuild)) {
	$sourceBlockedDetail = if ($AllowDirtyWorktree) {
		'Regression source state changed before it could be locked. Rerun.'
	}
	else {
		'Release validation requires one clean, unchanged Git commit. ' +
		'Commit or revert all tracked and untracked changes, then rerun.'
	}
	Complete-ActiveStep -Status 'BLOCKED' -Detail $sourceBlockedDetail
	Add-MissingStepResults -Detail 'Source-state gate was blocked.'
	Write-RunSummary -Status 'BLOCKED' -Detail $sourceBlockedDetail
	[Console]::Error.WriteLine(
		"REBIRTH_RELEASE_HARNESS BLOCKED $sourceBlockedDetail")
	exit 2
}
Complete-ActiveStep `
	-Status 'PASS' `
	-Detail "Source state locked as $sourceLockLabel at $($initialSourceState.commitSha)."

$usingAsciiBuildMirror = $projectRoot -match '[^\x00-\x7F]'
if ($usingAsciiBuildMirror) {
	$buildProjectRoot = Get-AsciiReleaseBuildRoot
	Write-Host "REBIRTH_RELEASE_HARNESS syncing ASCII build workspace: $buildProjectRoot"
	Invoke-ReleaseRobocopy `
		-Source $projectRoot `
		-Destination $buildProjectRoot `
		-Mirror
	$buildProjectFile = Join-Path $buildProjectRoot 'IndieGame.uproject'
	if (-not (Test-Path -LiteralPath $buildProjectFile -PathType Leaf)) {
		throw "ASCII release project descriptor was not created: $buildProjectFile"
	}
}

$buildScript = Join-Path $engineDirectory 'Build\BatchFiles\Build.bat'
$automationScript = Join-Path $engineDirectory 'Build\BatchFiles\RunUAT.bat'
if (-not (Test-Path -LiteralPath $editorCommand -PathType Leaf)) {
	throw "UnrealEditor-Cmd.exe was not found: $editorCommand"
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
			$buildProjectFile,
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
			$buildProjectFile,
			'-WaitMutex'
		) `
		-Label 'Development game build' `
		-LogPath $gameBuildLog
	Complete-ActiveStep -Status 'PASS' -Detail 'IndieGame Development built.'
	if ($usingAsciiBuildMirror) {
		Invoke-ReleaseRobocopy `
			-Source (Join-Path $buildProjectRoot 'Binaries\Win64') `
			-Destination (Join-Path $projectRoot 'Binaries\Win64')
	}
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

if (-not $skipEditorRuntimeValidation) {
	$persistenceLog = Join-Path $runDirectory 'PersistenceSpikes.log'
	$persistenceEvidence =
		Join-Path $runDirectory 'PersistenceSpikes'
	Set-ActiveStep -Name 'persistence_spikes' -LogPath $persistenceLog
	Invoke-NativeChecked `
		-FilePath 'powershell.exe' `
		-Arguments @(
			'-NoProfile',
			'-ExecutionPolicy',
			'Bypass',
			'-File',
			$persistenceScript,
			'-TimeoutSeconds',
			"$RuntimeTimeoutSeconds",
			'-EvidenceDirectory',
			$persistenceEvidence
		) `
		-Label 'Process-boundary persistence spikes' `
		-LogPath $persistenceLog
	Assert-PersistenceSpikeEvidence `
		-LogPath $persistenceLog `
		-EvidenceDirectory $persistenceEvidence `
		-ExpectedCommitSha $initialSourceState.commitSha
	Complete-ActiveStep `
		-Status 'PASS' `
		-Detail (
			'CH01 memory-boundary before/after images, CH02 P1/P2, P5 ' +
			'checkpoints 0..3, P3 checkpoints 0..6, and S4 ending A/B ' +
			'state passed separate-process disk restore.')

	$freedomLog = Join-Path $runDirectory 'CH02FreedomSpikes.log'
	$freedomEvidence = Join-Path $runDirectory 'CH02FreedomSpikes'
	Set-ActiveStep -Name 'ch02_freedom_spikes' -LogPath $freedomLog
	Invoke-NativeChecked `
		-FilePath 'powershell.exe' `
		-Arguments @(
			'-NoProfile',
			'-ExecutionPolicy',
			'Bypass',
			'-File',
			$ch02FreedomScript,
			'-TimeoutSeconds',
			"$RuntimeTimeoutSeconds",
			'-EvidenceDirectory',
			$freedomEvidence
		) `
		-Label 'CH02 free-order and skip-route spikes' `
		-LogPath $freedomLog
	Assert-CH02FreedomSpikeEvidence `
		-LogPath $freedomLog `
		-EvidenceDirectory $freedomEvidence `
		-ExpectedCommitSha $initialSourceState.commitSha
	Complete-ActiveStep `
		-Status 'PASS' `
		-Detail (
			'P1/P2 both orders, either skip, and both-skip alternate records ' +
			'reached the same CH03 handoff in five isolated processes.')

	$anchorLog = Join-Path $runDirectory 'CheckpointAnchorSpikes.log'
	$anchorEvidence = Join-Path $runDirectory 'CheckpointAnchorSpikes'
	Set-ActiveStep -Name 'checkpoint_anchor_spikes' -LogPath $anchorLog
	Invoke-NativeChecked `
		-FilePath 'powershell.exe' `
		-Arguments @(
			'-NoProfile',
			'-ExecutionPolicy',
			'Bypass',
			'-File',
			$checkpointAnchorScript,
			'-TimeoutSeconds',
			"$RuntimeTimeoutSeconds",
			'-EvidenceDirectory',
			$anchorEvidence
		) `
		-Label 'Checkpoint map re-entry and physical anchor spikes' `
		-LogPath $anchorLog
	Assert-CheckpointAnchorSpikeEvidence `
		-LogPath $anchorLog `
		-EvidenceDirectory $anchorEvidence `
		-ExpectedCommitSha $initialSourceState.commitSha
	Complete-ActiveStep `
		-Status 'PASS' `
		-Detail (
			'CH02 corridor/store and CH03 apartment/flood/roof checkpoints ' +
			're-entered their maps on clear capsules with supported floors.')
}
else {
	foreach ($notRunStep in @(
		'persistence_spikes',
		'ch02_freedom_spikes',
		'checkpoint_anchor_spikes')) {
		Add-StepResult `
			-Name $notRunStep `
			-Status 'NOT_RUN' `
			-Detail $editorRuntimeSkipDetail `
			-LogPath $null
	}
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

if (-not $skipEditorRuntimeValidation) {
	$endingALog = Join-Path $runDirectory 'RebirthRelease_EndingA.log'
	Set-ActiveStep -Name 'runtime_ending_a' -LogPath $endingALog
	Invoke-RebirthRuntimeCase -EditorCommand $editorCommand -Ending 'A'
	Complete-ActiveStep `
		-Status 'PASS' `
		-Detail (
			'CH01-to-CH03 production-event route, S1 outfit, S2 roof door, ' +
			'sampled collision, synthetic PCM/component lifecycle, in-process ' +
			'v3 disk round-trip, P3/P5, and ending A passed.')

	$endingBLog = Join-Path $runDirectory 'RebirthRelease_EndingB.log'
	Set-ActiveStep -Name 'runtime_ending_b' -LogPath $endingBLog
	Invoke-RebirthRuntimeCase -EditorCommand $editorCommand -Ending 'B'
	Complete-ActiveStep `
		-Status 'PASS' `
		-Detail (
			'S2 roof door, sampled collision, synthetic PCM/component lifecycle, ' +
			'in-process v3 disk round-trip, P3/P5, and ending B passed.')
}
else {
	foreach ($notRunStep in @('runtime_ending_a', 'runtime_ending_b')) {
		Add-StepResult `
			-Name $notRunStep `
			-Status 'NOT_RUN' `
			-Detail $editorRuntimeSkipDetail `
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
			"-project=$buildProjectFile",
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
			'-applocaldirectory=$(EngineDir)/Binaries/ThirdParty/AppLocalDependencies',
			'-archive',
			"-archivedirectory=$ArchiveDirectory"
		) `
		-Label 'Shipping package' `
		-LogPath $shippingLog
	Write-ShippingArchiveManifest
	Complete-ActiveStep `
		-Status 'PASS' `
		-Detail (
			'Shipping archive, AppLocal CRT, exact product metadata/icon ' +
			'parity evidence, and ' +
			"SHA-256 manifest created at $ArchiveDirectory.")
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

if (-not $SkipShippingPackage -and -not $SkipRuntimeValidation) {
	foreach ($shippingEnding in @('A', 'B')) {
		$stepSuffix = $shippingEnding.ToLowerInvariant()
		$receiptPath = Join-Path `
			$runDirectory `
			"ShippingRuntime_Ending$shippingEnding.txt"
		Set-ActiveStep `
			-Name "shipping_runtime_ending_$stepSuffix" `
			-LogPath $receiptPath
		Invoke-RebirthShippingRuntimeCase `
			-ArchiveRoot $ArchiveDirectory `
			-Ending $shippingEnding
		Complete-ActiveStep `
			-Status 'PASS' `
			-Detail (
				"Packaged Shipping ending $shippingEnding completed from an " +
				'isolated user directory and produced the exact runtime receipt.')
	}
	$shippingPersistenceLog = Join-Path (
		$runDirectory) 'ShippingPersistenceSpikes.log'
	$shippingPersistenceEvidence = Join-Path (
		$runDirectory) 'ShippingPersistenceSpikes'
	Set-ActiveStep `
		-Name 'shipping_persistence_spikes' `
		-LogPath $shippingPersistenceLog
	Invoke-NativeChecked `
		-FilePath 'powershell.exe' `
		-Arguments @(
			'-NoProfile',
			'-ExecutionPolicy',
			'Bypass',
			'-File',
			$persistenceScript,
			'-ArchiveDirectory',
			$ArchiveDirectory,
			'-TimeoutSeconds',
			"$RuntimeTimeoutSeconds",
			'-EvidenceDirectory',
			$shippingPersistenceEvidence
		) `
		-Label 'Packaged Shipping process-boundary persistence spikes' `
		-LogPath $shippingPersistenceLog
	Assert-PersistenceSpikeEvidence `
		-LogPath $shippingPersistenceLog `
		-EvidenceDirectory $shippingPersistenceEvidence `
		-ExpectedCommitSha $initialSourceState.commitSha
	$script:shippingPersistenceSummaryPath = Join-Path (
		$shippingPersistenceEvidence) 'summary.json'
	Complete-ActiveStep `
		-Status 'PASS' `
		-Detail (
			'Packaged Shipping passed 46 independent save, exit, restart, ' +
			'restore and deletion processes with exact receipts.')

	$shippingFrontendLog = Join-Path (
		$runDirectory) 'ShippingFrontendInputHud.log'
	$shippingFrontendEvidence = Join-Path (
		$runDirectory) 'ShippingFrontendInputHud'
	$frontendTimeoutSeconds = [Math]::Min(
		180,
		[Math]::Max(30, $RuntimeTimeoutSeconds))
	Set-ActiveStep `
		-Name 'shipping_frontend_input_hud' `
		-LogPath $shippingFrontendLog
	Invoke-NativeChecked `
		-FilePath 'powershell.exe' `
		-Arguments @(
			'-NoProfile',
			'-ExecutionPolicy',
			'Bypass',
			'-File',
			$frontendShippingProbeScript,
			'-ArchiveDirectory',
			$ArchiveDirectory,
			'-TimeoutSeconds',
			"$frontendTimeoutSeconds",
			'-EvidenceDirectory',
			$shippingFrontendEvidence
		) `
		-Label 'Packaged Shipping input and HUD layout probe' `
		-LogPath $shippingFrontendLog
	Assert-FrontendShippingProbeEvidence `
		-LogPath $shippingFrontendLog `
		-EvidenceDirectory $shippingFrontendEvidence `
		-ArchiveRoot $ArchiveDirectory
	$script:shippingFrontendSummaryPath = Join-Path (
		$shippingFrontendEvidence) 'summary.json'
	Complete-ActiveStep `
		-Status 'PASS' `
		-Detail (
			'Packaged Shipping processed keyboard and gamepad menu input and ' +
			'kept native HUD text inside 720p, 900p, 1080p and 1440p canvases.')

	Set-ActiveStep `
		-Name 'shipping_archive_post_runtime' `
		-LogPath $shippingArchiveManifestPath
	Assert-ShippingArchiveManifestUnchanged `
		-ArchiveRoot $ArchiveDirectory `
		-ManifestPath $shippingArchiveManifestPath
	Complete-ActiveStep `
		-Status 'PASS' `
		-Detail (
			'Shipping archive file count, sizes and SHA-256 values remained ' +
			'unchanged after every packaged runtime validation.')
}
else {
	$shippingRuntimeSkipDetail = if ($SkipShippingPackage) {
		'SkipShippingPackage requested.'
	}
	else {
		'SkipRuntimeValidation requested.'
	}
	foreach ($notRunStep in @(
		'shipping_runtime_ending_a',
		'shipping_runtime_ending_b',
		'shipping_persistence_spikes',
		'shipping_frontend_input_hud',
		'shipping_archive_post_runtime')) {
		Add-StepResult `
			-Name $notRunStep `
			-Status 'NOT_RUN' `
			-Detail $shippingRuntimeSkipDetail `
			-LogPath $null
	}
}

Set-ActiveStep -Name 'source_state_post'
$sourceStateAfterValidation = Get-SourceState
if (-not (Test-RequestedSourceStateLocked `
		-Initial $initialSourceState `
		-Current $sourceStateAfterValidation)) {
	throw (
		'Git source state changed during release validation. ' +
		'Discard this run and rerun from one frozen source snapshot.')
}
Complete-ActiveStep `
	-Status 'PASS' `
	-Detail "Source state remained $($initialSourceState.commitSha)."

$hasSkippedStages =
	$SkipDevelopmentBuild `
	-or $SkipMapCheck `
	-or $SkipEditorRuntimeValidation `
	-or $SkipRuntimeValidation `
	-or $SkipShippingPackage `
	-or $AllowDirtyWorktree
if ($hasSkippedStages) {
	Write-RunSummary `
		-Status 'PARTIAL' `
		-Detail (
			'All requested automatic stages passed; at least one release stage was ' +
			'skipped or the run used an unchanged dirty regression snapshot.')
	Write-Host 'REBIRTH_RELEASE_HARNESS PARTIAL complete (not release-candidate eligible)'
}
else {
	Write-RunSummary `
		-Status 'PASS' `
		-Detail 'All automated release stages completed from one clean commit.'
	Write-Host 'REBIRTH_RELEASE_HARNESS PASS complete'
}
