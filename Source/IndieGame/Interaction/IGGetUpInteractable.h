#pragma once

#include "CoreMinimal.h"
#include "Interaction/IGInteractableActor.h"
#include "IGGetUpInteractable.generated.h"

class AIGWakeUpDirector;

/** Context action shown after the alarm is stopped; it advances the wake director once. */
UCLASS(Blueprintable)
class INDIEGAME_API AIGGetUpInteractable : public AIGInteractableActor
{
	GENERATED_BODY()

public:
	AIGGetUpInteractable();

	virtual bool CanInteract_Implementation(AActor* Interactor) const override;
	virtual void CompleteInteraction_Implementation(const FIGInteractionContext& Context) override;

protected:
	virtual void BeginPlay() override;

	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Wake Flow")
	TObjectPtr<AIGWakeUpDirector> WakeUpDirector;
};
