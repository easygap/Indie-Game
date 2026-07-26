#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Interaction/IGInteractable.h"
#include "IGInteractableActor.generated.h"

/** Tick-free reusable base for world interaction targets. */
UCLASS(Abstract, Blueprintable)
class INDIEGAME_API AIGInteractableActor : public AActor, public IIGInteractable
{
	GENERATED_BODY()

public:
	AIGInteractableActor();

	UFUNCTION(BlueprintCallable, Category = "Interaction")
	void SetInteractionEnabled(bool bEnabled);

	UFUNCTION(BlueprintPure, Category = "Interaction")
	bool IsInteractionEnabled() const { return bInteractionEnabled; }

	UFUNCTION(BlueprintCallable, Category = "Interaction")
	void SetInteractionPrompt(const FText& InPrompt) { InteractionPrompt = InPrompt; }

	virtual bool CanInteract_Implementation(AActor* Interactor) const override;
	virtual FText GetInteractionPrompt_Implementation(AActor* Interactor) const override;
	virtual FGameplayTag GetInteractionTag_Implementation(AActor* Interactor) const override;
	virtual float GetInteractionHoldDuration_Implementation(AActor* Interactor) const override;
	virtual void BeginInteraction_Implementation(const FIGInteractionContext& Context) override;
	virtual void UpdateInteraction_Implementation(const FIGInteractionContext& Context) override;
	virtual void CompleteInteraction_Implementation(const FIGInteractionContext& Context) override;
	virtual void EndInteraction_Implementation(
		const FIGInteractionContext& Context,
		EIGInteractionEndReason EndReason) override;

protected:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Interaction")
	bool bInteractionEnabled = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Interaction")
	FText InteractionPrompt;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Interaction")
	FGameplayTag InteractionTag;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Interaction", meta = (ClampMin = "0.0", Units = "s"))
	float InteractionHoldDuration = 0.0f;
};
