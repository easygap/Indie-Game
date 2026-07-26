#include "Player/IGPlayerController.h"

#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"

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
		InputComponent->BindAction(TEXT("ToggleCursor"), IE_Pressed, this, &ThisClass::ToggleCursorMode);
	}
}

void AIGPlayerController::ToggleCursorMode()
{
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
