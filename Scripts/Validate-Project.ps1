[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
$projectFile = Join-Path $projectRoot 'IndieGame.uproject'

$requiredFiles = @(
    'IndieGame.uproject',
    'Config/DefaultEngine.ini',
    'Config/DefaultGame.ini',
	'Config/DefaultGameUserSettings.ini',
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
	'Content/SourceArt/AI/SheetSubmergedBodyAnatomyReference_v2.png',
	'Content/SourceArt/AI/ApplicationIcon_raw.png',
	'Content/SourceArt/AI/DialogueHUDConcept_v1.png',
	'Content/SourceArt/AI/TextureHudDialogueFilm.png',
	'Content/SourceArt/AI/ApartmentVisualTarget_v1.png',
	'Content/SourceArt/AI/TextureApartmentWallpaperVintage.png',
	'Content/SourceArt/AI/TextureMovingBoxCardboard_v1.png',
	'Content/SourceArt/AI/TextureCaptureMercyNotePaper_D.png',
	'Content/SourceArt/AI/MaskApartmentWallPatina.png',
	'Content/SourceArt/AI/TitleBackgroundMissingFloor_v1.png',
	'Content/SourceArt/T_HudDialogueFilm_D.png',
	'Content/SourceArt/T_TitleBackground_D.png',
	'Content/SourceArt/T_ApartmentWallpaperV2_D.png',
	'Content/SourceArt/T_ApartmentWallpaperV2_N.png',
	'Content/SourceArt/T_ApartmentWallpaperV2_R.png',
	'Content/SourceArt/T_ApartmentWallpaperV2_A.png',
	'Content/SourceArt/T_ApartmentWallPatina_M.png',
	'Content/SourceArt/T_MovingBoxCardboard_D.png',
	'Content/SourceArt/T_MovingBoxCardboard_N.png',
	'Content/SourceArt/T_MovingBoxCardboard_R.png',
	'Content/SourceArt/T_MovingBoxCardboard_A.png',
	'Content/SourceArt/T_CaptureMercyNote_D.png',
	'Build/Windows/ApplicationIcon.png',
	'Build/Windows/Application.ico',
	'Content/Prototype/Textures/T_PaperClean_V2_D.uasset',
	'Content/Prototype/Textures/T_PaperWet_V2_D.uasset',
	'Content/Prototype/Textures/T_PaperFolded_V2_D.uasset',
	'Content/Prototype/Textures/T_PaperOld_V2_D.uasset',
	'Content/Prototype/Textures/T_HudDialogueFilm_D.uasset',
	'Content/UI/Textures/T_TitleBackground_D.uasset',
	'Content/Prototype/Textures/T_ApartmentWallpaperV2_D.uasset',
	'Content/Prototype/Textures/T_ApartmentWallpaperV2_N.uasset',
	'Content/Prototype/Textures/T_ApartmentWallpaperV2_R.uasset',
	'Content/Prototype/Textures/T_ApartmentWallpaperV2_A.uasset',
	'Content/Prototype/Textures/T_ApartmentWallPatina_M.uasset',
	'Content/Prototype/Materials/M_ApartmentWallPatina.uasset',
	'Content/Prototype/Textures/T_MovingBoxCardboard_D.uasset',
	'Content/Prototype/Textures/T_MovingBoxCardboard_N.uasset',
	'Content/Prototype/Textures/T_MovingBoxCardboard_R.uasset',
	'Content/Prototype/Textures/T_MovingBoxCardboard_A.uasset',
	'Content/Prototype/Materials/M_MovingBoxCardboardUV.uasset',
	'Content/Prototype/Textures/T_CaptureMercyNote_D.uasset',
	'Content/Prototype/Materials/M_CaptureMercyNote.uasset',
	'Content/Meshes/SM_CaptureMercyNote.uasset',
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
	'Docs/Media/ch03-fifth-floor-doorway.png',
	'Docs/Media/ch03-lens-droplet-default-early.png',
	'Docs/Media/ch03-lens-droplet-default-late.png',
	'Docs/Media/ch03-lens-droplet-reduced-early.png',
	'Docs/Media/ch03-lens-droplet-reduced-late.png',
	'Docs/Media/ch03-roof-tank.png',
	'Docs/Media/ch03-tank-reveal.png',
	'Docs/Media/dialogue-hud-default-1080.png',
	'Docs/Media/dialogue-hud-accessibility-200-1080.png',
	'Docs/Media/title-menu-first-run-1080.png',
	'Docs/Media/m65-capture-mercy-note.png',
	'Docs/Media/m65-mercy-note-slide.gif',
	'Docs/Media/m65-first-run-audio-calibration.png',
	'Docs/Media/settings-display-1080.png',
	'Docs/Media/settings-accessibility-1080.png',
	'Source/IndieGame/UI/Fonts/Pretendard-Regular.otf',
	'Source/IndieGame/UI/Fonts/Pretendard-SemiBold.otf',
	'Source/IndieGame/UI/Fonts/GowunBatang-Bold.ttf',
	'Source/IndieGame/UI/Fonts/OFL-Pretendard.txt',
	'Source/IndieGame/UI/Fonts/OFL-GowunBatang.txt',
	'Docs/Media/readme-route-preview.gif',
	'Docs/FEASIBILITY.md',
	'Docs/IMAGEGEN_PROMPTS_2026-08-05.md',
	'Docs/IMAGEGEN_PROMPTS_2026-08-12.md',
	'Docs/IMAGEGEN_PROMPTS_2026-08-12_AUDIO_CALIBRATION.md',
	'Docs/PERFORMANCE.md',
	'Docs/RELEASE_VALIDATION.md',
	'Docs/SAVE_COMPATIBILITY.md',
	'Docs/UI_STYLE_GUIDE.md',
	'Scripts/RunGame.bat',
	'Scripts/RunGame-Chapter2.bat',
	'Scripts/RunGame-Chapter3.bat',
	'Scripts/Resolve-UnrealEditor.ps1',
	'Scripts/Run-Prologue-Capture.ps1',
	'Scripts/Run-Rebirth-Greybox.bat',
	'Scripts/Run-Rebirth-ReleaseValidation.ps1',
	'Scripts/run_rebirth_savegame_roundtrips.py',
	'Scripts/Run-Rebirth-CH02FreedomSpikes.ps1',
	'Scripts/Run-Rebirth-CheckpointAnchorSpikes.ps1',
	'Scripts/Run-Rebirth-BackgroundRuntimeValidation.ps1',
	'Scripts/Run-Rebirth-LensDropletCapture.ps1',
	'Scripts/Run-Rebirth-FrontendShippingProbe.ps1',
	'Scripts/Run-MissingFloor-SettingsPreview.ps1',
	'Scripts/Test-Rebirth-NarrativeContract.ps1',
	'Scripts/Test-Rebirth-ItemContinuityContract.ps1',
	'Scripts/Test-Rebirth-EvidenceContract.ps1',
	'Scripts/Test-Rebirth-ChapterTwoTimeEntryContract.ps1',
	'Scripts/Test-Rebirth-AudioContract.ps1',
	'Scripts/Test-Rebirth-AccessibilityContract.ps1',
	'Scripts/Test-Rebirth-DialogueContract.ps1',
	'Scripts/Test-Rebirth-FrontendContract.ps1',
	'Scripts/prepare_application_icon.py',
	'Scripts/Test-Windows-ExecutableIcon.ps1',
	'Scripts/Copy-Windows-ExecutableVersionResource.ps1',
	'Scripts/Test-Windows-ExecutableMetadata.ps1',
	'Scripts/create_readme_media.py',
	'Scripts/Test-ArtAssetContract.ps1',
	'Scripts/Test-MissingFloor-M0InputContract.ps1',
	'Scripts/Test-MissingFloor-ProductionEntryContract.ps1',
	'Scripts/Run-MissingFloor-ArrivalProbe.ps1',
	'Scripts/Run-MissingFloor-ArrivalCapture.ps1',
	'Scripts/Test-MissingFloor-M5RevealContract.ps1',
	'Scripts/Test-MissingFloor-ReleaseEndingContract.ps1',
	'Scripts/Test-MissingFloor-ReleaseGateContract.ps1',
	'Scripts/Test-MissingFloor-SignatureSfxContract.ps1',
	'Scripts/Test-MissingFloor-TuningTableContract.ps1',
	'Scripts/Test-MissingFloor-MixAndMovementContract.ps1',
	'Scripts/Test-MissingFloor-InputBindingContract.ps1',
	'Scripts/Test-MissingFloor-BibleContract.ps1',
	'Scripts/Run-MissingFloor-EndingPreview.ps1',
	'Scripts/Test-MissingFloor-M6AudioVisualContract.ps1',
	'Scripts/Test-MissingFloor-M65MercyNoteContract.ps1',
	'Scripts/Test-MissingFloor-M8DifficultyContract.ps1',
	'Scripts/Test-MissingFloor-M65AudioCalibrationContract.ps1',
	'Scripts/Test-MissingFloor-M3CctvChannelContract.ps1',
	'Scripts/Test-MissingFloor-M3DoorBeatContract.ps1',
	'Scripts/Test-MissingFloor-M4PassByContract.ps1',
	'Scripts/Test-MissingFloor-NightFiveSlotContract.ps1',
	'Scripts/Run-MissingFloor-NightFiveProbe.ps1',
	'Scripts/Run-MissingFloor-CctvFeedProbe.ps1',
	'Scripts/Run-MissingFloor-AudioCalibrationPreview.bat',
	'Scripts/Build-ArtAssets.ps1',
	'Scripts/Test-Rebirth-RouteMatrix.ps1',
	'Scripts/RunEditor.bat',
    'Source/IndieGame.Target.cs',
    'Source/IndieGameEditor.Target.cs',
    'Source/IndieGame/IndieGame.Build.cs',
    'Source/IndieGame/IndieGame.h',
	'Source/IndieGame/IndieGame.cpp',
	'Source/IndieGame/Player/IGFrontendMenuLayout.h',
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

$readme = Get-Content -Raw -Encoding UTF8 -LiteralPath (
	Join-Path $projectRoot 'README.md')
foreach ($requiredReadmeToken in @(
	# 소개에 쓰는 실제 화면과 실행 안내가 빠지지 않았는지 확인한다.
	# 퍼즐 해답이나 개별 연출의 검증용 캡처를 README에 고정하지 않는다.
	'Docs/Media/readme/prologue-corridor.webp',
	'Docs/Media/readme/prologue-alley.webp',
	'Docs/Media/readme/prologue-store-counter.webp',
	'Docs/Media/readme/readme-route-preview.gif',
	'Docs/Media/readme/night-listener-chase.gif',
	'Docs/Media/readme/p1-meter-cabinet.webp',
	'Docs/PLAYING.md',
	'## 조작',
	'## 접근성',
	'## 직접 해 보기'
)) {
	if (-not $readme.Contains($requiredReadmeToken)) {
		throw "README product overview is missing: $requiredReadmeToken"
	}
}
foreach ($forbiddenReadmeToken in @(
	'## Contributors',
	'## 기여자'
)) {
	if ($readme.Contains($forbiddenReadmeToken)) {
		throw "README must not contain a generated contributor section: $forbiddenReadmeToken"
	}
}
$readmeMediaReferences = @(
	[regex]::Matches($readme, '(?:src="|\]\()(?<path>Docs/[^\)"]+)') |
		ForEach-Object { $_.Groups['path'].Value } |
		Sort-Object -Unique
)
foreach ($readmeMediaReference in $readmeMediaReferences) {
	if (-not (Test-Path -LiteralPath (
		Join-Path $projectRoot $readmeMediaReference) -PathType Leaf)) {
		throw "README media or document link is missing: $readmeMediaReference"
	}
}
$readmeGif = Get-Item -LiteralPath (
	Join-Path $projectRoot 'Docs/Media/readme/readme-route-preview.gif')
if ($readmeGif.Length -lt 500KB -or $readmeGif.Length -gt 10MB) {
	throw 'README route preview must stay legible and below the 10 MB review budget.'
}
$chaseGif = Get-Item -LiteralPath (
	Join-Path $projectRoot 'Docs/Media/readme/night-listener-chase.gif')
if ($chaseGif.Length -lt 500KB -or $chaseGif.Length -gt 10MB) {
	throw 'README chase preview must stay legible and below the 10 MB review budget.'
}
# 접힌 영역의 이미지도 내려받을 수 있으므로, 실제 이미지 태그를 모두 센다.
# 일반 링크로 제공하는 장면 모음 GIF는 본문 이미지 용량에 넣지 않는다.
$readmeInlineImages = @(
	[regex]::Matches($readme, '(?:src="|!\[[^\]]*\]\()(?<path>Docs/[^\)"]+)') |
		ForEach-Object { $_.Groups['path'].Value } |
		Sort-Object -Unique
)
$readmeInlineBytes = 0L
foreach ($readmeInlineImage in $readmeInlineImages) {
	if (-not $readmeInlineImage.StartsWith('Docs/Media/readme/')) {
		throw "README 본문에는 축소본을 사용해 주세요: $readmeInlineImage"
	}
	$readmeInlineBytes += (Get-Item -LiteralPath (
		Join-Path $projectRoot $readmeInlineImage)).Length
}
if ($readmeInlineBytes -gt 6MB) {
	throw 'README 본문 이미지의 총용량은 6 MB 이하여야 합니다.'
}
$readmeMediaRecipe = Get-Content -Raw -Encoding UTF8 -LiteralPath (
	Join-Path $projectRoot 'Scripts/create_readme_media.py')
foreach ($requiredRecipeToken in @(
	'CAPTURES = (',
	'prologue-bedroom.png',
	'prologue-store.png',
	'Actual in-game capture route preview'
)) {
	if (-not $readmeMediaRecipe.Contains($requiredRecipeToken)) {
		throw "README media recipe is missing: $requiredRecipeToken"
	}
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
# Run it, do not just read it. Every launcher and every art build starts by
# asking this script where the engine is, so the one thing it must never do is
# fail in a way that is not its own failure. It used to: Join-Path rejects a
# null -Path, and on a host with no ProgramFiles the candidate list threw
# during its own construction -- before the IG_UNREAL_EDITOR override this
# script's error message recommends had been tried at all.
#
# Both outcomes are correct here. A machine with the engine prints its path;
# a machine without it says so. A third outcome -- some other exception -- is
# the bug, and only running it can tell those apart.
#
# The resolver sets its own $ErrorActionPreference = 'Stop', which turns its
# closing Write-Error into a terminating error in this scope, so the missing
# engine arrives as an exception rather than as output. Catch it and read the
# message either way; the distinction being drawn is which message, not how
# it travelled.
$resolverText = ''
try {
	$resolverText = (
		& (Join-Path $projectRoot 'Scripts/Resolve-UnrealEditor.ps1') `
			-ProjectPath $projectFile -Commandlet 2>&1 | Out-String)
}
catch {
	$resolverText = [string]$_.Exception.Message
}
if ($resolverText -notmatch 'UnrealEditor(-Cmd)?\.exe' -and
	$resolverText -notmatch 'was not found\. Install it or set IG_UNREAL_EDITOR') {
	throw (
		'Resolve-UnrealEditor.ps1 failed with something other than its own ' +
		"missing-engine error: $($resolverText.Trim())")
}
$headlessScripts = @(
	'Scripts/Build-ArtAssets.ps1',
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
	'GlobalDefaultGameMode=/Script/IndieGame.IGPrologueGameMode',
	'r.TextureStreaming=True',
	'r.PSOPrecaching=1',
	'r.PSOPrecache.Components=1',
	'r.PSOPrecache.GlobalShaders=1'
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

# 감사들은 못 보는 자리를 스스로 보고한다. 그 숫자가 늘어나는 것은 검사가
# 조용히 눈이 머는 것인데, findings=0은 그대로라 화면에서 구분되지 않는다.
# 지금 값을 천장으로 박아 두고 넘으면 막는다. 줄었으면 천장도 같이 내린다 —
# 낡은 천장은 「여기까지는 못 봐도 된다」로 읽힌다.
function Assert-AuditBlindSpot {
	param(
		[Parameter(Mandatory = $true)][AllowNull()]$Output,
		[Parameter(Mandatory = $true)][string]$Pattern,
		[Parameter(Mandatory = $true)][int]$Ceiling,
		[Parameter(Mandatory = $true)][string]$What
	)
	# 보고 줄이 아예 없으면 못 보는 것이 하나도 없다는 뜻이다.
	$blindMatch = [regex]::Match(($Output | Out-String), $Pattern)
	$blindCount = 0
	if ($blindMatch.Success) {
		$blindCount = [int]$blindMatch.Groups['count'].Value
	}
	if ($blindCount -gt $Ceiling) {
		throw (
			'감사가 못 보는 자리가 늘었다 — {0}: {1}건(천장 {2}건)' -f
				$What, $blindCount, $Ceiling)
	}
	if ($blindCount -lt $Ceiling) {
		throw (
			'감사가 더 많이 보게 됐다 — {0}: {1}건. 천장을 {1}로 내려라(지금 {2})' -f
				$What, $blindCount, $Ceiling)
	}
}

$utf8Strict = New-Object System.Text.UTF8Encoding($false, $true)
$koreanSourceFilesWithoutBom = @(
	# -Include with -LiteralPath is provider-dependent and has admitted binary
	# font files on some PowerShell versions. Filter FileInfo objects explicitly
	# before any byte stream is decoded as UTF-8.
	Get-ChildItem -LiteralPath (Join-Path $projectRoot 'Source') -Recurse -File |
		Where-Object { $_.Extension -in @('.h', '.cpp') } |
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

# 한 파일 안에서 줄바꿈이 섞이면 사람 눈에는 안 보이는데 도구는 걸린다. 문자열
# 바늘이 안 맞아 패치가 조용히 빗나가고, 편집기마다 다른 줄에 커서를 놓는다.
# 저장소에 들어가는 형태는 `.gitattributes`의 `text=auto`가 LF로 맞춰 주기
# 때문에 git diff에는 아무것도 안 뜬다. 작업 트리는 아무도 안 보고 있었다.
#
# 갓 받아 온 클론은 어느 OS에서든 한 가지로 통일돼 있다. 여기서 섞였다는 것은
# 도구가 다른 줄바꿈으로 덧썼다는 뜻이다.
$lineEndingExtensions = @(
	'.h', '.cpp', '.cs', '.ps1', '.py', '.md', '.ini', '.json', '.bat',
	'.txt', '.uproject')
$mixedLineEndingFiles = @(
	@(Get-ChildItem -LiteralPath $projectRoot -File) + @(
		@('Source', 'Scripts', 'Docs', 'Config') | ForEach-Object {
			$searchRoot = Join-Path $projectRoot $_
			if (Test-Path -LiteralPath $searchRoot -PathType Container) {
				Get-ChildItem -LiteralPath $searchRoot -Recurse -File
			}
		}) |
		Where-Object { $_.Extension -in $lineEndingExtensions } |
		ForEach-Object {
			$candidate = $_
			$text = [System.Text.Encoding]::UTF8.GetString(
				[System.IO.File]::ReadAllBytes($candidate.FullName))
			$carriageReturns = [regex]::Matches($text, "`r`n").Count
			$lineFeeds = [regex]::Matches($text, "`n").Count
			if ($carriageReturns -gt 0 -and $lineFeeds -gt $carriageReturns) {
				$candidate.FullName.Substring($projectRoot.Length + 1)
			}
		}
)
if ($mixedLineEndingFiles.Count -gt 0) {
	throw (
		'Line endings are mixed inside these files: {0}' -f
			($mixedLineEndingFiles -join ', '))
}

# 파일만 있고 아무도 안 부르는 계약. M8 난이도(108개 단언)와 REBIRTH 증거
# (113개)가 그랬다 — 통과도 하는데 검증기가 부르질 않아 그냥 안 돌고 있었다.
# 저장소 목록에는 계약이 있고, 화면에서는 「본다」와 구분되지 않는다.
$registeredChecks = Get-Content -Raw -Encoding UTF8 -LiteralPath $PSCommandPath
$unregisteredChecks = @(
	Get-ChildItem -LiteralPath (Join-Path $projectRoot 'Scripts') -File |
		Where-Object {
			($_.Name -like 'Test-*.ps1') -or ($_.Name -like 'audit_*.py')
		} |
		Where-Object { -not $registeredChecks.Contains($_.Name) } |
		ForEach-Object { $_.Name })
if ($unregisteredChecks.Count -gt 0) {
	throw (
		'검증기가 부르지 않는 검사가 있다: {0}' -f
			($unregisteredChecks -join ', '))
}

# 그레이박스 셋업은 액터를 열다섯 개 순서대로 세우고 중간 어디서든 false로
# 빠진다. 빠지면 0.3초 뒤 처음부터 다시 도는데, 스폰에 이름을 지정하므로
# 만들다 만 액터가 살아 있으면 같은 이름 때문에 다음 스폰이 실패한다. 그러면
# 재시도가 영영 통과하지 못하고 6초 뒤 「월드 씬이나 플레이어가 없다」로
# 끝난다 — 실제 이유와 다른 말이다.
#
# 그래서 세우는 목록과 치우는 목록이 같아야 한다. 새 디렉터를 하나 더 세우면
# 여기서 걸린다.
$greyboxStageSource = Get-Content -Raw -Encoding UTF8 -LiteralPath (
	Join-Path $projectRoot 'Source/IndieGame/Entity/IGListenerGreyboxDirector.cpp')
$stageSetupBody = [regex]::Match(
	$greyboxStageSource,
	'bool AIGListenerGreyboxDirector::SetupStage\(\)\r?\n\{(?<body>[\s\S]*?)\r?\n\}')
$stageTeardownBody = [regex]::Match(
	$greyboxStageSource,
	'void AIGListenerGreyboxDirector::DestroyPartialStage\(\)\r?\n\{(?<body>[\s\S]*?)\r?\n\}')
if (-not $stageSetupBody.Success -or -not $stageTeardownBody.Success) {
	throw 'The greybox stage setup or its teardown could not be read.'
}
if ($stageSetupBody.Groups['body'].Value -notmatch 'DestroyPartialStage\(\);') {
	throw 'The greybox stage setup no longer clears what a failed attempt left behind.'
}
# 셋업이 직접 세우는 것과, 셋업이 부르는 헬퍼가 세우는 것을 함께 센다.
# 증인 다섯은 지금 마지막 실패 경로보다 뒤에 있지만, 그 사이에 실패가 하나
# 생기면 이름이 살아남아 재시도를 막는다.
# 셋업이 부르는 스폰 헬퍼는 셋이다. 셋업 본문만 보면 이 열둘을 놓친다. 밤 베드는
# 액터가 아니라 이름 붙인 컴포넌트라 teardown이 DestroyComponent로 걷는다.
$stageHelperBodies = ''
foreach ($stageHelperName in @('SpawnOptionalWitnesses', 'SpawnArrivalInteractables', 'SpawnNightAmbienceBeds')) {
	$stageHelperBody = [regex]::Match(
		$greyboxStageSource,
		'void AIGListenerGreyboxDirector::' + $stageHelperName +
			'\([^)]*\)\r?\n\{(?<body>[\s\S]*?)\r?\n\}')
	if (-not $stageHelperBody.Success) {
		throw "The greybox spawner $stageHelperName could not be read."
	}
	$stageHelperBodies += $stageHelperBody.Groups['body'].Value
}
# 셋을 넘어 더 부르기 시작하면 위 목록으로는 부족해진다.
$stageHelperCalls = @(
	[regex]::Matches(
		$stageSetupBody.Groups['body'].Value,
		'(?m)^\s*(?<name>Spawn[A-Za-z0-9_]*)\(') |
		ForEach-Object { $_.Groups['name'].Value } |
		Sort-Object -Unique)
foreach ($stageHelper in $stageHelperCalls) {
	if ($stageHelper -notin @('SpawnOptionalWitnesses', 'SpawnArrivalInteractables', 'SpawnNightAmbienceBeds')) {
		throw (
			'The greybox stage setup calls {0}; the teardown audit does not follow it.' -f
				$stageHelper)
	}
}
# 람다로 세우는 것(SpawnEvidence)도 같은 이름 규칙을 쓰므로 함께 센다.
$stageSpawned = @(
	[regex]::Matches(
		$stageSetupBody.Groups['body'].Value + $stageHelperBodies,
		'(?m)^\s*(?<name>[A-Za-z_][A-Za-z0-9_]*) = (?:World->SpawnActor<|SpawnEvidence\()') |
		ForEach-Object { $_.Groups['name'].Value } |
		Sort-Object -Unique)
if ($stageSpawned.Count -lt 27) {
	throw (
		'The greybox stage builds {0} actors; twenty-seven were authored.' -f
			$stageSpawned.Count)
}
foreach ($stageActor in $stageSpawned) {
	if ($stageTeardownBody.Groups['body'].Value -notmatch
		('(?m)^\s*' + [regex]::Escape($stageActor) + ' = nullptr;')) {
		throw (
			'The greybox stage builds {0} but never clears it; a failed attempt would block the retry.' -f
				$stageActor)
	}
}

$tickingActors = Get-ChildItem -LiteralPath (Join-Path $projectRoot 'Source') -Recurse -Include '*.h','*.cpp' |
    Select-String -Pattern 'PrimaryActorTick\.bCanEverTick\s*=\s*true'
# Reviewed exceptions. Every entry sets bStartWithTickEnabled = false; the
# first group enables Tick only for bounded animation/presentation windows and
# switches it off again. IGPlayerController ticks during the 10-second display
# confirmation and the bounded -IGFrontendShippingProbe process only.
# IGDemoDirector is a development-only capture driver that is spawned solely
# under -IGCapture / -IGDemo / -IGDemoFrames.
# IGMissingFloorNightFourDirector starts disabled and wakes only while the
# final reveal, Mok retreat, ending prop movement, or the two camera-dependent
# detail layers are visible. It disables itself as soon as those presentation
# windows close.
# IGNightLoopDirector는 기본 Tick을 끄고, 5회 포획 후 종이가 미끄러지는
# 0.82초에만 켠 뒤 다시 타이머 기반 리셋 처리로 돌아간다.
# IGMissingFloorFifthDawnDirector는 기본 Tick을 끄고, 재관람 스킵을 누르는
# 동안과 중도 해제 후 진행률을 되감는 짧은 구간에만 켠다. 완료 또는
# 되감기 종료 즉시 스스로 비활성화하며 평상시 비용은 발생하지 않는다.
# IGMissingFloorEpilogueDirector는 같은 이유로 같은 방식이다. 87초 시각표는
# 타이머가 밀고, Tick은 재관람 우회를 누르는 동안과 중도 해제 뒤 진행률을
# 되감는 구간에만 켜진다. 마지막 카드로 건너뛰거나 되감기가 끝나면 스스로
# 비활성화한다.
# IGMissingFloorMercyDirector는 기본 Tick을 끄고, 문 아래로 종이가 밀려
# 들어오는 0.94초 동안만 켠 뒤 스스로 끈다. 90초 정체 시계는 타이머다.
# IGCctvChannelFive는 기본 Tick을 끄고, 채널 5가 화면에 있는 6.78초 동안만
# 켠다. 그 Tick이 초당 12회 씬 캡처와 낮은 형체의 이동을 구동하며, 채널이
# 죽는 프레임에 렌더타깃·캡처·형체를 해제하고 자신을 비활성화한다. 이것이
# §14의 상시 렌더 금지를 만족시키는 방식이므로 타이머로 대체할 수 없다.
#
# IGListenerEntity is the one deliberate always-on actor tick in the project.
# 위층 사람 is a pursuer: its state machine, crawl locomotion, drag-loop gain
# and threat pressure are per-frame concerns for its whole life, exactly like
# the player pawn's. It exists only while a night stage is armed
# (-IGListenerGreybox spawns it), it owns no timers that could substitute for
# the tick, and gating it would make the pursuit visibly step.
$reviewedTickingFiles = @(
	# 전용 실행 인자에서만 생성하고 약 3초 뒤 종료한다. 실제 입력 제동 거리를 잰다.
	'IGGameplayRealismProbe.cpp',
	'IGWakeUpDirector.cpp',
	'IGPlayerCharacter.cpp',
	'IGPlayerController.cpp',
	'IGFridge.cpp',
	'IGSwingDoor.cpp',
	'IGSlidingDoor.cpp',
	'IGElevator.cpp',
	'IGNeighborhoodLifeDirector.cpp',
	'IGDemoDirector.cpp',
	'IGListenerEntity.cpp',
	'IGMissingFloorNightFourDirector.cpp',
	'IGNightLoopDirector.cpp',
	'IGMissingFloorFifthDawnDirector.cpp',
	'IGMissingFloorEpilogueDirector.cpp',
	'IGMissingFloorMercyDirector.cpp',
	'IGCctvChannelFive.cpp'
)
$unreviewedTickingActors = @($tickingActors | Where-Object {
	$reviewedTickingFiles -notcontains [System.IO.Path]::GetFileName($_.Path)
})
if ($unreviewedTickingActors.Count -gt 0) {
    $locations = $unreviewedTickingActors | ForEach-Object { "$($_.Path):$($_.LineNumber)" }
    throw "Actor Tick requires an explicit architecture review: $($locations -join ', ')"
}
$alwaysTickingComponents = @(Get-ChildItem `
	-LiteralPath (Join-Path $projectRoot 'Source') `
	-Recurse `
	-Include '*.h','*.cpp' |
	Select-String -Pattern 'PrimaryComponentTick\.bStartWithTickEnabled\s*=\s*true')
if ($alwaysTickingComponents.Count -gt 0) {
	$locations = $alwaysTickingComponents | ForEach-Object {
		"$($_.Path):$($_.LineNumber)"
	}
	throw (
		'Component Tick must start disabled and wake from explicit state: ' +
		($locations -join ', '))
}

$attributesFile = Join-Path $projectRoot '.gitattributes'
$attributes = Get-Content -Raw -LiteralPath $attributesFile
foreach ($extension in @('*.uasset', '*.umap', '*.png', '*.gif', '*.zip', '*.bin')) {
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
	'Scripts/run_rebirth_savegame_roundtrips.py')
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
$lensDropletCaptureScriptPath = Join-Path $projectRoot (
	'Scripts/Run-Rebirth-LensDropletCapture.ps1')
$lensDropletCaptureScript = Get-Content -Raw -Encoding UTF8 -LiteralPath (
	$lensDropletCaptureScriptPath)
$frontendShippingProbeScriptPath = Join-Path $projectRoot (
	'Scripts/Run-Rebirth-FrontendShippingProbe.ps1')
$frontendShippingProbeScript = Get-Content -Raw -Encoding UTF8 -LiteralPath (
	$frontendShippingProbeScriptPath)
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
$playerControllerSource = Get-Content -Raw -Encoding UTF8 -LiteralPath (
	Join-Path $projectRoot 'Source/IndieGame/Player/IGPlayerController.cpp')
$playerCharacterSource = Get-Content -Raw -Encoding UTF8 -LiteralPath (
	Join-Path $projectRoot 'Source/IndieGame/Player/IGPlayerCharacter.cpp')
$flashlightHeader = Get-Content -Raw -Encoding UTF8 -LiteralPath (
	Join-Path $projectRoot 'Source/IndieGame/Player/IGFlashlightComponent.h')
$flashlightSource = Get-Content -Raw -Encoding UTF8 -LiteralPath (
	Join-Path $projectRoot 'Source/IndieGame/Player/IGFlashlightComponent.cpp')
$stressSource = Get-Content -Raw -Encoding UTF8 -LiteralPath (
	Join-Path $projectRoot 'Source/IndieGame/Player/IGStressComponent.cpp')
$morningDirectorSource = Get-Content -Raw -Encoding UTF8 -LiteralPath (
	Join-Path $projectRoot 'Source/IndieGame/Sequence/IGMorningRoutineDirector.cpp')
$pickupItemSource = Get-Content -Raw -Encoding UTF8 -LiteralPath (
	Join-Path $projectRoot 'Source/IndieGame/Interaction/IGPickupItem.cpp')
$checkoutSource = Get-Content -Raw -Encoding UTF8 -LiteralPath (
	Join-Path $projectRoot 'Source/IndieGame/Interaction/IGCheckoutCounter.cpp')
$demoDirectorSource = Get-Content -Raw -Encoding UTF8 -LiteralPath (
	Join-Path $projectRoot 'Source/IndieGame/Sequence/IGDemoDirector.cpp')
$prologueCaptureScript = Get-Content -Raw -Encoding UTF8 -LiteralPath (
	Join-Path $projectRoot 'Scripts/Run-Prologue-Capture.ps1')
$elevatorSource = Get-Content -Raw -Encoding UTF8 -LiteralPath (
	Join-Path $projectRoot 'Source/IndieGame/Interaction/IGElevator.cpp')
$swingDoorSource = Get-Content -Raw -Encoding UTF8 -LiteralPath (
	Join-Path $projectRoot 'Source/IndieGame/Interaction/IGSwingDoor.cpp')
$slidingDoorSource = Get-Content -Raw -Encoding UTF8 -LiteralPath (
	Join-Path $projectRoot 'Source/IndieGame/Interaction/IGSlidingDoor.cpp')
$neighborhoodSource = Get-Content -Raw -Encoding UTF8 -LiteralPath (
	Join-Path $projectRoot 'Source/IndieGame/Environment/IGNeighborhoodLifeDirector.cpp')
$surfaceMaterialSource = Get-Content -Raw -Encoding UTF8 -LiteralPath (
	Join-Path $projectRoot 'Scripts/create_textured_materials.py')
$surfaceAuditSource = Get-Content -Raw -Encoding UTF8 -LiteralPath (
	Join-Path $projectRoot 'Scripts/validate_baked_art_assets.py')
$artBuildSource = Get-Content -Raw -Encoding UTF8 -LiteralPath (
	Join-Path $projectRoot 'Scripts/Build-ArtAssets.ps1')

foreach ($surfaceResponseInvariant in @(
	'SURFACE_RESPONSE_DEFAULTS',
	'"macro_strength":',
	'"detail_normal_strength":',
	'"roughness_variation":',
	'"roughness_detail_strength":',
	'def _texture_exists(assets, name):',
	'SURFACE_RESPONSE_MARKER = "IG_SurfaceResponse_v1"',
	'unreal.MaterialProperty.MP_SPECULAR',
	'IG_SURFACE_RESPONSE_ONLY'
)) {
	if (-not $surfaceMaterialSource.Contains($surfaceResponseInvariant)) {
		throw "Layered surface-response invariant is missing: $surfaceResponseInvariant"
	}
}
$saveGameRoundTripText = Get-Content -Raw -Encoding UTF8 -LiteralPath (
	Join-Path $projectRoot 'Scripts/run_rebirth_savegame_roundtrips.py')
foreach ($saveGameRoundTripInvariant in @(
	'UnrealEditor-Cmd.exe',
	'-nullrhi',
	'-nosound',
	'-RenderOffscreen'
)) {
	if (-not $saveGameRoundTripText.Contains($saveGameRoundTripInvariant)) {
		throw (
			'Save-game round-trip headless invariant is missing: ' +
			$saveGameRoundTripInvariant)
	}
}
foreach ($surfaceAuditInvariant in @(
	# Open paren only. This asserts the audit exists, not what it takes: the
	# closed form went stale the moment a material_specs parameter was added
	# for the targeted apartment build, and the gate then failed on a function
	# that was sitting right there.
	'def validate_surface_response_materials(',
	'Macro colour blend is missing',
	'Detail-normal blend is missing',
	'Roughness variation is missing',
	'pixel_samples <= 7'
)) {
	if (-not $surfaceAuditSource.Contains($surfaceAuditInvariant)) {
		throw "Surface-response UAsset audit is missing: $surfaceAuditInvariant"
	}
}
foreach ($surfaceBuildInvariant in @(
	'[switch]$SurfaceResponseOnly',
	'IG_SURFACE_RESPONSE_ONLY',
	'Surface response material update complete'
)) {
	if (-not $artBuildSource.Contains($surfaceBuildInvariant)) {
		throw "Targeted surface-response build is missing: $surfaceBuildInvariant"
	}
}
foreach ($propResponseInvariant in @(
	'PRINT_RESPONSE_MARKER = "IG_PrintResponse_v1"',
	'OPTICAL_RESPONSE_MARKER = "IG_OpticalResponse_v1"',
	'WET_GROUND_RESPONSE_MARKER = "IG_WetGroundResponse_v1"',
	'def _connect_print_response(material, base_sample, spec):',
	'def create_optical_prop_materials(assets, tools, update_in_place=False):',
	'"micro_stem": "T_PaperClean_V2"',
	'"micro_stem": "T_CarrierBagFilm"',
	'IG_PROP_RESPONSE_ONLY'
)) {
	if (-not $surfaceMaterialSource.Contains($propResponseInvariant)) {
		throw "Prop-response material invariant is missing: $propResponseInvariant"
	}
}
foreach ($propAuditInvariant in @(
	'def validate_prop_response_materials()',
	'Compiled print sample budget exceeded',
	'Compiled optical-prop sample budget exceeded',
	'Compiled wet-ground sample budget exceeded'
)) {
	if (-not $surfaceAuditSource.Contains($propAuditInvariant)) {
		throw "Prop-response UAsset audit is missing: $propAuditInvariant"
	}
}
foreach ($propBuildInvariant in @(
	'[switch]$PropResponseOnly',
	'IG_PROP_RESPONSE_ONLY',
	'Prop response material update complete'
)) {
	if (-not $artBuildSource.Contains($propBuildInvariant)) {
		throw "Targeted prop-response build is missing: $propBuildInvariant"
	}
}
foreach ($surfaceLightingInvariant in @(
	'PostProcess->Settings.LocalExposureDetailStrength = 1.12f;',
	'PostProcess->Settings.FilmSlope = 0.90f;',
	'PostProcess->Settings.AmbientOcclusionIntensity = 0.48f;',
	'PostProcess->Settings.LumenAmbientOcclusionIntensity = 0.55f;',
	'Light->ContactShadowLength = 0.0f;',
	'Light->SetSpecularScale(1.0f);'
)) {
	if (-not $worldSceneSource.Contains($surfaceLightingInvariant)) {
		throw "Surface-lighting response invariant is missing: $surfaceLightingInvariant"
	}
}

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
	'FVector(-140, -214.81f, 154)',
	'PropMesh(TEXT("SM_ApartmentCalendar2025"))',
	'FVector(-392.5f, -385, 80), FVector(125, 20, 340)',
	'FVector(-392.5f, -225, 80), FVector(125, 20, 340)',
	'FVector(-455, -305, 80), FVector(20, 200, 340)',
	'FVector(-392.5f, -305, 250), FVector(125, 160, 20)',
	'FVector(800, -394, 620), FVector(160, 6, 1240)',
	'FVector(1015, -385, 230), FVector(270, 20, 460)',
	'TexMat(TEXT("M_StainlessUV"), FridgeBodyMaterial)',
	'CabVisuals.DiffuserMaterial = SignWhiteMaterial',
	'TexMat(TEXT("M_SteelDoorUV"), FridgeBodyMaterial)'
)) {
	if (-not $worldSceneSource.Contains($worldContinuityInvariant)) {
		throw "World spatial-continuity invariant is missing: $worldContinuityInvariant"
	}
}
if ($engineConfig -match '(?m)^r\.VolumetricFog\s*=') {
	throw (
		'r.VolumetricFog must remain scalability-owned; a project-level value ' +
		'prevents Low ShadowQuality from disabling its 3D volume.')
}
$saltDepositStart = $worldSceneSource.IndexOf('struct FSaltDepositSpec')
$saltDepositEnd = if ($saltDepositStart -ge 0) {
	$worldSceneSource.IndexOf(
		'for (const FSaltDepositSpec& Deposit : SaltDeposits)',
		$saltDepositStart)
}
else {
	-1
}
if ($saltDepositStart -lt 0 -or $saltDepositEnd -le $saltDepositStart) {
	throw 'CH02 swept-salt deposit block is malformed.'
}
$saltDepositBlock = $worldSceneSource.Substring(
	$saltDepositStart,
	$saltDepositEnd - $saltDepositStart)
if ([regex]::Matches($saltDepositBlock, 'FVector2D\(').Count -ne 8 -or
	-not $worldSceneSource.Contains('FVector(Deposit.Position.X, Deposit.Position.Y, 0.28f)')) {
	throw 'CH02 swept salt must remain eight low, irregular neutral deposits.'
}
$saltRenderingEnd = $worldSceneSource.IndexOf(
	'// Only the dry circular trace of the removed rice bowl remains.',
	$saltDepositEnd)
if ($saltRenderingEnd -le $saltDepositEnd) {
	throw 'CH02 swept-salt rendering block is malformed.'
}
$saltRenderingBlock = $worldSceneSource.Substring(
	$saltDepositStart,
	$saltRenderingEnd - $saltDepositStart)
if ($saltRenderingBlock.Contains('SignWhiteMaterial') -or
	$saltRenderingBlock.Contains('WaterBlueMaterial')) {
	throw 'CH02 swept salt must use the restrained neutral material contract.'
}
if ($worldSceneSource.Contains('FVector(-187.2f, -60, 152)')) {
	throw 'The calendar must not regress behind the wardrobe and bedside table.'
}
if (-not $demoDirectorSource.Contains('aborting spatial-continuity capture') -or
	$demoDirectorSource.Contains('snapping to waypoint')) {
	throw 'Demo capture must fail on a blocked threshold instead of teleporting through it.'
}
foreach ($requiredCaptureHarnessToken in @(
	'-IGCapture',
	'-RenderOffscreen',
	'-d3d12',
	'AddMinutes(10)',
	"Get-Process -Name 'UnrealEditor'",
	'Demo walkthrough complete; exiting.',
	'prologue-bedroom.png',
	'prologue-kitchen.png',
	'prologue-not-found-note.png',
	'prologue-corridor.png',
	'prologue-elevator.png',
	'prologue-lobby.png',
	'prologue-villa.png',
	'prologue-alley.png',
	'prologue-ramyeon.png',
	'prologue-store.png'
)) {
	if (-not $prologueCaptureScript.Contains($requiredCaptureHarnessToken)) {
		throw "Prologue capture harness is missing: $requiredCaptureHarnessToken"
	}
}
foreach ($elevatorLightingInvariant in @(
	'CabLight->SetIntensity(520.0f)',
	'CabLight->SetAttenuationRadius(300.0f)',
	'CabLight->SetLightColor(FLinearColor(0.84f, 0.91f, 1.0f))',
	'FloorFill->SetIntensity(36.0f)'
)) {
	if (-not $elevatorSource.Contains($elevatorLightingInvariant)) {
		throw "Elevator anti-clipping lighting invariant is missing: $elevatorLightingInvariant"
	}
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
foreach ($requiredIdleTickInvariant in @(
	'PrimaryComponentTick.bStartWithTickEnabled = false',
	'SetComponentTickEnabled(true)',
	'SetComponentTickEnabled(false)'
)) {
	if (-not $flashlightSource.Contains($requiredIdleTickInvariant)) {
		throw "Flashlight idle-Tick invariant is missing: $requiredIdleTickInvariant"
	}
}
foreach ($requiredStressTickInvariant in @(
	'PrimaryComponentTick.bStartWithTickEnabled = false',
	'void UIGStressComponent::RefreshTickState()',
	'HeartbeatSuppressionRemaining > KINDA_SMALL_NUMBER',
	'SetComponentTickEnabled(bNeedsTick)'
)) {
	if (-not $stressSource.Contains($requiredStressTickInvariant)) {
		throw "Stress idle-Tick invariant is missing: $requiredStressTickInvariant"
	}
}
foreach ($requiredStorePerformanceInvariant in @(
	'Components/InstancedStaticMeshComponent.h',
	'ExpectedStoreStockInstances = 1318',
	'TEXT("SM_RetailCupBeef")',
	'TEXT("M_RetailPriceCupBeef")',
	'MaximumStoreStockBatches = 28',
	'StoreStockCullStartCentimeters = 1600',
	'StoreStockCullEndCentimeters = 2200',
	'Batch->SetAffectDistanceFieldLighting(false)',
	'Batch->SetCollisionEnabled(ECollisionEnabled::NoCollision)',
	'FinalizeStoreStockBatches();',
	'REBIRTH_RELEASE PASS store_instancing instances=%d batches=%d'
)) {
	if (-not $worldSceneSource.Contains($requiredStorePerformanceInvariant)) {
		throw "Store runtime-performance invariant is missing: $requiredStorePerformanceInvariant"
	}
}
foreach ($movablePresentationPattern in @(
	'P4ReceiptFragment\s*=\s*CreateBlock\([\s\S]{0,260}nullptr,\s*true\);',
	'InspectionRodVisual\s*=\s*CreateBlock\([\s\S]{0,360}CylinderMesh\.Get\(\),\s*true\);'
)) {
	if ($thirdMorningSource -notmatch $movablePresentationPattern) {
		throw (
			'CH03 presentation changes a Static component transform: ' +
			$movablePresentationPattern)
	}
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
	'RequestedSamplesPerTrack = 4096',
	'TrackNonZeroSamples > 32',
	'TrackClippedSamples == 0',
	'ExpectedM5DurationSeconds = 45.05f',
	'REBIRTH_RELEASE PASS audio_synthesis',
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
	'FVector(1355, -340.0f, 170)',
	'FVector(1355, -311, 290)',
	'"The fifth-floor landing must preserve the 160 cm doorway route."',
	# 398.5, not 397: the fixture is 3 cm deep, so its top sat at 398.5 while
	# the landing ceiling slab -- FVector(1355, -340.0f, 410) by (290, 76, 20)
	# -- undersides at 400. It hung 1.5 cm clear of the ceiling it is screwed
	# to. What this line guards is that the maintenance fixture is still at the
	# landing, not the gap that used to be under it.
	'FVector(1415, -420, 398.5f)',
	'FVector(1415, -420, 386)',
	'FVector(1570, -420, 305)',
	'1850.0f',
	'4600.0f',
	'HangingVinyl = CreateBlock(',
	'FVector(1325, HangingVinylWallY, 300)',
	'constexpr float HangingVinylMaximumRouteReach = 12.0f',
	'"The moving vinyl must not visually seal the landing route."',
	'UpdateHangingVinylSway()',
	'&ThisClass::UpdateHangingVinylSway',
	'TEXT("ch03-fifth-floor-doorway")',
	'visual_stills=5',
	'FVector(470, 147, 20)',
	'FVector(1450, -502.5f, 205)',
	'FVector(1450, -297.5f, 205)',
	'FVector(1450, -400, 400)',
	'constexpr float UpperFlightRightWallCenterY = -200.0f',
	'constexpr float UpperFlightRightWallDepth = 260.0f',
	'constexpr float RequiredWallClearance = 20.0f',
	'"The upper-flight wall must leave a capsule-safe turn at the landing."',
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
	'CreateBlock(FVector(1210, -235, 195), FVector(20, 330, 390)',
	'FVector(1390, -335, 300)',
	'FVector(3, 155, 210)',
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
	'Tank + FVector(-149, -28, 574)'
)) {
	if (-not $thirdMorningSource.Contains($requiredChapterThreeEvidenceInvariant)) {
		throw "Required CH03 evidence invariant is missing: $requiredChapterThreeEvidenceInvariant"
	}
}
foreach ($requiredTankRevealInvariant in @(
	'AuthoredTankBodyPlacement(-88.0f, 0.0f, 542.0f)',
	'AuthoredTankBodyRotation(0.0f, 70.0f, 0.0f)',
	'AuthoredSleeveStitchLocalBase(8.0f, -27.0f, 15.2f)',
	'GetAuthoredSleeveStitchFocusOffset()',
	'Tank + FVector(-151.0f, 45.0f, 580.0f)',
	'FRotator(90.0f, 0.0f, 0.0f)',
	'const FVector ClothingEvidenceOffset = bUsesAuthoredTankBody',
	'Tank + ClothingEvidenceOffset',
	'FVector(34, 28, 18)'
)) {
	if (-not $thirdMorningSource.Contains($requiredTankRevealInvariant)) {
		throw "CH03 tank-reveal alignment invariant is missing: $requiredTankRevealInvariant"
	}
}
if ($thirdMorningSource.Contains('Tank + FVector(-10, -30, 540)')) {
	throw 'CH03 clothing evidence focus regressed to its pre-body-alignment position.'
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
$lensDropletCaptureTokens = $null
$lensDropletCaptureParseErrors = $null
[void][System.Management.Automation.Language.Parser]::ParseFile(
	$lensDropletCaptureScriptPath,
	[ref]$lensDropletCaptureTokens,
	[ref]$lensDropletCaptureParseErrors)
if ($lensDropletCaptureParseErrors.Count -gt 0) {
	$parseMessages = $lensDropletCaptureParseErrors |
		ForEach-Object { $_.Message }
	throw "REBIRTH lens-droplet capture script does not parse: $($parseMessages -join '; ')"
}
$frontendShippingProbeTokens = $null
$frontendShippingProbeParseErrors = $null
[void][System.Management.Automation.Language.Parser]::ParseFile(
	$frontendShippingProbeScriptPath,
	[ref]$frontendShippingProbeTokens,
	[ref]$frontendShippingProbeParseErrors)
if ($frontendShippingProbeParseErrors.Count -gt 0) {
	$parseMessages = $frontendShippingProbeParseErrors |
		ForEach-Object { $_.Message }
	throw "REBIRTH frontend Shipping probe script does not parse: $($parseMessages -join '; ')"
}
foreach ($requiredLensCaptureHarnessInvariant in @(
	"'-IGCaptureCH03LensDroplet'",
	"'-IGReducedMotion'",
	"'-RenderOffscreen'",
	"'-d3d12'",
	'-WindowStyle Hidden',
	"'Windows\IndieGame.exe'",
	"'Windows\IndieGame\Binaries\Win64\IndieGame-Win64-Shipping.exe'",
	'ArchiveDirectory must be the BuildCookRun archive root containing',
	'Get-PngDimensions',
	'Assert-ArchiveUnchanged',
	'REBIRTH_CH03_LENS_CAPTURE PASS',
	'REBIRTH_LENS_CAPTURE_HARNESS PASS modes=2 stills=4',
	'$Mode -eq ''default'' -and $travelY -lt 8.0',
	'$Mode -eq ''reduced'' -and $travelY -gt 0.5',
	'$minimumAlpha -lt 0.55',
	'$canvasWidth -ne 1280 -or $canvasHeight -ne 720'
)) {
	if (-not $lensDropletCaptureScript.Contains(
			$requiredLensCaptureHarnessInvariant)) {
		throw (
			'REBIRTH lens-droplet capture invariant is missing: ' +
			$requiredLensCaptureHarnessInvariant)
	}
}
foreach ($requiredFrontendShippingHarnessInvariant in @(
	"'-IGFrontendShippingProbe'",
	'"-IGFrontendExpectedWidth=$Width"',
	'"-IGFrontendExpectedHeight=$Height"',
	'"-IGFrontendResultPath=$receiptPath"',
	'"-IGFrontendAccessibilityScreenshotPath=$accessibilityScreenshotPath"',
	'"-IGFrontendDisplayScreenshotPath=$displayScreenshotPath"',
	'"-IGFrontendTitleScreenshotPath=$titleScreenshotPath"',
	'"-IGFrontendDefaultScreenshotPath=$defaultScreenshotPath"',
	'"-IGFrontendScreenshotPath=$screenshotPath"',
	"'-RenderOffscreen'",
	"'-d3d12'",
	'-WindowStyle Hidden',
	'-FilePath $launcher',
	'-WorkingDirectory (Split-Path -Parent $launcher)',
	'1280; height = 720',
	'1600; height = 900',
	'1920; height = 1080',
	'2560; height = 1440',
	'Assert-ArchiveUnchanged',
	'Get-PngDimensions',
	'dialogueScreenshotSha256',
	'accessibilityScreenshotSha256',
	'displayScreenshotSha256',
	'titleScreenshotSha256',
	'defaultDialogueScreenshotSha256',
	'keyboard_access=1 gamepad_access=1 dpad_down=1',
	'gamepad_pause=1 display=1 title=1 first_run=1',
	'dialogue=1 dialogue_default=1',
	'speaker=1 continuation=1 default_scale=100 max_scale=200',
	'sound_lane=1 samples=11 elements_min=',
	'input_events=11 bounds=',
	'REBIRTH_FRONTEND_SHIPPING PASS resolutions=4 input_events=44',
	'layout_samples=44 dialogue_cases=8 title_cases=4',
	'schemaVersion = 4',
	'titleCaseCount = $results.Count',
	'archiveUnchanged = $true',
	'layoutSampleCount'
)) {
	if (-not $frontendShippingProbeScript.Contains(
		$requiredFrontendShippingHarnessInvariant)) {
		throw (
			'REBIRTH frontend Shipping harness invariant is missing: ' +
			$requiredFrontendShippingHarnessInvariant)
	}
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
	'function Invoke-RebirthShippingRuntimeCase',
	"'shipping_runtime_ending_a'",
	"'shipping_runtime_ending_b'",
	'"-IGRebirthResultPath=$resultPath"',
	'"-UserDir=$userDirectory"',
	'$resultText -cne $expectedResult',
	'Invoke-RebirthShippingRuntimeCase',
	'shippingRuntimeResults = [pscustomobject]$shippingRuntimeResults',
	"'shipping_persistence_spikes'",
	"'shipping_frontend_input_hud'",
	'Packaged Shipping process-boundary persistence spikes',
	'Packaged Shipping input and HUD layout probe',
	"'-ArchiveDirectory'",
	'$frontendShippingProbeScript',
	'Assert-FrontendShippingProbeEvidence',
	'REBIRTH_FRONTEND_SHIPPING PASS resolutions=4 input_events=44',
	'$summaryResults.Count -ne 4',
	'$result.layoutSamples -ne 11',
	"`$engineBuildVersion -ceq '5.8.0-55116800'",
	'FDataflowToolNodeSnapshot::Date is not initialized properly',
	'1 Uninitialized script struct members found including 0 object properties',
	'known UE 5.8.0 DataflowNodes Date startup diagnostics count=2',
	'Frontend Shipping executable hash mismatch',
	'[regex]::Escape($resolution)',
	'Frontend Shipping receipt evidence mismatch',
	'shippingPersistenceSummary = $shippingPersistenceSummaryPath',
	'shippingPersistenceSummarySha256 = $shippingPersistenceSummaryHash',
	'shippingFrontendSummary = $shippingFrontendSummaryPath',
	'shippingFrontendSummarySha256 = $shippingFrontendSummaryHash',
	'$packagedShipping',
	"'REBIRTH_SPIKE PASS '",
	'function Assert-ShippingArchiveManifestUnchanged',
	"'shipping_archive_post_runtime'",
	'Shipping manifest path escaped the archive',
	'Shipping archive file hash changed after runtime',
	'Assert-ShippingArchiveManifestUnchanged',
	'if (-not $SkipShippingPackage -and -not $SkipRuntimeValidation)',
	'[switch]$SkipEditorRuntimeValidation',
	'$skipEditorRuntimeValidation',
	'SkipEditorRuntimeValidation requested; packaged Shipping runtime remains enabled.',
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
	'REBIRTH_RELEASE PASS store_instancing instances=1318',
	'REBIRTH_RELEASE PASS audio_synthesis tracks=19 invalid=0 clipped=0',
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
	'AllowUE58UnifiedErrorStartupNoise',
	'LogTemp: Error test: UE::UnifiedErrorTest::Empty:',
	'LogTemp: Error with param: UE::UnifiedErrorTest::WithInt:',
	'LogTemp: Error with context: UE::UnifiedErrorTest::Empty:',
	'LogTemp: FError that has been invalidated:',
	'LogTemp: FError that has been moved from:',
	'$conditionIndexes.Count -eq 15',
	'scope=map_check',
	'Unapproved Unreal Ensure/Error/Fatal diagnostic',
	'Get-AsciiReleaseBuildRoot',
	'Invoke-ReleaseRobocopy',
	'$buildProjectFile',
	'usingAsciiBuildMirror',
	'Get-VcRuntimePrerequisite',
	"'vc_runtime_prerequisite'",
	"[Version]'14.50.35719.0'",
	'VC++ runtime prerequisite was blocked.',
	'Shipping AppLocal CRT source was blocked.',
	'vcRuntimePrerequisite = $vcRuntimePrerequisite',
	'appLocalDirectory = $appLocalDirectory',
	'appLocalVersions = [pscustomobject]$appLocalVersions',
	'appLocalValid = $appLocalValid',
	'schemaVersion = 3',
	'unrealDiagnosticAllowlist = @($unrealDiagnosticAllowlist)',
	'Add-MissingStepResults',
	'automatedReleaseCandidateEligible',
	'releaseEligible = $false',
	'ShippingArchiveManifest.json',
	'Test-Windows-ExecutableIcon.ps1',
	'WINDOWS_EXECUTABLE_ICON PASS size=32 matched_pixels=1024',
	'Copy-Windows-ExecutableVersionResource.ps1',
	'Test-Windows-ExecutableMetadata.ps1',
	'WINDOWS_EXECUTABLE_METADATA_SYNC PASS',
	'WINDOWS_EXECUTABLE_METADATA PASS',
	"'-applocaldirectory=`$(EngineDir)/Binaries/ThirdParty/AppLocalDependencies'",
	"@('msvcp140_2.dll', 'vcruntime140_1.dll')",
	'appLocalRuntime = [pscustomobject]@{',
	'ShippingExecutableMetadataSync.log',
	'ShippingExecutableMetadata.log',
	'executableMetadata = [pscustomobject]@{',
	'shippingExecutableMetadataLogSha256',
	'ShippingApplicationIcon.png',
	'ShippingApplicationIcon.log',
	'applicationIcon = [pscustomobject]@{',
	'shippingApplicationIconEvidenceSha256',
	'shippingApplicationIconLog',
	'Shipping archive directory must be absent or empty',
	"Name -ieq 'IndieGame.exe'",
	"Name -ieq 'IndieGame-Win64-Shipping.exe'",
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
	"Get-Command 'pwsh.exe'",
	'PowerShell 7 is required for the UTF-8 Korean release contracts.',
	'-FilePath $powerShellHost'
)) {
	if (-not $releaseValidationScript.Contains(
			$requiredReleaseValidationInvariant)) {
		throw "REBIRTH release harness invariant is missing: $requiredReleaseValidationInvariant"
	}
}
if ($releaseValidationScript.Contains("-FilePath 'powershell.exe'") -or
	$releaseValidationScript.Contains('& powershell.exe')) {
	throw 'Release harness must not decode Korean UTF-8 contracts with Windows PowerShell 5.1.'
}
$shippingPackageGateIndex = $releaseValidationScript.LastIndexOf(
	"Set-ActiveStep -Name 'shipping_package'")
$shippingRuntimeGateIndex = $releaseValidationScript.LastIndexOf(
	'Invoke-RebirthShippingRuntimeCase')
$shippingPersistenceGateIndex = $releaseValidationScript.IndexOf(
	"-Name 'shipping_persistence_spikes'",
	$shippingRuntimeGateIndex)
$shippingFrontendGateIndex = $releaseValidationScript.IndexOf(
	"-Name 'shipping_frontend_input_hud'",
	$shippingPersistenceGateIndex)
$shippingPostRuntimeGateIndex = $releaseValidationScript.LastIndexOf(
	'Assert-ShippingArchiveManifestUnchanged')
$fullSourcePostGateIndex = $releaseValidationScript.LastIndexOf(
	"Set-ActiveStep -Name 'source_state_post'")
if ($shippingPackageGateIndex -lt 0 -or
	$shippingRuntimeGateIndex -le $shippingPackageGateIndex -or
	$shippingPersistenceGateIndex -le $shippingRuntimeGateIndex -or
	$shippingFrontendGateIndex -le $shippingPersistenceGateIndex -or
	$shippingPostRuntimeGateIndex -le $shippingFrontendGateIndex -or
	$fullSourcePostGateIndex -le $shippingPostRuntimeGateIndex) {
	throw (
		'Shipping A/B, packaged persistence, input/HUD receipts and the final ' +
		'archive hash check must run after packaging and before source lock.')
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
	'run_rebirth_savegame_roundtrips.py',
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
	'time_entry_physical=2',
	'time_entry_mesh_components=23',
	'pressure_caps=',
	'physicalContracts = 2',
	'meshComponents = 23',
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
	'parser.add_argument("--timeout-seconds"',
	'parser.add_argument("--archive-directory")',
	'self.using_packaged_shipping',
	'IndieGame-Win64-Shipping.exe',
	'subprocess.run(',
	'subprocess.CREATE_NO_WINDOW',
	'f"-UserDir={self.user_directory}"',
	'f"-IGRebirthPersistenceProbe={mode}"',
	'f"-IGRebirthProbeResultPath={receipt_path}"',
	'assert_archive_unchanged(',
	'Archive root must contain the launcher and Shipping runtime:',
	'if mode == "CatChoiceRead":',
	'arguments.append("-IGChapterTwo")',
	'"BoundaryBeforeWrite"',
	'"BoundaryBeforeRead"',
	'"BoundaryAfterWrite"',
	'"BoundaryAfterRead"',
	'"CatChoiceWrite"',
	'"CatChoiceRead"',
	'"CH02TimeWrite"',
	'"CH02TimeRead"',
	'"P5Write"',
	'"P5Read"',
	'"P3Write"',
	'"P3Read"',
	'"EndingWrite"',
	'"EndingCommit"',
	'"EndingVerify"',
	'for checkpoint in range(7):',
	'for ending in ("A", "B"):',
	'"memoryBoundaryProcessRestarts": 2',
	'"catChoiceProcessRestarts": 5',
	'"ch02TimeProcessRestarts": 2',
	'"p5ProcessRestarts": 4',
	'"p3ProcessRestarts": 7',
	'"endingProcessRestarts": 4',
	'SaveSnapshots',
	'shutil.copy2(',
	'"saveSnapshotSha256"',
	'"receiptSha256"',
	'"saveDeleted"',
	'self.archive_manifest_before: list[dict[str, object]] = []',
	'"archiveUnchanged": self.using_packaged_shipping',
	'"processCount": len(self.results)',
	'sha256_file(',
	'REBIRTH_SPIKE_HARNESS PASS complete boundary=2 cat_choices=5 ch02_time=2 p5=4 p3=7 endings=2'
)) {
	if (-not $persistenceSpikeScript.Contains(
			$requiredPersistenceHarnessInvariant)) {
		throw (
			'Persistence spike harness invariant is missing: ' +
			$requiredPersistenceHarnessInvariant)
	}
}
foreach ($forbiddenSaveGameRunnerToken in @(
	'shell=True',
	'Invoke-Expression',
	'powershell.exe',
	'-ExecutionPolicy'
)) {
	if ($persistenceSpikeScript.Contains($forbiddenSaveGameRunnerToken)) {
		throw (
			'Save-game round-trip runner contains a forbidden shell/security ' +
			"token: $forbiddenSaveGameRunnerToken")
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
	'P3_MISMATCH checkpoint=%d',
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
	'IGRebirthProbeResultPath=',
	'WriteResultReceipt',
	'ForceUTF8WithoutBOM',
	'RequestExitWithStatus'
)) {
	if (-not $persistenceProbeSource.Contains(
			$requiredPersistenceProbeInvariant)) {
		throw (
			'Persistence probe invariant is missing: ' +
			$requiredPersistenceProbeInvariant)
	}
}
$p3CompletedCheckpointStart = $persistenceProbeSource.IndexOf('case 6:')
$p3CompletedCheckpointEnd = $persistenceProbeSource.IndexOf(
	'break;',
	$p3CompletedCheckpointStart)
if ($p3CompletedCheckpointStart -lt 0 -or $p3CompletedCheckpointEnd -lt 0) {
	throw 'P3 completed checkpoint fixture block is missing.'
}
$p3CompletedCheckpointBlock = $persistenceProbeSource.Substring(
	$p3CompletedCheckpointStart,
	$p3CompletedCheckpointEnd - $p3CompletedCheckpointStart)
if (-not $p3CompletedCheckpointBlock.Contains(
		'State.PressureRiseElapsedSeconds = 0.0f;')) {
	throw (
		'P3 completed checkpoint fixture must match runtime normalization by ' +
		'clearing the pressure-rise timer.')
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
	'기준일: `2026-08-06`',
	'`UInstancedStaticMeshComponent` 23개 배치(상한 24개)',
	'`instances=1301`',
	'Component Tick은 기본 활성 상태로 시작할 수 없으며',
	'현재 월드는 `BeginPlay`에서 절차적으로 조립되므로',
	'`stat PSOPrecache`',
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
# 문서의 중심 주장은 「지원 범위 v1·v2·v3」과 「현재 코드는 3」이다. 위
# 열 줄은 그 문장들이 남아 있는지만 보고, 번호가 코드와 같은지는 안 본다.
# 번호를 올리면 문서는 그대로 v1~v3을 주장하고 픽스처 셋은 현재 버전을
# 덮지 않게 된다 — 출시 게이트가 자기가 못 덮는 범위를 덮는다고 말한다.
#
# IGSaveGame.h가 「CurrentSchemaVersion must stay 3, which the compatibility
# contract pins」라고 적어 두었는데, 그 계약이 여기 없었다.
$saveSchemaSource = Get-Content -Raw -Encoding UTF8 -LiteralPath (
	Join-Path $projectRoot 'Source/IndieGame/Save/IGSaveGame.h')
$rebirthSchemaSource = Get-Content -Raw -Encoding UTF8 -LiteralPath (
	Join-Path $projectRoot 'Source/IndieGame/Narrative/IGRebirthNarrativeSubsystem.cpp')

$statedExternal = [regex]::Match(
	$saveCompatibilityContract,
	'`UIGSaveGame::CurrentSchemaVersion=(?<version>[0-9]+)`')
if (-not $statedExternal.Success) {
	throw 'The save compatibility contract no longer states the external schema version.'
}
$declaredExternal = [regex]::Match(
	$saveSchemaSource, 'CurrentSchemaVersion = (?<version>[0-9]+);')
if (-not $declaredExternal.Success) {
	throw 'UIGSaveGame::CurrentSchemaVersion could not be read.'
}
if ($declaredExternal.Groups['version'].Value -ne $statedExternal.Groups['version'].Value) {
	throw (
		'세이브 스키마가 코드 v{0}, 문서 v{1}이다. 호환성 문서가 못 덮는 범위를 덮는다고 말한다.' -f
			$declaredExternal.Groups['version'].Value,
			$statedExternal.Groups['version'].Value)
}

# 지원 범위의 맨 끝이 현재 버전이어야 한다. 픽스처는 그 범위대로 만든다.
$claimedRange = [regex]::Match(
	$saveCompatibilityContract, '지원 주장 범위: 외부 스키마 (?<list>[^\r\n]+)')
if (-not $claimedRange.Success) {
	throw 'The save compatibility contract no longer states its supported range.'
}
$claimedVersions = @(
	[regex]::Matches($claimedRange.Groups['list'].Value, 'v(?<n>[0-9]+)') |
		ForEach-Object { [int]$_.Groups['n'].Value } |
		Sort-Object)
if ($claimedVersions[-1] -ne [int]$declaredExternal.Groups['version'].Value) {
	throw (
		'지원 범위가 v{0}까지인데 코드는 v{1}이다. 픽스처가 현재 버전을 덮지 않는다.' -f
			$claimedVersions[-1], $declaredExternal.Groups['version'].Value)
}

# 내부 REBIRTH 스냅샷도 문서가 번호를 댄다. 줄바꿈을 넘어 읽는다.
$statedInternal = [regex]::Match(
	$saveCompatibilityContract, '내부 REBIRTH\s*스냅샷 스키마 (?<version>[0-9]+)')
if (-not $statedInternal.Success) {
	throw 'The save compatibility contract no longer states the REBIRTH snapshot schema.'
}
$declaredInternal = [regex]::Match(
	$rebirthSchemaSource, 'SnapshotSchemaVersion = (?<version>[0-9]+);')
if (-not $declaredInternal.Success) {
	throw 'IGRebirthState::SnapshotSchemaVersion could not be read.'
}
if ($declaredInternal.Groups['version'].Value -ne $statedInternal.Groups['version'].Value) {
	throw (
		'REBIRTH 스냅샷 스키마가 코드 {0}, 문서 {1}이다.' -f
			$declaredInternal.Groups['version'].Value,
			$statedInternal.Groups['version'].Value)
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

foreach ($requiredFrontendProbeControllerInvariant in @(
	'IGFrontendShippingProbe',
	'void AIGPlayerController::TickFrontendShippingProbe()',
	'FInputKeyEventArgs Pressed(',
	'FInputKeyEventArgs Released(',
	'!Params.IsSimulatedInput()',
	'EKeys::F10',
	'EKeys::Gamepad_DPad_Down',
	'EKeys::Gamepad_Special_Right',
	'EKeys::Gamepad_FaceButton_Bottom',
	'EKeys::Gamepad_FaceButton_Right',
	'EKeys::Gamepad_Special_Left',
	'TryCaptureFrontendProbeLayout(TEXT("display_gamepad"), 11)',
	'TEXT("dialogue_default_scale"),',
	'TEXT("dialogue_max_scale"),',
	'GetDialogueRenderSample(',
	'IGFrontendDefaultScreenshotPath=',
	'IGFrontendAccessibilityScreenshotPath=',
	'IGFrontendDisplayScreenshotPath=',
	'IGFrontendTitleScreenshotPath=',
	'IGFrontendScreenshotPath=',
	'bFrontendProbeCompilationDrained',
	'FAssetCompilingManager::Get().FinishAllCompilation()',
	'GShaderCompilingManager->FinishAllCompilation()',
	'FScreenshotRequest::RequestScreenshot(',
	'Settings.CaptionSizeScale = 2.0f',
	'FrontendProbeLayoutSampleCount != 11',
	'FrontendProbePressedEventCount != 11',
	'REBIRTH_FRONTEND PASS contract=4 resolution=%dx%d',
	'FPlatformMisc::RequestExitWithStatus'
)) {
	if (-not $playerControllerSource.Contains(
		$requiredFrontendProbeControllerInvariant)) {
		throw (
			'Frontend Shipping input-path invariant is missing: ' +
			$requiredFrontendProbeControllerInvariant)
	}
}
foreach ($requiredHudLayoutProbeInvariant in @(
	'bLayoutValidationEnabled = FParse::Param(',
	'bool AIGHorrorHUD::GetLayoutValidationSample(',
	'void AIGHorrorHUD::BeginLayoutValidationSample()',
	'void AIGHorrorHUD::RecordLayoutValidationRect(',
	'void AIGHorrorHUD::FinalizeLayoutValidationSample()',
	'if (bLayoutValidationEnabled)',
	'Canvas->StrLen(Font, Text.ToString(), TextWidth, TextHeight, true)',
	'bLayoutValidationAllInsideCanvas',
	'PixelTolerance = 1.5f'
)) {
	if (-not $horrorHudSource.Contains($requiredHudLayoutProbeInvariant)) {
		throw (
			'Frontend HUD layout-probe invariant is missing: ' +
			$requiredHudLayoutProbeInvariant)
	}
}
$layoutMeasurementIndex = $horrorHudSource.IndexOf(
	'Canvas->StrLen(Font, Text.ToString(), TextWidth, TextHeight, true)')
$layoutGateIndex = $horrorHudSource.LastIndexOf(
	'if (bLayoutValidationEnabled)',
	$layoutMeasurementIndex)
if ($layoutMeasurementIndex -lt 0 -or
	$layoutGateIndex -lt 0 -or
	$layoutMeasurementIndex - $layoutGateIndex -gt 100) {
	throw 'HUD layout measurement must remain gated out of normal gameplay.'
}

foreach ($requiredLensDropletHudInvariant in @(
	'void AIGHorrorHUD::InitializeLensDropletTexture()',
	'MakeUniqueObjectName(',
	'UTexture2D::CreateTransient(',
	'PF_B8G8R8A8',
	'LensDropletTexture->NeverStream = true',
	'void AIGHorrorHUD::DrawLensDroplet(',
	'Accessibility->IsReducedCameraMotionEnabled()',
	'bool AIGHorrorHUD::GetLensDropletRenderSample(',
	'LensDropletLastPosition = DropPosition',
	'LensDropletLastCanvasSize = FVector2D(Canvas->ClipX, Canvas->ClipY)',
	'LensDropletLastRenderTime = CurrentTime',
	'Droplet.BlendMode = SE_BLEND_Translucent',
	'DrawLensDroplet(CurrentTime);'
)) {
	if (-not $horrorHudSource.Contains($requiredLensDropletHudInvariant)) {
		throw "CH03 one-shot lens-droplet HUD invariant is missing: $requiredLensDropletHudInvariant"
	}
}
foreach ($requiredLensDropletFlowInvariant in @(
	'OpeningLensDropletDelaySeconds = 1.05f',
	'OpeningLensDropletDurationSeconds = 3.0f',
	'&ThisClass::ShowOpeningLensDroplet',
	'if ((bCaptureMode && !bLensDropletCaptureMode)',
	'AIGHorrorHUD::PushLensDroplet('
)) {
	if (-not $thirdMorningSource.Contains($requiredLensDropletFlowInvariant)) {
		throw "CH03 one-shot lens-droplet flow invariant is missing: $requiredLensDropletFlowInvariant"
	}
}
foreach ($requiredLensDropletCaptureInvariant in @(
	'IGCaptureCH03LensDroplet',
	'LensDropletCaptureEarlyDelaySeconds = 0.42f',
	'LensDropletCaptureLateDelaySeconds = 1.35f',
	'LensDropletCaptureMinimumTravelPixels = 8.0f',
	'LensDropletCaptureMaximumReducedTravelPixels = 0.5f',
	'LensDropletCaptureMinimumAlpha = 0.55f',
	'void AIGThirdMorningDirector::StartLensDropletCaptureSequence()',
	'void AIGThirdMorningDirector::CaptureLensDropletEarlyFrame()',
	'void AIGThirdMorningDirector::CaptureLensDropletLateFrame()',
	'void AIGThirdMorningDirector::FinishLensDropletCaptureSequence()',
	'FPaths::FileExists(LensDropletCaptureEarlyPath)',
	'REBIRTH_CH03_LENS_CAPTURE %s mode=%s reduced_motion=%d',
	'hud_safe=%d',
	'WriteCaptureReceipt(Receipt)'
)) {
	if (-not $thirdMorningSource.Contains($requiredLensDropletCaptureInvariant)) {
		throw (
			'CH03 Shipping lens-droplet capture invariant is missing: ' +
			$requiredLensDropletCaptureInvariant)
	}
}
if (([regex]::Matches(
	$worldSceneSource,
	'IGCaptureCH03LensDroplet')).Count -ne 2) {
	throw 'CH03 lens-droplet capture flag must route and configure exactly once each.'
}
$drawHudStart = $horrorHudSource.IndexOf('void AIGHorrorHUD::DrawHUD()')
if ($drawHudStart -lt 0) {
	throw 'CH03 lens-droplet DrawHUD entry is missing.'
}
$drawAudioCaptionStart = $horrorHudSource.IndexOf(
	'bool AIGHorrorHUD::DrawAudioCaption(',
	$drawHudStart)
if ($drawAudioCaptionStart -le $drawHudStart) {
	throw 'CH03 lens-droplet DrawHUD block is malformed.'
}
$drawHudBlock = $horrorHudSource.Substring(
	$drawHudStart,
	$drawAudioCaptionStart - $drawHudStart)
$chapterCardDrawIndex = $drawHudBlock.IndexOf('DrawChapterCard(CurrentTime)')
$lensDropletDrawIndex = $drawHudBlock.IndexOf('DrawLensDroplet(CurrentTime);')
$crosshairDrawIndex = $drawHudBlock.IndexOf('DrawCrosshair(')
if ($chapterCardDrawIndex -lt 0 -or
	$lensDropletDrawIndex -le $chapterCardDrawIndex -or
	$crosshairDrawIndex -le $lensDropletDrawIndex) {
	throw 'CH03 lens droplet must stay below native HUD content and outside chapter cards.'
}
if (([regex]::Matches(
	$thirdMorningSource,
	'&ThisClass::ShowOpeningLensDroplet')).Count -ne 1) {
	throw 'CH03 opening lens droplet must be scheduled exactly once.'
}

if (-not $playerCharacterSource.Contains(
	'InitCapsuleSize(30.0f, 96.0f)')) {
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
	'FVector(2515, -470, 0)',
	'FVector(2910, -470, 0)',
	'FVector(2760, -285, 0)',
	'FVector(2670, -285, 0)'
)) {
	if (-not $demoDirectorSource.Contains($requiredCaptureWaypoint)) {
		throw "Required collision-safe CH01 capture waypoint is missing: $requiredCaptureWaypoint"
	}
}
if ($demoDirectorSource.Contains('FVector(2610, -320, 0)')) {
	throw 'CH01 capture route must not use the sub-capsule gap between the register and gondola.'
}
if (-not $worldSceneSource.Contains(
	'CreateBlock(FVector(131, -225, 220), FVector(110, 20, 20), WallX)')) {
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

$rebirthEvidenceContractScript = Join-Path $projectRoot `
	'Scripts/Test-Rebirth-EvidenceContract.ps1'
& $rebirthEvidenceContractScript

$chapterTwoTimeEntryContractScript = Join-Path $projectRoot `
	'Scripts/Test-Rebirth-ChapterTwoTimeEntryContract.ps1'
& $chapterTwoTimeEntryContractScript

$audioContractScript = Join-Path $projectRoot `
	'Scripts/Test-Rebirth-AudioContract.ps1'
& $audioContractScript

$accessibilityContractScript = Join-Path $projectRoot `
	'Scripts/Test-Rebirth-AccessibilityContract.ps1'
& $accessibilityContractScript

$dialogueContractScript = Join-Path $projectRoot `
	'Scripts/Test-Rebirth-DialogueContract.ps1'
& $dialogueContractScript

$frontendContractScript = Join-Path $projectRoot `
	'Scripts/Test-Rebirth-FrontendContract.ps1'
& $frontendContractScript

$artAssetContractScript = Join-Path $projectRoot `
	'Scripts/Test-ArtAssetContract.ps1'
& $artAssetContractScript

$missingFloorM0InputContractScript = Join-Path $projectRoot `
	'Scripts/Test-MissingFloor-M0InputContract.ps1'
& $missingFloorM0InputContractScript

$missingFloorProductionEntryContractScript = Join-Path $projectRoot `
	'Scripts/Test-MissingFloor-ProductionEntryContract.ps1'
& $missingFloorProductionEntryContractScript

$missingFloorM1CaptureContractScript = Join-Path $projectRoot `
	'Scripts/Test-MissingFloor-M1CaptureContract.ps1'
& $missingFloorM1CaptureContractScript

$missingFloorM1WakeEchoContractScript = Join-Path $projectRoot `
	'Scripts/Test-MissingFloor-M1WakeEchoContract.ps1'
& $missingFloorM1WakeEchoContractScript

$missingFloorM5RevealContractScript = Join-Path $projectRoot `
	'Scripts/Test-MissingFloor-M5RevealContract.ps1'
& $missingFloorM5RevealContractScript

$missingFloorReleaseEndingContractScript = Join-Path $projectRoot `
	'Scripts/Test-MissingFloor-ReleaseEndingContract.ps1'
& $missingFloorReleaseEndingContractScript

$missingFloorReleaseGateContractScript = Join-Path $projectRoot `
	'Scripts/Test-MissingFloor-ReleaseGateContract.ps1'
& $missingFloorReleaseGateContractScript

$missingFloorSignatureSfxContractScript = Join-Path $projectRoot `
	'Scripts/Test-MissingFloor-SignatureSfxContract.ps1'
& $missingFloorSignatureSfxContractScript

$missingFloorTuningTableContractScript = Join-Path $projectRoot `
	'Scripts/Test-MissingFloor-TuningTableContract.ps1'
& $missingFloorTuningTableContractScript

$missingFloorMixMovementContractScript = Join-Path $projectRoot `
	'Scripts/Test-MissingFloor-MixAndMovementContract.ps1'
& $missingFloorMixMovementContractScript

$missingFloorInputBindingContractScript = Join-Path $projectRoot `
	'Scripts/Test-MissingFloor-InputBindingContract.ps1'
& $missingFloorInputBindingContractScript

$missingFloorBibleContractScript = Join-Path $projectRoot `
	'Scripts/Test-MissingFloor-BibleContract.ps1'
& $missingFloorBibleContractScript

$missingFloorM6AudioVisualContractScript = Join-Path $projectRoot `
	'Scripts/Test-MissingFloor-M6AudioVisualContract.ps1'
& $missingFloorM6AudioVisualContractScript

$missingFloorM65MercyNoteContractScript = Join-Path $projectRoot `
	'Scripts/Test-MissingFloor-M65MercyNoteContract.ps1'
& $missingFloorM65MercyNoteContractScript

# §20 난이도 네 모드와 자비 안전망. 파일은 있었는데 아무도 부르지
# 않아서 108개 단언이 그냥 안 돌고 있었다.
$missingFloorM8DifficultyContractScript = Join-Path $projectRoot `
	'Scripts/Test-MissingFloor-M8DifficultyContract.ps1'
& $missingFloorM8DifficultyContractScript

$missingFloorM65AudioCalibrationContractScript = Join-Path $projectRoot `
	'Scripts/Test-MissingFloor-M65AudioCalibrationContract.ps1'
& $missingFloorM65AudioCalibrationContractScript

$missingFloorM3CctvChannelContractScript = Join-Path $projectRoot `
	'Scripts/Test-MissingFloor-M3CctvChannelContract.ps1'
& $missingFloorM3CctvChannelContractScript

$missingFloorM3DoorBeatContractScript = Join-Path $projectRoot `
	'Scripts/Test-MissingFloor-M3DoorBeatContract.ps1'
& $missingFloorM3DoorBeatContractScript

$missingFloorM4PassByContractScript = Join-Path $projectRoot `
	'Scripts/Test-MissingFloor-M4PassByContract.ps1'
& $missingFloorM4PassByContractScript

$missingFloorNightFiveSlotContractScript = Join-Path $projectRoot `
	'Scripts/Test-MissingFloor-NightFiveSlotContract.ps1'
& $missingFloorNightFiveSlotContractScript

# Physical plausibility of the code-authored world, and the offline half of
# the art contracts. Both run without Unreal, so they gate every commit rather
# than waiting for an editor pass.
$python = Get-Command python -ErrorAction SilentlyContinue
if (-not $python) {
	$python = Get-Command python3 -ErrorAction SilentlyContinue
}
if ($python) {
	$geometryAudit = Join-Path $projectRoot 'Scripts/audit_world_geometry.py'
	# 이 스캐너는 감사 여섯 개가 같이 쓴다. 람다 인자와 회전이 안 풀리면
	# 상자가 대각선 길이짜리 정육면체로 부풀어 조용히 거짓 양성을 쏟는다.
	& $python.Source $geometryAudit --self-test
	if ($LASTEXITCODE -ne 0) {
		throw "World geometry scanner self-test failed ($LASTEXITCODE)"
	}

	$worldGeometryOutput = & $python.Source $geometryAudit --check
	$worldGeometryOutput | ForEach-Object { Write-Host $_ }
	if ($LASTEXITCODE -ne 0) {
		throw "World geometry audit found impossible placements ($LASTEXITCODE)"
	}
	Assert-AuditBlindSpot $worldGeometryOutput '자리를 풀지 못한 상자 (?<count>\d+)건' 52 `
		'좌표가 트랜스폼 지역 변수나 포인터 삼항에 걸려 자리를 풀지 못한 상자'

	$atlasPacker = Join-Path $projectRoot 'Scripts/build_texture_atlas.py'
	& $python.Source $atlasPacker --self-test
	if ($LASTEXITCODE -ne 0) {
		throw "Texture atlas packer self-test failed ($LASTEXITCODE)"
	}

	# What the packaged build will and will not contain. The cooker does not
	# read C++, so an asset only a LoadObject path names is absent from the pak
	# and null at runtime -- in the packaged build alone, which is the build
	# nobody runs while iterating. This catches that here instead.
	#
	# The self-test runs first because the audit itself skips on a checkout
	# that did not fetch LFS, and a gate that can skip needs something that
	# cannot.
	$cookReferences = Join-Path $projectRoot 'Scripts/check_cook_references.py'
	& $python.Source $cookReferences --self-test
	if ($LASTEXITCODE -ne 0) {
		throw "Cook reference audit self-test failed ($LASTEXITCODE)"
	}

	& $python.Source $cookReferences --check
	if ($LASTEXITCODE -ne 0) {
		throw "Cook reference audit found unreachable assets ($LASTEXITCODE)"
	}

	# The atlas only saves anything if the textures it replaced stop being
	# cooked. That cannot be observed until the editor rebuilds the print
	# materials, so this moves the one edge per material the rebuild moves and
	# reports the whole delta -- including what would break.
	& $python.Source $cookReferences --simulate-rebuild
	if ($LASTEXITCODE -ne 0) {
		throw "The atlas rebuild would not drop the textures it replaces ($LASTEXITCODE)"
	}

	# The full pass recreates its materials; the targeted IG_*_ONLY passes
	# update them in place and leave the pre-atlas sampler behind. This runs
	# each of those over the pre-atlas graph both ways, so the pass list stays
	# honest and the exposure stays visible.
	& $python.Source $cookReferences --simulate-targeted
	if ($LASTEXITCODE -ne 0) {
		throw "A targeted material pass would keep its atlassed textures ($LASTEXITCODE)"
	}

	# 쿠크에 들어 있어도 씬이 이름을 못 부르면 화면에는 없는 것과 같다.
	# LoadTexturedMaterials()의 배열이 그 관문인데, 이름 한 줄이 빠져도
	# TexMat은 조용히 폴백을 돌려주므로 컴파일도 쿠크도 통과한다.
	$sceneMaterials = Join-Path $projectRoot 'Scripts/audit_scene_materials.py'
	& $python.Source $sceneMaterials --self-test
	if ($LASTEXITCODE -ne 0) {
		throw "Scene material audit self-test failed ($LASTEXITCODE)"
	}

	& $python.Source $sceneMaterials --check
	if ($LASTEXITCODE -ne 0) {
		throw "A material the scene asks for never reaches it ($LASTEXITCODE)"
	}

	# 씬이 CreateBlock으로 짓는 것은 위 기하 감사가 좌표까지 읽는다. 밤과
	# 퍼즐의 소품은 디렉터가 SpawnActor 뒤에 Configure로 붙이므로 그 경로에
	# 있었고, 크기 인자의 뜻이 함수마다 달라서 조용히 1m 정육면체가 되거나
	# 재질 없이 엔진 기본 격자로 그려지고 있었다.
	$directorProps = Join-Path $projectRoot 'Scripts/audit_director_props.py'
	& $python.Source $directorProps --self-test
	if ($LASTEXITCODE -ne 0) {
		throw "Director prop audit self-test failed ($LASTEXITCODE)"
	}

	$directorPropsOutput = & $python.Source $directorProps --check
	$directorPropsOutput | ForEach-Object { Write-Host $_ }
	if ($LASTEXITCODE -ne 0) {
		throw "A director-spawned prop is not configured to contract ($LASTEXITCODE)"
	}
	Assert-AuditBlindSpot $directorPropsOutput '자리를 풀지 못한 소품 (?<count>\d+)건' 0 `
		'크기나 좌표가 리터럴이 아니라 자리를 풀지 못한 소품'
	Assert-AuditBlindSpot $directorPropsOutput '대조하지 못한 호출부 (?<count>\d+)건' 0 `
		'SpawnActor를 같은 파일에서 찾지 못해 대조 못 한 호출부'

	# 건축 재질은 월드 좌표를 읽으므로 축이 맞는 면에서만 무늬가 변한다.
	# 이름도 자리도 크기도 맞는데 면의 방향 하나가 어긋나면 그 면 전체가
	# 한 줄로 늘어나고, 위 감사 넷은 그것을 보지 않는다.
	$surfaceProjection = Join-Path $projectRoot 'Scripts/audit_surface_projection.py'
	& $python.Source $surfaceProjection --self-test
	if ($LASTEXITCODE -ne 0) {
		throw "Surface projection audit self-test failed ($LASTEXITCODE)"
	}

	$surfaceProjectionOutput = & $python.Source $surfaceProjection --check
	$surfaceProjectionOutput | ForEach-Object { Write-Host $_ }
	if ($LASTEXITCODE -ne 0) {
		throw "A world-projected material is stretched across the face it is on ($LASTEXITCODE)"
	}
	Assert-AuditBlindSpot $surfaceProjectionOutput 'unresolved=(?<count>\d+)' 25 `
		'호출부가 리터럴도 지역 변수도 아니라 재질을 풀지 못한 상자'

	# 간판과 명판은 메시 UV를 읽는데 엔진 기본 큐브는 여섯 면이 그 UV를
	# 나눠 쓴다. 두께가 있는 몸통에 인쇄를 통째로 주면 옆면에도 같은 그림이
	# 눌려 한 번 더 찍힌다.
	$printedFaces = Join-Path $projectRoot 'Scripts/audit_printed_faces.py'
	& $python.Source $printedFaces --self-test
	if ($LASTEXITCODE -ne 0) {
		throw "Printed face audit self-test failed ($LASTEXITCODE)"
	}

	$printedFacesOutput = & $python.Source $printedFaces --check
	$printedFacesOutput | ForEach-Object { Write-Host $_ }
	if ($LASTEXITCODE -ne 0) {
		throw "A printed material is wrapped around a whole body instead of its face ($LASTEXITCODE)"
	}
	Assert-AuditBlindSpot $printedFacesOutput 'unresolved=(?<count>\d+)' 25 `
		'인쇄 재질을 풀지 못한 상자'

	# 발소리 표면 태그는 소리만 정하는 게 아니라 반향 공간까지 고른다.
	# 옥상 슬래브 하나가 태그를 빼먹으면 탁 트인 옥상이 복도로 울린다.
	$footstepSurfaces = Join-Path $projectRoot 'Scripts/audit_footstep_surfaces.py'
	& $python.Source $footstepSurfaces --self-test
	if ($LASTEXITCODE -ne 0) {
		throw "Footstep surface audit self-test failed ($LASTEXITCODE)"
	}

	& $python.Source $footstepSurfaces --check
	if ($LASTEXITCODE -ne 0) {
		throw "A walkable surface is missing its footstep tag ($LASTEXITCODE)"
	}

	# §18.7은 홀드 완료 편차를 ±3%로 걸어 두었다. 눈으로 읽어서는 지킬 수
	# 없는 줄이라 실제로 돌려 본다 — 짧은 홀드를 새로 적어 넣으면 여기서
	# 걸린다.
	$holdTiming = Join-Path $projectRoot 'Scripts/audit_hold_timing.py'
	& $python.Source $holdTiming --self-test
	if ($LASTEXITCODE -ne 0) {
		throw "Hold timing audit self-test failed ($LASTEXITCODE)"
	}

	& $python.Source $holdTiming --check
	if ($LASTEXITCODE -ne 0) {
		throw "A hold completes outside the §18.7 window ($LASTEXITCODE)"
	}

	# 같은 사실을 두 파일이 각자 적어 두는 것. 냉장고 험, 4층 슬래브 높이,
	# 로비 모니터 치수가 차례로 그랬다. 셋 다 컴파일도 되고 화면도 뜬다.
	$duplicateConstants = Join-Path $projectRoot 'Scripts/audit_duplicate_constants.py'
	& $python.Source $duplicateConstants --self-test
	if ($LASTEXITCODE -ne 0) {
		throw "Duplicate constant audit self-test failed ($LASTEXITCODE)"
	}

	& $python.Source $duplicateConstants --check
	if ($LASTEXITCODE -ne 0) {
		throw "An authored constant is written in two places ($LASTEXITCODE)"
	}

	# 끊어진 접근성 설정은 화면으로 안 보인다. 메뉴에 뜨고 켜지고 저장되고,
	# 다시 켜면 켜져 있다. 바뀌는 게 없다는 것만 다르다.
	$accessibilityReach =
		Join-Path $projectRoot 'Scripts/audit_accessibility_reach.py'
	& $python.Source $accessibilityReach --self-test
	if ($LASTEXITCODE -ne 0) {
		throw "Accessibility reach audit self-test failed ($LASTEXITCODE)"
	}

	$accessibilityReachOutput = & $python.Source $accessibilityReach --check
	$accessibilityReachOutput | ForEach-Object { Write-Host $_ }
	if ($LASTEXITCODE -ne 0) {
		throw "An accessibility setting changes nothing a player can feel ($LASTEXITCODE)"
	}

	# 아무도 안 부르는 접근자. 설정 자체는 다른 경로로 살아 있어서 고장은
	# 아니지만, 이 수가 늘면 쓰지도 않는 문을 계속 세우고 있다는 뜻이다.
	Assert-AuditBlindSpot $accessibilityReachOutput `
		'아무도 안 부르는 접근자 (?<count>\d+)개' 3 `
		'설정을 읽지만 아무도 부르지 않는 접근자'

	# 합성기는 음을 그냥 더하고 ±1.0에서 자른다. 겹친 음의 합이 1을 넘으면
	# 파형이 int16으로 굳기 전에 깎이고, 그 뒤로는 버스를 줄이든 감쇠를 걸든
	# 되돌릴 방법이 없다.
	$toneHeadroom = Join-Path $projectRoot 'Scripts/audit_tone_headroom.py'
	& $python.Source $toneHeadroom --self-test
	if ($LASTEXITCODE -ne 0) {
		throw "Tone headroom audit self-test failed ($LASTEXITCODE)"
	}

	$toneHeadroomOutput = & $python.Source $toneHeadroom --check
	$toneHeadroomOutput | ForEach-Object { Write-Host $_ }
	if ($LASTEXITCODE -ne 0) {
		throw "A generated waveform clips before it reaches the mix ($LASTEXITCODE)"
	}

	# 반복문 안에서 음을 만들거나 진폭이 실행 중에 정해지는 생성기는 못 읽는다.
	# 못 읽은 것을 통과로 세지 않으니, 이 수가 늘면 판정 못 하는 파형이 늘어난
	# 것이다.
	Assert-AuditBlindSpot $toneHeadroomOutput `
		'판정을 못 하는 생성기 (?<count>\d+)개' 33 `
		'음을 다 못 읽어서 깎임 여부를 판정 못 한 생성기'

	# 설계값을 맨 숫자로 찾는 계약. 3300줄 문서에서 0.6은 열일곱 번 나오므로
	# 그런 줄은 절이 통째로 사라져도 통과한다.
	$weakNeedles = Join-Path $projectRoot 'Scripts/audit_weak_needles.py'
	& $python.Source $weakNeedles --self-test
	if ($LASTEXITCODE -ne 0) {
		throw "Weak needle audit self-test failed ($LASTEXITCODE)"
	}

	& $python.Source $weakNeedles --check
	if ($LASTEXITCODE -ne 0) {
		throw "A contract pins a design value with a bare number ($LASTEXITCODE)"
	}

	# 광원도 가구와 같은 리터럴 좌표로 놓는다. 옆 가구가 자라면 그 안으로
	# 들어가는데, 방이 어두워질 뿐 아무것도 실패하지 않는다.
	$lightPlacement = Join-Path $projectRoot 'Scripts/audit_light_placement.py'
	& $python.Source $lightPlacement --self-test
	if ($LASTEXITCODE -ne 0) {
		throw "Light placement audit self-test failed ($LASTEXITCODE)"
	}

	& $python.Source $lightPlacement --check
	if ($LASTEXITCODE -ne 0) {
		throw "A light source is sealed inside solid geometry ($LASTEXITCODE)"
	}

	# 스캔 소품의 맞춤 상자는 메시의 로컬 축에 먹는다. 상자를 월드 기준으로
	# 적으면 요각에서 가로세로가 뒤집히고, 아무것도 실패하지 않은 채 침대가
	# 벽 안으로 들어간다. 원본 glTF 바운드로 최종 크기를 직접 계산한다.
	$photoPropFit = Join-Path $projectRoot 'Scripts/audit_photo_prop_fit.py'
	& $python.Source $photoPropFit --self-test
	if ($LASTEXITCODE -ne 0) {
		throw "Photo prop fit audit self-test failed ($LASTEXITCODE)"
	}

	$photoPropOutput = & $python.Source $photoPropFit --check
	$photoPropOutput | ForEach-Object { Write-Host $_ }
	if ($LASTEXITCODE -ne 0) {
		throw "A scanned prop lands inside the structure it stands against ($LASTEXITCODE)"
	}
	Assert-AuditBlindSpot $photoPropOutput '건너뛴 원본 (?<count>\d+)종' 6 `
		'메시가 여럿이라 크기를 못 재는 사진 원본'

	# 나중에 세운 상자가 이미 있던 상자의 면과 소수점까지 같은 평면에 놓이면
	# 깊이 버퍼가 둘을 갈라내지 못한다. 파고든 깊이가 0이라 기하 감사도
	# 못 보고, 화면에서는 카메라가 움직일 때마다 두 재질이 번갈아 이긴다.
	$coplanarSurfaces = Join-Path $projectRoot 'Scripts/audit_coplanar_surfaces.py'
	& $python.Source $coplanarSurfaces --self-test
	if ($LASTEXITCODE -ne 0) {
		throw "Coplanar surface audit self-test failed ($LASTEXITCODE)"
	}

	& $python.Source $coplanarSurfaces --check
	if ($LASTEXITCODE -ne 0) {
		throw "Two drawn surfaces share a plane and will fight for depth ($LASTEXITCODE)"
	}

	# 검은 금속이 모두 오류인 것은 아니다. 색 손실 후보를 추려 사람이 볼 목록을 남긴다.
	$metalBaseColor = Join-Path $projectRoot 'Scripts/audit_metal_base_color.py'
	$metalBaseColorReport = Join-Path $projectRoot 'Saved/MetalBaseColorAudit.json'
	& $python.Source $metalBaseColor --output $metalBaseColorReport
	if ($LASTEXITCODE -ne 0) {
		throw "금속 색 손실 후보 검사 실패 ($LASTEXITCODE)"
	}
	$metalBaseColorCandidates = @(Get-Content -Raw -Encoding UTF8 -LiteralPath $metalBaseColorReport | ConvertFrom-Json)
	if ($metalBaseColorCandidates.Count -gt 0) {
		Write-Warning ("금속 색 손실 후보 {0}개를 화면에서 확인해야 한다: {1}" -f $metalBaseColorCandidates.Count, $metalBaseColorReport)
	}
} else {
	Write-Warning 'python not found; skipped the geometry audit, atlas self-test, cook reference audit, scene material audit, director prop audit, surface projection audit, printed face audit, footstep surface audit, light placement audit, photo prop fit audit and coplanar surface audit.'
}

Write-Host 'Project structure validation passed (this is not an Unreal build).' -ForegroundColor Green
