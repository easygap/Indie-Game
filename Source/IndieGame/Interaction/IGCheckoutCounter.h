#pragma once

#include "CoreMinimal.h"
#include "Interaction/IGInteractableActor.h"
#include "IGCheckoutCounter.generated.h"

class AIGCheckoutCounter;
class UMaterialInterface;
class USceneComponent;
class UStaticMesh;
class UStaticMeshComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
	FIGCheckoutSignature,
	AIGCheckoutCounter*, Counter);

/**
 * Self-service register. It only offers an interaction while the player is
 * holding the water and the purchase has not happened yet; completing it
 * records the purchase state and plays the scan/register presentation.
 */
UCLASS(Blueprintable)
class INDIEGAME_API AIGCheckoutCounter : public AIGInteractableActor
{
	GENERATED_BODY()

public:
	AIGCheckoutCounter();

	/** Builds the register/scanner cluster that sits on the counter top. */
	void ConfigurePrototypeVisuals(
		UStaticMesh* CubeMesh,
		UMaterialInterface* BodyMaterial,
		UMaterialInterface* ScreenMaterial);

	/** Hides the greybox visuals (a scanned prop stands in) but keeps collision. */
	void SetVisualsHidden(bool bInHidden);

	/**
	 * Rebinds this register to one chapter's inventory/purchase states and
	 * presentation copy. Invalid tags leave their current values unchanged.
	 */
	UFUNCTION(BlueprintCallable, Category = "Checkout|Story")
	void ConfigureChapterPurchase(
		FGameplayTag InRequiredStateTag,
		FGameplayTag InPurchasedStateTag,
		const FText& InInteractionPrompt,
		const FText& InPurchaseThought);

	/**
	 * Reuses the physical POS as a story action that does not require a carried
	 * product. CH02 uses this for the employee-call button: the completion tag
	 * is committed, but checkout audio is deliberately suppressed.
	 */
	UFUNCTION(BlueprintCallable, Category = "Checkout|Story")
	void ConfigureChapterAction(
		FGameplayTag InCompletedStateTag,
		const FText& InInteractionPrompt,
		const FText& InCompletionThought);

	/**
	 * Adds a second story prerequisite without replacing the carried-item
	 * requirement. Passing an invalid tag clears the extra gate.
	 */
	UFUNCTION(BlueprintCallable, Category = "Checkout|Story")
	void SetAdditionalRequiredState(FGameplayTag InRequiredStateTag);

	/**
	 * Cancels delayed register audio and makes the counter available again.
	 * The chapter director remains responsible for clearing story states.
	 */
	UFUNCTION(BlueprintCallable, Category = "Checkout|Story")
	void ResetForNewChapter();

	UPROPERTY(BlueprintAssignable, Category = "Checkout|Events")
	FIGCheckoutSignature OnPurchaseCompleted;

	virtual bool CanInteract_Implementation(AActor* Interactor) const override;
	virtual void CompleteInteraction_Implementation(const FIGInteractionContext& Context) override;

protected:
	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Checkout|Components")
	TObjectPtr<USceneComponent> CheckoutRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Checkout|Components")
	TObjectPtr<UStaticMeshComponent> RegisterMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Checkout|Components")
	TObjectPtr<UStaticMeshComponent> ScreenMesh;

	/** Must be present before the register accepts the interaction. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Checkout|Story")
	FGameplayTag RequiredStateTag;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Checkout|Story")
	FGameplayTag PurchasedStateTag;

	/** Optional clue/route prerequisite in addition to RequiredStateTag. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Checkout|Story")
	FGameplayTag AdditionalRequiredStateTag;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Checkout|Story")
	FText PurchaseThought;

private:
	void PlayRegisterTimerElapsed();

	FTimerHandle RegisterSoundTimerHandle;
	bool bRequiresPrimaryState = true;
	bool bPlayRegisterPresentation = true;
};
