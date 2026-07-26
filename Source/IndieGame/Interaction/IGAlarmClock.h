#pragma once

#include "CoreMinimal.h"
#include "Interaction/IGInteractableActor.h"
#include "IGAlarmClock.generated.h"

class AIGAlarmClock;
class UAudioComponent;
class USceneComponent;
class UStaticMeshComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
	FIGAlarmStoppedSignature,
	AIGAlarmClock*, AlarmClock);

/** Interactable alarm that permanently completes the first time it is stopped. */
UCLASS(Blueprintable)
class INDIEGAME_API AIGAlarmClock : public AIGInteractableActor
{
	GENERATED_BODY()

public:
	AIGAlarmClock();

	/** Starts the assigned sound. Configure the Sound Wave/Cue/MetaSound to loop. */
	UFUNCTION(BlueprintCallable, Category = "Alarm")
	void StartAlarm();

	/** Stops and completes the alarm. Returns false after the first successful stop. */
	UFUNCTION(BlueprintCallable, Category = "Alarm")
	bool StopAlarm();

	/** Restores a completed checkpoint without emitting live gameplay events. */
	UFUNCTION(BlueprintCallable, Category = "Alarm")
	void ApplyRestoredStoppedState();

	UFUNCTION(BlueprintPure, Category = "Alarm")
	bool IsRinging() const { return bIsRinging; }

	UFUNCTION(BlueprintPure, Category = "Alarm")
	bool HasCompleted() const { return bHasCompleted; }

	/**
	 * Clears the completed flag so the clock can ring again tomorrow.
	 *
	 * StopAlarm() latches bHasCompleted for the rest of the session, which is
	 * right for a single morning and wrong for a game whose whole premise is
	 * that the same 4:44 keeps arriving.
	 */
	UFUNCTION(BlueprintCallable, Category = "Alarm")
	void Rearm();

	UFUNCTION(BlueprintPure, Category = "Alarm")
	UAudioComponent* GetAlarmAudioComponent() const { return AlarmAudioComponent; }

	UPROPERTY(BlueprintAssignable, Category = "Alarm|Events")
	FIGAlarmStoppedSignature OnAlarmStopped;

	virtual bool CanInteract_Implementation(AActor* Interactor) const override;
	virtual void CompleteInteraction_Implementation(const FIGInteractionContext& Context) override;

protected:
	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Alarm|Components")
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Alarm|Components")
	TObjectPtr<UStaticMeshComponent> ClockMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Alarm|Components")
	TObjectPtr<UAudioComponent> AlarmAudioComponent;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Alarm")
	bool bStartRingingOnBeginPlay = true;

private:
	UPROPERTY(VisibleInstanceOnly, Category = "Alarm")
	bool bIsRinging = false;

	UPROPERTY(VisibleInstanceOnly, Category = "Alarm")
	bool bHasCompleted = false;
};
