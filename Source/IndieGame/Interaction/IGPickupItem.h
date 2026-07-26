#pragma once

#include "CoreMinimal.h"
#include "Interaction/IGInteractableActor.h"
#include "IGPickupItem.generated.h"

class AIGPickupItem;
class UMaterialInterface;
class UStaticMesh;
class UStaticMeshComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
	FIGPickupSignature,
	AIGPickupItem*, Item);

/** What happens to the item after a successful pickup. */
UENUM(BlueprintType)
enum class EIGPickupMode : uint8
{
	/** Attach to the player camera and stay visible (water bottle). */
	CarryInHand,
	/** Disappear into a pocket with only a thought line (wallet). */
	Pocket
};

/**
 * Grabbable world item, optionally physics-simulated while it sits in the
 * world. Picking it up records a story state and either parents the mesh to
 * the player camera or pockets it invisibly.
 */
UCLASS(Blueprintable)
class INDIEGAME_API AIGPickupItem : public AIGInteractableActor
{
	GENERATED_BODY()

public:
	AIGPickupItem();

	/** Assigns mesh/material/scale and optionally enables simulation. */
	void ConfigurePrototypeVisuals(
		UStaticMesh* Mesh,
		UMaterialInterface* Material,
		const FVector& Scale,
		bool bSimulatePhysics,
		float MassKg = 0.6f);

	UFUNCTION(BlueprintPure, Category = "Pickup")
	bool WasPickedUp() const { return bPickedUp; }

	UFUNCTION(BlueprintPure, Category = "Pickup")
	UStaticMeshComponent* GetMeshComponent() const { return MeshComponent; }

	/** Consumes the pickup silently for checkpoint restores. */
	UFUNCTION(BlueprintCallable, Category = "Pickup")
	void ApplyRestoredPickedUpState(bool bAttachToPlayer);

	UPROPERTY(BlueprintAssignable, Category = "Pickup|Events")
	FIGPickupSignature OnPickedUp;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pickup")
	EIGPickupMode PickupMode = EIGPickupMode::CarryInHand;

	/** Story state recorded when the item is taken. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pickup|Story")
	FGameplayTag StateTagOnPickup;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pickup|Story")
	FText ThoughtOnPickup;

	/** Camera-relative pose used by CarryInHand mode. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pickup")
	FVector CarryOffset = FVector(34.0f, 15.0f, -14.0f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pickup")
	FRotator CarryRotation = FRotator(0.0f, -12.0f, 0.0f);

	virtual bool CanInteract_Implementation(AActor* Interactor) const override;
	virtual void CompleteInteraction_Implementation(const FIGInteractionContext& Context) override;

protected:
	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Pickup|Components")
	TObjectPtr<UStaticMeshComponent> MeshComponent;

private:
	void FinishPickup(AActor* Interactor, bool bBroadcast, bool bPlayFeedback);

	bool bPickedUp = false;
};
