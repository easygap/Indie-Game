#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GameplayTagContainer.h"
#include "IGWakeUpDirector.generated.h"

class AIGAlarmClock;
class UCameraComponent;

UENUM(BlueprintType)
enum class EIGWakeState : uint8
{
	NotStarted,
	FadeIn,
	BedLocked,
	AwaitAlarm,
	GettingUp,
	FreeRoam
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
	FIGWakeStateChangedSignature,
	EIGWakeState, PreviousState,
	EIGWakeState, NewState);

/**
 * Authoritative, idempotent wake-up flow. Blueprint events only present each state;
 * gameplay progression remains here even when a sequence is skipped or interrupted.
 */
UCLASS(Blueprintable)
class INDIEGAME_API AIGWakeUpDirector : public AActor
{
	GENERATED_BODY()

public:
	AIGWakeUpDirector();
	virtual void Tick(float DeltaSeconds) override;

	/** Connects a runtime-spawned alarm and safely updates delegate bindings. */
	UFUNCTION(BlueprintCallable, Category = "Wake Flow")
	void SetAlarmClock(AIGAlarmClock* InAlarmClock);

	UFUNCTION(BlueprintCallable, Category = "Wake Flow")
	bool StartWakeUp();

	UFUNCTION(BlueprintCallable, Category = "Wake Flow")
	bool NotifyEyesOpened();

	UFUNCTION(BlueprintCallable, Category = "Wake Flow")
	bool NotifyAlarmStopped();

	UFUNCTION(BlueprintCallable, Category = "Wake Flow")
	bool RequestGetUp();

	/**
	 * Blind-reach fallback: pressing Interact with nothing focused while the
	 * alarm still rings silences it, as a hand slapping toward the nightstand.
	 */
	UFUNCTION(BlueprintCallable, Category = "Wake Flow")
	bool RequestStopAlarmFallback();

	/** Sequence completion and skip paths must both call this function. */
	UFUNCTION(BlueprintCallable, Category = "Wake Flow")
	bool CompleteGettingUp();

	UFUNCTION(BlueprintCallable, Category = "Wake Flow")
	void RestoreStandingCheckpoint();

	UFUNCTION(BlueprintPure, Category = "Wake Flow")
	EIGWakeState GetWakeState() const { return WakeState; }

	UFUNCTION(BlueprintPure, Category = "Wake Flow")
	bool IsFreeRoam() const { return WakeState == EIGWakeState::FreeRoam; }

	/**
	 * Puts the director back to NotStarted so a later chapter can run the
	 * waking beat again.
	 *
	 * The prologue was written as if the player wakes up exactly once, so
	 * StartWakeUp() early-returns forever after the first free-roam. Chapter
	 * two is the same morning happening again, which makes waking up a
	 * repeatable event rather than a one-shot piece of intro.
	 */
	UFUNCTION(BlueprintCallable, Category = "Wake")
	void ResetForNewChapter();

	/**
	 * Rebinds wake progression and its standing checkpoint for a chapter.
	 * Invalid tags leave their current values unchanged. The autosave flag is
	 * always applied, allowing repeated mornings to opt out of CH01 saves.
	 */
	UFUNCTION(BlueprintCallable, Category = "Wake Flow|Save")
	void ConfigureChapterSaveTags(
		FGameplayTag InChapterId,
		FGameplayTag InAlarmStoppedStateTag,
		FGameplayTag InStandingStateTag,
		FGameplayTag InStandingCheckpointTag,
		bool bInAutosaveWhenStanding);

	UPROPERTY(BlueprintAssignable, Category = "Wake Flow")
	FIGWakeStateChangedSignature OnWakeStateChanged;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UFUNCTION(BlueprintImplementableEvent, Category = "Wake Flow|Presentation")
	void PresentWakeStarted();

	UFUNCTION(BlueprintImplementableEvent, Category = "Wake Flow|Presentation")
	void PresentEyesOpened();

	UFUNCTION(BlueprintImplementableEvent, Category = "Wake Flow|Presentation")
	void PresentAlarmStopped();

	UFUNCTION(BlueprintImplementableEvent, Category = "Wake Flow|Presentation")
	void PresentGettingUp();

	UFUNCTION(BlueprintImplementableEvent, Category = "Wake Flow|Presentation")
	void PresentFreeRoamRestored();

	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Wake Flow")
	TObjectPtr<AIGAlarmClock> AlarmClock;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Wake Flow")
	bool bAutoStart = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Wake Flow", meta = (ClampMin = "0.0", Units = "s"))
	float FadeInDuration = 2.0f;

	/** Fail-safe used when the presentation sequence never reports completion. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Wake Flow", meta = (ClampMin = "0.0", Units = "s"))
	float GettingUpFallbackDuration = 2.0f;

	/** Native camera motion keeps the C++ prototype complete without a Level Sequence. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Wake Flow|Presentation")
	bool bUseNativeCameraPresentation = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Wake Flow|Presentation")
	FVector LyingCameraLocation = FVector(0.0f, 0.0f, 28.0f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Wake Flow|Presentation")
	FRotator LyingCameraRotation = FRotator(-8.0f, 0.0f, -82.0f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Wake Flow|Save")
	bool bAutosaveWhenStanding = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Wake Flow|Save")
	FGameplayTag ChapterId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Wake Flow|Save")
	FGameplayTag AlarmStoppedStateTag;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Wake Flow|Save")
	FGameplayTag StandingStateTag;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Wake Flow|Save")
	FGameplayTag StandingCheckpointTag;

private:
	UFUNCTION()
	void HandleAlarmClockStopped(AIGAlarmClock* StoppedAlarm);

	UFUNCTION()
	void HandleStoryStateChanged(FGameplayTag StateTag, bool bAdded);

	void HandleFadeTimerElapsed();
	void HandleGettingUpFallback();
	bool TransitionTo(EIGWakeState NewState);
	void ResolveDefaultTags();
	void SetMovementLocked(bool bLocked);
	void SetLookLocked(bool bLocked);
	void SetInteractionLocked(bool bLocked);
	void AddStoryState(FGameplayTag StateTag) const;
	bool HasStoryState(FGameplayTag StateTag) const;
	void SaveStandingCheckpoint();
	UCameraComponent* ResolvePlayerCamera();
	void ApplyLyingCameraPose();
	void BeginNativeGetUpPresentation();
	void RestoreStandingCameraPose();

	UPROPERTY(VisibleInstanceOnly, Category = "Wake Flow")
	EIGWakeState WakeState = EIGWakeState::NotStarted;

	FTimerHandle TransitionTimerHandle;
	bool bAlarmStopped = false;
	bool bAlarmStopPending = false;
	bool bMovementLocked = false;
	bool bLookInputLocked = false;
	bool bInteractionLocked = false;
	bool bNativeGetUpActive = false;
	float NativeGetUpElapsed = 0.0f;
	FTransform StandingCameraTransform;
	FTransform GetUpStartCameraTransform;
	float GetUpStartControlRoll = 0.0f;
	float GetUpStartControlPitch = 0.0f;
	TWeakObjectPtr<UCameraComponent> CachedPlayerCamera;
};
