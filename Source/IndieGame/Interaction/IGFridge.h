#pragma once

#include "CoreMinimal.h"
#include "Interaction/IGDoorAnimation.h"
#include "Interaction/IGInteractableActor.h"
#include "IGFridge.generated.h"

class AIGFridge;
class UAudioComponent;
class UMaterialInterface;
class UPointLightComponent;
class USceneComponent;
class UStaticMesh;
class UStaticMeshComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
	FIGFridgeInspectedSignature,
	AIGFridge*, Fridge);

/**
 * Small studio refrigerator with an animated door and an interior reveal.
 * The first time the door finishes opening it reports the "no water left"
 * inspection exactly once and records the configured story state.
 */
UCLASS(Blueprintable)
class INDIEGAME_API AIGFridge : public AIGInteractableActor
{
	GENERATED_BODY()

public:
	AIGFridge();
	virtual void Tick(float DeltaSeconds) override;

	/** Authored props placed inside the fridge (see Scripts/generate_meshes.py). */
	struct FIGFridgeContentMeshes
	{
		UStaticMesh* KimchiTub = nullptr;
		UStaticMesh* MilkCarton = nullptr;
		UStaticMesh* SojuBottle = nullptr;
	};

	/** Supplies the lathed interior props; call before ConfigurePrototypeVisuals. */
	void SetContentMeshes(const FIGFridgeContentMeshes& InContentMeshes)
	{
		ContentMeshes = InContentMeshes;
	}

	/** Builds the greybox shell; safe to call once before or during BeginPlay. */
	void ConfigurePrototypeVisuals(
		UStaticMesh* CubeMesh,
		UStaticMesh* CylinderMesh,
		UMaterialInterface* BodyMaterial,
		UMaterialInterface* InteriorMaterial,
		UMaterialInterface* HandleMaterial,
		UMaterialInterface* GlassMaterial,
		UMaterialInterface* MetalMaterial,
		UMaterialInterface* AccentRedMaterial,
		UMaterialInterface* AccentYellowMaterial,
		UMaterialInterface* BottleGreenMaterial);

	UFUNCTION(BlueprintPure, Category = "Fridge")
	bool IsDoorOpen() const { return bDoorOpen; }

	UFUNCTION(BlueprintPure, Category = "Fridge")
	bool HasBeenInspected() const { return bInspectionDone; }

	/** World-space transform of the middle interior shelf surface. */
	UFUNCTION(BlueprintPure, Category = "Fridge")
	FTransform GetInteriorShelfTransform() const;

	/** Unscaled hinge pivot; dressing attached here follows the door swing. */
	UFUNCTION(BlueprintPure, Category = "Fridge")
	USceneComponent* GetDoorPivot() const { return DoorPivot; }

	/** Marks the reveal as consumed without side effects (checkpoint restore). */
	UFUNCTION(BlueprintCallable, Category = "Fridge")
	void ApplyRestoredInspectedState();

	/**
	 * Rebinds the empty-fridge reveal to a chapter-specific story state and
	 * inner-voice line. Invalid tags leave the current tag unchanged.
	 */
	UFUNCTION(BlueprintCallable, Category = "Fridge|Story")
	void ConfigureChapterState(
		FGameplayTag InInspectedStateTag,
		const FText& InInspectionThought);

	/**
	 * Silently closes the door and clears the one-shot inspection latch.
	 * Story-state removal is intentionally owned by the chapter director.
	 */
	UFUNCTION(BlueprintCallable, Category = "Fridge|Story")
	void ResetForNewChapter();

	UPROPERTY(BlueprintAssignable, Category = "Fridge|Events")
	FIGFridgeInspectedSignature OnFridgeInspected;

	virtual bool CanInteract_Implementation(AActor* Interactor) const override;
	virtual FText GetInteractionPrompt_Implementation(AActor* Interactor) const override;
	virtual void CompleteInteraction_Implementation(const FIGInteractionContext& Context) override;

protected:
	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Fridge|Components")
	TObjectPtr<USceneComponent> FridgeRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Fridge|Components")
	TObjectPtr<USceneComponent> DoorPivot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Fridge|Components")
	TObjectPtr<UStaticMeshComponent> DoorMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Fridge|Components")
	TObjectPtr<UStaticMeshComponent> HandleMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Fridge|Components")
	TObjectPtr<UPointLightComponent> InteriorLight;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Fridge|Components")
	TObjectPtr<UAudioComponent> HumAudioComponent;

	/** Yaw applied to the door pivot when fully open (positive swings toward local -X/+Y). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Fridge", meta = (ClampMin = "-179.0", ClampMax = "179.0"))
	float OpenYaw = 118.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Fridge", meta = (ClampMin = "0.05", Units = "s"))
	float DoorAnimDuration = 0.85f;

	/** Delay between the door finishing opening and the empty-shelf realization. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Fridge", meta = (ClampMin = "0.0", Units = "s"))
	float InspectionDelay = 0.9f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Fridge|Story")
	FGameplayTag InspectedStateTag;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Fridge|Story")
	FText InspectionThought;

private:
	void BeginDoorSwing(bool bOpen);
	void HandleInspectionTimer();
	void UpdateHumIntensity();

	TArray<TObjectPtr<UStaticMeshComponent>> ShellMeshes;
	FIGFridgeContentMeshes ContentMeshes;
	FIGDoorAnimation DoorAnimation;
	FTimerHandle InspectionTimerHandle;
	FVector InteriorShelfLocalCenter = FVector::ZeroVector;
	bool bDoorOpen = false;
	bool bInspectionDone = false;
	bool bVisualsConfigured = false;
};
