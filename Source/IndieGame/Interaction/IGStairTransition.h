#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "IGStairTransition.generated.h"

class APawn;
class APlayerController;
class UBoxComponent;
class USceneComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
	FIGStairTransitionCompletedSignature,
	bool,
	bGoingDown);

/**
 * Bounded transition between two occluded switchback landings.
 *
 * The greybox villa does not stream three identical intermediate floors.
 * The player walks into a dark 180-degree turn, the screen closes for 0.12
 * seconds, and traversal resumes at the matching turn at the other end.
 */
UCLASS(NotBlueprintable, Transient)
class INDIEGAME_API AIGStairTransition final : public AActor
{
	GENERATED_BODY()

public:
	AIGStairTransition();

	void Configure(
		const FVector& InUpperTriggerLocation,
		const FVector& InUpperExitLocation,
		const FRotator& InUpperExitRotation,
		const FVector& InLowerTriggerLocation,
		const FVector& InLowerExitLocation,
		const FRotator& InLowerExitRotation);

	UPROPERTY(BlueprintAssignable, Category = "Stairs")
	FIGStairTransitionCompletedSignature OnTransitionCompleted;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	UFUNCTION()
	void HandleUpperOverlap(
		UPrimitiveComponent* OverlappedComponent,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComponent,
		int32 OtherBodyIndex,
		bool bFromSweep,
		const FHitResult& SweepResult);

	UFUNCTION()
	void HandleLowerOverlap(
		UPrimitiveComponent* OverlappedComponent,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComponent,
		int32 OtherBodyIndex,
		bool bFromSweep,
		const FHitResult& SweepResult);

	void BeginTransfer(AActor* OtherActor, bool bGoingDown);
	void CompleteTransfer();
	void ClearCooldown();

	UPROPERTY(VisibleAnywhere, Category = "Stairs")
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(VisibleAnywhere, Category = "Stairs")
	TObjectPtr<UBoxComponent> UpperPortal;

	UPROPERTY(VisibleAnywhere, Category = "Stairs")
	TObjectPtr<UBoxComponent> LowerPortal;

	TWeakObjectPtr<APawn> PendingPawn;
	TWeakObjectPtr<APlayerController> PendingController;
	FVector UpperExitLocation = FVector::ZeroVector;
	FVector LowerExitLocation = FVector::ZeroVector;
	FRotator UpperExitRotation = FRotator::ZeroRotator;
	FRotator LowerExitRotation = FRotator::ZeroRotator;
	bool bPendingGoingDown = false;
	bool bTransferPending = false;
	bool bCooldown = false;
	FTimerHandle TransferTimer;
	FTimerHandle CooldownTimer;
};
