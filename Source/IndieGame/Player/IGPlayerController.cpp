#include "Player/IGPlayerController.h"

#include "Accessibility/IGAccessibilitySubsystem.h"
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
#include "HAL/PlatformTime.h"
#include "Misc/App.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Narrative/IGRebirthNarrativeSubsystem.h"
#include "Narrative/IGStoryStateSubsystem.h"
#include "Player/IGHorrorHUD.h"
#include "Save/IGSaveSubsystem.h"
#include "Sequence/IGSecondMorningDirector.h"
#include "Sequence/IGThirdMorningDirector.h"

namespace IGAccessibilityMenu
{
	constexpr int32 RowCount = 12;
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
}

void AIGPlayerController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	UnbindSaveNotifications();
	Super::EndPlay(EndPlayReason);
}

void AIGPlayerController::Tick(const float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!bDisplaySettingsAwaitingConfirmation)
	{
		SetActorTickEnabled(false);
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
		InputComponent->BindAction(
			TEXT("RequestHint"),
			IE_Pressed,
			this,
			&ThisClass::RequestManualHint);
	}
}

bool AIGPlayerController::InputKey(const FInputKeyEventArgs& Params)
{
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

void AIGPlayerController::ToggleSystemMenu()
{
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

	if (AccessibilitySelection == 10)
	{
		if (bConfirm)
		{
			Accessibility->ResetToDefaults();
		}
		RefreshMenuHud();
		return;
	}
	if (AccessibilitySelection == 11)
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
		Settings.CaptionSizeScale = FMath::Clamp(
			Settings.CaptionSizeScale + (Direction < 0 ? -0.10f : 0.10f),
			0.85f,
			1.25f);
		break;
	case 7:
		Settings.CaptionSafeAreaScale = FMath::Clamp(
			Settings.CaptionSafeAreaScale + (Direction < 0 ? -0.05f : 0.05f),
			0.80f,
			1.0f);
		break;
	case 8:
		Settings.bToggleHoldInteractions = !Settings.bToggleHoldInteractions;
		break;
	case 9:
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
	const bool bMenuVisible = bAccessibilityMenuVisible
		|| SystemMenuMode != EIGSystemMenuMode::Hidden;
	bShowMouseCursor = bMenuVisible && !bUsingGamepadForHud;
	if (bMenuVisible)
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
		|| FParse::Param(CommandLine, TEXT("IGChapterThree")))
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
