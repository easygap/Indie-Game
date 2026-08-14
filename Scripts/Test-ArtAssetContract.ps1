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
	'AI\SheetFirstPersonKnockPhases_v1.png',
	'AI\SheetFirstPersonKnockPhases_v1_RGBA.png',
	'AI\SheetFirstPersonKnockPhases_v2.png',
	'AI\SheetFirstPersonKnockPhases_v2_RGBA.png',
	'AI\SheetListenerCaptureEmbracePhases_v1.png',
	'AI\SheetListenerCaptureEmbracePhases_v1_RGBA.png',
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
	'AI\MaskApartmentWallPatina.png',
	'AI\TextureStickyNotePaper_D.png',
	'AI\TextureStickyNote404Doodle_D.png',
	'AI\TextureCaptureMercyNotePaper_D.png',
	'AI\SheetMissingFloorEnvironmentReference.png',
	'AI\SheetListenerEntityAnatomyReference.png',
	'AI\TextureMissingFloorDryPlaster.png',
	'AI\SheetMissingFloorResidueMasks.png',
	'AI\SheetMissingFloorDistantCharacters.png',
	'AI\SheetMissingFloorHeroPropsReference.png',
	'AI\ListenerEntityFrontCutout.png',
	'AI\SheetListenerEntityCrawlPhases.png',
	'AI\SheetFinalCavityRemainsReference_v1.png',
	'AI\SheetMokHansooConfrontationReference_v1.png',
	'AI\FinalCavityFrontBlend_v1.png',
	'AI\MokHansooFinalFrontBlend_v1.png',
	'AI\TextureMissingFloorJournalPaper_v1.png'
)
$requiredMasks = @(
	'T_EvidenceSlipperTrail_M.png',
	'T_EvidenceCatPawTrail_M.png',
	'T_EvidenceHoseDrag_M.png',
	'T_EvidenceHandSmear_M.png',
	'T_MissingFloorHandprints_M.png',
	'T_MissingFloorDragTrails_M.png',
	'T_MissingFloorDustJoint_M.png',
	'T_MissingFloorCavityScratches_M.png'
)
$requiredOverlays = @(
	'T_DecalDampWallpaper_D.png',
	'T_DecalRustFasteners_D.png',
	'T_DecalMineralScale_D.png',
	'T_DecalRainGrime_D.png',
	'T_SpriteSeo_D.png',
	'T_SpriteMok_D.png',
	'T_SpriteHwang_D.png',
	'T_SpriteNarin_D.png',
	'T_SpriteListenerFront_D.png',
	'T_SpriteListenerCrawl0_D.png',
	'T_SpriteListenerCrawl1_D.png',
	'T_SpriteListenerCrawl2_D.png',
	'T_SpriteListenerCrawl3_D.png',
	'T_SpriteFinalCavity_D.png',
	'T_SpriteMokFinalUpper_D.png',
	'T_FPHandKnock0_D.png',
	'T_FPHandKnock1_D.png',
	'T_FPHandKnock2_D.png',
	'T_FPHandKnock3_D.png',
	'T_FPCaptureEmbrace0_D.png',
	'T_FPCaptureEmbrace1_D.png',
	'T_FPCaptureEmbrace2_D.png',
	'T_FPCaptureEmbrace3_D.png'
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
	'T_AudioCalibrationWall_D.png',
	'T_HudDialogueFilm_D.png',
	'T_MissingFloorJournalPaper_D.png',
	'T_ApartmentWallpaperV2_D.png',
	'T_MissingFloorDryPlaster_D.png'
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
	'T_ApartmentWallpaperV2_A.png',
	'T_MissingFloorDryPlaster_N.png',
	'T_MissingFloorDryPlaster_R.png',
	'T_MissingFloorDryPlaster_A.png',
	'T_SpriteListenerFront_N.png',
	'T_SpriteListenerFront_R.png',
	'T_SpriteListenerFront_A.png',
	'T_SpriteListenerCrawl0_N.png',
	'T_SpriteListenerCrawl0_R.png',
	'T_SpriteListenerCrawl0_A.png',
	'T_SpriteListenerCrawl1_N.png',
	'T_SpriteListenerCrawl1_R.png',
	'T_SpriteListenerCrawl1_A.png',
	'T_SpriteListenerCrawl2_N.png',
	'T_SpriteListenerCrawl2_R.png',
	'T_SpriteListenerCrawl2_A.png',
	'T_SpriteListenerCrawl3_N.png',
	'T_SpriteListenerCrawl3_R.png',
	'T_SpriteListenerCrawl3_A.png',
	'T_SpriteFinalCavity_N.png',
	'T_SpriteFinalCavity_R.png',
	'T_SpriteFinalCavity_A.png',
	'T_SpriteMokFinalUpper_N.png',
	'T_SpriteMokFinalUpper_R.png',
	'T_SpriteMokFinalUpper_A.png'
)
$requiredDerived = @(
	$requiredMasks + $requiredOverlays + $requiredMaterialMasks +
	$requiredMaterialTextures + $requiredPbrMaps
)
$requiredEasterEggSigns = @{
	'T_CaptureMercyNote_D.png' = @(1024, 640)
	'T_Note404NotFound_D.png' = @(512, 512)
	'T_Plate401_D.png' = @(128, 64)
	'T_Plate402_D.png' = @(128, 64)
	'T_Plate403_D.png' = @(128, 64)
	'T_PlateCommon_D.png' = @(128, 64)
}
foreach ($relativePath in @($requiredRaw + $requiredDerived)) {
	$path = Join-Path $sourceArt $relativePath
	if (-not (Test-Path -LiteralPath $path -PathType Leaf)) {
		throw "Art source is missing: $relativePath"
	}
	if ((Get-Item -LiteralPath $path).Length -le 1024) {
		throw "Art source is unexpectedly small: $relativePath"
	}
}

foreach ($entry in $requiredEasterEggSigns.GetEnumerator()) {
	$relativePath = [string]$entry.Key
	$path = Join-Path $sourceArt $relativePath
	if (-not (Test-Path -LiteralPath $path -PathType Leaf)) {
		throw "403/404 entrance sign source is missing: $relativePath"
	}
	$image = [System.Drawing.Bitmap]::FromFile($path)
	try {
		$expectedWidth = [int]$entry.Value[0]
		$expectedHeight = [int]$entry.Value[1]
		if ($image.Width -ne $expectedWidth -or $image.Height -ne $expectedHeight) {
			throw "403/404 entrance sign must be ${expectedWidth}x${expectedHeight}: $relativePath"
		}
		$darkSamples = 0
		$lightSamples = 0
		$sampleStep = [Math]::Max(1, [int]($image.Width / 64))
		for ($y = 0; $y -lt $image.Height; $y += $sampleStep) {
			for ($x = 0; $x -lt $image.Width; $x += $sampleStep) {
				$pixel = $image.GetPixel($x, $y)
				$luma = $pixel.R * 0.2126 + $pixel.G * 0.7152 + $pixel.B * 0.0722
				if ($luma -lt 96) { $darkSamples++ }
				if ($luma -gt 180) { $lightSamples++ }
			}
		}
		if ($darkSamples -lt 4 -or $lightSamples -lt 4) {
			throw "403/404 entrance sign lost its readable ink contrast: $relativePath"
		}
	}
	finally {
		$image.Dispose()
	}
}

foreach ($relativePath in $requiredDerived) {
	$path = Join-Path $sourceArt $relativePath
	$image = [System.Drawing.Bitmap]::FromFile($path)
	try {
		$expectedSize = if ($relativePath -like 'T_FPHandKnock*_D.png') {
			768
		}
		elseif (
			$requiredMaterialMasks -contains $relativePath -or
			$requiredMaterialTextures -contains $relativePath -or
			$requiredPbrMaps -contains $relativePath -or
			$relativePath -like 'T_SpriteListener*_D.png' -or
			$relativePath -like 'T_FPCaptureEmbrace*_D.png'
		) { 1024 } else { 512 }
		$expectedWidth = if ($relativePath -eq 'T_MissingFloorJournalPaper_D.png') {
			1672
		} elseif ($relativePath -like 'T_SpriteFinalCavity_*' -or
			$relativePath -like 'T_SpriteMokFinalUpper_*') {
			1024
		} else { $expectedSize }
		$expectedHeight = if ($relativePath -eq 'T_MissingFloorJournalPaper_D.png') {
			941
		} elseif ($relativePath -like 'T_SpriteFinalCavity_*' -or
			$relativePath -like 'T_SpriteMokFinalUpper_*') {
			1536
		} else { $expectedSize }
		if ($image.Width -ne $expectedWidth -or $image.Height -ne $expectedHeight) {
			throw "Derived art must be ${expectedWidth}x${expectedHeight}: $relativePath"
		}

		$opaqueSamples = 0
		$transparentSamples = 0
		$visibleMagentaSamples = 0
		$visibleGreenSamples = 0
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
				if ($pixel.A -gt 8 -and
					$pixel.G - $pixel.R -gt 24 -and
					$pixel.G - $pixel.B -gt 24) {
					$visibleGreenSamples++
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
			if (($relativePath -like 'T_SpriteListener*_D.png' -or
				$relativePath -like 'T_SpriteFinalCavity_D.png' -or
				$relativePath -like 'T_SpriteMokFinalUpper_D.png' -or
				$relativePath -like 'T_FPHandKnock*_D.png' -or
				$relativePath -like 'T_FPCaptureEmbrace*_D.png') -and
				$visibleGreenSamples -gt 0) {
				throw "RGBA overlay retained a visible green fringe: $relativePath"
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
				'T_AudioCalibrationWall_D.png' {
					if ($meanLuma -lt 18 -or $meanLuma -gt 75 -or
						$dynamicRange -lt 12 -or $dynamicRange -gt 110) {
						throw "Calibration wall lost its useful near-black detail range: $relativePath"
					}
				}
				'T_MissingFloorJournalPaper_D.png' {
					if ($meanLuma -lt 185 -or $meanLuma -gt 235 -or
						$dynamicRange -lt 30) {
						throw "Journal paper lost its readable tactile range: $relativePath"
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
$demoSource = Get-Content -Raw -Encoding UTF8 -LiteralPath (
	Join-Path $projectRoot 'Source\IndieGame\Sequence\IGDemoDirector.cpp')
$neighborhoodSource = Get-Content -Raw -Encoding UTF8 -LiteralPath (
	Join-Path $projectRoot 'Source\IndieGame\Environment\IGNeighborhoodLifeDirector.cpp')
$prologueSource = Get-Content -Raw -Encoding UTF8 -LiteralPath (
	Join-Path $projectRoot 'Source\IndieGame\Core\IGPrologueWorldScene.cpp')
$listenerSource = Get-Content -Raw -Encoding UTF8 -LiteralPath (
	Join-Path $projectRoot 'Source\IndieGame\Entity\IGListenerEntity.cpp')
$greyboxSource = Get-Content -Raw -Encoding UTF8 -LiteralPath (
	Join-Path $projectRoot 'Source\IndieGame\Entity\IGListenerGreyboxDirector.cpp')
$nightThreeSource = Get-Content -Raw -Encoding UTF8 -LiteralPath (
	Join-Path $projectRoot 'Source\IndieGame\Entity\IGMissingFloorNightThreeDirector.cpp')
$fifthDawnSource = Get-Content -Raw -Encoding UTF8 -LiteralPath (
	Join-Path $projectRoot 'Source\IndieGame\Entity\IGMissingFloorFifthDawnDirector.cpp')
$nightFourSource = Get-Content -Raw -Encoding UTF8 -LiteralPath (
	Join-Path $projectRoot 'Source\IndieGame\Entity\IGMissingFloorNightFourDirector.cpp')
$missingFloorNarrativeSource = Get-Content -Raw -Encoding UTF8 -LiteralPath (
	Join-Path $projectRoot 'Source\IndieGame\Narrative\IGMissingFloorNarrativeSubsystem.cpp')
$missingFloorNarrativeHeader = Get-Content -Raw -Encoding UTF8 -LiteralPath (
	Join-Path $projectRoot 'Source\IndieGame\Narrative\IGMissingFloorNarrativeSubsystem.h')
$missingFloorStory = Get-Content -Raw -Encoding UTF8 -LiteralPath (
	Join-Path $projectRoot 'Docs\STORY_BIBLE_MISSING_FLOOR.md')
$puzzleTwoSource = Get-Content -Raw -Encoding UTF8 -LiteralPath (
	Join-Path $projectRoot 'Source\IndieGame\Entity\IGMissingFloorPuzzleTwoDirector.cpp')
$playerSource = Get-Content -Raw -Encoding UTF8 -LiteralPath (
	Join-Path $projectRoot 'Source\IndieGame\Player\IGPlayerCharacter.cpp')
$playerControllerSource = Get-Content -Raw -Encoding UTF8 -LiteralPath (
	Join-Path $projectRoot 'Source\IndieGame\Player\IGPlayerController.cpp')
$horrorHudSource = Get-Content -Raw -Encoding UTF8 -LiteralPath (
	Join-Path $projectRoot 'Source\IndieGame\Player\IGHorrorHUD.cpp')
$toneSequenceSource = Get-Content -Raw -Encoding UTF8 -LiteralPath (
	Join-Path $projectRoot 'Source\IndieGame\Audio\IGToneSequenceSoundWave.cpp')
$inputConfig = Get-Content -Raw -Encoding UTF8 -LiteralPath (
	Join-Path $projectRoot 'Config\DefaultInput.ini')
$buildScript = Get-Content -Raw -Encoding UTF8 -LiteralPath (
	Join-Path $projectRoot 'Scripts\Build-ArtAssets.ps1')
$signScript = Get-Content -Raw -Encoding UTF8 -LiteralPath (
	Join-Path $projectRoot 'Scripts\Create-SignTextures.ps1')
$surfaceScript = Get-Content -Raw -Encoding UTF8 -LiteralPath (
	Join-Path $projectRoot 'Scripts\generate_surface_textures.py')
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
	'def append_indexed_surface',
	'def append_wrapped_label_surface',
	'getattr(unreal, "GeometryScript_MeshEdits", None)',
	'EDITS.append_buffers_to_mesh(mesh, buffers, 0, False)',
	'uvs.append((u, v))',
	'build_sticky_note_76mm',
	'"SM_StickyNote76mm"',
	'build_capture_mercy_note',
	'"SM_CaptureMercyNote"',
	'width = 18.0',
	'depth = 11.0',
	'[(4.18, 0.0, 1.0), (5.36, 7.2, 0.0)]'
)) {
	if (-not $meshScript.Contains($token)) {
		throw "Physical packaging UV/adhesive-note mesh contract is missing: $token"
	}
}
foreach ($token in @(
	'"M_Note404NotFound": {',
	'"tex_asset": "T_Note404NotFound_D", "rough": 0.88, "two_sided": True,',
	'"M_CaptureMercyNote": {',
	'"tex_asset": "T_CaptureMercyNote_D", "rough": 0.92, "two_sided": True,',
	'"M_Plate402":      {"tex_asset": "T_Plate402_D", "rough": 0.35}',
	'"M_PlateCommon":   {"tex_asset": "T_PlateCommon_D", "rough": 0.35}',
	'material.set_editor_property("two_sided", bool(spec.get("two_sided", False)))',
	'IG_CORRIDOR_SIGNAGE_ONLY',
	'Corridor entrance signage material update complete'
)) {
	if (-not $materialScript.Contains($token)) {
		throw "403/404 entrance material contract is missing: $token"
	}
}
foreach ($token in @(
	'[switch]$CorridorEntranceOnly',
	"AI\TextureStickyNote404Doodle_D.png",
	"AI\TextureCaptureMercyNotePaper_D.png",
	"-BackgroundImagePath `$notFoundPaper",
	"-BackgroundImagePath `$captureMercyNotePaper",
	"Draw-CenteredText `$g '404'",
	"Draw-CenteredText `$g 'Not'",
	"Draw-CenteredText `$g 'Found'",
	"Draw-CenteredText `$g '소리를 줄여라.'",
	"Draw-CenteredText `$g '걔는 눈이 없어.'"
)) {
	if (-not $signScript.Contains($token)) {
		throw "403/404 generated-paper composition contract is missing: $token"
	}
}
foreach ($token in @(
	'CORRIDOR_SIGNAGE_ONLY = os.environ.get("IG_CORRIDOR_SIGNAGE_ONLY") == "1"',
	'CORRIDOR_SIGNAGE_TEXTURE_NAMES',
	'"T_CaptureMercyNote_D"',
	'"T_Note404NotFound_D"',
	'"T_PlateCommon_D"',
	'"T_NoteFridge_D",',
	'"T_CaptureMercyNote_D",',
	'asset_name.startswith("T_Plate")'
)) {
	if (-not $surfaceScript.Contains($token)) {
		throw "403/404 targeted texture-import contract is missing: $token"
	}
}
foreach ($token in @(
	'INSTANCED_PRODUCT_MATERIALS',
	'WRAPPED_LABEL_MATERIALS',
	'used_with_instanced_static_meshes'
)) {
	if (-not $materialScript.Contains($token) -or -not $auditScript.Contains($token)) {
		throw "Runtime product-material contract is missing: $token"
	}
}
if (-not $materialScript.Contains('material.set_editor_property("two_sided", True)') -or
	-not $auditScript.Contains('material.get_editor_property("two_sided")')) {
	throw 'Wrapped product film must be authored and audited as two-sided.'
}
foreach ($token in @(
	'"M_Note404NotFound": "T_Note404NotFound_D"',
	'"M_CaptureMercyNote": "T_CaptureMercyNote_D"',
	'ENTRANCE_PLATE_MATERIALS',
	'TWO_SIDED_PRINT_MATERIALS = {',
	'"M_Note404NotFound",',
	'"M_CaptureMercyNote",',
	'"M_MercyNoteUnderDoor",',
	'Printed paper lost two-sided rendering'
)) {
	if (-not $auditScript.Contains($token)) {
		throw "403/404 baked-material audit contract is missing: $token"
	}
}
if ($meshScript.Contains('set_mesh_u_vs_from_cylinder_projection') -or
	$meshScript.Contains('set_mesh_uvs_from_cylinder_projection')) {
	throw 'Printed sleeves must use an explicit single-seam UV instead of projection.'
}
foreach ($token in @(
	'constexpr float BedsideTableTopZ = 60.0f;',
	'constexpr float AlarmContactBottomLocalZ = -0.40f;',
	'constexpr float PropContactEmbedZ = 0.10f;',
	'BedsideTable->CalcBounds(',
	'BedsideSurfaceWorldZ - IGPrologueWorld::AlarmContactBottomLocalZ',
	'AlarmWorldLocation',
	'PropMesh(TEXT("SM_StickyNote76mm"))',
	'FVector(-7.43f, 36.0f, 18.0f)',
	'FVector(3.30f, 3.30f, 5.5f)',
	'BuildCabInterior owns the sole rider COP',
	'Prop->bDisallowNanite = true;'
)) {
	if (-not $prologueSource.Contains($token)) {
		throw "Household/retail physical placement contract is missing: $token"
	}
}
foreach ($token in @(
	'const FName NotFoundEasterEggTag(TEXT("EasterEgg.404NotFound"));',
	'TEXT("M_Plate402"), TEXT("M_Plate401")',
	'TexMat(TEXT("M_Plate403"), FridgeInteriorMaterial)',
	'TexMat(TEXT("M_Note404NotFound"), SignWhiteMaterial)',
	'FVector(216.0f, -235.12f, 171.0f)',
	'NotFoundNote->SetCullDistance(520.0f);',
	'TEXT("M_Plate403"), TEXT("M_PlateCommon")'
)) {
	if (-not $prologueSource.Contains($token)) {
		throw "403/404 entrance runtime contract is missing: $token"
	}
}
foreach ($token in @(
	'NotFoundNoteSpot(216, -235, FloorZ + 171)',
	'MakeWalkLook(FVector(310, -350, 0), NotFoundNoteSpot)',
	'MakeStill(TEXT("prologue-not-found-note"))',
	'BaseName == TEXT("prologue-not-found-note")'
)) {
	if (-not $demoSource.Contains($token)) {
		throw "403/404 entrance capture contract is missing: $token"
	}
}
foreach ($token in @(
	'const bool bRamyeonBay =',
	'constexpr float RamyeonShelfSurfaceZ = 121.5f;',
	'FVector(CupX, RowY, RamyeonShelfSurfaceZ)',
	'const float TierHeights[] = {30.0f, 60.0f, 90.0f, 120.0f, 150.0f};',
	'FVector(2640, GondolaY, 174)',
	'for (const float TierZ : {16.0f, 46.0f, 76.0f, 106.0f, 136.0f, 166.0f})'
)) {
	if (-not $prologueSource.Contains($token)) {
		throw "Convenience-store shelf-bay placement contract is missing: $token"
	}
}
foreach ($forbiddenToken in @(
	'AddStoreStockCup(FVector(CupX, -365, 142)',
	'AddStoreStockCup(FVector(CupX, -654, 167.5f)',
	'constexpr float AlarmFootBottomLocalZ = -0.65f;',
	'PropContactClearanceZ'
)) {
	if ($prologueSource.Contains($forbiddenToken)) {
		throw "A known floating/top-cap retail placement regressed: $forbiddenToken"
	}
}
foreach ($forbiddenToken in @(
	'FVector(-6.9f, 36.0f, 18.0f)',
	'FVector(0.012f, 0.20f, 0.20f)',
	'FVector(12, -74.2f, CabBaseZ + 108)',
	'FVector(4.10f, 4.10f, 7.2f)',
	'FVector(3.36f, 3.36f, 8.6f)'
)) {
	if ($prologueSource.Contains($forbiddenToken)) {
		throw "A known floating/intersecting/stretched visual regressed: $forbiddenToken"
	}
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
	'Anatomically readable wet hoodie',
	'leg_segments = (',
	'Two grounded slide slippers',
	'location=(50.0, -22.0, 7.0)',
	'((45.0, 6.0, -8.0), -8.0)'
)) {
	if (-not $meshScript.Contains($token)) {
		throw "Submerged human-anatomy silhouette contract is missing: $token"
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
	'TankBodyPlacementAdjustmentZ = -9.0f',
	'AuthoredTankBodyPlacement(-88.0f, 0.0f, 542.0f)',
	'AuthoredTankBodyRotation(0.0f, 70.0f, 0.0f)',
	'AuthoredSleeveStitchLocalBase(8.0f, -27.0f, 15.2f)',
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
	'ART_BUILD PASS meshes=39'
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
	'[switch]$CorridorSignageOnly',
	'-CorridorEntranceOnly',
	'[switch]$TankWaterOnly',
	'[switch]$TankInteriorOnly',
	'[switch]$SubmergedClothingOnly',
	'ART_TARGETED_BUILD PASS',
	'@($targetRelativeAssets).Count',
	'IG_HUD_UI_ONLY',
	'IG_CORRIDOR_SIGNAGE_ONLY',
	'SM_CaptureMercyNote.uasset',
	'T_CaptureMercyNote_D.uasset',
	'M_CaptureMercyNote.uasset',
	'T_Note404NotFound_D.uasset',
	'M_Note404NotFound.uasset',
	'T_HudDialogueFilm_D.uasset',
	'T_AudioCalibrationWall_D.uasset',
	'T_MissingFloorJournalPaper_D.uasset',
	'T_FPHandKnock0_D.uasset',
	'T_FPHandKnock3_D.uasset',
	'T_FPCaptureEmbrace0_D.uasset',
	'T_FPCaptureEmbrace3_D.uasset',
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

# 「없는 층」의 ImageGen 원본은 참고 시트에서 끝나지 않는다. 근접 인체는
# 연속 3D 접지 셸과 정면 PBR 레이어를 결합하고, 흔적은 값 마스크, 접근
# 불가 인물은 고정 스프라이트로 제한하는 적용 경계를 소스 계약으로 잠근다.
foreach ($token in @(
	'T_MissingFloorDryPlaster',
	'M_MissingFloorListenerPlasterUV',
	'M_MissingFloorHandprints',
	'M_SpriteSeo',
	'M_SpriteListenerFront',
	'M_SpriteListenerCrawl0',
	'M_SpriteListenerCrawl3',
	'M_SpriteFinalCavity',
	'M_SpriteMokFinalUpper'
)) {
	if (-not $materialScript.Contains($token)) {
		throw "Missing-floor material pipeline is missing: $token"
	}
}
foreach ($token in @(
	'Every targeted pass can compile runtime bindings',
	"-Source (Join-Path `$unrealProjectRoot 'Binaries\Win64')",
	"-Destination (Join-Path `$projectRoot 'Binaries\Win64')"
)) {
	if (-not $buildScript.Contains($token)) {
		throw "ASCII targeted-build binary sync is missing: $token"
	}
}
foreach ($token in @(
	'build_listener_entity_crawl',
	'"SM_ListenerEntityCrawl"',
	'build_final_cavity_clothing_shell',
	'"SM_FinalCavityClothingShell"',
	'build_final_cavity_bone_insert',
	'"SM_FinalCavityBoneInsert"',
	'build_final_cavity_tarp',
	'"SM_FinalCavityTarp"',
	'build_final_cavity_broken_caster',
	'"SM_FinalCavityBrokenCaster"',
	'build_mok_hansoo_workwear',
	'"SM_MokHansooWorkwear"',
	'build_mok_hansoo_head_hands',
	'"SM_MokHansooHeadHands"',
	'build_mok_hansoo_gypsum_board',
	'"SM_MokHansooGypsumBoard"',
	'Long tuner fingers remain closed plaster geometry',
	'build_tuning_hammer',
	'"SM_TuningHammer"',
	'A tuning hammer is not a listening wand',
	'build_tuner_tool_cart',
	'"SM_TunerToolCart"',
	'build_complaint_ledger',
	'"SM_ComplaintLedger"',
	'build_calendar_journal',
	'"SM_CalendarJournal"',
	'IG_MISSING_FLOOR_ONLY'
)) {
	if (-not $meshScript.Contains($token)) {
		throw "Missing-floor anatomical mesh contract is missing: $token"
	}
}
foreach ($token in @(
	'SM_ListenerEntityCrawl.SM_ListenerEntityCrawl',
	'M_MissingFloorListenerPlasterUV',
	'M_SpriteListenerFront',
	'M_SpriteListenerCrawl0',
	'M_SpriteListenerCrawl3',
	'UpdatePresentationPose(LastMoveSpeed, DeltaSeconds)',
	'ListenerPhaseMaterials.Num() == 4',
	'State == EIGListenerState::Waiting',
	'ListenerPhase = 1.0f',
	'const float FramesPerSecond = FMath::Lerp(1.6f, 6.0f, SpeedAlpha)',
	'SetCastHiddenShadow(bFrontCardActive)',
	'Distance > 160.0f',
	'Distance > 125.0f',
	'Facing > 0.60f',
	'FVector(0.0f, 0.0f, -27.0f)'
)) {
	if (-not $listenerSource.Contains($token)) {
		throw "Listener release-visual binding is missing: $token"
	}
}
foreach ($token in @(
	'M_MissingFloorPlaster_X',
	'M_MissingFloorHandprints',
	'M_MissingFloorCavityScratches'
)) {
	if (-not $prologueSource.Contains($token)) {
		throw "Fifth-floor PBR/residue placement is missing: $token"
	}
}
foreach ($token in @(
	'M_SpriteSeo.M_SpriteSeo',
	'Feet sit exactly on Z=0',
	'RefreshDistantSeoVisibility',
	'SM_TuningHammer.SM_TuningHammer',
	'조율 렌치',
	'SM_TunerToolCart.SM_TunerToolCart',
	'SM_CalendarJournal.SM_CalendarJournal'
)) {
	if (-not $nightThreeSource.Contains($token)) {
		throw "Missing-floor night-three visual binding is missing: $token"
	}
}
foreach ($token in @(
	'SM_ComplaintLedger.SM_ComplaintLedger',
	'ComplaintLedgerMesh ? LedgerMaterial',
	'77.5f'
)) {
	if (-not $puzzleTwoSource.Contains($token)) {
		throw "Missing-floor physical ledger binding is missing: $token"
	}
}

# 정사 v2.5의 공간·퍼즐·영속성 계약. 시각 에셋이 맞아도 포털, 조기 P3
# 해결, 가상 P5 설비 또는 복원 누락이 돌아오면 같은 빌드로 취급하지 않는다.
foreach ($token in @(
	'MissingFloorRouteLengthCentimeters = 640.0f',
	'MissingFloorUpperStepCount = 14',
	'ValidateMissingFloorRooftopRoute',
	'OpenMissingFloorCavity',
	'SetMissingFloorAnnexPower'
)) {
	if (-not $prologueSource.Contains($token)) {
		throw "Missing-floor physical topology contract is missing: $token"
	}
}
if ($nightThreeSource.Contains('MissingFloorAnnexTransition') -or
	$nightThreeSource.Contains('AnnexTransition')) {
	throw 'Night three regressed to a detached annex portal.'
}
$valveStart = $nightThreeSource.IndexOf(
	'void AIGMissingFloorNightThreeDirector::HandleValveOpened')
$listenStart = $nightThreeSource.IndexOf(
	'void AIGMissingFloorNightThreeDirector::HandleWallListened')
if ($valveStart -lt 0 -or $listenStart -le $valveStart) {
	throw 'Night-three P3 function boundaries are missing.'
}
$valveBody = $nightThreeSource.Substring($valveStart, $listenStart - $valveStart)
if ($valveBody.Contains('MarkPuzzleSolved')) {
	throw 'P3 must not be solved by opening the valve before identifying a wall.'
}
foreach ($token in @(
	'P5.RoofCleaningDrain',
	'P5.RoofFloatBypass',
	'P5.TransferPump',
	'WaterMaskHumHandle',
	'RecordNightFourWallStrike',
	'SetMissingFloorAnnexPower(false)',
	'OpenMissingFloorCavity',
	'Ending.A',
	'Ending.B',
	'Ending.C'
)) {
	if (-not $nightFourSource.Contains($token)) {
		throw "Night-four runtime contract is missing: $token"
	}
}
foreach ($token in @(
	'AnswerRhythmJournal',
	'NightFourControlOrder',
	'NightFourWallStrikeCount',
	'bFirstReportMade',
	'bSecondReportMade',
	'SelectEnding'
)) {
	if (-not $missingFloorNarrativeSource.Contains($token) -and
		-not $missingFloorNarrativeHeader.Contains($token)) {
		throw "Missing-floor v2 persistence contract is missing: $token"
	}
}
if (-not $missingFloorNarrativeHeader.Contains('SnapshotSchemaVersion = 2')) {
	throw 'Missing-floor snapshot schema was not advanced for night-four state.'
}
foreach ($token in @(
	'제작 정사 v3.2',
	'세척 배수 OPEN',
	'부자밸브 우회 OPEN',
	'저수조 이송펌프',
	'2분 40초',
	'05:30 최초 신고',
	'0:52~1:20 / 7월 29일 셋째 새벽',
	'1:55~2:40 / 7월 31일 다섯째이자 마지막 새벽'
)) {
	if (-not $missingFloorStory.Contains($token)) {
		throw "Missing-floor story v3.2 contract is missing: $token"
	}
}
foreach ($token in @(
	'EIGListenerState::FinaleLured',
	'BeginFinalePass(',
	'SetActorEnableCollision(false)',
	'FinaleRoutePoints[FinaleRouteIndex]',
	'State == EIGListenerState::FinaleLured'
)) {
	if (-not $listenerSource.Contains($token)) {
		throw "Night-four harmless entity-pass contract is missing: $token"
	}
}
# v2.4 입력 실행 계약. 설정에 키 이름만 있거나 코드에 함수 이름만 있는
# 반쪽 구현을 허용하지 않고, 실제 퍼즐 경로와 화면 프롬프트까지 함께 묶는다.
foreach ($actionName in @('Sprint', 'Crouch', 'Knock', 'Listen', 'HoldBreath')) {
	if (-not $inputConfig.Contains(('ActionName="{0}"' -f $actionName))) {
		throw "Missing-floor input action is missing from DefaultInput.ini: $actionName"
	}
}
if ($inputConfig.Contains('ActionName="Interact",bShift=False,bCtrl=False,bAlt=False,bCmd=False,Key=Q')) {
	throw 'Q regressed to an Interact alias; P4 taps would mix with ordinary interaction.'
}
foreach ($token in @(
	'void AIGPlayerCharacter::BeginSprint()',
	'void AIGPlayerCharacter::ToggleCrouch()',
	'void AIGPlayerCharacter::Knock()',
	'void AIGPlayerCharacter::BeginListen()',
	'void AIGPlayerCharacter::BeginHoldBreath()',
	'SprintFootstepLoudness = 0.50f',
	'CrouchFootstepLoudness = 0.05f',
	'MaximumBreathHoldSeconds = 4.0f',
	'IsHourSealed()'
)) {
	if (-not $playerSource.Contains($token)) {
		throw "Missing-floor player-input runtime contract is missing: $token"
	}
}
# v2.5의 낮 기록은 입력 이름, 일시정지, 실제 출처, 접근성 재페이지와 종이
# 원샷이 한 경로로 연결돼야 한다. 정적인 배경 이미지 한 장만으로는 통과하지 않는다.
foreach ($actionName in @('Journal', 'JournalPrevious', 'JournalNext')) {
	if (-not $inputConfig.Contains(('ActionName="{0}"' -f $actionName))) {
		throw "Missing-floor journal input action is missing: $actionName"
	}
}
foreach ($token in @(
	'void AIGPlayerController::BeginJournalInput()',
	'JournalHoldSeconds = 0.30',
	'UsesToggleHoldInteractions()',
	'지금은 그럴 때가 아니다.',
	'SetMissingFloorJournalState',
	'CreateJournalPageTurn'
)) {
	if (-not $playerControllerSource.Contains($token)) {
		throw "Missing-floor journal controller contract is missing: $token"
	}
}
foreach ($token in @(
	'T_MissingFloorJournalPaper_D',
	'MissingFloorJournalTitle',
	'JournalLaneAdministration',
	'JournalLaneLife',
	'JournalLanePersonal',
	'Record.bConfirmed',
	'GetCaptionSizeScale()',
	'CardsPerLanePerPage'
)) {
	if (-not $horrorHudSource.Contains($token)) {
		throw "Missing-floor journal HUD contract is missing: $token"
	}
}
foreach ($token in @(
	'UIGToneSequenceSoundWave::CreateJournalPageTurn',
	'IGJournalPageTurn',
	'fingertip brushes'
)) {
	if (-not $toneSequenceSource.Contains($token)) {
		throw "Missing-floor journal sound contract is missing: $token"
	}
}
foreach ($token in @(
	'## 26. 2026-08-11 제품 감사',
	'### 26.2 첫 12분 체험 계약',
	'### 26.3 오디오 제작·믹스 계약',
	'### 26.4 UI·UX·조작 편의 계약',
	'### 26.5 성능·화질 예산',
	'채택 방화벽'
)) {
	if (-not $missingFloorStory.Contains($token)) {
		throw "Missing-floor v2.5 product contract is missing: $token"
	}
}
foreach ($token in @(
	'bool AIGMissingFloorNightThreeDirector::TryPlayerKnock',
	'bool AIGMissingFloorNightThreeDirector::TryPlayerListen',
	'MissingFloor.Verb.Knock',
	'MissingFloor.Verb.Listen'
)) {
	if (-not $nightThreeSource.Contains($token)) {
		throw "Missing-floor contextual verb routing is missing: $token"
	}
}
foreach ($token in @(
	'KnockPromptFormatKeyboard',
	'ListenPromptFormatGamepad'
)) {
	if (-not $horrorHudSource.Contains($token)) {
		throw "Missing-floor contextual prompt contract is missing: $token"
	}
}
foreach ($token in @(
	'MissingFloor->IsHourSealed()',
	'401호에 물어볼 수 있다.'
)) {
	if (-not $playerControllerSource.Contains($token)) {
		throw "Missing-floor hint policy runtime is missing: $token"
	}
}
# 다섯 새벽은 렌더가 없는 대신 시간·오디오·입력·저장이 모두 실제여야 한다.
foreach ($token in @(
	'DurationSeconds = 160.0f',
	'24.0f, 24.0f, 52.0f, 58.0f, 74.0f',
	'80.0f, 115.0f, 118.0f, 148.0f, 159.2f',
	'CreateTrappedBreathBed',
	'CreateAnswerKnockPattern(this, 0.93f)',
	'SetFifthDawnInterludeCompleted(true)',
	'SetSensoryInterludePresentation'
)) {
	if (-not $fifthDawnSource.Contains($token)) {
		throw "Missing-floor fifth-dawn runtime contract is missing: $token"
	}
}
foreach ($token in @(
	'MissingFloorFifthDawnDirector',
	'ValidateTimeline()',
	'HandleFifthDawnCompleted',
	'WasFifthDawnInterludeCompleted()'
)) {
	if (-not $greyboxSource.Contains($token)) {
		throw "Missing-floor fifth-dawn route wiring is missing: $token"
	}
}
if (-not $horrorHudSource.Contains('bSensoryInterludePresentation')) {
	throw 'The fifth-dawn black frame no longer suppresses the ordinary HUD.'
}
# v2.4 플레이 표면 계약. 조작감·UI/UX·난이도·사운드 실행·재미·몰입 절이
# 사라지면 서사가 맞아도 같은 빌드로 취급하지 않는다.
foreach ($token in @(
	'## 18. 조작감 계약',
	'## 19. UI·UX 계약',
	'## 20. 난이도 설계',
	'## 21. 효과음·믹스 제작 명세',
	'## 22. 재미의 구조',
	'## 23. 몰입 계약',
	'즉시 차단 22개'
)) {
	if (-not $missingFloorStory.Contains($token)) {
		throw "Missing-floor play-surface contract is missing: $token"
	}
}
foreach ($token in @(
	'세대 계량기 3개(401·402·403)',
	'**403호 정사·404 이스터에그:**',
	'**`404 / Not Found`**',
	'상호작용·윤곽선·자막·',
	'진실·엔딩·04:30과 연결하지 않는다'
)) {
	if (-not $missingFloorStory.Contains($token)) {
		throw "403/404 entrance story boundary is missing: $token"
	}
}
foreach ($forbidden in @(
	'1F 드레인/에어빼기',
	'저수조 양수펌프',
	'무엇을 하든 4분이 흐른다',
	'되어 보는 4분',
	'렌더 0의 4분',
	'즉시 차단 15개'
)) {
	if ($missingFloorStory.Contains($forbidden)) {
		throw "Superseded missing-floor story literal remains: $forbidden"
	}
}

# --- 5층 분진 잔흔: 얇은 자리는 기질로 사라져야 한다 -----------------------
# 마스크를 불투명도에만 쓰면 남는 것은 「있다/없다」뿐이고, 어두운 바닥 위의
# 균일한 밝은 판이 된다 — CCTV 정중앙에서 바닥 위에 떠 있는 도장 자국으로
# 읽혔다. 그래서 같은 마스크로 밝기까지 변조하는데, **방향이 중요하다.**
#
# 처음 판은 알베도를 곱했다. BLEND_MASKED에 부분 투명이 없으니 얇은 자리를
# 표현할 수단이 알베도뿐인데, 곱셈은 그것을 검정 쪽으로 끌어당겼다. 근접
# 프레임에서 잔흔이 덮은 픽셀의 중앙값이 바로 인접한 바닥의 0.84배로 나왔다 —
# 석고 분진이 흙때가 된 것이다. 얇은 가루층은 기질과 가루의 혼합이므로
# 기질색에서 잔흔색으로 보간해야 한다. A가 기질, B가 잔흔이다. 이 순서가
# 뒤집히면 두껍게 쌓인 자리가 바닥색이 되고 스친 자리만 밝아진다.
$residueMaterialSource = Get-Content -Raw -Encoding UTF8 -LiteralPath (
	Join-Path $projectRoot 'Scripts/create_textured_materials.py')
foreach ($needle in @(
	'substrate = spec.get("substrate")',
	'MaterialExpressionLinearInterpolate',
	'ground, "", shade, "A"',
	'base, "", shade, "B"',
	'opacity, "", shade, "Alpha"',
	'shade, "", unreal.MaterialProperty.MP_BASE_COLOR')) {
	if (-not $residueMaterialSource.Contains($needle)) {
		throw "ART_ASSET_CONTRACT FAIL: residue substrate blend is missing: $needle"
	}
}
# 곱셈 방식으로 되돌아가면 얇은 자리가 다시 검정으로 간다.
if ($residueMaterialSource.Contains('shaded, "", unreal.MaterialProperty.MP_BASE_COLOR')) {
	throw 'ART_ASSET_CONTRACT FAIL: 잔흔 알베도를 곱하면 얇은 자리가 바닥보다 어두워진다.'
}
# 어두운 콘크리트 위의 석고 분진은 살짝 밝은 얼룩이다. 원래 값(0.48~0.68
# 알베도, 증폭 1.8~2.5)은 바닥의 두 배 밝기에 이진 실루엣이었다. 기질값은
# 그 면의 실제 재질에서 온다 — 바닥은 콘크리트 다크, 벽은 마른 석고다.
foreach ($needle in @(
	'"color": (0.24, 0.23, 0.21), "mask_gain": 1.15',
	'"color": (0.26, 0.25, 0.23), "mask_gain": 1.10',
	'"color": (0.23, 0.22, 0.205), "mask_gain": 1.25',
	'"substrate": (0.124, 0.126, 0.132)',
	'"substrate": (0.487, 0.474, 0.443)')) {
	if (-not $residueMaterialSource.Contains($needle)) {
		throw "ART_ASSET_CONTRACT FAIL: residue tuning drifted: $needle"
	}
}
# 같은 계열의 잔흔을 다른 요각으로 열린 바닥에서 겹쳐 두면, 카메라에서 보면
# 칠해 놓은 X 한 개로 합쳐진다. 분진 이음은 벽선에 붙여 눕힌다.
$residueSceneSource = Get-Content -Raw -Encoding UTF8 -LiteralPath (
	Join-Path $projectRoot 'Source/IndieGame/Core/IGPrologueWorldScene.cpp')
foreach ($needle in @(
	'FVector(150.0f, 906.0f, 1200.26f)',
	'FVector(268.0f, 42.0f, 0.32f)',
	'FVector(20.0f, 718.0f, 1200.25f)')) {
	if (-not $residueSceneSource.Contains($needle)) {
		throw "ART_ASSET_CONTRACT FAIL: residue placement drifted: $needle"
	}
}

Write-Host 'ART_ASSET_CONTRACT PASS raw=50 masks=9 overlays=23 signage=6 material_scans=14 pbr_maps=62 meshes=45 photo_meshes=50'
