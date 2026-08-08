#include "Entity/IGNightLoopDirector.h"

#include "Camera/PlayerCameraManager.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Entity/IGListenerEntity.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Narrative/IGMissingFloorNarrativeSubsystem.h"
#include "Player/IGPlayerCharacter.h"
#include "TimerManager.h"

AIGNightLoopDirector::AIGNightLoopDirector()
{
	PrimaryActorTick.bCanEverTick = false;
}

void AIGNightLoopDirector::BeginPlay()
{
	Super::BeginPlay();

	// A placed director adopts any entity already in the world; a spawner
	// that creates both can also pair them explicitly via RegisterEntity.
	if (!ListenerEntity.IsValid())
	{
		for (TActorIterator<AIGListenerEntity> It(GetWorld()); It; ++It)
		{
			RegisterEntity(*It);
			break;
		}
	}
}

void AIGNightLoopDirector::SetWakeTransform(const FTransform& Transform)
{
	WakeTransform = Transform;
	bWakeTransformSet = true;
}

void AIGNightLoopDirector::RegisterEntity(AIGListenerEntity* Entity)
{
	if (!Entity || ListenerEntity.Get() == Entity)
	{
		return;
	}
	ListenerEntity = Entity;
	Entity->OnPlayerCaptured.AddUObject(
		this, &AIGNightLoopDirector::HandlePlayerCaptured);
}

void AIGNightLoopDirector::HandlePlayerCaptured(APawn* Player)
{
	if (bResetInFlight)
	{
		return;
	}
	AIGPlayerCharacter* Character = Cast<AIGPlayerCharacter>(Player);
	if (!Character)
	{
		return;
	}

	bResetInFlight = true;
	++CaptureCount;
	CapturedPlayer = Character;

	// Without an authored wake point the first capture teaches us one: the
	// spot the player stood when the night began is better than nothing,
	// but directors in real maps must call SetWakeTransform.
	if (!bWakeTransformSet)
	{
		WakeTransform = Character->GetActorTransform();
		bWakeTransformSet = true;
	}

	if (APlayerController* Controller =
		Cast<APlayerController>(Character->GetController()))
	{
		Character->DisableInput(Controller);
		if (Controller->PlayerCameraManager)
		{
			Controller->PlayerCameraManager->StartCameraFade(
				0.0f, 1.0f, FadeOutSeconds, FLinearColor::Black,
				/*bShouldFadeAudio=*/false, /*bHoldWhenFinished=*/true);
		}
	}

	GetWorldTimerManager().SetTimer(
		ResetTimer,
		this,
		&AIGNightLoopDirector::FinishReset,
		FMath::Max(FadeOutSeconds, 0.05f) + 0.4f,
		false);
}

void AIGNightLoopDirector::FinishReset()
{
	AIGPlayerCharacter* Character = CapturedPlayer.Get();
	if (Character)
	{
		Character->TeleportTo(
			WakeTransform.GetLocation(),
			WakeTransform.Rotator(),
			false,
			true);
		if (UCharacterMovementComponent* Movement =
			Character->GetCharacterMovement())
		{
			Movement->StopMovementImmediately();
		}

		if (APlayerController* Controller =
			Cast<APlayerController>(Character->GetController()))
		{
			Controller->SetControlRotation(WakeTransform.Rotator());
			Character->EnableInput(Controller);
			if (Controller->PlayerCameraManager)
			{
				Controller->PlayerCameraManager->StartCameraFade(
					1.0f, 0.0f, FadeInSeconds, FLinearColor::Black,
					/*bShouldFadeAudio=*/false, /*bHoldWhenFinished=*/false);
			}
		}
	}

	if (AIGListenerEntity* Entity = ListenerEntity.Get())
	{
		// The tier lives in the narrative snapshot, not on the pawn, so a
		// quit-and-resume cannot hand the player back a patient pursuer.
		if (UIGMissingFloorNarrativeSubsystem* Narrative = GetNarrative())
		{
			Entity->SetAggressionTier(Narrative->RecordCapture());
			Entity->ResetToPatrolStart(/*bRaiseAggression=*/false);
		}
		else
		{
			Entity->ResetToPatrolStart(/*bRaiseAggression=*/true);
		}
	}

	CapturedPlayer = nullptr;
	bResetInFlight = false;
}

UIGMissingFloorNarrativeSubsystem* AIGNightLoopDirector::GetNarrative() const
{
	const UWorld* World = GetWorld();
	UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
	return GameInstance
		? GameInstance->GetSubsystem<UIGMissingFloorNarrativeSubsystem>()
		: nullptr;
}
