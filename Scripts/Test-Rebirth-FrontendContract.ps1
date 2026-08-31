[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

$projectRoot = Split-Path -Parent $PSScriptRoot
$controllerHeader = Get-Content -Raw -Encoding UTF8 -LiteralPath (
	Join-Path $projectRoot 'Source/IndieGame/Player/IGPlayerController.h')
$controllerSource = Get-Content -Raw -Encoding UTF8 -LiteralPath (
	Join-Path $projectRoot 'Source/IndieGame/Player/IGPlayerController.cpp')
$hudHeader = Get-Content -Raw -Encoding UTF8 -LiteralPath (
	Join-Path $projectRoot 'Source/IndieGame/Player/IGHorrorHUD.h')
$hudSource = Get-Content -Raw -Encoding UTF8 -LiteralPath (
	Join-Path $projectRoot 'Source/IndieGame/Player/IGHorrorHUD.cpp')
$settingsLayout = Get-Content -Raw -Encoding UTF8 -LiteralPath (
	Join-Path $projectRoot 'Source/IndieGame/Player/IGSettingsMenuLayout.h')
$frontendLayout = Get-Content -Raw -Encoding UTF8 -LiteralPath (
	Join-Path $projectRoot 'Source/IndieGame/Player/IGFrontendMenuLayout.h')
$moduleRules = Get-Content -Raw -Encoding UTF8 -LiteralPath (
	Join-Path $projectRoot 'Source/IndieGame/IndieGame.Build.cs')
$saveHeader = Get-Content -Raw -Encoding UTF8 -LiteralPath (
	Join-Path $projectRoot 'Source/IndieGame/Save/IGSaveSubsystem.h')
$saveSource = Get-Content -Raw -Encoding UTF8 -LiteralPath (
	Join-Path $projectRoot 'Source/IndieGame/Save/IGSaveSubsystem.cpp')
$thirdMorningHeader = Get-Content -Raw -Encoding UTF8 -LiteralPath (
	Join-Path $projectRoot 'Source/IndieGame/Sequence/IGThirdMorningDirector.h')
$thirdMorningSource = Get-Content -Raw -Encoding UTF8 -LiteralPath (
	Join-Path $projectRoot 'Source/IndieGame/Sequence/IGThirdMorningDirector.cpp')
$inputConfig = Get-Content -Raw -Encoding UTF8 -LiteralPath (
	Join-Path $projectRoot 'Config/DefaultInput.ini')
$gameConfig = Get-Content -Raw -Encoding UTF8 -LiteralPath (
	Join-Path $projectRoot 'Config/DefaultGame.ini')
$userSettingsConfig = Get-Content -Raw -Encoding UTF8 -LiteralPath (
	Join-Path $projectRoot 'Config/DefaultGameUserSettings.ini')
$projectDescriptor = Get-Content -Raw -Encoding UTF8 -LiteralPath (
	Join-Path $projectRoot 'IndieGame.uproject')
$gameTarget = Get-Content -Raw -Encoding UTF8 -LiteralPath (
	Join-Path $projectRoot 'Source/IndieGame.Target.cs')
$iconScript = Get-Content -Raw -Encoding UTF8 -LiteralPath (
	Join-Path $projectRoot 'Scripts/prepare_application_icon.py')
$executableIconScript = Get-Content -Raw -Encoding UTF8 -LiteralPath (
	Join-Path $projectRoot 'Scripts/Test-Windows-ExecutableIcon.ps1')
$executableMetadataSyncScript = Get-Content `
	-Raw `
	-Encoding UTF8 `
	-LiteralPath (
		Join-Path $projectRoot `
			'Scripts/Copy-Windows-ExecutableVersionResource.ps1')
$executableMetadataScript = Get-Content -Raw -Encoding UTF8 -LiteralPath (
	Join-Path $projectRoot 'Scripts/Test-Windows-ExecutableMetadata.ps1')
$releaseValidationScript = Get-Content -Raw -Encoding UTF8 -LiteralPath (
	Join-Path $projectRoot 'Scripts/Run-Rebirth-ReleaseValidation.ps1')
$frontendProbeScript = Get-Content -Raw -Encoding UTF8 -LiteralPath (
	Join-Path $projectRoot 'Scripts/Run-Rebirth-FrontendShippingProbe.ps1')
$prepareAiArtScript = Get-Content -Raw -Encoding UTF8 -LiteralPath (
	Join-Path $projectRoot 'Scripts/Prepare-AIArt.ps1')
$surfaceTextureScript = Get-Content -Raw -Encoding UTF8 -LiteralPath (
	Join-Path $projectRoot 'Scripts/generate_surface_textures.py')
$assetPolicy = Get-Content -Raw -Encoding UTF8 -LiteralPath (
	Join-Path $projectRoot 'Docs/ASSET_POLICY.md')
$iconPrompt = Get-Content -Raw -Encoding UTF8 -LiteralPath (
	Join-Path $projectRoot 'Docs/IMAGEGEN_PROMPTS_2026-08-05.md')
$titlePrompt = Get-Content -Raw -Encoding UTF8 -LiteralPath (
	Join-Path $projectRoot 'Docs/IMAGEGEN_PROMPTS_2026-08-12.md')
$iconPngPath = Join-Path $projectRoot 'Build/Windows/ApplicationIcon.png'
$iconIcoPath = Join-Path $projectRoot 'Build/Windows/Application.ico'
$iconPngBytes = [IO.File]::ReadAllBytes($iconPngPath)
$iconIcoBytes = [IO.File]::ReadAllBytes($iconIcoPath)
$assertionCount = 0

function Assert-True {
	param(
		[Parameter(Mandatory = $true)]
		[bool]$Condition,
		[Parameter(Mandatory = $true)]
		[string]$Message
	)
	if (-not $Condition) {
		throw "REBIRTH_FRONTEND_CONTRACT FAIL: $Message"
	}
	$script:assertionCount++
}

function Assert-ContainsAll {
	param(
		[Parameter(Mandatory = $true)]
		[string]$Text,
		[Parameter(Mandatory = $true)]
		[string[]]$Needles,
		[Parameter(Mandatory = $true)]
		[string]$Context
	)
	foreach ($needle in $Needles) {
		Assert-True ($Text.Contains($needle)) "$Context 누락: $needle"
	}
}

Assert-ContainsAll $controllerHeader @(
	'EIGSystemMenuMode',
	'Title',
	'Pause',
	'AudioCalibration',
	'DisplaySettings',
	'Credits',
	'ShouldShowTitleMenu',
	'RefreshMenuHud() const',
	'StartNewGame',
	'ContinueLatestAutosave',
	'QuitToDesktop',
	'HandleMenuPointerClick',
	'HandleSaveCompleted',
	'HandleLoadCompleted'
) '네이티브 프런트엔드 상태 모델'

Assert-ContainsAll $controllerSource @(
	'FApp::IsUnattended()',
	'IsRunningCommandlet()',
	'IGSkipFrontend',
	'IGResumeSave',
	'IGNewGame',
	'IGChapterTwo',
	'IGChapterThree',
	'Binding.bExecuteWhenPaused = true',
	'SetPause(NewMode != EIGSystemMenuMode::Hidden)',
	'StoryState->ClearStates(false)',
	'RebirthState->ResetNarrative()',
	'SaveSubsystem->ClearRotatingAutosaves()',
	'bCompatibleAutosaveAvailable && !bNewGameConfirmationArmed',
	'bNewGameConfirmationArmed = true;',
	'bNewGameConfirmationArmed = false;',
	'RequestLoadLatestAutosave()',
	'OpenDisplaySettings()',
	'OpenAudioCalibration(true)',
	'CompleteAudioCalibration()',
	'CalibrationCompleted',
	'VolumeStep',
	'BrightnessStep',
	'PlayCalibrationKnock()',
	'Settings->SetFullscreenMode(WindowMode)',
	'Settings->SetScreenResolution(',
	'Settings->SetOverallScalabilityLevel(DisplayQualityIndex == 0 ? 0 : 2)',
	'Settings->SetVSyncEnabled(bDisplayVSync)',
	'Settings->SetFrameRateLimit(',
	'Settings->ApplyResolutionSettings(false)',
	'Settings->ApplyNonResolutionSettings()',
	'Settings->ConfirmVideoMode()',
	'Settings->RevertVideoMode()',
	'Settings->SaveSettings()',
	'DisplayConfirmationDeadline = FPlatformTime::Seconds() + 10.0',
	'PrimaryActorTick.bStartWithTickEnabled = false',
	'PrimaryActorTick.bTickEvenWhenPaused = true',
	'SetActorTickEnabled(true)',
	'SetActorTickEnabled(false)',
	'RevertPendingDisplaySettings()',
	'Params.Key == EKeys::LeftMouseButton',
	'TryGetMenuRowFromPointer(',
	'TryGetSystemMenuRowFromPointer(',
	'IGFrontendMenuLayout::HitTestAction(',
	'FInputModeGameAndUI',
	'OnSaveCompleted.AddUniqueDynamic',
	'OnLoadCompleted.AddUniqueDynamic',
	'저장에 실패했습니다. 저장 공간과 폴더 권한을 확인하세요.',
	'자동 저장을 불러오지 못했습니다. 파일이 손상됐거나 호환되지 않습니다.',
	'UKismetSystemLibrary::QuitGame',
	'EQuitPreference::Quit'
) '타이틀·일시정지·새 게임·이어하기 계약'
Assert-ContainsAll $hudSource @(
	'GetOwningPlayerController()',
	'IndieController->RefreshMenuHud()'
) 'HUD 생성 순서와 메뉴 상태 재동기화'

Assert-ContainsAll $saveHeader @(
	'HasCompatibleAutosave() const',
	'FindNewestCompatibleAutosave(FString& OutSlotName) const'
) '저장 호환성 공개 계약'
Assert-ContainsAll $saveSource @(
	'FindNewestCompatibleAutosave(NewestSlot)',
	'UGameplayStatics::DoesSaveGameExist',
	'UGameplayStatics::LoadGameFromSlot',
	'IsAutosaveLoadable(Candidate)',
	'SaveGame->Progress.ChapterId.IsValid()',
	'!SaveGame->Progress.MapPackageName.IsNone()',
	'SaveGame->Progress.CheckpointTag.IsValid()',
	'Candidate->Progress.SavedAtUtc > NewestTimestamp'
) '최신 호환 자동 저장 선별'
Assert-ContainsAll $saveSource @(
	'bool UIGSaveSubsystem::ClearRotatingAutosavesNow()',
	'!UGameplayStatics::DeleteGameInSlot',
	'Failed to delete autosave slot',
	'Rejected malformed autosave',
	'Save operation failed for slot',
	'Load operation failed for slot'
) '세이브 삭제·비동기 실패 진단'

Assert-ContainsAll $hudHeader @(
	'SetSystemMenuState',
	'DrawSystemMenuPanel',
	'FIGSystemMenuPresentation',
	'bUseTitleBackdrop',
	'DrawDisplaySettingsPanel',
	'DrawAudioCalibrationPanel',
	'bSystemMenuCanContinue'
) 'HUD 프런트엔드 인터페이스'
Assert-ContainsAll $hudSource @(
	'DrawSystemMenuPanel()',
	'없는 층',
	'THE MISSING FLOOR',
	'무영로 · 04:30',
	'이 게임은 헤드폰으로 듣도록 만들어졌다.',
	'소리 · 밝기 보정',
	'노크가 겨우 들리면서',
	'가운데 칸은 겨우 보이고',
	'노크 다시 듣기',
	'이어하기',
	'게임 시작',
	'새 게임',
	'자동 저장을 덮어씁니다.',
	'새 게임 확인',
	'Enter 시작  ·  Esc 취소',
	'최근 자동 저장 불러오기',
	'화면 설정',
	'공포 연출의 가독성과 프레임 안정성을 함께 조정합니다.',
	'카테고리',
	'세부 설정',
	'선택한 항목',
	'플레이 보조',
	'변경 사항',
	'화면 모드',
	'1280 x 720',
	'1920 x 1080',
	'2560 x 1440',
	'그래픽 품질',
	'수직 동기화',
	'프레임 제한',
	# 값을 고르면 그 자리에서 적용되고 저장된다. 화면을 못 보게 만들 수 있는
	# 화면 모드·해상도만 10초 확인을 거친다.
	'바꾸는 즉시 적용되고 저장됩니다',
	'이 설정 유지',
	# 적용하지 않은 변경이라는 개념이 없어졌다. 이 줄은 그냥 돌아가기다.
	'돌아가기',
	'설정을 적용했습니다.',
	'이 화면 설정을 유지할까요? {0}초 뒤 자동으로 되돌립니다.',
	'이 설정 유지',
	'이전 설정으로 되돌리기',
	'마우스 선택',
	'접근성 설정',
	'제작 정보',
	'게임 종료',
	'기획 · 개발    easygap',
	'ambientCG · CC0',
	'Poly Haven · CC0',
	'Copyright 2026 easygap. All rights reserved.'
) '한국어 타이틀·메뉴·크레딧 카피'
Assert-ContainsAll $frontendLayout @(
	'namespace IGFrontendMenuLayout',
	# §9 「밤 5」가 여섯 번째 액션이다. 화면에서는 「이어하기」 바로 밑이지만
	# 액션 목록의 마지막이라, 기존 다섯 액션의 인덱스는 하나도 움직이지 않는다.
	'constexpr int32 ActionCount = 6',
	'constexpr int32 NightFiveAction = 5',
	'constexpr int32 ScreenOrder[ActionCount] = {0, 2, 3, 4, 5, 1}',
	'HidesNightFive',
	'IsActionHidden',
	'MakeMetrics',
	'FMath::Max(44.0f, 54.0f * Result.Scale)',
	'HidesContinue',
	'GetVisibleActionCount',
	'GetVisibleSlotForAction',
	'GetActionForVisibleSlot',
	'HitTestAction',
	'GetRowHitBox',
	'ValidateMetrics'
) '타이틀 렌더링·포인터 공용 레이아웃 계약'
Assert-ContainsAll $hudSource @(
	'/Game/UI/Textures/T_TitleBackground_D.T_TitleBackground_D',
	'FrontendTitleFontSize = 64',
	'KoreanFrontendTitleFont',
	'IsReducedCameraMotionEnabled()',
	'(Now - SystemMenuOpenedAt) / 0.32',
	'IGFrontendMenuLayout::GetVisibleSlotForAction',
	'RecordLayoutValidationRect(HitBox.Min, HitBox.Max)'
) '타이틀 키아트·타이포·동작 감소·포커스 계약'
Assert-ContainsAll $settingsLayout @(
	'FPanelMetrics',
	'MakePanelMetrics',
	'GetDisplayCategory',
	'GetAccessibilityCategory',
	'HitTestSettingsRow',
	'ValidatePanelMetrics',
	'FMath::Max(48.0f, 56.0f * Result.Scale)',
	'FMath::Max(52.0f, 62.0f * Result.Scale)'
) '설정 렌더링·포인터 공용 레이아웃 계약'
Assert-ContainsAll $controllerSource @(
	'TryGetDisplaySettingsRowFromPointer',
	'TryGetAccessibilityRowFromPointer',
	'IGSettingsMenuLayout::HitTestSettingsRow',
	'bFrontendProbeCompilationDrained',
	'FAssetCompilingManager::Get().FinishAllCompilation()',
	'GShaderCompilingManager->FinishAllCompilation()',
	'&& !bCategoryHit',
	'if (!bCategoryHit)'
) '설정 카테고리·옵션 포인터 계약'
Assert-ContainsAll $hudSource @(
	'LoadBundledFontFace(',
	'Pretendard-Regular.otf',
	'Pretendard-SemiBold.otf',
	'GowunBatang-Bold.ttf',
	'GetFittedTextScale(',
	'DrawSettingsDetailText(',
	'DrawSettingsFooterText(',
	'ValidateSettingsTextRect(',
	'bLayoutValidationAllInsideSettingsContainers',
	'WrapHudText(',
	'PreviewLines'
) '번들 한글 타이포·설정 컨테이너 오버플로 계약'
Assert-ContainsAll $moduleRules @(
	'BundledFontFiles',
	'Pretendard-Regular.otf',
	'Pretendard-SemiBold.otf',
	'GowunBatang-Bold.ttf',
	'OFL-Pretendard.txt',
	'OFL-GowunBatang.txt',
	'$(TargetOutputDir)/UI/Fonts/',
	'StagedFileType.NonUFS'
) 'Shipping 한글 폰트·라이선스 스테이징'
Assert-True (
	$hudSource.IndexOf('if (bAccessibilityMenuVisible)') -lt
	$hudSource.IndexOf('if (bSystemMenuVisible)')
) '접근성 설정이 시스템 메뉴보다 위에 그려지지 않는다'

Assert-ContainsAll $inputConfig @(
	'ActionName="PauseMenu"',
	'Key=Escape',
	'Key=Gamepad_Special_Left',
	'ActionName="AccessibilityMenu"',
	'Key=F10',
	'Key=Gamepad_Special_Right',
	'ActionName="AccessibilityUp"',
	'Key=W',
	'ActionName="AccessibilityDown"',
	'Key=S',
	'ActionName="AccessibilityConfirm"',
	'Key=Enter',
	'Key=Gamepad_FaceButton_Bottom'
) '키보드·게임패드 메뉴 매핑'
Assert-True (-not $inputConfig.Contains('ActionName="ToggleCursor"')) `
	'Esc가 배포 빌드에서 커서 토글로 남아 있다'
Assert-True (-not $controllerSource.Contains('ToggleCursorMode')) `
	'컨트롤러에 이전 커서 토글 경로가 남아 있다'

Assert-ContainsAll $gameConfig @(
	'ProjectName=없는 층',
	'ProjectVersion=1.0.0',
	'Description=없는 층 — 소리와 기억, 불법 증축된 한 층을 추적하는 한국형 1인칭 심리 공포 게임.',
	'CompanyName=easygap',
	'Homepage=https://github.com/easygap/Indie-Game',
	'SupportContact=https://github.com/easygap/Indie-Game/issues',
	'CopyrightNotice=Copyright 2026 easygap. All rights reserved.',
	'BuildConfiguration=PPBC_Shipping',
	'FullRebuild=True',
	'ForDistribution=True',
	'UsePakFile=True',
	'bUseIoStore=True',
	'bCompressed=True',
	'+DirectoriesToAlwaysCook=(Path="/Game/UI")',
	'IncludePrerequisites=True',
	'IncludeAppLocalPrerequisites=True',
	'ApplocalPrerequisitesDirectory=(Path="")'
) '기본 Shipping 패키징 설정'
Assert-True (-not $gameConfig.Contains('BuildConfiguration=PPBC_Development')) `
	'기본 패키징 구성이 Development로 되돌아갔다'
Assert-True (-not $gameConfig.Contains('ForDistribution=False')) `
	'배포 플래그가 비활성화되어 있다'
Assert-ContainsAll $gameTarget @(
	'BuildVersion = "1.0.0";',
	'WindowsPlatform.bSetResourceVersions = true;'
) 'Win64 공개 버전 리소스 계약'
$configuredVersion = [regex]::Match(
	$gameConfig,
	# .gitattributes normalizes this repo to CRLF on Windows, and in .NET the
	# multiline $ anchors immediately before \n only. Excluding \r from the
	# capture and then anchoring therefore never matched a CRLF checkout, so
	# the carriage return has to be consumed explicitly.
	'(?m)^ProjectVersion=(?<version>[^\r\n]+)\r?$')
$targetVersion = [regex]::Match(
	$gameTarget,
	'BuildVersion\s*=\s*"(?<version>[^"]+)";')
Assert-True ($configuredVersion.Success -and $targetVersion.Success) `
	'설정 또는 빌드 대상에서 공개 버전을 읽을 수 없다'
Assert-True (
	$configuredVersion.Groups['version'].Value.Trim() -ceq
	$targetVersion.Groups['version'].Value.Trim()
) 'DefaultGame.ini와 Win64 실행 파일 버전 계약이 다르다'
Assert-ContainsAll $projectDescriptor @(
	'"EngineAssociation": "5.8"',
	'"Category": "Games"',
	'"Description": "없는 층 — a Korean first-person psychological horror game about listening, memory, and an illegal floor."'
) '프로젝트 설명자 제품 정보'
Assert-ContainsAll $userSettingsConfig @(
	'[/Script/Engine.GameUserSettings]',
	'bUseVSync=True',
	'bUseDynamicResolution=False',
	'ResolutionSizeX=1920',
	'ResolutionSizeY=1080',
	'FullscreenMode=0',
	'FrameRateLimit=60.000000',
	'Version=5',
	'sg.ResolutionQuality=100.000000',
	'sg.ViewDistanceQuality=2',
	'sg.AntiAliasingQuality=2',
	'sg.ShadowQuality=2',
	'sg.GlobalIlluminationQuality=2',
	'sg.ReflectionQuality=2',
	'sg.PostProcessQuality=2',
	'sg.TextureQuality=2',
	'sg.EffectsQuality=2',
	'sg.FoliageQuality=2',
	'sg.ShadingQuality=2',
	'sg.LandscapeQuality=2'
) '첫 실행 1080p High 60fps 설정'

Assert-ContainsAll $iconScript @(
	'ICON_SIZES = (16, 24, 32, 48, 64, 128, 256)',
	'center_square',
	'Image.Resampling.LANCZOS',
	'ImageEnhance.Contrast',
	'ImageEnhance.Color',
	'ImageFilter.UnsharpMask',
	'format="ICO"'
) 'Windows 아이콘 재현 스크립트'
Assert-ContainsAll $assetPolicy @(
	'Content/SourceArt/AI/ApplicationIcon_raw.png',
	'Build/Windows/ApplicationIcon.png',
	'Build/Windows/Application.ico',
	'Docs/IMAGEGEN_PROMPTS_2026-08-05.md',
	'Pretendard 1.3.9',
	'GowunBatang-Bold.ttf',
	'SIL Open Font License 1.1',
	'Content/SourceArt/AI/TitleBackgroundMissingFloor_v1.png',
	'4831357AA6439F9CF93CC3D5CC4664DDF8EB995D59759E1F319504246CD57D39',
	'/Game/UI/Textures/T_TitleBackground_D'
) '배포 아이콘 출처·라이선스 대장'
Assert-ContainsAll $titlePrompt @(
	'타이틀 배경 — 무영로 새벽 빌라',
	'Content/SourceArt/AI/TitleBackgroundMissingFloor_v1.png',
	'Content/UI/Textures/T_TitleBackground_D.uasset',
	'4831357AA6439F9CF93CC3D5CC4664DDF8EB995D59759E1F319504246CD57D39',
	'leave the left third uncluttered and dark enough for pale runtime text',
	'absolutely no text, numbers, signage, logos, watermark, UI'
) '타이틀 ImageGen 생성·경계·최종 프롬프트 기록'
Assert-ContainsAll $prepareAiArtScript @(
	"Source = 'TitleBackgroundMissingFloor_v1'",
	"Target = 'T_TitleBackground_D.png'",
	'Size = @(1920, 1080)'
) '타이틀 ImageGen 파생 재현 계약'
Assert-ContainsAll $surfaceTextureScript @(
	'FRONTEND_TEXTURE_PACKAGE_ROOT = "/Game/UI/Textures"',
	'FRONTEND_UI_ONLY',
	'FRONTEND_UI_TEXTURE_NAMES',
	'"T_TitleBackground_D"'
) '타이틀 UI 텍스처 임포트 계약'
# §9 에필로그의 세 정지 화면도 같은 UI 경로로 들어간다. 월드 아틀라스에
# 섞이면 화면 전체를 덮는 그림이 밉맵과 스트리밍을 타게 된다.
Assert-ContainsAll $surfaceTextureScript @(
	'"T_EpilogueWorkshop_D"',
	'"T_EpilogueAutumn_D"',
	'"T_EpilogueServiceBay_D"'
) '에필로그 정지 화면 UI 텍스처 임포트 계약'
Assert-ContainsAll $prepareAiArtScript @(
	"Source = 'EpilogueWorkshop_v1'",
	"Source = 'EpilogueAutumn_v1'",
	"Source = 'EpilogueServiceBay_v1'"
) '에필로그 ImageGen 파생 재현 계약'
foreach ($relativePath in @(
	'Content/SourceArt/AI/TitleBackgroundMissingFloor_v1.png',
	'Content/SourceArt/T_TitleBackground_D.png',
	'Content/UI/Textures/T_TitleBackground_D.uasset')) {
	Assert-True (Test-Path -LiteralPath (Join-Path $projectRoot $relativePath)) `
		"타이틀 에셋이 없다: $relativePath"
}
Assert-ContainsAll $iconPrompt @(
	'Windows application icon',
	'4시 44분',
	'no text, no numbers, no letters',
	'16, 24, 32, 48, 64, 128 and 256 pixel'
) '배포 아이콘 최종 프롬프트·검수 기록'
Assert-True ($iconPngBytes.Length -gt 100000) '등급 PNG가 비었거나 지나치게 작다'
Assert-True (
	$iconPngBytes[0] -eq 0x89 -and
	$iconPngBytes[1] -eq 0x50 -and
	$iconPngBytes[2] -eq 0x4E -and
	$iconPngBytes[3] -eq 0x47
) '등급 아이콘이 PNG 형식이 아니다'
Assert-True (
	($iconPngBytes[16..19] -join ',') -eq '0,0,4,0' -and
	($iconPngBytes[20..23] -join ',') -eq '0,0,4,0'
) '등급 PNG가 1024x1024가 아니다'
Assert-True (
	$iconIcoBytes[0] -eq 0 -and
	$iconIcoBytes[1] -eq 0 -and
	$iconIcoBytes[2] -eq 1 -and
	$iconIcoBytes[3] -eq 0
) 'Application.ico 헤더가 유효하지 않다'
$iconEntryCount = [BitConverter]::ToUInt16($iconIcoBytes, 4)
Assert-True ($iconEntryCount -eq 7) "ICO 레벨 수 오류: $iconEntryCount"
$actualIconSizes = @()
for ($entry = 0; $entry -lt $iconEntryCount; $entry++) {
	$entryOffset = 6 + (16 * $entry)
	$entryWidth = if ($iconIcoBytes[$entryOffset] -eq 0) {
		256
	}
	else {
		[int]$iconIcoBytes[$entryOffset]
	}
	$entryHeight = if ($iconIcoBytes[$entryOffset + 1] -eq 0) {
		256
	}
	else {
		[int]$iconIcoBytes[$entryOffset + 1]
	}
	Assert-True ($entryWidth -eq $entryHeight) `
		"ICO 레벨이 정사각형이 아니다: ${entryWidth}x${entryHeight}"
	Assert-True (
		[BitConverter]::ToUInt16($iconIcoBytes, $entryOffset + 6) -eq 32
	) "ICO 레벨이 32-bit가 아니다: $entryWidth"
	$actualIconSizes += $entryWidth
}
Assert-True (($actualIconSizes -join ',') -eq '16,24,32,48,64,128,256') `
	"ICO 크기 목록 오류: $($actualIconSizes -join ',')"
Assert-ContainsAll $executableIconScript @(
	'[System.Drawing.Icon]::ExtractAssociatedIcon',
	'$expectedBitmap.GetPixel($x, $y).ToArgb() -ne',
	'$actualBitmap.GetPixel($x, $y).ToArgb()',
	'matched_pixels=$matchedPixels',
	'evidence_sha256=$evidenceHash'
) 'Shipping 실행 파일 아이콘 픽셀 동등성 검증'
Assert-ContainsAll $executableMetadataSyncScript @(
	'BeginUpdateResource',
	'UpdateResource',
	'EndUpdateResource',
	'LoadLibraryAsImageResource',
	'ReplaceUtf16Value',
	'OriginalFilename',
	'WINDOWS_EXECUTABLE_METADATA_SYNC PASS'
) 'Shipping 루트 런처 VERSIONINFO 동기화'
Assert-ContainsAll $executableMetadataScript @(
	"ExpectedProductName = '없는 층'",
	"ExpectedVersion = '1.0.0'",
	"ExpectedCompanyName = 'easygap'",
	'FileDescription',
	'FileVersion',
	'ProductName',
	'ProductVersion',
	'CompanyName',
	'LegalCopyright',
	'InternalName',
	'OriginalFilename',
	'Engine build metadata leaked',
	'WINDOWS_EXECUTABLE_METADATA PASS'
) 'Shipping 실행 파일 제품 정보 검증'
Assert-ContainsAll $thirdMorningHeader @(
	'WriteRebirthReleaseValidationResult'
) 'Shipping 종단 결과 기록 선언'
Assert-ContainsAll $thirdMorningSource @(
	'TEXT("IGRebirthResultPath=")',
	'"REBIRTH_PACKAGED_RUNTIME PASS contract=1 ending=%s "',
	'"REBIRTH_PACKAGED_RUNTIME FAIL contract=1 ending=%s "',
	'FFileHelper::SaveStringToFile(',
	'TEXT("result_evidence_write")'
) 'Shipping 종단 결과 기록 계약'
Assert-ContainsAll $releaseValidationScript @(
	'function Invoke-RebirthShippingRuntimeCase',
	'$_.Name -ieq ''IndieGame-Win64-Shipping.exe''',
	'"-IGRebirthResultPath=$resultPath"',
	'"-UserDir=$userDirectory"',
	"'shipping_runtime_ending_a'",
	"'shipping_runtime_ending_b'",
	'$resultText -cne $expectedResult',
	'shippingRuntimeResults = [pscustomobject]$shippingRuntimeResults',
	'function Assert-ShippingArchiveManifestUnchanged',
	'accessibilityScreenshotSha256',
	'displayScreenshotSha256',
	'$result.layoutSamples -ne 11',
	"'shipping_archive_post_runtime'",
	'Shipping manifest path escaped the archive',
	'Shipping archive file hash changed after runtime',
	'Assert-ShippingArchiveManifestUnchanged',
	'if (-not $SkipShippingPackage -and -not $SkipRuntimeValidation)'
) 'Shipping 패키지 A/B 실제 실행 영수증 검증'
Assert-ContainsAll $frontendProbeScript @(
	'-FilePath $launcher',
	'-WorkingDirectory (Split-Path -Parent $launcher)',
	'IGFrontendAccessibilityScreenshotPath=',
	'IGFrontendDisplayScreenshotPath=',
	'IGFrontendTitleScreenshotPath=',
	'settings-accessibility.png',
	'settings-display.png',
	'title-first-run.png',
	'accessibilityScreenshotSha256',
	'displayScreenshotSha256',
	'titleScreenshotSha256',
	'Frontend Shipping settings screenshot dimensions failed'
) 'Shipping 설정 화면 시각 증거'

$forbiddenCreditCodePoints = @(
	@(67, 104, 97, 116, 71, 80, 84),
	@(67, 111, 100, 101, 120),
	@(79, 112, 101, 110, 65, 73),
	@(99, 111, 119, 111, 114, 107, 101, 114),
	@(99, 111, 110, 116, 114, 105, 98, 117, 116, 111, 114)
)
foreach ($codePoints in $forbiddenCreditCodePoints) {
	$forbiddenCredit = -join @($codePoints | ForEach-Object { [char]$_ })
	Assert-True (-not $hudSource.Contains($forbiddenCredit)) `
		"플레이어 크레딧에 금지된 제작자 표기가 있다: $forbiddenCredit"
}

foreach ($height in @(720.0, 900.0, 1080.0, 1440.0)) {
	$width = $height * (16.0 / 9.0)
	$menuScale = [Math]::Min(2.0, [Math]::Max(0.67, [Math]::Min(
		$width / 1920.0,
		$height / 1080.0)))
	$rowStart = [Math]::Max(282.0 * $menuScale, $height * 0.34)
	$rowHeight = [Math]::Max(44.0, 54.0 * $menuScale)
	$rowGap = 8.0 * $menuScale
	$rowStride = $rowHeight + $rowGap
	$lastMenuBottom = $rowStart + (4.0 * $rowStride) + $rowHeight
	$footerTop = $height - [Math]::Max(38.0, 44.0 * $menuScale)
	Assert-True ($lastMenuBottom -lt $footerTop) `
		"시스템 메뉴와 조작 안내가 겹친다: height=$height"
	Assert-True ($rowHeight -ge 44.0) `
		"시스템 메뉴 포인터 목표가 44px 미만이다: height=$height"

	$supportScale = [Math]::Max(0.90, $menuScale)
	$creditStart = $rowStart
	$creditSpacing = 34.0 * $supportScale
	$lastCreditBottom = $creditStart + (4.0 * $creditSpacing) + (22.0 * $supportScale)
	Assert-True ($lastCreditBottom -lt $footerTop) `
		"크레딧과 돌아가기 안내가 겹친다: height=$height"

	$scale = [Math]::Min(2.0, [Math]::Max(0.85, [Math]::Min(
		$width / 1920.0,
		$height / 1080.0)))
	$verticalMargin = [Math]::Max(20.0, 28.0 * $scale)
	$panelHeight = [Math]::Min(
		[Math]::Max(420.0, $height - (2.0 * $verticalMargin)),
		800.0 * $scale)
	$panelTop = ($height - $panelHeight) * 0.5
	$headerBottom = $panelTop + 108.0 * $scale
	$settingsFooterTop = $panelTop + $panelHeight - 64.0 * $scale
	$optionStart = $headerBottom + 56.0 * $scale
	$optionSpacing = [Math]::Max(52.0, 62.0 * $scale)
	$lastOptionBottom = $optionStart + 5.0 * $optionSpacing
	Assert-True ($lastOptionBottom -lt $settingsFooterTop) `
		"설정 옵션과 패널 조작 안내가 겹친다: height=$height"
}

Write-Host (
	"REBIRTH_FRONTEND_CONTRACT PASS assertions=$assertionCount " +
	"title=1 pause=1 settings=1 mouse=1 save_failure_feedback=1 continue=1 new_game_reset=1 credits=1 quit=1 shipping_defaults=1 unattended_bypass=1 layout_profiles=4 icon_levels=7 exe_icon_verifier=1 exe_metadata_verifier=1 bootstrap_metadata_sync=1 packaged_runtime_receipt=1 packaged_runtime_harness=1 archive_immutability=1"
) -ForegroundColor Green
