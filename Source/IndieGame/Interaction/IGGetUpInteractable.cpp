#include "Interaction/IGGetUpInteractable.h"

#include "Sequence/IGWakeUpDirector.h"

AIGGetUpInteractable::AIGGetUpInteractable()
{
	InteractionPrompt = NSLOCTEXT("IGGetUpInteractable", "GetUpPrompt", "Get up");
	InteractionHoldDuration = 0.0f;
}

void AIGGetUpInteractable::BeginPlay()
{
	Super::BeginPlay();
	if (!InteractionTag.IsValid())
	{
		InteractionTag = FGameplayTag::RequestGameplayTag(
			FName(TEXT("Interaction.GetUp")),
			false);
	}
}

bool AIGGetUpInteractable::CanInteract_Implementation(AActor* Interactor) const
{
	return Super::CanInteract_Implementation(Interactor)
		&& WakeUpDirector
		&& WakeUpDirector->GetWakeState() == EIGWakeState::BedLocked;
}

void AIGGetUpInteractable::CompleteInteraction_Implementation(const FIGInteractionContext& Context)
{
	Super::CompleteInteraction_Implementation(Context);

	if (WakeUpDirector && WakeUpDirector->RequestGetUp())
	{
		SetInteractionEnabled(false);
	}
}
