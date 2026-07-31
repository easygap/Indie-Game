#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "IGDemoDirector.generated.h"

class AIGAlarmClock;
class AIGCheckoutCounter;
class AIGElevator;
class AIGFridge;
class AIGPickupItem;
class AIGSwingDoor;
class AIGWakeUpDirector;

/**
 * Drives the whole morning routine hands-free with natural first-person
 * motion: the pawn genuinely walks the route (head-bob, footsteps, doors,
 * zone triggers and story beats all fire exactly as they do for a player).
 *
 * Used for verification and promotional capture:
 *  -IGCapture     walk the story and save documentation stills at key beats
 *  -IGDemoFrames  additionally dump a frame sequence for video assembly
 */
UCLASS(NotBlueprintable, Transient)
class INDIEGAME_API AIGDemoDirector : public AActor
{
	GENERATED_BODY()

public:
	AIGDemoDirector();
	virtual void Tick(float DeltaSeconds) override;

	void ConfigureDemo(
		AIGWakeUpDirector* InWakeDirector,
		AIGAlarmClock* InAlarmClock,
		AIGFridge* InFridge,
		AIGPickupItem* InWallet,
		AIGSwingDoor* InHomeDoor,
		AIGElevator* InElevator,
		AIGSwingDoor* InBuildingDoor,
		AIGPickupItem* InWaterBottle,
		AIGCheckoutCounter* InCheckout,
		bool bInCaptureStills,
		bool bInDumpFrames,
		bool bInExitWhenDone);

protected:
	virtual void BeginPlay() override;

private:
	enum class EIGDemoStepType : uint8
	{
		Wait,
		WalkTo,
		Interact,
		StopAlarm,
		RequestGetUp,
		Still,
		Exit
	};

	struct FIGDemoStep
	{
		EIGDemoStepType Type = EIGDemoStepType::Wait;
		float Duration = 1.0f;
		FVector Target = FVector::ZeroVector;
		FVector LookAt = FVector::ZeroVector;
		float LookAtPawnZOffset = 0.0f;
		bool bHasLookAt = false;
		bool bTrackPawnHeight = false;
		TWeakObjectPtr<AActor> Actor;
		FString StillName;
	};

	void BuildScript();
	void AdvanceStep();
	void UpdateLook(float DeltaSeconds);
	void UpdateWalk(float DeltaSeconds);
	void ExecuteInteraction(AActor* Target);
	void RequestStill(const FString& BaseName) const;
	void DumpFrameIfDue(float DeltaSeconds);
	APawn* GetDemoPawn() const;

	FIGDemoStep MakeWait(float Duration, const FVector& LookAt, bool bLook = true);
	FIGDemoStep MakeRideWait(
		float Duration,
		const FVector& LookAt,
		float ReferencePawnZ);
	FIGDemoStep MakeWalk(const FVector& Target);
	FIGDemoStep MakeWalkLook(const FVector& Target, const FVector& LookAt);
	FIGDemoStep MakeInteract(AActor* Actor, float PostDelay, const FVector& LookAt);
	FIGDemoStep MakeStill(const TCHAR* Name);

	TWeakObjectPtr<AIGWakeUpDirector> WakeDirector;
	TWeakObjectPtr<AIGAlarmClock> AlarmClock;
	TWeakObjectPtr<AIGFridge> Fridge;
	TWeakObjectPtr<AIGPickupItem> Wallet;
	TWeakObjectPtr<AIGSwingDoor> HomeDoor;
	TWeakObjectPtr<AIGElevator> Elevator;
	TWeakObjectPtr<AIGSwingDoor> BuildingDoor;
	TWeakObjectPtr<AIGPickupItem> WaterBottle;
	TWeakObjectPtr<AIGCheckoutCounter> Checkout;

	TArray<FIGDemoStep> Steps;
	int32 StepIndex = INDEX_NONE;
	float StepElapsed = 0.0f;
	float WalkTimeout = 0.0f;
	float FrameAccumulator = 0.0f;
	int32 FrameCounter = 0;
	FVector CurrentLookAt = FVector::ZeroVector;
	bool bHasCurrentLookAt = false;
	bool bCaptureStills = false;
	bool bDumpFrames = false;
	bool bExitWhenDone = false;
	bool bScriptFinished = false;

	/** Seconds between dumped frames (~7 fps keeps encode light). */
	static constexpr float FrameInterval = 0.14f;
	static constexpr float ArriveDistance = 55.0f;
	static constexpr float MaxWalkSeconds = 8.0f;
};
