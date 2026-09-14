#pragma once

#include "CoreMinimal.h"
#include "Interaction/IGDoorAnimation.h"
#include "Interaction/IGInteractableActor.h"
#include "IGSwingDoor.generated.h"

class UMaterialInterface;
class USceneComponent;
class UStaticMesh;
class UStaticMeshComponent;

/** One gate condition for a lockable door, checked in order. */
USTRUCT(BlueprintType)
struct INDIEGAME_API FIGDoorRequirement
{
	GENERATED_BODY()

	/** Story state that must be present for the door to open. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Door")
	FGameplayTag RequiredState;

	/** Interaction prompt shown while this requirement is unmet. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Door")
	FText LockedPrompt;

	/** Inner-voice line pushed when the player tries the locked door. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Door")
	FText LockedThought;
};

/**
 * Hinged interactable door. The actor location is the hinge axis; the panel
 * extends along +Y in local space. Optional story-state requirements keep it
 * locked with contextual feedback until the player is ready to leave.
 */
UCLASS(Blueprintable)
class INDIEGAME_API AIGSwingDoor : public AIGInteractableActor
{
	GENERATED_BODY()

public:
	AIGSwingDoor();
	virtual void Tick(float DeltaSeconds) override;

	/** Builds the panel/handle meshes; call once before or during BeginPlay. */
	void ConfigurePrototypeVisuals(
		UStaticMesh* CubeMesh,
		UMaterialInterface* DoorMaterial,
		UMaterialInterface* HandleMaterial,
		const FVector& PanelSize);

	/**
	 * Storefront-style variant: aluminum frame, mid rail, push bar and two
	 * glass insets, so the glass door reads as a door rather than a hole.
	 */
	void ConfigureFramedGlassVisuals(
		UStaticMesh* CubeMesh,
		UMaterialInterface* GlassMaterial,
		UMaterialInterface* FrameMaterial,
		const FVector& PanelSize);

	void SetRequirements(TArray<FIGDoorRequirement>&& InRequirements);

	/** Swing direction/extent; negative yaw opens toward local -Y. */
	void SetOpenYaw(float InOpenYaw) { OpenYaw = InOpenYaw; }

	/** Rotating leaf pivot; dressing attached here follows the swing. */
	USceneComponent* GetDoorPivot() const { return DoorPivot; }

	/**
	 * Swaps the box handle for an authored lever mesh (rose plate + swept
	 * lever, modelled in centimeters with the rose facing +Z).
	 */
	void SetLeverMesh(UStaticMesh* LeverMesh, UMaterialInterface* Material, const FVector& PanelSize);

	/**
	 * Blender에서 구운 왼손 문짝(SM_UnitDoorLeafL)을 통째로 쓴다. 그 메시는
	 * 원점이 바닥 중심, 앞면이 -Y, 힌지가 +X 쪽이라 yaw -90으로 놓으면 힌지
	 * 축이 이 액터의 원점에, 문짝은 +Y로, 바깥면은 -X로 온다. 레버·도어락은
	 * 원점이 같은 SM_UnitDoorHardwareL이라 손잡이 컴포넌트에 문짝과 같은
	 * 자세로 달아 함께 돌린다. 상세 블록은 만들지 않는다.
	 * ConfigurePrototypeVisuals 대신 부른다.
	 */
	void ConfigureAuthoredLeaf(
		UStaticMesh* LeafMesh, UStaticMesh* HardwareMesh, const FVector& PanelSize);

	UFUNCTION(BlueprintPure, Category = "Door")
	bool IsOpen() const { return bOpen; }

	UFUNCTION(BlueprintPure, Category = "Door")
	bool IsLocked() const;

	/** Immediately snaps the leaf open or shut without audio or events. */
	UFUNCTION(BlueprintCallable, Category = "Door|Script")
	void ForceOpenState(bool bInOpen);

	/**
	 * Starts a lock-bypassing authored swing. A scripted close can play only
	 * the hinge creak by enabling bPlayCreak and suppressing its final thud.
	 */
	UFUNCTION(BlueprintCallable, Category = "Door|Script")
	bool BeginScriptedSwing(
		bool bInOpen,
		bool bPlayCreak = true,
		bool bSuppressCloseThud = false);

	virtual bool CanInteract_Implementation(AActor* Interactor) const override;
	virtual FText GetInteractionPrompt_Implementation(AActor* Interactor) const override;
	virtual float GetInteractionHoldDuration_Implementation(AActor* Interactor) const override;
	virtual void CompleteInteraction_Implementation(const FIGInteractionContext& Context) override;
	virtual void EndInteraction_Implementation(
		const FIGInteractionContext& Context,
		EIGInteractionEndReason EndReason) override;

protected:
	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Door|Components")
	TObjectPtr<USceneComponent> HingeRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Door|Components")
	TObjectPtr<USceneComponent> DoorPivot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Door|Components")
	TObjectPtr<UStaticMeshComponent> DoorMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Door|Components")
	TObjectPtr<UStaticMeshComponent> HandleMesh;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Door", meta = (ClampMin = "-179.0", ClampMax = "179.0"))
	float OpenYaw = -100.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Door", meta = (ClampMin = "0.05", Units = "s"))
	float SwingDuration = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Door")
	FText OpenPrompt;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Door")
	FText ClosePrompt;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Door")
	TArray<FIGDoorRequirement> Requirements;

	/**
	 * The 없는 층 verb (§5.3): holding eases the leaf open and barely sounds,
	 * a tap yanks it and carries. Zero disables the hold entirely and restores
	 * press-to-open, which is what a locked door does so its rattle is instant.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Door|Noise", meta = (ClampMin = "0.0", Units = "s"))
	float QuietOpenHoldSeconds = 1.4f;

	/** Reported loudness for an eased open/close, 0..1. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Door|Noise", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float QuietSwingLoudness = 0.1f;

	/** Reported loudness for a yanked open/close, 0..1. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Door|Noise", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float NormalSwingLoudness = 0.35f;

	/** How much longer an eased swing takes than a yanked one. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Door|Noise", meta = (ClampMin = "1.0"))
	float QuietSwingDurationScale = 1.8f;

private:
	const FIGDoorRequirement* FindUnmetRequirement() const;
	bool BeginSwing(
		bool bInOpen,
		bool bPlayCreak,
		bool bSuppressCloseThud,
		float Loudness,
		float DurationScale);
	/** Reports one sound to the building's ear; silent when the bus is absent. */
	void ReportSwingNoise(float Loudness) const;

	FIGDoorAnimation DoorAnimation;
	bool bOpen = false;
	bool bSuppressNextCloseThud = false;
	// 조용히 닫았을 때 걸쇠도 같은 힘으로 닫힌다.
	float CloseThudVolume = 0.9f;
};
