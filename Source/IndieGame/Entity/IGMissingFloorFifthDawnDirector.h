#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TimerManager.h"
#include "IGMissingFloorFifthDawnDirector.generated.h"

class AIGPlayerCharacter;
class UAudioComponent;

DECLARE_MULTICAST_DELEGATE(FIGFifthDawnCompletedSignature);

/**
 * 「다섯 번째 새벽」 — a fixed 160-second sensory memory between nights 3/4.
 *
 * The camera is black and the ordinary HUD is suppressed, but this is not a
 * loading screen: Q/B still adds a player-owned knock and E/RT still leans
 * into the outside layer. Historical cues remain fixed, so input can never
 * rewrite Hwang's 7/29 reply or manufacture one on the final dawn.
 */
UCLASS(NotBlueprintable, Transient)
class INDIEGAME_API AIGMissingFloorFifthDawnDirector : public AActor
{
	GENERATED_BODY()

public:
	AIGMissingFloorFifthDawnDirector();

	/** Starts the canonical timeline and takes movement, not look/input. */
	bool StartInterlude(AIGPlayerCharacter* InPlayer);

	/** Player-owned experiential layer; neither method advances the history. */
	bool RegisterPlayerKnock();
	bool SetPlayerListening(bool bListening);

	bool IsActive() const { return bActive; }

	/** Release/probe receipt for all authored day boundaries and the final cut. */
	bool ValidateTimeline() const;
	/** Runs start/input/finish plumbing without waiting 160 seconds in CI. */
	bool CompleteImmediatelyForProbe();

	FIGFifthDawnCompletedSignature OnCompleted;

protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	void FireCue(int32 CueIndex);
	void ScheduleNextCue();
	void HandleNextCue();
	void FinishInterlude();
	void SetSensoryHud(bool bEnabled) const;
	void PushDirectionCaption(const FText& Caption, float Seconds) const;

	UPROPERTY(Transient)
	TObjectPtr<UAudioComponent> WaterBed;

	UPROPERTY(Transient)
	TObjectPtr<UAudioComponent> PrayerBed;

	UPROPERTY(Transient)
	TObjectPtr<UAudioComponent> BreathBed;

	TWeakObjectPtr<AIGPlayerCharacter> Player;
	FTimerHandle CueTimerHandle;
	double StartWorldSeconds = 0.0;
	float ElapsedSeconds = 0.0f;
	uint32 FiredCueMask = 0;
	int32 PlayerKnockCount = 0;
	int32 NextCueIndex = 1;
	bool bActive = false;
	bool bPlayerListening = false;
};
