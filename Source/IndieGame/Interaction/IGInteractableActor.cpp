#include "Interaction/IGInteractableActor.h"

AIGInteractableActor::AIGInteractableActor()
{
	PrimaryActorTick.bCanEverTick = false;
	PrimaryActorTick.bStartWithTickEnabled = false;
	SetActorTickEnabled(false);
}

void AIGInteractableActor::SetInteractionEnabled(const bool bEnabled)
{
	bInteractionEnabled = bEnabled;
}

bool AIGInteractableActor::CanInteract_Implementation(AActor* Interactor) const
{
	return bInteractionEnabled;
}

FText AIGInteractableActor::GetInteractionPrompt_Implementation(AActor* Interactor) const
{
	return InteractionPrompt;
}

FGameplayTag AIGInteractableActor::GetInteractionTag_Implementation(AActor* Interactor) const
{
	return InteractionTag;
}

float AIGInteractableActor::GetInteractionHoldDuration_Implementation(AActor* Interactor) const
{
	return FMath::Max(0.0f, InteractionHoldDuration);
}

void AIGInteractableActor::BeginInteraction_Implementation(const FIGInteractionContext& Context)
{
}

void AIGInteractableActor::UpdateInteraction_Implementation(const FIGInteractionContext& Context)
{
}

void AIGInteractableActor::CompleteInteraction_Implementation(const FIGInteractionContext& Context)
{
}

void AIGInteractableActor::EndInteraction_Implementation(
	const FIGInteractionContext& Context,
	const EIGInteractionEndReason EndReason)
{
}
