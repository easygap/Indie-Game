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
	'Docs/Media/prologue-bedroom.png',
	'Docs/Media/prologue-alley.png',
	'Docs/Media/prologue-store.png',
	'Docs/Media/ch02-mirror-room.png',
	'Docs/Media/ch02-lobby-offering.png',
	'Docs/Media/ch02-receipt-0444.png',
	'Scripts/RunGame.bat',
	'Scripts/RunGame-Chapter2.bat',
	'Scripts/RunEditor.bat',
    'Source/IndieGame.Target.cs',
    'Source/IndieGameEditor.Target.cs',
    'Source/IndieGame/IndieGame.Build.cs',
    'Source/IndieGame/IndieGame.h',
    'Source/IndieGame/IndieGame.cpp',
	'Source/IndieGame/Sequence/IGObjectiveProvider.h',
	'Source/IndieGame/Sequence/IGSecondMorningDirector.h',
	'Source/IndieGame/Sequence/IGSecondMorningDirector.cpp'
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

foreach ($requiredChapterTwoCopy in @(
	'알람을 끄자',
	'…또 물이 없다. 편의점에 다녀오자',
	'계산하고 나가자',
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
	'2026-07-26 04:44',
	'새벽수 500mL',
	'1,100원',
	'체크카드 승인',
	'4482**',
	'적립 없음'
)) {
	if (-not $worldSceneSource.Contains($requiredReceiptCopy)) {
		throw "Required STORY_BIBLE receipt copy is missing: $requiredReceiptCopy"
	}
}

Write-Host 'Project structure validation passed (this is not an Unreal build).' -ForegroundColor Green
