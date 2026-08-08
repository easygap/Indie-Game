[CmdletBinding()]
param()

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Drawing

$projectRoot = Split-Path -Parent $PSScriptRoot
$sourceArt = Join-Path $projectRoot 'Content\SourceArt'

$requiredRaw = @(
	'AI\SheetHorrorEvidenceMasks.png',
	'AI\SheetHorrorSurfaceBlends.png',
	'AI\SheetEvidenceProps.png',
	'AI\SheetAlleyCatPoseReference.png',
	'AI\SheetFirstPersonSleeveReference.png',
	'AI\SheetP3ServiceCabinetReference.png',
	'AI\SheetRooftopFireDoorReference.png',
	'AI\SheetRooftopUnlockedPadlockKeysReference.png',
	'AI\SheetRooftopWaterTankReference.png',
	'AI\SheetTankAccessSafetyHardwareReference.png',
	'AI\SheetTankExteriorAccessStairReference.png',
	'AI\TextureP3CabinetPaintedSteel.png',
	'AI\TextureWetHoodieFabric.png',
	'AI\TextureCarrierBagFilm.png',
	'AI\TextureAlleyCatTabby.png',
	'AI\SheetSubmergedBodyPoseReference.png',
	'AI\TextureWaterTankGalvanized.png',
	'AI\TextureTankInteriorBiofilm.png',
	'AI\SheetAccidentPropsReference.png',
	'AI\TextureWetServiceHoseRubber.png',
	'AI\SheetLadderRungFailureReference.png',
	'AI\TextureWetRungPadRubber.png',
	'AI\TextureTankWaterSurface.png',
	'AI\DialogueHUDConcept_v1.png',
	'AI\TextureHudDialogueFilm.png',
	'AI\ApartmentVisualTarget_v1.png',
	'AI\TextureApartmentWallpaperVintage.png',
	'AI\MaskApartmentWallPatina.png'
)
$requiredMasks = @(
	'T_EvidenceSlipperTrail_M.png',
	'T_EvidenceCatPawTrail_M.png',
	'T_EvidenceHoseDrag_M.png',
	'T_EvidenceHandSmear_M.png'
)
$requiredOverlays = @(
	'T_DecalDampWallpaper_D.png',
	'T_DecalRustFasteners_D.png',
	'T_DecalMineralScale_D.png',
	'T_DecalRainGrime_D.png'
)
$requiredMaterialMasks = @(
	'T_ApartmentWallPatina_M.png'
)
$requiredMaterialTextures = @(
	'T_WetHoodie_D.png',
	'T_CarrierBagFilm_D.png',
	'T_AlleyCatTabby_D.png',
	'T_WaterTankGalvanized_D.png',
	'T_TankInteriorBiofilm_D.png',
	'T_WetServiceHose_D.png',
	'T_WetRungPad_D.png',
	'T_TankWaterSurface_D.png',
	'T_P3CabinetPaintedSteel_D.png',
	'T_HudDialogueFilm_D.png',
	'T_ApartmentWallpaperV2_D.png'
)
$requiredPbrMaps = @(
	'T_WetHoodie_N.png',
	'T_WetHoodie_R.png',
	'T_WetHoodie_A.png',
	'T_WetHoodie_W.png',
	'T_AlleyCatTabby_N.png',
	'T_AlleyCatTabby_R.png',
	'T_AlleyCatTabby_A.png',
	'T_WaterTankGalvanized_N.png',
	'T_WaterTankGalvanized_R.png',
	'T_WaterTankGalvanized_A.png',
	'T_WaterTankGalvanized_W.png',
	'T_WaterTankGalvanized_M.png',
	'T_TankInteriorBiofilm_N.png',
	'T_TankInteriorBiofilm_R.png',
	'T_TankInteriorBiofilm_A.png',
	'T_TankInteriorBiofilm_W.png',
	'T_TankInteriorBiofilm_M.png',
	'T_WetServiceHose_N.png',
	'T_WetServiceHose_R.png',
	'T_WetServiceHose_A.png',
	'T_WetServiceHose_W.png',
	'T_WetRungPad_N.png',
	'T_WetRungPad_R.png',
	'T_WetRungPad_A.png',
	'T_WetRungPad_W.png',
	'T_P3CabinetPaintedSteel_N.png',
	'T_P3CabinetPaintedSteel_R.png',
	'T_P3CabinetPaintedSteel_A.png',
	'T_P3CabinetPaintedSteel_W.png',
	'T_CarrierBagFilm_N.png',
	'T_CarrierBagFilm_R.png',
	'T_CarrierBagFilm_A.png',
	'T_TankWaterSurface_N.png',
	'T_TankWaterSurface_R.png',
	'T_TankWaterSurface_A.png',
	'T_ApartmentWallpaperV2_N.png',
	'T_ApartmentWallpaperV2_R.png',
	'T_ApartmentWallpaperV2_A.png'
)
$requiredDerived = @(
	$requiredMasks + $requiredOverlays + $requiredMaterialMasks +
	$requiredMaterialTextures + $requiredPbrMaps
)
foreach ($relativePath in @($requiredRaw + $requiredDerived)) {
	$path = Join-Path $sourceArt $relativePath
	if (-not (Test-Path -LiteralPath $path -PathType Leaf)) {
		throw "Art source is missing: $relativePath"
	}
	if ((Get-Item -LiteralPath $path).Length -le 1024) {
		throw "Art source is unexpectedly small: $relativePath"
	}
}

foreach ($relativePath in $requiredDerived) {
	$path = Join-Path $sourceArt $relativePath
	$image = [System.Drawing.Bitmap]::FromFile($path)
	try {
		$expectedSize = if (
			$requiredMaterialMasks -contains $relativePath -or
			$requiredMaterialTextures -contains $relativePath -or
			$requiredPbrMaps -contains $relativePath
		) { 1024 } else { 512 }
		if ($image.Width -ne $expectedSize -or $image.Height -ne $expectedSize) {
			throw "Derived art must be ${expectedSize}x${expectedSize}: $relativePath"
		}

		$opaqueSamples = 0
		$transparentSamples = 0
		$visibleMagentaSamples = 0
		$lumaTotal = 0.0
		$lumaMinimum = 255.0
		$lumaMaximum = 0.0
		$colorSamples = 0
		$redTotal = 0.0
		$greenTotal = 0.0
		$blueTotal = 0.0
		for ($y = 0; $y -lt $image.Height; $y += 8) {
			for ($x = 0; $x -lt $image.Width; $x += 8) {
				$pixel = $image.GetPixel($x, $y)
				$luma =
					$pixel.R * 0.2126 +
					$pixel.G * 0.7152 +
					$pixel.B * 0.0722
				$lumaTotal += $luma
				$lumaMinimum = [Math]::Min($lumaMinimum, $luma)
				$lumaMaximum = [Math]::Max($lumaMaximum, $luma)
				$colorSamples++
				$redTotal += $pixel.R
				$greenTotal += $pixel.G
				$blueTotal += $pixel.B
				if ($pixel.A -ge 240) {
					$opaqueSamples++
				}
				elseif ($pixel.A -le 8) {
					$transparentSamples++
				}
				if ($pixel.A -gt 8 -and
					$pixel.R -gt 180 -and
					$pixel.B -gt 160 -and
					$pixel.G -lt 110) {
					$visibleMagentaSamples++
				}
			}
		}

		if ($requiredMasks -contains $relativePath) {
			if ($opaqueSamples -lt 4000) {
				throw "Evidence mask lost its opaque black backing: $relativePath"
			}
		}
		elseif ($requiredOverlays -contains $relativePath) {
			if ($opaqueSamples -lt 8 -or $transparentSamples -lt 512) {
				throw "RGBA overlay lacks usable foreground/background: $relativePath"
			}
			if ($visibleMagentaSamples -gt 0) {
				throw "RGBA overlay retained a visible magenta fringe: $relativePath"
			}
		}
		elseif ($opaqueSamples -lt 12000) {
			throw "Material scan lost its opaque color field: $relativePath"
		}

		if ($requiredMaterialMasks -contains $relativePath) {
			$meanLuma = $lumaTotal / [Math]::Max(1, $colorSamples)
			$dynamicRange = $lumaMaximum - $lumaMinimum
			if ($meanLuma -lt 8 -or $meanLuma -gt 220 -or $dynamicRange -lt 45) {
				throw "Wall-patina mask lost its usable damage range: $relativePath"
			}
		}
		elseif ($requiredMaterialTextures -contains $relativePath) {
			$meanLuma = $lumaTotal / [Math]::Max(1, $colorSamples)
			$dynamicRange = $lumaMaximum - $lumaMinimum
			switch ($relativePath) {
				'T_WetHoodie_D.png' {
					if ($meanLuma -gt 70 -or $dynamicRange -lt 70) {
						throw "Wet hoodie lost its dark soaked-fabric range: $relativePath"
					}
				}
				'T_CarrierBagFilm_D.png' {
					if ($meanLuma -lt 125 -or $meanLuma -gt 185 -or $dynamicRange -lt 55) {
						throw "Carrier film no longer preserves translucent midtones: $relativePath"
					}
				}
				'T_AlleyCatTabby_D.png' {
					if ($meanLuma -lt 45 -or $meanLuma -gt 125 -or $dynamicRange -lt 120) {
						throw "Tabby coat lost its restrained stripe contrast: $relativePath"
					}
				}
				'T_WaterTankGalvanized_D.png' {
					if ($meanLuma -lt 95 -or $meanLuma -gt 190 -or $dynamicRange -lt 45) {
						throw "Tank metal lost its restrained galvanized range: $relativePath"
					}
				}
				'T_TankInteriorBiofilm_D.png' {
					if ($meanLuma -lt 65 -or $meanLuma -gt 160 -or $dynamicRange -lt 45) {
						throw "Tank interior lost its restrained mineral and biofilm range: $relativePath"
					}
				}
				'T_WetServiceHose_D.png' {
					if ($meanLuma -gt 55 -or $dynamicRange -lt 35) {
						throw "Service hose lost its dark wet-rubber range: $relativePath"
					}
				}
				'T_WetRungPad_D.png' {
					if ($meanLuma -gt 65 -or $dynamicRange -lt 40) {
						throw "Rung pad lost its dark ribbed-rubber range: $relativePath"
					}
				}
				'T_TankWaterSurface_D.png' {
					if ($meanLuma -gt 75 -or $dynamicRange -lt 25) {
						throw "Tank water lost its dark low-contrast ripple range: $relativePath"
					}
				}
				'T_P3CabinetPaintedSteel_D.png' {
					if ($meanLuma -lt 120 -or $meanLuma -gt 180 -or $dynamicRange -lt 45) {
						throw "P3 cabinet lost its restrained painted-steel range: $relativePath"
					}
				}
				'T_HudDialogueFilm_D.png' {
					if ($meanLuma -lt 12 -or $meanLuma -gt 70 -or
						$dynamicRange -lt 8 -or $dynamicRange -gt 90) {
						throw "Dialogue film lost its restrained near-black UI range: $relativePath"
					}
				}
			}
		}
		elseif ($requiredPbrMaps -contains $relativePath) {
			$meanLuma = $lumaTotal / [Math]::Max(1, $colorSamples)
			$dynamicRange = $lumaMaximum - $lumaMinimum
			if ($relativePath.EndsWith('_N.png')) {
				$meanRed = $redTotal / [Math]::Max(1, $colorSamples)
				$meanGreen = $greenTotal / [Math]::Max(1, $colorSamples)
				$meanBlue = $blueTotal / [Math]::Max(1, $colorSamples)
				if ($meanRed -lt 112 -or $meanRed -gt 143 -or
					$meanGreen -lt 112 -or $meanGreen -gt 143 -or
					$meanBlue -lt 240) {
					throw "Tangent normal map is not neutral/up-facing: $relativePath"
				}
			}
			elseif ($relativePath.EndsWith('_W.png')) {
				if ($dynamicRange -lt 45 -or $meanLuma -gt 150) {
					throw "Wetness mask lacks a restrained local blend range: $relativePath"
				}
			}
			elseif ($relativePath.EndsWith('_M.png')) {
				if ($dynamicRange -lt 35 -or $meanLuma -lt 120) {
					throw "Galvanized metal mask no longer separates rust from metal: $relativePath"
				}
			}
			elseif ($relativePath.EndsWith('_R.png')) {
				$minimumRoughnessRange = if ($relativePath -eq 'T_TankWaterSurface_R.png') { 2 } else { 4 }
				if ($dynamicRange -lt $minimumRoughnessRange -or $meanLuma -lt 12 -or $meanLuma -gt 242) {
					throw "Roughness map lost usable physical variation: $relativePath"
				}
			}
			elseif ($relativePath.EndsWith('_A.png') -and $meanLuma -lt 160) {
				throw "Ambient-occlusion map is crushing indirect light: $relativePath"
			}
		}
	}
	finally {
		$image.Dispose()
	}
}

$materialScript = Get-Content -Raw -Encoding UTF8 -LiteralPath (
	Join-Path $projectRoot 'Scripts\create_textured_materials.py')
$pbrScript = Get-Content -Raw -Encoding UTF8 -LiteralPath (
	Join-Path $projectRoot 'Scripts\generate_ai_pbr_maps.py')
$meshScript = Get-Content -Raw -Encoding UTF8 -LiteralPath (
	Join-Path $projectRoot 'Scripts\generate_meshes.py')
$directorSource = Get-Content -Raw -Encoding UTF8 -LiteralPath (
	Join-Path $projectRoot 'Source\IndieGame\Sequence\IGThirdMorningDirector.cpp')
$neighborhoodSource = Get-Content -Raw -Encoding UTF8 -LiteralPath (
	Join-Path $projectRoot 'Source\IndieGame\Environment\IGNeighborhoodLifeDirector.cpp')
$prologueSource = Get-Content -Raw -Encoding UTF8 -LiteralPath (
	Join-Path $projectRoot 'Source\IndieGame\Core\IGPrologueWorldScene.cpp')
$playerSource = Get-Content -Raw -Encoding UTF8 -LiteralPath (
	Join-Path $projectRoot 'Source\IndieGame\Player\IGPlayerCharacter.cpp')
$buildScript = Get-Content -Raw -Encoding UTF8 -LiteralPath (
	Join-Path $projectRoot 'Scripts\Build-ArtAssets.ps1')
$auditScript = Get-Content -Raw -Encoding UTF8 -LiteralPath (
	Join-Path $projectRoot 'Scripts\validate_baked_art_assets.py')
$photoLodContract = Get-Content -Raw -Encoding UTF8 -LiteralPath (
	Join-Path $projectRoot 'Scripts\photo_prop_lod_contract.py')
$photoLodApplyScript = Get-Content -Raw -Encoding UTF8 -LiteralPath (
	Join-Path $projectRoot 'Scripts\apply_photo_prop_lods.py')
$photoImportScript = Get-Content -Raw -Encoding UTF8 -LiteralPath (
	Join-Path $projectRoot 'Scripts\import_photo_props.py')

foreach ($token in @(
	'pitch=rotation[0]',
	'yaw=rotation[1]',
	'roll=rotation[2]'
)) {
	if (-not $meshScript.Contains($token)) {
		throw "Mesh transform axis contract is missing: $token"
	}
}
if ($meshScript.Contains('unreal.Rotator(*rotation)')) {
	throw 'Mesh transforms must not pass C++-ordered rotations positionally.'
}

foreach ($token in @(
	'M_EvidenceSlipperTrail',
	'M_EvidenceCatPawTrail',
	'M_EvidenceHoseDrag',
	'M_EvidenceHandSmear',
	'M_DecalDampWallpaper',
	'M_DecalRustFasteners',
	'M_DecalMineralScale',
	'M_DecalRainGrime',
	'M_ApartmentWallPatina',
	'"tex": "ApartmentWallpaperV2"',
	'M_WetHoodieUV',
	'M_SubmergedHoodieUV',
	'M_SubmergedPantsUV',
	'M_SubmergedSlippersUV',
	'M_SubmergedSlipperWearUV',
	'M_AlleyCatTabbyUV',
	'M_WaterTankMetalUV',
	'M_TankInteriorBiofilmUV',
	'M_WetServiceHoseUV',
	'M_WetRungPadUV',
	'M_P3CabinetMetalUV',
	'M_TankWaterReveal',
	'M_CarrierBagFilm',
	'BLEND_MASKED',
	'MP_OPACITY_MASK',
	'BLEND_TRANSLUCENT',
	'MP_OPACITY',
	'MaterialExpressionPanner',
	'MaterialExpressionNormalize',
	'ripple_a_uv',
	'ripple_b_uv',
	'IG_TANK_WATER_ONLY',
	'IG_TANK_INTERIOR_ONLY',
	'IG_SUBMERGED_CLOTHING_ONLY',
	'MaterialExpressionMax',
	'minimum_wetness',
	'"wet_waterline_z": 561.0',
	'MaterialExpressionWorldPosition',
	'"M_TankInteriorBiofilmUV": DECAL_MATERIALS['
)) {
	if (-not $materialScript.Contains($token)) {
		throw "Masked material contract is missing: $token"
	}
}
if ($materialScript -notmatch
	'SAMPLERTYPE_MASKS\s*\r?\n\s*if mask_only else unreal\.MaterialSamplerType\.SAMPLERTYPE_COLOR') {
	throw 'Evidence mask textures must use the Masks material sampler type.'
}
foreach ($token in @(
	'SurfaceSpec("T_WetHoodie"',
	'SurfaceSpec("T_AlleyCatTabby"',
	'SurfaceSpec("T_WaterTankGalvanized"',
	'"T_TankInteriorBiofilm", 0.58',
	'SurfaceSpec("T_WetServiceHose"',
	'SurfaceSpec("T_WetRungPad"',
	'SurfaceSpec("T_P3CabinetPaintedSteel"',
	'SurfaceSpec("T_CarrierBagFilm"',
	'SurfaceSpec("T_TankWaterSurface"',
	'ImageOps.autocontrast',
	'normal_strength',
	'wetness',
	'metalness',
	'coated_metal'
)) {
	if (-not $pbrScript.Contains($token)) {
		throw "PBR source-map generation contract is missing: $token"
	}
}
foreach ($token in @(
	'def _connect_scan_pbr',
	'MaterialProperty.MP_NORMAL',
	'MaterialProperty.MP_ROUGHNESS',
	'MaterialProperty.MP_AMBIENT_OCCLUSION',
	'MaterialProperty.MP_METALLIC',
	'wet_normal_flatten',
	'SAMPLERTYPE_LINEAR_GRAYSCALE',
	'SAMPLERTYPE_MASKS'
)) {
	if (-not $materialScript.Contains($token)) {
		throw "PBR material blending contract is missing: $token"
	}
}
foreach ($token in @(
	'HERO_MESHES',
	'LARGE_PROP_PREFIXES',
	'def apply_lod_contract',
	'StaticMeshEditorSubsystem',
	'set_lod_group',
	'LOD0 preserved (story-critical close inspection)',
	'enable_nanite", False'
)) {
	if (-not $meshScript.Contains($token)) {
		throw "Static-mesh LOD contract is missing: $token"
	}
}
if ($directorSource.Contains('for (int32 RingIndex = 0; RingIndex < 3; ++RingIndex)')) {
	throw 'Opaque segmented ripple bars returned above the translucent tank water'
}
foreach ($token in @(
	'SM_HornRimGlasses',
	'SM_InspectionRod',
	'SM_CrackedPhone'
)) {
	if (-not $meshScript.Contains($token) -or
		-not $directorSource.Contains($token)) {
		throw "Evidence prop is not generated and loaded: $token"
	}
}
if (-not $meshScript.Contains('SM_OfferingWaterBowl') -or
	-not $prologueSource.Contains('SM_OfferingWaterBowl')) {
	throw 'Lobby offering bowl is not generated and loaded as an open vessel'
}
foreach ($token in @(
	'def build_offering_water_bowl',
	'(12.2, 8.0)',
	'(10.7, 7.7)',
	'revolve(mesh, profile, steps=48, smooth=True, scale_to_fill=True)',
	'return bake(mesh, "SM_OfferingWaterBowl", add_collision=False)',
	'FVector(-126, -280, 7.72f)',
	'WaterSurface->SetCastShadow(false)'
)) {
	if (-not $meshScript.Contains($token) -and
		-not $prologueSource.Contains($token)) {
		throw "Offering-bowl open-water contract is missing: $token"
	}
}
if (-not $meshScript.Contains('SM_AlleyCatRun') -or
	-not $neighborhoodSource.Contains('SM_AlleyCatRun') -or
	-not $neighborhoodSource.Contains('M_AlleyCatTabbyUV')) {
	throw 'Alley cat mesh/material is not generated and loaded'
}
foreach ($token in @(
	'SM_SubmergedHoodieCurl',
	'SM_SubmergedPantsCurl',
	'SM_SubmergedSlippersCurl'
)) {
	if (-not $meshScript.Contains($token) -or
		-not $directorSource.Contains($token)) {
		throw "Submerged body group is not generated and loaded: $token"
	}
}
foreach ($token in @(
	'M_SubmergedHoodieUV',
	'M_SubmergedPantsUV',
	'M_SubmergedSlippersUV',
	'M_SubmergedSlipperWearUV',
	'SubmergedPantsMaterial',
	'SubmergedSlippersMaterial',
	'SubmergedSlipperWearMaterial'
)) {
	if (-not $directorSource.Contains($token)) {
		throw "Submerged clothing material contract is missing: $token"
	}
}
if ($directorSource -match
	'BeddingMaterial,\s*\r?\n\s*SubmergedPantsMesh' -or
	$directorSource -match
	'PlasticMaterial,\s*\r?\n\s*SubmergedSlippersMesh') {
	throw 'Authored submerged clothing regressed to a generic proxy material'
}
foreach ($token in @('SM_TankAccessDeck', 'SM_TankAccessLid')) {
	if (-not $meshScript.Contains($token) -or
		-not $directorSource.Contains($token)) {
		throw "Tank access mesh is not generated and loaded: $token"
	}
}
foreach ($token in @(
	'SM_RooftopWaterTankShell',
	'SM_TankInternalLining',
	'SM_RooftopTankPipeCluster',
	'SM_TankInternalLadder',
	'SM_TankAccessGuardRail',
	'SM_TankExteriorAccessStair'
)) {
	if (-not $meshScript.Contains($token) -or
		-not $directorSource.Contains($token)) {
		throw "Rooftop water-tank asset is not generated and loaded: $token"
	}
}
foreach ($token in @(
	'SM_RooftopServiceHose',
	'SM_HoseCoupling',
	'SM_CarrierBagCollapsed'
)) {
	if (-not $meshScript.Contains($token) -or
		-not $directorSource.Contains($token)) {
		throw "P5 accident prop is not generated and loaded: $token"
	}
}
foreach ($token in @(
	'SM_RooftopFireDoorLeaf',
	'SM_RooftopFireDoorFrame'
)) {
	if (-not $meshScript.Contains($token) -or
		-not $directorSource.Contains($token)) {
		throw "Rooftop fire-door asset is not generated and loaded: $token"
	}
}
if (-not $meshScript.Contains('SM_RooftopUnlockedPadlockKeys') -or
	-not $directorSource.Contains('SM_RooftopUnlockedPadlockKeys')) {
	throw 'Rooftop unlocked padlock/key assembly is not generated and loaded'
}
if (($directorSource.Split(
	@('InspectionRodVisual = CreateBlock('),
	[System.StringSplitOptions]::None).Count - 1) -ne 1 -or
	($directorSource.Split(
	@('InspectionRodMesh ? InspectionRodMesh.Get() : CylinderMesh.Get()'),
	[System.StringSplitOptions]::None).Count - 1) -ne 1) {
	throw 'CH03 inspection rod must be one movable component without a duplicate prop'
}
foreach ($token in @(
	'SetInspectionRodWedged(true)',
	'SetInspectionRodWedged(false)',
	'never spawn a second explanatory prop'
)) {
	if (-not $directorSource.Contains($token)) {
		throw "Inspection-rod continuity contract is missing: $token"
	}
}
foreach ($token in @(
	'Content\Meshes\SM_RooftopFireDoorLeaf.uasset',
	'Content\Meshes\SM_RooftopFireDoorFrame.uasset',
	'Content\Meshes\SM_RooftopUnlockedPadlockKeys.uasset'
)) {
	if (-not $buildScript.Contains($token)) {
		throw "Rooftop door build output is not release-gated: $token"
	}
}
foreach ($token in @(
	'def build_rooftop_unlocked_padlock_keys',
	'for key_index, (key_y, blade_z, blade_length, blade_width, roll) in enumerate(key_specs)',
	'return bake(mesh, "SM_RooftopUnlockedPadlockKeys", add_collision=True)'
)) {
	if (-not $meshScript.Contains($token)) {
		throw "Rooftop padlock/key mesh contract is missing: $token"
	}
}
foreach ($token in @(
	'const bool bHasAuthoredPadlockKeys',
	'"열린 자물쇠와 꽂힌 열쇠 확인하기"',
	'"자물쇠가 열려 있다. 관리 열쇠도 그대로 꽂혀 있다."',
	'KeyIndex < 3'
)) {
	if (-not $directorSource.Contains($token)) {
		throw "Rooftop unlocked key interaction contract is missing: $token"
	}
}
foreach ($forbiddenToken in @(
	'RoofDoorUnlockedPrompt',
	'열쇠로 옥상 철문 열기'
)) {
	if ($directorSource.Contains($forbiddenToken)) {
		throw "Rooftop door regressed into an unintended lock puzzle: $forbiddenToken"
	}
}
foreach ($token in @(
	'RoofDoorLatchedAngleDegrees = 5.441396f',
	'const bool bHasAuthoredRoofDoor',
	'if (bHasAuthoredRoofDoor)',
	'FVector(4.5f, 116.0f, 230.0f)',
	'MeasureRoofDoorFreeEdgeGap()',
	'FVector(1645.5f, -340.5f, CatCenterZ)',
	'FVector(1655.5f, -339.0f, CatCenterZ)',
	'FCollisionShape::MakeCapsule(3.5f, 3.5f)',
	'M_SteelDoorUV'
)) {
	if (-not $directorSource.Contains($token)) {
		throw "Rooftop fire-door physical contract is missing: $token"
	}
}
foreach ($token in @(
	'def build_rooftop_fire_door_leaf',
	'(4.5, 116.0, 230.0)',
	'def build_rooftop_fire_door_frame',
	'(14.0, 136.0, 1.2)'
)) {
	if (-not $meshScript.Contains($token)) {
		throw "Rooftop fire-door mesh dimensional contract is missing: $token"
	}
}
foreach ($token in @(
	'SM_LadderFailureRung',
	'SM_LadderRungPadLifted',
	'SM_LadderRungRetainingClips'
)) {
	if (-not $meshScript.Contains($token) -or
		-not $directorSource.Contains($token)) {
		throw "P5 rung-failure asset is not generated and loaded: $token"
	}
}
foreach ($token in @(
	'def build_tank_exterior_access_stair',
	'for tread_index in range(18)',
	'if tread_index == 16',
	'(20.0, 105.0, 3.0)',
	'(355.0, platform_y, 455.0)',
	'return bake(mesh, "SM_TankExteriorAccessStair", add_collision=False)'
)) {
	if (-not $meshScript.Contains($token)) {
		throw "Exterior access-stair dimensional contract is missing: $token"
	}
}
if (-not $directorSource.Contains('M_WaterTankMetalUV')) {
	throw 'CH03 tank assembly does not load the shared galvanized material'
}
if (-not $directorSource.Contains('M_TankInteriorBiofilmUV')) {
	throw 'CH03 tank interior does not load its mineral and biofilm material'
}
if (-not $directorSource.Contains('M_WetServiceHoseUV')) {
	throw 'P5 hose does not load the shared wet-rubber material'
}
if (-not $directorSource.Contains('M_WetRungPadUV')) {
	throw 'P5 rung pad does not load the dedicated ribbed-rubber material'
}
foreach ($token in @(
	'M_TankWaterReveal',
	'TankRevealWaterMaterial',
	'TankInternalFloorZ = 381.0f',
	'TankDeckUndersideZ = 596.0f',
	'TankWaterSurfaceZ = 561.0f',
	'TankBodyPlacementAdjustmentZ = -16.0f',
	'AuthoredTankBodyPlacement(-90.0f, 0.0f, 542.0f)',
	'AuthoredTankBodyRotation(0.0f, 90.0f, 0.0f)',
	'AuthoredSleeveStitchLocalBase(23.0f, -25.0f, 13.0f)',
	'GetAuthoredSleeveStitchFocusOffset()',
	'TankDeckUndersideZ - TankInternalFloorZ == 215.0f',
	'TankWaterSurfaceZ - TankInternalFloorZ == 180.0f',
	'TankDeckUndersideZ - TankWaterSurfaceZ == 35.0f',
	'PlaneMesh ? FVector(270, 270, 1.0f) : FVector(270, 270, 3.0f)',
	'TankWaterSurface->SetCastShadow(false)',
	'TankWaterSurface->SetTranslucentSortPriority(2)',
	'Tank + FVector(-151.0f, 45.0f, 580.0f)',
	'Tank + FVector(-149, -28, 574)',
	'const FVector ClothingEvidenceOffset = bUsesAuthoredTankBody',
	'Tank + ClothingEvidenceOffset',
	'ClothingEvidence->GetPresentationMesh()->SetVisibility(false)'
)) {
	if (-not $directorSource.Contains($token)) {
		throw "Tank-water reveal contract is missing: $token"
	}
}
foreach ($token in @(
	'def build_rooftop_water_tank_shell',
	'(149.0, -130.0)',
	'(153.0, 130.0)',
	'for seam_index in range(16)',
	'for hoop_z in (-118.0, 0.0, 118.0)',
	'location=(0.0, 0.0, -91.0)',
	'def build_rooftop_tank_pipe_cluster',
	'4.45,',
	'3.8,',
	'return bake(mesh, "SM_RooftopTankPipeCluster", add_collision=False)'
)) {
	if (-not $meshScript.Contains($token)) {
		throw "Rooftop water-tank dimensional mesh contract is missing: $token"
	}
}
foreach ($token in @(
	'def build_tank_internal_lining',
	'(145.0, 382.0)',
	'(147.0, 596.0)',
	'location=(0.0, 0.0, 381.2)',
	'return bake(mesh, "SM_TankInternalLining", add_collision=False)',
	'TankInternalLiningMesh',
	'TankInteriorBiofilmMaterial',
	'InteriorLining->SetCastShadow(false)'
)) {
	if (-not $meshScript.Contains($token) -and -not $directorSource.Contains($token)) {
		throw "Tank-interior lining contract is missing: $token"
	}
}
foreach ($token in @(
	'def build_tank_internal_ladder',
	'for rung_index in range(7)',
	'rung_z = 400.0 + rung_index * 30.0',
	'[(-125.0, -23.0, rung_z), (-125.0, 23.0, rung_z)]',
	'for bracket_z in (405.0, 575.0)',
	'return bake(mesh, "SM_TankInternalLadder", add_collision=False)',
	'def build_tank_access_guard_rail',
	'for rail_y in (-105.0, 105.0)',
	'(-65.0, rail_y, 0.0)',
	'(0.0, 116.0, 56.0)',
	'return bake(mesh, "SM_TankAccessGuardRail", add_collision=False)'
)) {
	if (-not $meshScript.Contains($token)) {
		throw "Tank access-safety mesh contract is missing: $token"
	}
}
foreach ($token in @(
	'const bool bHasAuthoredTankShell',
	'CollisionPanel->SetVisibility(false, true)',
	'const bool bHasAuthoredTankPlumbing',
	'FVector(8.9f, 8.9f, 300)',
	'FVector(7.6f, 7.6f, 53)',
	'FVector(18, 18, 3)'
)) {
	if (-not $directorSource.Contains($token)) {
		throw "Rooftop water-tank runtime contract is missing: $token"
	}
}
foreach ($token in @(
	'if (TankInternalLadderMesh)',
	'InnerRungIndex < 7',
	'Tank + FVector(-125, 0, 400 + InnerRungIndex * 30.0f)',
	'if (TankAccessGuardRailMesh)',
	'RailCollision->SetVisibility(false, true)',
	'FVector(2335, -184, 662)',
	'"난간 U볼트의 젖은 안경 확인하기"',
	'FRotator(0.0f, -8.0f, 82.0f)'
)) {
	if (-not $directorSource.Contains($token)) {
		throw "Tank ladder/guardrail/glasses runtime contract is missing: $token"
	}
}
if ($directorSource.Contains('FVector(1880, -250, 247)') -or
	$directorSource.Contains('FVector(1880, -250, 250)')) {
	throw 'Glasses regressed from the upper U-bolt to the rooftop floor'
}
foreach ($token in @(
	'def create_tank_water_material',
	'MaterialProperty.MP_OPACITY',
	'MaterialProperty.MP_REFRACTION',
	'refraction_floor.set_editor_property("r", 1.006)'
)) {
	if (-not $materialScript.Contains($token)) {
		throw "Tank-water material contract is missing: $token"
	}
}
if (-not $directorSource.Contains('FVector(4.2f, 4.2f, 82.0f)') -or
	$directorSource.Contains('FVector(14, 14, 82)')) {
	throw 'P5 hose regressed from 42 mm service hose to the oversized greybox'
}
foreach ($token in @(
	'const bool bHasCarrierBagMesh = CarrierBagCollapsedMesh != nullptr;',
	'if (!bHasCarrierBagMesh)',
	'bHasCarrierBagMesh ? CarrierBagCollapsedMesh.Get() : nullptr'
)) {
	if (-not $directorSource.Contains($token)) {
		throw "Carrier-bag no-duplication contract is missing: $token"
	}
}
foreach ($token in @(
	'const bool bHasAuthoredExteriorStair',
	'TankExteriorAccessStairMesh && LadderFailureRungMesh',
	'StairCollision->SetVisibility(false, true)',
	'FVector(2235, -300, 582.0f)',
	'FVector(8.5f, 54.0f, 0.3f)',
	'FRotator(6.0f, 0.0f, 0.0f)'
)) {
	if (-not $directorSource.Contains($token)) {
		throw "Rung-failure physical contract is missing: $token"
	}
}
foreach ($token in @(
	'(5.0, 54.0, 0.30)',
	'(3.5, 54.0, 0.30)',
	'location=(-1.75, 0.0, 1.66)',
	'rotation=(15.0, 0.0, 0.0)'
)) {
	if (-not $meshScript.Contains($token)) {
		throw "Rung-pad long-edge lift contract is missing: $token"
	}
}
foreach ($token in @(
	'Content\Meshes\SM_TankExteriorAccessStair.uasset',
	'Content\Meshes\SM_TankInternalLining.uasset',
	'ART_BUILD PASS meshes=30'
)) {
	if (-not $buildScript.Contains($token)) {
		throw "Exterior access-stair output is not release-gated: $token"
	}
}
foreach ($token in @(
	'FVector(0, 0, 600)',
	'FVector(108, 108, 6)',
	'TankHatchOffset(-96.0f, 0.0f, 0.0f)',
	'TankLidOpenOffset(36.9f, 0.0f, 49.5f)',
	'Tank + IGThirdMorning::TankHatchOffset + FVector(0, 0, 610)',
	'FRotator(-78, 0, 0)',
	'FVector(2308.5f, -300, 610), FVector(87, 220, 20)'
)) {
	if (-not $directorSource.Contains($token)) {
		throw "Tank service-hatch physical contract is missing: $token"
	}
}
foreach ($token in @(
	'location=(-96.0, 0.0, -7.0)',
	'location=(-96.0, 0.0, 3.0)',
	'location=(49.0, hinge_y, 2.0)'
)) {
	if (-not $meshScript.Contains($token)) {
		throw "West-side service-hatch mesh contract is missing: $token"
	}
}
if ($directorSource.Contains('FVector(310, 310, 8)')) {
	throw 'The non-liftable three-metre tank lid returned'
}
foreach ($token in @('M_SubmergedHoodieUV', 'M_CarrierBagFilm')) {
	if (-not $directorSource.Contains($token)) {
		throw "CH03 material is not loaded: $token"
	}
}
if (-not $prologueSource.Contains('M_CarrierBagFilm')) {
	throw 'CH01/CH02 purchase bag does not share the carrier-film material'
}
if (-not $playerSource.Contains('M_WetHoodieUV')) {
	throw 'First-person sleeve does not share the wet-hoodie identity material'
}
foreach ($token in @(
	'SM_FirstPersonHoodieSleeve',
	'bHasAuthoredSleeveMesh',
	'OutfitSleeveProxy->SetRelativeScale3D(FVector::OneVector)',
	'FVector(1.0f, -5.75f, -7.0f + StitchIndex * 1.4f)',
	'FVector(0.012f, 0.003f, 0.0025f)'
)) {
	if (-not $playerSource.Contains($token)) {
		throw "First-person authored sleeve contract is missing: $token"
	}
}
foreach ($token in @(
	'def build_first_person_hoodie_sleeve',
	'(13.5, 12.4, 24.0)',
	'(11.8, 11.0, 18.0)',
	'5.25,',
	'SM_FirstPersonHoodieSleeve'
)) {
	if (-not $meshScript.Contains($token)) {
		throw "First-person sleeve mesh contract is missing: $token"
	}
}
foreach ($token in @(
	'SM_P3ServiceCabinetShell',
	'SM_P3ServiceManifold',
	'SM_P3ValveWheelLarge',
	'SM_P3ValveWheelSmall',
	'SM_P3PressureGauge'
)) {
	if (-not $meshScript.Contains($token) -or
		-not $directorSource.Contains($token)) {
		throw "P3 authored control cluster is not generated and loaded: $token"
	}
}
foreach ($token in @(
	'const bool bHasAuthoredP3Cluster',
	'FVector(18, 18, 4)',
	'FVector(12, 12, 3)',
	'FVector(16, 16, 4)',
	'FVector(812, -176.8f, 130)',
	'FVector(812, -176.8f, 122)',
	'FVector(0.026f, 0.026f, 0.40f * FlowScale)',
	'FRotator(FMath::Lerp(-55.0f, 55.0f, Alpha), 0, 0)',
	'Wheel->SetRelativeRotation(FRotator(0, 45, 0))'
)) {
	if (-not $directorSource.Contains($token)) {
		throw "P3 physical feedback contract is missing: $token"
	}
}
foreach ($token in @(
	'def build_p3_service_cabinet_shell',
	'(270.0, 4.0, 196.0)',
	'def build_p3_service_manifold',
	'_build_p3_valve_wheel("SM_P3ValveWheelLarge", 18.0',
	'_build_p3_valve_wheel("SM_P3ValveWheelSmall", 12.0',
	'build_p3_pressure_gauge'
)) {
	if (-not $meshScript.Contains($token)) {
		throw "P3 mesh dimensional contract is missing: $token"
	}
}
foreach ($token in @(
	'Prepare-AIArt.ps1',
	'generate_ai_pbr_maps.py',
	'[switch]$SourceOnly',
	'[switch]$CodeOnly',
	'[switch]$HudUiOnly',
	'[switch]$TankWaterOnly',
	'[switch]$TankInteriorOnly',
	'[switch]$SubmergedClothingOnly',
	'ART_TARGETED_BUILD PASS',
	'@($targetRelativeAssets).Count',
	'IG_HUD_UI_ONLY',
	'T_HudDialogueFilm_D.uasset',
	'IG_APARTMENT_VISUAL_ONLY',
	'T_ApartmentWallpaperV2_D.uasset',
	'T_ApartmentWallPatina_M.uasset',
	'IG_TANK_WATER_ONLY',
	'IG_TANK_INTERIOR_ONLY',
	'IG_SUBMERGED_CLOTHING_ONLY',
	'ART_SOURCE_BUILD PASS',
	'ART_CODE_BUILD PASS',
	'ART_BUILD compiling IndieGameEditor Win64 Development',
	'UnrealEditor-IndieGame.dll',
	"Join-Path `$unrealProjectRoot 'Binaries\Win64'",
	"Join-Path `$projectRoot 'Binaries\Win64'",
	'generate_meshes.py',
	'generate_surface_textures.py',
	'create_textured_materials.py',
	'apply_photo_prop_lods.py',
	'Content\Photo\Props',
	'validate_baked_art_assets.py',
	'Get-AsciiArtBuildRoot',
	'Invoke-ArtRobocopy',
	'ART_UASSET_AUDIT PASS',
	'-abslog=',
	'ART_BUILD PASS'
)) {
	if (-not $buildScript.Contains($token)) {
		throw "Art build pipeline stage is missing: $token"
	}
}
foreach ($token in @(
	'StaticMeshEditorSubsystem',
	'get_lod_count',
	'compression_settings',
	'MP_NORMAL',
	'MP_ROUGHNESS',
	'MP_AMBIENT_OCCLUSION',
	'MP_METALLIC',
	'MP_OPACITY',
	'MP_OPACITY_MASK',
	'get_material_expressions',
	'validate_photo_prop_lods',
	'photo_meshes=',
	'recompile_material',
	'ART_UASSET_AUDIT PASS'
)) {
	if (-not $auditScript.Contains($token)) {
		throw "Baked UAsset audit contract is missing: $token"
	}
}
foreach ($token in @(
	'PHOTO_PROP_ROOT = "/Game/Photo/Props"',
	'LARGE_PROP_IDS',
	'def inspect_photo_prop_lods',
	'def apply_photo_prop_lod_contract',
	'get_number_verts',
	'set_lod_group',
	'lod_count < 2',
	'50_000'
)) {
	if (-not $photoLodContract.Contains($token)) {
		throw "Photo-prop LOD contract is missing: $token"
	}
}
if ($photoLodContract.Contains('set_nanite_settings')) {
	throw 'Photo-prop LOD automation must not enable Nanite without an A/B GPU trace.'
}
foreach ($token in @(
	'PHOTO_PROP_LOD_BUILD PASS',
	'apply_photo_prop_lod_contract'
)) {
	if (-not $photoLodApplyScript.Contains($token) -or
		-not $photoImportScript.Contains('apply_photo_prop_lod_contract')) {
		throw "Photo-prop import/apply pipeline is missing: $token"
	}
}

Write-Host 'ART_ASSET_CONTRACT PASS raw=28 masks=5 overlays=4 material_scans=11 pbr_maps=38 meshes=31 photo_meshes=50'
