[CmdletBinding()]
param(
	[string]$ProjectPath,
	[string]$ExplicitEditorPath = $env:IG_UNREAL_EDITOR,
	[switch]$Commandlet
)

$ErrorActionPreference = 'Stop'

if ([string]::IsNullOrWhiteSpace($ProjectPath)) {
	$ProjectPath = Join-Path $PSScriptRoot '..\IndieGame.uproject'
}

if (-not (Test-Path -LiteralPath $ProjectPath -PathType Leaf)) {
	throw "Unreal project was not found: $ProjectPath"
}

$project = Get-Content -Raw -Encoding UTF8 -LiteralPath $ProjectPath |
	ConvertFrom-Json
$association = [string]$project.EngineAssociation
if ([string]::IsNullOrWhiteSpace($association)) {
	throw "EngineAssociation is missing from: $ProjectPath"
}

function Test-EngineAssociation {
	param(
		[Parameter(Mandatory)][string]$EditorPath,
		[Parameter(Mandatory)][string]$ExpectedAssociation
	)

	if ($ExpectedAssociation -notmatch '^(\d+)\.(\d+)$') {
		# Source builds commonly use a GUID association. The HKCU mapping is
		# authoritative for those and cannot be compared as a semantic version.
		return $true
	}

	$engineDirectory = Split-Path -Parent (
		Split-Path -Parent (
			Split-Path -Parent $EditorPath))
	$buildVersionPath = Join-Path $engineDirectory 'Build\Build.version'
	if (-not (Test-Path -LiteralPath $buildVersionPath -PathType Leaf)) {
		return $false
	}

	try {
		$buildVersion = Get-Content -Raw -Encoding UTF8 `
			-LiteralPath $buildVersionPath | ConvertFrom-Json
		return [int]$buildVersion.MajorVersion -eq [int]$Matches[1] -and
			[int]$buildVersion.MinorVersion -eq [int]$Matches[2]
	}
	catch {
		return $false
	}
}

$candidateRoots = [System.Collections.Generic.List[string]]::new()
if (-not [string]::IsNullOrWhiteSpace($ExplicitEditorPath)) {
	$candidateRoots.Add($ExplicitEditorPath)
}

# Both variables are null anywhere that is not Windows, and Join-Path rejects
# a null -Path outright. Called inside an array literal it threw before the
# loop body's own emptiness check could run, and before any candidate was
# tried at all -- so IG_UNREAL_EDITOR, the override this script's own error
# message tells you to set, could not be used on such a machine. The check has
# to happen before the join, not after it.
foreach ($programFiles in @($env:ProgramFiles, ${env:ProgramFiles(x86)})) {
	if ([string]::IsNullOrWhiteSpace($programFiles)) {
		continue
	}
	$candidateRoots.Add((Join-Path $programFiles "Epic Games\UE_$association"))
}

foreach ($registryPath in @(
	"HKLM:\SOFTWARE\EpicGames\Unreal Engine\$association",
	"HKLM:\SOFTWARE\WOW6432Node\EpicGames\Unreal Engine\$association"
)) {
	if (Test-Path -LiteralPath $registryPath) {
		$installedDirectory =
			(Get-ItemProperty -LiteralPath $registryPath).InstalledDirectory
		if (-not [string]::IsNullOrWhiteSpace($installedDirectory)) {
			$candidateRoots.Add([string]$installedDirectory)
		}
	}
}

$userBuildsPath = 'HKCU:\SOFTWARE\Epic Games\Unreal Engine\Builds'
if (Test-Path -LiteralPath $userBuildsPath) {
	$buildProperties =
		(Get-ItemProperty -LiteralPath $userBuildsPath).PSObject.Properties |
		Where-Object { $_.Name -notlike 'PS*' }
	foreach ($property in $buildProperties) {
		if (-not [string]::IsNullOrWhiteSpace([string]$property.Value)) {
			$candidateRoots.Add([string]$property.Value)
		}
	}
}

$launcherManifest =
	'C:\ProgramData\Epic\UnrealEngineLauncher\LauncherInstalled.dat'
if (Test-Path -LiteralPath $launcherManifest -PathType Leaf) {
	$launcher = Get-Content -Raw -Encoding UTF8 -LiteralPath $launcherManifest |
		ConvertFrom-Json
	foreach ($installation in @($launcher.InstallationList)) {
		if ([string]$installation.AppName -match
				[regex]::Escape($association) -or
			[string]$installation.AppVersion -like "$association*") {
			$candidateRoots.Add([string]$installation.InstallLocation)
		}
	}
}

foreach ($candidate in $candidateRoots |
		Where-Object { -not [string]::IsNullOrWhiteSpace($_) } |
		Select-Object -Unique) {
	$requestedBinary = if ($Commandlet) {
		'UnrealEditor-Cmd.exe'
	}
	else {
		'UnrealEditor.exe'
	}
	$editorPath = if (
		$candidate.EndsWith('UnrealEditor.exe', [StringComparison]::OrdinalIgnoreCase) -or
		$candidate.EndsWith('UnrealEditor-Cmd.exe', [StringComparison]::OrdinalIgnoreCase)
	) {
		Join-Path (Split-Path -Parent $candidate) $requestedBinary
	}
	else {
		Join-Path $candidate "Engine\Binaries\Win64\$requestedBinary"
	}

	if ((Test-Path -LiteralPath $editorPath -PathType Leaf) -and
		(Test-EngineAssociation `
			-EditorPath $editorPath `
			-ExpectedAssociation $association)) {
		(Get-Item -LiteralPath $editorPath).FullName
		exit 0
	}
}

Write-Error (
	"Unreal Engine $association was not found. " +
	"Install it or set IG_UNREAL_EDITOR to the engine editor binary.")
exit 1
