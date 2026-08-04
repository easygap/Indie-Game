#include "Player/IGPlayerController.h"

#include "Accessibility/IGAccessibilitySubsystem.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"
#include "EngineUtils.h"
#include "InputCoreTypes.h"
#include "InputKeyEventArgs.h"
#include "Kismet/GameplayStatics.h"
#include "Player/IGHorrorHUD.h"
#include "Sequence/IGSecondMorningDirector.h"
#include "Sequence/IGThirdMorningDirector.h"

namespace IGAccessibilityMenu
{
	constexpr int32 RowCount = 12;
}

AIGPlayerController::AIGPlayerController()
{
	PrimaryActorTick.bCanEverTick = false;
}

void AIGPlayerController::BeginPlay()
{
	Super::BeginPlay();

	bShowMouseCursor = false;
	FInputModeGameOnly InputMode;
	SetInputMode(InputMode);
	ApplyDefaultInputMapping();
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
		BindPausedAction(TEXT("ToggleCursor"), &ThisClass::ToggleCursorMode);
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
	return Super::InputKey(Params);
}

void AIGPlayerController::ToggleCursorMode()
{
	if (bAccessibilityMenuVisible)
	{
		CloseAccessibilityMenu();
		return;
	}
	bShowMouseCursor = !bShowMouseCursor;
	if (bShowMouseCursor)
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

void AIGPlayerController::ToggleAccessibilityMenu()
{
	if (bAccessibilityMenuVisible)
	{
		CloseAccessibilityMenu();
		return;
	}

	bAccessibilityMenuVisible = true;
	bGameWasPausedBeforeAccessibility =
		UGameplayStatics::IsGamePaused(this);
	SetPause(true);
	bShowMouseCursor = false;
	FInputModeGameOnly InputMode;
	SetInputMode(InputMode);
	RefreshAccessibilityHud();
}

void AIGPlayerController::CloseAccessibilityMenu()
{
	if (!bAccessibilityMenuVisible)
	{
		return;
	}
	bAccessibilityMenuVisible = false;
	SetPause(bGameWasPausedBeforeAccessibility);
	bShowMouseCursor = false;
	FInputModeGameOnly InputMode;
	SetInputMode(InputMode);
	RefreshAccessibilityHud();
}

void AIGPlayerController::MoveAccessibilitySelectionUp()
{
	if (!bAccessibilityMenuVisible)
	{
		return;
	}
	AccessibilitySelection =
		(AccessibilitySelection + IGAccessibilityMenu::RowCount - 1)
		% IGAccessibilityMenu::RowCount;
	RefreshAccessibilityHud();
}

void AIGPlayerController::MoveAccessibilitySelectionDown()
{
	if (!bAccessibilityMenuVisible)
	{
		return;
	}
	AccessibilitySelection =
		(AccessibilitySelection + 1) % IGAccessibilityMenu::RowCount;
	RefreshAccessibilityHud();
}

void AIGPlayerController::AdjustAccessibilityLeft()
{
	ChangeAccessibilitySetting(-1, false);
}

void AIGPlayerController::AdjustAccessibilityRight()
{
	ChangeAccessibilitySetting(1, false);
}

void AIGPlayerController::ConfirmAccessibilitySelection()
{
	ChangeAccessibilitySetting(1, true);
}

void AIGPlayerController::RequestManualHint()
{
	if (bAccessibilityMenuVisible || !GetWorld())
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
		RefreshAccessibilityHud();
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
	RefreshAccessibilityHud();
}

void AIGPlayerController::RefreshAccessibilityHud() const
{
	if (AIGHorrorHUD* HorrorHUD = Cast<AIGHorrorHUD>(GetHUD()))
	{
		HorrorHUD->SetAccessibilityMenuState(
			bAccessibilityMenuVisible,
			AccessibilitySelection);
		HorrorHUD->SetInputDevicePresentation(bUsingGamepadForHud);
	}
}

void AIGPlayerController::SetInputDevicePresentation(const bool bUsingGamepad)
{
	bUsingGamepadForHud = bUsingGamepad;
	RefreshAccessibilityHud();
}

UIGAccessibilitySubsystem*
AIGPlayerController::GetAccessibilitySubsystem() const
{
	const UGameInstance* GameInstance = GetGameInstance();
	return GameInstance
		? GameInstance->GetSubsystem<UIGAccessibilitySubsystem>()
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
