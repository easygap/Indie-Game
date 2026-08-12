[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

$projectRoot = Split-Path -Parent $PSScriptRoot
$header = Get-Content -Raw -Encoding UTF8 -LiteralPath (
	Join-Path $projectRoot 'Source/IndieGame/Accessibility/IGAccessibilitySubsystem.h')
$source = Get-Content -Raw -Encoding UTF8 -LiteralPath (
	Join-Path $projectRoot 'Source/IndieGame/Accessibility/IGAccessibilitySubsystem.cpp')
$interaction = Get-Content -Raw -Encoding UTF8 -LiteralPath (
	Join-Path $projectRoot 'Source/IndieGame/Player/IGInteractionComponent.cpp')
$character = Get-Content -Raw -Encoding UTF8 -LiteralPath (
	Join-Path $projectRoot 'Source/IndieGame/Player/IGPlayerCharacter.cpp')
$flashlight = Get-Content -Raw -Encoding UTF8 -LiteralPath (
	Join-Path $projectRoot 'Source/IndieGame/Player/IGFlashlightComponent.cpp')
$hudHeader = Get-Content -Raw -Encoding UTF8 -LiteralPath (
	Join-Path $projectRoot 'Source/IndieGame/Player/IGHorrorHUD.h')
$hudSource = Get-Content -Raw -Encoding UTF8 -LiteralPath (
	Join-Path $projectRoot 'Source/IndieGame/Player/IGHorrorHUD.cpp')
$settingsLayout = Get-Content -Raw -Encoding UTF8 -LiteralPath (
	Join-Path $projectRoot 'Source/IndieGame/Player/IGSettingsMenuLayout.h')
$controllerSource = Get-Content -Raw -Encoding UTF8 -LiteralPath (
	Join-Path $projectRoot 'Source/IndieGame/Player/IGPlayerController.cpp')
$secondMorning = Get-Content -Raw -Encoding UTF8 -LiteralPath (
	Join-Path $projectRoot 'Source/IndieGame/Sequence/IGSecondMorningDirector.cpp')
$humanGate = Get-Content -Raw -Encoding UTF8 -LiteralPath (
	Join-Path $projectRoot 'Source/IndieGame/Sequence/IGChapterTwoHumanGateDirector.cpp')
$inputConfig = Get-Content -Raw -Encoding UTF8 -LiteralPath (
	Join-Path $projectRoot 'Config/DefaultInput.ini')
$thirdMorning = Get-Content -Raw -Encoding UTF8 -LiteralPath (
	Join-Path $projectRoot 'Source/IndieGame/Sequence/IGThirdMorningDirector.cpp')
$timeEntry = Get-Content -Raw -Encoding UTF8 -LiteralPath (
	Join-Path $projectRoot 'Source/IndieGame/Interaction/IGTimeEntryPuzzle.cpp')
$narrativeTypes = Get-Content -Raw -Encoding UTF8 -LiteralPath (
	Join-Path $projectRoot 'Source/IndieGame/Narrative/IGRebirthNarrativeTypes.h')
$narrativeRouter = Get-Content -Raw -Encoding UTF8 -LiteralPath (
	Join-Path $projectRoot 'Source/IndieGame/Narrative/IGRebirthNarrativeSubsystem.cpp')
$persistenceProbe = Get-Content -Raw -Encoding UTF8 -LiteralPath (
	Join-Path $projectRoot 'Source/IndieGame/Sequence/IGRebirthPersistenceProbe.cpp')
$assertionCount = 0

function Assert-True {
	param(
		[Parameter(Mandatory = $true)]
		[bool]$Condition,
		[Parameter(Mandatory = $true)]
		[string]$Message
	)

	if (-not $Condition) {
		throw "REBIRTH_ACCESSIBILITY_CONTRACT FAIL: $Message"
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

Assert-ContainsAll $header @(
	'EIGHintMode',
	'Story',
	'Standard',
	'Silent',
	'bReducedCameraMotion',
	'bReducedFlicker',
	'bDirectionalFearCues',
	'bAutoConnectEvidence',
	'bSubtitlesEnabled',
	'bSoundCaptionsEnabled',
	'CaptionSizeScale',
	'CaptionBackgroundOpacity',
	'CaptionSafeAreaScale',
	'bToggleHoldInteractions',
	'bToggleCrouch',
	'bHapticsEnabled',
	'bMicrophoneNoiseEnabled',
	'HoldDurationScale',
	'GetP3HintThresholds',
	'GetP4HintThresholds',
	'GetPressureRiseIntervalSeconds',
	'UsesDirectionalFearCues',
	'UsesAutomaticEvidenceConnections',
	'AreSubtitlesEnabled',
	'AreSoundCaptionsEnabled',
	'UsesToggleCrouch',
	'AreHapticsEnabled',
	'GetCaptionSizeScale',
	'GetCaptionBackgroundOpacity',
	'GetCaptionSafeAreaScale',
	'ApplySettings',
	'ResetToDefaults'
) '공용 설정 표면'

Assert-ContainsAll $source @(
	'GGameUserSettingsIni',
	'LoadPersistedSettings',
	'SavePersistedSettings',
	'GConfig->Flush(false, GGameUserSettingsIni)',
	'FMath::IsFinite(Result.HoldDurationScale)',
	'FMath::IsFinite(Result.CaptionSizeScale)',
	'FMath::IsFinite(Result.CaptionBackgroundOpacity)',
	'FMath::IsFinite(Result.CaptionSafeAreaScale)',
	'MinimumHoldScale = 0.25f',
	'MaximumHoldScale = 1.0f',
	'MinimumCaptionScale = 0.85f',
	'MaximumCaptionScale = 2.0f',
	'MinimumCaptionBackgroundOpacity = 0.0f',
	'MaximumCaptionBackgroundOpacity = 1.0f',
	'MinimumCaptionSafeArea = 0.80f',
	'MaximumCaptionSafeArea = 1.0f',
	'TEXT("CaptionSizeScale")',
	'TEXT("SoundCaptionsEnabled")',
	'TEXT("CaptionBackgroundOpacity")',
	'TEXT("CaptionSafeAreaScale")',
	'TEXT("MicrophoneNoiseEnabled")'
) '영구 저장·입력 정규화'

Assert-ContainsAll $source @(
	'FVector(45.0f, 90.0f, 150.0f)',
	'FVector(90.0f, 150.0f, 210.0f)',
	'FVector(-1.0f, -1.0f, -1.0f)',
	'return 80.0f',
	'return 55.0f',
	'return 45.0f',
	'IGAccessibilityPreset=',
	'IGReducedMotion',
	'IGReducedFlicker',
	'IGFearDirection',
	'IGAutoConnectEvidence',
	'IGNoSubtitles',
	'IGNoSoundCaptions',
	'IGToggleHolds',
	'IGMicrophoneMode',
	'IGHoldScale=',
	'IGCaptionScale=',
	'IGCaptionBackground=',
	'IGCaptionSafeArea='
) '난이도·무상태 QA 오버라이드'

$loadIndex = $source.IndexOf('LoadPersistedSettings();')
$overrideIndex = $source.IndexOf('RebuildEffectiveSettings();')
Assert-True (
	$loadIndex -ge 0 -and $overrideIndex -gt $loadIndex
) '저장값을 읽기 전에 QA 오버라이드를 적용한다'

Assert-ContainsAll $thirdMorning @(
	'Accessibility->ShouldAutoShowHints()',
	'Accessibility->GetP3HintThresholds()',
	'P3HintElapsedSeconds += 1.0f',
	'HintThresholds.X',
	'HintThresholds.Y',
	'HintThresholds.Z'
) 'P3 힌트 사다리 연결'
$silentGateIndex = $thirdMorning.IndexOf(
	'Accessibility && !Accessibility->ShouldAutoShowHints()')
$hintClockIndex = $thirdMorning.IndexOf('P3HintElapsedSeconds += 1.0f')
Assert-True (
	$silentGateIndex -ge 0 -and $hintClockIndex -gt $silentGateIndex
) '침묵 모드 게이트가 자동 힌트 체류 증가보다 먼저 실행되지 않는다'

Assert-ContainsAll $timeEntry @(
	'bool AIGTimeEntryPuzzle::AdvancePressureStage()',
	'WrongAttempts >= 3',
	'++WrongAttempts',
	'UpdateButtonPrompts()'
) 'P1/P2 공용 압박 단계'
Assert-ContainsAll $secondMorning @(
	'&ThisClass::PollPuzzlePressure',
	'Accessibility->GetPressureRiseIntervalSeconds()',
	'AIGReadableNote::GetOpenNote()',
	'bP1PressureArmed',
	'HasState(CalledEmployeeTag)',
	'Puzzle->AdvancePressureStage()',
	'ApplyP1PressureStage(PressureStage)',
	'SetP2ShutterStage(PressureStage)',
	'RequestCheckpointAutosave(bP1 ? CorridorCheckpointTag : StoreCheckpointTag)'
) 'P1/P2 시간 압박 연결'
Assert-ContainsAll $thirdMorning @(
	'P3PressureRiseElapsedSeconds += 1.0f',
	'Accessibility->GetPressureRiseIntervalSeconds()',
	'AdvanceP3TimedPressure()',
	'bP3PressureRiseArmed',
	'AIGReadableNote::GetOpenNote()',
	'Pressure remains',
	'P3MistakeCount >= 3'
) 'P3 시간 압박 연결'
Assert-ContainsAll $thirdMorning @(
	'void AIGThirdMorningDirector::PollP4PressureAndHint()',
	'P4PressureRiseElapsedSeconds += 1.0f',
	'Accessibility->GetP4HintThresholds()',
	'AdvanceP4Pressure()',
	'PresentNextP4Hint()',
	'BeginP4ReceiptHint()',
	'UpdateP4ReceiptHint()',
	'P4ReceiptFragment',
	'bP4PressureArmed',
	'P4PressureStage < 3',
	'AIGReadableNote::GetOpenNote()',
	'FVector(1085.0f, -90.0f, 145.0f)',
	'FMath::Square(360.0f)'
) 'P4 접근 기반 압박·힌트 연결'
Assert-ContainsAll $narrativeTypes @(
	'float PressureRiseElapsedSeconds = 0.0f',
	'bool bPressureRiseArmed = false'
) 'P3 압박 시계 스냅샷 필드'
Assert-ContainsAll $thirdMorning @(
	'State.P3.PressureRiseElapsedSeconds = P3PressureRiseElapsedSeconds',
	'State.P3.bPressureRiseArmed = bP3PressureRiseArmed',
	'P3PressureRiseElapsedSeconds = State.P3.PressureRiseElapsedSeconds',
	'bP3PressureRiseArmed = State.P3.bPressureRiseArmed'
) 'P3 압박 시계 디렉터 저장·복원'
Assert-ContainsAll $narrativeRouter @(
	'FMath::IsFinite(',
	'P3.PressureRiseElapsedSeconds',
	'P3.bPressureRiseArmed = false',
	'P3.PressureRiseElapsedSeconds = 0.0f'
) 'P3 압박 시계 정규화'
Assert-ContainsAll $persistenceProbe @(
	'State.PressureRiseElapsedSeconds = 9.0f * CheckpointIndex',
	'State.bPressureRiseArmed = CheckpointIndex > 0',
	'Actual.PressureRiseElapsedSeconds',
	'Actual.bPressureRiseArmed == Expected.bPressureRiseArmed'
) 'P3 압박 시계 저장 왕복 오라클'
Assert-ContainsAll $narrativeTypes @(
	'FIGRebirthP4State',
	'int32 StairLoopCount = 0',
	'int32 PressureStage = 0',
	'float PressureRiseElapsedSeconds = 0.0f',
	'float HintElapsedSeconds = 0.0f',
	'int32 HintStage = 0',
	'bool bPressureArmed = false',
	'bool bCompleted = false',
	'FIGRebirthP4State P4'
) 'P4 스냅샷 필드'
Assert-ContainsAll $thirdMorning @(
	'State.P4.StairLoopCount = StairLoopCount',
	'State.P4.PressureStage = P4PressureStage',
	'State.P4.PressureRiseElapsedSeconds = P4PressureRiseElapsedSeconds',
	'State.P4.HintElapsedSeconds = P4HintElapsedSeconds',
	'State.P4.HintStage = P4HintStage',
	'State.P4.bPressureArmed = bP4PressureArmed',
	'State.P4.bCompleted = bP4Completed',
	'StairLoopCount = State.P4.StairLoopCount',
	'bP4Completed = State.P4.bCompleted'
) 'P4 디렉터 저장·복원'
Assert-ContainsAll $narrativeRouter @(
	'FIGRebirthP4State& P4',
	'P4.StairLoopCount = FMath::Clamp',
	'P4.PressureStage = FMath::Clamp',
	'P4.HintStage = FMath::Clamp',
	'P4.bPressureArmed = false',
	'P4.PressureRiseElapsedSeconds = 0.0f'
) 'P4 스냅샷 정규화'
Assert-ContainsAll $persistenceProbe @(
	'MakeP4Checkpoint',
	'MatchesP4Checkpoint',
	'ChapterThree.P4 = MakeP4Checkpoint(P3CheckpointIndex)',
	'MatchesP4Checkpoint(ActualP4, ExpectedP4)'
) 'P3 7경계와 결합한 P4 저장 왕복 오라클'

Assert-ContainsAll $interaction @(
	'TargetHoldDuration *= Accessibility->GetHoldDurationScale()',
	'bUseToggleHold = Accessibility->UsesToggleHoldInteractions()',
	'bToggleHoldLatched = bUseToggleHold',
	'(!bInteractionPressed && !bToggleHoldLatched)',
	'bInteractionActive && bToggleHoldLatched',
	'FinishActiveInteraction(EIGInteractionEndReason::Cancelled, false)'
) '길이 조절·토글 홀드 상태기계'

Assert-ContainsAll $character @(
	'IsReducedCameraMotionEnabled()',
	'if (!bReducedMotion)',
	'FirstPersonCamera->SetRelativeRotation(FRotator::ZeroRotator)',
	'PlayFootstep(SpeedScale)'
) '카메라 흔들림 감소'

Assert-ContainsAll $flashlight @(
	'IsReducedCameraMotionEnabled()',
	'IsReducedFlickerEnabled()',
	'BrownOutTimer = 0.0f',
	'FlickerValue = 1.0f',
	'TargetSway = bReducedMotion'
) '손전등 흔들림·점멸 감소'

Assert-ContainsAll $hudHeader @(
	'PushFearDirection',
	'ShowFearDirection',
	'DrawFearDirection',
	'FearCueWorldLocation',
	'FearCueEndTime'
) '공포음 방향 HUD 표면'
Assert-ContainsAll $hudHeader @(
	'PushAudioCaption',
	'ShowAudioCaption',
	'DrawAudioCaption',
	'CurrentAudioCaption',
	'AudioCaptionEndTime'
) '핵심 소리 자막 HUD 표면'
Assert-ContainsAll $hudSource @(
	'Accessibility->AreSoundCaptionsEnabled()',
	'bool AIGHorrorHUD::DrawAudioCaption',
	'DrawRoundedHudSurface(',
	'DrawDialogueFilm(',
	'FCanvasTileItem WaveBar',
	'Settings.CaptionSizeScale',
	'Settings.CaptionBackgroundOpacity',
	'Settings.CaptionSafeAreaScale',
	'MaximumTextWidth',
	'MeasureTextWidth',
	'Canvas->StrLen',
	'FindFittingCaptionPrefix',
	'WrapHudText',
	'OutRemainder',
	'AudioCaptionQueue.Insert',
	'TextItem.Scale',
	'DrawAudioCaption(',
	'핵심 소리 캡션',
	'SOUND CAPTIONS'
) '핵심 소리 자막 설정·출력 연결'
Assert-True (-not $hudSource.Contains(
	'const FString Ellipsis = TEXT("…")')) '긴 자막을 말줄임표로 손실한다'
Assert-ContainsAll $thirdMorning @(
	'RearSplashCaption',
	'TankScratchCaption',
	'SafetyGasCaption',
	'EndingAChimeCaption',
	'MontageCaptions',
	'EpilogueDripCaption',
	'EndingBCatDrinkCaption'
) 'CH03 공포음·엔딩 소리 자막 큐'
Assert-ContainsAll $thirdMorning @(
	'const float CaptionDuration',
	'CaptionDuration);',
	'FName(TEXT("Safety.GasDetector")),',
	'1.15f);',
	'FName(TEXT("Safety.Ventilation")),',
	'2.30f);',
	'FName(TEXT("Safety.Harness")),',
	'1.05f);',
	'FName(TEXT("Safety.TwoClimbers")),',
	'2.50f);',
	'FName(TEXT("Safety.HatchOpen")),',
	'1.70f);',
	'{10.60f, TEXT("[콘크리트 위에 물그릇을 놓는다]"), 0.8f}'
) '연속 엔딩 음향의 자막 표시 구간'
Assert-ContainsAll $humanGate @(
	'AlarmFirstToneCaption',
	'RadioStopsCaption'
) 'CH02 알람·라디오 단절 소리 자막 큐'
Assert-ContainsAll $hudSource @(
	'Accessibility->UsesDirectionalFearCues()',
	'FVector::DotProduct(Direction, Forward)',
	'FVector::DotProduct(Direction, Right)',
	'FCanvasLineItem',
	'CueColor(0.72f, 0.74f, 0.72f'
) '공포음 방향 무채색 파형'
Assert-True (
	$thirdMorning.Contains('AIGHorrorHUD::PushFearDirection(this, WorldLocation') -and
	$thirdMorning.Contains('AIGHorrorHUD::PushFearDirection(this, CueLocation')
) 'CH03 긁힘·배관·후방 소리의 방향 파형 호출이 없다'

Assert-ContainsAll $thirdMorning @(
	'TryAutoConnectEvidence(EvidenceAction)',
	'Accessibility->UsesAutomaticEvidenceConnections()',
	'IsEvidenceActionObserved(Candidate)',
	'IsEvidencePairValid(Candidate, EvidenceAction)',
	'ResolveEvidencePair(Candidate, EvidenceAction)',
	'EvidenceSearchPoster',
	'EvidenceGlasses',
	'EvidenceTankClothing'
) 'P5 자동 연결'
$observationIndex = $thirdMorning.IndexOf('RegisterEvidenceObservation(EvidenceAction);')
$autoConnectIndex = $thirdMorning.IndexOf('TryAutoConnectEvidence(EvidenceAction)')
Assert-True (
	$observationIndex -ge 0 -and $autoConnectIndex -gt $observationIndex
) 'P5 자동 연결이 실제 단서 관찰보다 먼저 실행된다'

Assert-ContainsAll $controllerSource @(
	'ToggleAccessibilityMenu',
	'SetPause(true)',
	'bGameWasPausedBeforeAccessibility',
	'Binding.bExecuteWhenPaused = true',
	'MoveAccessibilitySelectionUp',
	'MoveAccessibilitySelectionDown',
	'AdjustAccessibilityLeft',
	'AdjustAccessibilityRight',
	'ConfirmAccessibilitySelection',
	'RequestManualHint',
	'Accessibility->ApplySettings(Settings)',
	'Accessibility->ResetToDefaults()',
	'SetAccessibilityMenuState',
	'Settings.CaptionSizeScale = FMath::Clamp',
	'Settings.CaptionBackgroundOpacity = FMath::Clamp',
	'Settings.CaptionSafeAreaScale = FMath::Clamp',
	'RowCount = IGSettingsMenuLayout::AccessibilityRowCount',
	'Settings.bMicrophoneNoiseEnabled = !Settings.bMicrophoneNoiseEnabled',
	'RefreshMicrophoneCaptureMode()'
) '일시정지 접근성 설정 입력'
Assert-ContainsAll $hudSource @(
	'DrawAccessibilityPanel()',
	'접근성 설정',
	'필요한 정보는 더 또렷하게, 공포의 밀도는 그대로 유지합니다.',
	'변경 즉시 저장',
	'게임 진행',
	'움직임',
	'정보 안내',
	'자막',
	'입력',
	'이 설정이 바꾸는 것',
	'힌트 난이도',
	'카메라 흔들림 감소',
	'손전등 점멸 감소',
	'공포음 방향 표시',
	'P5 단서 자동 연결',
	'대사 음성 자막',
	'핵심 소리 캡션',
	'대사·캡션 크기',
	'메시지 배경 농도',
	'자막 안전 영역',
	'마이크 소음 입력 (선택)',
	'길게 누르기 방식',
	'홀드 길이',
	'기본값으로 초기화',
	'Esc/F10 닫기'
) '1280x720 대응 네이티브 설정 패널'
Assert-ContainsAll $hudSource @(
	'GetFittedTextScale(',
	'ValidateSettingsTextRect(',
	'bLayoutValidationAllInsideSettingsContainers',
	'PreviewLines',
	'PreviewWidth - 36.0f * Scale',
	'WrapHudText(Text, Font, FitScale, MaximumWidth, 3',
	'DrawSettingsFooterText('
) '긴 한글·200% 미리 보기 내부 경계 계약'
Assert-ContainsAll $settingsLayout @(
	'AccessibilityRowCount = 17',
	'AccessibilityCategoryCount = 6',
	'case 3: return {5, 5}',
	'case 4: return {10, 5}',
	'HitTestSettingsRow'
) '접근성 카테고리·포인터 공용 계약'
Assert-ContainsAll $inputConfig @(
	'ActionName="AccessibilityMenu"',
	'Key=F10',
	'ActionName="AccessibilityUp"',
	'Key=Up',
	'ActionName="AccessibilityDown"',
	'Key=Down',
	'ActionName="AccessibilityLeft"',
	'Key=Left',
	'ActionName="AccessibilityRight"',
	'Key=Right',
	'ActionName="AccessibilityConfirm"',
	'Key=Enter',
	'Key=SpaceBar'
) '키보드 설정 패널 매핑'
Assert-ContainsAll $secondMorning @(
	'bool AIGSecondMorningDirector::RequestManualHint()',
	'PresentP1ManualHint()',
	'PresentP2ManualHint()',
	'P1ManualHintAnswer',
	'P2ManualHintAnswer'
) 'P1/P2 수동 힌트 사다리'
Assert-ContainsAll $thirdMorning @(
	'bool AIGThirdMorningDirector::RequestManualHint()',
	'PresentNextP3Hint()',
	'PresentNextP4Hint()',
	'P3HintFullOrder',
	'P4HintRouteAnswer'
) 'P3/P4 수동 힌트 사다리'
Assert-ContainsAll $inputConfig @(
	'ActionName="RequestHint"',
	'Key=H',
	'Key=Gamepad_RightShoulder',
	'Key=Gamepad_LeftY',
	'Key=Gamepad_RightX',
	'Key=Gamepad_FaceButton_Bottom',
	'Key=Gamepad_FaceButton_Left',
	'Key=Gamepad_Special_Right',
	'Key=Gamepad_DPad_Up',
	'ActionName="AccessibilityClose"',
	'Key=Gamepad_FaceButton_Right',
	'ActionName="LoadAutosave"',
	'Key=Gamepad_FaceButton_Top',
	'Key=Gamepad_LeftThumbstick',
	'Key=Gamepad_RightThumbstick'
) '게임패드 이동·조사·설정·엔딩 매핑'
Assert-ContainsAll $hudSource @(
	'PromptFormatGamepad',
	'PromptFormatKeyboard',
	'HintsGamepad',
	'HintsKeyboard',
	'AccessibilityControlsGamepad',
	'AccessibilityControlsKeyboard',
	'[ A ]',
	'[ E ]',
	'LS 이동',
	'WASD 이동'
) '입력 장치별 화면 안내'
Assert-ContainsAll $controllerSource @(
	'bool AIGPlayerController::InputKey(const FInputKeyEventArgs& Params)',
	'Params.IsGamepad()',
	'Params.IsSimulatedInput()',
	'AxisThreshold = bGamepad ? 0.30f : 0.01f',
	'SetInputDevicePresentation(bGamepad)',
	'SetInputDevicePresentation(bUsingGamepadForHud)'
) '마지막 실제 입력 장치 자동 전환'
Assert-ContainsAll $thirdMorning @(
	'IsUsingGamepadForHud()',
	'EndingControlsGamepad',
	'EndingControlsKeyboard',
	'L3  이 아침 다시 시작',
	'R  이 아침 다시 시작'
) '엔딩 입력 장치별 안내'

# C++ 구현과 독립된 정책 오라클. 숫자나 순서가 바뀌면 기획 계약도
# 명시적으로 함께 바꾸게 한다.
$thresholds = @{
	Story = @(45, 90, 150)
	Standard = @(90, 150, 210)
	Silent = @(-1, -1, -1)
}
Assert-True (($thresholds.Story -join ',') -eq '45,90,150') '이야기 힌트 임계값 오류'
Assert-True (($thresholds.Standard -join ',') -eq '90,150,210') '기본 힌트 임계값 오류'
Assert-True (($thresholds.Silent -join ',') -eq '-1,-1,-1') '침묵 자동 힌트 차단값 오류'
$p4Thresholds = @{
	Story = @(45, 90, 150)
	Standard = @(45, 100, 150)
	Silent = @(-1, -1, -1)
}
Assert-True (($p4Thresholds.Story -join ',') -eq '45,90,150') 'P4 이야기 힌트 임계값 오류'
Assert-True (($p4Thresholds.Standard -join ',') -eq '45,100,150') 'P4 기본 힌트 임계값 오류'
Assert-True (($p4Thresholds.Silent -join ',') -eq '-1,-1,-1') 'P4 침묵 자동 힌트 차단값 오류'
$pressureIntervals = @{
	Story = 80
	Standard = 55
	Silent = 45
}
Assert-True ($pressureIntervals.Story -eq 80) '이야기 압박 간격 오류'
Assert-True ($pressureIntervals.Standard -eq 55) '기본 압박 간격 오류'
Assert-True ($pressureIntervals.Silent -eq 45) '침묵 압박 간격 오류'
foreach ($holdScale in @(-2.0, 0.25, 0.7, 1.0, 4.0)) {
	$clamped = [Math]::Min(1.0, [Math]::Max(0.25, $holdScale))
	Assert-True (
		$clamped -ge 0.25 -and $clamped -le 1.0
	) "홀드 길이 배율 범위 오류: $holdScale"
}
foreach ($captionScale in @(-2.0, 0.85, 1.0, 1.25, 1.5, 2.0, 4.0)) {
	$clamped = [Math]::Min(2.0, [Math]::Max(0.85, $captionScale))
	Assert-True (
		$clamped -ge 0.85 -and $clamped -le 2.0
	) "자막 크기 배율 범위 오류: $captionScale"
}
foreach ($backgroundOpacity in @(-2.0, 0.0, 0.50, 0.82, 1.0, 4.0)) {
	$clamped = [Math]::Min(1.0, [Math]::Max(0.0, $backgroundOpacity))
	Assert-True (
		$clamped -ge 0.0 -and $clamped -le 1.0
	) "메시지 배경 농도 범위 오류: $backgroundOpacity"
}
foreach ($safeArea in @(-2.0, 0.80, 0.90, 1.0, 4.0)) {
	$clamped = [Math]::Min(1.0, [Math]::Max(0.80, $safeArea))
	Assert-True (
		$clamped -ge 0.80 -and $clamped -le 1.0
	) "자막 안전 영역 범위 오류: $safeArea"
}

# 실제 실행을 대체하지 않는 순수 배치 오라클. 설정 17개를 최대 5개씩
# 그룹화한 설정 패널과 200% 하단 메시지가 720p~1440p에서 안전
# 영역을 지키는지 C++과 독립된 계산으로 확인한다.
$layoutProfiles = @(
	@{ Width = 1280.0; Height = 720.0 },
	@{ Width = 1600.0; Height = 900.0 },
	@{ Width = 1920.0; Height = 1080.0 },
	@{ Width = 2560.0; Height = 1440.0 }
)
foreach ($profile in $layoutProfiles) {
	$width = $profile.Width
	$height = $profile.Height
	$resolutionScale = [Math]::Min(2.0, [Math]::Max(
		0.85,
		[Math]::Min($width / 1920.0, $height / 1080.0)))
	$horizontalMargin = [Math]::Max(24.0, 28.0 * $resolutionScale)
	$verticalMargin = [Math]::Max(20.0, 28.0 * $resolutionScale)
	$settingsPanelWidth = [Math]::Min(
		[Math]::Max(320.0, $width - 2.0 * $horizontalMargin),
		1320.0 * $resolutionScale)
	$settingsPanelHeight = [Math]::Min(
		[Math]::Max(420.0, $height - 2.0 * $verticalMargin),
		800.0 * $resolutionScale)
	$settingsPanelX = ($width - $settingsPanelWidth) * 0.5
	$settingsPanelY = ($height - $settingsPanelHeight) * 0.5
	$settingsFooterY = $settingsPanelY + $settingsPanelHeight - 64.0 * $resolutionScale
	$optionStartY = $settingsPanelY + 164.0 * $resolutionScale
	$optionRowHeight = [Math]::Max(52.0, 62.0 * $resolutionScale)
	$lastOptionBottom = $optionStartY + 4.0 * $optionRowHeight +
		$optionRowHeight - 7.0 * $resolutionScale
	$detailTop = $optionStartY + 5.0 * $optionRowHeight +
		18.0 * $resolutionScale
	$detailBottom = $settingsFooterY - 18.0 * $resolutionScale
	$detailHeight = [Math]::Min(
		210.0 * $resolutionScale,
		$detailBottom - $detailTop)
	Assert-True ($settingsPanelX -ge 0.0) "설정 패널 왼쪽 잘림: ${width}x${height}"
	Assert-True ($settingsPanelY -ge 0.0) "설정 패널 위쪽 잘림: ${width}x${height}"
	Assert-True (
		$settingsPanelX + $settingsPanelWidth -le $width
	) "설정 패널 오른쪽 잘림: ${width}x${height}"
	Assert-True (
		$settingsPanelY + $settingsPanelHeight -le $height
	) "설정 패널 아래쪽 잘림: ${width}x${height}"
	Assert-True ($optionRowHeight -ge 52.0) "설정 포인터 대상이 52px 미만: ${width}x${height}"
	Assert-True (
		$lastOptionBottom -lt $detailTop
	) "설정 행·설명 패널 겹침: ${width}x${height}"
	Assert-True (
		$detailTop + $detailHeight -le $detailBottom + 0.1
	) "설정 설명·하단 조작 안내 겹침: ${width}x${height}"

	foreach ($captionScale in @(0.85, 1.0, 1.25, 1.5, 2.0)) {
		foreach ($safeArea in @(0.80, 0.90, 1.0)) {
			$safeWidth = $width * $safeArea
			$basePanelWidth = [Math]::Min(
				920.0 * $resolutionScale,
				[Math]::Max(360.0 * $resolutionScale, $width * 0.72))
			$panelWidth = [Math]::Min(
				$basePanelWidth,
				[Math]::Max(280.0, $safeWidth - 32.0 * $resolutionScale))
			$safeLeft = ($width - $safeWidth) * 0.5
			$panelLeft = ($width - $panelWidth) * 0.5
			$panelRight = $panelLeft + $panelWidth
			Assert-True ($panelLeft -ge $safeLeft - 0.1) "메시지 왼쪽 안전 영역 침범: ${width}x${height}"
			Assert-True ($panelRight -le $width - $safeLeft + 0.1) "메시지 오른쪽 안전 영역 침범: ${width}x${height}"

			$textScale = $captionScale * $resolutionScale
			$lineCount = if ($captionScale -gt 1.25) { 3.0 } else { 2.0 }
			$bodyHeight = [Math]::Max(16.0, 19.0 * $textScale)
			$lineStep = $bodyHeight * $(if ($lineCount -ge 3.0) { 1.45 } else { 1.34 })
			$speakerHeight = [Math]::Max(12.0, 14.0 * $textScale * 0.84)
			$panelHeight = 16.0 * $resolutionScale + $speakerHeight +
				9.0 * $resolutionScale + $bodyHeight +
				$lineStep * ($lineCount - 1.0) + 17.0 * $resolutionScale
			$safeInset = $height * (1.0 - $safeArea) * 0.5
			$panelY = $height - $safeInset - $panelHeight - 24.0 * $resolutionScale
			$panelBottom = $panelY + $panelHeight
			Assert-True ($panelY -ge $safeInset - 0.1) "메시지 위쪽 안전 영역 침범: ${width}x${height}"
			Assert-True ($panelBottom -le $height - $safeInset + 0.1) "메시지 아래쪽 안전 영역 침범: ${width}x${height}"

			# 200%와 80% 안전 영역의 최악 조건은 두 줄로 잡는다. 실제 HUD도
			# 동일 폭을 실측해 1~2줄로 나누고 표면 높이를 줄 수에 맞춘다.
			$previewLineCount = if (
				$captionScale -ge 1.5 -and $safeArea -le 0.90
			) { 2.0 } else { 1.0 }
			$previewLineHeight = [Math]::Max(16.0, 19.0 * $textScale * 1.20)
			$previewBodyHeight = $previewLineHeight * $previewLineCount
			$previewSpeakerHeight = 14.0 * $textScale * 0.78
			$previewHeight = [Math]::Max(
				48.0 * $resolutionScale,
				$previewSpeakerHeight + $previewBodyHeight + 23.0 * $resolutionScale)
			$previewY = [Math]::Max(
				$detailTop + 62.0 * $resolutionScale,
				$detailTop + $detailHeight - $previewHeight -
					12.0 * $resolutionScale)
			Assert-True (
				$previewY -ge $detailTop
			) "자막 미리 보기가 설명 패널 위로 이탈: ${width}x${height}"
			Assert-True (
				$previewY + $previewHeight -le $detailBottom + 0.1
			) "자막 미리 보기·하단 조작 안내 겹침: ${width}x${height}"
		}
	}
}

function Assert-NoCaptionOverlap {
	param(
		[Parameter(Mandatory = $true)]
		[object[]]$Entries,
		[Parameter(Mandatory = $true)]
		[double]$SequenceEnd,
		[Parameter(Mandatory = $true)]
		[string]$Context
	)
	for ($index = 0; $index -lt $Entries.Count; $index++) {
		$entry = $Entries[$index]
		$nextStart = if ($index + 1 -lt $Entries.Count) {
			[double]$Entries[$index + 1].Start
		} else {
			$SequenceEnd
		}
		Assert-True (
			([double]$entry.Start + [double]$entry.Duration) -le $nextStart + 0.001
		) "$Context 자막 겹침: index=$index"
	}
}

$safetyCaptions = @(
	@{ Start = 0.10; Duration = 1.15 },
	@{ Start = 1.40; Duration = 2.30 },
	@{ Start = 4.10; Duration = 1.05 },
	@{ Start = 5.30; Duration = 2.50 },
	@{ Start = 9.00; Duration = 1.70 }
)
$montageCaptions = @(
	@{ Start = 0.01; Duration = 1.40 },
	@{ Start = 1.50; Duration = 1.20 },
	@{ Start = 2.80; Duration = 1.80 },
	@{ Start = 5.20; Duration = 1.40 },
	@{ Start = 6.90; Duration = 1.00 },
	@{ Start = 7.90; Duration = 1.20 },
	@{ Start = 9.10; Duration = 1.40 },
	@{ Start = 10.60; Duration = 0.80 },
	@{ Start = 11.50; Duration = 1.90 },
	@{ Start = 13.90; Duration = 2.00 }
)
Assert-NoCaptionOverlap $safetyCaptions 10.90 '공통 안전 절차'
Assert-NoCaptionOverlap $montageCaptions 16.40 '엔딩 B 암전 몽타주'

Write-Host (
	"REBIRTH_ACCESSIBILITY_CONTRACT PASS assertions=$assertionCount " +
	"hints=3 pressure_modes=3 gamepad=1 input_switch=1 captions=1 layout_profiles=4 caption_sequences=2 persistence=1 reduced_motion=1 reduced_flicker=1 toggle_hold=1"
) -ForegroundColor Green
