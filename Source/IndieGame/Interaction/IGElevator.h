#pragma once

#include "CoreMinimal.h"
#include "Interaction/IGDoorAnimation.h"
#include "Interaction/IGInteractableActor.h"
#include "IGElevator.generated.h"

class AIGElevator;
class APawn;
class UBoxComponent;
class UMaterialInterface;
class USceneComponent;
class UStaticMesh;
class UStaticMeshComponent;
class UTextRenderComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
	FIGElevatorArrivedSignature,
	AIGElevator*, Elevator);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
	FIGElevatorReturnedSignature,
	AIGElevator*, Elevator);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
	FIGElevatorIntermediateStopSignature,
	AIGElevator*, Elevator);

/**
 * Reusable villa elevator between the 4F corridor and the ground-floor lobby.
 *
 * Identical enclosed cab shells sit at 4F, 2F and 1F. Calling the elevator
 * (this actor's interaction is the hall button) opens the upper doors;
 * stepping into the cab closes them, the ride hums and chimes down the
 * floors while the rider is transferred only behind fully closed doors.
 *
 * The authored 2F interruption uses its own physical landing. It must never
 * borrow the 1F lobby doorway: the player should be able to trust every
 * threshold that is visible.
 */
UCLASS(Blueprintable)
class INDIEGAME_API AIGElevator : public AIGInteractableActor
{
	GENERATED_BODY()

public:
	AIGElevator();
	virtual void Tick(float DeltaSeconds) override;

	/** Meshes and materials the cab is dressed with. */
	struct FIGElevatorVisuals
	{
		UStaticMesh* CubeMesh = nullptr;
		UStaticMesh* CylinderMesh = nullptr;
		UStaticMesh* CallPlateMesh = nullptr;
		/** Hairline stainless: cab walls and jambs. */
		UMaterialInterface* StainlessMaterial = nullptr;
		/** Powder-coated landing/car door face, kept separate from the cab shell. */
		UMaterialInterface* DoorMaterial = nullptr;
		/** Brushed rear-wall stainless; no readable player reflection. */
		UMaterialInterface* MirrorMaterial = nullptr;
		/** Marble slab under foot, with a darker inlay material for the border. */
		UMaterialInterface* FloorMaterial = nullptr;
		UMaterialInterface* InlayMaterial = nullptr;
		/** Car operating panel artwork (buttons + red readout). */
		UMaterialInterface* CopMaterial = nullptr;
		/** Landing hall lantern artwork. */
		UMaterialInterface* HallMaterial = nullptr;
		/** Translucent ceiling diffuser panels. */
		UMaterialInterface* DiffuserMaterial = nullptr;
	};

	/**
	 * Builds both cab shells and door sets. The actor location is the center
	 * of the UPPER cab floor; FloorDeltaZ is how far DOWN the lower cab sits.
	 */
	void ConfigurePrototypeVisuals(const FIGElevatorVisuals& Visuals, float InFloorDeltaZ);

	UFUNCTION(BlueprintPure, Category = "Elevator")
	bool IsRideComplete() const { return bRideComplete; }

	UFUNCTION(BlueprintPure, Category = "Elevator")
	bool HasReturnedToFourthFloor() const;

	/**
	 * Puts the lift back to idle-with-doors-shut on 4F for a new chapter.
	 */
	UFUNCTION(BlueprintCallable, Category = "Elevator")
	void ResetForNewRide();

	/** Restores a safe downstairs checkpoint with the lobby doors open. */
	UFUNCTION(BlueprintCallable, Category = "Elevator|Save")
	void RestoreAtLobbyOpen();

	/**
	 * Enables an authored stop at 2F. The lower doors open only by the given
	 * total gap and then remain held until ReleaseIntermediateStop is called.
	 * Disabled by default, preserving the chapter-one direct ride.
	 */
	UFUNCTION(BlueprintCallable, Category = "Elevator|Intermediate Stop")
	void ConfigureIntermediateStop(
		bool bEnabled,
		float InDoorGapCentimeters = 12.0f);

	UFUNCTION(BlueprintPure, Category = "Elevator|Intermediate Stop")
	bool IsHoldingAtIntermediateStop() const;

	/** World-space centre of the authored 2F cab floor. */
	FVector GetIntermediateCabWorldLocation() const;

	/** Closes the narrow 2F gap and resumes the descent to the lobby. */
	UFUNCTION(BlueprintCallable, Category = "Elevator|Intermediate Stop")
	bool ReleaseIntermediateStop();

	UPROPERTY(BlueprintAssignable, Category = "Elevator|Events")
	FIGElevatorArrivedSignature OnArrivedAtLobby;

	/** Fires only after the return doors are fully open on the physical 4F landing. */
	UPROPERTY(BlueprintAssignable, Category = "Elevator|Events")
	FIGElevatorReturnedSignature OnReturnedToFourthFloor;

	/** Fires after the narrow 2F opening reaches its hold position. */
	UPROPERTY(BlueprintAssignable, Category = "Elevator|Events")
	FIGElevatorIntermediateStopSignature OnIntermediateStopOpened;

	virtual bool CanInteract_Implementation(AActor* Interactor) const override;
	virtual FText GetInteractionPrompt_Implementation(AActor* Interactor) const override;
	virtual void CompleteInteraction_Implementation(const FIGInteractionContext& Context) override;

protected:
	virtual void BeginPlay() override;

	UFUNCTION()
	void HandleCabBeginOverlap(
		UPrimitiveComponent* OverlappedComponent,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComponent,
		int32 OtherBodyIndex,
		bool bFromSweep,
		const FHitResult& SweepResult);

	UFUNCTION()
	void HandleCabEndOverlap(
		UPrimitiveComponent* OverlappedComponent,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComponent,
		int32 OtherBodyIndex);

	/** Seconds the doors take to open/close. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Elevator", meta = (ClampMin = "0.1", Units = "s"))
	float DoorSlideDuration = 1.1f;

	/** Seconds between the floor chimes during the descent. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Elevator", meta = (ClampMin = "0.2", Units = "s"))
	float SecondsPerFloor = 1.4f;

private:
	enum class EIGElevatorState : uint8
	{
		IdleClosed,
		OpeningUpper,
		WaitingForRider,
		ClosingUpper,
		Descending,
		OpeningIntermediate,
		HoldingIntermediate,
		ClosingIntermediate,
		OpeningLower,
		DoneAtLobby,
		WaitingForReturnRider,
		ClosingLowerForReturn,
		OpeningLowerForReturn,
		ClosingUpperForEmptyCall,
		EmptyDescending,
		ClosingLowerForEmptyCall,
		EmptyAscending,
		Ascending,
		OpeningUpperForReturn,
		ReturnedToFourthFloor
	};

	UStaticMeshComponent* MakePiece(
		USceneComponent* Parent,
		UStaticMesh* Mesh,
		UMaterialInterface* Material,
		const FVector& RelativeLocation,
		const FVector& SizeCentimeters,
		bool bCollide = true,
		const FRotator& Rotation = FRotator::ZeroRotator);
	void BuildCabShell(USceneComponent* Parent, float BaseZ);
	/** Interior fit-out of one cab: ceiling light box, COP, handrails, floor. */
	void BuildCabInterior(USceneComponent* Parent, float BaseZ);
	void SetState(EIGElevatorState NewState);
	void ApplyDoorOffsets(
		float UpperOffset,
		float IntermediateOffset,
		float LowerOffset);
	void HandleFloorChime();
	void PollForRider();
	void PollForReturnRider();
	void TryAdmitRider(APawn* Pawn);
	void TryAdmitReturnRider(APawn* Pawn);
	bool IsRiderSafelyInsideUpperCab(const APawn* Pawn) const;
	bool IsRiderSafelyInsideLowerCab(const APawn* Pawn) const;
	bool TransferRiderToCab(APawn* Rider, float CabBaseZ);
	void StartRide();
	void StartReturnRide();
	void BeginOpeningUpperAfterReturn();
	void StartEmptyCallToLobby();
	void StartEmptyCallToFourthFloor();
	void StartEmptyTravelToLobby();
	void StartEmptyTravelToFourthFloor();
	void BeginOpeningLowerAfterEmptyCall();
	void BeginOpeningUpperAfterEmptyCall();
	void PlayEmptyTravelHum(bool bAscending);
	float GetIntermediateCabBaseZ() const;

	UFUNCTION()
	void HandleLowerCabBeginOverlap(
		UPrimitiveComponent* OverlappedComponent,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComponent,
		int32 OtherBodyIndex,
		bool bFromSweep,
		const FHitResult& SweepResult);

	UFUNCTION()
	void HandleLowerCabEndOverlap(
		UPrimitiveComponent* OverlappedComponent,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComponent,
		int32 OtherBodyIndex);

	UPROPERTY(VisibleAnywhere, Category = "Elevator|Components")
	TObjectPtr<USceneComponent> ElevatorRoot;

	UPROPERTY(VisibleAnywhere, Category = "Elevator|Components")
	TObjectPtr<UStaticMeshComponent> CallButtonMesh;

	UPROPERTY(VisibleAnywhere, Category = "Elevator|Components")
	TObjectPtr<UStaticMeshComponent> LowerCallButtonMesh;

	UPROPERTY(VisibleAnywhere, Category = "Elevator|Components")
	TObjectPtr<UBoxComponent> UpperCabTrigger;

	UPROPERTY(VisibleAnywhere, Category = "Elevator|Components")
	TObjectPtr<UBoxComponent> LowerCabTrigger;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UStaticMeshComponent>> UpperDoorPanels;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UStaticMeshComponent>> IntermediateDoorPanels;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UStaticMeshComponent>> LowerDoorPanels;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UTextRenderComponent>> HallDisplays;
	void SetHallFloor(int32 Floor);
	void AdvanceHallFloor();
	int32 HallFloor = 4;
	int32 HallDirection = 0;
	FTimerHandle HallDisplayTimer;

	UPROPERTY(Transient)
	TObjectPtr<UStaticMesh> CachedCubeMesh;

	UPROPERTY(Transient)
	TObjectPtr<UStaticMesh> CachedCylinderMesh;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInterface> CachedCabMaterial;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInterface> CachedDoorMaterial;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInterface> CachedMirrorMaterial;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInterface> CachedFloorMaterial;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInterface> CachedInlayMaterial;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInterface> CachedCopMaterial;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInterface> CachedDiffuserMaterial;

	FIGDoorAnimation DoorAnimation;
	FTimerHandle RideTimerHandle;
	FTimerHandle ChimeTimerHandle;
	FTimerHandle AdmissionPollHandle;
	FTimerHandle ReturnAdmissionPollHandle;
	TWeakObjectPtr<APawn> PendingRider;
	TWeakObjectPtr<APawn> ActiveRider;
	EIGElevatorState State = EIGElevatorState::IdleClosed;
	float FloorDeltaZ = 900.0f;
	float DoorPanelWidth = 55.0f;
	float IntermediateDoorOffset = 6.0f;
	int32 ChimeFloor = 4;
	int32 PieceCounter = 0;
	bool bIntermediateStopEnabled = false;
	bool bUpperDoorDepartureStarted = false;
	bool bRideComplete = false;
	bool bVisualsConfigured = false;
};
