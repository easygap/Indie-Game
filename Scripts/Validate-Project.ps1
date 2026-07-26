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
	'Scripts/RunGame.bat',
	'Scripts/RunGame-Chapter2.bat',
	'Scripts/RunGame-Chapter3.bat',
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
$thirdMorningSource = Get-Content -Raw -Encoding UTF8 -LiteralPath (
	Join-Path $projectRoot 'Source/IndieGame/Sequence/IGThirdMorningDirector.cpp')
$horrorHudSource = Get-Content -Raw -Encoding UTF8 -LiteralPath (
	Join-Path $projectRoot 'Source/IndieGame/Player/IGHorrorHUD.cpp')
$playerCharacterSource = Get-Content -Raw -Encoding UTF8 -LiteralPath (
	Join-Path $projectRoot 'Source/IndieGame/Player/IGPlayerCharacter.cpp')
$demoDirectorSource = Get-Content -Raw -Encoding UTF8 -LiteralPath (
	Join-Path $projectRoot 'Source/IndieGame/Sequence/IGDemoDirector.cpp')
$swingDoorSource = Get-Content -Raw -Encoding UTF8 -LiteralPath (
	Join-Path $projectRoot 'Source/IndieGame/Interaction/IGSwingDoor.cpp')
$slidingDoorSource = Get-Content -Raw -Encoding UTF8 -LiteralPath (
	Join-Path $projectRoot 'Source/IndieGame/Interaction/IGSlidingDoor.cpp')
$neighborhoodSource = Get-Content -Raw -Encoding UTF8 -LiteralPath (
	Join-Path $projectRoot 'Source/IndieGame/Environment/IGNeighborhoodLifeDirector.cpp')

foreach ($requiredChapterTwoCopy in @(
	'알람을 끄자',
	'지갑과 손전등을 챙겨 편의점으로 가자',
	'열린 403호 안을 확인하자',
	'엘리베이터를 타고 내려가자',
	'새벽24 무영로점으로 가자',
	'진열대에서 새벽샘물 500mL를 집자',
	'POS 옆에 놓인 영수증을 확인하자',
	'계산하고 나가자',
	'방금 나온 영수증을 확인하자',
	'집으로 돌아가자',
	'…뒤에서 나갔나.',
	'403호가… 여기였나?',
	'…우리 집이랑 똑같네.',
	'지금… 안에서 잠갔는데.',
	'눌리지도 않은 층이잖아.',
	'아까는… 없었어.',
	'같은 시간. 같은 물.'
)) {
	if (-not $secondMorningSource.Contains($requiredChapterTwoCopy) -and
		-not $worldSceneSource.Contains($requiredChapterTwoCopy)) {
		throw "Required STORY_BIBLE CH02 copy is missing: $requiredChapterTwoCopy"
	}
}

foreach ($requiredReceiptCopy in @(
	'2024/07/26(금) 04:44',
	'새벽샘물500mL',
	'1,100원',
	'사업자등록번호 110-81-04444',
	'체크카드(일시불)',
	'5417-****-****-0444',
	'승인번호',
	'04821736',
	'거래NO'
)) {
	if (-not $worldSceneSource.Contains($requiredReceiptCopy)) {
		throw "Required STORY_BIBLE receipt copy is missing: $requiredReceiptCopy"
	}
}

foreach ($requiredProgressionGate in @(
	'SetAdditionalRequiredState',
	'State.CH02.Loop.SawReceipt',
	'State.CH02.Loop.EnteredAlley',
	'ChapterTwoOutdoorZone'
)) {
	if (-not $worldSceneSource.Contains($requiredProgressionGate)) {
		throw "Required CH02 progression/ambient gate is missing: $requiredProgressionGate"
	}
}

if (-not $secondMorningSource.Contains('State.CH02.Loop.ReadDuplicateReceipt')) {
	throw 'Required CH02 duplicate-receipt reading gate is missing.'
}

foreach ($requiredChapterThreeCopy in @(
	'방 안의 물과 시간을 확인하자',
	'금 간 휴대폰을 확인하자',
	'쌀 보냈다. 물 많이 마시고 다녀라.',
	'주말에 내려오니?',
	'7/26 07:02  지운아',
	'생수 · 골목 고양이 밥',
	'미르워터텍',
	'03:50  단수 밸브 잠금 / 옥상 개방',
	'03:56  비용 재검토로 작업 연기',
	'□ 열쇠 회수     □ 뚜껑 확인',
	'7/29 제출  “옥상 잠김 / 작업 취소”',
	'위에서 고양이가 긁는 줄 알았는데',
	'안경을 물 위에 놓고 뚜껑을 열어 둔다',
	'2024년 8월 16일',
	'가족에게 돌아갔다.'
)) {
	if (-not $thirdMorningSource.Contains($requiredChapterThreeCopy)) {
		throw "Required STORY_BIBLE CH03 copy is missing: $requiredChapterThreeCopy"
	}
}

foreach ($requiredChapterThreeGate in @(
	'State.CH03.Flood.ReadMotherPhone',
	'bMotherPhoneRead',
	'bPlannerRead',
	'bEstimateRead',
	'EIGThirdMorningPhase::Choice',
	'BeginEndingB'
)) {
	if (-not $thirdMorningSource.Contains($requiredChapterThreeGate)) {
		throw "Required CH03 progression/ending gate is missing: $requiredChapterThreeGate"
	}
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

Write-Host 'Project structure validation passed (this is not an Unreal build).' -ForegroundColor Green
