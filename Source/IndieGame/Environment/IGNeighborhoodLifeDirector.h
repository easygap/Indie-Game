#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "IGNeighborhoodLifeDirector.generated.h"

class UAudioComponent;
class UMaterialInterface;
class USceneComponent;
class UStaticMesh;
class UStaticMeshComponent;

/**
 * Environmental grammar used by the first two mornings.
 *
 * CH01 establishes ordinary pre-dawn traffic and animal activity. CH02 can
 * either make the same systems subtly wrong, or remove them altogether.  The
 * enum is deliberately independent of story tags so later chapters can reuse
 * the director without depending on the prologue state machine.
 */
UENUM(BlueprintType)
enum class EIGNeighborhoodChapterVariant : uint8
{
	ChapterOneNormal UMETA(DisplayName = "CH01 - Normal"),
	ChapterTwoUncanny UMETA(DisplayName = "CH02 - Uncanny"),
	ChapterTwoAbsent UMETA(DisplayName = "CH02 - Absent")
};

/** One coarse gust signal for signs, wires, foliage and chapter scripting. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(
	FIGNeighborhoodWindGustSignature,
	float, PeakStrength,
	FVector, Direction,
	float, DurationSeconds);

/**
 * Allocation-bounded life for a small Korean residential alley.
 *
 * The actor intentionally needs no imported vehicle, animal, VFX or audio
 * assets. It builds a tiny pool from Engine basic shapes and renders all
 * sounds with UIGToneSequenceSoundWave. Runtime work is event driven: the
 * actor ticks only for an active pass, gust, leaf or cat trace, and scheduled
 * callbacks skip when the player is too far away.
 */
UCLASS(BlueprintType, NotBlueprintable, Transient)
class INDIEGAME_API AIGNeighborhoodLifeDirector final : public AActor
{
	GENERATED_BODY()

public:
	AIGNeighborhoodLifeDirector();

	/**
	 * Defines the road centre line and restarts the deterministic schedule.
	 * Call immediately after spawning. Coordinates are in world space.
	 */
	UFUNCTION(BlueprintCallable, Category = "Indie Game|Neighborhood")
	void ConfigureNeighborhood(
		FVector InRoadStart,
		FVector InRoadEnd,
		int32 InDeterministicSeed = 4040444);

	/** Switches normal/uncanny/absent grammar without reallocating the pools. */
	UFUNCTION(BlueprintCallable, Category = "Indie Game|Neighborhood")
	void SetChapterVariant(EIGNeighborhoodChapterVariant InVariant);

	/**
	 * Starts the authored alley sequence when the player reaches street level.
	 * CH01 then guarantees a car followed by a delivery motorcycle instead of
	 * spending those deterministic events unheard while the player is on 4F.
	 */
	UFUNCTION(BlueprintCallable, Category = "Indie Game|Neighborhood")
	void PrimeOutdoorSequence();

	/**
	 * Stops ambient scheduling and plays the canonical third gust with a cat
	 * trace from the villa forecourt into the common entrance.
	 */
	UFUNCTION(BlueprintCallable, Category = "Indie Game|Neighborhood")
	void PlayAuthoredReturnIncident(FVector CatStart, FVector CatEnd);

	/** Emits one directed cat call without spawning or moving the cat trace. */
	UFUNCTION(BlueprintCallable, Category = "Indie Game|Neighborhood")
	void PlayAuthoredCatCall(FVector WorldLocation, bool bUncanny = false);

	UFUNCTION(BlueprintPure, Category = "Indie Game|Neighborhood")
	EIGNeighborhoodChapterVariant GetChapterVariant() const { return ChapterVariant; }

	/**
	 * Optional wind response for a movable sign or cable assembly.
	 * The component returns to the captured neutral rotation after every gust.
	 */
	UFUNCTION(BlueprintCallable, Category = "Indie Game|Neighborhood")
	void RegisterWindReactiveComponent(USceneComponent* Component, float ResponseScale = 1.0f);

	UFUNCTION(BlueprintCallable, Category = "Indie Game|Neighborhood")
	void UnregisterWindReactiveComponent(USceneComponent* Component);

	/** Current signed wind vector; magnitude is normalized to roughly 0..1. */
	UFUNCTION(BlueprintPure, Category = "Indie Game|Neighborhood")
	FVector GetCurrentWindSignal() const { return CurrentWindSignal; }

	UPROPERTY(BlueprintAssignable, Category = "Indie Game|Neighborhood")
	FIGNeighborhoodWindGustSignature OnWindGust;

protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	enum class EIGPooledVehicleKind : uint8
	{
		PassengerCar,
		DeliveryMotorcycle
	};

	struct FVehicleRuntime
	{
		bool bActive = false;
		bool bAudioDropsOut = false;
		EIGPooledVehicleKind Kind = EIGPooledVehicleKind::PassengerCar;
		FVector Start = FVector::ZeroVector;
		FVector End = FVector::ZeroVector;
		FVector Velocity = FVector::ZeroVector;
		float Elapsed = 0.0f;
		float Duration = 1.0f;
		float BasePitch = 1.0f;
		float BaseVolume = 1.0f;
	};

	struct FLeafRuntime
	{
		bool bActive = false;
		FVector Position = FVector::ZeroVector;
		FVector Velocity = FVector::ZeroVector;
		float Age = 0.0f;
		float Lifetime = 1.0f;
		float Phase = 0.0f;
	};

	struct FWindReactorRuntime
	{
		TWeakObjectPtr<USceneComponent> Component;
		FRotator NeutralRotation = FRotator::ZeroRotator;
		float ResponseScale = 1.0f;
	};

	struct FCatTraceRuntime
	{
		bool bActive = false;
		FVector Start = FVector::ZeroVector;
		FVector End = FVector::ZeroVector;
		float Elapsed = 0.0f;
		float Duration = 1.0f;
	};

	void InitializePools();
	void RestartDeterministicSchedule();
	void StopAllRuntimeEvents();
	void UpdateRuntime(float DeltaSeconds);
	void RefreshRuntimeUpdates();

	void ScheduleNextVehicle();
	void ScheduleNextGust();
	void ScheduleNextCatTrace();

	void LaunchVehicleEvent();
	void LaunchWindGust();
	void LaunchCatTrace();

	void UpdateVehicles(float DeltaSeconds, const FVector& ListenerLocation);
	void UpdateGust(float DeltaSeconds);
	void UpdateLeaves(float DeltaSeconds, const FVector& ListenerLocation);
	void UpdateCatTrace(float DeltaSeconds);
	void UpdateWindReactors();

	void ActivateLeaves(const FVector& Origin, int32 Count, float ImpulseScale);
	void DeactivateVehicle(int32 SlotIndex);
	void DeactivateLeaf(int32 LeafIndex);
	void DeactivateCatTrace();

	UAudioComponent* CreateSpatialAudioComponent(
		USceneComponent* Parent,
		FName ComponentName,
		float InnerRadius,
		float FalloffDistance);
	class UIGToneSequenceSoundWave* CreateVehicleLoop(
		UObject* Outer,
		EIGPooledVehicleKind Kind) const;
	class UIGToneSequenceSoundWave* CreateGustSound(UObject* Outer, float Duration) const;
	class UIGToneSequenceSoundWave* CreateCatCall(UObject* Outer, bool bUncanny) const;

	bool TryGetListenerLocation(FVector& OutLocation) const;
	bool IsPlayerNearRoad(float MaxDistance) const;
	float RandomRangeForVariant(float NormalMin, float NormalMax, float UncannyMin, float UncannyMax) ;

	UPROPERTY(VisibleAnywhere, Category = "Indie Game|Neighborhood")
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(EditAnywhere, Category = "Indie Game|Neighborhood|Scheduling")
	EIGNeighborhoodChapterVariant ChapterVariant =
		EIGNeighborhoodChapterVariant::ChapterOneNormal;

	UPROPERTY(EditAnywhere, Category = "Indie Game|Neighborhood|Scheduling", meta = (ClampMin = "1"))
	int32 DeterministicSeed = 4040444;

	UPROPERTY(EditAnywhere, Category = "Indie Game|Neighborhood|Performance", meta = (ClampMin = "500.0"))
	float EventActivationDistance = 900.0f;

	UPROPERTY(EditAnywhere, Category = "Indie Game|Neighborhood|Performance", meta = (ClampMin = "500.0"))
	float LeafSimulationDistance = 1900.0f;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UStaticMeshComponent>> VehicleBodies;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UStaticMeshComponent>> VehicleCabins;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UStaticMeshComponent>> VehicleCargoBoxes;

	/** Rider helmet per slot; visible only on delivery motorcycles. */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UStaticMeshComponent>> VehicleRiderHeads;

	/** Four wheel components per slot; motorcycles use indices 0 and 1. */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UStaticMeshComponent>> VehicleWheels;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UAudioComponent>> VehicleAudio;

	/** Bounded one-shots that must be stopped when chapters change. */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UAudioComponent>> TransientAudio;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UStaticMeshComponent>> LeafMeshes;

	UPROPERTY(Transient)
	TObjectPtr<USceneComponent> CatTraceRoot;

	/** Six pooled primitives: body, head, tail and three leg streaks. */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UStaticMeshComponent>> CatSilhouetteParts;

	UPROPERTY(Transient)
	TObjectPtr<UStaticMesh> CubeMesh;

	UPROPERTY(Transient)
	TObjectPtr<UStaticMesh> SphereMesh;

	UPROPERTY(Transient)
	TObjectPtr<UStaticMesh> CylinderMesh;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInterface> DarkMaterial;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInterface> MetalMaterial;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInterface> LeafMaterial;

	TArray<FVehicleRuntime> VehicleRuntime;
	TArray<FLeafRuntime> LeafRuntime;
	TArray<FWindReactorRuntime> WindReactors;
	FCatTraceRuntime CatRuntime;
	FRandomStream Random;

	// The procedural vehicles are authored with their wheel bottoms 5 cm
	// above the path origin, so -5 seats them on the alley's Z=0 asphalt.
	FVector RoadStart = FVector(-360.0f, -555.0f, -5.0f);
	FVector RoadEnd = FVector(2390.0f, -555.0f, -5.0f);
	FVector CurrentWindSignal = FVector::ZeroVector;
	FVector GustDirection = FVector::ForwardVector;
	float GustElapsed = -1.0f;
	float GustDuration = 0.0f;
	float GustPeakStrength = 0.0f;
	float GustVisualPhase = 0.0f;
	int32 ActiveVehicleOrdinal = 0;
	bool bPoolsInitialized = false;
	bool bOutdoorSequencePrimed = false;

	FTimerHandle VehicleScheduleHandle;
	FTimerHandle GustScheduleHandle;
	FTimerHandle CatScheduleHandle;
};
