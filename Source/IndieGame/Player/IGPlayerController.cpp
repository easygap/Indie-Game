#include "Player/IGPlayerController.h"

#include "Accessibility/IGAccessibilitySubsystem.h"
#include "Audio/IGAudioHelpers.h"
#include "Audio/IGAudioRenderProbe.h"
#include "Audio/IGMissingFloorAudioSubsystem.h"
#include "Audio/IGToneSequenceSoundWave.h"
#include "AssetCompilingManager.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Entity/IGMissingFloorEpilogueDirector.h"
#include "Entity/IGMissingFloorFifthDawnDirector.h"
#include "InputCoreTypes.h"
#include "InputKeyEventArgs.h"
#include "GameFramework/GameUserSettings.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformMisc.h"
#include "HAL/PlatformTime.h"
#include "HighResScreenshot.h"
#include "Misc/App.h"
#include "Misc/CommandLine.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Narrative/IGMissingFloorNarrativeSubsystem.h"
#include "Narrative/IGRebirthNarrativeSubsystem.h"
#include "Narrative/IGStoryStateSubsystem.h"
#include "Player/IGCameraFeelModifier.h"
#include "Player/IGHorrorHUD.h"
#include "Player/IGInputBindingSubsystem.h"
#include "Player/IGFrontendMenuLayout.h"
#include "Player/IGPlayerCharacter.h"
#include "Player/IGSettingsMenuLayout.h"
#include "Save/IGSaveSubsystem.h"
#include "Sequence/IGSecondMorningDirector.h"
#include "Sequence/IGThirdMorningDirector.h"
#include "ShaderCompiler.h"
#include "IndieGame.h"

namespace IGInputLocks
{
	/** 이 시간을 넘겨 잠금이 남아 있으면 누가 붙들고 있는지 한 번 찍는다. */
	constexpr double WatchdogSeconds = 12.0;
}

namespace IGAccessibilityMenu
{
	constexpr int32 RowCount = IGSettingsMenuLayout::AccessibilityRowCount;
}

namespace IGSystemMenu
{
	constexpr int32 RowCount = IGFrontendMenuLayout::ActionCount;
}

namespace IGNightFive
{
	/**
	 * §9 「밤 5」의 30초. 대부분이 침묵이다 — 그것이 이 비트의 내용이다.
	 * 두 소리 사이의 9초가 「새 사건도, 갇힌 사람도 암시하지 않는다」를
	 * 지키는 방식이고, 뒤의 17초는 아무 일도 일어나지 않는다는 것을 확인하는
	 * 시간이다. 채울 수 있다는 이유로 채우면 이 슬롯은 다른 게임이 된다.
	 */
	constexpr float PollSeconds = 0.1f;
	constexpr float SignalAtSeconds = 3.2f;
	constexpr float AnswerAtSeconds = 12.6f;

	/** 그녀 자신의 손이므로 가깝고 마른 소리. */
	constexpr float SignalVolume = 0.86f;
	constexpr float CloseRadius = 4000.0f;
	constexpr float CloseFalloff = 6000.0f;
	/** 복도 끝이므로 작고 젖은 소리. 감쇠는 열어 두고 잔향이 거리를 말한다. */
	constexpr float AnswerVolume = 0.38f;
	constexpr float FarRadius = 4000.0f;
	constexpr float FarFalloff = 6000.0f;
}

namespace IGDisplaySettings
{
	constexpr int32 RowCount = IGSettingsMenuLayout::DisplayRowCount;
	constexpr int32 WindowModeCount = 3;
	constexpr int32 ResolutionCount = 3;
	constexpr int32 QualityCount = 2;
	constexpr int32 FrameLimitCount = 3;
	const FIntPoint Resolutions[ResolutionCount] =
	{
		FIntPoint(1280, 720),
		FIntPoint(1920, 1080),
		FIntPoint(2560, 1440)
	};
	const float FrameLimits[FrameLimitCount] = {30.0f, 60.0f, 0.0f};
}

namespace IGAudioCalibration
{
	// 행을 번호로 세다가 하나 끼우면 밝기가 출력 방식이 된다. 이름을 준다.
	enum ERow : int32
	{
		Master = 0,
		Music,
		Ambience,
		Output,
		Brightness,
		TestKnock,
		Done,
		Count
	};
	constexpr int32 RowCount = ERow::Count;
	constexpr int32 MusicStepCount = 5;
	constexpr int32 AmbienceStepCount = 5;
	// 음악은 0까지 내려간다. 작가가 얹은 것이라 없어도 사건은 남는다.
	constexpr float MusicValues[MusicStepCount] =
	{
		0.0f, 0.25f, 0.50f, 0.75f, 1.0f
	};
	// 환경음은 바닥이 있다. 건물이 내는 소리 자체가 단서다(§21.1).
	constexpr float AmbienceValues[AmbienceStepCount] =
	{
		0.40f, 0.55f, 0.70f, 0.85f, 1.0f
	};
	constexpr int32 VolumeStepCount = 7;
	constexpr int32 BrightnessStepCount = 5;
	constexpr const TCHAR* ConfigSection = TEXT("IndieGame.AudioOnboarding");
	constexpr float VolumeValues[VolumeStepCount] =
	{
		0.25f, 0.375f, 0.50f, 0.625f, 0.75f, 0.875f, 1.0f
	};
	constexpr float GammaValues[BrightnessStepCount] =
	{
		1.80f, 2.00f, 2.20f, 2.40f, 2.60f
	};
}

AIGPlayerController::AIGPlayerController()
{
	PrimaryActorTick.bCanEverTick = true;
	// PlayerTick이 입력과 카메라 갱신을 맡는다. 메뉴가 닫혀도 끄면 안 된다.
	PrimaryActorTick.bStartWithTickEnabled = true;
	PrimaryActorTick.bTickEvenWhenPaused = true;
}

void AIGPlayerController::BeginPlay()
{
	Super::BeginPlay();

	bShowMouseCursor = false;
	FInputModeGameOnly InputMode;
	SetInputMode(InputMode);
	ApplyDefaultInputMapping();
	// 손맛 회전(노크·포획 킥, 공포 떨림)은 카메라 매니저 단계에서 얹는다.
	// 카메라 컴포넌트의 상대 회전은 폰 제어 회전에 덮여 화면에 안 나온다.
	if (PlayerCameraManager)
	{
		PlayerCameraManager->AddNewCameraModifier(UIGCameraFeelModifier::StaticClass());
	}
	BindSaveNotifications();
	LoadAudioCalibrationSettings();

	bNightFiveProbeRequested =
		FParse::Param(FCommandLine::Get(), TEXT("IGNightFiveProbe"));
	if (bNightFiveProbeRequested)
	{
		UE_LOG(
			LogTemp,
			Display,
			TEXT("MISSINGFLOOR_NIGHT5_BOOT local=%d title=%d"),
			IsLocalController() ? 1 : 0,
			ShouldShowTitleMenu() ? 1 : 0);
	}

	if (IsLocalController() && ShouldShowTitleMenu())
	{
		SystemMenuSelection = 0;
		SetSystemMenuMode(EIGSystemMenuMode::Title);
		// 고지가 먼저다. 무엇이 나오는지 모른 채로 소리부터 맞추게 할 수 없다.
		ShowContentNoticeIfNeeded();
		StartHeadphoneRecommendationIfNeeded();
		if (bNightFiveProbeRequested)
		{
			StartNightFiveProbe();
		}
	}
	else
	{
		RefreshMenuHud();
	}

	if (IsLocalController() && IGAudioRenderProbe::RunIfRequested(this))
	{
		return;
	}
	if (IsLocalController()
		&& FParse::Param(
			FCommandLine::Get(),
			TEXT("IGFrontendShippingProbe")))
	{
		StartFrontendShippingProbe();
	}
	else if (IsLocalController()
		&& FParse::Param(
			FCommandLine::Get(),
			TEXT("IGMissingFloorJournalPreview")))
	{
		StartMissingFloorJournalPreviewProbe();
	}
	else if (IsLocalController()
		&& FParse::Param(
			FCommandLine::Get(),
			TEXT("IGAudioCalibrationPreview")))
	{
		StartAudioCalibrationPreviewProbe();
	}
	else if (IsLocalController()
		&& FParse::Param(
			FCommandLine::Get(),
			TEXT("IGMissingFloorEndingPreview")))
	{
		StartMissingFloorEndingPreviewProbe();
	}
	else if (IsLocalController()
		&& FParse::Param(
			FCommandLine::Get(),
			TEXT("IGDisplaySettingsPreview")))
	{
		StartDisplaySettingsPreviewProbe();
	}
}

void AIGPlayerController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	UnbindSaveNotifications();
	Super::EndPlay(EndPlayReason);
}

void AIGPlayerController::Tick(const float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (bFrontendShippingProbe)
	{
		TickFrontendShippingProbe();
		return;
	}
	if (bMissingFloorJournalPreviewProbe)
	{
		TickMissingFloorJournalPreviewProbe();
		return;
	}
	if (bAudioCalibrationPreviewProbe)
	{
		TickAudioCalibrationPreviewProbe();
		return;
	}
	if (bMissingFloorEndingPreviewProbe)
	{
		TickMissingFloorEndingPreviewProbe();
		return;
	}
	if (bDisplaySettingsPreviewProbe)
	{
		TickDisplaySettingsPreviewProbe();
		return;
	}
	if (InputLockWatchdogNextReportTime > 0.0
		&& FPlatformTime::Seconds() >= InputLockWatchdogNextReportTime)
	{
		UE_LOG(
			LogIndieGame,
			Warning,
			TEXT("IG_INPUT_LOCK held for %.0fs by %s"),
			IGInputLocks::WatchdogSeconds,
			*DescribeInputLocks());
		InputLockWatchdogNextReportTime =
			FPlatformTime::Seconds() + IGInputLocks::WatchdogSeconds;
	}
	if (bJournalInputHeld && !bMissingFloorJournalVisible)
	{
		const UIGAccessibilitySubsystem* Accessibility =
			GetAccessibilitySubsystem();
		const double JournalHoldSeconds = 0.30 * (Accessibility
			? Accessibility->GetHoldDurationScale()
			: 1.0f);
		if (FPlatformTime::Seconds() - JournalInputPressedAt >= JournalHoldSeconds)
		{
			bJournalInputHeld = false;
			OpenMissingFloorJournal();
		}
	}
	if (bHeadphoneRecommendationVisible
		&& FPlatformTime::Seconds() >= HeadphoneRecommendationDeadline)
	{
		DismissHeadphoneRecommendation();
	}
	if (SystemMenuMode == EIGSystemMenuMode::AudioCalibration
		&& NextAudioCalibrationKnockTime > 0.0
		&& FPlatformTime::Seconds() >= NextAudioCalibrationKnockTime)
	{
		PlayAudioCalibrationKnock();
	}
	if (!bDisplaySettingsAwaitingConfirmation
		&& !bJournalInputHeld
		&& !bHeadphoneRecommendationVisible
		&& NextAudioCalibrationKnockTime <= 0.0)
	{
		return;
	}
	if (!bDisplaySettingsAwaitingConfirmation)
	{
		return;
	}
	const double Remaining = DisplayConfirmationDeadline - FPlatformTime::Seconds();
	if (Remaining <= 0.0)
	{
		RevertPendingDisplaySettings();
		return;
	}
	const int32 RoundedRemaining = FMath::Max(1, FMath::CeilToInt(Remaining));
	if (DisplayConfirmationSecondsRemaining != RoundedRemaining)
	{
		DisplayConfirmationSecondsRemaining = RoundedRemaining;
		RefreshMenuHud();
	}
}

void AIGPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();
	if (InputComponent)
	{
		auto BindPausedAction = [this](
			const FName ActionName,
			void (ThisClass::*Handler)())
		{
			FInputActionBinding& Binding = InputComponent->BindAction(
				ActionName,
				IE_Pressed,
				this,
				Handler);
			Binding.bExecuteWhenPaused = true;
		};
		BindPausedAction(TEXT("PauseMenu"), &ThisClass::ToggleSystemMenu);
		BindPausedAction(
			TEXT("AccessibilityMenu"),
			&ThisClass::ToggleAccessibilityMenu);
		BindPausedAction(
			TEXT("AccessibilityUp"),
			&ThisClass::MoveAccessibilitySelectionUp);
		BindPausedAction(
			TEXT("AccessibilityDown"),
			&ThisClass::MoveAccessibilitySelectionDown);
		BindPausedAction(
			TEXT("AccessibilityLeft"),
			&ThisClass::AdjustAccessibilityLeft);
		BindPausedAction(
			TEXT("AccessibilityRight"),
			&ThisClass::AdjustAccessibilityRight);
		BindPausedAction(
			TEXT("AccessibilityConfirm"),
			&ThisClass::ConfirmAccessibilitySelection);
		BindPausedAction(
			TEXT("AccessibilityClose"),
			&ThisClass::CloseAccessibilityMenu);
		FInputActionBinding& JournalPressed = InputComponent->BindAction(
			TEXT("Journal"),
			IE_Pressed,
			this,
			&ThisClass::BeginJournalInput);
		JournalPressed.bExecuteWhenPaused = true;
		FInputActionBinding& JournalReleased = InputComponent->BindAction(
			TEXT("Journal"),
			IE_Released,
			this,
			&ThisClass::EndJournalInput);
		JournalReleased.bExecuteWhenPaused = true;
		BindPausedAction(
			TEXT("JournalPrevious"),
			&ThisClass::MoveMissingFloorJournalPageLeft);
		BindPausedAction(
			TEXT("JournalNext"),
			&ThisClass::MoveMissingFloorJournalPageRight);
		InputComponent->BindAction(
			TEXT("RequestHint"),
			IE_Pressed,
			this,
			&ThisClass::RequestManualHint);
	}
}

bool AIGPlayerController::InputKey(const FInputKeyEventArgs& Params)
{
	if (bFrontendShippingProbe
		&& !Params.IsSimulatedInput()
		&& Params.Event == IE_Pressed)
	{
		++FrontendProbePressedEventCount;
	}
	if (CaptureKeyBindingInput(Params))
	{
		return true;
	}
	const bool bGamepad = Params.IsGamepad();
	const float AxisThreshold = bGamepad ? 0.30f : 0.01f;
	const bool bMeaningfulInput = !Params.IsSimulatedInput()
		&& (Params.Event == IE_Pressed
			|| Params.Event == IE_Repeat
			|| (Params.Event == IE_Axis
				&& (FMath::Abs(Params.AmountDepressed) > AxisThreshold
					|| Params.AmountDepressed2D.SizeSquared()
						> FMath::Square(AxisThreshold))));
	if (bHeadphoneRecommendationVisible
		&& !Params.IsSimulatedInput()
		&& Params.Event == IE_Pressed)
	{
		DismissHeadphoneRecommendation();
		return true;
	}
	if (bMeaningfulInput && bUsingGamepadForHud != bGamepad)
	{
		SetInputDevicePresentation(bGamepad);
	}
	if (!Params.IsSimulatedInput()
		&& Params.Event == IE_Axis
		&& (Params.Key == EKeys::MouseX || Params.Key == EKeys::MouseY))
	{
		UpdateMenuPointerHover();
	}
	if (!Params.IsSimulatedInput()
		&& Params.Event == IE_Pressed
		&& Params.Key == EKeys::LeftMouseButton
		&& HandleMenuPointerClick())
	{
		return true;
	}
	// 방향 패드의 메뉴 이동과 겹치므로 자유 조작 중에만 안내 키를 먼저 받는다.
	if (Params.Event == IE_Pressed && SystemMenuMode == EIGSystemMenuMode::Hidden
		&& !bAccessibilityMenuVisible && !bMissingFloorJournalVisible)
	{
		const UIGInputBindingSubsystem* Bindings = GetGameInstance()->GetSubsystem<UIGInputBindingSubsystem>();
		AIGHorrorHUD* Hud = Cast<AIGHorrorHUD>(GetHUD());
		if (Bindings && Hud && Params.Key == Bindings->GetBoundKey(
			static_cast<int32>(EIGBindableAction::GameplayGuide), bGamepad))
		{
			if (Hud->CanShowGameplayGuide()) Hud->ToggleGameplayGuide();
			return true;
		}
	}
	return Super::InputKey(Params);
}

void AIGPlayerController::StartFrontendShippingProbe()
{
	const TCHAR* CommandLine = FCommandLine::Get();
	FParse::Value(
		CommandLine,
		TEXT("IGFrontendExpectedWidth="),
		FrontendProbeExpectedWidth);
	FParse::Value(
		CommandLine,
		TEXT("IGFrontendExpectedHeight="),
		FrontendProbeExpectedHeight);
	if (FrontendProbeExpectedWidth <= 0 || FrontendProbeExpectedHeight <= 0)
	{
		bFrontendShippingProbe = true;
		FailFrontendShippingProbe(TEXT("expected_resolution_missing"));
		return;
	}

	bFrontendShippingProbe = true;
	FrontendProbeStep = 0;
	FrontendProbeLayoutSampleCount = 0;
	FrontendProbeMinimumElementCount = MAX_int32;
	FrontendProbePressedEventCount = 0;
	bFrontendDialogueDefaultVerified = false;
	bFrontendDialogueVerified = false;
	bFrontendDialogueSpeakerVerified = false;
	bFrontendDialogueContinuationVerified = false;
	bFrontendAccessibilityScreenshotRequested = false;
	bFrontendDisplayScreenshotRequested = false;
	bFrontendProbeCompilationDrained = false;
	FrontendProbeAwaitFrameSerial = 0;
	FrontendProbeDefaultScreenshotPath.Reset();
	FrontendProbeScreenshotPath.Reset();
	FrontendProbeTitleScreenshotPath.Reset();
	FrontendProbeBoundsMin = FVector2D(
		TNumericLimits<float>::Max(),
		TNumericLimits<float>::Max());
	FrontendProbeBoundsMax = FVector2D(
		TNumericLimits<float>::Lowest(),
		TNumericLimits<float>::Lowest());

	bAccessibilityMenuVisible = false;
	// Start on the five-row caption category: this is the tightest settings
	// composition and includes the live 200%-scale preview exercised below.
	AccessibilitySelection = 5;
	SystemMenuSelection = 0;
	SetSystemMenuMode(EIGSystemMenuMode::Hidden);
	SetInputDevicePresentation(false);
	const double Now = FPlatformTime::Seconds();
	FrontendProbeNextActionTime = Now + 0.75;
	FrontendProbeStepDeadline = Now + 8.0;
	SetActorTickEnabled(true);
}

void AIGPlayerController::TickFrontendShippingProbe()
{
	const double Now = FPlatformTime::Seconds();
	if (Now > FrontendProbeStepDeadline)
	{
		FailFrontendShippingProbe(FString::Printf(
			TEXT("timeout_step_%d"),
			FrontendProbeStep));
		return;
	}
	if (Now < FrontendProbeNextActionTime)
	{
		return;
	}
	if (!bFrontendProbeCompilationDrained)
	{
		// Offscreen editor runs can still be compiling materials when the first
		// settings frame is ready. Drain that work before collecting evidence so
		// engine progress text never contaminates a product-facing capture.
		FAssetCompilingManager::Get().FinishAllCompilation();
		if (GShaderCompilingManager)
		{
			GShaderCompilingManager->FinishAllCompilation();
		}
		if (GEngine)
		{
			GEngine->bEnableOnScreenDebugMessages = false;
		}
		ConsoleCommand(TEXT("DisableAllScreenMessages"), true);
		bFrontendProbeCompilationDrained = true;
		const double SettledAt = FPlatformTime::Seconds();
		FrontendProbeNextActionTime = SettledAt + 0.40;
		FrontendProbeStepDeadline = SettledAt + 8.0;
		return;
	}
	auto WaitForInputProcessing = [this, Now](const int32 NextStep)
	{
		FrontendProbeStep = NextStep;
		FrontendProbeNextActionTime = Now + 0.05;
		FrontendProbeStepDeadline = Now + 4.0;
	};

	switch (FrontendProbeStep)
	{
	case 0:
		DispatchFrontendProbeKey(EKeys::F10);
		WaitForInputProcessing(1);
		return;
	case 1:
		if (!bAccessibilityMenuVisible || bUsingGamepadForHud)
		{
			FailFrontendShippingProbe(TEXT("keyboard_f10_open"));
			return;
		}
		ReleaseFrontendProbeKey(EKeys::F10);
		FrontendProbeStep = 2;
		AwaitFrontendProbeFrame();
		return;
	case 2:
		if (!bFrontendAccessibilityScreenshotRequested)
		{
			if (!TryCaptureFrontendProbeLayout(
				TEXT("accessibility_keyboard"),
				14))
			{
				return;
			}
			FString ScreenshotPath;
			if (FParse::Value(
				FCommandLine::Get(),
				TEXT("IGFrontendAccessibilityScreenshotPath="),
				ScreenshotPath))
			{
				ScreenshotPath.TrimQuotesInline();
				ScreenshotPath = FPaths::ConvertRelativePathToFull(ScreenshotPath);
				IFileManager::Get().MakeDirectory(
					*FPaths::GetPath(ScreenshotPath),
					true);
				FScreenshotRequest::RequestScreenshot(ScreenshotPath, true, false);
			}
			bFrontendAccessibilityScreenshotRequested = true;
			// Give the requested capture a complete frame before changing focus
			// to the other dense category.
			FrontendProbeNextActionTime = Now + 0.12;
			FrontendProbeStepDeadline = Now + 4.0;
			return;
		}
		AccessibilitySelection = 9;
		DispatchFrontendProbeKey(EKeys::Gamepad_DPad_Down);
		WaitForInputProcessing(3);
		return;
	case 3:
		if (!bAccessibilityMenuVisible
			|| AccessibilitySelection != 10
			|| !bUsingGamepadForHud)
		{
			FailFrontendShippingProbe(TEXT("gamepad_dpad_down"));
			return;
		}
		ReleaseFrontendProbeKey(EKeys::Gamepad_DPad_Down);
		FrontendProbeStep = 4;
		AwaitFrontendProbeFrame();
		return;
	case 4:
		if (!TryCaptureFrontendProbeLayout(TEXT("accessibility_gamepad"), 14))
		{
			return;
		}
		DispatchFrontendProbeKey(EKeys::Up);
		WaitForInputProcessing(5);
		return;
	case 5:
		if (!bAccessibilityMenuVisible
			|| AccessibilitySelection != 9
			|| bUsingGamepadForHud)
		{
			FailFrontendShippingProbe(TEXT("keyboard_up_return"));
			return;
		}
		ReleaseFrontendProbeKey(EKeys::Up);
		FrontendProbeStep = 6;
		AwaitFrontendProbeFrame();
		return;
	case 6:
		if (!TryCaptureFrontendProbeLayout(TEXT("accessibility_keyboard_return"), 14))
		{
			return;
		}
		DispatchFrontendProbeKey(EKeys::F10);
		WaitForInputProcessing(7);
		return;
	case 7:
		if (bAccessibilityMenuVisible)
		{
			FailFrontendShippingProbe(TEXT("keyboard_f10_close"));
			return;
		}
		ReleaseFrontendProbeKey(EKeys::F10);
		FrontendProbeStep = 8;
		FrontendProbeNextActionTime = Now + 0.10;
		FrontendProbeStepDeadline = Now + 4.0;
		return;
	case 8:
		DispatchFrontendProbeKey(EKeys::Gamepad_Special_Right);
		WaitForInputProcessing(9);
		return;
	case 9:
		if (!bAccessibilityMenuVisible || !bUsingGamepadForHud)
		{
			FailFrontendShippingProbe(TEXT("gamepad_menu_open"));
			return;
		}
		ReleaseFrontendProbeKey(EKeys::Gamepad_Special_Right);
		FrontendProbeStep = 10;
		AwaitFrontendProbeFrame();
		return;
	case 10:
		if (!TryCaptureFrontendProbeLayout(TEXT("accessibility_gamepad_reopen"), 14))
		{
			return;
		}
		DispatchFrontendProbeKey(EKeys::Gamepad_FaceButton_Right);
		WaitForInputProcessing(11);
		return;
	case 11:
		if (bAccessibilityMenuVisible)
		{
			FailFrontendShippingProbe(TEXT("gamepad_b_close"));
			return;
		}
		ReleaseFrontendProbeKey(EKeys::Gamepad_FaceButton_Right);
		FrontendProbeStep = 12;
		FrontendProbeNextActionTime = Now + 0.10;
		FrontendProbeStepDeadline = Now + 4.0;
		return;
	case 12:
		DispatchFrontendProbeKey(EKeys::Escape);
		WaitForInputProcessing(13);
		return;
	case 13:
		if (SystemMenuMode != EIGSystemMenuMode::Pause
			|| bUsingGamepadForHud)
		{
			FailFrontendShippingProbe(TEXT("keyboard_escape_pause"));
			return;
		}
		ReleaseFrontendProbeKey(EKeys::Escape);
		FrontendProbeStep = 14;
		AwaitFrontendProbeFrame();
		return;
	case 14:
		if (!TryCaptureFrontendProbeLayout(TEXT("pause_keyboard"), 8))
		{
			return;
		}
		DispatchFrontendProbeKey(EKeys::Gamepad_DPad_Down);
		WaitForInputProcessing(15);
		return;
	case 15:
		if (SystemMenuMode != EIGSystemMenuMode::Pause
			|| SystemMenuSelection != 2
			|| !bUsingGamepadForHud)
		{
			FailFrontendShippingProbe(TEXT("gamepad_pause_settings_row"));
			return;
		}
		ReleaseFrontendProbeKey(EKeys::Gamepad_DPad_Down);
		FrontendProbeStep = 16;
		AwaitFrontendProbeFrame();
		return;
	case 16:
		if (!TryCaptureFrontendProbeLayout(TEXT("pause_gamepad"), 8))
		{
			return;
		}
		DispatchFrontendProbeKey(EKeys::Gamepad_FaceButton_Bottom);
		WaitForInputProcessing(17);
		return;
	case 17:
		if (SystemMenuMode != EIGSystemMenuMode::DisplaySettings)
		{
			FailFrontendShippingProbe(TEXT("gamepad_a_display_open"));
			return;
		}
		ReleaseFrontendProbeKey(EKeys::Gamepad_FaceButton_Bottom);
		// Performance has the most simultaneous display rows and is therefore
		// the strongest packaged screenshot for horizontal text fitting.
		DisplaySettingsSelection = IGSettingsMenuLayout::Quality;
		RefreshMenuHud();
		FrontendProbeStep = 18;
		AwaitFrontendProbeFrame();
		return;
	case 18:
		if (!TryCaptureFrontendProbeLayout(TEXT("display_gamepad"), 11))
		{
			return;
		}
		if (!bFrontendDisplayScreenshotRequested)
		{
			FString ScreenshotPath;
			if (FParse::Value(
				FCommandLine::Get(),
				TEXT("IGFrontendDisplayScreenshotPath="),
				ScreenshotPath))
			{
				ScreenshotPath.TrimQuotesInline();
				ScreenshotPath = FPaths::ConvertRelativePathToFull(ScreenshotPath);
				IFileManager::Get().MakeDirectory(
					*FPaths::GetPath(ScreenshotPath),
					true);
				FScreenshotRequest::RequestScreenshot(ScreenshotPath, true, false);
			}
			bFrontendDisplayScreenshotRequested = true;
		}
		DispatchFrontendProbeKey(EKeys::Gamepad_FaceButton_Right);
		WaitForInputProcessing(19);
		return;
	case 19:
		if (SystemMenuMode != EIGSystemMenuMode::Pause)
		{
			FailFrontendShippingProbe(TEXT("gamepad_b_display_back"));
			return;
		}
		ReleaseFrontendProbeKey(EKeys::Gamepad_FaceButton_Right);
		FrontendProbeStep = 20;
		AwaitFrontendProbeFrame();
		return;
	case 20:
		if (!TryCaptureFrontendProbeLayout(TEXT("pause_gamepad_return"), 8))
		{
			return;
		}
		DispatchFrontendProbeKey(EKeys::Gamepad_Special_Left);
		WaitForInputProcessing(21);
		return;
	case 21:
		if (SystemMenuMode != EIGSystemMenuMode::Hidden)
		{
			FailFrontendShippingProbe(TEXT("gamepad_view_pause_close"));
			return;
		}
		ReleaseFrontendProbeKey(EKeys::Gamepad_Special_Left);
		if (bAccessibilityMenuVisible
			|| SystemMenuMode != EIGSystemMenuMode::Hidden)
		{
			FailFrontendShippingProbe(TEXT("frontend_not_closed"));
			return;
		}
		if (UIGAccessibilitySubsystem* Accessibility =
			GetAccessibilitySubsystem())
		{
			FIGAccessibilitySettings Settings = Accessibility->GetSettings();
			Settings.bSubtitlesEnabled = true;
			Settings.bSoundCaptionsEnabled = true;
			Settings.CaptionSizeScale = 1.0f;
			Settings.CaptionBackgroundOpacity = 0.82f;
			Settings.CaptionSafeAreaScale = 0.90f;
			Accessibility->ApplySettings(Settings);
		}
		else
		{
			FailFrontendShippingProbe(TEXT("dialogue_accessibility_missing"));
			return;
		}
		if (AIGHorrorHUD* HorrorHUD = Cast<AIGHorrorHUD>(GetHUD()))
		{
			HorrorHUD->ShowDialogue(
				NSLOCTEXT("IGFrontendProbe", "DialogueSpeaker", "휴대폰"),
				NSLOCTEXT(
					"IGFrontendProbe",
					"DialogueDefaultKorean",
					"라디오가 끊겼다. 휴대폰 시계는 4시 44분에서 멈춰 있다."),
				EIGDialogueChannel::Device,
				5.0f,
				EIGDialoguePriority::Story);
			HorrorHUD->ShowAudioCaption(
				NSLOCTEXT(
					"IGFrontendProbe",
					"SoundCaption",
					"[오른쪽 문 너머에서 라디오가 끊긴다]"),
				5.0f);
		}
		else
		{
			FailFrontendShippingProbe(TEXT("dialogue_hud_missing"));
			return;
		}
		FrontendProbeStep = 22;
		AwaitFrontendProbeFrame();
		// Capture the settled reading state, not an early frame from the 240 ms
		// entrance transition. This keeps visual evidence representative while
		// layout validation still observes the animated path frame-by-frame.
		FrontendProbeNextActionTime = Now + 0.30;
		return;
	case 22:
		if (!TryCaptureFrontendProbeLayout(
			TEXT("dialogue_default_scale"),
			5,
			false)
			|| !TryVerifyFrontendDialogueLayout(
				TEXT("dialogue_default"),
				1,
				2,
				false))
		{
			return;
		}
		bFrontendDialogueDefaultVerified = true;
		if (FParse::Value(
			FCommandLine::Get(),
			TEXT("IGFrontendDefaultScreenshotPath="),
			FrontendProbeDefaultScreenshotPath))
		{
			FrontendProbeDefaultScreenshotPath.TrimQuotesInline();
			FrontendProbeDefaultScreenshotPath = FPaths::ConvertRelativePathToFull(
				FrontendProbeDefaultScreenshotPath);
			IFileManager::Get().MakeDirectory(
				*FPaths::GetPath(FrontendProbeDefaultScreenshotPath),
				true);
			FScreenshotRequest::RequestScreenshot(
				FrontendProbeDefaultScreenshotPath,
				true,
				false);
		}
		FrontendProbeStep = 23;
		FrontendProbeNextActionTime = Now + 0.08;
		FrontendProbeStepDeadline = Now + 5.0;
		return;
	case 23:
		if (!FrontendProbeDefaultScreenshotPath.IsEmpty()
			&& !FPaths::FileExists(FrontendProbeDefaultScreenshotPath))
		{
			FrontendProbeNextActionTime = Now + 0.05;
			return;
		}
		if (UIGAccessibilitySubsystem* Accessibility =
			GetAccessibilitySubsystem())
		{
			FIGAccessibilitySettings Settings = Accessibility->GetSettings();
			Settings.CaptionSizeScale = 2.0f;
			Settings.CaptionBackgroundOpacity = 0.92f;
			Settings.CaptionSafeAreaScale = 0.80f;
			Accessibility->ApplySettings(Settings);
		}
		else
		{
			FailFrontendShippingProbe(TEXT("dialogue_max_accessibility_missing"));
			return;
		}
		if (AIGHorrorHUD* HorrorHUD = Cast<AIGHorrorHUD>(GetHUD()))
		{
			HorrorHUD->ShowDialogue(
				NSLOCTEXT("IGFrontendProbe", "DialogueSpeaker", "휴대폰"),
				NSLOCTEXT(
					"IGFrontendProbe",
					"DialogueLongKorean",
					"통화가 연결되지 않습니다. 복도 오른쪽 문 너머에서 라디오가 끊겼고, 같은 순간 휴대폰의 시계가 4시 44분으로 돌아왔습니다. 이 문장은 큰 글자에서도 잘리지 않고 다음 페이지로 이어져야 합니다. 플레이어가 이동 중이어도 앞 문장을 덮어쓰지 않아야 합니다."),
				EIGDialogueChannel::Device,
				5.0f,
				EIGDialoguePriority::Critical);
		}
		else
		{
			FailFrontendShippingProbe(TEXT("dialogue_max_hud_missing"));
			return;
		}
		FrontendProbeStep = 24;
		AwaitFrontendProbeFrame();
		FrontendProbeNextActionTime = Now + 0.30;
		return;
	case 24:
		if (!TryCaptureFrontendProbeLayout(
			TEXT("dialogue_max_scale"),
			8,
			false)
			|| !TryVerifyFrontendDialogueLayout(
				TEXT("dialogue_max"),
				3,
				3,
				true))
		{
			return;
		}
		bFrontendDialogueVerified = true;
		bFrontendDialogueSpeakerVerified = true;
		bFrontendDialogueContinuationVerified = true;
		if (FParse::Value(
			FCommandLine::Get(),
			TEXT("IGFrontendScreenshotPath="),
			FrontendProbeScreenshotPath))
		{
			FrontendProbeScreenshotPath.TrimQuotesInline();
			FrontendProbeScreenshotPath = FPaths::ConvertRelativePathToFull(
				FrontendProbeScreenshotPath);
			IFileManager::Get().MakeDirectory(
				*FPaths::GetPath(FrontendProbeScreenshotPath),
				true);
			FScreenshotRequest::RequestScreenshot(
				FrontendProbeScreenshotPath,
				true,
				false);
			FrontendProbeStep = 25;
			FrontendProbeNextActionTime = Now + 0.08;
			FrontendProbeStepDeadline = Now + 5.0;
			return;
		}
		CompleteFrontendShippingProbe();
		return;
	case 25:
		if (!FPaths::FileExists(FrontendProbeScreenshotPath))
		{
			FrontendProbeNextActionTime = Now + 0.05;
			return;
		}
		if (!FParse::Value(
			FCommandLine::Get(),
			TEXT("IGFrontendTitleScreenshotPath="),
			FrontendProbeTitleScreenshotPath))
		{
			FailFrontendShippingProbe(TEXT("title_screenshot_argument_missing"));
			return;
		}
		FrontendProbeTitleScreenshotPath.TrimQuotesInline();
		FrontendProbeTitleScreenshotPath = FPaths::ConvertRelativePathToFull(
			FrontendProbeTitleScreenshotPath);
		IFileManager::Get().MakeDirectory(
			*FPaths::GetPath(FrontendProbeTitleScreenshotPath),
			true);
		SystemMenuSelection = 0;
		SetSystemMenuMode(EIGSystemMenuMode::Title);
		SetInputDevicePresentation(false);
		FrontendProbeStep = 26;
		AwaitFrontendProbeFrame();
		// The normal 320 ms alpha entrance is part of the visual contract. Capture
		// its settled state while reduced-motion runs remain instant.
		FrontendProbeNextActionTime = Now + 0.38;
		FrontendProbeStepDeadline = Now + 5.0;
		return;
	case 26:
		if (SystemMenuMode != EIGSystemMenuMode::Title
			|| bCompatibleAutosaveAvailable
			|| SystemMenuSelection != 1)
		{
			FailFrontendShippingProbe(TEXT("title_first_run_state"));
			return;
		}
		if (!TryCaptureFrontendProbeLayout(TEXT("title_first_run"), 10))
		{
			return;
		}
		FScreenshotRequest::RequestScreenshot(
			FrontendProbeTitleScreenshotPath,
			true,
			false);
		FrontendProbeStep = 27;
		FrontendProbeNextActionTime = Now + 0.08;
		FrontendProbeStepDeadline = Now + 5.0;
		return;
	case 27:
		if (!FPaths::FileExists(FrontendProbeTitleScreenshotPath))
		{
			FrontendProbeNextActionTime = Now + 0.05;
			return;
		}
		CompleteFrontendShippingProbe();
		return;
	default:
		FailFrontendShippingProbe(TEXT("invalid_step"));
		return;
	}
}

void AIGPlayerController::DispatchFrontendProbeKey(const FKey& Key)
{
	const FInputDeviceId DeviceId = FInputDeviceId::CreateFromInternalId(0);
	FInputKeyEventArgs Pressed(
		nullptr,
		DeviceId,
		Key,
		IE_Pressed,
		1.0f,
		false,
		FPlatformTime::Cycles64());
	InputKey(Pressed);
}

void AIGPlayerController::ReleaseFrontendProbeKey(const FKey& Key)
{
	const FInputDeviceId DeviceId = FInputDeviceId::CreateFromInternalId(0);
	FInputKeyEventArgs Released(
		nullptr,
		DeviceId,
		Key,
		IE_Released,
		0.0f,
		false,
		FPlatformTime::Cycles64());
	InputKey(Released);
}

void AIGPlayerController::AwaitFrontendProbeFrame()
{
	FrontendProbeAwaitFrameSerial = 0;
	if (const AIGHorrorHUD* HorrorHUD = Cast<AIGHorrorHUD>(GetHUD()))
	{
		FVector2D CanvasSize;
		FVector2D BoundsMin;
		FVector2D BoundsMax;
		int32 ElementCount = 0;
		bool bAllInsideCanvas = false;
		bool bAllInsideSettingsContainers = false;
		HorrorHUD->GetLayoutValidationSample(
			CanvasSize,
			BoundsMin,
			BoundsMax,
			ElementCount,
			bAllInsideCanvas,
			bAllInsideSettingsContainers,
			FrontendProbeAwaitFrameSerial);
	}
	const double Now = FPlatformTime::Seconds();
	FrontendProbeNextActionTime = Now + 0.05;
	FrontendProbeStepDeadline = Now + 4.0;
}

bool AIGPlayerController::TryCaptureFrontendProbeLayout(
	const TCHAR* PanelName,
	const int32 MinimumElementCount,
	const bool bIncludeInMinimumElementCoverage)
{
	const AIGHorrorHUD* HorrorHUD = Cast<AIGHorrorHUD>(GetHUD());
	if (!HorrorHUD)
	{
		return false;
	}
	FVector2D CanvasSize;
	FVector2D BoundsMin;
	FVector2D BoundsMax;
	int32 ElementCount = 0;
	bool bAllInsideCanvas = false;
	bool bAllInsideSettingsContainers = false;
	uint64 FrameSerial = 0;
	if (!HorrorHUD->GetLayoutValidationSample(
		CanvasSize,
		BoundsMin,
		BoundsMax,
		ElementCount,
		bAllInsideCanvas,
		bAllInsideSettingsContainers,
		FrameSerial)
		|| FrameSerial <= FrontendProbeAwaitFrameSerial)
	{
		return false;
	}
	if (FMath::Abs(CanvasSize.X - FrontendProbeExpectedWidth) > 1.0f
		|| FMath::Abs(CanvasSize.Y - FrontendProbeExpectedHeight) > 1.0f)
	{
		FailFrontendShippingProbe(FString::Printf(
			TEXT("%s_canvas_%dx%d"),
			PanelName,
			FMath::RoundToInt(CanvasSize.X),
			FMath::RoundToInt(CanvasSize.Y)));
		return false;
	}
	if (!bAllInsideCanvas)
	{
		FailFrontendShippingProbe(FString::Printf(
			TEXT("%s_overflow_%d_%d_%d_%d"),
			PanelName,
			FMath::RoundToInt(BoundsMin.X),
			FMath::RoundToInt(BoundsMin.Y),
			FMath::RoundToInt(BoundsMax.X),
			FMath::RoundToInt(BoundsMax.Y)));
		return false;
	}
	if (!bAllInsideSettingsContainers)
	{
		FailFrontendShippingProbe(FString::Printf(
			TEXT("%s_container_overflow"),
			PanelName));
		return false;
	}
	if (ElementCount < MinimumElementCount)
	{
		FailFrontendShippingProbe(FString::Printf(
			TEXT("%s_elements_%d"),
			PanelName,
			ElementCount));
		return false;
	}

	FrontendProbeBoundsMin.X = FMath::Min(
		FrontendProbeBoundsMin.X,
		BoundsMin.X);
	FrontendProbeBoundsMin.Y = FMath::Min(
		FrontendProbeBoundsMin.Y,
		BoundsMin.Y);
	FrontendProbeBoundsMax.X = FMath::Max(
		FrontendProbeBoundsMax.X,
		BoundsMax.X);
	FrontendProbeBoundsMax.Y = FMath::Max(
		FrontendProbeBoundsMax.Y,
		BoundsMax.Y);
	if (bIncludeInMinimumElementCoverage)
	{
		FrontendProbeMinimumElementCount = FMath::Min(
			FrontendProbeMinimumElementCount,
			ElementCount);
	}
	++FrontendProbeLayoutSampleCount;
	FrontendProbeAwaitFrameSerial = FrameSerial;
	return true;
}

bool AIGPlayerController::TryVerifyFrontendDialogueLayout(
	const TCHAR* CaseName,
	const int32 MinimumLineCount,
	const int32 MaximumLineCount,
	const bool bExpectedContinuation)
{
	const AIGHorrorHUD* HorrorHUD = Cast<AIGHorrorHUD>(GetHUD());
	if (!HorrorHUD)
	{
		FailFrontendShippingProbe(FString::Printf(
			TEXT("%s_hud_missing"),
			CaseName));
		return false;
	}

	FVector2D PanelMinimum;
	FVector2D PanelMaximum;
	FVector2D CanvasSize;
	int32 LineCount = 0;
	bool bSpeakerVisible = false;
	bool bHasContinuation = false;
	bool bInsideSafeArea = false;
	uint64 RenderSerial = 0;
	const bool bValid = HorrorHUD->GetDialogueRenderSample(
		PanelMinimum,
		PanelMaximum,
		CanvasSize,
		LineCount,
		bSpeakerVisible,
		bHasContinuation,
		bInsideSafeArea,
		RenderSerial)
		&& FMath::Abs(CanvasSize.X - FrontendProbeExpectedWidth) <= 1.0f
		&& FMath::Abs(CanvasSize.Y - FrontendProbeExpectedHeight) <= 1.0f
		&& LineCount >= MinimumLineCount
		&& LineCount <= MaximumLineCount
		&& bSpeakerVisible
		&& bHasContinuation == bExpectedContinuation
		&& bInsideSafeArea
		&& RenderSerial > 0;
	if (!bValid)
	{
		FailFrontendShippingProbe(FString::Printf(
			TEXT("%s_lines_%d_speaker_%d_cont_%d_safe_%d"),
			CaseName,
			LineCount,
			bSpeakerVisible ? 1 : 0,
			bHasContinuation ? 1 : 0,
			bInsideSafeArea ? 1 : 0));
		return false;
	}
	return true;
}

void AIGPlayerController::CompleteFrontendShippingProbe()
{
	if (FrontendProbeLayoutSampleCount != 11
		|| FrontendProbeMinimumElementCount < 8
		|| FrontendProbePressedEventCount != 11
		|| !bFrontendDialogueDefaultVerified
		|| !bFrontendDialogueVerified
		|| !bFrontendDialogueSpeakerVerified
		|| !bFrontendDialogueContinuationVerified)
	{
		FailFrontendShippingProbe(FString::Printf(
			TEXT("coverage_samples_%d_elements_%d_inputs_%d"),
			FrontendProbeLayoutSampleCount,
			FrontendProbeMinimumElementCount,
			FrontendProbePressedEventCount));
		return;
	}
	const bool bReceiptWritten = WriteFrontendShippingProbeReceipt(
		true,
		TEXT("complete"));
	bFrontendShippingProbe = false;
	FPlatformMisc::RequestExitWithStatus(true, bReceiptWritten ? 0 : 3);
}

void AIGPlayerController::FailFrontendShippingProbe(const FString& Reason)
{
	WriteFrontendShippingProbeReceipt(false, Reason);
	bFrontendShippingProbe = false;
	FPlatformMisc::RequestExitWithStatus(true, 2);
}

void AIGPlayerController::StartMissingFloorJournalPreviewProbe()
{
	const TCHAR* CommandLine = FCommandLine::Get();
	FParse::Value(
		CommandLine,
		TEXT("IGJournalPreviewExpectedWidth="),
		MissingFloorJournalPreviewExpectedWidth);
	FParse::Value(
		CommandLine,
		TEXT("IGJournalPreviewExpectedHeight="),
		MissingFloorJournalPreviewExpectedHeight);
	FParse::Value(
		CommandLine,
		TEXT("IGJournalPreviewScreenshotPath="),
		MissingFloorJournalPreviewScreenshotPath);
	MissingFloorJournalPreviewScreenshotPath.TrimQuotesInline();
	MissingFloorJournalPreviewScreenshotPath = FPaths::ConvertRelativePathToFull(
		MissingFloorJournalPreviewScreenshotPath);
	if (MissingFloorJournalPreviewExpectedWidth <= 0
		|| MissingFloorJournalPreviewExpectedHeight <= 0
		|| MissingFloorJournalPreviewScreenshotPath.IsEmpty())
	{
		FailMissingFloorJournalPreviewProbe(TEXT("arguments_missing"));
		return;
	}

	UIGMissingFloorNarrativeSubsystem* Narrative = GetGameInstance()
		? GetGameInstance()->GetSubsystem<UIGMissingFloorNarrativeSubsystem>()
		: nullptr;
	if (!Narrative)
	{
		FailMissingFloorJournalPreviewProbe(TEXT("narrative_missing"));
		return;
	}
	Narrative->ResetNarrative();
	auto AddSource = [Narrative](
		const EIGMissingFloorTruth Truth,
		const EIGMissingFloorSource Source)
	{
		Narrative->RegisterTruthSource(Truth, Source);
	};
	AddSource(EIGMissingFloorTruth::LivedUpstairs,
		EIGMissingFloorSource::MeterFifthDial);
	AddSource(EIGMissingFloorTruth::LivedUpstairs,
		EIGMissingFloorSource::MeterReadingSheet);
	AddSource(EIGMissingFloorTruth::TenantIdentity,
		EIGMissingFloorSource::ShippingLabels);
	AddSource(EIGMissingFloorTruth::TenantIdentity,
		EIGMissingFloorSource::TunerNotebookName);
	AddSource(EIGMissingFloorTruth::NoiseWasHomecoming,
		EIGMissingFloorSource::NoiseForumPosts);
	AddSource(EIGMissingFloorTruth::NoiseWasHomecoming,
		EIGMissingFloorSource::TunerWorkSchedule);
	AddSource(EIGMissingFloorTruth::LandingStruggle,
		EIGMissingFloorSource::ForumFinalPost);
	AddSource(EIGMissingFloorTruth::LandingStruggle,
		EIGMissingFloorSource::LandingImpactMark);
	AddSource(EIGMissingFloorTruth::WallSealedThatDay,
		EIGMissingFloorSource::BoardDeliveryReceipt);
	AddSource(EIGMissingFloorTruth::WallSealedThatDay,
		EIGMissingFloorSource::FreshPlasterDating);
	AddSource(EIGMissingFloorTruth::FiveNightsOfThirst,
		EIGMissingFloorSource::KnockTallyJournal);
	AddSource(EIGMissingFloorTruth::FiveNightsOfThirst,
		EIGMissingFloorSource::TankWaterAudition);

	float RequestedTextScale = 1.0f;
	FParse::Value(
		CommandLine,
		TEXT("IGJournalPreviewTextScale="),
		RequestedTextScale);
	if (UIGAccessibilitySubsystem* Accessibility =
		GetAccessibilitySubsystem())
	{
		FIGAccessibilitySettings Settings = Accessibility->GetSettings();
		Settings.CaptionSizeScale = FMath::Clamp(RequestedTextScale, 0.85f, 2.0f);
		Accessibility->ApplySettings(Settings);
	}

	bMissingFloorJournalPreviewProbe = true;
	bMissingFloorJournalPreviewScreenshotRequested = false;
	bMissingFloorJournalPreviewCompilationDrained = false;
	SystemMenuMode = EIGSystemMenuMode::Hidden;
	bAccessibilityMenuVisible = false;
	SetInputDevicePresentation(false);
	// The production path pauses in OpenMissingFloorJournal. This probe keeps
	// the world running until the GPU has produced and saved one evidence frame;
	// pausing before the first offscreen present can starve the capture driver.
	bMissingFloorJournalVisible = true;
	MissingFloorJournalPage = 0;
	ApplyMenuInputMode();
	RefreshMenuHud();
	IFileManager::Get().MakeDirectory(
		*FPaths::GetPath(MissingFloorJournalPreviewScreenshotPath),
		true);
	const double Now = FPlatformTime::Seconds();
	MissingFloorJournalPreviewNextActionTime = Now + 0.75;
	MissingFloorJournalPreviewDeadline = Now + 8.0;
	SetActorTickEnabled(true);
}

void AIGPlayerController::TickMissingFloorJournalPreviewProbe()
{
	const double Now = FPlatformTime::Seconds();
	if (Now > MissingFloorJournalPreviewDeadline)
	{
		FailMissingFloorJournalPreviewProbe(TEXT("timeout"));
		return;
	}
	if (Now < MissingFloorJournalPreviewNextActionTime)
	{
		return;
	}
	if (!bMissingFloorJournalPreviewCompilationDrained)
	{
		FAssetCompilingManager::Get().FinishAllCompilation();
		if (GShaderCompilingManager)
		{
			GShaderCompilingManager->FinishAllCompilation();
		}
		if (GEngine)
		{
			GEngine->bEnableOnScreenDebugMessages = false;
		}
		ConsoleCommand(TEXT("DisableAllScreenMessages"), true);
		bMissingFloorJournalPreviewCompilationDrained = true;
		MissingFloorJournalPreviewNextActionTime = Now + 0.40;
		MissingFloorJournalPreviewDeadline = Now + 8.0;
		return;
	}
	if (!bMissingFloorJournalPreviewScreenshotRequested)
	{
		const AIGHorrorHUD* HorrorHUD = Cast<AIGHorrorHUD>(GetHUD());
		FVector2D CanvasSize;
		FVector2D BoundsMinimum;
		FVector2D BoundsMaximum;
		int32 ElementCount = 0;
		bool bInsideCanvas = false;
		bool bInsideSettingsContainers = false;
		uint64 FrameSerial = 0;
		const bool bLayoutReady = HorrorHUD
			&& HorrorHUD->IsMissingFloorJournalVisible()
			&& HorrorHUD->GetLayoutValidationSample(
				CanvasSize,
				BoundsMinimum,
				BoundsMaximum,
				ElementCount,
				bInsideCanvas,
				bInsideSettingsContainers,
				FrameSerial)
			&& FMath::Abs(
				CanvasSize.X - MissingFloorJournalPreviewExpectedWidth) <= 1.0f
			&& FMath::Abs(
				CanvasSize.Y - MissingFloorJournalPreviewExpectedHeight) <= 1.0f
			&& bInsideCanvas
			&& ElementCount >= 12
			&& FrameSerial > 0;
		if (!bLayoutReady)
		{
			MissingFloorJournalPreviewNextActionTime = Now + 0.05;
			return;
		}
		FScreenshotRequest::RequestScreenshot(
			MissingFloorJournalPreviewScreenshotPath,
			true,
			false);
		bMissingFloorJournalPreviewScreenshotRequested = true;
		MissingFloorJournalPreviewNextActionTime = Now + 0.08;
		return;
	}
	if (!FPaths::FileExists(MissingFloorJournalPreviewScreenshotPath))
	{
		MissingFloorJournalPreviewNextActionTime = Now + 0.05;
		return;
	}

	UE_LOG(
		LogTemp,
		Display,
		TEXT("MISSINGFLOOR_JOURNAL_PREVIEW PASS resolution=%dx%d path=%s"),
		MissingFloorJournalPreviewExpectedWidth,
		MissingFloorJournalPreviewExpectedHeight,
		*MissingFloorJournalPreviewScreenshotPath);
	bMissingFloorJournalPreviewProbe = false;
	FPlatformMisc::RequestExitWithStatus(true, 0);
}

void AIGPlayerController::FailMissingFloorJournalPreviewProbe(
	const FString& Reason) const
{
	UE_LOG(
		LogTemp,
		Error,
		TEXT("MISSINGFLOOR_JOURNAL_PREVIEW FAIL reason=%s"),
		*Reason);
	FPlatformMisc::RequestExitWithStatus(true, 2);
}

void AIGPlayerController::StartAudioCalibrationPreviewProbe()
{
	const TCHAR* CommandLine = FCommandLine::Get();
	FParse::Value(
		CommandLine,
		TEXT("IGAudioCalibrationExpectedWidth="),
		AudioCalibrationPreviewExpectedWidth);
	FParse::Value(
		CommandLine,
		TEXT("IGAudioCalibrationExpectedHeight="),
		AudioCalibrationPreviewExpectedHeight);
	FParse::Value(
		CommandLine,
		TEXT("IGAudioCalibrationScreenshotPath="),
		AudioCalibrationPreviewScreenshotPath);
	AudioCalibrationPreviewScreenshotPath.TrimQuotesInline();
	AudioCalibrationPreviewScreenshotPath = FPaths::ConvertRelativePathToFull(
		AudioCalibrationPreviewScreenshotPath);
	bAudioCalibrationPreviewProbe = true;
	if (AudioCalibrationPreviewExpectedWidth <= 0
		|| AudioCalibrationPreviewExpectedHeight <= 0
		|| AudioCalibrationPreviewScreenshotPath.IsEmpty())
	{
		FailAudioCalibrationPreviewProbe(TEXT("arguments_missing"));
		return;
	}

	bAudioCalibrationPreviewScreenshotRequested = false;
	bAudioCalibrationPreviewCompilationDrained = false;
	bAccessibilityMenuVisible = false;
	AudioCalibrationVolumeStep = 4;
	AudioCalibrationBrightnessStep = 2;
	AudioCalibrationReturnMode = EIGSystemMenuMode::Title;
	bAudioCalibrationSessionActive = true;
	bAudioCalibrationFirstRun = true;
	AudioCalibrationSelection = 0;
	SetInputDevicePresentation(false);
	SetSystemMenuMode(EIGSystemMenuMode::AudioCalibration);
	ApplyAudioCalibrationValues();
	PlayAudioCalibrationKnock();
	IFileManager::Get().MakeDirectory(
		*FPaths::GetPath(AudioCalibrationPreviewScreenshotPath),
		true);
	const double Now = FPlatformTime::Seconds();
	AudioCalibrationPreviewNextActionTime = Now + 0.75;
	AudioCalibrationPreviewDeadline = Now + 8.0;
	SetActorTickEnabled(true);
}

void AIGPlayerController::TickAudioCalibrationPreviewProbe()
{
	const double Now = FPlatformTime::Seconds();
	if (Now > AudioCalibrationPreviewDeadline)
	{
		FailAudioCalibrationPreviewProbe(TEXT("timeout"));
		return;
	}
	if (Now < AudioCalibrationPreviewNextActionTime)
	{
		return;
	}
	if (!bAudioCalibrationPreviewCompilationDrained)
	{
		FAssetCompilingManager::Get().FinishAllCompilation();
		if (GShaderCompilingManager)
		{
			GShaderCompilingManager->FinishAllCompilation();
		}
		if (GEngine)
		{
			GEngine->bEnableOnScreenDebugMessages = false;
		}
		ConsoleCommand(TEXT("DisableAllScreenMessages"), true);
		bAudioCalibrationPreviewCompilationDrained = true;
		AudioCalibrationPreviewNextActionTime = Now + 0.40;
		AudioCalibrationPreviewDeadline = Now + 8.0;
		return;
	}
	if (!bAudioCalibrationPreviewScreenshotRequested)
	{
		const AIGHorrorHUD* HorrorHUD = Cast<AIGHorrorHUD>(GetHUD());
		const UIGMissingFloorAudioSubsystem* AudioDirector = GetWorld()
			? GetWorld()->GetSubsystem<UIGMissingFloorAudioSubsystem>()
			: nullptr;
		FVector2D CanvasSize;
		FVector2D BoundsMinimum;
		FVector2D BoundsMaximum;
		int32 ElementCount = 0;
		bool bInsideCanvas = false;
		bool bInsideSettingsContainers = false;
		uint64 FrameSerial = 0;
		const bool bLayoutReady = HorrorHUD
			&& SystemMenuMode == EIGSystemMenuMode::AudioCalibration
			&& AudioDirector
			&& FMath::IsNearlyEqual(
				AudioDirector->GetUserMasterVolume(),
				IGAudioCalibration::VolumeValues[AudioCalibrationVolumeStep],
				0.001f)
			&& AudioDirector->GetCalibrationKnockPlayCount() >= 1
			&& HorrorHUD->GetLayoutValidationSample(
				CanvasSize,
				BoundsMinimum,
				BoundsMaximum,
				ElementCount,
				bInsideCanvas,
				bInsideSettingsContainers,
				FrameSerial)
			&& FMath::Abs(
				CanvasSize.X - AudioCalibrationPreviewExpectedWidth) <= 1.0f
			&& FMath::Abs(
				CanvasSize.Y - AudioCalibrationPreviewExpectedHeight) <= 1.0f
			&& bInsideCanvas
			&& ElementCount >= 1
			&& FrameSerial > 0;
		if (!bLayoutReady)
		{
			AudioCalibrationPreviewNextActionTime = Now + 0.05;
			return;
		}
		FScreenshotRequest::RequestScreenshot(
			AudioCalibrationPreviewScreenshotPath,
			true,
			false);
		bAudioCalibrationPreviewScreenshotRequested = true;
		AudioCalibrationPreviewNextActionTime = Now + 0.08;
		return;
	}
	if (!FPaths::FileExists(AudioCalibrationPreviewScreenshotPath))
	{
		AudioCalibrationPreviewNextActionTime = Now + 0.05;
		return;
	}

	UE_LOG(
		LogTemp,
		Display,
		TEXT("MISSINGFLOOR_AUDIO_CALIBRATION_PREVIEW PASS "
			"resolution=%dx%d volume_step=%d brightness_step=%d "
			"master_gain=%.3f hrtf_knock=1 path=%s"),
		AudioCalibrationPreviewExpectedWidth,
		AudioCalibrationPreviewExpectedHeight,
		AudioCalibrationVolumeStep,
		AudioCalibrationBrightnessStep,
		IGAudioCalibration::VolumeValues[AudioCalibrationVolumeStep],
		*AudioCalibrationPreviewScreenshotPath);
	bAudioCalibrationPreviewProbe = false;
	FPlatformMisc::RequestExitWithStatus(true, 0);
}

void AIGPlayerController::FailAudioCalibrationPreviewProbe(
	const FString& Reason) const
{
	UE_LOG(
		LogTemp,
		Error,
		TEXT("MISSINGFLOOR_AUDIO_CALIBRATION_PREVIEW FAIL reason=%s"),
		*Reason);
	FPlatformMisc::RequestExitWithStatus(true, 2);
}

void AIGPlayerController::AddInputLock(
	const FName Reason,
	const bool bLockMove,
	const bool bLockLook)
{
	if (Reason.IsNone() || (!bLockMove && !bLockLook))
	{
		return;
	}
	InputLockReasons.Add(Reason, TPair<bool, bool>(bLockMove, bLockLook));
	ApplyInputLocks();
}

void AIGPlayerController::RemoveInputLock(const FName Reason)
{
	if (InputLockReasons.Remove(Reason) > 0)
	{
		ApplyInputLocks();
	}
}

FString AIGPlayerController::DescribeInputLocks() const
{
	if (InputLockReasons.IsEmpty())
	{
		return TEXT("none");
	}
	TArray<FString> Names;
	Names.Reserve(InputLockReasons.Num());
	for (const TPair<FName, TPair<bool, bool>>& Entry : InputLockReasons)
	{
		Names.Add(FString::Printf(
			TEXT("%s(%s%s)"),
			*Entry.Key.ToString(),
			Entry.Value.Key ? TEXT("move") : TEXT(""),
			Entry.Value.Value ? TEXT("+look") : TEXT("")));
	}
	Names.Sort();
	return FString::Join(Names, TEXT(", "));
}

void AIGPlayerController::ApplyInputLocks()
{
	bool bMove = false;
	bool bLook = false;
	for (const TPair<FName, TPair<bool, bool>>& Entry : InputLockReasons)
	{
		bMove |= Entry.Value.Key;
		bLook |= Entry.Value.Value;
	}
	// 카운터를 0으로 되돌린 뒤 필요한 만큼만 다시 올린다. 이렇게 해야 두 번
	// 걸거나 해제 순서가 엇갈려도 남은 이름과 실제 무시 상태가 어긋나지 않는다.
	ResetIgnoreMoveInput();
	ResetIgnoreLookInput();
	if (bMove)
	{
		SetIgnoreMoveInput(true);
	}
	if (bLook)
	{
		SetIgnoreLookInput(true);
	}
	if (InputLockReasons.IsEmpty())
	{
		InputLockWatchdogNextReportTime = 0.0;
	}
	else if (InputLockWatchdogNextReportTime <= 0.0)
	{
		// 잠금이 오래 남아 있으면 누가 붙들고 있는지 로그로 말한다. 조작이
		// 죽었는데 아무것도 안 찍히는 상태가 이 결함을 추적 불가능하게 만든다.
		InputLockWatchdogNextReportTime =
			FPlatformTime::Seconds() + IGInputLocks::WatchdogSeconds;
		SetActorTickEnabled(true);
	}
}

void AIGPlayerController::StartDisplaySettingsPreviewProbe()
{
	const TCHAR* CommandLine = FCommandLine::Get();
	FParse::Value(
		CommandLine,
		TEXT("IGDisplaySettingsExpectedWidth="),
		DisplaySettingsPreviewExpectedWidth);
	FParse::Value(
		CommandLine,
		TEXT("IGDisplaySettingsExpectedHeight="),
		DisplaySettingsPreviewExpectedHeight);
	FParse::Value(
		CommandLine,
		TEXT("IGDisplaySettingsScreenshotPath="),
		DisplaySettingsPreviewScreenshotPath);
	DisplaySettingsPreviewScreenshotPath.TrimQuotesInline();
	DisplaySettingsPreviewScreenshotPath = FPaths::ConvertRelativePathToFull(
		DisplaySettingsPreviewScreenshotPath);
	bDisplaySettingsPreviewProbe = true;
	if (DisplaySettingsPreviewExpectedWidth <= 0
		|| DisplaySettingsPreviewExpectedHeight <= 0
		|| DisplaySettingsPreviewScreenshotPath.IsEmpty())
	{
		FailDisplaySettingsPreviewProbe(TEXT("arguments_missing"));
		return;
	}

	bDisplaySettingsPreviewScreenshotRequested = false;
	bDisplaySettingsPreviewCompilationDrained = false;
	bAccessibilityMenuVisible = false;
	// 저장된 값을 그대로 읽어 화면에 세운다. 프리뷰가 임의의 값을 보여 주면
	// 증빙으로 쓸 수 없다.
	RefreshStagedDisplaySettings();
	// 어느 줄을 세워 둘지 고를 수 있어야 카테고리별로 증빙을 남길 수 있다.
	int32 RequestedRow = 0;
	FParse::Value(CommandLine, TEXT("IGDisplaySettingsRow="), RequestedRow);
	DisplaySettingsSelection = FMath::Clamp(RequestedRow, 0, 8);
	SetInputDevicePresentation(false);
	SetSystemMenuMode(EIGSystemMenuMode::DisplaySettings);
	// SetSystemMenuMode는 모드만 바꾼다. HUD가 이 화면을 그리려면 표시 상태를
	// 한 번 밀어 줘야 한다.
	RefreshMenuHud();
	IFileManager::Get().MakeDirectory(
		*FPaths::GetPath(DisplaySettingsPreviewScreenshotPath),
		true);
	const double Now = FPlatformTime::Seconds();
	DisplaySettingsPreviewNextActionTime = Now + 0.75;
	DisplaySettingsPreviewDeadline = Now + 8.0;
	SetActorTickEnabled(true);
}

void AIGPlayerController::TickDisplaySettingsPreviewProbe()
{
	const double Now = FPlatformTime::Seconds();
	if (Now > DisplaySettingsPreviewDeadline)
	{
		const AIGHorrorHUD* TimedOutHud = Cast<AIGHorrorHUD>(GetHUD());
		FVector2D TimedOutCanvas = FVector2D::ZeroVector;
		FVector2D Unused0;
		FVector2D Unused1;
		int32 TimedOutElements = 0;
		bool bTimedOutInsideCanvas = false;
		bool bTimedOutInsideContainers = false;
		uint64 TimedOutSerial = 0;
		const bool bSampled = TimedOutHud
			&& TimedOutHud->GetLayoutValidationSample(
				TimedOutCanvas, Unused0, Unused1, TimedOutElements,
				bTimedOutInsideCanvas, bTimedOutInsideContainers,
				TimedOutSerial);
		UE_LOG(
			LogTemp,
			Error,
			TEXT("MISSINGFLOOR_DISPLAY_SETTINGS_PREVIEW DIAG hud=%d mode=%d "
				"sampled=%d canvas=%.0fx%.0f elements=%d inside=%d serial=%llu"),
			TimedOutHud ? 1 : 0,
			static_cast<int32>(SystemMenuMode),
			bSampled ? 1 : 0,
			TimedOutCanvas.X,
			TimedOutCanvas.Y,
			TimedOutElements,
			bTimedOutInsideCanvas ? 1 : 0,
			TimedOutSerial);
		FailDisplaySettingsPreviewProbe(TEXT("timeout"));
		return;
	}
	if (Now < DisplaySettingsPreviewNextActionTime)
	{
		return;
	}
	if (!bDisplaySettingsPreviewCompilationDrained)
	{
		FAssetCompilingManager::Get().FinishAllCompilation();
		if (GShaderCompilingManager)
		{
			GShaderCompilingManager->FinishAllCompilation();
		}
		if (GEngine)
		{
			GEngine->bEnableOnScreenDebugMessages = false;
		}
		ConsoleCommand(TEXT("DisableAllScreenMessages"), true);
		bDisplaySettingsPreviewCompilationDrained = true;
		DisplaySettingsPreviewNextActionTime = Now + 0.40;
		DisplaySettingsPreviewDeadline = Now + 8.0;
		return;
	}
	if (!bDisplaySettingsPreviewScreenshotRequested)
	{
		// BeginPlay 시점에는 HUD가 아직 없을 수 있다. 표시 상태를 한 번만
		// 밀면 그 호출이 통째로 헛돌고, 화면은 영원히 열리지 않는다.
		RefreshMenuHud();
		const AIGHorrorHUD* HorrorHUD = Cast<AIGHorrorHUD>(GetHUD());
		FVector2D CanvasSize;
		FVector2D BoundsMinimum;
		FVector2D BoundsMaximum;
		int32 ElementCount = 0;
		bool bInsideCanvas = false;
		bool bInsideSettingsContainers = false;
		uint64 FrameSerial = 0;
		const bool bLayoutReady = HorrorHUD
			&& SystemMenuMode == EIGSystemMenuMode::DisplaySettings
			&& HorrorHUD->GetLayoutValidationSample(
				CanvasSize,
				BoundsMinimum,
				BoundsMaximum,
				ElementCount,
				bInsideCanvas,
				bInsideSettingsContainers,
				FrameSerial)
			&& FMath::Abs(
				CanvasSize.X - DisplaySettingsPreviewExpectedWidth) <= 1.0f
			&& FMath::Abs(
				CanvasSize.Y - DisplaySettingsPreviewExpectedHeight) <= 1.0f
			&& bInsideCanvas
			&& ElementCount >= 1
			&& FrameSerial > 0;
		if (!bLayoutReady)
		{
			DisplaySettingsPreviewNextActionTime = Now + 0.05;
			return;
		}
		FScreenshotRequest::RequestScreenshot(
			DisplaySettingsPreviewScreenshotPath,
			true,
			false);
		bDisplaySettingsPreviewScreenshotRequested = true;
		DisplaySettingsPreviewNextActionTime = Now + 0.08;
		return;
	}
	if (!FPaths::FileExists(DisplaySettingsPreviewScreenshotPath))
	{
		DisplaySettingsPreviewNextActionTime = Now + 0.05;
		return;
	}

	UE_LOG(
		LogTemp,
		Display,
		TEXT("MISSINGFLOOR_DISPLAY_SETTINGS_PREVIEW PASS "
			"resolution=%dx%d awaiting_confirmation=%d path=%s"),
		DisplaySettingsPreviewExpectedWidth,
		DisplaySettingsPreviewExpectedHeight,
		bDisplaySettingsAwaitingConfirmation ? 1 : 0,
		*DisplaySettingsPreviewScreenshotPath);
	bDisplaySettingsPreviewProbe = false;
	FPlatformMisc::RequestExitWithStatus(true, 0);
}

void AIGPlayerController::FailDisplaySettingsPreviewProbe(
	const FString& Reason) const
{
	UE_LOG(
		LogTemp,
		Error,
		TEXT("MISSINGFLOOR_DISPLAY_SETTINGS_PREVIEW FAIL reason=%s"),
		*Reason);
	FPlatformMisc::RequestExitWithStatus(true, 2);
}

void AIGPlayerController::StartMissingFloorEndingPreviewProbe()
{
	const TCHAR* CommandLine = FCommandLine::Get();
	FParse::Value(
		CommandLine,
		TEXT("IGEndingPreviewExpectedWidth="),
		MissingFloorEndingPreviewExpectedWidth);
	FParse::Value(
		CommandLine,
		TEXT("IGEndingPreviewExpectedHeight="),
		MissingFloorEndingPreviewExpectedHeight);
	FParse::Value(
		CommandLine,
		TEXT("IGEndingPreviewScreenshotPath="),
		MissingFloorEndingPreviewScreenshotPath);
	FParse::Value(
		CommandLine,
		TEXT("IGEndingPreviewElapsed="),
		MissingFloorEndingPreviewElapsedSeconds);
	MissingFloorEndingPreviewScreenshotPath.TrimQuotesInline();
	MissingFloorEndingPreviewScreenshotPath = FPaths::ConvertRelativePathToFull(
		MissingFloorEndingPreviewScreenshotPath);
	MissingFloorEndingPreviewElapsedSeconds = FMath::Clamp(
		MissingFloorEndingPreviewElapsedSeconds,
		0.0f,
		10.0f);
	if (MissingFloorEndingPreviewExpectedWidth <= 0
		|| MissingFloorEndingPreviewExpectedHeight <= 0
		|| MissingFloorEndingPreviewScreenshotPath.IsEmpty())
	{
		FailMissingFloorEndingPreviewProbe(TEXT("arguments_missing"));
		return;
	}

	bMissingFloorEndingPreviewProbe = true;
	bMissingFloorEndingPreviewScreenshotRequested = false;
	bMissingFloorEndingPreviewCompilationDrained = false;
	SystemMenuMode = EIGSystemMenuMode::Hidden;
	bAccessibilityMenuVisible = false;
	bMissingFloorJournalVisible = false;
	SetInputDevicePresentation(false);
	ApplyMenuInputMode();
	RefreshMenuHud();
	IFileManager::Get().MakeDirectory(
		*FPaths::GetPath(MissingFloorEndingPreviewScreenshotPath),
		true);
	MissingFloorEndingPreviewNextActionTime = FPlatformTime::Seconds() + 0.75;
	MissingFloorEndingPreviewDeadline = FPlatformTime::Seconds() + 10.0;
	SetActorTickEnabled(true);
}

void AIGPlayerController::TickMissingFloorEndingPreviewProbe()
{
	const double Now = FPlatformTime::Seconds();
	if (Now > MissingFloorEndingPreviewDeadline)
	{
		FailMissingFloorEndingPreviewProbe(TEXT("timeout"));
		return;
	}
	if (Now < MissingFloorEndingPreviewNextActionTime)
	{
		return;
	}
	if (!bMissingFloorEndingPreviewCompilationDrained)
	{
		FAssetCompilingManager::Get().FinishAllCompilation();
		if (GShaderCompilingManager)
		{
			GShaderCompilingManager->FinishAllCompilation();
		}
		if (GEngine)
		{
			GEngine->bEnableOnScreenDebugMessages = false;
		}
		ConsoleCommand(TEXT("DisableAllScreenMessages"), true);
		bMissingFloorEndingPreviewCompilationDrained = true;
		MissingFloorEndingPreviewNextActionTime = Now + 0.40;
		MissingFloorEndingPreviewDeadline = Now + 8.0;
		return;
	}

	AIGHorrorHUD* HorrorHUD = Cast<AIGHorrorHUD>(GetHUD());
	if (!HorrorHUD)
	{
		FailMissingFloorEndingPreviewProbe(TEXT("hud_missing"));
		return;
	}
	if (!HorrorHUD->IsMissingFloorFailureEndingVisible())
	{
		HorrorHUD->BeginMissingFloorFailureEnding(
			MissingFloorEndingPreviewElapsedSeconds);
		HorrorHUD->SetMissingFloorFailureRetryEnabled(
			MissingFloorEndingPreviewElapsedSeconds >= 3.2f);
		MissingFloorEndingPreviewNextActionTime = Now + 0.10;
		return;
	}

	if (!bMissingFloorEndingPreviewScreenshotRequested)
	{
		FVector2D CanvasSize;
		FVector2D BoundsMinimum;
		FVector2D BoundsMaximum;
		int32 ElementCount = 0;
		bool bInsideCanvas = false;
		bool bInsideSettingsContainers = false;
		uint64 FrameSerial = 0;
		const int32 MinimumElementCount =
			MissingFloorEndingPreviewElapsedSeconds >= 3.2f ? 9 : 7;
		const bool bLayoutReady = HorrorHUD->GetLayoutValidationSample(
				CanvasSize,
				BoundsMinimum,
				BoundsMaximum,
				ElementCount,
				bInsideCanvas,
				bInsideSettingsContainers,
				FrameSerial)
			&& FMath::Abs(
				CanvasSize.X - MissingFloorEndingPreviewExpectedWidth) <= 1.0f
			&& FMath::Abs(
				CanvasSize.Y - MissingFloorEndingPreviewExpectedHeight) <= 1.0f
			&& bInsideCanvas
			&& ElementCount >= MinimumElementCount
			&& FrameSerial > 0;
		if (!bLayoutReady)
		{
			MissingFloorEndingPreviewNextActionTime = Now + 0.05;
			return;
		}
		FScreenshotRequest::RequestScreenshot(
			MissingFloorEndingPreviewScreenshotPath,
			true,
			false);
		bMissingFloorEndingPreviewScreenshotRequested = true;
		MissingFloorEndingPreviewNextActionTime = Now + 0.08;
		return;
	}
	if (!FPaths::FileExists(MissingFloorEndingPreviewScreenshotPath))
	{
		MissingFloorEndingPreviewNextActionTime = Now + 0.05;
		return;
	}

	UE_LOG(
		LogTemp,
		Display,
		TEXT("MISSINGFLOOR_ENDING_PREVIEW PASS resolution=%dx%d elapsed=%.2f path=%s"),
		MissingFloorEndingPreviewExpectedWidth,
		MissingFloorEndingPreviewExpectedHeight,
		MissingFloorEndingPreviewElapsedSeconds,
		*MissingFloorEndingPreviewScreenshotPath);
	bMissingFloorEndingPreviewProbe = false;
	FPlatformMisc::RequestExitWithStatus(true, 0);
}

void AIGPlayerController::FailMissingFloorEndingPreviewProbe(
	const FString& Reason) const
{
	UE_LOG(
		LogTemp,
		Error,
		TEXT("MISSINGFLOOR_ENDING_PREVIEW FAIL reason=%s"),
		*Reason);
	FPlatformMisc::RequestExitWithStatus(true, 2);
}

bool AIGPlayerController::WriteFrontendShippingProbeReceipt(
	const bool bSuccess,
	const FString& Reason) const
{
	FString ResultPath;
	if (!FParse::Value(
		FCommandLine::Get(),
		TEXT("IGFrontendResultPath="),
		ResultPath))
	{
		return false;
	}
	ResultPath.TrimQuotesInline();
	if (ResultPath.IsEmpty())
	{
		return false;
	}
	const FString FullPath = FPaths::ConvertRelativePathToFull(ResultPath);
	IFileManager::Get().MakeDirectory(
		*FPaths::GetPath(FullPath),
		true);
	FString Receipt;
	if (bSuccess)
	{
		Receipt = FString::Printf(
			TEXT("REBIRTH_FRONTEND PASS contract=4 resolution=%dx%d ")
			TEXT("keyboard_access=1 gamepad_access=1 dpad_down=1 ")
			TEXT("keyboard_up=1 gamepad_close=1 keyboard_pause=1 ")
			TEXT("gamepad_pause=1 display=1 title=1 first_run=1 ")
			TEXT("dialogue=1 dialogue_default=1 ")
			TEXT("speaker=1 continuation=1 default_scale=100 max_scale=200 ")
			TEXT("sound_lane=1 ")
			TEXT("samples=%d elements_min=%d ")
			TEXT("input_events=%d bounds=%d,%d,%d,%d"),
			FrontendProbeExpectedWidth,
			FrontendProbeExpectedHeight,
			FrontendProbeLayoutSampleCount,
			FrontendProbeMinimumElementCount,
			FrontendProbePressedEventCount,
			FMath::RoundToInt(FrontendProbeBoundsMin.X),
			FMath::RoundToInt(FrontendProbeBoundsMin.Y),
			FMath::RoundToInt(FrontendProbeBoundsMax.X),
			FMath::RoundToInt(FrontendProbeBoundsMax.Y));
	}
	else
	{
		FString SafeReason = Reason;
		SafeReason.ReplaceInline(TEXT("\r"), TEXT("_"));
		SafeReason.ReplaceInline(TEXT("\n"), TEXT("_"));
		SafeReason.ReplaceInline(TEXT(" "), TEXT("_"));
		Receipt = FString::Printf(
			TEXT("REBIRTH_FRONTEND FAIL reason=%s step=%d samples=%d inputs=%d"),
			*SafeReason,
			FrontendProbeStep,
			FrontendProbeLayoutSampleCount,
			FrontendProbePressedEventCount);
	}
	return FFileHelper::SaveStringToFile(
		Receipt,
		*FullPath,
		FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
}

void AIGPlayerController::ToggleSystemMenu()
{
	if (bMissingFloorJournalVisible)
	{
		CloseMissingFloorJournal();
		return;
	}
	if (bAccessibilityMenuVisible)
	{
		CloseAccessibilityMenu();
		return;
	}

	switch (SystemMenuMode)
	{
	case EIGSystemMenuMode::Title:
		if (bNewGameConfirmationArmed)
		{
			bNewGameConfirmationArmed = false;
			RefreshMenuHud();
		}
		// The title is an intentional gate. A stray Esc must not start the story.
		return;
	case EIGSystemMenuMode::Credits:
		ReturnFromCredits();
		return;
	case EIGSystemMenuMode::ContentNotice:
		// 읽고 넘어가는 화면이라 확인과 취소가 같은 뜻이다. 뒤로 갈 데가 없다.
		DismissContentNotice();
		return;
	case EIGSystemMenuMode::KeyBindings:
		if (bKeyBindingCapturing)
		{
			// 대기 중의 취소는 화면을 닫는 것이 아니라 그 한 칸을 포기하는 것이다.
			bKeyBindingCapturing = false;
			KeyBindingStatusText = FText::GetEmpty();
			RefreshMenuHud();
			return;
		}
		CloseKeyBindings();
		return;
	case EIGSystemMenuMode::AudioCalibration:
		CancelAudioCalibration();
		return;
	case EIGSystemMenuMode::DisplaySettings:
		if (bDisplaySettingsAwaitingConfirmation)
		{
			RevertPendingDisplaySettings();
		}
		else
		{
			ReturnFromDisplaySettings();
		}
		return;
	case EIGSystemMenuMode::Pause:
		SetSystemMenuMode(EIGSystemMenuMode::Hidden);
		return;
	case EIGSystemMenuMode::Hidden:
	default:
		SystemMenuSelection = 0;
		SetSystemMenuMode(EIGSystemMenuMode::Pause);
		return;
	}
}

void AIGPlayerController::ToggleAccessibilityMenu()
{
	if (bMissingFloorJournalVisible)
	{
		CloseMissingFloorJournal();
		return;
	}
	if (bAccessibilityMenuVisible)
	{
		CloseAccessibilityMenu();
		return;
	}

	bAccessibilityMenuVisible = true;
	bNewGameConfirmationArmed = false;
	AccessibilityReturnMode = SystemMenuMode;
	bGameWasPausedBeforeAccessibility =
		UGameplayStatics::IsGamePaused(this);
	SetPause(true);
	ApplyMenuInputMode();
	RefreshMenuHud();
}

void AIGPlayerController::CloseAccessibilityMenu()
{
	if (bMissingFloorJournalVisible)
	{
		CloseMissingFloorJournal();
		return;
	}
	if (!bAccessibilityMenuVisible)
	{
		if (SystemMenuMode == EIGSystemMenuMode::Credits)
		{
			ReturnFromCredits();
		}
		else if (SystemMenuMode == EIGSystemMenuMode::Pause)
		{
			SetSystemMenuMode(EIGSystemMenuMode::Hidden);
		}
		else if (SystemMenuMode == EIGSystemMenuMode::DisplaySettings)
		{
			if (bDisplaySettingsAwaitingConfirmation)
			{
				RevertPendingDisplaySettings();
			}
			else
			{
				ReturnFromDisplaySettings();
			}
		}
		else if (SystemMenuMode == EIGSystemMenuMode::AudioCalibration)
		{
			CancelAudioCalibration();
		}
		return;
	}
	bAccessibilityMenuVisible = false;
	const bool bReturnsToSystemMenu =
		AccessibilityReturnMode == EIGSystemMenuMode::Title
		|| AccessibilityReturnMode == EIGSystemMenuMode::Pause
		|| AccessibilityReturnMode == EIGSystemMenuMode::AudioCalibration
		|| AccessibilityReturnMode == EIGSystemMenuMode::DisplaySettings;
	SetPause(bReturnsToSystemMenu || bGameWasPausedBeforeAccessibility);
	AccessibilityReturnMode = EIGSystemMenuMode::Hidden;
	ApplyMenuInputMode();
	RefreshMenuHud();
}

void AIGPlayerController::BeginJournalInput()
{
	if (bMissingFloorJournalVisible)
	{
		CloseMissingFloorJournal();
		return;
	}
	if (bAccessibilityMenuVisible
		|| SystemMenuMode != EIGSystemMenuMode::Hidden)
	{
		return;
	}
	if (UWorld* World = GetWorld())
	{
		for (TActorIterator<AIGMissingFloorFifthDawnDirector> It(World); It; ++It)
		{
			if (It->BeginReplaySkipInput())
			{
				return;
			}
		}
		// 에필로그도 같은 손짓을 쓴다. 두 장면이 동시에 살아 있는 경로는
		// 없으므로 순서만 정하면 충돌하지 않는다.
		for (TActorIterator<AIGMissingFloorEpilogueDirector> It(World); It; ++It)
		{
			if (It->BeginReplaySkipInput())
			{
				return;
			}
		}
	}
	if (IsMissingFloorNight())
	{
		AIGHorrorHUD::PushThought(
			this,
			NSLOCTEXT(
				"IGMissingFloorJournal",
				"NightDenied",
				"지금은 그럴 때가 아니다."),
			2.2f);
		return;
	}
	if (const UIGAccessibilitySubsystem* Accessibility =
		GetAccessibilitySubsystem())
	{
		if (Accessibility->UsesToggleHoldInteractions())
		{
			OpenMissingFloorJournal();
			return;
		}
	}

	bJournalInputHeld = true;
	JournalInputPressedAt = FPlatformTime::Seconds();
	SetActorTickEnabled(true);
}

void AIGPlayerController::EndJournalInput()
{
	if (UWorld* World = GetWorld())
	{
		for (TActorIterator<AIGMissingFloorFifthDawnDirector> It(World); It; ++It)
		{
			if (It->EndReplaySkipInput())
			{
				return;
			}
		}
		for (TActorIterator<AIGMissingFloorEpilogueDirector> It(World); It; ++It)
		{
			if (It->EndReplaySkipInput())
			{
				return;
			}
		}
	}
	bJournalInputHeld = false;
}

void AIGPlayerController::OpenMissingFloorJournal()
{
	if (bMissingFloorJournalVisible || IsMissingFloorNight())
	{
		return;
	}

	bMissingFloorJournalVisible = true;
	MissingFloorJournalPage = 0;
	bGameWasPausedBeforeJournal = UGameplayStatics::IsGamePaused(this);
	PlayMissingFloorJournalPaperSound(1.0f);
	SetPause(true);
	ApplyMenuInputMode();
	RefreshMenuHud();
}

void AIGPlayerController::CloseMissingFloorJournal()
{
	if (!bMissingFloorJournalVisible)
	{
		return;
	}

	bMissingFloorJournalVisible = false;
	bJournalInputHeld = false;
	PlayMissingFloorJournalPaperSound(0.72f);
	SetPause(bGameWasPausedBeforeJournal);
	ApplyMenuInputMode();
	RefreshMenuHud();
}

void AIGPlayerController::MoveMissingFloorJournalPage(const int32 Direction)
{
	if (!bMissingFloorJournalVisible || Direction == 0)
	{
		return;
	}

	const AIGHorrorHUD* HorrorHUD = Cast<AIGHorrorHUD>(GetHUD());
	const int32 PageCount = HorrorHUD
		? FMath::Max(1, HorrorHUD->GetMissingFloorJournalPageCount())
		: 1;
	MissingFloorJournalPage =
		(MissingFloorJournalPage + PageCount + FMath::Sign(Direction)) % PageCount;
	PlayMissingFloorJournalPaperSound(0.58f);
	RefreshMenuHud();
}

void AIGPlayerController::MoveMissingFloorJournalPageLeft()
{
	MoveMissingFloorJournalPage(-1);
}

void AIGPlayerController::MoveMissingFloorJournalPageRight()
{
	MoveMissingFloorJournalPage(1);
}

bool AIGPlayerController::IsMissingFloorNight() const
{
	if (const AIGHorrorHUD* HorrorHUD = Cast<AIGHorrorHUD>(GetHUD()))
	{
		if (HorrorHUD->IsNightPresentation())
		{
			return true;
		}
	}
	const UGameInstance* GameInstance = GetGameInstance();
	const UIGMissingFloorNarrativeSubsystem* Narrative = GameInstance
		? GameInstance->GetSubsystem<UIGMissingFloorNarrativeSubsystem>()
		: nullptr;
	return Narrative && Narrative->IsHourSealed();
}

void AIGPlayerController::PlayMissingFloorJournalPaperSound(
	const float VolumeMultiplier) const
{
	if (UIGToneSequenceSoundWave* Paper =
		UIGToneSequenceSoundWave::CreateJournalPageTurn(
			const_cast<AIGPlayerController*>(this)))
	{
		UIGMissingFloorAudioSubsystem* AudioDirector = GetWorld()
			? GetWorld()->GetSubsystem<UIGMissingFloorAudioSubsystem>()
			: nullptr;
		if (AudioDirector)
		{
			AudioDirector->PrepareSound(Paper, EIGAudioBus::UI);
		}
		UAudioComponent* PaperVoice = UGameplayStatics::SpawnSound2D(
			this,
			Paper,
			FMath::Clamp(VolumeMultiplier, 0.0f, 1.0f));
		if (AudioDirector)
		{
			AudioDirector->RegisterComponent(PaperVoice, EIGAudioBus::UI);
		}
	}
}

void AIGPlayerController::MoveAccessibilitySelectionUp()
{
	if (!bAccessibilityMenuVisible)
	{
		MoveSystemMenuSelection(-1);
		return;
	}
	AccessibilitySelection =
		(AccessibilitySelection + IGAccessibilityMenu::RowCount - 1)
		% IGAccessibilityMenu::RowCount;
	RefreshMenuHud();
}

void AIGPlayerController::MoveAccessibilitySelectionDown()
{
	if (!bAccessibilityMenuVisible)
	{
		MoveSystemMenuSelection(1);
		return;
	}
	AccessibilitySelection =
		(AccessibilitySelection + 1) % IGAccessibilityMenu::RowCount;
	RefreshMenuHud();
}

void AIGPlayerController::AdjustAccessibilityLeft()
{
	if (!bAccessibilityMenuVisible)
	{
		if (SystemMenuMode == EIGSystemMenuMode::DisplaySettings)
		{
			AdjustDisplaySetting(-1);
		}
		else if (SystemMenuMode == EIGSystemMenuMode::AudioCalibration)
		{
			AdjustAudioCalibrationSetting(-1);
		}
		else if (SystemMenuMode == EIGSystemMenuMode::KeyBindings)
		{
			// 좌우는 값이 아니라 칸을 고른다. 키보드와 패드를 같은 화면에
			// 나란히 두고, 어느 쪽을 바꾸는지 손이 먼저 알게 한다.
			MoveKeyBindingColumn(-1);
		}
		return;
	}
	ChangeAccessibilitySetting(-1, false);
}

void AIGPlayerController::AdjustAccessibilityRight()
{
	if (!bAccessibilityMenuVisible)
	{
		if (SystemMenuMode == EIGSystemMenuMode::DisplaySettings)
		{
			AdjustDisplaySetting(1);
		}
		else if (SystemMenuMode == EIGSystemMenuMode::AudioCalibration)
		{
			AdjustAudioCalibrationSetting(1);
		}
		else if (SystemMenuMode == EIGSystemMenuMode::KeyBindings)
		{
			// 좌우는 값이 아니라 칸을 고른다. 키보드와 패드를 같은 화면에
			// 나란히 두고, 어느 쪽을 바꾸는지 손이 먼저 알게 한다.
			MoveKeyBindingColumn(1);
		}
		return;
	}
	ChangeAccessibilitySetting(1, false);
}

void AIGPlayerController::ConfirmAccessibilitySelection()
{
	if (!bAccessibilityMenuVisible)
	{
		ConfirmSystemMenuSelection();
		return;
	}
	ChangeAccessibilitySetting(1, true);
}

void AIGPlayerController::RequestManualHint()
{
	if (bAccessibilityMenuVisible
		|| SystemMenuMode != EIGSystemMenuMode::Hidden
		|| !GetWorld())
	{
		return;
	}
	if (UGameInstance* GameInstance = GetGameInstance())
	{
		if (const UIGMissingFloorNarrativeSubsystem* MissingFloor =
			GameInstance->GetSubsystem<UIGMissingFloorNarrativeSubsystem>();
			MissingFloor && MissingFloor->GetNightIndex() > 0)
		{
			// 없는 층 never reveals a puzzle answer from the default path.
			// During the hour even this social nudge disappears with the HUD.
			if (!MissingFloor->IsHourSealed())
			{
				AIGHorrorHUD::PushThought(
					this,
					NSLOCTEXT(
						"IGMissingFloor",
						"DayHintAskHwang",
						"401호에 물어볼 수 있다."),
					2.4f);
			}
			return;
		}
	}
	for (TActorIterator<AIGThirdMorningDirector> It(GetWorld()); It; ++It)
	{
		if (It->RequestManualHint())
		{
			return;
		}
	}
	for (TActorIterator<AIGSecondMorningDirector> It(GetWorld()); It; ++It)
	{
		if (It->RequestManualHint())
		{
			return;
		}
	}
}

void AIGPlayerController::ChangeAccessibilitySetting(
	const int32 Direction,
	const bool bConfirm)
{
	if (!bAccessibilityMenuVisible)
	{
		return;
	}
	UIGAccessibilitySubsystem* Accessibility = GetAccessibilitySubsystem();
	if (!Accessibility)
	{
		return;
	}

	if (AccessibilitySelection == IGSettingsMenuLayout::ResetDefaults)
	{
		if (bConfirm)
		{
			Accessibility->ResetToDefaults();
			if (AIGPlayerCharacter* PlayerCharacter =
				Cast<AIGPlayerCharacter>(GetPawn()))
			{
				PlayerCharacter->RefreshMicrophoneCaptureMode();
			}
		}
		RefreshMenuHud();
		return;
	}
	if (AccessibilitySelection == IGSettingsMenuLayout::CloseMenu)
	{
		if (bConfirm)
		{
			CloseAccessibilityMenu();
		}
		return;
	}

	FIGAccessibilitySettings Settings = Accessibility->GetSettings();
	switch (AccessibilitySelection)
	{
	case IGSettingsMenuLayout::HintMode:
	{
		constexpr int32 HintModeCount = 3;
		const int32 Current = static_cast<int32>(Settings.HintMode);
		Settings.HintMode = static_cast<EIGHintMode>(
			(Current + (Direction < 0 ? HintModeCount - 1 : 1))
			% HintModeCount);
		break;
	}
	case IGSettingsMenuLayout::ReducedCameraMotion:
		Settings.bReducedCameraMotion = !Settings.bReducedCameraMotion;
		break;
	case IGSettingsMenuLayout::ReducedFlicker:
		Settings.bReducedFlicker = !Settings.bReducedFlicker;
		break;
	case IGSettingsMenuLayout::FieldOfView:
		Settings.FieldOfViewDegrees = FMath::Clamp(
			Settings.FieldOfViewDegrees + (Direction < 0 ? -2.0f : 2.0f),
			68.0f,
			100.0f);
		break;
	case IGSettingsMenuLayout::ComfortVignette:
		Settings.ComfortVignetteStrength = FMath::Clamp(
			Settings.ComfortVignetteStrength + (Direction < 0 ? -0.25f : 0.25f),
			0.0f,
			1.0f);
		break;
	case IGSettingsMenuLayout::DirectionalFearCues:
		Settings.bDirectionalFearCues = !Settings.bDirectionalFearCues;
		break;
	case IGSettingsMenuLayout::KnockRippleSubstitute:
		Settings.bKnockRippleSubstitute = !Settings.bKnockRippleSubstitute;
		break;
	case IGSettingsMenuLayout::KnockHapticSubstitute:
		Settings.bKnockHapticSubstitute = !Settings.bKnockHapticSubstitute;
		break;
	case IGSettingsMenuLayout::HeartbeatWarning:
		Settings.bHeartbeatWarning = !Settings.bHeartbeatWarning;
		break;
	case IGSettingsMenuLayout::CognitiveAssist:
		Settings.bCognitiveAssist = !Settings.bCognitiveAssist;
		break;
	case IGSettingsMenuLayout::AutoConnectEvidence:
		Settings.bAutoConnectEvidence = !Settings.bAutoConnectEvidence;
		break;
	case IGSettingsMenuLayout::Subtitles:
		Settings.bSubtitlesEnabled = !Settings.bSubtitlesEnabled;
		break;
	case IGSettingsMenuLayout::SoundCaptions:
		Settings.bSoundCaptionsEnabled = !Settings.bSoundCaptionsEnabled;
		break;
	case IGSettingsMenuLayout::CaptionSize:
		Settings.CaptionSizeScale = FMath::Clamp(
			Settings.CaptionSizeScale + (Direction < 0 ? -0.10f : 0.10f),
			0.85f,
			2.0f);
		break;
	case IGSettingsMenuLayout::CaptionBackground:
		Settings.CaptionBackgroundOpacity = FMath::Clamp(
			Settings.CaptionBackgroundOpacity
				+ (Direction < 0 ? -0.10f : 0.10f),
			0.0f,
			1.0f);
		break;
	case IGSettingsMenuLayout::CaptionSafeArea:
		Settings.CaptionSafeAreaScale = FMath::Clamp(
			Settings.CaptionSafeAreaScale + (Direction < 0 ? -0.05f : 0.05f),
			0.80f,
			1.0f);
		break;
	case IGSettingsMenuLayout::CaptionDuration:
		Settings.CaptionDurationScale = FMath::Clamp(
			Settings.CaptionDurationScale + (Direction < 0 ? -0.25f : 0.25f),
			0.75f,
			2.0f);
		break;
	case IGSettingsMenuLayout::ToggleCrouch:
		Settings.bToggleCrouch = !Settings.bToggleCrouch;
		break;
	case IGSettingsMenuLayout::ToggleHold:
		Settings.bToggleHoldInteractions = !Settings.bToggleHoldInteractions;
		break;
	case IGSettingsMenuLayout::HoldDuration:
		Settings.HoldDurationScale = FMath::Clamp(
			Settings.HoldDurationScale + (Direction < 0 ? -0.25f : 0.25f),
			0.25f,
			1.0f);
		break;
	case IGSettingsMenuLayout::Haptics:
		Settings.bHapticsEnabled = !Settings.bHapticsEnabled;
		break;
	case IGSettingsMenuLayout::MicrophoneNoise:
		Settings.bMicrophoneNoiseEnabled = !Settings.bMicrophoneNoiseEnabled;
		break;
	default:
		return;
	}
	Accessibility->ApplySettings(Settings);
	if (AIGPlayerCharacter* PlayerCharacter =
		Cast<AIGPlayerCharacter>(GetPawn()))
	{
		PlayerCharacter->RefreshMicrophoneCaptureMode();
		PlayerCharacter->RefreshFieldOfView();
	}
	RefreshMenuHud();
}

void AIGPlayerController::MoveSystemMenuSelection(const int32 Direction)
{
	if (SystemMenuMode == EIGSystemMenuMode::DisplaySettings)
	{
		MoveDisplaySettingsSelection(Direction);
		return;
	}
	if (SystemMenuMode == EIGSystemMenuMode::AudioCalibration)
	{
		MoveAudioCalibrationSelection(Direction);
		return;
	}
	if (SystemMenuMode == EIGSystemMenuMode::KeyBindings)
	{
		MoveKeyBindingSelection(Direction);
		return;
	}
	if (SystemMenuMode == EIGSystemMenuMode::Hidden
		|| SystemMenuMode == EIGSystemMenuMode::Credits
		|| Direction == 0)
	{
		return;
	}
	if (bNightFivePlaying)
	{
		// 검정 화면 뒤의 메뉴를 더듬게 두지 않는다.
		return;
	}
	bNewGameConfirmationArmed = false;
	SystemMenuStatusText = FText::GetEmpty();
	bSystemMenuStatusIsError = false;

	// 화면 순서로 움직인다. 액션 인덱스 순서는 디스패치의 것이고, 위아래
	// 키는 눈의 것이다 — 밤 5는 액션 목록의 마지막이지만 화면에서는 이어하기
	// 바로 밑에 있으므로, 액션 순서로 돌면 선택이 화면을 건너뛴다.
	const bool bTitleMenu = SystemMenuMode == EIGSystemMenuMode::Title;
	const int32 VisibleCount = IGFrontendMenuLayout::GetVisibleActionCount(
		bTitleMenu,
		bCompatibleAutosaveAvailable,
		bNightFiveAvailable);
	if (VisibleCount <= 0)
	{
		return;
	}
	int32 VisibleSlot = IGFrontendMenuLayout::GetVisibleSlotForAction(
		SystemMenuSelection,
		bTitleMenu,
		bCompatibleAutosaveAvailable,
		bNightFiveAvailable);
	if (VisibleSlot == INDEX_NONE)
	{
		VisibleSlot = 0;
	}
	for (int32 Attempt = 0; Attempt < VisibleCount; ++Attempt)
	{
		VisibleSlot =
			(VisibleSlot + (Direction < 0 ? VisibleCount - 1 : 1)) % VisibleCount;
		const int32 Candidate = IGFrontendMenuLayout::GetActionForVisibleSlot(
			VisibleSlot,
			bTitleMenu,
			bCompatibleAutosaveAvailable,
			bNightFiveAvailable);
		if (Candidate != INDEX_NONE && IsSystemMenuRowEnabled(Candidate))
		{
			SystemMenuSelection = Candidate;
			break;
		}
	}
	RefreshMenuHud();
}

void AIGPlayerController::ConfirmSystemMenuSelection()
{
	if (bNightFivePlaying)
	{
		// 30초를 끝까지 들을 의무는 없다. 어떤 확인 입력이든 타이틀로 돌린다.
		EndNightFive();
		return;
	}
	if (SystemMenuMode == EIGSystemMenuMode::Hidden)
	{
		return;
	}
	if (SystemMenuMode == EIGSystemMenuMode::Credits)
	{
		ReturnFromCredits();
		return;
	}
	if (SystemMenuMode == EIGSystemMenuMode::ContentNotice)
	{
		DismissContentNotice();
		return;
	}
	if (SystemMenuMode == EIGSystemMenuMode::KeyBindings)
	{
		ConfirmKeyBindingSelection();
		return;
	}
	if (SystemMenuMode == EIGSystemMenuMode::DisplaySettings)
	{
		ConfirmDisplaySettingsSelection();
		return;
	}
	if (SystemMenuMode == EIGSystemMenuMode::AudioCalibration)
	{
		ConfirmAudioCalibrationSelection();
		return;
	}
	if (!IsSystemMenuRowEnabled(SystemMenuSelection))
	{
		return;
	}

	if (SystemMenuSelection == 0)
	{
		if (SystemMenuMode == EIGSystemMenuMode::Title)
		{
			ContinueLatestAutosave();
		}
		else
		{
			SetSystemMenuMode(EIGSystemMenuMode::Hidden);
		}
		return;
	}
	if (SystemMenuSelection == 1)
	{
		if (SystemMenuMode == EIGSystemMenuMode::Title)
		{
			if (bCompatibleAutosaveAvailable && !bNewGameConfirmationArmed)
			{
				bNewGameConfirmationArmed = true;
				RefreshMenuHud();
				return;
			}
			StartNewGame();
		}
		else
		{
			ContinueLatestAutosave();
		}
		return;
	}
	if (SystemMenuSelection == IGFrontendMenuLayout::NightFiveAction)
	{
		PlayNightFive();
		return;
	}
	if (SystemMenuSelection == 2)
	{
		OpenDisplaySettings();
		return;
	}
	if (SystemMenuSelection == 3)
	{
		CreditsReturnMode = SystemMenuMode;
		SetSystemMenuMode(EIGSystemMenuMode::Credits);
		return;
	}
	QuitToDesktop();
}

void AIGPlayerController::SetSystemMenuMode(const EIGSystemMenuMode NewMode)
{
	SystemMenuMode = NewMode;
	if (UWorld* World = GetWorld())
	{
		if (UIGMissingFloorAudioSubsystem* AudioDirector =
			World->GetSubsystem<UIGMissingFloorAudioSubsystem>())
		{
			const bool bKeepTitleSoundscape =
				NewMode == EIGSystemMenuMode::Title
				|| (NewMode == EIGSystemMenuMode::DisplaySettings
					&& DisplaySettingsReturnMode == EIGSystemMenuMode::Title)
				|| (NewMode == EIGSystemMenuMode::Credits
					&& CreditsReturnMode == EIGSystemMenuMode::Title);
			AudioDirector->SetTitleMode(bKeepTitleSoundscape);
		}
	}
	if (NewMode != EIGSystemMenuMode::Title)
	{
		bNewGameConfirmationArmed = false;
		bHeadphoneRecommendationVisible = false;
	}
	if (NewMode != EIGSystemMenuMode::DisplaySettings)
	{
		bDisplaySettingsApplied = false;
	}
	if (NewMode == EIGSystemMenuMode::Title
		|| NewMode == EIGSystemMenuMode::Pause)
	{
		bCompatibleAutosaveAvailable = HasCompatibleAutosave();
	bNightFiveAvailable = HasEndingBAutosave();
		const int32 LoadRow = NewMode == EIGSystemMenuMode::Title ? 0 : 1;
		if (!bCompatibleAutosaveAvailable && SystemMenuSelection == LoadRow)
		{
			SystemMenuSelection = (LoadRow + 1) % IGSystemMenu::RowCount;
		}
	}
	SetPause(NewMode != EIGSystemMenuMode::Hidden);
	ApplyMenuInputMode();
	RefreshMenuHud();
}

void AIGPlayerController::StartHeadphoneRecommendationIfNeeded()
{
	if (!IsLocalController()
		|| SystemMenuMode != EIGSystemMenuMode::Title
		|| FParse::Param(FCommandLine::Get(), TEXT("IGFrontendShippingProbe"))
		|| FParse::Param(FCommandLine::Get(), TEXT("IGMissingFloorJournalPreview"))
		|| FParse::Param(FCommandLine::Get(), TEXT("IGAudioCalibrationPreview"))
		|| FParse::Param(FCommandLine::Get(), TEXT("IGDisplaySettingsPreview"))
		// 밤 5 검증은 타이틀 목록 그 자체를 검사한다. 첫 실행 온보딩이 메뉴를
		// 가져가면 어떤 행도 선택 가능하지 않다.
		|| FParse::Param(FCommandLine::Get(), TEXT("IGNightFiveProbe")))
	{
		return;
	}

	constexpr const TCHAR* Section = TEXT("IndieGame.AudioOnboarding");
	bool bAlreadyShown = false;
	if (GConfig)
	{
		GConfig->GetBool(
			Section,
			TEXT("HeadphoneRecommendationShown"),
			bAlreadyShown,
			GGameUserSettingsIni);
	}
	if (bAlreadyShown)
	{
		if (!bAudioCalibrationCompleted)
		{
			OpenAudioCalibration(true);
		}
		return;
	}

	bHeadphoneRecommendationVisible = true;
	HeadphoneRecommendationDeadline = FPlatformTime::Seconds() + 2.5;
	SetActorTickEnabled(true);
	RefreshMenuHud();
}

void AIGPlayerController::DismissHeadphoneRecommendation()
{
	if (!bHeadphoneRecommendationVisible)
	{
		return;
	}
	bHeadphoneRecommendationVisible = false;
	if (GConfig)
	{
		GConfig->SetBool(
			IGAudioCalibration::ConfigSection,
			TEXT("HeadphoneRecommendationShown"),
			true,
			GGameUserSettingsIni);
		GConfig->Flush(false, GGameUserSettingsIni);
	}
	if (!bAudioCalibrationCompleted)
	{
		OpenAudioCalibration(true);
	}
	else
	{
		RefreshMenuHud();
	}
}

void AIGPlayerController::LoadAudioCalibrationSettings()
{
	if (GConfig)
	{
		GConfig->GetBool(
			IGAudioCalibration::ConfigSection,
			TEXT("CalibrationCompleted"),
			bAudioCalibrationCompleted,
			GGameUserSettingsIni);
		GConfig->GetInt(
			IGAudioCalibration::ConfigSection,
			TEXT("VolumeStep"),
			AudioCalibrationVolumeStep,
			GGameUserSettingsIni);
		GConfig->GetInt(
			IGAudioCalibration::ConfigSection,
			TEXT("BrightnessStep"),
			AudioCalibrationBrightnessStep,
			GGameUserSettingsIni);
		GConfig->GetBool(
			IGAudioCalibration::ConfigSection,
			TEXT("HeadphoneOutput"),
			bHeadphoneOutput,
			GGameUserSettingsIni);
		GConfig->GetInt(
			IGAudioCalibration::ConfigSection,
			TEXT("MusicStep"),
			AudioCalibrationMusicStep,
			GGameUserSettingsIni);
		GConfig->GetInt(
			IGAudioCalibration::ConfigSection,
			TEXT("AmbienceStep"),
			AudioCalibrationAmbienceStep,
			GGameUserSettingsIni);
	}
	AudioCalibrationMusicStep = FMath::Clamp(
		AudioCalibrationMusicStep, 0, IGAudioCalibration::MusicStepCount - 1);
	AudioCalibrationAmbienceStep = FMath::Clamp(
		AudioCalibrationAmbienceStep,
		0,
		IGAudioCalibration::AmbienceStepCount - 1);
	AudioCalibrationVolumeStep = FMath::Clamp(
		AudioCalibrationVolumeStep,
		0,
		IGAudioCalibration::VolumeStepCount - 1);
	AudioCalibrationBrightnessStep = FMath::Clamp(
		AudioCalibrationBrightnessStep,
		0,
		IGAudioCalibration::BrightnessStepCount - 1);
	ApplyAudioCalibrationValues();
}

void AIGPlayerController::OpenAudioCalibration(const bool bFirstRun)
{
	if (!bAudioCalibrationSessionActive)
	{
		AudioCalibrationReturnMode =
			SystemMenuMode == EIGSystemMenuMode::Pause
				? EIGSystemMenuMode::Pause
				: SystemMenuMode == EIGSystemMenuMode::DisplaySettings
					? EIGSystemMenuMode::DisplaySettings
					: EIGSystemMenuMode::Title;
		PreviousAudioCalibrationVolumeStep = AudioCalibrationVolumeStep;
		PreviousAudioCalibrationBrightnessStep = AudioCalibrationBrightnessStep;
		bPreviousHeadphoneOutput = bHeadphoneOutput;
		PreviousAudioCalibrationMusicStep = AudioCalibrationMusicStep;
		PreviousAudioCalibrationAmbienceStep = AudioCalibrationAmbienceStep;
		bAudioCalibrationSessionActive = true;
	}
	bAudioCalibrationFirstRun = bFirstRun;
	AudioCalibrationSelection = 0;
	SystemMenuStatusText = FText::GetEmpty();
	bSystemMenuStatusIsError = false;
	SetSystemMenuMode(EIGSystemMenuMode::AudioCalibration);
	NextAudioCalibrationKnockTime = FPlatformTime::Seconds() + 0.35;
	SetActorTickEnabled(true);
}

void AIGPlayerController::CancelAudioCalibration()
{
	if (!bAudioCalibrationSessionActive)
	{
		SetSystemMenuMode(AudioCalibrationReturnMode);
		return;
	}
	AudioCalibrationVolumeStep = PreviousAudioCalibrationVolumeStep;
	AudioCalibrationBrightnessStep = PreviousAudioCalibrationBrightnessStep;
	bHeadphoneOutput = bPreviousHeadphoneOutput;
	AudioCalibrationMusicStep = PreviousAudioCalibrationMusicStep;
	AudioCalibrationAmbienceStep = PreviousAudioCalibrationAmbienceStep;
	ApplyAudioCalibrationValues();
	bAudioCalibrationSessionActive = false;
	bAudioCalibrationFirstRun = false;
	NextAudioCalibrationKnockTime = -1.0;
	SetSystemMenuMode(AudioCalibrationReturnMode);
}

void AIGPlayerController::CompleteAudioCalibration()
{
	bAudioCalibrationCompleted = true;
	if (GConfig)
	{
		GConfig->SetBool(
			IGAudioCalibration::ConfigSection,
			TEXT("CalibrationCompleted"),
			true,
			GGameUserSettingsIni);
		GConfig->SetInt(
			IGAudioCalibration::ConfigSection,
			TEXT("VolumeStep"),
			AudioCalibrationVolumeStep,
			GGameUserSettingsIni);
		GConfig->SetInt(
			IGAudioCalibration::ConfigSection,
			TEXT("BrightnessStep"),
			AudioCalibrationBrightnessStep,
			GGameUserSettingsIni);
		GConfig->SetBool(
			IGAudioCalibration::ConfigSection,
			TEXT("HeadphoneOutput"),
			bHeadphoneOutput,
			GGameUserSettingsIni);
		GConfig->SetInt(
			IGAudioCalibration::ConfigSection,
			TEXT("MusicStep"),
			AudioCalibrationMusicStep,
			GGameUserSettingsIni);
		GConfig->SetInt(
			IGAudioCalibration::ConfigSection,
			TEXT("AmbienceStep"),
			AudioCalibrationAmbienceStep,
			GGameUserSettingsIni);
		GConfig->Flush(false, GGameUserSettingsIni);
	}
	bAudioCalibrationSessionActive = false;
	bAudioCalibrationFirstRun = false;
	NextAudioCalibrationKnockTime = -1.0;
	SetSystemMenuMode(AudioCalibrationReturnMode);
}

void AIGPlayerController::MoveAudioCalibrationSelection(const int32 Direction)
{
	if (SystemMenuMode != EIGSystemMenuMode::AudioCalibration || Direction == 0)
	{
		return;
	}
	AudioCalibrationSelection =
		(AudioCalibrationSelection
			+ (Direction < 0 ? IGAudioCalibration::RowCount - 1 : 1))
		% IGAudioCalibration::RowCount;
	RefreshMenuHud();
}

void AIGPlayerController::AdjustAudioCalibrationSetting(const int32 Direction)
{
	if (SystemMenuMode != EIGSystemMenuMode::AudioCalibration || Direction == 0)
	{
		return;
	}
	const int32 Step = Direction < 0 ? -1 : 1;
	if (AudioCalibrationSelection == IGAudioCalibration::Master)
	{
		AudioCalibrationVolumeStep = FMath::Clamp(
			AudioCalibrationVolumeStep + Step,
			0,
			IGAudioCalibration::VolumeStepCount - 1);
		ApplyAudioCalibrationValues();
		NextAudioCalibrationKnockTime = FPlatformTime::Seconds() + 0.12;
		SetActorTickEnabled(true);
	}
	else if (AudioCalibrationSelection == IGAudioCalibration::Music)
	{
		AudioCalibrationMusicStep = FMath::Clamp(
			AudioCalibrationMusicStep + Step,
			0,
			IGAudioCalibration::MusicStepCount - 1);
		ApplyAudioCalibrationValues();
	}
	else if (AudioCalibrationSelection == IGAudioCalibration::Ambience)
	{
		AudioCalibrationAmbienceStep = FMath::Clamp(
			AudioCalibrationAmbienceStep + Step,
			0,
			IGAudioCalibration::AmbienceStepCount - 1);
		ApplyAudioCalibrationValues();
	}
	else if (AudioCalibrationSelection == IGAudioCalibration::Brightness)
	{
		AudioCalibrationBrightnessStep = FMath::Clamp(
			AudioCalibrationBrightnessStep + Step,
			0,
			IGAudioCalibration::BrightnessStepCount - 1);
		ApplyAudioCalibrationValues();
	}
	else if (AudioCalibrationSelection == IGAudioCalibration::Output)
	{
		// 둘 중 하나라 좌우 어느 쪽이든 뒤집힌다.
		bHeadphoneOutput = !bHeadphoneOutput;
		ApplyAudioCalibrationValues();
		// 바꾼 직후에 노크를 한 번 들려준다. 귀로 확인할 수 없는 음향
		// 설정은 화면에 글자만 바뀌는 것과 같다.
		NextAudioCalibrationKnockTime = FPlatformTime::Seconds() + 0.18;
		SetActorTickEnabled(true);
	}
	RefreshMenuHud();
}

void AIGPlayerController::ConfirmAudioCalibrationSelection()
{
	if (AudioCalibrationSelection < IGAudioCalibration::TestKnock)
	{
		AdjustAudioCalibrationSetting(1);
		return;
	}
	if (AudioCalibrationSelection == IGAudioCalibration::TestKnock)
	{
		PlayAudioCalibrationKnock();
		return;
	}
	CompleteAudioCalibration();
}

void AIGPlayerController::ApplyAudioCalibrationValues()
{
	IGAudio::SetOutputMode(
		bHeadphoneOutput
			? IGAudio::EIGOutputMode::Headphones
			: IGAudio::EIGOutputMode::Speakers);
	if (UWorld* World = GetWorld())
	{
		if (UIGMissingFloorAudioSubsystem* AudioDirector =
			World->GetSubsystem<UIGMissingFloorAudioSubsystem>())
		{
			AudioDirector->SetUserMasterVolume(
				IGAudioCalibration::VolumeValues[AudioCalibrationVolumeStep]);
			AudioDirector->SetHeadphoneOutput(bHeadphoneOutput);
			AudioDirector->SetScoreUserVolume(
				IGAudioCalibration::MusicValues[AudioCalibrationMusicStep]);
			AudioDirector->SetAmbienceUserVolume(
				IGAudioCalibration::AmbienceValues[
					AudioCalibrationAmbienceStep]);
		}
	}
	ConsoleCommand(
		FString::Printf(
			TEXT("gamma %.2f"),
			IGAudioCalibration::GammaValues[AudioCalibrationBrightnessStep]),
		true);
}

void AIGPlayerController::PlayAudioCalibrationKnock()
{
	NextAudioCalibrationKnockTime = -1.0;
	if (UWorld* World = GetWorld())
	{
		if (UIGMissingFloorAudioSubsystem* AudioDirector =
			World->GetSubsystem<UIGMissingFloorAudioSubsystem>())
		{
			AudioDirector->PlayCalibrationKnock();
		}
	}
}

void AIGPlayerController::OpenDisplaySettings()
{
	if (SystemMenuMode != EIGSystemMenuMode::Title
		&& SystemMenuMode != EIGSystemMenuMode::Pause
		&& SystemMenuMode != EIGSystemMenuMode::AudioCalibration)
	{
		return;
	}
	DisplaySettingsReturnMode = SystemMenuMode;
	DisplaySettingsSelection = 0;
	SystemMenuStatusText = FText::GetEmpty();
	bSystemMenuStatusIsError = false;
	RefreshStagedDisplaySettings();
	SetSystemMenuMode(EIGSystemMenuMode::DisplaySettings);
}

void AIGPlayerController::ReturnFromDisplaySettings()
{
	const EIGSystemMenuMode ReturnMode =
		DisplaySettingsReturnMode == EIGSystemMenuMode::Pause
			? EIGSystemMenuMode::Pause
			: DisplaySettingsReturnMode == EIGSystemMenuMode::AudioCalibration
				? EIGSystemMenuMode::AudioCalibration
				: EIGSystemMenuMode::Title;
	DisplaySettingsReturnMode = EIGSystemMenuMode::Title;
	SetSystemMenuMode(ReturnMode);
}

void AIGPlayerController::RefreshStagedDisplaySettings()
{
	UGameUserSettings* Settings = GEngine
		? GEngine->GetGameUserSettings()
		: nullptr;
	if (!Settings)
	{
		return;
	}

	switch (Settings->GetFullscreenMode())
	{
	case EWindowMode::WindowedFullscreen:
		DisplayWindowModeIndex = 1;
		break;
	case EWindowMode::Windowed:
		DisplayWindowModeIndex = 2;
		break;
	case EWindowMode::Fullscreen:
	default:
		DisplayWindowModeIndex = 0;
		break;
	}

	const FIntPoint CurrentResolution = Settings->GetScreenResolution();
	int64 NearestDistance = TNumericLimits<int64>::Max();
	for (int32 Index = 0; Index < IGDisplaySettings::ResolutionCount; ++Index)
	{
		const int64 DeltaX = static_cast<int64>(CurrentResolution.X)
			- IGDisplaySettings::Resolutions[Index].X;
		const int64 DeltaY = static_cast<int64>(CurrentResolution.Y)
			- IGDisplaySettings::Resolutions[Index].Y;
		const int64 Distance = DeltaX * DeltaX + DeltaY * DeltaY;
		if (Distance < NearestDistance)
		{
			NearestDistance = Distance;
			DisplayResolutionIndex = Index;
		}
	}

	DisplayQualityIndex = Settings->GetOverallScalabilityLevel() <= 1 ? 0 : 1;
	bDisplayVSync = Settings->IsVSyncEnabled();
	const float CurrentFrameLimit = Settings->GetFrameRateLimit();
	DisplayFrameLimitIndex = CurrentFrameLimit <= 0.0f
		? 2
		: CurrentFrameLimit <= 45.0f
			? 0
			: 1;
	bDisplaySettingsApplied = false;
}

void AIGPlayerController::MoveDisplaySettingsSelection(const int32 Direction)
{
	if (SystemMenuMode != EIGSystemMenuMode::DisplaySettings || Direction == 0)
	{
		return;
	}
	if (bDisplaySettingsAwaitingConfirmation)
	{
		DisplaySettingsSelection =
			DisplaySettingsSelection == IGSettingsMenuLayout::ApplyOrKeep
				? IGSettingsMenuLayout::BackOrRevert
				: IGSettingsMenuLayout::ApplyOrKeep;
		RefreshMenuHud();
		return;
	}
	DisplaySettingsSelection =
		(DisplaySettingsSelection
			+ (Direction < 0 ? IGDisplaySettings::RowCount - 1 : 1))
		% IGDisplaySettings::RowCount;
	RefreshMenuHud();
}

void AIGPlayerController::AdjustDisplaySetting(const int32 Direction)
{
	if (SystemMenuMode != EIGSystemMenuMode::DisplaySettings
		|| bDisplaySettingsAwaitingConfirmation
		|| Direction == 0)
	{
		return;
	}
	const auto Wrap = [Direction](const int32 Value, const int32 Count)
	{
		return (Value + (Direction < 0 ? Count - 1 : 1)) % Count;
	};
	switch (DisplaySettingsSelection)
	{
	case 0:
		DisplayWindowModeIndex = Wrap(
			DisplayWindowModeIndex,
			IGDisplaySettings::WindowModeCount);
		break;
	case 1:
		DisplayResolutionIndex = Wrap(
			DisplayResolutionIndex,
			IGDisplaySettings::ResolutionCount);
		break;
	case 2:
		DisplayQualityIndex = Wrap(
			DisplayQualityIndex,
			IGDisplaySettings::QualityCount);
		break;
	case 3:
		bDisplayVSync = !bDisplayVSync;
		break;
	case 4:
		DisplayFrameLimitIndex = Wrap(
			DisplayFrameLimitIndex,
			IGDisplaySettings::FrameLimitCount);
		break;
	default:
		return;
	}
	// 고른 즉시 적용한다. 값만 바꿔 두고 「변경 적용」 줄을 따로 찾아 눌러야
	// 반영되던 방식은, 눌러야 하는 줄이 화면 밖에 있으면 아무 일도 일어나지
	// 않는 것처럼 보인다. 화면을 못 보게 만들 수 있는 것(화면 모드·해상도)만
	// 적용 뒤 10초 확인을 띄우고, 나머지는 바로 저장한다.
	ApplyDisplaySettings();
	SystemMenuStatusText = FText::GetEmpty();
	bSystemMenuStatusIsError = false;
	RefreshMenuHud();
}

void AIGPlayerController::ConfirmDisplaySettingsSelection()
{
	if (SystemMenuMode != EIGSystemMenuMode::DisplaySettings)
	{
		return;
	}
	if (bDisplaySettingsAwaitingConfirmation)
	{
		if (DisplaySettingsSelection == IGSettingsMenuLayout::ApplyOrKeep)
		{
			ConfirmPendingDisplaySettings();
		}
		else
		{
			RevertPendingDisplaySettings();
		}
		return;
	}
	if (DisplaySettingsSelection <= 4)
	{
		AdjustDisplaySetting(1);
		return;
	}
	if (DisplaySettingsSelection == IGSettingsMenuLayout::AccessibilityPanel)
	{
		ToggleAccessibilityMenu();
		return;
	}
	if (DisplaySettingsSelection == IGSettingsMenuLayout::AudioCalibrationPanel)
	{
		OpenAudioCalibration(false);
		return;
	}
	if (DisplaySettingsSelection == IGSettingsMenuLayout::KeyBindingsPanel)
	{
		OpenKeyBindings();
		return;
	}
	if (DisplaySettingsSelection == IGSettingsMenuLayout::ApplyOrKeep)
	{
		ApplyDisplaySettings();
		return;
	}
	ReturnFromDisplaySettings();
}

void AIGPlayerController::ApplyDisplaySettings()
{
	UGameUserSettings* Settings = GEngine
		? GEngine->GetGameUserSettings()
		: nullptr;
	if (!Settings)
	{
		SystemMenuStatusText = NSLOCTEXT(
			"IGFrontend",
			"DisplaySettingsUnavailable",
			"화면 설정을 적용할 수 없습니다.");
		bSystemMenuStatusIsError = true;
		bDisplaySettingsApplied = false;
		RefreshMenuHud();
		return;
	}
	PreviousDisplayQualityLevel = Settings->GetOverallScalabilityLevel();
	if (PreviousDisplayQualityLevel < 0)
	{
		PreviousDisplayQualityLevel = 2;
	}
	bPreviousDisplayVSync = Settings->IsVSyncEnabled();
	PreviousDisplayFrameLimit = Settings->GetFrameRateLimit();

	const EWindowMode::Type WindowMode = DisplayWindowModeIndex == 1
		? EWindowMode::WindowedFullscreen
		: DisplayWindowModeIndex == 2
			? EWindowMode::Windowed
			: EWindowMode::Fullscreen;
	// 되돌릴 수 있어야 하는 것은 화면을 못 보게 만들 수 있는 둘뿐이다.
	// 품질·수직 동기화·프레임 제한은 잘못 골라도 화면이 살아 있으므로
	// 확인을 물을 이유가 없다.
	const bool bDisplayModeChanged =
		Settings->GetFullscreenMode() != WindowMode
		|| Settings->GetScreenResolution()
			!= IGDisplaySettings::Resolutions[DisplayResolutionIndex];

	Settings->SetFullscreenMode(WindowMode);
	Settings->SetScreenResolution(
		IGDisplaySettings::Resolutions[DisplayResolutionIndex]);
	// UE 5.8의 중간 품질은 Lumen Lite를 쓴다. 성능 모드에서도 간접광을 남긴다.
	Settings->SetOverallScalabilityLevel(DisplayQualityIndex == 0 ? 1 : 2);
	Settings->SetVSyncEnabled(bDisplayVSync);
	Settings->SetFrameRateLimit(
		IGDisplaySettings::FrameLimits[DisplayFrameLimitIndex]);
	Settings->ApplyResolutionSettings(false);
	Settings->ApplyNonResolutionSettings();

	if (!bDisplayModeChanged)
	{
		// 여기서 바로 디스크에 쓴다. 사람이 저장을 찾아 누르지 않아도
		// 다음 실행에 남아 있어야 한다.
		Settings->SaveSettings();
		bDisplaySettingsApplied = true;
		bDisplaySettingsAwaitingConfirmation = false;
		DisplayConfirmationSecondsRemaining = 0;
		SystemMenuStatusText = FText::GetEmpty();
		bSystemMenuStatusIsError = false;
		RefreshMenuHud();
		return;
	}

	bDisplaySettingsApplied = false;
	bDisplaySettingsAwaitingConfirmation = true;
	DisplaySettingsSelection = IGSettingsMenuLayout::ApplyOrKeep;
	DisplayConfirmationSecondsRemaining = 10;
	DisplayConfirmationDeadline = FPlatformTime::Seconds() + 10.0;
	SetActorTickEnabled(true);
	SystemMenuStatusText = FText::GetEmpty();
	bSystemMenuStatusIsError = false;
	RefreshMenuHud();
}

void AIGPlayerController::ConfirmPendingDisplaySettings()
{
	if (!bDisplaySettingsAwaitingConfirmation)
	{
		return;
	}
	if (UGameUserSettings* Settings = GEngine
		? GEngine->GetGameUserSettings()
		: nullptr)
	{
		Settings->ConfirmVideoMode();
		Settings->SaveSettings();
	}
	bDisplaySettingsAwaitingConfirmation = false;
	bDisplaySettingsApplied = true;
	DisplayConfirmationSecondsRemaining = 0;
	RefreshStagedDisplaySettings();
	bDisplaySettingsApplied = true;
	RefreshMenuHud();
}

void AIGPlayerController::RevertPendingDisplaySettings()
{
	if (!bDisplaySettingsAwaitingConfirmation)
	{
		return;
	}
	if (UGameUserSettings* Settings = GEngine
		? GEngine->GetGameUserSettings()
		: nullptr)
	{
		Settings->RevertVideoMode();
		Settings->SetOverallScalabilityLevel(PreviousDisplayQualityLevel);
		Settings->SetVSyncEnabled(bPreviousDisplayVSync);
		Settings->SetFrameRateLimit(PreviousDisplayFrameLimit);
		Settings->ApplyResolutionSettings(false);
		Settings->ApplyNonResolutionSettings();
		Settings->SaveSettings();
	}
	bDisplaySettingsAwaitingConfirmation = false;
	bDisplaySettingsApplied = false;
	DisplayConfirmationSecondsRemaining = 0;
	RefreshStagedDisplaySettings();
	SystemMenuStatusText = NSLOCTEXT(
		"IGFrontend",
		"DisplaySettingsReverted",
		"이전 화면 설정으로 되돌렸습니다.");
	bSystemMenuStatusIsError = false;
	RefreshMenuHud();
}

void AIGPlayerController::ReturnFromCredits()
{
	const EIGSystemMenuMode ReturnMode =
		CreditsReturnMode == EIGSystemMenuMode::Pause
			? EIGSystemMenuMode::Pause
			: EIGSystemMenuMode::Title;
	SetSystemMenuMode(ReturnMode);
}

void AIGPlayerController::StartNewGame()
{
	UIGSaveSubsystem* SaveSubsystem = GetSaveSubsystem();
	if (!GetWorld() || (SaveSubsystem && SaveSubsystem->IsBusy()))
	{
		return;
	}
	if (SaveSubsystem && !SaveSubsystem->ClearRotatingAutosaves())
	{
		SystemMenuStatusText = NSLOCTEXT(
			"IGFrontend",
			"NewGameDeleteFailed",
			"자동 저장을 지우지 못했습니다. 저장 공간과 폴더 권한을 확인하세요.");
		bSystemMenuStatusIsError = true;
		bNewGameConfirmationArmed = false;
		RefreshMenuHud();
		return;
	}

	if (UGameInstance* GameInstance = GetGameInstance())
	{
		if (UIGStoryStateSubsystem* StoryState =
			GameInstance->GetSubsystem<UIGStoryStateSubsystem>())
		{
			StoryState->ClearStates(false);
		}
		if (UIGRebirthNarrativeSubsystem* RebirthState =
			GameInstance->GetSubsystem<UIGRebirthNarrativeSubsystem>())
		{
			RebirthState->ResetNarrative();
		}
		// Game-instance subsystems outlive a new game inside one process, so
		// without this the previous run's truths and aggression tier leak in.
		if (UIGMissingFloorNarrativeSubsystem* MissingFloorState =
			GameInstance->GetSubsystem<UIGMissingFloorNarrativeSubsystem>())
		{
			MissingFloorState->ResetNarrative();
		}
	}
	bNewGameConfirmationArmed = false;
	SystemMenuStatusText = FText::GetEmpty();
	bSystemMenuStatusIsError = false;

	const FName LevelName(*UGameplayStatics::GetCurrentLevelName(this, true));
	UGameplayStatics::OpenLevel(
		this,
		LevelName,
		true,
		TEXT("IGMissingFloor=1?IGIgnoreDirectStart=1?IGNewGame=1"));
}

void AIGPlayerController::ContinueLatestAutosave()
{
	UIGSaveSubsystem* SaveSubsystem = GetSaveSubsystem();
	if (!SaveSubsystem || SaveSubsystem->IsBusy())
	{
		SystemMenuStatusText = NSLOCTEXT(
			"IGFrontend",
			"SaveOperationBusy",
			"저장 작업이 끝난 뒤 다시 시도하세요.");
		bSystemMenuStatusIsError = true;
		RefreshMenuHud();
		return;
	}
	if (SaveSubsystem->RequestLoadLatestAutosave())
	{
		SystemMenuStatusText = NSLOCTEXT(
			"IGFrontend",
			"LoadingAutosave",
			"최근 자동 저장을 불러오는 중입니다.");
		bSystemMenuStatusIsError = false;
		RefreshMenuHud();
		return;
	}
	bCompatibleAutosaveAvailable = HasCompatibleAutosave();
	bNightFiveAvailable = HasEndingBAutosave();
	SystemMenuStatusText = NSLOCTEXT(
		"IGFrontend",
		"CompatibleAutosaveMissing",
		"불러올 수 있는 자동 저장이 없습니다.");
	bSystemMenuStatusIsError = true;
	RefreshMenuHud();
}

void AIGPlayerController::QuitToDesktop()
{
	UKismetSystemLibrary::QuitGame(
		this,
		this,
		EQuitPreference::Quit,
		false);
}

void AIGPlayerController::BindSaveNotifications()
{
	if (UIGSaveSubsystem* SaveSubsystem = GetSaveSubsystem())
	{
		SaveSubsystem->OnSaveCompleted.AddUniqueDynamic(
			this,
			&ThisClass::HandleSaveCompleted);
		SaveSubsystem->OnLoadCompleted.AddUniqueDynamic(
			this,
			&ThisClass::HandleLoadCompleted);
	}
}

void AIGPlayerController::UnbindSaveNotifications()
{
	if (UIGSaveSubsystem* SaveSubsystem = GetSaveSubsystem())
	{
		SaveSubsystem->OnSaveCompleted.RemoveDynamic(
			this,
			&ThisClass::HandleSaveCompleted);
		SaveSubsystem->OnLoadCompleted.RemoveDynamic(
			this,
			&ThisClass::HandleLoadCompleted);
	}
}

void AIGPlayerController::HandleSaveCompleted(
	const bool bSuccess,
	FString SlotName)
{
	static_cast<void>(SlotName);
	if (bSuccess)
	{
		// §19.7. 수동 슬롯이 없는 게임이라 저장됐다는 사실을 어디선가는
		// 말해야 한다. 점 하나 0.8초, 그 이상은 §23이 금지한 상시 표시다.
		if (AIGHorrorHUD* HorrorHUD = Cast<AIGHorrorHUD>(GetHUD()))
		{
			HorrorHUD->ShowSaveIndicator();
		}
		return;
	}
	const FText FailureText = NSLOCTEXT(
		"IGFrontend",
		"SaveFailed",
		"저장에 실패했습니다. 저장 공간과 폴더 권한을 확인하세요.");
	if (SystemMenuMode == EIGSystemMenuMode::Hidden
		&& !bAccessibilityMenuVisible)
	{
		AIGHorrorHUD::PushThought(this, FailureText, 4.0f);
		return;
	}
	SystemMenuStatusText = FailureText;
	bSystemMenuStatusIsError = true;
	RefreshMenuHud();
}

void AIGPlayerController::HandleLoadCompleted(
	const bool bSuccess,
	FString SlotName,
	UIGSaveGame* SaveGame)
{
	static_cast<void>(SlotName);
	static_cast<void>(SaveGame);
	if (bSuccess)
	{
		return;
	}
	bCompatibleAutosaveAvailable = HasCompatibleAutosave();
	bNightFiveAvailable = HasEndingBAutosave();
	SystemMenuStatusText = NSLOCTEXT(
		"IGFrontend",
		"LoadFailed",
		"자동 저장을 불러오지 못했습니다. 파일이 손상됐거나 호환되지 않습니다.");
	bSystemMenuStatusIsError = true;
	if (SystemMenuMode == EIGSystemMenuMode::Hidden
		&& !bAccessibilityMenuVisible)
	{
		AIGHorrorHUD::PushThought(this, SystemMenuStatusText, 4.0f);
	}
	RefreshMenuHud();
}

void AIGPlayerController::RefreshMenuHud() const
{
	if (AIGHorrorHUD* HorrorHUD = Cast<AIGHorrorHUD>(GetHUD()))
	{
		HorrorHUD->SetAccessibilityMenuState(
			bAccessibilityMenuVisible,
			AccessibilitySelection);
		FIGSystemMenuPresentation Presentation;
		Presentation.bVisible =
			SystemMenuMode != EIGSystemMenuMode::Hidden;
		Presentation.bTitle = SystemMenuMode == EIGSystemMenuMode::Title;
		Presentation.bUseTitleBackdrop =
			SystemMenuMode == EIGSystemMenuMode::Title
			|| (SystemMenuMode == EIGSystemMenuMode::Credits
				&& CreditsReturnMode == EIGSystemMenuMode::Title);
		Presentation.bCredits = SystemMenuMode == EIGSystemMenuMode::Credits;
		Presentation.bContentNotice =
			SystemMenuMode == EIGSystemMenuMode::ContentNotice;
		Presentation.bKeyBindings =
			SystemMenuMode == EIGSystemMenuMode::KeyBindings;
		Presentation.KeyBindingSelection = KeyBindingSelection;
		Presentation.bKeyBindingCapturing = bKeyBindingCapturing;
		Presentation.bKeyBindingColumnGamepad = bKeyBindingColumnGamepad;
		Presentation.KeyBindingStatus = KeyBindingStatusText;
		Presentation.bKeyBindingStatusIsError = bKeyBindingStatusIsError;
		Presentation.bAudioCalibration =
			SystemMenuMode == EIGSystemMenuMode::AudioCalibration;
		Presentation.bDisplaySettings =
			SystemMenuMode == EIGSystemMenuMode::DisplaySettings;
		Presentation.bCanContinue = bCompatibleAutosaveAvailable;
		Presentation.bNightFiveAvailable = bNightFiveAvailable;
		Presentation.bNightFiveSpent = bNightFiveSpent;
		Presentation.bNightFivePlaying = bNightFivePlaying;
		Presentation.bConfirmNewGame = bNewGameConfirmationArmed;
		Presentation.bHeadphoneRecommendation =
			bHeadphoneRecommendationVisible;
		Presentation.bVSync = bDisplayVSync;
		Presentation.bDisplaySettingsApplied = bDisplaySettingsApplied;
		Presentation.bDisplaySettingsAwaitingConfirmation =
			bDisplaySettingsAwaitingConfirmation;
		Presentation.bStatusIsError = bSystemMenuStatusIsError;
		Presentation.SelectedRow = SystemMenuSelection;
		Presentation.DisplaySelectedRow = DisplaySettingsSelection;
		Presentation.AudioCalibrationSelectedRow = AudioCalibrationSelection;
		Presentation.AudioCalibrationVolumeStep = AudioCalibrationVolumeStep;
		Presentation.AudioCalibrationBrightnessStep =
			AudioCalibrationBrightnessStep;
		Presentation.bAudioCalibrationFirstRun = bAudioCalibrationFirstRun;
		Presentation.bHeadphoneOutput = bHeadphoneOutput;
		Presentation.AudioCalibrationMusicStep = AudioCalibrationMusicStep;
		Presentation.AudioCalibrationAmbienceStep = AudioCalibrationAmbienceStep;
		Presentation.WindowModeIndex = DisplayWindowModeIndex;
		Presentation.ResolutionIndex = DisplayResolutionIndex;
		Presentation.QualityIndex = DisplayQualityIndex;
		Presentation.FrameLimitIndex = DisplayFrameLimitIndex;
		Presentation.ConfirmationSecondsRemaining =
			DisplayConfirmationSecondsRemaining;
		Presentation.StatusText = SystemMenuStatusText;
		HorrorHUD->SetSystemMenuState(Presentation);
		HorrorHUD->SetMissingFloorJournalState(
			bMissingFloorJournalVisible,
			MissingFloorJournalPage);
		HorrorHUD->SetInputDevicePresentation(bUsingGamepadForHud);
	}
}

void AIGPlayerController::SetInputDevicePresentation(const bool bUsingGamepad)
{
	bUsingGamepadForHud = bUsingGamepad;
	ApplyMenuInputMode();
	RefreshMenuHud();
}

void AIGPlayerController::ApplyMenuInputMode()
{
	const bool bPointerMenuVisible = bAccessibilityMenuVisible
		|| SystemMenuMode != EIGSystemMenuMode::Hidden;
	const bool bInputLayerVisible = bPointerMenuVisible
		|| bMissingFloorJournalVisible;
	bShowMouseCursor = bPointerMenuVisible && !bUsingGamepadForHud;
	if (bInputLayerVisible)
	{
		FInputModeGameAndUI InputMode;
		InputMode.SetHideCursorDuringCapture(false);
		SetInputMode(InputMode);
	}
	else
	{
		FInputModeGameOnly InputMode;
		SetInputMode(InputMode);
	}
}

bool AIGPlayerController::TryGetMenuRowFromPointer(
	const int32 RowCount,
	const float MinimumStartY,
	const float StartYFraction,
	const float MinimumSpacing,
	const float MaximumSpacing,
	const float SpacingFraction,
	int32& OutRow) const
{
	OutRow = INDEX_NONE;
	int32 ViewportWidth = 0;
	int32 ViewportHeight = 0;
	GetViewportSize(ViewportWidth, ViewportHeight);
	float PointerX = 0.0f;
	float PointerY = 0.0f;
	if (ViewportWidth <= 0
		|| ViewportHeight <= 0
		|| !GetMousePosition(PointerX, PointerY)
		|| PointerX < ViewportWidth * 0.16f
		|| PointerX > ViewportWidth * 0.84f)
	{
		return false;
	}

	const float RowStart = FMath::Max(
		MinimumStartY,
		ViewportHeight * StartYFraction);
	const float RowSpacing = FMath::Clamp(
		ViewportHeight * SpacingFraction,
		MinimumSpacing,
		MaximumSpacing);
	const int32 Candidate = FMath::RoundToInt(
		(PointerY - RowStart) / RowSpacing);
	if (Candidate < 0
		|| Candidate >= RowCount
		|| FMath::Abs(PointerY - (RowStart + Candidate * RowSpacing))
			> RowSpacing * 0.46f)
	{
		return false;
	}
	OutRow = Candidate;
	return true;
}

bool AIGPlayerController::TryGetSystemMenuRowFromPointer(int32& OutRow) const
{
	OutRow = INDEX_NONE;
	int32 ViewportWidth = 0;
	int32 ViewportHeight = 0;
	GetViewportSize(ViewportWidth, ViewportHeight);
	float PointerX = 0.0f;
	float PointerY = 0.0f;
	if (ViewportWidth <= 0
		|| ViewportHeight <= 0
		|| !GetMousePosition(PointerX, PointerY))
	{
		return false;
	}

	OutRow = IGFrontendMenuLayout::HitTestAction(
		IGFrontendMenuLayout::MakeMetrics(ViewportWidth, ViewportHeight),
		FVector2D(PointerX, PointerY),
		SystemMenuMode == EIGSystemMenuMode::Title,
		bCompatibleAutosaveAvailable,
		bNightFiveAvailable);
	return OutRow != INDEX_NONE;
}

bool AIGPlayerController::TryGetDisplaySettingsRowFromPointer(
	int32& OutRow,
	bool& bOutCategoryHit) const
{
	OutRow = INDEX_NONE;
	bOutCategoryHit = false;
	int32 ViewportWidth = 0;
	int32 ViewportHeight = 0;
	GetViewportSize(ViewportWidth, ViewportHeight);
	float PointerX = 0.0f;
	float PointerY = 0.0f;
	if (ViewportWidth <= 0
		|| ViewportHeight <= 0
		|| !GetMousePosition(PointerX, PointerY))
	{
		return false;
	}
	return IGSettingsMenuLayout::HitTestSettingsRow(
		IGSettingsMenuLayout::MakePanelMetrics(ViewportWidth, ViewportHeight),
		FVector2D(PointerX, PointerY),
		DisplaySettingsSelection,
		IGSettingsMenuLayout::DisplayCategoryCount,
		IGSettingsMenuLayout::GetDisplayCategory,
		OutRow,
		bOutCategoryHit);
}

bool AIGPlayerController::TryGetAccessibilityRowFromPointer(
	int32& OutRow,
	bool& bOutCategoryHit) const
{
	OutRow = INDEX_NONE;
	bOutCategoryHit = false;
	int32 ViewportWidth = 0;
	int32 ViewportHeight = 0;
	GetViewportSize(ViewportWidth, ViewportHeight);
	float PointerX = 0.0f;
	float PointerY = 0.0f;
	if (ViewportWidth <= 0
		|| ViewportHeight <= 0
		|| !GetMousePosition(PointerX, PointerY))
	{
		return false;
	}
	return IGSettingsMenuLayout::HitTestSettingsRow(
		IGSettingsMenuLayout::MakePanelMetrics(ViewportWidth, ViewportHeight),
		FVector2D(PointerX, PointerY),
		AccessibilitySelection,
		IGSettingsMenuLayout::AccessibilityCategoryCount,
		IGSettingsMenuLayout::GetAccessibilityCategory,
		OutRow,
		bOutCategoryHit);
}

void AIGPlayerController::UpdateMenuPointerHover()
{
	int32 Row = INDEX_NONE;
	bool bCategoryHit = false;
	int32 ViewportWidth = 0;
	int32 ViewportHeight = 0;
	GetViewportSize(ViewportWidth, ViewportHeight);
	if (bAccessibilityMenuVisible)
	{
		if (TryGetAccessibilityRowFromPointer(Row, bCategoryHit)
			&& !bCategoryHit
			&& AccessibilitySelection != Row)
		{
			AccessibilitySelection = Row;
			RefreshMenuHud();
		}
		return;
	}
	if (SystemMenuMode == EIGSystemMenuMode::DisplaySettings)
	{
		if (TryGetDisplaySettingsRowFromPointer(Row, bCategoryHit)
			&& !bCategoryHit
			&& (!bDisplaySettingsAwaitingConfirmation || Row >= 7)
			&& DisplaySettingsSelection != Row)
		{
			DisplaySettingsSelection = Row;
			RefreshMenuHud();
		}
		return;
	}
	if (SystemMenuMode == EIGSystemMenuMode::AudioCalibration)
	{
		const float CalibrationScale = FMath::Clamp(
			FMath::Min(
				ViewportHeight / 1080.0f,
				ViewportWidth / 1920.0f),
			0.67f,
			2.0f);
		const float PanelHeight = FMath::Min(
			ViewportHeight * 0.70f,
			690.0f * CalibrationScale);
		const float PanelTop =
			(ViewportHeight - PanelHeight) * 0.5f + 12.0f * CalibrationScale;
		const float RowStart =
			PanelTop + PanelHeight - 224.0f * CalibrationScale;
		if (TryGetMenuRowFromPointer(
				IGAudioCalibration::RowCount,
				RowStart,
				0.0f,
				42.0f * CalibrationScale,
				42.0f * CalibrationScale,
				0.0f,
				Row)
			&& AudioCalibrationSelection != Row)
		{
			AudioCalibrationSelection = Row;
			RefreshMenuHud();
		}
		return;
	}
	if (SystemMenuMode == EIGSystemMenuMode::Title
		|| SystemMenuMode == EIGSystemMenuMode::Pause)
	{
		if (TryGetSystemMenuRowFromPointer(Row)
			&& IsSystemMenuRowEnabled(Row)
			&& SystemMenuSelection != Row)
		{
			SystemMenuSelection = Row;
			bNewGameConfirmationArmed = false;
			SystemMenuStatusText = FText::GetEmpty();
			bSystemMenuStatusIsError = false;
			RefreshMenuHud();
		}
	}
}

bool AIGPlayerController::HandleMenuPointerClick()
{
	if (!bAccessibilityMenuVisible
		&& SystemMenuMode == EIGSystemMenuMode::Hidden)
	{
		return false;
	}
	if (SystemMenuMode == EIGSystemMenuMode::Credits
		&& !bAccessibilityMenuVisible)
	{
		ReturnFromCredits();
		return true;
	}

	int32 Row = INDEX_NONE;
	bool bCategoryHit = false;
	int32 ViewportWidth = 0;
	int32 ViewportHeight = 0;
	GetViewportSize(ViewportWidth, ViewportHeight);
	if (bAccessibilityMenuVisible)
	{
		if (TryGetAccessibilityRowFromPointer(Row, bCategoryHit))
		{
			AccessibilitySelection = Row;
			if (!bCategoryHit)
			{
				ChangeAccessibilitySetting(1, true);
			}
			else
			{
				RefreshMenuHud();
			}
		}
		return true;
	}
	if (SystemMenuMode == EIGSystemMenuMode::DisplaySettings)
	{
		if (TryGetDisplaySettingsRowFromPointer(Row, bCategoryHit)
			&& (!bDisplaySettingsAwaitingConfirmation || Row >= 7))
		{
			DisplaySettingsSelection = Row;
			if (!bCategoryHit)
			{
				ConfirmDisplaySettingsSelection();
			}
			else
			{
				RefreshMenuHud();
			}
		}
		return true;
	}
	if (SystemMenuMode == EIGSystemMenuMode::AudioCalibration)
	{
		const float CalibrationScale = FMath::Clamp(
			FMath::Min(
				ViewportHeight / 1080.0f,
				ViewportWidth / 1920.0f),
			0.67f,
			2.0f);
		const float PanelHeight = FMath::Min(
			ViewportHeight * 0.70f,
			690.0f * CalibrationScale);
		const float PanelTop =
			(ViewportHeight - PanelHeight) * 0.5f + 12.0f * CalibrationScale;
		const float RowStart =
			PanelTop + PanelHeight - 224.0f * CalibrationScale;
		if (TryGetMenuRowFromPointer(
			IGAudioCalibration::RowCount,
			RowStart,
			0.0f,
			42.0f * CalibrationScale,
			42.0f * CalibrationScale,
			0.0f,
			Row))
		{
			AudioCalibrationSelection = Row;
			ConfirmAudioCalibrationSelection();
		}
		return true;
	}
	if (TryGetSystemMenuRowFromPointer(Row)
		&& IsSystemMenuRowEnabled(Row))
	{
		if (SystemMenuSelection != Row)
		{
			bNewGameConfirmationArmed = false;
		}
		SystemMenuSelection = Row;
		ConfirmSystemMenuSelection();
	}
	return true;
}

namespace IGContentNotice
{
	const TCHAR* ConfigSection = TEXT("IndieGame.Onboarding");
	const TCHAR* ShownKey = TEXT("ContentNoticeShown");
}

void AIGPlayerController::OpenKeyBindings()
{
	KeyBindingsReturnMode = SystemMenuMode;
	KeyBindingSelection = 0;
	bKeyBindingCapturing = false;
	bKeyBindingColumnGamepad = bUsingGamepadForHud;
	KeyBindingStatusText = FText::GetEmpty();
	bKeyBindingStatusIsError = false;
	SetSystemMenuMode(EIGSystemMenuMode::KeyBindings);
}

void AIGPlayerController::CloseKeyBindings()
{
	bKeyBindingCapturing = false;
	KeyBindingStatusText = FText::GetEmpty();
	SetSystemMenuMode(
		KeyBindingsReturnMode == EIGSystemMenuMode::KeyBindings
			? EIGSystemMenuMode::Title
			: KeyBindingsReturnMode);
}

void AIGPlayerController::MoveKeyBindingSelection(const int32 Direction)
{
	if (SystemMenuMode != EIGSystemMenuMode::KeyBindings || bKeyBindingCapturing)
	{
		return;
	}
	// 시점 셋이 위에, 동사 목록이 가운데, 「전부 기본값으로」가 마지막이다.
	const int32 RowCount = UIGInputBindingSubsystem::LookRowCount
		+ UIGInputBindingSubsystem::GetActionCount() + 1;
	KeyBindingSelection =
		(KeyBindingSelection + Direction + RowCount) % RowCount;
	KeyBindingStatusText = FText::GetEmpty();
	bKeyBindingStatusIsError = false;
	RefreshMenuHud();
}

void AIGPlayerController::MoveKeyBindingColumn(const int32 Direction)
{
	if (SystemMenuMode != EIGSystemMenuMode::KeyBindings
		|| bKeyBindingCapturing
		|| Direction == 0)
	{
		return;
	}
	// 시점 행에는 고를 칸이 없다. 좌우가 곧 값이다.
	if (KeyBindingSelection < UIGInputBindingSubsystem::LookRowCount)
	{
		UIGInputBindingSubsystem* Bindings = GetGameInstance()
			? GetGameInstance()->GetSubsystem<UIGInputBindingSubsystem>()
			: nullptr;
		if (!Bindings)
		{
			return;
		}
		switch (KeyBindingSelection)
		{
		case 0:
			Bindings->AdjustMouseSensitivity(Direction);
			break;
		case 1:
			Bindings->AdjustGamepadSensitivity(Direction);
			break;
		case 2:
			Bindings->AdjustVerticalLookScale(Direction);
			break;
		default:
			Bindings->ToggleInvertLookY();
			break;
		}
		KeyBindingStatusText = FText::GetEmpty();
		bKeyBindingStatusIsError = false;
		RefreshMenuHud();
		return;
	}
	bKeyBindingColumnGamepad = Direction > 0;
	KeyBindingStatusText = FText::GetEmpty();
	bKeyBindingStatusIsError = false;
	RefreshMenuHud();
}

void AIGPlayerController::ConfirmKeyBindingSelection()
{
	UIGInputBindingSubsystem* Bindings = GetGameInstance()
		? GetGameInstance()->GetSubsystem<UIGInputBindingSubsystem>()
		: nullptr;
	if (!Bindings)
	{
		return;
	}
	const int32 ActionIndex =
		KeyBindingSelection - UIGInputBindingSubsystem::LookRowCount;
	if (ActionIndex < 0)
	{
		// 감도는 좌우로 맞춘다. 반전은 켜고 끄는 것뿐이라 확인으로도 뒤집는다.
		if (KeyBindingSelection == 3)
		{
			Bindings->ToggleInvertLookY();
			KeyBindingStatusText = FText::GetEmpty();
		}
		else
		{
			KeyBindingStatusText = NSLOCTEXT(
				"IGHUD",
				"KeyBindingsUseArrows",
				"이 행은 좌우로 맞춥니다.");
		}
		bKeyBindingStatusIsError = false;
		RefreshMenuHud();
		return;
	}
	if (ActionIndex >= UIGInputBindingSubsystem::GetActionCount())
	{
		Bindings->ResetToDefaults();
		KeyBindingStatusText = NSLOCTEXT(
			"IGHUD", "KeyBindingsReset", "전부 기본값으로 되돌렸습니다.");
		bKeyBindingStatusIsError = false;
		RefreshMenuHud();
		return;
	}
	bKeyBindingCapturing = true;
	KeyBindingStatusText = bKeyBindingColumnGamepad
		? NSLOCTEXT(
			"IGHUD", "KeyBindingsAwaitPad", "쓸 버튼을 누르세요. B로 취소.")
		: NSLOCTEXT(
			"IGHUD", "KeyBindingsAwaitKey", "쓸 키를 누르세요. Esc로 취소.");
	bKeyBindingStatusIsError = false;
	RefreshMenuHud();
}

bool AIGPlayerController::CaptureKeyBindingInput(const FInputKeyEventArgs& Params)
{
	if (!bKeyBindingCapturing
		|| SystemMenuMode != EIGSystemMenuMode::KeyBindings
		|| Params.IsSimulatedInput()
		|| Params.Event != IE_Pressed)
	{
		return false;
	}
	// 취소는 캡처보다 먼저 본다. 취소 키를 새 바인딩으로 삼으면 그 화면에서
	// 나갈 수 없다.
	if (Params.Key == EKeys::Escape || Params.Key == EKeys::Gamepad_FaceButton_Right)
	{
		bKeyBindingCapturing = false;
		KeyBindingStatusText = FText::GetEmpty();
		bKeyBindingStatusIsError = false;
		RefreshMenuHud();
		return true;
	}

	UIGInputBindingSubsystem* Bindings = GetGameInstance()
		? GetGameInstance()->GetSubsystem<UIGInputBindingSubsystem>()
		: nullptr;
	if (!Bindings)
	{
		bKeyBindingCapturing = false;
		return true;
	}
	FText Failure;
	if (Bindings->TryRebind(
		KeyBindingSelection - UIGInputBindingSubsystem::LookRowCount,
		bKeyBindingColumnGamepad,
		Params.Key,
		Failure))
	{
		bKeyBindingCapturing = false;
		KeyBindingStatusText = FText::Format(
			NSLOCTEXT("IGHUD", "KeyBindingsBound", "「{0}」으로 바꿨습니다."),
			Params.Key.GetDisplayName());
		bKeyBindingStatusIsError = false;
	}
	else
	{
		// 거절해도 대기 상태로 남는다. 다시 누르면 되는 것이지, 처음부터
		// 다시 들어와야 하는 것이 아니다.
		KeyBindingStatusText = Failure;
		bKeyBindingStatusIsError = true;
	}
	RefreshMenuHud();
	return true;
}

void AIGPlayerController::ShowContentNoticeIfNeeded()
{
	if (!IsLocalController()
		|| SystemMenuMode != EIGSystemMenuMode::Title
		|| FParse::Param(FCommandLine::Get(), TEXT("IGFrontendShippingProbe"))
		|| FParse::Param(FCommandLine::Get(), TEXT("IGMissingFloorJournalPreview"))
		|| FParse::Param(FCommandLine::Get(), TEXT("IGAudioCalibrationPreview"))
		|| FParse::Param(FCommandLine::Get(), TEXT("IGDisplaySettingsPreview"))
		|| FParse::Param(FCommandLine::Get(), TEXT("IGNightFiveProbe")))
	{
		return;
	}
	if (FParse::Param(FCommandLine::Get(), TEXT("IGContentNoticePreview")))
	{
		// 캡처용 강제 표시. 저장값을 읽지도 쓰지도 않는다.
		SetSystemMenuMode(EIGSystemMenuMode::ContentNotice);
		return;
	}

	bool bAlreadyShown = false;
	if (GConfig)
	{
		GConfig->GetBool(
			IGContentNotice::ConfigSection,
			IGContentNotice::ShownKey,
			bAlreadyShown,
			GGameUserSettingsIni);
	}
	if (bAlreadyShown)
	{
		return;
	}
	SetSystemMenuMode(EIGSystemMenuMode::ContentNotice);
}

void AIGPlayerController::DismissContentNotice()
{
	if (SystemMenuMode != EIGSystemMenuMode::ContentNotice)
	{
		return;
	}
	if (GConfig
		&& !FParse::Param(FCommandLine::Get(), TEXT("IGContentNoticePreview")))
	{
		GConfig->SetBool(
			IGContentNotice::ConfigSection,
			IGContentNotice::ShownKey,
			true,
			GGameUserSettingsIni);
		GConfig->Flush(false, GGameUserSettingsIni);
	}
	SystemMenuSelection = 0;
	SetSystemMenuMode(EIGSystemMenuMode::Title);
	// 고지를 닫고 나서야 소리 맞추기로 넘어간다.
	StartHeadphoneRecommendationIfNeeded();
}

void AIGPlayerController::ShowTitleAfterEnding()
{
	if (!IsLocalController())
	{
		return;
	}
	SystemMenuSelection = 0;
	SetSystemMenuMode(EIGSystemMenuMode::Title);
}

bool AIGPlayerController::ShouldShowTitleMenu() const
{
	// 밤 5 검증은 타이틀 그 자체를 검사하므로 무인 실행에서도 타이틀이 필요하다.
	// 프런트엔드 출하 프로브가 이미 같은 예외를 쓰고 있다.
	if (FParse::Param(FCommandLine::Get(), TEXT("IGNightFiveProbe")))
	{
		return true;
	}
	if (FApp::IsUnattended() || IsRunningCommandlet())
	{
		return false;
	}
	const UWorld* World = GetWorld();
	if (World
		&& (World->URL.HasOption(TEXT("IGResumeSave"))
			|| World->URL.HasOption(TEXT("IGNewGame"))
			|| World->URL.HasOption(TEXT("IGChapterTwo"))
			|| World->URL.HasOption(TEXT("IGChapterThree"))))
	{
		return false;
	}

	const TCHAR* CommandLine = FCommandLine::Get();
	if (FParse::Param(CommandLine, TEXT("IGSkipFrontend"))
		|| FParse::Param(CommandLine, TEXT("IGChapterTwo"))
		|| FParse::Param(CommandLine, TEXT("IGChapterThree"))
		|| FParse::Param(CommandLine, TEXT("IGFrontendShippingProbe"))
		|| FParse::Param(CommandLine, TEXT("IGAudioCalibrationPreview"))
		|| FParse::Param(CommandLine, TEXT("IGDisplaySettingsPreview")))
	{
		return false;
	}
	const FString CommandLineText(CommandLine);
	return !CommandLineText.Contains(TEXT("-IGRebirth"), ESearchCase::IgnoreCase)
		&& !CommandLineText.Contains(TEXT("-IGCapture"), ESearchCase::IgnoreCase)
		&& !CommandLineText.Contains(TEXT("-IGDemo"), ESearchCase::IgnoreCase);
}

bool AIGPlayerController::HasCompatibleAutosave() const
{
	const UIGSaveSubsystem* SaveSubsystem = GetSaveSubsystem();
	return SaveSubsystem && SaveSubsystem->HasCompatibleAutosave();
}

bool AIGPlayerController::IsSystemMenuRowEnabled(const int32 Row) const
{
	if (Row < 0 || Row >= IGSystemMenu::RowCount
		|| SystemMenuMode == EIGSystemMenuMode::Credits
		|| SystemMenuMode == EIGSystemMenuMode::AudioCalibration
		|| SystemMenuMode == EIGSystemMenuMode::DisplaySettings
		|| SystemMenuMode == EIGSystemMenuMode::Hidden)
	{
		return false;
	}
	if (Row == IGFrontendMenuLayout::NightFiveAction)
	{
		// 흐려진 뒤에도 고를 수 있다. 없는 것은 엔딩 B가 없을 때뿐이다.
		return !IGFrontendMenuLayout::HidesNightFive(
			SystemMenuMode == EIGSystemMenuMode::Title,
			bNightFiveAvailable);
	}
	const int32 LoadRow =
		SystemMenuMode == EIGSystemMenuMode::Title ? 0 : 1;
	return Row != LoadRow || bCompatibleAutosaveAvailable;
}

void AIGPlayerController::RequestNightFiveProbeExit(const bool bFailed)
{
	if (NightFiveProbeTicker.IsValid())
	{
		FTSTicker::GetCoreTicker().RemoveTicker(NightFiveProbeTicker);
		NightFiveProbeTicker.Reset();
	}
	EndNightFive();
	FPlatformMisc::RequestExitWithStatus(true, bFailed ? 2 : 0);
}

void AIGPlayerController::StartNightFiveProbe()
{
	// --- 대응이 전단사인지 -------------------------------------------------
	// 밤 5는 액션 5이면서 화면 자리 1이다. 이 치환이 깨지면 플레이어가 누른
	// 줄과 실행되는 액션이 달라진다 — 조용히, 그리고 정확히 한 행씩.
	for (int32 Combination = 0; Combination < 4; ++Combination)
	{
		const bool bCanContinue = (Combination & 1) != 0;
		const bool bNightFive = (Combination & 2) != 0;
		const int32 VisibleCount = IGFrontendMenuLayout::GetVisibleActionCount(
			true,
			bCanContinue,
			bNightFive);
		const int32 Expected = IGFrontendMenuLayout::ActionCount
			- (bCanContinue ? 0 : 1)
			- (bNightFive ? 0 : 1);
		if (VisibleCount != Expected)
		{
			UE_LOG(
				LogTemp,
				Error,
				TEXT("MISSINGFLOOR_NIGHT5 FAIL: rows=%d expected=%d "
					"continue=%d nightfive=%d"),
				VisibleCount,
				Expected,
				bCanContinue ? 1 : 0,
				bNightFive ? 1 : 0);
			RequestNightFiveProbeExit(true);
			return;
		}
		for (int32 Slot = 0; Slot < VisibleCount; ++Slot)
		{
			const int32 Action = IGFrontendMenuLayout::GetActionForVisibleSlot(
				Slot,
				true,
				bCanContinue,
				bNightFive);
			const int32 RoundTrip = IGFrontendMenuLayout::GetVisibleSlotForAction(
				Action,
				true,
				bCanContinue,
				bNightFive);
			if (Action == INDEX_NONE || RoundTrip != Slot)
			{
				UE_LOG(
					LogTemp,
					Error,
					TEXT("MISSINGFLOOR_NIGHT5 FAIL: slot=%d action=%d back=%d "
						"continue=%d nightfive=%d"),
					Slot,
					Action,
					RoundTrip,
					bCanContinue ? 1 : 0,
					bNightFive ? 1 : 0);
				RequestNightFiveProbeExit(true);
				return;
			}
		}
	}
	// 화면에서 밤 5는 이어하기 바로 밑이다. 그것이 「이어하기 목록에 한 줄」의
	// 구현이므로 자리 자체를 검사한다.
	if (IGFrontendMenuLayout::GetVisibleSlotForAction(
			IGFrontendMenuLayout::NightFiveAction,
			true,
			true,
			true)
		!= 1)
	{
		UE_LOG(
			LogTemp,
			Error,
			TEXT("MISSINGFLOOR_NIGHT5 FAIL: night five is not under continue"));
		RequestNightFiveProbeExit(true);
		return;
	}
	// 일시정지 메뉴에는 절대 없다. 메타 개입은 본편 바깥이다.
	if (!IGFrontendMenuLayout::HidesNightFive(false, true))
	{
		UE_LOG(
			LogTemp,
			Error,
			TEXT("MISSINGFLOOR_NIGHT5 FAIL: night five reachable while paused"));
		RequestNightFiveProbeExit(true);
		return;
	}

	// --- 30초 자체 ---------------------------------------------------------
	// 하네스는 세이브를 만들지 않는다(§14). 조회 결과만 덮어써서 그 행이 있는
	// 세계를 만든다 — 세이브 파일을 위조하는 것과는 다른 일이다.
	bNightFiveAvailable = HasEndingBAutosave();
	SystemMenuSelection = IGFrontendMenuLayout::NightFiveAction;
	NightFiveProbeStep = 0;
	NightFiveProbeSeconds = 0.0f;
	NightFiveProbeTicker = FTSTicker::GetCoreTicker().AddTicker(
		FTickerDelegate::CreateUObject(
			this,
			&AIGPlayerController::AdvanceNightFiveProbe));
}

bool AIGPlayerController::AdvanceNightFiveProbe(const float DeltaSeconds)
{
	NightFiveProbeSeconds += DeltaSeconds;
	switch (NightFiveProbeStep)
	{
	case 0:
		if (!IsSystemMenuRowEnabled(IGFrontendMenuLayout::NightFiveAction))
		{
			UE_LOG(
				LogTemp,
				Error,
				TEXT("MISSINGFLOOR_NIGHT5 FAIL: the row was not selectable"));
			RequestNightFiveProbeExit(true);
			return false;
		}
		ConfirmSystemMenuSelection();
		if (!bNightFivePlaying)
		{
			UE_LOG(
				LogTemp,
				Error,
				TEXT("MISSINGFLOOR_NIGHT5 FAIL: confirming did not start it"));
			RequestNightFiveProbeExit(true);
			return false;
		}
		NightFiveProbeStep = 1;
		NightFiveProbeSeconds = 0.0f;
		return true;

	case 1:
		// 두 소리가 저작된 시각에 나갔는지. 순서가 뒤집히면 대답이 먼저 온다.
		if (NightFiveProbeSeconds >= 14.0f && NightFiveCuesPlayed < 2)
		{
			UE_LOG(
				LogTemp,
				Error,
				TEXT("MISSINGFLOOR_NIGHT5 FAIL: cues=%d after %.1fs"),
				NightFiveCuesPlayed,
				NightFiveProbeSeconds);
			RequestNightFiveProbeExit(true);
			return false;
		}
		if (NightFiveProbeSeconds < NightFiveTotalSeconds - 1.0f)
		{
			if (!bNightFivePlaying)
			{
				UE_LOG(
					LogTemp,
					Error,
					TEXT("MISSINGFLOOR_NIGHT5 FAIL: it ended early at %.1fs"),
					NightFiveProbeSeconds);
				RequestNightFiveProbeExit(true);
				return false;
			}
			// 아직 재생 중이다. 계속 틱해야 한다 — 여기서 false를 돌려주면
			// 프로브가 첫 대기에서 스스로 멈춘다.
			return true;
		}
		NightFiveProbeStep = 2;
		return true;

	default:
		if (bNightFivePlaying)
		{
			if (NightFiveProbeSeconds > NightFiveTotalSeconds + 3.0f)
			{
				UE_LOG(
					LogTemp,
					Error,
					TEXT("MISSINGFLOOR_NIGHT5 FAIL: it never returned to the title"));
				RequestNightFiveProbeExit(true);
				return false;
			}
			// 아직 끝나지 않았다. 마지막 1초를 기다린다.
			return true;
		}
		if (NightFiveCuesPlayed != 2
			|| !bNightFiveSpent
			|| !IsSystemMenuRowEnabled(IGFrontendMenuLayout::NightFiveAction))
		{
			UE_LOG(
				LogTemp,
				Error,
				TEXT("MISSINGFLOOR_NIGHT5 FAIL: cues=%d spent=%d selectable=%d"),
				NightFiveCuesPlayed,
				bNightFiveSpent ? 1 : 0,
				IsSystemMenuRowEnabled(IGFrontendMenuLayout::NightFiveAction)
					? 1
					: 0);
			RequestNightFiveProbeExit(true);
			return false;
		}
		UE_LOG(
			LogTemp,
			Display,
			TEXT("MISSINGFLOOR_NIGHT5 PASS: row under continue in all four "
				"layouts, never while paused, two cues in %.1fs, dimmed but "
				"still selectable, no save file written"),
			NightFiveTotalSeconds);
		RequestNightFiveProbeExit(false);
		return false;
	}
}

FVector AIGPlayerController::NightFiveListenPoint() const
{
	// 타이틀에는 폰이 없다. 카메라 자리에서 재생하면 감쇠는 형식이 되고,
	// 거리는 §10.4의 리버브 센드가 말한다.
	FVector ViewLocation = FVector::ZeroVector;
	FRotator ViewRotation = FRotator::ZeroRotator;
	GetPlayerViewPoint(ViewLocation, ViewRotation);
	return ViewLocation;
}

void AIGPlayerController::PlayNightFive()
{
	UWorld* World = GetWorld();
	if (!World || bNightFivePlaying || !bNightFiveAvailable)
	{
		return;
	}
	// 로딩이 없다. 레벨도, 세이브도, 새 액터도 만들지 않는다 — 타이틀 위에
	// 검정 한 장과 두 개의 소리를 올릴 뿐이다(§14: 실제 세이브 파일은 만들지
	// 않는다).
	bNightFivePlaying = true;
	NightFiveSeconds = 0.0f;
	NightFiveCuesPlayed = 0;
	// 같은 공간 잔향. 엔딩 B는 별관 복도 안이고, 그 대답은 복도 끝에서 왔다.
	if (UIGMissingFloorAudioSubsystem* AudioDirector =
		World->GetSubsystem<UIGMissingFloorAudioSubsystem>())
	{
		AudioDirector->SetAcousticSpace(EIGAcousticSpace::Corridor);
	}
	RefreshMenuHud();
	NightFiveTicker = FTSTicker::GetCoreTicker().AddTicker(
		FTickerDelegate::CreateUObject(
			this,
			&AIGPlayerController::AdvanceNightFive));
}

bool AIGPlayerController::AdvanceNightFive(const float DeltaSeconds)
{
	UWorld* World = GetWorld();
	if (!World || !bNightFivePlaying)
	{
		EndNightFive();
		return false;
	}
	NightFiveSeconds += DeltaSeconds;

	// 그녀 자신의 손. 게임이 기억한 마지막 입력이 그대로 돌아온다.
	if (NightFiveCuesPlayed == 0
		&& NightFiveSeconds >= IGNightFive::SignalAtSeconds)
	{
		NightFiveCuesPlayed = 1;
		IGAudio::SpawnOneShotAt(
			this,
			UIGToneSequenceSoundWave::CreateAnswerKnockPattern(this, 0.0f),
			NightFiveListenPoint(),
			IGNightFive::SignalVolume,
			1.0f,
			IGNightFive::CloseRadius,
			IGNightFive::CloseFalloff,
			EIGAudioBus::Player);
		AIGHorrorHUD::PushAudioCaption(
			this,
			NSLOCTEXT("IGMissingFloor", "NightFiveSignal", "둘 — 쉬고 — 하나"),
			3.0f);
		return true;
	}

	// 복도 끝에서 대답 둘. 새 사건이 아니라 이미 있었던 대답이다.
	if (NightFiveCuesPlayed == 1
		&& NightFiveSeconds >= IGNightFive::AnswerAtSeconds)
	{
		NightFiveCuesPlayed = 2;
		IGAudio::SpawnOneShotAt(
			this,
			UIGToneSequenceSoundWave::CreateWallKnockReply(this),
			NightFiveListenPoint(),
			IGNightFive::AnswerVolume,
			1.0f,
			IGNightFive::FarRadius,
			IGNightFive::FarFalloff,
			EIGAudioBus::Entity);
		AIGHorrorHUD::PushAudioCaption(
			this,
			NSLOCTEXT("IGMissingFloor", "NightFiveAnswer", "복도 끝 — 대답 둘"),
			3.4f);
		return true;
	}

	if (NightFiveSeconds >= NightFiveTotalSeconds)
	{
		EndNightFive();
		return false;
	}
	return true;
}

void AIGPlayerController::EndNightFive()
{
	if (NightFiveTicker.IsValid())
	{
		FTSTicker::GetCoreTicker().RemoveTicker(NightFiveTicker);
		NightFiveTicker.Reset();
	}
	if (!bNightFivePlaying)
	{
		return;
	}
	bNightFivePlaying = false;
	// 한 번 재생하면 흐려진다. 사라지지는 않는다 — 다시 들을 수 있다.
	bNightFiveSpent = true;
	NightFiveSeconds = 0.0f;
	// 타이틀로 복귀한다. 애초에 떠난 적이 없으므로 되돌릴 상태도 없다.
	SystemMenuSelection = IGFrontendMenuLayout::NightFiveAction;
	RefreshMenuHud();
}

bool AIGPlayerController::HasEndingBAutosave() const
{
	if (bNightFiveProbeRequested)
	{
		// 하네스 우회. 세이브를 만들거나 고치지 않고 **조회 결과만** 덮어써서
		// 그 행이 있는 세계를 만든다(§14: 실제 세이브 파일은 만들지 않는다).
		// 이 자리에 두는 이유는 메뉴가 새로 그려질 때마다 조회가 다시 돌기
		// 때문이다 — 플래그를 한 번 세워 두는 방식은 곧 지워진다.
		return true;
	}
	const UIGSaveSubsystem* SaveSubsystem = GetSaveSubsystem();
	return SaveSubsystem && SaveSubsystem->HasEndingBAutosave();
}

UIGAccessibilitySubsystem*
AIGPlayerController::GetAccessibilitySubsystem() const
{
	const UGameInstance* GameInstance = GetGameInstance();
	return GameInstance
		? GameInstance->GetSubsystem<UIGAccessibilitySubsystem>()
		: nullptr;
}

UIGSaveSubsystem* AIGPlayerController::GetSaveSubsystem() const
{
	UGameInstance* GameInstance = GetGameInstance();
	return GameInstance
		? GameInstance->GetSubsystem<UIGSaveSubsystem>()
		: nullptr;
}

void AIGPlayerController::ApplyDefaultInputMapping() const
{
	if (!IsLocalController() || !DefaultMappingContext)
	{
		return;
	}

	if (ULocalPlayer* LocalPlayer = GetLocalPlayer())
	{
		if (UEnhancedInputLocalPlayerSubsystem* InputSubsystem =
			ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(LocalPlayer))
		{
			InputSubsystem->AddMappingContext(DefaultMappingContext, DefaultMappingPriority);
		}
	}
}
