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
	'Scripts/RunGame.bat',
	'Scripts/RunGame-Chapter2.bat',
	'Scripts/RunGame-Chapter3.bat',
	'Scripts/Resolve-UnrealEditor.ps1',
	'Scripts/Run-Rebirth-Greybox.bat',
	'Scripts/Test-Rebirth-NarrativeContract.ps1',
	'Scripts/Test-Rebirth-RouteMatrix.ps1',
	'Scripts/RunEditor.bat',
    'Source/IndieGame.Target.cs',
    'Source/IndieGameEditor.Target.cs',
    'Source/IndieGame/IndieGame.Build.cs',
    'Source/IndieGame/IndieGame.h',
    'Source/IndieGame/IndieGame.cpp',
	'Source/IndieGame/Sequence/IGObjectiveProvider.h',
	'Source/IndieGame/Sequence/IGSecondMorningDirector.h',
	'Source/IndieGame/Sequence/IGSecondMorningDirector.cpp',
	'Source/IndieGame/Sequence/IGThirdMorningDirector.h',
	'Source/IndieGame/Sequence/IGThirdMorningDirector.cpp',
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

$engineResolver = Get-Content -Raw -Encoding UTF8 -LiteralPath (
	Join-Path $projectRoot 'Scripts/Resolve-UnrealEditor.ps1')
foreach ($resolverInvariant in @(
	'$env:IG_UNREAL_EDITOR',
	'LauncherInstalled.dat',
	'Build\Build.version',
	'Test-EngineAssociation'
)) {
	if (-not $engineResolver.Contains($resolverInvariant)) {
		throw "Portable Unreal resolver invariant is missing: $resolverInvariant"
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
$gameplayTagsConfig = Get-Content -Raw -Encoding UTF8 -LiteralPath (
	Join-Path $projectRoot 'Config/DefaultGameplayTags.ini')
$rebirthGreyboxScript = Get-Content -Raw -Encoding UTF8 -LiteralPath (
	Join-Path $projectRoot 'Scripts/Run-Rebirth-Greybox.bat')
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
	'REBIRTH_GREYBOX PASS'
)) {
	if (-not $thirdMorningSource.Contains($requiredChapterThreeGate)) {
		throw "Required CH03 progression/ending gate is missing: $requiredChapterThreeGate"
	}
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

Write-Host 'Project structure validation passed (this is not an Unreal build).' -ForegroundColor Green
