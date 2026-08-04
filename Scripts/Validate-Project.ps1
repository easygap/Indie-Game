[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
$projectFile = Join-Path $projectRoot 'IndieGame.uproject'

$requiredFiles = @(
    'IndieGame.uproject',
    'Config/DefaultEngine.ini',
    'Config/DefaultGame.ini',
    'Config/DefaultInput.ini',
    'Config/DefaultGameplayTags.ini',
	'Content/Maps/Prologue_Morning.umap',
	'Content/Prototype/Materials/M_RoomWall.uasset',
	'Content/Prototype/Materials/M_RoomFloor.uasset',
	'Content/Prototype/Materials/M_DarkWood.uasset',
	'Content/Prototype/Materials/M_Bedding.uasset',
	'Content/Prototype/Materials/M_Door.uasset',
	'Content/Prototype/Materials/M_Alarm.uasset',
	'Content/Prototype/Materials/M_FridgeBody.uasset',
	'Content/Prototype/Materials/M_FridgeInterior.uasset',
	'Content/Prototype/Materials/M_WalletBrown.uasset',
	'Content/Prototype/Materials/M_WindowGlow.uasset',
	'Content/Prototype/Materials/M_Asphalt.uasset',
	'Content/Prototype/Materials/M_Concrete.uasset',
	'Content/Prototype/Materials/M_ConcreteDark.uasset',
	'Content/Prototype/Materials/M_WindowDark.uasset',
	'Content/Prototype/Materials/M_NightSky.uasset',
	'Content/Prototype/Materials/M_StreetLampGlow.uasset',
	'Content/Prototype/Materials/M_TrashBag.uasset',
	'Content/Prototype/Materials/M_Cardboard.uasset',
	'Content/Prototype/Materials/M_StoreFloor.uasset',
	'Content/Prototype/Materials/M_LightPanel.uasset',
	'Content/Prototype/Materials/M_SignMint.uasset',
	'Content/Prototype/Materials/M_SignWhite.uasset',
	'Content/Prototype/Materials/M_Glass.uasset',
	'Content/Prototype/Materials/M_MetalFrame.uasset',
	'Content/Prototype/Materials/M_PlasticDark.uasset',
	'Content/Prototype/Materials/M_CounterTop.uasset',
	'Content/Prototype/Materials/M_CoolerBody.uasset',
	'Content/Prototype/Materials/M_ScreenGlow.uasset',
	'Content/Prototype/Materials/M_WaterBlue.uasset',
	'Content/Prototype/Materials/M_SignBladeLit.uasset',
	'Content/Prototype/Materials/M_SignMainLit.uasset',
	'Content/Prototype/Materials/M_BottleGreen.uasset',
	'Content/Prototype/Materials/M_BottleBrown.uasset',
	'Content/Prototype/Materials/M_SnackRed.uasset',
	'Content/Prototype/Materials/M_SnackYellow.uasset',
	'Content/Prototype/Materials/M_SnackBlue.uasset',
	'Content/Prototype/Materials/M_CupNoodle.uasset',
	'Content/SourceArt/AI/SheetPaperNotes_v2.png',
	'Content/Prototype/Textures/T_PaperClean_V2_D.uasset',
	'Content/Prototype/Textures/T_PaperWet_V2_D.uasset',
	'Content/Prototype/Textures/T_PaperFolded_V2_D.uasset',
	'Content/Prototype/Textures/T_PaperOld_V2_D.uasset',
	'Content/Prototype/Textures/T_SignMain_D.uasset',
	'Content/Prototype/Textures/T_PriceStrip_D.uasset',
	'Content/Prototype/Textures/T_PosterSale_D.uasset',
	'Content/Prototype/Textures/T_LabelWater_D.uasset',
	'Docs/Media/prologue-bedroom.png',
	'Docs/Media/prologue-alley.png',
	'Docs/Media/prologue-store.png',
	'Docs/Media/ch02-mirror-room.png',
	'Docs/Media/ch02-lobby-offering.png',
	'Docs/Media/ch02-receipt-0444.png',
	'Docs/Media/ch03-full-fridge.png',
	'Docs/Media/ch03-stair-up.png',
	'Docs/Media/ch03-roof-tank.png',
	'Docs/Media/ch03-tank-reveal.png',
	'Docs/FEASIBILITY.md',
	'Docs/PERFORMANCE.md',
	'Docs/RELEASE_VALIDATION.md',
	'Docs/SAVE_COMPATIBILITY.md',
	'Scripts/RunGame.bat',
	'Scripts/RunGame-Chapter2.bat',
	'Scripts/RunGame-Chapter3.bat',
	'Scripts/Resolve-UnrealEditor.ps1',
	'Scripts/Run-Rebirth-Greybox.bat',
	'Scripts/Run-Rebirth-ReleaseValidation.ps1',
	'Scripts/Run-Rebirth-PersistenceSpikes.ps1',
	'Scripts/Run-Rebirth-CH02FreedomSpikes.ps1',
	'Scripts/Run-Rebirth-CheckpointAnchorSpikes.ps1',
	'Scripts/Run-Rebirth-BackgroundRuntimeValidation.ps1',
	'Scripts/Test-Rebirth-NarrativeContract.ps1',
	'Scripts/Test-Rebirth-ItemContinuityContract.ps1',
	'Scripts/Test-Rebirth-ChapterTwoTimeEntryContract.ps1',
	'Scripts/Test-Rebirth-AccessibilityContract.ps1',
	'Scripts/Test-ArtAssetContract.ps1',
	'Scripts/Build-ArtAssets.ps1',
	'Scripts/Test-Rebirth-RouteMatrix.ps1',
	'Scripts/RunEditor.bat',
    'Source/IndieGame.Target.cs',
    'Source/IndieGameEditor.Target.cs',
    'Source/IndieGame/IndieGame.Build.cs',
    'Source/IndieGame/IndieGame.h',
    'Source/IndieGame/IndieGame.cpp',
	'Source/IndieGame/Sequence/IGObjectiveProvider.h',
	'Source/IndieGame/Interaction/IGTimeEntryPuzzle.h',
	'Source/IndieGame/Interaction/IGTimeEntryPuzzle.cpp',
	'Source/IndieGame/Sequence/IGSecondMorningDirector.h',
	'Source/IndieGame/Sequence/IGSecondMorningDirector.cpp',
	'Source/IndieGame/Sequence/IGThirdMorningDirector.h',
	'Source/IndieGame/Sequence/IGThirdMorningDirector.cpp',
	'Source/IndieGame/Sequence/IGRebirthPersistenceProbe.h',
	'Source/IndieGame/Sequence/IGRebirthPersistenceProbe.cpp',
	'Source/IndieGame/Accessibility/IGAccessibilitySubsystem.h',
	'Source/IndieGame/Accessibility/IGAccessibilitySubsystem.cpp',
	'Source/IndieGame/Core/IGPrologueGameMode.cpp',
	'Source/IndieGame/Narrative/IGRebirthNarrativeTypes.h',
	'Source/IndieGame/Narrative/IGRebirthNarrativeSubsystem.h',
	'Source/IndieGame/Narrative/IGRebirthNarrativeSubsystem.cpp',
	'Source/IndieGame/Save/IGSaveGame.h',
	'Source/IndieGame/Save/IGSaveSubsystem.cpp',
	'Source/IndieGame/Environment/IGNeighborhoodLifeDirector.h',
	'Source/IndieGame/Environment/IGNeighborhoodLifeDirector.cpp',
	'Source/IndieGame/Audio/IGChapterOnePresenceAudioComponent.h',
	'Source/IndieGame/Audio/IGChapterOnePresenceAudioComponent.cpp'
)

$missing = @(
    foreach ($relativePath in $requiredFiles) {
        if (-not (Test-Path -LiteralPath (Join-Path $projectRoot $relativePath))) {
            $relativePath
        }
    }
)

if ($missing.Count -gt 0) {
    throw "Missing required project files: $($missing -join ', ')"
}

$descriptor = Get-Content -Raw -LiteralPath $projectFile | ConvertFrom-Json
if ($descriptor.FileVersion -ne 3) {
    throw 'IndieGame.uproject must use descriptor FileVersion 3.'
}

if (-not ($descriptor.Modules | Where-Object { $_.Name -eq 'IndieGame' -and $_.Type -eq 'Runtime' })) {
    throw 'IndieGame.uproject is missing the IndieGame Runtime module.'
}

if ($descriptor.EngineAssociation -ne '5.8') {
    throw 'IndieGame.uproject must target the portable launcher association 5.8.'
}

$androidFileServer = @(
	$descriptor.Plugins |
		Where-Object { $_.Name -eq 'AndroidFileServer' }
)
if ($androidFileServer.Count -ne 1 -or
	[bool]$androidFileServer[0].Enabled) {
	throw 'AndroidFileServer must stay explicitly disabled to prevent token generation.'
}

$engineResolver = Get-Content -Raw -Encoding UTF8 -LiteralPath (
	Join-Path $projectRoot 'Scripts/Resolve-UnrealEditor.ps1')
foreach ($resolverInvariant in @(
	'$env:IG_UNREAL_EDITOR',
	'LauncherInstalled.dat',
	'Build\Build.version',
	'Test-EngineAssociation',
	'[switch]$Commandlet',
	'UnrealEditor-Cmd.exe'
)) {
	if (-not $engineResolver.Contains($resolverInvariant)) {
		throw "Portable Unreal resolver invariant is missing: $resolverInvariant"
	}
}
$headlessScripts = @(
	'Scripts/Build-ArtAssets.ps1',
	'Scripts/Run-Rebirth-PersistenceSpikes.ps1',
	'Scripts/Run-Rebirth-CH02FreedomSpikes.ps1',
	'Scripts/Run-Rebirth-CheckpointAnchorSpikes.ps1',
	'Scripts/Run-Rebirth-ReleaseValidation.ps1',
	'Scripts/Run-Rebirth-Greybox.bat'
)
foreach ($headlessScript in $headlessScripts) {
	$headlessText = Get-Content -Raw -Encoding UTF8 -LiteralPath (
		Join-Path $projectRoot $headlessScript)
	foreach ($headlessInvariant in @(
		'-Commandlet',
		'-nullrhi',
		'-nosound',
		'-RenderOffscreen'
	)) {
		if (-not $headlessText.Contains($headlessInvariant)) {
			throw "Headless Unreal invariant is missing ($headlessInvariant): $headlessScript"
		}
	}
	foreach ($forbiddenFallback in @(
		'$editorCommand = $editor',
		'$editorCommand = $editorExecutable'
	)) {
		if ($headlessText.Contains($forbiddenFallback)) {
			throw "Headless Unreal script can fall back to a visible editor: $headlessScript"
		}
	}
}
foreach ($launcherScript in @(
	'Scripts/RunGame.bat',
	'Scripts/RunGame-Chapter2.bat',
	'Scripts/RunGame-Chapter3.bat',
	'Scripts/RunEditor.bat',
	'Scripts/Run-Rebirth-Greybox.bat'
)) {
	$launcherText = Get-Content -Raw -Encoding UTF8 -LiteralPath (
		Join-Path $projectRoot $launcherScript)
	if (-not $launcherText.Contains('Resolve-UnrealEditor.ps1') -or
		$launcherText.Contains(
			'C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe')) {
		throw "Launcher must use the portable Unreal resolver: $launcherScript"
	}
}

$engineConfig = Get-Content -Raw -LiteralPath (Join-Path $projectRoot 'Config/DefaultEngine.ini')
foreach ($primaryAssetType in @('IGChapter', 'IGStoryBeat')) {
    if ($engineConfig -notmatch "PrimaryAssetType=`"$primaryAssetType`"") {
        throw "Asset Manager scan rule is missing for $primaryAssetType."
    }
}

foreach ($requiredSetting in @(
	'GameDefaultMap=/Game/Maps/Prologue_Morning',
	'GlobalDefaultGameMode=/Script/IndieGame.IGPrologueGameMode'
)) {
	if ($engineConfig -notmatch [regex]::Escape($requiredSetting)) {
		throw "Default playable scene setting is missing: $requiredSetting"
	}
}

$configFiles = Get-ChildItem -LiteralPath (Join-Path $projectRoot 'Config') -Recurse -File
$securityTokens = @($configFiles | Select-String -SimpleMatch 'SecurityToken=')
if ($securityTokens.Count -gt 0) {
	$locations = $securityTokens | ForEach-Object {
		$relativePath = $_.Path.Substring($projectRoot.Length + 1)
		"${relativePath}:$($_.LineNumber)"
	}
	throw "SecurityToken must not be committed under Config: $($locations -join ', ')"
}

$headers = Get-ChildItem -LiteralPath (Join-Path $projectRoot 'Source') -Recurse -Filter '*.h'
foreach ($header in $headers) {
    $includeLines = @(Select-String -LiteralPath $header.FullName -Pattern '^#include ')
    $generatedIncludes = @($includeLines | Where-Object { $_.Line -match '\.generated\.h"$' })

    if ($generatedIncludes.Count -gt 1) {
        throw "Multiple generated headers found in $($header.FullName)."
    }

    if ($generatedIncludes.Count -eq 1 -and $generatedIncludes[0].LineNumber -ne $includeLines[-1].LineNumber) {
        throw "Generated include must be the final include in $($header.FullName)."
    }
}

$utf8Strict = New-Object System.Text.UTF8Encoding($false, $true)
$koreanSourceFilesWithoutBom = @(
	Get-ChildItem -LiteralPath (Join-Path $projectRoot 'Source') -Recurse -File -Include '*.h','*.cpp' |
		ForEach-Object {
			$sourceFile = $_
			$bytes = [System.IO.File]::ReadAllBytes($sourceFile.FullName)
			try {
				$text = $utf8Strict.GetString($bytes)
			}
			catch {
				throw "Source file is not valid UTF-8: $($sourceFile.FullName)"
			}

			if ($text -match '[\uAC00-\uD7A3]') {
				$hasUtf8Bom = $bytes.Length -ge 3 -and
					$bytes[0] -eq 0xEF -and
					$bytes[1] -eq 0xBB -and
					$bytes[2] -eq 0xBF

				if (-not $hasUtf8Bom) {
					$sourceFile.FullName.Substring($projectRoot.Length + 1)
				}
			}
		}
)
if ($koreanSourceFilesWithoutBom.Count -gt 0) {
	throw "C++ source files containing Korean literals must use UTF-8 BOM: $($koreanSourceFilesWithoutBom -join ', ')"
}

$tickingActors = Get-ChildItem -LiteralPath (Join-Path $projectRoot 'Source') -Recurse -Include '*.h','*.cpp' |
    Select-String -Pattern 'PrimaryActorTick\.bCanEverTick\s*=\s*true'
# Reviewed exceptions. Every entry sets bStartWithTickEnabled = false; the
# first group enables Tick only for bounded animation/presentation windows and
# switches it off again, and IGDemoDirector is a development-only capture
# driver that is spawned solely under -IGCapture / -IGDemo / -IGDemoFrames.
$reviewedTickingFiles = @(
	'IGWakeUpDirector.cpp',
	'IGPlayerCharacter.cpp',
	'IGFridge.cpp',
	'IGSwingDoor.cpp',
	'IGSlidingDoor.cpp',
	'IGElevator.cpp',
	'IGNeighborhoodLifeDirector.cpp',
	'IGDemoDirector.cpp'
)
$unreviewedTickingActors = @($tickingActors | Where-Object {
	$reviewedTickingFiles -notcontains [System.IO.Path]::GetFileName($_.Path)
})
if ($unreviewedTickingActors.Count -gt 0) {
    $locations = $unreviewedTickingActors | ForEach-Object { "$($_.Path):$($_.LineNumber)" }
    throw "Actor Tick requires an explicit architecture review: $($locations -join ', ')"
}

$attributesFile = Join-Path $projectRoot '.gitattributes'
$attributes = Get-Content -Raw -LiteralPath $attributesFile
foreach ($extension in @('*.uasset', '*.umap', '*.png', '*.zip', '*.bin')) {
    if ($attributes -notmatch [regex]::Escape("$extension filter=lfs")) {
        throw "$extension must be tracked by Git LFS."
    }
}

$secondMorningSource = Get-Content -Raw -Encoding UTF8 -LiteralPath (
	Join-Path $projectRoot 'Source/IndieGame/Sequence/IGSecondMorningDirector.cpp')
$worldSceneSource = Get-Content -Raw -Encoding UTF8 -LiteralPath (
	Join-Path $projectRoot 'Source/IndieGame/Core/IGPrologueWorldScene.cpp')
$storyBibleSource = Get-Content -Raw -Encoding UTF8 -LiteralPath (
	Join-Path $projectRoot 'Docs/STORY_BIBLE_REBIRTH.md')
$feasibilityContract = Get-Content -Raw -Encoding UTF8 -LiteralPath (
	Join-Path $projectRoot 'Docs/FEASIBILITY.md')
$thirdMorningSource = Get-Content -Raw -Encoding UTF8 -LiteralPath (
	Join-Path $projectRoot 'Source/IndieGame/Sequence/IGThirdMorningDirector.cpp')
$rebirthNarrativeSource = Get-Content -Raw -Encoding UTF8 -LiteralPath (
	Join-Path $projectRoot 'Source/IndieGame/Narrative/IGRebirthNarrativeSubsystem.cpp')
$rebirthNarrativeHeader = Get-Content -Raw -Encoding UTF8 -LiteralPath (
	Join-Path $projectRoot 'Source/IndieGame/Narrative/IGRebirthNarrativeTypes.h')
$saveGameHeader = Get-Content -Raw -Encoding UTF8 -LiteralPath (
	Join-Path $projectRoot 'Source/IndieGame/Save/IGSaveGame.h')
$saveSubsystemSource = Get-Content -Raw -Encoding UTF8 -LiteralPath (
	Join-Path $projectRoot 'Source/IndieGame/Save/IGSaveSubsystem.cpp')
$moduleRulesSource = Get-Content -Raw -Encoding UTF8 -LiteralPath (
	Join-Path $projectRoot 'Source/IndieGame/IndieGame.Build.cs')
$gameplayTagsConfig = Get-Content -Raw -Encoding UTF8 -LiteralPath (
	Join-Path $projectRoot 'Config/DefaultGameplayTags.ini')
$rebirthGreyboxScript = Get-Content -Raw -Encoding UTF8 -LiteralPath (
	Join-Path $projectRoot 'Scripts/Run-Rebirth-Greybox.bat')
$releaseValidationScriptPath = Join-Path $projectRoot (
	'Scripts/Run-Rebirth-ReleaseValidation.ps1')
$releaseValidationScript = Get-Content -Raw -Encoding UTF8 -LiteralPath (
	$releaseValidationScriptPath)
$persistenceSpikeScriptPath = Join-Path $projectRoot (
	'Scripts/Run-Rebirth-PersistenceSpikes.ps1')
$persistenceSpikeScript = Get-Content -Raw -Encoding UTF8 -LiteralPath (
	$persistenceSpikeScriptPath)
$ch02FreedomSpikeScriptPath = Join-Path $projectRoot (
	'Scripts/Run-Rebirth-CH02FreedomSpikes.ps1')
$ch02FreedomSpikeScript = Get-Content -Raw -Encoding UTF8 -LiteralPath (
	$ch02FreedomSpikeScriptPath)
$checkpointAnchorSpikeScriptPath = Join-Path $projectRoot (
	'Scripts/Run-Rebirth-CheckpointAnchorSpikes.ps1')
$checkpointAnchorSpikeScript = Get-Content -Raw -Encoding UTF8 -LiteralPath (
	$checkpointAnchorSpikeScriptPath)
$backgroundRuntimeScriptPath = Join-Path $projectRoot (
	'Scripts/Run-Rebirth-BackgroundRuntimeValidation.ps1')
$backgroundRuntimeScript = Get-Content -Raw -Encoding UTF8 -LiteralPath (
	$backgroundRuntimeScriptPath)
$persistenceProbeSource = Get-Content -Raw -Encoding UTF8 -LiteralPath (
	Join-Path $projectRoot (
		'Source/IndieGame/Sequence/IGRebirthPersistenceProbe.cpp'))
$prologueGameModeSource = Get-Content -Raw -Encoding UTF8 -LiteralPath (
	Join-Path $projectRoot 'Source/IndieGame/Core/IGPrologueGameMode.cpp')
$releaseValidationProcedure = Get-Content -Raw -Encoding UTF8 -LiteralPath (
	Join-Path $projectRoot 'Docs/RELEASE_VALIDATION.md')
$performanceContract = Get-Content -Raw -Encoding UTF8 -LiteralPath (
	Join-Path $projectRoot 'Docs/PERFORMANCE.md')
$saveCompatibilityContract = Get-Content -Raw -Encoding UTF8 -LiteralPath (
	Join-Path $projectRoot 'Docs/SAVE_COMPATIBILITY.md')
$horrorHudSource = Get-Content -Raw -Encoding UTF8 -LiteralPath (
	Join-Path $projectRoot 'Source/IndieGame/Player/IGHorrorHUD.cpp')
$playerCharacterSource = Get-Content -Raw -Encoding UTF8 -LiteralPath (
	Join-Path $projectRoot 'Source/IndieGame/Player/IGPlayerCharacter.cpp')
$flashlightHeader = Get-Content -Raw -Encoding UTF8 -LiteralPath (
	Join-Path $projectRoot 'Source/IndieGame/Player/IGFlashlightComponent.h')
$flashlightSource = Get-Content -Raw -Encoding UTF8 -LiteralPath (
	Join-Path $projectRoot 'Source/IndieGame/Player/IGFlashlightComponent.cpp')
$morningDirectorSource = Get-Content -Raw -Encoding UTF8 -LiteralPath (
	Join-Path $projectRoot 'Source/IndieGame/Sequence/IGMorningRoutineDirector.cpp')
$pickupItemSource = Get-Content -Raw -Encoding UTF8 -LiteralPath (
	Join-Path $projectRoot 'Source/IndieGame/Interaction/IGPickupItem.cpp')
$checkoutSource = Get-Content -Raw -Encoding UTF8 -LiteralPath (
	Join-Path $projectRoot 'Source/IndieGame/Interaction/IGCheckoutCounter.cpp')
$demoDirectorSource = Get-Content -Raw -Encoding UTF8 -LiteralPath (
	Join-Path $projectRoot 'Source/IndieGame/Sequence/IGDemoDirector.cpp')
$elevatorSource = Get-Content -Raw -Encoding UTF8 -LiteralPath (
	Join-Path $projectRoot 'Source/IndieGame/Interaction/IGElevator.cpp')
$swingDoorSource = Get-Content -Raw -Encoding UTF8 -LiteralPath (
	Join-Path $projectRoot 'Source/IndieGame/Interaction/IGSwingDoor.cpp')
$slidingDoorSource = Get-Content -Raw -Encoding UTF8 -LiteralPath (
	Join-Path $projectRoot 'Source/IndieGame/Interaction/IGSlidingDoor.cpp')
$neighborhoodSource = Get-Content -Raw -Encoding UTF8 -LiteralPath (
	Join-Path $projectRoot 'Source/IndieGame/Environment/IGNeighborhoodLifeDirector.cpp')

foreach ($spatialContinuityInvariant in @(
	'GetIntermediateCabBaseZ',
	'IntermediateDoorPanels',
	'RiderHalfHeight + FloorClearance',
	'bIntermediateStopEnabled ? GetIntermediateCabBaseZ() : -FloorDeltaZ'
)) {
	if (-not $elevatorSource.Contains($spatialContinuityInvariant)) {
		throw "Elevator spatial-continuity invariant is missing: $spatialContinuityInvariant"
	}
}
if ($elevatorSource.Contains(
	'Rider->GetActorLocation() - FVector(0, 0, FloorDeltaZ)')) {
	throw 'Elevator must land from its visible cab floor, not a relative falling offset.'
}
foreach ($worldContinuityInvariant in @(
	'constexpr float SecondFloorZ = 300.0f',
	'FVector(-140, -213.2f, 154)'
)) {
	if (-not $worldSceneSource.Contains($worldContinuityInvariant)) {
		throw "World spatial-continuity invariant is missing: $worldContinuityInvariant"
	}
}
if ($worldSceneSource.Contains('FVector(-187.2f, -60, 152)')) {
	throw 'The calendar must not regress behind the wardrobe and bedside table.'
}
if (-not $demoDirectorSource.Contains('aborting spatial-continuity capture') -or
	$demoDirectorSource.Contains('snapping to waypoint')) {
	throw 'Demo capture must fail on a blocked threshold instead of teleporting through it.'
}

foreach ($requiredChapterTwoCopy in @(
	'알람을 끄자',
	'달라진 아침을 확인하거나 복도로 나가자',
	'열린 403호를 보거나 아래층으로 내려가자',
	'불 켜진 편의점에서 사람을 찾거나 다른 흔적을 확인하자',
	'카운터의 직원 호출 버튼을 눌러 보자',
	'POS 옆의 04:44 거래 기록을 확인하자',
	'처음 결제와 지금 기록을 대조하자',
	'방으로 돌아가 달라진 소리를 확인하자',
	'…뒤에서 나갔나.',
	'403호가… 여기였나?',
	'…우리 집이랑 똑같네.',
	'도어락 소리가… 바깥에서 났다. 문은 아직 열린다.',
	'눌리지도 않은 층이잖아.',
	'아까는… 없었어.',
	'처음 결제는 4시 31분인데.',
	'401호도 관리실도… 나를 찾고 있었어.'
)) {
	if (-not $secondMorningSource.Contains($requiredChapterTwoCopy) -and
		-not $worldSceneSource.Contains($requiredChapterTwoCopy)) {
		throw "Required STORY_BIBLE CH02 copy is missing: $requiredChapterTwoCopy"
	}
}

foreach ($requiredReceiptCopy in @(
	'BuildPurchaseReceiptSummary',
	'BuildPurchaseReceiptData',
	'ActualPurchase0431',
	'DeathOverlay0444',
	'2024/07/26(금) 04:31',
	'2024/07/26(금) 04:44',
	'새벽샘물 500mL',
	'한강수 1L',
	'맑은산 2L',
	'사업자등록번호 110-81-32765',
	'체크카드(일시불)',
	'5417-****-****-2719',
	'승인번호',
	'거래NO'
)) {
	if (-not $worldSceneSource.Contains($requiredReceiptCopy)) {
		throw "Required STORY_BIBLE receipt copy is missing: $requiredReceiptCopy"
	}
}

foreach ($purchaseProfileContract in @(
	@{
		Profile = 'ProfileA500MlX2'
		Product = '새벽샘물 500mL'
		Quantity = 2
		UnitPrice = 1000
	},
	@{
		Profile = 'ProfileB1LX1'
		Product = '한강수 1L'
		Quantity = 1
		UnitPrice = 1500
	},
	@{
		Profile = 'ProfileC2LX2'
		Product = '맑은산 2L'
		Quantity = 2
		UnitPrice = 2000
	}
)) {
	$profilePattern = '(?s)case EIGRebirthPurchaseProfile::' +
		[regex]::Escape($purchaseProfileContract.Profile) +
		':.*?' + [regex]::Escape($purchaseProfileContract.Product) +
		'.*?Spec\.Quantity\s*=\s*' + $purchaseProfileContract.Quantity +
		';.*?Spec\.UnitPrice\s*=\s*' + $purchaseProfileContract.UnitPrice + ';'
	if ($worldSceneSource -notmatch $profilePattern) {
		throw "Purchase profile receipt contract is incomplete: $($purchaseProfileContract.Profile)"
	}
}

foreach ($storyProfileContract in @(
	'| A | 새벽샘물 500mL × 2 | 1,000원 | 2,000원 |',
	'| B | 한강수 1L × 1 | 1,500원 | 1,500원 |',
	'| C | 맑은산 2L × 2 | 2,000원 | 4,000원 |',
	'`PurchaseProfile`의 상품명·용량·수량·단가·합계는 CH02에도 그대로 유지',
	'기존 영수증과 중복 영수증은 거래 시각만 `04:31`에서 `04:44`로 바꾼다.'
)) {
	if (-not $storyBibleSource.Contains($storyProfileContract)) {
		throw "STORY_BIBLE purchase-profile invariant is missing: $storyProfileContract"
	}
}

foreach ($receiptTimelineContract in @(
	@{
		Receipt = 'ChapterOneReceipt'
		Timeline = 'ActualPurchase0431'
	},
	@{
		Receipt = 'ExistingReceipt'
		Timeline = 'DeathOverlay0444'
	},
	@{
		Receipt = 'DuplicateReceipt'
		Timeline = 'DeathOverlay0444'
	}
)) {
	$timelinePattern = '(?s)ConfigureReceipt\(\s*' +
		[regex]::Escape($receiptTimelineContract.Receipt) +
		'\s*,\s*IGPrologueWorld::EReceiptTimeline::' +
		[regex]::Escape($receiptTimelineContract.Timeline) + '\s*\)'
	if ($worldSceneSource -notmatch $timelinePattern) {
		throw "Receipt timeline contract is incomplete: $($receiptTimelineContract.Receipt)"
	}
}

foreach ($requiredBagContract in @(
	'REBIRTH.PurchaseBagProxy',
	'AddStaticPurchaseBagProxy',
	'RebirthPurchaseProfileOnPickup == PurchaseProfile',
	'Component->SetVisibility(bShowBag, true)'
)) {
	if (-not $worldSceneSource.Contains($requiredBagContract)) {
		throw "Purchase-bag presentation contract is missing: $requiredBagContract"
	}
}

$openApartmentDoorContracts = [regex]::Matches(
	$worldSceneSource,
	'TArray<FIGDoorRequirement> NoDoorRequirements;').Count
if ($openApartmentDoorContracts -lt 2 -or
	$worldSceneSource.Contains('SecondDoorNeedFridge') -or
	$worldSceneSource.Contains('SecondDoorNeedWallet') -or
	$worldSceneSource.Contains('SecondDoorNeedTorch')) {
	throw 'CH01 and CH02 apartment exits must not hard-lock optional investigation routes.'
}

foreach ($requiredProgressionGate in @(
	'ConfigureChapterAction',
	'State.CH02.Loop.CalledEmployee',
	'State.CH02.Loop.EnteredAlley',
	'ChapterTwoOutdoorZone',
	'MirrorAlarmMemo',
	'FVector(300, -305, 1010)'
)) {
	if (-not $worldSceneSource.Contains($requiredProgressionGate)) {
		throw "Required CH02 progression/ambient gate is missing: $requiredProgressionGate"
	}
}

if (-not $secondMorningSource.Contains('CH02.ManagementComplaint')) {
	throw 'Required CH02 management-complaint truth source is missing.'
}

if (-not $secondMorningSource.Contains('State.CH02.Loop.ReadDuplicateReceipt')) {
	throw 'Required CH02 duplicate-receipt reading gate is missing.'
}
if (-not $secondMorningSource.Contains(
		'EIGRebirthConvergencePoint::C3SecondMorning') -or
	-not $secondMorningSource.Contains('Truth.Alarm0510') -or
	-not $secondMorningSource.Contains('Truth.DeathOverlay') -or
	-not $secondMorningSource.Contains('Truth.WasSearched') -or
	-not $secondMorningSource.Contains('EnteredAlleyTag') -or
	-not $worldSceneSource.Contains(
		'[대조] 원거래 04:31 / 현재 04:44 중복') -or
	$secondMorningSource -match
		'HasState\(EnteredStoreTag\)\s*&&\s*CanConvergeSecondMorning' -or
	$secondMorningSource.Contains('진열대에서 새벽샘물 500mL를 집자') -or
	$secondMorningSource.Contains('계산하고 나가자') -or
	$worldSceneSource -match
	'ConfigureChapterPurchase\([\s\S]{0,250}State\.CH02\.Loop\.HasWater') {
	throw 'CH02 must converge through optional truth sources without a live water-repurchase gate.'
}
foreach ($selectionInvariant in @(
	'State.CH01.Morning.LeftApartment',
	'LeftApartmentStateTag',
	'ReturnForProfileSwap',
	'ReleaseCarriedActor(this)',
	'HandlePurchaseSelectionChanged',
	'bHeavyBagInteractionProxyActive',
	'ProfileC2LX2'
)) {
	if (-not $gameplayTagsConfig.Contains($selectionInvariant) -and
		-not $morningDirectorSource.Contains($selectionInvariant) -and
		-not $pickupItemSource.Contains($selectionInvariant) -and
		-not $worldSceneSource.Contains($selectionInvariant) -and
		-not $playerCharacterSource.Contains($selectionInvariant)) {
		throw "CH01 free-selection/static-proxy invariant is missing: $selectionInvariant"
	}
}
if ($flashlightHeader.Contains('BatterySeconds') -or
	$flashlightSource -match
		'BatteryFraction\s*=\s*FMath::Max\([^;]*DeltaSeconds' -or
	-not $flashlightSource.Contains('presentation-only and always recover')) {
	throw 'Flashlight brown-outs must remain presentation-only without battery depletion.'
}
if (-not $checkoutSource.Contains('ConfigureChapterAction') -or
	-not $checkoutSource.Contains('bRequiresPrimaryState = false') -or
	-not $checkoutSource.Contains('bPlayRegisterPresentation = false')) {
	throw 'The CH02 employee call must reuse the POS without product or payment audio.'
}

foreach ($requiredChapterThreeCopy in @(
	'방을 더 보거나 복도로 나가자',
	'서비스함을 보거나 물이 온 방향을 찾자',
	'쌀 보냈다. 물 많이 마시고 다녀라.',
	'주말에 내려오니?',
	'7/26 07:02  지운아',
	'생수 · 골목 고양이 밥',
	'미르워터텍',
	'03:58  작업 중단 / 점검구 임시 하강',
	'04:03  강만식  [사진 2장 전송]',
	'실제 입력시각  07/26 06:20:42',
	'미르워터텍 설비 재확인  7월 31일 04:00',
	'04:00  경찰·소방 합동 개방',
	'직결 급수 잠그기',
	'압력 해제 열기',
	'0 확인 후 바닥 배수 열기',
	'들어온 앞발자국 확인하기',
	'앞의 흔적과 대조한다',
	'고양이는 나갔어.',
	'물을 친 건… 호스였고.',
	'저 안에 있던 게, 나였어.',
	'점검봉을 끼운다',
	'2024년 7월 31일 04:00',
	'가족에게 돌아갔다.'
)) {
	if (-not $thirdMorningSource.Contains($requiredChapterThreeCopy)) {
		throw "Required STORY_BIBLE CH03 copy is missing: $requiredChapterThreeCopy"
	}
}

foreach ($requiredChapterThreeGate in @(
	'BeginP3PressureRelease',
	'P3PressureKPa',
	'FocusedEvidence',
	'IsEvidencePairValid',
	'ResolveEvidencePair',
	'HandleRebirthTruthChanged',
	'bAllEvidenceActorsReady',
	'bAllP5SourcesValid',
	'bAllNegligenceSourcesValid',
	'EIGRebirthConvergencePoint::C5FinalChoice',
	'SetTankOpenedEarly',
	'REBIRTH_GREYBOX PASS',
	'REBIRTH_SPIKE PASS s1_outfit_sleeve',
	'ValidateRebirthRoofDoor',
	'EIGRoofDoorState::LatchedGap',
	'EIGRoofDoorState::PulledOpen',
	'EIGRoofDoorState::ReturnToGap',
	'REBIRTH_RELEASE PASS s2_roof_door',
	'authoritative_collision=1',
	'ValidateRebirthCollisionRoute',
	'ValidateRebirthAudioQueue',
	'ValidateRebirthItemContinuity',
	'ReleaseValidationSaveIdleRetryCount > 100',
	'savegame_v3 autosave did not become idle',
	'HandleReleaseValidationSaveCompleted',
	'HandleReleaseValidationLoadCompleted',
	'FinishRebirthEndingValidation',
	'FCollisionShape::MakeCapsule(34.0f, 96.0f)',
	'OutFloorSamples = FloorSamples.Num()',
	'OutCapsuleSegments = UE_ARRAY_COUNT(RouteSegments)',
	'OnGeneratePCMAudio(GeneratedPcm, RequestedSamples)',
	'OutNonZeroSamples > 0',
	'savegame_v3 stale slot cleanup failed',
	'Progress.ChapterId.MatchesTagExact',
	'Progress.CheckpointTag.MatchesTagExact',
	'Progress.MapPackageName.IsNone',
	'bDeleteSucceeded',
	'bSlotDeleted',
	'REBIRTH_RELEASE PASS collision_route',
	'REBIRTH_RELEASE PASS audio_queue',
	'REBIRTH_RELEASE PASS s5_item_continuity',
	'REBIRTH_RELEASE PASS savegame_v3',
	'REBIRTH_RELEASE PASS ending='
)) {
	if (-not $thirdMorningSource.Contains($requiredChapterThreeGate)) {
		throw "Required CH03 progression/ending gate is missing: $requiredChapterThreeGate"
	}
}
foreach ($requiredEndToEndInvariant in @(
	'IGRebirthEndToEndValidation',
	'REBIRTH_E2E PASS ch01_router',
	'REBIRTH_E2E PASS ch02_router',
	'cat_aftermath=1',
	'REBIRTH_E2E PASS ch03_handoff'
)) {
	if (-not $worldSceneSource.Contains($requiredEndToEndInvariant)) {
		throw "REBIRTH end-to-end invariant is missing: $requiredEndToEndInvariant"
	}
}
foreach ($requiredChapterTwoEndToEndInvariant in @(
	'RunRebirthEndToEndRoute',
	'AddState(WakeAlarmStoppedTag);',
	'AddState(WakeStandingTag);',
	'State.CH02.Wake.Standing'
)) {
	if (-not $secondMorningSource.Contains($requiredChapterTwoEndToEndInvariant)) {
		throw "CH02 end-to-end state transition is missing: $requiredChapterTwoEndToEndInvariant"
	}
}
$chapterTwoWakeAutosavePattern =
	'(?s)WakeDirector->ConfigureChapterSaveTags\s*\(.*?' +
	'Checkpoint\.CH02\.Woke.*?true\s*\);'
if ($worldSceneSource -notmatch $chapterTwoWakeAutosavePattern) {
	throw 'CH02 must write the Woke checkpoint after the player reaches the safe standing state.'
}
foreach ($requiredRoofDoorInvariant in @(
	'RoofDoorLeaf->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics)',
	'RoofDoorLeaf->GetCollisionResponseToChannel(ECC_Pawn)',
	'IsRoofDoorReturnClear',
	'OverlapAnyTestByObjectType',
	'FCollisionShape::MakeCapsule(3.5f, 3.5f)',
	'RoofDoorLatchedAngleDegrees = 5.441396f',
	'MeasureRoofDoorFreeEdgeGap()',
	'FVector(1645.5f, -340.5f, CatCenterZ)',
	'FVector(1655.5f, -339.0f, CatCenterZ)',
	'OutCatEnterPasses == 20',
	'OutCatExitPasses == 20',
	'OutLatchedHumanBlocks == 20',
	'OutOpenHumanPasses == 20',
	'OutReturnHumanBlocks == 20'
)) {
	if (-not $thirdMorningSource.Contains($requiredRoofDoorInvariant)) {
		throw "S2 authoritative roof-door invariant is missing: $requiredRoofDoorInvariant"
	}
}
foreach ($requiredCommonPropInvariant in @(
	'ReleaseValidationTankLidBeforeEnding',
	'CountOwnedChapterThreeActions(EIGChapterThreeAction::OpenTank)',
	'TankLidAction == ReleaseValidationTankLidBeforeEnding',
	'TankLidCountAfterEnding == 1',
	'OwnedActionCountAfterEnding',
	'ExpectedLidLocation',
	'ValidateCommonDiscoveryWorldState()',
	'ExpectedRodLocation',
	'IsRetiredDocument',
	'bAllEvidenceRetired',
	'!TankLidAction->GetActorEnableCollision()',
	'!TankLidAction->IsInteractionEnabled()',
	'bAllBodySilhouetteHidden',
	'ReleaseValidationSafetyOpeningCues == ExpectedSafetyOpeningCues',
	'Safety.GasDetector',
	'Safety.Ventilation',
	'Safety.Harness',
	'Safety.TwoClimbers',
	'Safety.HatchOpen',
	'REBIRTH_SPIKE PASS s4_common_prop',
	'actual_state=1 safety_cues=5'
)) {
	if (-not $thirdMorningSource.Contains($requiredCommonPropInvariant)) {
		throw "S4 common-prop invariant is missing: $requiredCommonPropInvariant"
	}
}
foreach ($requiredChapterThreeRouteInvariant in @(
	'FVector(975, -227.5f, 130)',
	'FVector(975, 227.5f, 130)',
	'FVector(975, 0, 245)',
	'FVector(1265, -451.5f, 170)',
	'FVector(470, 147, 20)',
	'FVector(1450, -502.5f, 205)',
	'FVector(1450, -297.5f, 205)',
	'FVector(1450, -400, 400)',
	'FVector(2308.5f, -300, 610), FVector(87, 220, 20)',
	'FloorSamples.Reserve(41)',
	'FVector(1040.0f, 0.0f, StandingCenter)',
	'FVector(1455.0f, -420.0f, 180.0f + StandingCenter)',
	'FVector(1868.0f, -300.0f, IGThirdMorning::RoofFloorZ + StandingCenter)',
	'FVector(2280.0f, -300.0f, 620.0f + StandingCenter)'
)) {
	if (-not $thirdMorningSource.Contains($requiredChapterThreeRouteInvariant)) {
		throw "CH03 physical-route invariant is missing: $requiredChapterThreeRouteInvariant"
	}
}
foreach ($forbiddenChapterThreeRouteBlocker in @(
	'CreateBlock(FVector(975, 0, 130), FVector(20, 760, 280)',
	'CreateBlock(FVector(1240, -400, 170), FVector(420, 250, 20)',
	'FVector(1265, -400, 170), FVector(470, 250, 20)',
	'CreateBlock(FVector(1450, -400, 205), FVector(18, 250, 410)',
	'CreateBlock(FVector(2325, -300, 610), FVector(150, 220, 20)'
)) {
	if ($thirdMorningSource.Contains($forbiddenChapterThreeRouteBlocker)) {
		throw "CH03 physical route regressed to a blocking shell: $forbiddenChapterThreeRouteBlocker"
	}
}
foreach ($requiredExteriorStairInvariant in @(
	'const bool bHasAuthoredExteriorStair',
	'TankExteriorAccessStairMesh && LadderFailureRungMesh',
	'for (int32 StepIndex = 0; StepIndex < 18; ++StepIndex)',
	'StairCollision->SetVisibility(false, true)',
	'const FVector RailStart(1915, -300 + Side * 56.0f, 335)',
	'FVector(2235, -300, 578.5f)'
)) {
	if (-not $thirdMorningSource.Contains($requiredExteriorStairInvariant)) {
		throw "CH03 exterior-stair physical invariant is missing: $requiredExteriorStairInvariant"
	}
}
foreach ($requiredHatchReachInvariant in @(
	'TankHatchOffset(-96.0f, 0.0f, 0.0f)',
	'TankLidOpenOffset(36.9f, 0.0f, 49.5f)',
	'Tank + IGThirdMorning::TankHatchOffset + FVector(0, 0, 610)',
	'FVector(2308.5f, -300, 610), FVector(87, 220, 20)',
	'Tank + FVector(-125, 0, 400 + InnerRungIndex * 30.0f)',
	'FRotator(-78, 0, 0)'
)) {
	if (-not $thirdMorningSource.Contains($requiredHatchReachInvariant)) {
		throw "CH03 west-side service-hatch invariant is missing: $requiredHatchReachInvariant"
	}
}
foreach ($forbiddenExteriorStairToken in @(
	'FVector(2265, -365, 455)',
	'for (int32 RungIndex = 0; RungIndex < 10; ++RungIndex)',
	'FVector(10, 136, 7)'
)) {
	if ($thirdMorningSource.Contains($forbiddenExteriorStairToken)) {
		throw "The unrelated vertical ladder returned beside the diagonal service stair: $forbiddenExteriorStairToken"
	}
}
foreach ($evidenceQueryInvariant in @(
	'const bool bEvidenceOnly',
	'PresentationMesh->SetCollisionEnabled(ECollisionEnabled::QueryOnly)',
	'PresentationMesh->SetCollisionResponseToAllChannels(ECR_Ignore)',
	'PresentationMesh->SetCollisionResponseToChannel(',
	'ECC_Visibility'
)) {
	if (-not $thirdMorningSource.Contains($evidenceQueryInvariant)) {
		throw "CH03 evidence query-only collision is missing: $evidenceQueryInvariant"
	}
}
if ($thirdMorningSource -match 'BindLambda\(\s*\[this(?:,|\])') {
	throw 'CH03 timer delegates must use weak UObject captures instead of raw this.'
}
if ($worldSceneSource -match 'BindLambda\(\s*\[this(?:,|\])') {
	throw 'World-scene timer delegates must use weak UObject captures instead of raw this.'
}
if (-not $moduleRulesSource.Contains(
	'PrivateDependencyModuleNames.Add("PhysicsCore")')) {
	throw 'The collision release verifier requires a direct PhysicsCore module dependency.'
}
foreach ($forbiddenLegacyChapterThreeGate in @(
	'if (!bFridgeInspected || !bPlannerRead || !bMotherPhoneRead)',
	'EIGChapterThreeAction::EnterTank',
	'2024년 8월 16일',
	'안경을 물 위에 놓고 뚜껑을 열어 둔다'
)) {
	if ($thirdMorningSource.Contains($forbiddenLegacyChapterThreeGate)) {
		throw "Legacy linear CH03 gate/copy remains: $forbiddenLegacyChapterThreeGate"
	}
}
foreach ($requiredChapterThreeEvidenceInvariant in @(
	'P5.HosePawCompression',
	'P5.CouplingImpact',
	'P5.InnerRimFriction',
	'P5.UpperSlipperEnd',
	'P5.LiftedPad',
	'P5.CorrodedClips',
	'P5.InwardHandSmear',
	'P5.CurrentSleeveThreeStitches',
	'P5.TankSleeveThreeStitches',
	'P5.TankHeelWear',
	'P5.TankOutfit',
	'P3.EmptyMeasurements',
	'ManagementApp.Photo0403',
	'Handover.Closed0358',
	'ManagementDb.FalseCompletion0620',
	'Tank + FVector(-118, -72, 632)',
	'Tank + FVector(-135, 10, 626)'
)) {
	if (-not $thirdMorningSource.Contains($requiredChapterThreeEvidenceInvariant)) {
		throw "Required CH03 evidence invariant is missing: $requiredChapterThreeEvidenceInvariant"
	}
}
if (-not $thirdMorningSource.Contains('RequestExitWithStatus(') -or
	$thirdMorningSource.Contains('RequestExit(!bPassed)')) {
	throw 'REBIRTH greybox must return an explicit process status after flushing logs.'
}
$p5PairStart = $thirdMorningSource.IndexOf(
	'bool AIGThirdMorningDirector::IsEvidencePairValid(')
$p5PairEnd = $thirdMorningSource.IndexOf(
	'bool AIGThirdMorningDirector::ResolveEvidencePair(',
	$p5PairStart)
$p5PairBlock = if ($p5PairStart -ge 0 -and $p5PairEnd -gt $p5PairStart) {
	$thirdMorningSource.Substring($p5PairStart, $p5PairEnd - $p5PairStart)
}
else {
	''
}
if ($p5PairBlock -match
	'EvidenceBag\s*,\s*EIGChapterThreeAction::EvidenceWetRung') {
	throw 'P5 fall truth must not be confirmed by the bag and wet rung alone.'
}
if ($thirdMorningSource -match
	'RegisterTruth\s*\(\s*TEXT\("Truth\.Negligence"\)\s*,\s*TEXT\("P3\.EmptyRecordPlusReopenPhoto"\)') {
	throw 'P3 empty measurements and the 04:03 photo must not directly confirm Negligence.'
}
if (-not $thirdMorningSource.Contains(
	'else if (IsEvidencePairValid(FocusedEvidence, Pair.Key))')) {
	throw 'P5 compare prompt must only appear for a valid evidence pair.'
}
foreach ($requiredDeathOverlayBridge in @(
	'State.CH02.Loop.ReadDuplicateReceipt',
	'Truth.DeathOverlay',
	'CH02.DuplicateReceipt'
)) {
	if (-not $worldSceneSource.Contains($requiredDeathOverlayBridge)) {
		throw "CH02-to-CH03 DeathOverlay bridge is missing: $requiredDeathOverlayBridge"
	}
}
foreach ($requiredDeathOverlayFallback in @(
	'MotherPhoneApproval0431',
	'MotherPhoneApproval0444',
	'CH03.PhoneApprovalHistory'
)) {
	if (-not $thirdMorningSource.Contains($requiredDeathOverlayFallback)) {
		throw "CH03 DeathOverlay fallback is missing: $requiredDeathOverlayFallback"
	}
}
$chapterTwoRetirementIndex = $worldSceneSource.IndexOf(
	'static_cast<AActor*>(SecondMorningDirector.Get())')
$deathOverlayBridgeIndex = $worldSceneSource.IndexOf('CH02.DuplicateReceipt')
$legacyClearAfterBridgeIndex = $worldSceneSource.IndexOf(
	'StoryState->ClearStates(false)',
	$deathOverlayBridgeIndex)
$chapterThreeSpawnIndex = $worldSceneSource.IndexOf(
	'ThirdMorningDirector = GetWorld()->SpawnActor',
	$deathOverlayBridgeIndex)
if ($chapterTwoRetirementIndex -lt 0 -or
	$deathOverlayBridgeIndex -le $chapterTwoRetirementIndex -or
	$legacyClearAfterBridgeIndex -le $deathOverlayBridgeIndex -or
	$chapterThreeSpawnIndex -le $legacyClearAfterBridgeIndex) {
	throw 'DeathOverlay must be bridged after retiring CH02 and before its legacy tags are cleared.'
}
foreach ($requiredRebirthStateInvariant in @(
	'FIGRebirthTruthRecord',
	'FIGRebirthNarrativeSnapshot',
	'NarrativeDebt',
	'EquippedOutfitChapters',
	'PlayedOneShotBeats'
)) {
	if (-not $rebirthNarrativeHeader.Contains($requiredRebirthStateInvariant)) {
		throw "REBIRTH state schema invariant is missing: $requiredRebirthStateInvariant"
	}
}
foreach ($requiredRebirthRouterInvariant in @(
	'RegisterTruthSource',
	'RecomputeChapterThreeDebt',
	'C5FinalChoice',
	'Truth.RecheckScheduled0731',
	'Truth.Found0731',
	'ConfirmsNegligence',
	'bUnsafeReopen',
	'bFalseCompletion'
)) {
	if (-not $rebirthNarrativeSource.Contains($requiredRebirthRouterInvariant)) {
		throw "REBIRTH router invariant is missing: $requiredRebirthRouterInvariant"
	}
}
foreach ($requiredTruthTag in @(
	'Truth.RecheckScheduled0731',
	'Truth.Found0731'
)) {
	if (-not $gameplayTagsConfig.Contains($requiredTruthTag)) {
		throw "Required REBIRTH truth tag is missing: $requiredTruthTag"
	}
}
if (-not $rebirthNarrativeSource.Contains(
	'Has(IGRebirthTruth::RecheckScheduled0731)') -or
	-not $rebirthNarrativeSource.Contains(
	'Has(IGRebirthTruth::Found0731)')) {
	throw 'C5 scheduled recheck and C6 actual discovery must remain separate truths.'
}
$c5RouterIndex = $rebirthNarrativeSource.IndexOf(
	'case EIGRebirthConvergencePoint::C5FinalChoice:')
$c6RouterIndex = $rebirthNarrativeSource.IndexOf(
	'case EIGRebirthConvergencePoint::C6AfterDiscovery:')
if ($c5RouterIndex -lt 0 -or $c6RouterIndex -le $c5RouterIndex) {
	throw 'REBIRTH C5/C6 router blocks are malformed.'
}
$routerDefaultIndex = $rebirthNarrativeSource.IndexOf(
	'default:',
	$c6RouterIndex)
if ($routerDefaultIndex -le $c6RouterIndex) {
	throw 'REBIRTH C5/C6 router blocks are malformed.'
}
$c5RouterBlock = $rebirthNarrativeSource.Substring(
	$c5RouterIndex,
	$c6RouterIndex - $c5RouterIndex)
$c6RouterBlock = $rebirthNarrativeSource.Substring(
	$c6RouterIndex,
	$routerDefaultIndex - $c6RouterIndex)
if (-not $c5RouterBlock.Contains('IGRebirthTruth::RecheckScheduled0731') -or
	$c5RouterBlock.Contains('IGRebirthTruth::Found0731')) {
	throw 'C5 must require the scheduled recheck, not the completed discovery.'
}
foreach ($requiredC6Invariant in @(
	'Ending.A',
	'Ending.B',
	'CH03.ActualStateRestored',
	'IGRebirthTruth::Found0731'
)) {
	if (-not $c6RouterBlock.Contains($requiredC6Invariant)) {
		throw "C6 actual-discovery invariant is missing: $requiredC6Invariant"
	}
}
foreach ($requiredGreyboxScriptInvariant in @(
	'-abslog="%RUN_LOG%"',
	'set "EDITOR_EXIT=%ERRORLEVEL%"',
	'REBIRTH_GREYBOX FAIL',
	'if not exist "%RUN_LOG%"'
)) {
	if (-not $rebirthGreyboxScript.Contains($requiredGreyboxScriptInvariant)) {
		throw "REBIRTH greybox stale-log guard is missing: $requiredGreyboxScriptInvariant"
	}
}
$releaseValidationTokens = $null
$releaseValidationParseErrors = $null
[void][System.Management.Automation.Language.Parser]::ParseFile(
	$releaseValidationScriptPath,
	[ref]$releaseValidationTokens,
	[ref]$releaseValidationParseErrors)
if ($releaseValidationParseErrors.Count -gt 0) {
	$parseMessages = $releaseValidationParseErrors |
		ForEach-Object { $_.Message }
	throw "REBIRTH release validation script does not parse: $($parseMessages -join '; ')"
}
$persistenceSpikeTokens = $null
$persistenceSpikeParseErrors = $null
[void][System.Management.Automation.Language.Parser]::ParseFile(
	$persistenceSpikeScriptPath,
	[ref]$persistenceSpikeTokens,
	[ref]$persistenceSpikeParseErrors)
if ($persistenceSpikeParseErrors.Count -gt 0) {
	$parseMessages = $persistenceSpikeParseErrors |
		ForEach-Object { $_.Message }
	throw "REBIRTH persistence spike script does not parse: $($parseMessages -join '; ')"
}
$backgroundRuntimeTokens = $null
$backgroundRuntimeParseErrors = $null
[void][System.Management.Automation.Language.Parser]::ParseFile(
	$backgroundRuntimeScriptPath,
	[ref]$backgroundRuntimeTokens,
	[ref]$backgroundRuntimeParseErrors)
if ($backgroundRuntimeParseErrors.Count -gt 0) {
	$parseMessages = $backgroundRuntimeParseErrors |
		ForEach-Object { $_.Message }
	throw "REBIRTH background runtime script does not parse: $($parseMessages -join '; ')"
}
foreach ($requiredBackgroundInvariant in @(
	'-WindowStyle Hidden',
	"'-nullrhi'",
	"'-nosound'",
	"'-RenderOffscreen'",
	"'-ExecCmds=MAP CHECK,QUIT_EDITOR'",
	'맵 체크 완료:',
	'$unrealDiagnosticPatterns = @(',
	'$unrealDiagnosticAllowlist = @(',
	'Assert-NoUnexpectedUnrealDiagnostics',
	'허용되지 않은 Unreal Ensure/Error/Fatal 진단',
	'REBIRTH_BACKGROUND PASS runtime ending=',
	'REBIRTH_BACKGROUND PASS complete',
	'visible_windows=0'
)) {
	if (-not $backgroundRuntimeScript.Contains($requiredBackgroundInvariant)) {
		throw "REBIRTH background runtime invariant is missing: $requiredBackgroundInvariant"
	}
}
$backgroundDiagnosticGuardCount = [regex]::Matches(
	$backgroundRuntimeScript,
	[regex]::Escape('Assert-NoUnexpectedUnrealDiagnostics')).Count
if ($backgroundDiagnosticGuardCount -lt 3) {
	throw 'Background diagnostic guard must cover its definition, Map Check, and runtime logs.'
}
foreach ($requiredReleaseValidationInvariant in @(
	'[switch]$StaticOnly',
	'[ValidateRange(30, 1800)]',
	"'IndieGameEditor'",
	"'IndieGame'",
	"'Development'",
	"'BuildCookRun'",
	"'-clientconfig=Shipping'",
	"'-prereqs'",
	"'/Game/Maps/Prologue_Morning'",
	"'-ExecCmds=MAP CHECK,QUIT_EDITOR'",
	'맵 체크 완료:',
	'REBIRTH_RELEASE_HARNESS PASS map_check',
	"'-IGRebirthReleaseValidation'",
	"'-IGRebirthEndToEndValidation'",
	'"-IGRebirthEnding=$Ending"',
	"Invoke-RebirthRuntimeCase -EditorCommand `$editorCommand -Ending 'A'",
	"Invoke-RebirthRuntimeCase -EditorCommand `$editorCommand -Ending 'B'",
	'Assert-ReleaseLog',
	'REBIRTH_E2E PASS ch01_router',
	'REBIRTH_E2E PASS ch02_router',
	'approval_0431=1',
	'approval_screen=1',
	'cat_aftermath=1',
	'authored_housings=2',
	'layered_displays=2',
	'pressure_caps=2',
	'time_entry_physical=2',
	'route_order=$expectedRouteOrder',
	"'p1_p2'",
	"'p2_p1'",
	'REBIRTH_E2E PASS ch03_handoff',
	'REBIRTH_SPIKE PASS s1_outfit_sleeve chapters=3 duplicates=0 stitches=3',
	'REBIRTH_RELEASE PASS s2_roof_door',
	'REBIRTH_SPIKE PASS s4_common_prop ending=$Ending duplicates=0',
	'actual_state=1 safety_cues=5',
	'REBIRTH_RELEASE PASS collision_route',
	'REBIRTH_RELEASE PASS audio_queue',
	'REBIRTH_RELEASE PASS s5_item_continuity profiles=3 closures=2 presentations=2 cases=12 duplicates=0',
	'REBIRTH_RELEASE PASS p3_p5',
	'REBIRTH_RELEASE PASS savegame_v3',
	'REBIRTH_RELEASE PASS complete',
	'StaticContracts.log',
	'EngineResolution.log',
	'Get-SourceState',
	'Test-SourceStateUnchanged',
	'Test-CleanSourceStateLocked',
	'Static validation requires one clean, unchanged Git commit.',
	'Git source state changed during static release validation.',
	'Refusing to write PARTIAL because the final Git',
	'worktreeClean = $initialWorktreeClean',
	'sourceStateUnchanged',
	'sourceChangedDuringRun',
	'cleanSourceStateLocked',
	'$unrealDiagnosticPatterns = @(',
	'$unrealDiagnosticAllowlist = @(',
	'Assert-NoUnexpectedUnrealDiagnostics',
	'Unapproved Unreal Ensure/Error/Fatal diagnostic',
	'Get-AsciiReleaseBuildRoot',
	'Invoke-ReleaseRobocopy',
	'$buildProjectFile',
	'usingAsciiBuildMirror',
	'Get-VcRuntimePrerequisite',
	"'vc_runtime_prerequisite'",
	"[Version]'14.50.35719.0'",
	'VC++ runtime prerequisite was blocked.',
	'vcRuntimePrerequisite = $vcRuntimePrerequisite',
	'schemaVersion = 3',
	'unrealDiagnosticAllowlist = @($unrealDiagnosticAllowlist)',
	'Add-MissingStepResults',
	'automatedReleaseCandidateEligible',
	'releaseEligible = $false',
	'ShippingArchiveManifest.json',
	'Shipping archive directory must be absent or empty',
	"Name -ieq 'IndieGame.exe'",
	'Refusing to write PASS because the final Git source state is not',
	'PASS evidence log could not be hashed',
	'did not report a native process exit code',
	'commitSha = $initialCommitSha',
	'engineAssociation = $engineAssociation',
	'engineBuildVersion = $engineBuildVersion',
	'logSha256 = $logHash',
	"Write-RunSummary -Status 'BLOCKED'",
	"-Status 'PARTIAL'",
	'All automated release stages completed from one clean commit.'
)) {
	if (-not $releaseValidationScript.Contains(
			$requiredReleaseValidationInvariant)) {
		throw "REBIRTH release harness invariant is missing: $requiredReleaseValidationInvariant"
	}
}
$initialSourceStateIndex = $releaseValidationScript.IndexOf(
	'$initialSourceState = Get-SourceState')
$staticContractsIndex = $releaseValidationScript.IndexOf(
	'$staticContractsLog =',
	$initialSourceStateIndex)
$staticPreBranchIndex = $releaseValidationScript.IndexOf(
	'if ($StaticOnly) {',
	$initialSourceStateIndex)
$staticPreGateIndex = $releaseValidationScript.IndexOf(
	"Set-ActiveStep -Name 'source_state_pre'",
	$staticPreBranchIndex)
if ($initialSourceStateIndex -lt 0 -or
	$staticPreBranchIndex -le $initialSourceStateIndex -or
	$staticPreGateIndex -le $staticPreBranchIndex -or
	$staticContractsIndex -le $staticPreGateIndex) {
	throw 'StaticOnly must lock a clean source state before static contracts run.'
}
$staticCompletionBranchIndex = $releaseValidationScript.IndexOf(
	'if ($StaticOnly) {',
	$staticContractsIndex)
$staticPostGateIndex = $releaseValidationScript.IndexOf(
	"Set-ActiveStep -Name 'source_state_post'",
	$staticCompletionBranchIndex)
$staticExitIndex = $releaseValidationScript.IndexOf(
	'exit 0',
	$staticCompletionBranchIndex)
if ($staticCompletionBranchIndex -le $staticContractsIndex -or
	$staticPostGateIndex -le $staticCompletionBranchIndex -or
	$staticExitIndex -le $staticPostGateIndex) {
	throw 'StaticOnly must recheck the clean source state before returning PARTIAL.'
}
$unrealDiagnosticGuardCount = [regex]::Matches(
	$releaseValidationScript,
	[regex]::Escape('Assert-NoUnexpectedUnrealDiagnostics')).Count
if ($unrealDiagnosticGuardCount -lt 5) {
	throw (
		'Unreal diagnostic guard must cover its definition, release A/B logs, ' +
		'persistence/anchor case logs, and Map Check.')
}
foreach ($requiredPersistenceIntegrationInvariant in @(
	"'persistence_spikes'",
	'Run-Rebirth-PersistenceSpikes.ps1',
	'Assert-PersistenceSpikeEvidence',
	'REBIRTH_SPIKE_HARNESS PASS complete boundary=2 cat_choices=5 ch02_time=2 p5=4 p3=7 endings=2',
	'memoryBoundaryProcessRestarts',
	'catChoiceProcessRestarts',
	'ch02TimeProcessRestarts',
	'p5ProcessRestarts',
	'p3ProcessRestarts',
	'endingProcessRestarts',
	'saveSnapshotSha256',
	'Persistence save snapshot hash mismatch',
	'Get-FileHash -Algorithm SHA256',
	'PersistenceSpikes.log'
)) {
	if (-not $releaseValidationScript.Contains(
			$requiredPersistenceIntegrationInvariant)) {
		throw (
			'Release persistence-spike integration invariant is missing: ' +
			$requiredPersistenceIntegrationInvariant)
	}
}
foreach ($requiredCH02FreedomIntegrationInvariant in @(
	"'ch02_freedom_spikes'",
	'Run-Rebirth-CH02FreedomSpikes.ps1',
	'Assert-CH02FreedomSpikeEvidence',
	'REBIRTH_CH02_FREEDOM_HARNESS PASS complete routes=5',
	'CH02FreedomSpikes.log',
	'CH02FreedomSpikes',
	'P1ThenP2',
	'P2ThenP1',
	'SkipP1',
	'SkipP2',
	'SkipBoth'
)) {
	if (-not $releaseValidationScript.Contains(
			$requiredCH02FreedomIntegrationInvariant)) {
		throw (
			'Release CH02-freedom integration invariant is missing: ' +
			$requiredCH02FreedomIntegrationInvariant)
	}
}
foreach ($requiredCH02FreedomHarnessInvariant in @(
	'[ValidateRange(30, 600)]',
	'-WindowStyle Hidden',
	'-IGRebirthEndToEndValidation',
	'-IGRebirthCH02FreedomProbe',
	'IGRebirthCH02Route',
	'route_order=',
	'authored_housings=2',
	'layered_displays=2',
	'time_entry_physical=2 pressure_caps=',
	'Get-FileHash -Algorithm SHA256',
	'REBIRTH_CH02_FREEDOM_HARNESS PASS complete routes=5'
)) {
	if (-not $ch02FreedomSpikeScript.Contains(
			$requiredCH02FreedomHarnessInvariant)) {
		throw (
			'CH02 freedom harness invariant is missing: ' +
			$requiredCH02FreedomHarnessInvariant)
	}
}
foreach ($requiredCheckpointAnchorIntegrationInvariant in @(
	"'checkpoint_anchor_spikes'",
	'Run-Rebirth-CheckpointAnchorSpikes.ps1',
	'Assert-CheckpointAnchorSpikeEvidence',
	'REBIRTH_ANCHOR_HARNESS PASS complete anchors=5 processes=10',
	'CheckpointAnchorSpikes.log',
	'mapReentryCount',
	'capsuleClearCount',
	'floorContactCount',
	'capsule_clear=1 map_reentered=1'
)) {
	if (-not $releaseValidationScript.Contains(
			$requiredCheckpointAnchorIntegrationInvariant)) {
		throw (
			'Release checkpoint-anchor integration invariant is missing: ' +
			$requiredCheckpointAnchorIntegrationInvariant)
	}
}
foreach ($requiredCheckpointAnchorHarnessInvariant in @(
	'[ValidateRange(30, 1800)]',
	'-WindowStyle Hidden',
	'"-IGRebirthPersistenceProbe=$Mode"',
	'"-IGRebirthAnchorCase=$AnchorCase"',
	"'CH02Corridor'",
	"'CH02Store'",
	"'CH03Apartment'",
	"'CH03Flood'",
	"'CH03Roof'",
	'SaveSnapshots',
	'saveSnapshotSha256',
	'Get-FileHash -Algorithm SHA256',
	'REBIRTH_ANCHOR_HARNESS PASS complete anchors=$($anchors.Count)'
)) {
	if (-not $checkpointAnchorSpikeScript.Contains(
			$requiredCheckpointAnchorHarnessInvariant)) {
		throw (
			'Checkpoint anchor harness invariant is missing: ' +
			$requiredCheckpointAnchorHarnessInvariant)
	}
}
foreach ($requiredChapterTwoAnchorInvariant in @(
	'FVector(430.0f, -305.0f, 998.0f)',
	'FVector(2515.0f, -455.0f, 104.0f)'
)) {
	if (-not $worldSceneSource.Contains($requiredChapterTwoAnchorInvariant)) {
		throw (
			'CH02 grounded checkpoint anchor invariant is missing: ' +
			$requiredChapterTwoAnchorInvariant)
	}
}
foreach ($requiredChapterThreeAnchorInvariant in @(
	'RestoredCapsuleCenter = StandingCapsuleCenter + 2.0f',
	'IGThirdMorning::RestoredCapsuleCenter'
)) {
	if (-not $thirdMorningSource.Contains(
			$requiredChapterThreeAnchorInvariant)) {
		throw (
			'CH03 grounded checkpoint anchor invariant is missing: ' +
			$requiredChapterThreeAnchorInvariant)
	}
}
foreach ($requiredPersistenceHarnessInvariant in @(
	'[ValidateRange(30, 1800)]',
	'Start-Process',
	'-WindowStyle Hidden',
	'"-UserDir=$userDirectory"',
	'"-IGRebirthPersistenceProbe=$Mode"',
	"if (`$Mode -eq 'CatChoiceRead')",
	"`$arguments += '-IGChapterTwo'",
	"'BoundaryBeforeWrite'",
	"'BoundaryBeforeRead'",
	"'BoundaryAfterWrite'",
	"'BoundaryAfterRead'",
	"'CatChoiceWrite'",
	"'CatChoiceRead'",
	"'CH02TimeWrite'",
	"'CH02TimeRead'",
	"'P5Write'",
	"'P5Read'",
	"'P3Write'",
	"'P3Read'",
	"'EndingWrite'",
	"'EndingCommit'",
	"'EndingVerify'",
	'$checkpoint -le 6',
	"foreach (`$ending in @('A', 'B'))",
	'memoryBoundaryProcessRestarts = 2',
	'catChoiceProcessRestarts = 5',
	'ch02TimeProcessRestarts = 2',
	'p5ProcessRestarts = 4',
	'p3ProcessRestarts = 7',
	'endingProcessRestarts = 4',
	'SaveSnapshots',
	'Copy-Item',
	'saveSnapshotSha256',
	'saveDeleted',
	'Get-FileHash -Algorithm SHA256',
	'REBIRTH_SPIKE_HARNESS PASS complete boundary=2 cat_choices=5 ch02_time=2 p5=4 p3=7 endings=2'
)) {
	if (-not $persistenceSpikeScript.Contains(
			$requiredPersistenceHarnessInvariant)) {
		throw (
			'Persistence spike harness invariant is missing: ' +
			$requiredPersistenceHarnessInvariant)
	}
}
foreach ($requiredPersistenceProbeInvariant in @(
	'IGRebirthPersistenceProbe=',
	'StartBoundaryWrite',
	'StartBoundaryRead',
	's5_boundary_resume phase=%s transient_tags=0 safe_states=%d',
	'StartCatChoiceWrite',
	'StartCatChoiceRead',
	'ResolveCatChoiceContract',
	'ValidateCatWaterAftermath',
	'RefreshChapterTwoCatWaterAftermath',
	'ValidateChapterTwoCatWaterAftermath',
	's5_cat_resume case=%s ch01_physical=1 ch02_physical=1',
	'State.CH02.Wake.AlarmStopped',
	'State.CH02.Wake.Standing',
	'MakeP5Checkpoint',
	'MatchesP5Checkpoint',
	'P5 checkpoint must be 0..3',
	's3_p5_resume checkpoint=%d exact=1',
	'MakeP3Checkpoint',
	'MatchesP3Checkpoint',
	'P3 checkpoint must be 0..6',
	'ResolveAnchorContract',
	'ValidateLoadedAnchor',
	'OverlapBlockingTestByChannel',
	'LineTraceSingleByChannel',
	's7_anchor_resume case=%s',
	'CommitChapterThreeCommonDiscovery()',
	'State->CommitChapterThreeCommonDiscovery()',
	'branch_exclusive=1',
	'DeleteGameInSlot',
	'REBIRTH_SPIKE PASS %s',
	'RequestExitWithStatus'
)) {
	if (-not $persistenceProbeSource.Contains(
			$requiredPersistenceProbeInvariant)) {
		throw (
			'Persistence probe invariant is missing: ' +
			$requiredPersistenceProbeInvariant)
	}
}
foreach ($requiredPersistenceBootstrapInvariant in @(
	'IGRebirthPersistenceProbe=',
	'SpawnActor<AIGRebirthPersistenceProbe>',
	'bBuildWorldForProbe',
	'TEXT("CatChoiceRead")',
	'IGResumeSave',
	'return;'
)) {
	if (-not $prologueGameModeSource.Contains(
			$requiredPersistenceBootstrapInvariant)) {
		throw (
			'Persistence probe bootstrap invariant is missing: ' +
			$requiredPersistenceBootstrapInvariant)
	}
}
foreach ($requiredReleaseProcedureInvariant in @(
	'진짜 초견 최소 16명',
	'`Wallet` 코호트 8명',
	'`Hood-card (no-wallet)` 코호트 8명',
	'`core_panel_id` 8명',
	'집단 이름 없이 단순히 `8명 중`',
	'각 코호트 전체',
	'퍼즐별 서로 다른 초견',
	'시작자 8명이 찰 때까지 추가 모집',
	'진짜 2회차 CCTV 표본을 최소 6명',
	'R1, R2, R3, R4마다 재사용하지 않는',
	'고유 절대 `-UserDir`',
	'`0 / false / unset`',
	'`FEASIBILITY.md`의 S1~S8',
	'바닥 지지점 41개',
	'평면 여유 구간 9개',
	'클린 VM 스냅샷 또는 별도 물리 PC',
	'Windows 사용자 계정 실행은 사전 점검',
	'`SAVE_COMPATIBILITY.md`',
	'`PERFORMANCE.md`',
	'성능 측정은 G6의 Shipping 후보에서만 승인하며',
	'G3 합격 조건에 포함하지'
)) {
	if (-not $releaseValidationProcedure.Contains(
		$requiredReleaseProcedureInvariant)) {
		throw "Release procedure invariant is missing: $requiredReleaseProcedureInvariant"
	}
}
foreach ($requiredPerformanceInvariant in @(
	'문서 버전: `Windows-v1`',
	'G3·G5를',
	'G6 성능 항목',
	'Intel Core i5-8400 또는 AMD Ryzen 5 2600',
	'Intel Core i5-12400 또는 AMD Ryzen 5 5600',
	'Windows 11 Home/Pro 25H2',
	'26H1을 포함한 다른 기능 업데이트',
	'1280×720',
	'1920×1080',
	'2560×1440',
	'3840×2160',
	'`r.ScreenPercentage=100`',
	'프레임 시간 `p95`',
	'`1% low`',
	'peak committed 12.0GB 이하',
	'peak 5.5GB 이하',
	'설치된 Shipping 배포물의 총 파일 크기는 8.0GB 이하',
	'`MIN-W10-NV`',
	'`MIN-W10-AMD`',
	'`MIN-W11-NV`',
	'`MIN-W11-AMD`',
	'`REC-W11-NV`',
	'`REC-W11-AMD`',
	'여섯 필수 장비',
	'판정은 **BLOCKED**'
)) {
	if (-not $performanceContract.Contains($requiredPerformanceInvariant)) {
		throw "Performance release contract is missing: $requiredPerformanceInvariant"
	}
}
foreach ($requiredSaveCompatibilityInvariant in @(
	'문서 버전: `Save-v1`',
	'`v1-ch02-legacy`',
	'`v2-ch03-nominal`',
	'`v2-normalization-boundary`',
	'`v3-p3-mid-bleed`',
	'`v3-p5-pre-choice`',
	'`v3-ending-common-a`',
	'과거 불변 태그의 원본',
	'현재 v3 클래스의 메모리 레이아웃으로 v1·v2를 직렬화',
	'clean Windows VM 스냅샷 또는 별도 물리 PC',
	'fixture-set.json',
	'fixture_sha256_before',
	'CurrentSchemaVersion보다 큰 외부 버전',
	'현재 G6 세이브 호환성 판정은 **BLOCKED**'
)) {
	if (-not $saveCompatibilityContract.Contains(
		$requiredSaveCompatibilityInvariant)) {
		throw "Save compatibility contract is missing: $requiredSaveCompatibilityInvariant"
	}
}
foreach ($requiredFeasibilityInvariant in @(
	'`Choice→ActualStateRestored→Found0731→CommonDiscoveryCard→BranchCoda`',
	'`ActualStateRestored→Found0731→CommonDiscoveryCard` 세 이벤트',
	'`Choice`는 각 선택값으로 공통 prefix보다 먼저 1회',
	'`BranchCoda`는 공통 prefix 뒤에 1회'
)) {
	if (-not $feasibilityContract.Contains($requiredFeasibilityInvariant)) {
		throw "Feasibility release contract is missing: $requiredFeasibilityInvariant"
	}
}
if (-not $saveGameHeader.Contains('CurrentSchemaVersion = 3') -or
	-not $saveGameHeader.Contains('FIGRebirthNarrativeSnapshot RebirthNarrative') -or
	-not $rebirthNarrativeHeader.Contains('int32 SchemaVersion = 3') -or
	-not $rebirthNarrativeHeader.Contains('FIGRebirthChapterThreeState ChapterThree') -or
	-not $saveSubsystemSource.Contains('RebirthState->BuildSnapshot()') -or
	-not $saveSubsystemSource.Contains('RebirthState->RestoreSnapshot(')) {
	throw 'Save schema v3 and REBIRTH snapshot v3 must persist the complete CH03 state.'
}
$restoreNarrativeIndex = $saveSubsystemSource.IndexOf(
	'RebirthState->RestoreSnapshot(')
$restoreStoryTagsIndex = $saveSubsystemSource.IndexOf(
	'StoryState->RestoreStateSnapshot(')
if ($restoreNarrativeIndex -lt 0 -or
	$restoreStoryTagsIndex -le $restoreNarrativeIndex) {
	throw 'REBIRTH snapshot must restore before legacy story-tag callbacks.'
}
foreach ($requiredPersistenceInvariant in @(
	'State.P3.PressureKPa = P3PressureKPa',
	'&ThisClass::UpdateP3PressureRelease',
	'State.FocusedEvidence',
	'State.AccidentScratchCount',
	'State.bAccidentScratchTailSettled',
	'SelectChapterThreeEnding(',
	'CommitChapterThreeCommonDiscovery()',
	'ResumeRestoredEnding()'
)) {
	if (-not $thirdMorningSource.Contains($requiredPersistenceInvariant)) {
		throw "CH03 persistence invariant is missing: $requiredPersistenceInvariant"
	}
}
foreach ($requiredNormalizationInvariant in @(
	'SnapshotSchemaVersion = 3',
	'NormalizeSnapshot(Snapshot, true)',
	'P3.PressureKPa = FMath::IsFinite',
	'IsValidEvidenceId',
	'IsValidEndingChoice',
	'Ending.CommonDiscoveryCard'
)) {
	if (-not $rebirthNarrativeSource.Contains($requiredNormalizationInvariant)) {
		throw "REBIRTH snapshot normalization invariant is missing: $requiredNormalizationInvariant"
	}
}
$bootstrapStart = $thirdMorningSource.IndexOf(
	'void AIGThirdMorningDirector::BootstrapRebirthContext()')
$commitStateStart = $thirdMorningSource.IndexOf(
	'void AIGThirdMorningDirector::CommitChapterThreeState()')
$openDoorStart = $thirdMorningSource.IndexOf(
	'case EIGChapterThreeAction::OpenApartmentDoor:')
$inspectElevatorStart = $thirdMorningSource.IndexOf(
	'case EIGChapterThreeAction::InspectElevator:')
if ($bootstrapStart -lt 0 -or $commitStateStart -le $bootstrapStart -or
	$openDoorStart -lt 0 -or $inspectElevatorStart -le $openDoorStart) {
	throw 'CH03 outfit timing blocks are malformed.'
}
$bootstrapBlock = $thirdMorningSource.Substring(
	$bootstrapStart,
	$commitStateStart - $bootstrapStart)
$openDoorBlock = $thirdMorningSource.Substring(
	$openDoorStart,
	$inspectElevatorStart - $openDoorStart)
if ($bootstrapBlock.Contains('MarkOutfitEquipped') -or
	-not $openDoorBlock.Contains('MarkOutfitEquipped(FName(TEXT("CH03")))') -or
	-not ($openDoorBlock -match
		'SetRebirthOutfitEquipped\s*\(\s*true\s*,\s*bFirstOutfitPresentation\s*\)')) {
	throw 'CH03 outfit must be committed and shown at the first apartment exit, not at chapter bootstrap.'
}
if (-not $thirdMorningSource.Contains(
	'for (int32 Column = 0; Column < 8; ++Column)')) {
	throw 'CH03 impossible fridge must retain the dense pooled bottle layout.'
}
if ($thirdMorningSource.Contains('TEXT("ROOF EXIT")')) {
	throw 'The Korean villa stair sign must not regress to an English ROOF EXIT label.'
}
if (-not $worldSceneSource.Contains(
	'CreateBlock(FVector(200, -245.6f, 115), FVector(20, 0.8f, 230), WallX, false)')) {
	throw 'The apartment east-wall return must keep its correctly oriented material cap.'
}

if (-not $horrorHudSource.Contains(
	'return Provider ? Provider->GetObjectiveText() : FText::GetEmpty();')) {
	throw 'CH03 HUD objective-provider fallback is missing.'
}

if (-not $playerCharacterSource.Contains(
	'InitCapsuleSize(34.0f, 96.0f)')) {
	throw 'First-person capsule must preserve clearance through the 84-88 cm interior doors.'
}
if (-not $playerCharacterSource.Contains(
	'constexpr float PresentationDurationSeconds = 1.2f') -or
	-not $playerCharacterSource.Contains(
		'Stitch->SetupAttachment(OutfitSleeveProxy)') -or
	-not $playerCharacterSource.Contains(
		'ValidateRebirthOutfitProxy') -or
	-not $playerCharacterSource.Contains(
		'OutStitchCount == 3') -or
	-not $playerCharacterSource.Contains('bPlayPresentation') -or
	$playerCharacterSource.Contains(
		'bOutfitPresentationPlayedForCurrentChapter') -or
	$playerCharacterSource.Contains('SetIgnoreMoveInput') -or
	$playerCharacterSource.Contains('SetIgnoreLookInput')) {
	throw 'The outfit reveal must be a non-blocking 1.2-second sleeve presentation with attached stitches.'
}

foreach ($requiredCaptureWaypoint in @(
	'FVector(198, -160, 0)',
	'FVector(143, -360, 0)',
	'FVector(2515, -457, 0)',
	'FVector(2900, -430, 0)',
	'FVector(2840, -195, 0)',
	'FVector(2665, -250, 0)'
)) {
	if (-not $demoDirectorSource.Contains($requiredCaptureWaypoint)) {
		throw "Required collision-safe CH01 capture waypoint is missing: $requiredCaptureWaypoint"
	}
}
if ($demoDirectorSource.Contains('FVector(2610, -320, 0)')) {
	throw 'CH01 capture route must not use the sub-capsule gap between the register and gondola.'
}
if (-not $worldSceneSource.Contains(
	'CreateBlock(FVector(142, -225, 220), FVector(88, 20, 20), WallX)')) {
	throw 'The apartment entrance must retain 210 cm clearance above the shoe step.'
}
if ($worldSceneSource.Contains(
	'CreateBlock(FVector(2405, -430, 12), FVector(12, 470, 24), Metal)')) {
	throw 'The convenience-store kick rail must not cross the automatic-door threshold.'
}
foreach ($doorSafetySource in @($swingDoorSource, $slidingDoorSource)) {
	if (-not $doorSafetySource.Contains(
		'UCollisionProfile::NoCollision_ProfileName')) {
		throw 'Moving door leaves must release blocking collision while opening.'
	}
}

foreach ($requiredNeighborhoodFeature in @(
	'PrimeOutdoorSequence',
	'bOutdoorSequencePrimed',
	'EIGPooledVehicleKind::DeliveryMotorcycle',
	'ActivateLeaves',
	'CatTraceRoot',
	'EIGNeighborhoodChapterVariant::ChapterTwoAbsent'
)) {
	if (-not $neighborhoodSource.Contains($requiredNeighborhoodFeature)) {
		throw "Required neighborhood-life feature is missing: $requiredNeighborhoodFeature"
	}
}

$narrativeContractScript = Join-Path $projectRoot `
	'Scripts/Test-Rebirth-NarrativeContract.ps1'
& $narrativeContractScript

$routeMatrixScript = Join-Path $projectRoot `
	'Scripts/Test-Rebirth-RouteMatrix.ps1'
& $routeMatrixScript

$itemContinuityContractScript = Join-Path $projectRoot `
	'Scripts/Test-Rebirth-ItemContinuityContract.ps1'
& $itemContinuityContractScript

$chapterTwoTimeEntryContractScript = Join-Path $projectRoot `
	'Scripts/Test-Rebirth-ChapterTwoTimeEntryContract.ps1'
& $chapterTwoTimeEntryContractScript

$accessibilityContractScript = Join-Path $projectRoot `
	'Scripts/Test-Rebirth-AccessibilityContract.ps1'
& $accessibilityContractScript

$artAssetContractScript = Join-Path $projectRoot `
	'Scripts/Test-ArtAssetContract.ps1'
& $artAssetContractScript

Write-Host 'Project structure validation passed (this is not an Unreal build).' -ForegroundColor Green
