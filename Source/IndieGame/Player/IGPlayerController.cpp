#include "Player/IGPlayerController.h"

#include "Accessibility/IGAccessibilitySubsystem.h"
#include "Audio/IGToneSequenceSoundWave.h"
#include "AssetCompilingManager.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "EngineUtils.h"
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
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Narrative/IGMissingFloorNarrativeSubsystem.h"
#include "Narrative/IGRebirthNarrativeSubsystem.h"
#include "Narrative/IGStoryStateSubsystem.h"
#include "Player/IGHorrorHUD.h"
#include "Save/IGSaveSubsystem.h"
#include "Sequence/IGSecondMorningDirector.h"
#include "Sequence/IGThirdMorningDirector.h"
#include "ShaderCompiler.h"

namespace IGAccessibilityMenu
{
	constexpr int32 RowCount = 14;
}

namespace IGSystemMenu
{
	constexpr int32 RowCount = 5;
}

namespace IGDisplaySettings
{
	constexpr int32 RowCount = 8;
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

AIGPlayerController::AIGPlayerController()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;
	PrimaryActorTick.bTickEvenWhenPaused = true;
}

void AIGPlayerController::BeginPlay()
{
	Super::BeginPlay();

	bShowMouseCursor = false;
	FInputModeGameOnly InputMode;
	SetInputMode(InputMode);
	ApplyDefaultInputMapping();
	BindSaveNotifications();

	if (IsLocalController() && ShouldShowTitleMenu())
	{
		SystemMenuSelection = 0;
		SetSystemMenuMode(EIGSystemMenuMode::Title);
	}
	else
	{
		RefreshMenuHud();
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
	if (!bDisplaySettingsAwaitingConfirmation && !bJournalInputHeld)
	{
		SetActorTickEnabled(false);
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
	const bool bGamepad = Params.IsGamepad();
	const float AxisThreshold = bGamepad ? 0.30f : 0.01f;
	const bool bMeaningfulInput = !Params.IsSimulatedInput()
		&& (Params.Event == IE_Pressed
			|| Params.Event == IE_Repeat
			|| (Params.Event == IE_Axis
				&& (FMath::Abs(Params.AmountDepressed) > AxisThreshold
					|| Params.AmountDepressed2D.SizeSquared()
						> FMath::Square(AxisThreshold))));
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
	FrontendProbeAwaitFrameSerial = 0;
	FrontendProbeDefaultScreenshotPath.Reset();
	FrontendProbeScreenshotPath.Reset();
	FrontendProbeBoundsMin = FVector2D(
		TNumericLimits<float>::Max(),
		TNumericLimits<float>::Max());
	FrontendProbeBoundsMax = FVector2D(
		TNumericLimits<float>::Lowest(),
		TNumericLimits<float>::Lowest());

	bAccessibilityMenuVisible = false;
	AccessibilitySelection = 0;
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
		if (!TryCaptureFrontendProbeLayout(TEXT("accessibility_keyboard"), 14))
		{
			return;
		}
		DispatchFrontendProbeKey(EKeys::Gamepad_DPad_Down);
		WaitForInputProcessing(3);
		return;
	case 3:
		if (!bAccessibilityMenuVisible
			|| AccessibilitySelection != 1
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
			|| AccessibilitySelection != 0
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
		FrontendProbeStep = 18;
		AwaitFrontendProbeFrame();
		return;
	case 18:
		if (!TryCaptureFrontendProbeLayout(TEXT("display_gamepad"), 11))
		{
			return;
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
		HorrorHUD->GetLayoutValidationSample(
			CanvasSize,
			BoundsMin,
			BoundsMax,
			ElementCount,
			bAllInsideCanvas,
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
	uint64 FrameSerial = 0;
	if (!HorrorHUD->GetLayoutValidationSample(
		CanvasSize,
		BoundsMin,
		BoundsMax,
		ElementCount,
		bAllInsideCanvas,
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
	if (FrontendProbeLayoutSampleCount != 10
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
		uint64 FrameSerial = 0;
		const bool bLayoutReady = HorrorHUD
			&& HorrorHUD->IsMissingFloorJournalVisible()
			&& HorrorHUD->GetLayoutValidationSample(
				CanvasSize,
				BoundsMinimum,
				BoundsMaximum,
				ElementCount,
				bInsideCanvas,
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
			TEXT("REBIRTH_FRONTEND PASS contract=3 resolution=%dx%d ")
			TEXT("keyboard_access=1 gamepad_access=1 dpad_down=1 ")
			TEXT("keyboard_up=1 gamepad_close=1 keyboard_pause=1 ")
			TEXT("gamepad_pause=1 display=1 dialogue=1 dialogue_default=1 ")
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
		return;
	}
	bAccessibilityMenuVisible = false;
	const bool bReturnsToSystemMenu =
		AccessibilityReturnMode == EIGSystemMenuMode::Title
		|| AccessibilityReturnMode == EIGSystemMenuMode::Pause
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
	bJournalInputHeld = false;
	if (!bDisplaySettingsAwaitingConfirmation)
	{
		SetActorTickEnabled(false);
	}
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
		UGameplayStatics::PlaySound2D(
			this,
			Paper,
			FMath::Clamp(VolumeMultiplier, 0.0f, 1.0f));
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

	if (AccessibilitySelection == 12)
	{
		if (bConfirm)
		{
			Accessibility->ResetToDefaults();
		}
		RefreshMenuHud();
		return;
	}
	if (AccessibilitySelection == 13)
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
	case 0:
	{
		constexpr int32 HintModeCount = 3;
		const int32 Current = static_cast<int32>(Settings.HintMode);
		Settings.HintMode = static_cast<EIGHintMode>(
			(Current + (Direction < 0 ? HintModeCount - 1 : 1))
			% HintModeCount);
		break;
	}
	case 1:
		Settings.bReducedCameraMotion = !Settings.bReducedCameraMotion;
		break;
	case 2:
		Settings.bReducedFlicker = !Settings.bReducedFlicker;
		break;
	case 3:
		Settings.bDirectionalFearCues = !Settings.bDirectionalFearCues;
		break;
	case 4:
		Settings.bAutoConnectEvidence = !Settings.bAutoConnectEvidence;
		break;
	case 5:
		Settings.bSubtitlesEnabled = !Settings.bSubtitlesEnabled;
		break;
	case 6:
		Settings.bSoundCaptionsEnabled = !Settings.bSoundCaptionsEnabled;
		break;
	case 7:
		Settings.CaptionSizeScale = FMath::Clamp(
			Settings.CaptionSizeScale + (Direction < 0 ? -0.10f : 0.10f),
			0.85f,
			2.0f);
		break;
	case 8:
		Settings.CaptionBackgroundOpacity = FMath::Clamp(
			Settings.CaptionBackgroundOpacity
				+ (Direction < 0 ? -0.10f : 0.10f),
			0.0f,
			1.0f);
		break;
	case 9:
		Settings.CaptionSafeAreaScale = FMath::Clamp(
			Settings.CaptionSafeAreaScale + (Direction < 0 ? -0.05f : 0.05f),
			0.80f,
			1.0f);
		break;
	case 10:
		Settings.bToggleHoldInteractions = !Settings.bToggleHoldInteractions;
		break;
	case 11:
		Settings.HoldDurationScale = FMath::Clamp(
			Settings.HoldDurationScale + (Direction < 0 ? -0.25f : 0.25f),
			0.25f,
			1.0f);
		break;
	default:
		return;
	}
	Accessibility->ApplySettings(Settings);
	RefreshMenuHud();
}

void AIGPlayerController::MoveSystemMenuSelection(const int32 Direction)
{
	if (SystemMenuMode == EIGSystemMenuMode::DisplaySettings)
	{
		MoveDisplaySettingsSelection(Direction);
		return;
	}
	if (SystemMenuMode == EIGSystemMenuMode::Hidden
		|| SystemMenuMode == EIGSystemMenuMode::Credits
		|| Direction == 0)
	{
		return;
	}
	bNewGameConfirmationArmed = false;
	SystemMenuStatusText = FText::GetEmpty();
	bSystemMenuStatusIsError = false;

	for (int32 Attempt = 0; Attempt < IGSystemMenu::RowCount; ++Attempt)
	{
		SystemMenuSelection =
			(SystemMenuSelection
				+ (Direction < 0 ? IGSystemMenu::RowCount - 1 : 1))
			% IGSystemMenu::RowCount;
		if (IsSystemMenuRowEnabled(SystemMenuSelection))
		{
			break;
		}
	}
	RefreshMenuHud();
}

void AIGPlayerController::ConfirmSystemMenuSelection()
{
	if (SystemMenuMode == EIGSystemMenuMode::Hidden)
	{
		return;
	}
	if (SystemMenuMode == EIGSystemMenuMode::Credits)
	{
		ReturnFromCredits();
		return;
	}
	if (SystemMenuMode == EIGSystemMenuMode::DisplaySettings)
	{
		ConfirmDisplaySettingsSelection();
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
	if (NewMode != EIGSystemMenuMode::Title)
	{
		bNewGameConfirmationArmed = false;
	}
	if (NewMode != EIGSystemMenuMode::DisplaySettings)
	{
		bDisplaySettingsApplied = false;
	}
	if (NewMode == EIGSystemMenuMode::Title
		|| NewMode == EIGSystemMenuMode::Pause)
	{
		bCompatibleAutosaveAvailable = HasCompatibleAutosave();
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

void AIGPlayerController::OpenDisplaySettings()
{
	if (SystemMenuMode != EIGSystemMenuMode::Title
		&& SystemMenuMode != EIGSystemMenuMode::Pause)
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
			: EIGSystemMenuMode::Title;
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

	DisplayQualityIndex = Settings->GetOverallScalabilityLevel() <= 0 ? 0 : 1;
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
		DisplaySettingsSelection = DisplaySettingsSelection == 6 ? 7 : 6;
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
	bDisplaySettingsApplied = false;
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
		if (DisplaySettingsSelection == 6)
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
	if (DisplaySettingsSelection == 5)
	{
		ToggleAccessibilityMenu();
		return;
	}
	if (DisplaySettingsSelection == 6)
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
	Settings->SetFullscreenMode(WindowMode);
	Settings->SetScreenResolution(
		IGDisplaySettings::Resolutions[DisplayResolutionIndex]);
	Settings->SetOverallScalabilityLevel(DisplayQualityIndex == 0 ? 0 : 2);
	Settings->SetVSyncEnabled(bDisplayVSync);
	Settings->SetFrameRateLimit(
		IGDisplaySettings::FrameLimits[DisplayFrameLimitIndex]);
	Settings->ApplyResolutionSettings(false);
	Settings->ApplyNonResolutionSettings();
	bDisplaySettingsApplied = false;
	bDisplaySettingsAwaitingConfirmation = true;
	DisplaySettingsSelection = 6;
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
	SetActorTickEnabled(false);
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
	SetActorTickEnabled(false);
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
		TEXT("IGIgnoreDirectStart=1?IGNewGame=1"));
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
		Presentation.bCredits = SystemMenuMode == EIGSystemMenuMode::Credits;
		Presentation.bDisplaySettings =
			SystemMenuMode == EIGSystemMenuMode::DisplaySettings;
		Presentation.bCanContinue = bCompatibleAutosaveAvailable;
		Presentation.bConfirmNewGame = bNewGameConfirmationArmed;
		Presentation.bVSync = bDisplayVSync;
		Presentation.bDisplaySettingsApplied = bDisplaySettingsApplied;
		Presentation.bDisplaySettingsAwaitingConfirmation =
			bDisplaySettingsAwaitingConfirmation;
		Presentation.bStatusIsError = bSystemMenuStatusIsError;
		Presentation.SelectedRow = SystemMenuSelection;
		Presentation.DisplaySelectedRow = DisplaySettingsSelection;
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

void AIGPlayerController::UpdateMenuPointerHover()
{
	int32 Row = INDEX_NONE;
	if (bAccessibilityMenuVisible)
	{
		if (TryGetMenuRowFromPointer(
				IGAccessibilityMenu::RowCount,
				116.0f,
				0.18f,
				28.0f,
				38.0f,
				0.047f,
				Row)
			&& AccessibilitySelection != Row)
		{
			AccessibilitySelection = Row;
			RefreshMenuHud();
		}
		return;
	}
	if (SystemMenuMode == EIGSystemMenuMode::DisplaySettings)
	{
		if (TryGetMenuRowFromPointer(
				IGDisplaySettings::RowCount,
				174.0f,
				0.24f,
				34.0f,
				42.0f,
				0.055f,
				Row)
			&& (!bDisplaySettingsAwaitingConfirmation || Row >= 6)
			&& DisplaySettingsSelection != Row)
		{
			DisplaySettingsSelection = Row;
			RefreshMenuHud();
		}
		return;
	}
	if (SystemMenuMode == EIGSystemMenuMode::Title
		|| SystemMenuMode == EIGSystemMenuMode::Pause)
	{
		if (TryGetMenuRowFromPointer(
				IGSystemMenu::RowCount,
				244.0f,
				0.36f,
				36.0f,
				46.0f,
				0.062f,
				Row)
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
	if (bAccessibilityMenuVisible)
	{
		if (TryGetMenuRowFromPointer(
			IGAccessibilityMenu::RowCount,
			116.0f,
			0.18f,
			28.0f,
			38.0f,
			0.047f,
			Row))
		{
			AccessibilitySelection = Row;
			ChangeAccessibilitySetting(1, true);
		}
		return true;
	}
	if (SystemMenuMode == EIGSystemMenuMode::DisplaySettings)
	{
		if (TryGetMenuRowFromPointer(
			IGDisplaySettings::RowCount,
			174.0f,
			0.24f,
			34.0f,
			42.0f,
			0.055f,
			Row)
			&& (!bDisplaySettingsAwaitingConfirmation || Row >= 6))
		{
			DisplaySettingsSelection = Row;
			ConfirmDisplaySettingsSelection();
		}
		return true;
	}
	if (TryGetMenuRowFromPointer(
		IGSystemMenu::RowCount,
		244.0f,
		0.36f,
		36.0f,
		46.0f,
		0.062f,
		Row)
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

bool AIGPlayerController::ShouldShowTitleMenu() const
{
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
		|| FParse::Param(CommandLine, TEXT("IGFrontendShippingProbe")))
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
		|| SystemMenuMode == EIGSystemMenuMode::DisplaySettings
		|| SystemMenuMode == EIGSystemMenuMode::Hidden)
	{
		return false;
	}
	const int32 LoadRow =
		SystemMenuMode == EIGSystemMenuMode::Title ? 0 : 1;
	return Row != LoadRow || bCompatibleAutosaveAvailable;
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
