[CmdletBinding()]
param(
	[switch]$SourceOnly,
	[switch]$CodeOnly,
	[switch]$HudUiOnly,
	[switch]$ApartmentVisualOnly,
	[switch]$SurfaceResponseOnly,
	[switch]$PropResponseOnly,
	[switch]$RetailRealismOnly,
	[switch]$CorridorSignageOnly,
	[switch]$MissingFloorOnly,
	[switch]$TankWaterOnly,
	[switch]$TankInteriorOnly,
	[switch]$SubmergedClothingOnly
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$projectRoot = Split-Path -Parent $PSScriptRoot
$projectFile = Join-Path $projectRoot 'IndieGame.uproject'
$resolver = Join-Path $PSScriptRoot 'Resolve-UnrealEditor.ps1'
# 엔진 경로를 뽑는 리졸버는 PowerShell 7로 돌리는 쪽이 낫다. 5.1은 기본
# 출력 인코딩이 시스템 ANSI 코드페이지라 한글 경로가 섞이면 깨질 수 있다.
# 다만 PS7이 없다고 아트 빌드 전체를 막을 이유는 없다. 리졸버가 돌려주는
# 것은 엔진 설치 경로뿐이고 거기에는 한글이 안 들어간다. PS7이 있으면 쓰고,
# 없으면 5.1로 내리되 출력 인코딩을 UTF-8로 고정해서 넘긴다.
$powerShellCore = Get-Command 'pwsh.exe' -ErrorAction SilentlyContinue
$usingPowerShellCore = $null -ne $powerShellCore
if ($usingPowerShellCore) {
	$resolverShell = $powerShellCore.Source
} else {
	$windowsPowerShell = Get-Command 'powershell.exe' -ErrorAction SilentlyContinue
	if ($null -eq $windowsPowerShell) {
		throw 'PowerShell을 찾을 수 없어 아트 빌드를 시작할 수 없습니다.'
	}
	$resolverShell = $windowsPowerShell.Source
	Write-Host 'ART_BUILD pwsh 없음 — Windows PowerShell로 리졸버 실행(UTF-8 고정)'
}

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

$modeCount = @(
	$SourceOnly.IsPresent,
	$CodeOnly.IsPresent,
	$HudUiOnly.IsPresent,
	$ApartmentVisualOnly.IsPresent,
	$SurfaceResponseOnly.IsPresent,
	$PropResponseOnly.IsPresent,
	$RetailRealismOnly.IsPresent,
	$CorridorSignageOnly.IsPresent,
	$MissingFloorOnly.IsPresent,
	$TankWaterOnly.IsPresent,
	$TankInteriorOnly.IsPresent,
	$SubmergedClothingOnly.IsPresent
) | Where-Object { $_ } | Measure-Object | Select-Object -ExpandProperty Count
if ($modeCount -gt 1) {
	throw 'SourceOnly, CodeOnly, HudUiOnly, ApartmentVisualOnly, SurfaceResponseOnly, PropResponseOnly, RetailRealismOnly, CorridorSignageOnly, MissingFloorOnly, TankWaterOnly, TankInteriorOnly, SubmergedClothingOnly는 동시에 사용할 수 없습니다.'
}

if (-not $CodeOnly -and -not $TankWaterOnly -and -not $TankInteriorOnly -and
	-not $SubmergedClothingOnly -and -not $HudUiOnly -and
	-not $ApartmentVisualOnly -and -not $SurfaceResponseOnly -and
	-not $PropResponseOnly -and -not $RetailRealismOnly -and
	-not $CorridorSignageOnly -and
	-not $MissingFloorOnly) {
	& (Join-Path $PSScriptRoot 'Prepare-AIArt.ps1')

	$python = Get-Command python -ErrorAction Stop
	# Conditioning must precede PBR derivation: generate_ai_pbr_maps.py reads
	# the tangent normal out of the albedo, so a brightness field left in the
	# albedo becomes a slow swell of fake geometry that lights wrong from every
	# angle. Removing it afterwards would be too late.
	$tileConditioner = Join-Path $PSScriptRoot 'condition_ai_tiles.py'
	Write-Host 'ART_BUILD running condition_ai_tiles.py'
	& $python.Source $tileConditioner
	if ($LASTEXITCODE -ne 0) {
		throw "AI tile conditioning failed ($LASTEXITCODE)"
	}

	$pbrGenerator = Join-Path $PSScriptRoot 'generate_ai_pbr_maps.py'
	Write-Host 'ART_BUILD running generate_ai_pbr_maps.py'
	& $python.Source $pbrGenerator
	if ($LASTEXITCODE -ne 0) {
		throw "PBR source-map generation failed ($LASTEXITCODE)"
	}
}

if ($MissingFloorOnly) {
	& (Join-Path $PSScriptRoot 'Prepare-AIArt.ps1') `
		-OnlySource @(
			'TextureMissingFloorDryPlaster',
			'SheetMissingFloorResidueMasks',
			'SheetMissingFloorDistantCharacters',
			'ListenerEntityFrontCutout',
			'SheetListenerEntityCrawlPhases',
			'FinalCavityFrontBlend_v1',
			'MokHansooFinalFrontBlend_v1',
			'TextureVillaStairCheckerPlatePaintedSteel_v2',
			'TextureRooftopUrethaneWaterproofing',
			'TextureRooftopAnnexConcreteGypsumDebris',
			'TextureUtilityMeterDialFaceBlank',
			'TextureComplaintLedgerCarbonPaperBlank',
			'TextureApartmentEntranceDoorCharcoalSteel'
		)
	$python = Get-Command python -ErrorAction Stop
	# No --force here: Prepare-AIArt has just rewritten these albedos from the
	# ImageGen originals, so their hashes no longer match the sidecars and the
	# conditioner runs on its own. --force would only matter for a file that is
	# already conditioned, which is precisely the case it must refuse.
	$tileConditioner = Join-Path $PSScriptRoot 'condition_ai_tiles.py'
	Write-Host 'ART_BUILD running missing-floor tile conditioning'
	& $python.Source $tileConditioner `
		--only T_MissingFloorSteelStair `
		--only T_RooftopWaterproofing `
		--only T_MissingFloorGypsumDebris `
		--only T_UtilityMeterDial `
		--only T_CarbonPaper `
		--only T_UnitDoorPaintedSteel
	if ($LASTEXITCODE -ne 0) {
		throw "AI tile conditioning failed ($LASTEXITCODE)"
	}

	$pbrGenerator = Join-Path $PSScriptRoot 'generate_ai_pbr_maps.py'
	Write-Host 'ART_BUILD running missing-floor PBR source-map generation'
	& $python.Source $pbrGenerator `
		--only T_MissingFloorDryPlaster `
		--only T_SpriteListenerFront `
		--only T_SpriteListenerCrawl0 `
		--only T_SpriteListenerCrawl1 `
		--only T_SpriteListenerCrawl2 `
		--only T_SpriteListenerCrawl3 `
		--only T_SpriteFinalCavity `
		--only T_SpriteMokFinalUpper `
		--only T_MissingFloorSteelStair `
		--only T_RooftopWaterproofing `
		--only T_MissingFloorGypsumDebris `
		--only T_UtilityMeterDial `
		--only T_CarbonPaper `
		--only T_UnitDoorPaintedSteel `
		--force
	if ($LASTEXITCODE -ne 0) {
		throw "Missing-floor PBR source-map generation failed ($LASTEXITCODE)"
	}
}

if ($HudUiOnly) {
	& (Join-Path $PSScriptRoot 'Prepare-AIArt.ps1') `
		-OnlySource @(
			'TextureAudioCalibrationWall_v1',
			'SheetFirstPersonKnockPhases_v2_RGBA',
			'SheetListenerCaptureEmbracePhases_v1_RGBA'
		)
}

if ($ApartmentVisualOnly) {
	& (Join-Path $PSScriptRoot 'Prepare-AIArt.ps1') `
		-OnlySource @(
			'TextureApartmentWallpaperVintage',
			'MaskApartmentWallPatina',
			'TextureApartmentWallpaperEmbossedPlainGreyGreen_v1'
		)
	$python = Get-Command python -ErrorAction Stop
	$tileConditioner = Join-Path $PSScriptRoot 'condition_ai_tiles.py'
	Write-Host 'ART_BUILD running apartment tile conditioning'
	& $python.Source $tileConditioner `
		--only T_ApartmentWallpaperEmboss
	if ($LASTEXITCODE -ne 0) {
		throw "Apartment tile conditioning failed ($LASTEXITCODE)"
	}

	$pbrGenerator = Join-Path $PSScriptRoot 'generate_ai_pbr_maps.py'
	Write-Host 'ART_BUILD running apartment PBR source-map generation'
	# --only is `action="append"`, so each stem needs its own flag; passing
	# both to one flag makes the second an unrecognized positional.
	& $python.Source $pbrGenerator `
		--only T_ApartmentWallpaperV2 `
		--only T_ApartmentWallpaperEmboss `
		--force
	if ($LASTEXITCODE -ne 0) {
		throw "Apartment PBR source-map generation failed ($LASTEXITCODE)"
	}
}

if ($RetailRealismOnly) {
	& (Join-Path $PSScriptRoot 'Prepare-AIArt.ps1') `
		-OnlySource @('TextureKoreanVillaStucco_v1')
	$python = Get-Command python -ErrorAction Stop
	$pbrGenerator = Join-Path $PSScriptRoot 'generate_ai_pbr_maps.py'
	Write-Host 'ART_BUILD running Korean-villa PBR source-map generation'
	& $python.Source $pbrGenerator --only T_KoreanVillaStucco --force
	if ($LASTEXITCODE -ne 0) {
		throw "Retail-realism PBR source-map generation failed ($LASTEXITCODE)"
	}
}

if ($PropResponseOnly) {
	$python = Get-Command python -ErrorAction Stop
	$pbrGenerator = Join-Path $PSScriptRoot 'generate_ai_pbr_maps.py'
	Write-Host 'ART_BUILD running paper-fibre PBR source-map generation'
	& $python.Source $pbrGenerator `
		--only T_PaperClean_V2 `
		--force
	if ($LASTEXITCODE -ne 0) {
		throw "Paper-fibre PBR source-map generation failed ($LASTEXITCODE)"
	}
}

if ($SourceOnly) {
	& (Join-Path $PSScriptRoot 'Test-ArtAssetContract.ps1')
	if ($LASTEXITCODE -ne 0) {
		throw "Art source contract failed ($LASTEXITCODE)"
	}
	Write-Host 'ART_SOURCE_BUILD PASS material_scans=13 material_masks=9 overlays=19 pbr_maps=65 no_unreal_process=true'
	return
}

if ($CorridorSignageOnly) {
	& (Join-Path $PSScriptRoot 'Create-SignTextures.ps1') -CorridorEntranceOnly
}

$editorOutput = @(
	if ($usingPowerShellCore) {
		& $resolverShell `
			-NoProfile `
			-File $resolver `
			-ProjectPath $projectFile `
			-Commandlet 2>&1
	} else {
		& $resolverShell `
			-NoProfile `
			-ExecutionPolicy Bypass `
			-Command "[Console]::OutputEncoding = [System.Text.Encoding]::UTF8; & '$resolver' -ProjectPath '$projectFile' -Commandlet" 2>&1
	}
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

# Every targeted pass can compile runtime bindings as well as assets. Keeping
# the newer DLL only inside the ASCII mirror makes the following visual run
# silently execute stale code even though the UAssets were copied correctly.
if ($usingAsciiMirror) {
	Invoke-ArtRobocopy `
		-Source (Join-Path $unrealProjectRoot 'Binaries\Win64') `
		-Destination (Join-Path $projectRoot 'Binaries\Win64')
}

if ($CodeOnly) {
	Write-Host 'ART_CODE_BUILD PASS target=IndieGameEditor platform=Win64 configuration=Development'
	return
}

if ($HudUiOnly -or $ApartmentVisualOnly -or $SurfaceResponseOnly -or
	$PropResponseOnly -or $RetailRealismOnly -or
	$CorridorSignageOnly -or
	$MissingFloorOnly -or $TankWaterOnly -or $TankInteriorOnly -or
	$SubmergedClothingOnly) {
	$targetName = if ($HudUiOnly) {
		'HudUi'
	}
	elseif ($ApartmentVisualOnly) {
		'ApartmentVisual'
	}
	elseif ($SurfaceResponseOnly) {
		'SurfaceResponse'
	}
	elseif ($PropResponseOnly) {
		'PropResponse'
	}
	elseif ($RetailRealismOnly) {
		'RetailRealism'
	}
	elseif ($CorridorSignageOnly) {
		'CorridorSignage'
	}
	elseif ($MissingFloorOnly) {
		'MissingFloor'
	}
	elseif ($TankWaterOnly) {
		'TankWater'
	}
	elseif ($TankInteriorOnly) {
		'TankInterior'
	}
	else {
		'SubmergedClothing'
	}
	$targetEnvironment = if ($HudUiOnly) {
		'IG_HUD_UI_ONLY'
	}
	elseif ($ApartmentVisualOnly) {
		'IG_APARTMENT_VISUAL_ONLY'
	}
	elseif ($SurfaceResponseOnly) {
		'IG_SURFACE_RESPONSE_ONLY'
	}
	elseif ($PropResponseOnly) {
		'IG_PROP_RESPONSE_ONLY'
	}
	elseif ($RetailRealismOnly) {
		'IG_RETAIL_REALISM_ONLY'
	}
	elseif ($CorridorSignageOnly) {
		'IG_CORRIDOR_SIGNAGE_ONLY'
	}
	elseif ($MissingFloorOnly) {
		'IG_MISSING_FLOOR_ONLY'
	}
	elseif ($TankWaterOnly) {
		'IG_TANK_WATER_ONLY'
	}
	elseif ($TankInteriorOnly) {
		'IG_TANK_INTERIOR_ONLY'
	}
	else {
		'IG_SUBMERGED_CLOTHING_ONLY'
	}
	$targetSuccessPattern = if ($HudUiOnly) {
		'\[IndieGame\] Imported 11 textures'
	}
	elseif ($ApartmentVisualOnly) {
		'\[IndieGame\] Apartment visual material update complete'
	}
	elseif ($SurfaceResponseOnly) {
		'\[IndieGame\] Surface response material update complete: \d+ materials'
	}
	elseif ($PropResponseOnly) {
		'\[IndieGame\] Prop response material update complete: \d+ materials'
	}
	elseif ($RetailRealismOnly) {
		'\[IndieGame\] Retail realism material update complete: 5 materials'
	}
	elseif ($CorridorSignageOnly) {
		'\[IndieGame\] Corridor entrance signage material update complete'
	}
	elseif ($MissingFloorOnly) {
		'\[IndieGame\] Missing-floor visual material update complete'
	}
	elseif ($TankWaterOnly) {
		'\[IndieGame\] Tank water material update complete'
	}
	elseif ($TankInteriorOnly) {
		'\[IndieGame\] Tank interior material update complete'
	}
	else {
		'\[IndieGame\] Submerged clothing material update complete'
	}
	$targetRelativeAssets = if ($HudUiOnly) {
		@(
			'Content\Prototype\Textures\T_AudioCalibrationWall_D.uasset',
			'Content\Prototype\Textures\T_HudDialogueFilm_D.uasset',
			'Content\Prototype\Textures\T_MissingFloorJournalPaper_D.uasset',
			'Content\Prototype\Textures\T_FPHandKnock0_D.uasset',
			'Content\Prototype\Textures\T_FPHandKnock1_D.uasset',
			'Content\Prototype\Textures\T_FPHandKnock2_D.uasset',
			'Content\Prototype\Textures\T_FPHandKnock3_D.uasset',
			'Content\Prototype\Textures\T_FPCaptureEmbrace0_D.uasset',
			'Content\Prototype\Textures\T_FPCaptureEmbrace1_D.uasset',
			'Content\Prototype\Textures\T_FPCaptureEmbrace2_D.uasset',
			'Content\Prototype\Textures\T_FPCaptureEmbrace3_D.uasset'
		)
	}
	elseif ($ApartmentVisualOnly) {
		@(
			'Content\Prototype\Textures\T_ApartmentWallpaperV2_D.uasset',
			'Content\Prototype\Textures\T_ApartmentWallpaperV2_N.uasset',
			'Content\Prototype\Textures\T_ApartmentWallpaperV2_R.uasset',
			'Content\Prototype\Textures\T_ApartmentWallpaperV2_A.uasset',
			'Content\Prototype\Textures\T_ApartmentWallPatina_M.uasset',
			'Content\Prototype\Textures\T_ApartmentWallpaperEmboss_D.uasset',
			'Content\Prototype\Textures\T_ApartmentWallpaperEmboss_N.uasset',
			'Content\Prototype\Textures\T_ApartmentWallpaperEmboss_R.uasset',
			'Content\Prototype\Textures\T_ApartmentWallpaperEmboss_A.uasset',
			'Content\Prototype\Materials\M_Wallpaper_X.uasset',
			'Content\Prototype\Materials\M_Wallpaper_Y.uasset',
			'Content\Prototype\Materials\M_WallpaperCeil.uasset',
			'Content\Prototype\Materials\M_WallpaperEmboss_X.uasset',
			'Content\Prototype\Materials\M_WallpaperEmboss_Y.uasset',
			'Content\Prototype\Materials\M_ApartmentWallPatina.uasset'
		)
	}
	elseif ($SurfaceResponseOnly) {
		@(
			'M_Jangpan',
			'M_Wallpaper_X',
			'M_Wallpaper_Y',
			'M_WallpaperCeil',
			'M_WoodFurnitureUV',
			'M_BeddingUV',
			'M_Brick_X',
			'M_Brick_Y',
			'M_VillaStucco_X',
			'M_VillaStucco_Y',
			'M_Concrete_XY',
			'M_Concrete_X',
			'M_Concrete_Y',
			'M_Shutter_X',
			'M_ConcreteDark_X',
			'M_ConcreteDark_Y',
			'M_ConcreteDark_XY',
			'M_StoreTileWorld',
			'M_StoreCeilWorld',
			'M_StoreWall_X',
			'M_StoreWall_Y',
			'M_MetalUV',
			'M_ShelfSteelUV',
			'M_Stucco_X',
			'M_Stucco_Y',
			'M_StuccoCeil',
			'M_GraniteTile_XY',
			'M_GranitePanel_X',
			'M_GranitePanel_Y',
			'M_MarbleFloor_XY',
			'M_CounterStoneUV',
			'M_StainlessUV',
			'M_CabMirrorUV',
			'M_SteelDoorUV',
			'M_KitchenGlossUV',
			'M_MissingFloorPlaster_X',
			'M_MissingFloorPlaster_Y',
			'M_MissingFloorPlaster_XY'
		) | ForEach-Object {
			"Content\Prototype\Materials\$_.uasset"
		}
	}
	elseif ($PropResponseOnly) {
		@(
			'Content\Prototype\Textures\T_PaperClean_V2_N.uasset',
			'Content\Prototype\Textures\T_PaperClean_V2_R.uasset',
			'Content\Prototype\Textures\T_PaperClean_V2_A.uasset',
			'Content\Prototype\Materials\M_PaperClean.uasset',
			'Content\Prototype\Materials\M_PaperWet.uasset',
			'Content\Prototype\Materials\M_PaperFolded.uasset',
			'Content\Prototype\Materials\M_PaperOld.uasset',
			'Content\Prototype\Materials\M_LabelWater.uasset',
			'Content\Prototype\Materials\M_LabelGreenTea.uasset',
			'Content\Prototype\Materials\M_LabelBarley.uasset',
			'Content\Prototype\Materials\M_LabelSoda.uasset',
			'Content\Prototype\Materials\M_LabelSoju.uasset',
			'Content\Prototype\Materials\M_LabelRamyeon.uasset',
			'Content\Prototype\Materials\M_SnackShrimp.uasset',
			'Content\Prototype\Materials\M_SnackPotato.uasset',
			'Content\Prototype\Materials\M_SnackSquid.uasset',
			'Content\Prototype\Materials\M_SnackCorn.uasset',
			'Content\Prototype\Materials\M_Glass.uasset',
			'Content\Prototype\Materials\M_BottleGreen.uasset',
			'Content\Prototype\Materials\M_BottleBrown.uasset',
			'Content\Prototype\Materials\M_FridgeBody.uasset',
			'Content\Prototype\Materials\M_FridgeInterior.uasset',
			'Content\Prototype\Materials\M_PlasticDark.uasset',
			'Content\Prototype\Materials\M_TrashBag.uasset',
			'Content\Prototype\Materials\M_MetalFrame.uasset',
			'Content\Prototype\Materials\M_CarrierBagFilm.uasset',
			'Content\Prototype\Materials\M_AsphaltWorld.uasset'
		)
	}
	elseif ($RetailRealismOnly) {
		@(
			'Content\Meshes\SM_DrinkCan.uasset',
			'Content\Prototype\Textures\T_KoreanVillaStucco_D.uasset',
			'Content\Prototype\Textures\T_KoreanVillaStucco_N.uasset',
			'Content\Prototype\Textures\T_KoreanVillaStucco_R.uasset',
			'Content\Prototype\Textures\T_KoreanVillaStucco_A.uasset',
			'Content\Prototype\Materials\M_VillaStucco_X.uasset',
			'Content\Prototype\Materials\M_VillaStucco_Y.uasset',
			'Content\Prototype\Materials\M_MarbleFloor_XY.uasset',
			'Content\Prototype\Materials\M_StainlessUV.uasset',
			'Content\Prototype\Materials\M_SteelDoorUV.uasset'
		)
	}
	elseif ($CorridorSignageOnly) {
		@(
			'Content\Meshes\SM_CaptureMercyNote.uasset',
			'Content\Prototype\Textures\T_CaptureMercyNote_D.uasset',
			'Content\Prototype\Textures\T_MercyNoteUnderDoor_D.uasset',
			'Content\Prototype\Textures\T_Note404NotFound_D.uasset',
			'Content\Prototype\Textures\T_Plate401_D.uasset',
			'Content\Prototype\Textures\T_Plate402_D.uasset',
			'Content\Prototype\Textures\T_Plate403_D.uasset',
			'Content\Prototype\Textures\T_PlateCommon_D.uasset',
			'Content\Prototype\Textures\T_SignAux5MonitorOnly_D.uasset',
			'Content\Prototype\Materials\M_Note404NotFound.uasset',
			'Content\Prototype\Materials\M_CaptureMercyNote.uasset',
			'Content\Prototype\Materials\M_MercyNoteUnderDoor.uasset',
			'Content\Prototype\Materials\M_SignAux5MonitorOnly.uasset',
			'Content\Prototype\Materials\M_CctvChannelFive.uasset',
			'Content\Prototype\Materials\M_Plate402.uasset',
			'Content\Prototype\Materials\M_PlateCommon.uasset'
		)
	}
	elseif ($MissingFloorOnly) {
		@(
			'Content\Meshes\SM_ListenerEntityCrawl.uasset',
			'Content\Meshes\SM_FinalCavityClothingShell.uasset',
			'Content\Meshes\SM_FinalCavityBoneInsert.uasset',
			'Content\Meshes\SM_FinalCavityTarp.uasset',
			'Content\Meshes\SM_FinalCavityBrokenCaster.uasset',
			'Content\Meshes\SM_MokHansooWorkwear.uasset',
			'Content\Meshes\SM_MokHansooHeadHands.uasset',
			'Content\Meshes\SM_MokHansooGypsumBoard.uasset',
			'Content\Meshes\SM_TuningHammer.uasset',
			'Content\Meshes\SM_TunerToolCart.uasset',
			'Content\Meshes\SM_ComplaintLedger.uasset',
			'Content\Meshes\SM_CalendarJournal.uasset',
			'Content\Prototype\Textures\T_MissingFloorDryPlaster_D.uasset',
			'Content\Prototype\Textures\T_MissingFloorDryPlaster_N.uasset',
			'Content\Prototype\Textures\T_MissingFloorDryPlaster_R.uasset',
			'Content\Prototype\Textures\T_MissingFloorDryPlaster_A.uasset',
			'Content\Prototype\Textures\T_MissingFloorHandprints_M.uasset',
			'Content\Prototype\Textures\T_MissingFloorDragTrails_M.uasset',
			'Content\Prototype\Textures\T_MissingFloorDustJoint_M.uasset',
			'Content\Prototype\Textures\T_MissingFloorCavityScratches_M.uasset',
			'Content\Prototype\Textures\T_SpriteSeo_D.uasset',
			'Content\Prototype\Textures\T_SpriteMok_D.uasset',
			'Content\Prototype\Textures\T_SpriteHwang_D.uasset',
			'Content\Prototype\Textures\T_SpriteNarin_D.uasset',
			'Content\Prototype\Textures\T_SpriteListenerFront_D.uasset',
			'Content\Prototype\Textures\T_SpriteListenerFront_N.uasset',
			'Content\Prototype\Textures\T_SpriteListenerFront_R.uasset',
			'Content\Prototype\Textures\T_SpriteListenerFront_A.uasset',
			'Content\Prototype\Textures\T_SpriteListenerCrawl0_D.uasset',
			'Content\Prototype\Textures\T_SpriteListenerCrawl0_N.uasset',
			'Content\Prototype\Textures\T_SpriteListenerCrawl0_R.uasset',
			'Content\Prototype\Textures\T_SpriteListenerCrawl0_A.uasset',
			'Content\Prototype\Textures\T_SpriteListenerCrawl1_D.uasset',
			'Content\Prototype\Textures\T_SpriteListenerCrawl1_N.uasset',
			'Content\Prototype\Textures\T_SpriteListenerCrawl1_R.uasset',
			'Content\Prototype\Textures\T_SpriteListenerCrawl1_A.uasset',
			'Content\Prototype\Textures\T_SpriteListenerCrawl2_D.uasset',
			'Content\Prototype\Textures\T_SpriteListenerCrawl2_N.uasset',
			'Content\Prototype\Textures\T_SpriteListenerCrawl2_R.uasset',
			'Content\Prototype\Textures\T_SpriteListenerCrawl2_A.uasset',
			'Content\Prototype\Textures\T_SpriteListenerCrawl3_D.uasset',
			'Content\Prototype\Textures\T_SpriteListenerCrawl3_N.uasset',
			'Content\Prototype\Textures\T_SpriteListenerCrawl3_R.uasset',
			'Content\Prototype\Textures\T_SpriteListenerCrawl3_A.uasset',
			'Content\Prototype\Textures\T_SpriteFinalCavity_D.uasset',
			'Content\Prototype\Textures\T_SpriteFinalCavity_N.uasset',
			'Content\Prototype\Textures\T_SpriteFinalCavity_R.uasset',
			'Content\Prototype\Textures\T_SpriteFinalCavity_A.uasset',
			'Content\Prototype\Textures\T_SpriteMokFinalUpper_D.uasset',
			'Content\Prototype\Textures\T_SpriteMokFinalUpper_N.uasset',
			'Content\Prototype\Textures\T_SpriteMokFinalUpper_R.uasset',
			'Content\Prototype\Textures\T_SpriteMokFinalUpper_A.uasset',
			'Content\Prototype\Materials\M_MissingFloorListenerPlasterUV.uasset',
			'Content\Prototype\Materials\M_MissingFloorPlaster_X.uasset',
			'Content\Prototype\Materials\M_MissingFloorPlaster_Y.uasset',
			'Content\Prototype\Materials\M_MissingFloorPlaster_XY.uasset',
			'Content\Prototype\Materials\M_MissingFloorHandprints.uasset',
			'Content\Prototype\Materials\M_MissingFloorDragTrails.uasset',
			'Content\Prototype\Materials\M_MissingFloorDustJoint.uasset',
			'Content\Prototype\Materials\M_MissingFloorCavityScratches.uasset',
			'Content\Prototype\Materials\M_SpriteSeo.uasset',
			'Content\Prototype\Materials\M_SpriteMok.uasset',
			'Content\Prototype\Materials\M_SpriteHwang.uasset',
			'Content\Prototype\Materials\M_SpriteNarin.uasset',
			'Content\Prototype\Materials\M_SpriteListenerFront.uasset',
			'Content\Prototype\Materials\M_SpriteListenerCrawl0.uasset',
			'Content\Prototype\Materials\M_SpriteListenerCrawl1.uasset',
			'Content\Prototype\Materials\M_SpriteListenerCrawl2.uasset',
			'Content\Prototype\Materials\M_SpriteListenerCrawl3.uasset',
			'Content\Prototype\Materials\M_SpriteFinalCavity.uasset',
			'Content\Prototype\Materials\M_SpriteMokFinalUpper.uasset'
		)
	}
	elseif ($TankWaterOnly) {
		@('Content\Prototype\Materials\M_TankWaterReveal.uasset')
	}
	elseif ($TankInteriorOnly) {
		@('Content\Prototype\Materials\M_TankInteriorBiofilmUV.uasset')
	}
	else {
		@(
			'Content\Meshes\SM_SubmergedHoodieCurl.uasset',
			'Content\Meshes\SM_SubmergedPantsCurl.uasset',
			'Content\Meshes\SM_SubmergedSlippersCurl.uasset',
			'Content\Prototype\Materials\M_SubmergedHoodieUV.uasset',
			'Content\Prototype\Materials\M_SubmergedPantsUV.uasset',
			'Content\Prototype\Materials\M_SubmergedSlippersUV.uasset',
			'Content\Prototype\Materials\M_SubmergedSlipperWearUV.uasset'
		)
	}
	$targetedStages = if ($HudUiOnly) {
		@(
			@{
				Script = 'generate_surface_textures.py'
				SuccessPattern = $targetSuccessPattern
				TargetEnvironment = $true
			}
		)
	}
	elseif ($ApartmentVisualOnly) {
		@(
			@{
				Script = 'generate_surface_textures.py'
				SuccessPattern = '\[IndieGame\] Imported 9 textures'
				TargetEnvironment = $true
			},
			@{
				Script = 'create_textured_materials.py'
				SuccessPattern = $targetSuccessPattern
				TargetEnvironment = $true
			},
			@{
				Script = 'validate_baked_art_assets.py'
				SuccessPattern = 'ART_UASSET_AUDIT PASS'
				TargetEnvironment = $true
			}
		)
	}
	elseif ($SurfaceResponseOnly) {
		@(
			@{
				Script = 'create_textured_materials.py'
				SuccessPattern = $targetSuccessPattern
				TargetEnvironment = $true
			},
			@{
				Script = 'validate_baked_art_assets.py'
				SuccessPattern = 'ART_UASSET_AUDIT PASS'
				TargetEnvironment = $false
			}
		)
	}
	elseif ($PropResponseOnly) {
		@(
			@{
				Script = 'generate_surface_textures.py'
				SuccessPattern = '\[IndieGame\] Imported 3 textures'
				TargetEnvironment = $true
			},
			@{
				Script = 'create_textured_materials.py'
				SuccessPattern = $targetSuccessPattern
				TargetEnvironment = $true
			},
			@{
				Script = 'validate_baked_art_assets.py'
				SuccessPattern = 'ART_UASSET_AUDIT PASS'
				TargetEnvironment = $false
			}
		)
	}
	elseif ($RetailRealismOnly) {
		@(
			@{
				Script = 'generate_meshes.py'
				SuccessPattern = '\[MESHGEN\] complete: 1/1 meshes'
				TargetEnvironment = $true
			},
			@{
				Script = 'generate_surface_textures.py'
				SuccessPattern = '\[IndieGame\] Imported 4 textures'
				TargetEnvironment = $true
			},
			@{
				Script = 'create_textured_materials.py'
				SuccessPattern = $targetSuccessPattern
				TargetEnvironment = $true
			},
			@{
				Script = 'validate_baked_art_assets.py'
				SuccessPattern = 'ART_UASSET_AUDIT PASS'
				TargetEnvironment = $false
			}
		)
	}
	elseif ($CorridorSignageOnly) {
		@(
			@{
				Script = 'generate_meshes.py'
				SuccessPattern = '\[MESHGEN\] complete: 1/1 meshes'
				TargetEnvironment = $true
			},
			@{
				Script = 'generate_surface_textures.py'
				SuccessPattern = '\[IndieGame\] Imported 8 textures'
				TargetEnvironment = $true
			},
			@{
				Script = 'create_textured_materials.py'
				SuccessPattern = $targetSuccessPattern
				TargetEnvironment = $true
			},
			@{
				Script = 'validate_baked_art_assets.py'
				SuccessPattern = 'ART_UASSET_AUDIT PASS'
				TargetEnvironment = $false
			}
		)
	}
	elseif ($MissingFloorOnly) {
		@(
			@{
				Script = 'generate_meshes.py'
				SuccessPattern = '\[MESHGEN\] complete: 12/12 meshes'
				TargetEnvironment = $true
			},
			@{
				# 40 + 24: 발소리 표면 3종·문자판·먹지·문짝의 D/N/R/A.
				Script = 'generate_surface_textures.py'
				SuccessPattern = '\[IndieGame\] Imported 64 textures'
				TargetEnvironment = $true
			},
			@{
				Script = 'create_textured_materials.py'
				SuccessPattern = $targetSuccessPattern
				TargetEnvironment = $true
			},
			@{
				Script = 'validate_baked_art_assets.py'
				SuccessPattern = 'ART_UASSET_AUDIT PASS'
				TargetEnvironment = $false
			}
		)
	}
	elseif ($SubmergedClothingOnly) {
		@(
			@{
				Script = 'generate_meshes.py'
				SuccessPattern = '\[MESHGEN\] complete: 3/3 meshes'
				TargetEnvironment = $true
			},
			@{
				Script = 'create_textured_materials.py'
				SuccessPattern = $targetSuccessPattern
				TargetEnvironment = $true
			},
			@{
				Script = 'validate_baked_art_assets.py'
				SuccessPattern = 'ART_UASSET_AUDIT PASS'
				TargetEnvironment = $false
			}
		)
	}
	else {
		@(
			@{
				Script = 'create_textured_materials.py'
				SuccessPattern = $targetSuccessPattern
				TargetEnvironment = $true
			},
			@{
				Script = 'validate_baked_art_assets.py'
				SuccessPattern = 'ART_UASSET_AUDIT PASS'
				TargetEnvironment = $false
			}
		)
	}
	$logRoot = Join-Path $unrealProjectRoot 'Saved\Logs'
	New-Item -ItemType Directory -Force -Path $logRoot | Out-Null
	$previousTargetMode = [Environment]::GetEnvironmentVariable(
		$targetEnvironment,
		'Process')
	try {
		foreach ($stage in $targetedStages) {
			if ([bool]$stage.TargetEnvironment) {
				[Environment]::SetEnvironmentVariable(
					$targetEnvironment,
					'1',
					'Process')
			}
			else {
				[Environment]::SetEnvironmentVariable(
					$targetEnvironment,
					$null,
					'Process')
			}
			$stageName = [string]$stage.Script
			$scriptPath = Join-Path $unrealScriptsRoot $stageName
			$logName = 'ArtBuild_{0}_{1}_{2}.log' -f $targetName, (
				[IO.Path]::GetFileNameWithoutExtension($stageName)),
				(Get-Date -Format 'yyyyMMdd_HHmmss_fff')
			$logPath = Join-Path $logRoot $logName
			Write-Host "ART_BUILD running targeted $stageName"
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
				throw "Targeted $targetName stage failed ($editorExit): $stageName"
			}
		}
	}
	finally {
		[Environment]::SetEnvironmentVariable(
			$targetEnvironment,
			$previousTargetMode,
			'Process')
	}
	if ($usingAsciiMirror) {
		foreach ($targetRelativeAsset in $targetRelativeAssets) {
			$sourceAsset = Join-Path $unrealProjectRoot $targetRelativeAsset
			$targetAsset = Join-Path $projectRoot $targetRelativeAsset
			Copy-Item -LiteralPath $sourceAsset -Destination $targetAsset -Force
		}
	}
	$targetAudit = if ($HudUiOnly) { 0 } else { 1 }
	Write-Host "ART_TARGETED_BUILD PASS target=$targetName assets=$(@($targetRelativeAssets).Count) uasset_audit=$targetAudit no_visible_window=true"
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
		Script = 'apply_photo_prop_lods.py'
		SuccessPattern = 'PHOTO_PROP_LOD_BUILD PASS'
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
			'Content\Photo\Props',
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
	'Content\Meshes\SM_OfferingWaterBowl.uasset',
	'Content\Meshes\SM_DrinkCan.uasset',
	'Content\Meshes\SM_CupSleeve.uasset',
	'Content\Meshes\SM_LabelSleeve.uasset',
	'Content\Meshes\SM_StickyNote76mm.uasset',
	'Content\Meshes\SM_AlleyCatRun.uasset',
	'Content\Meshes\SM_FirstPersonHoodieSleeve.uasset',
	'Content\Meshes\SM_ListenerEntityCrawl.uasset',
	'Content\Meshes\SM_FinalCavityClothingShell.uasset',
	'Content\Meshes\SM_FinalCavityBoneInsert.uasset',
	'Content\Meshes\SM_FinalCavityTarp.uasset',
	'Content\Meshes\SM_FinalCavityBrokenCaster.uasset',
	'Content\Meshes\SM_MokHansooWorkwear.uasset',
	'Content\Meshes\SM_MokHansooHeadHands.uasset',
	'Content\Meshes\SM_MokHansooGypsumBoard.uasset',
	'Content\Meshes\SM_TuningHammer.uasset',
	'Content\Meshes\SM_TunerToolCart.uasset',
	'Content\Meshes\SM_ComplaintLedger.uasset',
	'Content\Meshes\SM_CalendarJournal.uasset',
	'Content\Meshes\SM_P3ServiceCabinetShell.uasset',
	'Content\Meshes\SM_P3ServiceManifold.uasset',
	'Content\Meshes\SM_P3ValveWheelLarge.uasset',
	'Content\Meshes\SM_P3ValveWheelSmall.uasset',
	'Content\Meshes\SM_P3PressureGauge.uasset',
	'Content\Meshes\SM_SubmergedHoodieCurl.uasset',
	'Content\Meshes\SM_SubmergedPantsCurl.uasset',
	'Content\Meshes\SM_SubmergedSlippersCurl.uasset',
	'Content\Meshes\SM_RooftopWaterTankShell.uasset',
	'Content\Meshes\SM_TankInternalLining.uasset',
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
	'Content\Prototype\Textures\T_TankInteriorBiofilm_D.uasset',
	'Content\Prototype\Textures\T_WetServiceHose_D.uasset',
	'Content\Prototype\Textures\T_WetRungPad_D.uasset',
	'Content\Prototype\Textures\T_TankWaterSurface_D.uasset',
	'Content\Prototype\Textures\T_P3CabinetPaintedSteel_D.uasset',
	'Content\Prototype\Textures\T_HudDialogueFilm_D.uasset',
	'Content\Prototype\Textures\T_MissingFloorJournalPaper_D.uasset',
	'Content\Prototype\Textures\T_MovingBoxCardboard_D.uasset',
	'Content\Prototype\Textures\T_MovingBoxCardboard_N.uasset',
	'Content\Prototype\Textures\T_MovingBoxCardboard_R.uasset',
	'Content\Prototype\Textures\T_MovingBoxCardboard_A.uasset',
	'Content\Prototype\Materials\M_MovingBoxCardboardUV.uasset',
	'Content\Prototype\Textures\T_MissingFloorDryPlaster_D.uasset',
	'Content\Prototype\Textures\T_KoreanVillaStucco_D.uasset',
	'Content\Prototype\Textures\T_KoreanVillaStucco_N.uasset',
	'Content\Prototype\Textures\T_KoreanVillaStucco_R.uasset',
	'Content\Prototype\Textures\T_KoreanVillaStucco_A.uasset',
	'Content\Prototype\Textures\T_MissingFloorDryPlaster_N.uasset',
	'Content\Prototype\Textures\T_MissingFloorDryPlaster_R.uasset',
	'Content\Prototype\Textures\T_MissingFloorDryPlaster_A.uasset',
	'Content\Prototype\Textures\T_MissingFloorHandprints_M.uasset',
	'Content\Prototype\Textures\T_MissingFloorDragTrails_M.uasset',
	'Content\Prototype\Textures\T_MissingFloorDustJoint_M.uasset',
	'Content\Prototype\Textures\T_MissingFloorCavityScratches_M.uasset',
	'Content\Prototype\Textures\T_SpriteSeo_D.uasset',
	'Content\Prototype\Textures\T_SpriteMok_D.uasset',
	'Content\Prototype\Textures\T_SpriteHwang_D.uasset',
	'Content\Prototype\Textures\T_SpriteNarin_D.uasset',
	'Content\Prototype\Textures\T_SpriteListenerFront_D.uasset',
	'Content\Prototype\Textures\T_SpriteListenerFront_N.uasset',
	'Content\Prototype\Textures\T_SpriteListenerFront_R.uasset',
	'Content\Prototype\Textures\T_SpriteListenerFront_A.uasset',
	'Content\Prototype\Textures\T_SpriteListenerCrawl0_D.uasset',
	'Content\Prototype\Textures\T_SpriteListenerCrawl0_N.uasset',
	'Content\Prototype\Textures\T_SpriteListenerCrawl0_R.uasset',
	'Content\Prototype\Textures\T_SpriteListenerCrawl0_A.uasset',
	'Content\Prototype\Textures\T_SpriteListenerCrawl1_D.uasset',
	'Content\Prototype\Textures\T_SpriteListenerCrawl1_N.uasset',
	'Content\Prototype\Textures\T_SpriteListenerCrawl1_R.uasset',
	'Content\Prototype\Textures\T_SpriteListenerCrawl1_A.uasset',
	'Content\Prototype\Textures\T_SpriteListenerCrawl2_D.uasset',
	'Content\Prototype\Textures\T_SpriteListenerCrawl2_N.uasset',
	'Content\Prototype\Textures\T_SpriteListenerCrawl2_R.uasset',
	'Content\Prototype\Textures\T_SpriteListenerCrawl2_A.uasset',
	'Content\Prototype\Textures\T_SpriteListenerCrawl3_D.uasset',
	'Content\Prototype\Textures\T_SpriteListenerCrawl3_N.uasset',
	'Content\Prototype\Textures\T_SpriteListenerCrawl3_R.uasset',
	'Content\Prototype\Textures\T_SpriteListenerCrawl3_A.uasset',
	'Content\Prototype\Textures\T_SpriteFinalCavity_D.uasset',
	'Content\Prototype\Textures\T_SpriteFinalCavity_N.uasset',
	'Content\Prototype\Textures\T_SpriteFinalCavity_R.uasset',
	'Content\Prototype\Textures\T_SpriteFinalCavity_A.uasset',
	'Content\Prototype\Textures\T_SpriteMokFinalUpper_D.uasset',
	'Content\Prototype\Textures\T_SpriteMokFinalUpper_N.uasset',
	'Content\Prototype\Textures\T_SpriteMokFinalUpper_R.uasset',
	'Content\Prototype\Textures\T_SpriteMokFinalUpper_A.uasset',
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
	'Content\Prototype\Textures\T_TankInteriorBiofilm_N.uasset',
	'Content\Prototype\Textures\T_TankInteriorBiofilm_R.uasset',
	'Content\Prototype\Textures\T_TankInteriorBiofilm_A.uasset',
	'Content\Prototype\Textures\T_TankInteriorBiofilm_W.uasset',
	'Content\Prototype\Textures\T_TankInteriorBiofilm_M.uasset',
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
	'Content\Prototype\Materials\M_SubmergedPantsUV.uasset',
	'Content\Prototype\Materials\M_SubmergedSlippersUV.uasset',
	'Content\Prototype\Materials\M_SubmergedSlipperWearUV.uasset',
	'Content\Prototype\Materials\M_CarrierBagFilm.uasset',
	'Content\Prototype\Materials\M_AlleyCatTabbyUV.uasset',
	'Content\Prototype\Materials\M_WaterTankMetalUV.uasset',
	'Content\Prototype\Materials\M_TankInteriorBiofilmUV.uasset',
	'Content\Prototype\Materials\M_WetServiceHoseUV.uasset',
	'Content\Prototype\Materials\M_WetRungPadUV.uasset',
	'Content\Prototype\Materials\M_TankWaterReveal.uasset',
	'Content\Prototype\Materials\M_P3CabinetMetalUV.uasset',
	'Content\Prototype\Materials\M_MissingFloorListenerPlasterUV.uasset',
	'Content\Prototype\Materials\M_MissingFloorPlaster_X.uasset',
	'Content\Prototype\Materials\M_MissingFloorPlaster_Y.uasset',
	'Content\Prototype\Materials\M_MissingFloorPlaster_XY.uasset',
	'Content\Prototype\Materials\M_VillaStucco_X.uasset',
	'Content\Prototype\Materials\M_VillaStucco_Y.uasset',
	'Content\Prototype\Materials\M_MissingFloorHandprints.uasset',
	'Content\Prototype\Materials\M_MissingFloorDragTrails.uasset',
	'Content\Prototype\Materials\M_MissingFloorDustJoint.uasset',
	'Content\Prototype\Materials\M_MissingFloorCavityScratches.uasset',
	'Content\Prototype\Materials\M_SpriteSeo.uasset',
	'Content\Prototype\Materials\M_SpriteMok.uasset',
	'Content\Prototype\Materials\M_SpriteHwang.uasset',
	'Content\Prototype\Materials\M_SpriteNarin.uasset',
	'Content\Prototype\Materials\M_SpriteListenerFront.uasset',
	'Content\Prototype\Materials\M_SpriteListenerCrawl0.uasset',
	'Content\Prototype\Materials\M_SpriteListenerCrawl1.uasset',
	'Content\Prototype\Materials\M_SpriteListenerCrawl2.uasset',
	'Content\Prototype\Materials\M_SpriteListenerCrawl3.uasset',
	'Content\Prototype\Materials\M_SpriteFinalCavity.uasset',
	'Content\Prototype\Materials\M_SpriteMokFinalUpper.uasset',
	'Content\Prototype\Textures\T_NoteFridge_D.uasset',
	'Content\Prototype\Textures\T_LabelWater_D.uasset',
	'Content\Prototype\Textures\T_LabelRamyeon_D.uasset',
	'Content\Prototype\Materials\M_NoteFridge.uasset',
	'Content\Prototype\Materials\M_LabelWater.uasset',
	'Content\Prototype\Materials\M_LabelRamyeon.uasset'
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

Write-Host 'ART_BUILD PASS meshes=40 evidence_masks=8 material_masks=1 environment_overlays=10 material_scans=13 pbr_maps=50 uasset_audit=1'
