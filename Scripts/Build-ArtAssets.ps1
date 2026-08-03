[CmdletBinding()]
param(
	[switch]$SourceOnly,
	[switch]$CodeOnly
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$projectRoot = Split-Path -Parent $PSScriptRoot
$projectFile = Join-Path $projectRoot 'IndieGame.uproject'
$resolver = Join-Path $PSScriptRoot 'Resolve-UnrealEditor.ps1'

function Invoke-ArtRobocopy {
	param(
		[Parameter(Mandatory = $true)]
		[string]$Source,
		[Parameter(Mandatory = $true)]
		[string]$Destination,
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

	& robocopy.exe @arguments
	$robocopyExit = $LASTEXITCODE
	if ($robocopyExit -ge 8) {
		throw "Art build file sync failed ($robocopyExit): $Source -> $Destination"
	}
}

function Get-AsciiArtBuildRoot {
	param(
		[Parameter(Mandatory = $true)]
		[string]$SourceRoot
	)

	$hashAlgorithm = [Security.Cryptography.SHA256]::Create()
	try {
		$pathBytes = [Text.Encoding]::UTF8.GetBytes(
			$SourceRoot.ToLowerInvariant())
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
	if (-not $mirrorRoot.StartsWith(
			$mirrorBase,
			[StringComparison]::OrdinalIgnoreCase)) {
		throw "Unsafe art build mirror path: $mirrorRoot"
	}
	return $mirrorRoot
}

if ($SourceOnly -and $CodeOnly) {
	throw 'SourceOnly와 CodeOnly는 동시에 사용할 수 없습니다.'
}

if (-not $CodeOnly) {
	& (Join-Path $PSScriptRoot 'Prepare-AIArt.ps1')

	$python = Get-Command python -ErrorAction Stop
	$pbrGenerator = Join-Path $PSScriptRoot 'generate_ai_pbr_maps.py'
	Write-Host 'ART_BUILD running generate_ai_pbr_maps.py'
	& $python.Source $pbrGenerator
	if ($LASTEXITCODE -ne 0) {
		throw "PBR source-map generation failed ($LASTEXITCODE)"
	}
}

if ($SourceOnly) {
	& (Join-Path $PSScriptRoot 'Test-ArtAssetContract.ps1')
	if ($LASTEXITCODE -ne 0) {
		throw "Art source contract failed ($LASTEXITCODE)"
	}
	Write-Host 'ART_SOURCE_BUILD PASS material_scans=8 pbr_maps=30 no_unreal_process=true'
	return
}

$editorOutput = @(
	& powershell.exe `
		-NoProfile `
		-ExecutionPolicy Bypass `
		-File $resolver `
		-ProjectPath $projectFile `
		-Commandlet 2>&1
)
if ($LASTEXITCODE -ne 0 -or $editorOutput.Count -eq 0) {
	throw 'Unreal Engine resolution failed. Source PNGs are ready, but UAsset generation did not run.'
}
$editorCommand = ([string]$editorOutput[-1]).Trim()
if (-not $editorCommand.EndsWith(
		'UnrealEditor-Cmd.exe',
		[StringComparison]::OrdinalIgnoreCase)) {
	throw "Headless art build requires UnrealEditor-Cmd.exe: $editorCommand"
}

$unrealProjectRoot = $projectRoot
$usingAsciiMirror = $projectRoot -match '[^\x00-\x7F]'
if ($usingAsciiMirror) {
	$unrealProjectRoot = Get-AsciiArtBuildRoot -SourceRoot $projectRoot
	Write-Host "ART_BUILD syncing ASCII workspace: $unrealProjectRoot"
	Invoke-ArtRobocopy `
		-Source $projectRoot `
		-Destination $unrealProjectRoot `
		-Mirror
}
$unrealProjectFile = Join-Path $unrealProjectRoot 'IndieGame.uproject'
$unrealScriptsRoot = Join-Path $unrealProjectRoot 'Scripts'

$win64Directory = Split-Path -Parent $editorCommand
$binariesDirectory = Split-Path -Parent $win64Directory
$engineDirectory = Split-Path -Parent $binariesDirectory
$buildScript = Join-Path $engineDirectory 'Build\BatchFiles\Build.bat'
if (-not (Test-Path -LiteralPath $buildScript -PathType Leaf)) {
	throw "Unreal Build.bat was not found: $buildScript"
}
Write-Host 'ART_BUILD compiling IndieGameEditor Win64 Development'
& $buildScript `
	IndieGameEditor `
	Win64 `
	Development `
	"-Project=$unrealProjectFile" `
	-WaitMutex `
	-NoHotReloadFromIDE
if ($LASTEXITCODE -ne 0) {
	throw "IndieGameEditor build failed ($LASTEXITCODE)"
}
$moduleBinary = Join-Path $unrealProjectRoot 'Binaries\Win64\UnrealEditor-IndieGame.dll'
if (-not (Test-Path -LiteralPath $moduleBinary -PathType Leaf)) {
	throw "IndieGameEditor build did not produce the runtime module: $moduleBinary"
}

if ($CodeOnly) {
	if ($usingAsciiMirror) {
		Invoke-ArtRobocopy `
			-Source (Join-Path $unrealProjectRoot 'Binaries\Win64') `
			-Destination (Join-Path $projectRoot 'Binaries\Win64')
	}
	Write-Host 'ART_CODE_BUILD PASS target=IndieGameEditor platform=Win64 configuration=Development'
	return
}

$pythonStages = @(
	@{
		Script = 'generate_meshes.py'
		SuccessPattern = '\[MESHGEN\] complete: \d+/\d+ meshes'
	},
	@{
		Script = 'generate_surface_textures.py'
		SuccessPattern = '\[IndieGame\] Imported \d+ textures'
	},
	@{
		Script = 'create_textured_materials.py'
		SuccessPattern = '\[IndieGame\] Textured material pass complete: \d+ materials'
	},
	@{
		Script = 'validate_baked_art_assets.py'
		SuccessPattern = 'ART_UASSET_AUDIT PASS'
	}
)
$logRoot = Join-Path $unrealProjectRoot 'Saved\Logs'
New-Item -ItemType Directory -Force -Path $logRoot | Out-Null
foreach ($stage in $pythonStages) {
	$stageName = [string]$stage.Script
	$scriptPath = Join-Path $unrealScriptsRoot $stageName
	$logName = 'ArtBuild_{0}_{1}.log' -f (
		[IO.Path]::GetFileNameWithoutExtension($stageName)),
		(Get-Date -Format 'yyyyMMdd_HHmmss_fff')
	$logPath = Join-Path $logRoot $logName
	Write-Host "ART_BUILD running $stageName"
	& $editorCommand `
		$unrealProjectFile `
		-unattended `
		-nop4 `
		-nosplash `
		-nullrhi `
		-nosound `
		-RenderOffscreen `
		-stdout `
		-FullStdOutLogOutput `
		"-abslog=$logPath" `
		"-ExecutePythonScript=$scriptPath"
	$editorExit = $LASTEXITCODE
	$success = Select-String `
		-LiteralPath $logPath `
		-Pattern ([string]$stage.SuccessPattern) `
		-ErrorAction SilentlyContinue |
		Select-Object -Last 1
	if ($editorExit -ne 0 -or -not $success) {
		$errorLines = @(
			Select-String `
				-LiteralPath $logPath `
				-Pattern 'LogPython: Error|RuntimeError|Traceback' `
				-ErrorAction SilentlyContinue |
			Select-Object -Last 20 |
			ForEach-Object { $_.Line }
		)
		if ($errorLines.Count -gt 0) {
			Write-Warning ($errorLines -join [Environment]::NewLine)
		}
		throw "Art build stage failed ($editorExit): $stageName"
	}
}

if ($usingAsciiMirror) {
	# 한글 경로를 피해 빌드한 최신 모듈도 실제 프로젝트로 돌려보낸다.
	# 그렇지 않으면 UAsset은 최신인데 런타임은 이전 DLL을 읽을 수 있다.
	Invoke-ArtRobocopy `
		-Source (Join-Path $unrealProjectRoot 'Binaries\Win64') `
		-Destination (Join-Path $projectRoot 'Binaries\Win64')
	foreach ($relativeFolder in @(
			'Content\Meshes',
			'Content\Prototype\Textures',
			'Content\Prototype\Materials'
	)) {
		Invoke-ArtRobocopy `
			-Source (Join-Path $unrealProjectRoot $relativeFolder) `
			-Destination (Join-Path $projectRoot $relativeFolder)
	}
}

$requiredAssets = @(
	'Content\Meshes\SM_HornRimGlasses.uasset',
	'Content\Meshes\SM_InspectionRod.uasset',
	'Content\Meshes\SM_CrackedPhone.uasset',
	'Content\Meshes\SM_AlleyCatRun.uasset',
	'Content\Meshes\SM_FirstPersonHoodieSleeve.uasset',
	'Content\Meshes\SM_P3ServiceCabinetShell.uasset',
	'Content\Meshes\SM_P3ServiceManifold.uasset',
	'Content\Meshes\SM_P3ValveWheelLarge.uasset',
	'Content\Meshes\SM_P3ValveWheelSmall.uasset',
	'Content\Meshes\SM_P3PressureGauge.uasset',
	'Content\Meshes\SM_SubmergedHoodieCurl.uasset',
	'Content\Meshes\SM_SubmergedPantsCurl.uasset',
	'Content\Meshes\SM_SubmergedSlippersCurl.uasset',
	'Content\Meshes\SM_RooftopWaterTankShell.uasset',
	'Content\Meshes\SM_RooftopTankPipeCluster.uasset',
	'Content\Meshes\SM_TankInternalLadder.uasset',
	'Content\Meshes\SM_TankAccessGuardRail.uasset',
	'Content\Meshes\SM_TankAccessDeck.uasset',
	'Content\Meshes\SM_TankAccessLid.uasset',
	'Content\Meshes\SM_RooftopServiceHose.uasset',
	'Content\Meshes\SM_HoseCoupling.uasset',
	'Content\Meshes\SM_CarrierBagCollapsed.uasset',
	'Content\Meshes\SM_RooftopFireDoorLeaf.uasset',
	'Content\Meshes\SM_RooftopFireDoorFrame.uasset',
	'Content\Meshes\SM_RooftopUnlockedPadlockKeys.uasset',
	'Content\Meshes\SM_TankExteriorAccessStair.uasset',
	'Content\Meshes\SM_LadderFailureRung.uasset',
	'Content\Meshes\SM_LadderRungPadLifted.uasset',
	'Content\Meshes\SM_LadderRungRetainingClips.uasset',
	'Content\Prototype\Textures\T_EvidenceCatPawTrail_M.uasset',
	'Content\Prototype\Textures\T_DecalRustFasteners_D.uasset',
	'Content\Prototype\Textures\T_WetHoodie_D.uasset',
	'Content\Prototype\Textures\T_CarrierBagFilm_D.uasset',
	'Content\Prototype\Textures\T_AlleyCatTabby_D.uasset',
	'Content\Prototype\Textures\T_WaterTankGalvanized_D.uasset',
	'Content\Prototype\Textures\T_WetServiceHose_D.uasset',
	'Content\Prototype\Textures\T_WetRungPad_D.uasset',
	'Content\Prototype\Textures\T_TankWaterSurface_D.uasset',
	'Content\Prototype\Textures\T_P3CabinetPaintedSteel_D.uasset',
	'Content\Prototype\Textures\T_WetHoodie_N.uasset',
	'Content\Prototype\Textures\T_WetHoodie_R.uasset',
	'Content\Prototype\Textures\T_WetHoodie_A.uasset',
	'Content\Prototype\Textures\T_WetHoodie_W.uasset',
	'Content\Prototype\Textures\T_AlleyCatTabby_N.uasset',
	'Content\Prototype\Textures\T_AlleyCatTabby_R.uasset',
	'Content\Prototype\Textures\T_AlleyCatTabby_A.uasset',
	'Content\Prototype\Textures\T_WaterTankGalvanized_N.uasset',
	'Content\Prototype\Textures\T_WaterTankGalvanized_R.uasset',
	'Content\Prototype\Textures\T_WaterTankGalvanized_A.uasset',
	'Content\Prototype\Textures\T_WaterTankGalvanized_W.uasset',
	'Content\Prototype\Textures\T_WaterTankGalvanized_M.uasset',
	'Content\Prototype\Textures\T_WetServiceHose_N.uasset',
	'Content\Prototype\Textures\T_WetServiceHose_R.uasset',
	'Content\Prototype\Textures\T_WetServiceHose_A.uasset',
	'Content\Prototype\Textures\T_WetServiceHose_W.uasset',
	'Content\Prototype\Textures\T_WetRungPad_N.uasset',
	'Content\Prototype\Textures\T_WetRungPad_R.uasset',
	'Content\Prototype\Textures\T_WetRungPad_A.uasset',
	'Content\Prototype\Textures\T_WetRungPad_W.uasset',
	'Content\Prototype\Textures\T_P3CabinetPaintedSteel_N.uasset',
	'Content\Prototype\Textures\T_P3CabinetPaintedSteel_R.uasset',
	'Content\Prototype\Textures\T_P3CabinetPaintedSteel_A.uasset',
	'Content\Prototype\Textures\T_P3CabinetPaintedSteel_W.uasset',
	'Content\Prototype\Textures\T_CarrierBagFilm_N.uasset',
	'Content\Prototype\Textures\T_CarrierBagFilm_R.uasset',
	'Content\Prototype\Textures\T_CarrierBagFilm_A.uasset',
	'Content\Prototype\Textures\T_TankWaterSurface_N.uasset',
	'Content\Prototype\Textures\T_TankWaterSurface_R.uasset',
	'Content\Prototype\Textures\T_TankWaterSurface_A.uasset',
	'Content\Prototype\Materials\M_EvidenceCatPawTrail.uasset',
	'Content\Prototype\Materials\M_DecalRustFasteners.uasset',
	'Content\Prototype\Materials\M_WetHoodieUV.uasset',
	'Content\Prototype\Materials\M_CarrierBagFilm.uasset',
	'Content\Prototype\Materials\M_AlleyCatTabbyUV.uasset',
	'Content\Prototype\Materials\M_WaterTankMetalUV.uasset',
	'Content\Prototype\Materials\M_WetServiceHoseUV.uasset',
	'Content\Prototype\Materials\M_WetRungPadUV.uasset',
	'Content\Prototype\Materials\M_TankWaterReveal.uasset',
	'Content\Prototype\Materials\M_P3CabinetMetalUV.uasset'
)
$missing = @(
	$requiredAssets |
		Where-Object {
			-not (Test-Path -LiteralPath (Join-Path $projectRoot $_) -PathType Leaf)
		}
)
if ($missing.Count -gt 0) {
	throw ('Art build finished but required assets are missing: ' + ($missing -join ', '))
}

Write-Host 'ART_BUILD PASS meshes=29 evidence_masks=4 environment_overlays=4 material_scans=8 pbr_maps=30 uasset_audit=1'
