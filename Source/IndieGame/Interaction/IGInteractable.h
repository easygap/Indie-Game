#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "Interaction/IGInteractionTypes.h"
#include "IGInteractable.generated.h"

UINTERFACE(BlueprintType, Blueprintable)
class INDIEGAME_API UIGInteractable : public UInterface
{
	GENERATED_BODY()
};

/** Contract for actors that can be driven by UIGInteractionComponent. */
class INDIEGAME_API IIGInteractable
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Interaction")
	bool CanInteract(AActor* Interactor) const;

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Interaction")
	FText GetInteractionPrompt(AActor* Interactor) const;

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Interaction")
	FGameplayTag GetInteractionTag(AActor* Interactor) const;

	/** Zero means the interaction completes as soon as it is pressed. */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Interaction")
	float GetInteractionHoldDuration(AActor* Interactor) const;

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Interaction")
	void BeginInteraction(const FIGInteractionContext& Context);

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Interaction")
	void UpdateInteraction(const FIGInteractionContext& Context);

	/** Called once for a successful interaction, before EndInteraction. */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Interaction")
	void CompleteInteraction(const FIGInteractionContext& Context);

	/** Called exactly once for every interaction that successfully began. */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Interaction")
	void EndInteraction(const FIGInteractionContext& Context, EIGInteractionEndReason EndReason);
};
