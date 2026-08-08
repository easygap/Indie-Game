#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "IGListenerGreyboxDirector.generated.h"

class AIGListenerEntity;
class AIGNightLoopDirector;
class AIGNightPhaseDirector;
class AIGPlayerCharacter;
class AIGPrologueWorldScene;
class UIGNoiseSubsystem;

/**
 * M1 vertical-slice stage for The Missing Floor (-IGListenerGreybox):
 * places the one upstairs in the real 4F corridor of the prologue villa,
 * strings its patrol along the corridor light fixtures, registers the first
 * hum-mask sources and pairs a night-loop director with the player's actual
 * wake pose. No legacy chapter content is altered; without the flag nothing
 * here exists.
 *
 * With -IGListenerGreyboxProbe it also runs the M1 smoke contract from
 * STORY_BIBLE_MISSING_FLOOR.md §4.5/§15: masking swallows sound, one sound
 * investigates, a second sound chases, touch captures, capture resets the
 * night and raises the aggression tier. Logs MISSINGFLOOR_GREYBOX PASS/FAIL
 * and exits with 0/1 for Scripts\Run-MissingFloor-Greybox.bat.
 */
UCLASS()
class INDIEGAME_API AIGListenerGreyboxDirector : public AActor
{
	GENERATED_BODY()

public:
	AIGListenerGreyboxDirector();

protected:
	virtual void BeginPlay() override;

private:
	void TrySetupStage();
	bool SetupStage();
	void StartProbe();
	void AdvanceProbe();
	void FailProbe(const FString& Reason);
	void PassProbe();
	void RequestExit(bool bFailed);

	/** Emits a synthetic sound near the entity, as the probe's stand-in ear bait. */
	void EmitProbeNoise();

	class UIGMissingFloorNarrativeSubsystem* GetNarrative() const;

	/** Central hour-boundary wiring: dormancy, booth lock, day verbs. */
	void HandleHourActiveChanged(bool bActive);
	void HandleNightOneSolved();
	void HandleNightTwoSolved();
	void HandleSleepRequested(class AIGMissingFloorEvidence* Evidence);
	void HandleUnit401Knocked(class AIGMissingFloorEvidence* Evidence);

	UPROPERTY(Transient)
	TObjectPtr<AIGListenerEntity> Entity;

	UPROPERTY(Transient)
	TObjectPtr<AIGNightLoopDirector> NightLoop;

	UPROPERTY(Transient)
	TObjectPtr<AIGNightPhaseDirector> NightPhase;

	UPROPERTY(Transient)
	TObjectPtr<class AIGMissingFloorPuzzleOneDirector> PuzzleOne;

	UPROPERTY(Transient)
	TObjectPtr<class AIGNightOneBeatDirector> NightOneBeats;

	UPROPERTY(Transient)
	TObjectPtr<class AIGMissingFloorPuzzleTwoDirector> PuzzleTwo;

	/** Day interactions: the bed that ends the day, 401's door that talks. */
	UPROPERTY(Transient)
	TObjectPtr<class AIGMissingFloorEvidence> SleepTarget;

	UPROPERTY(Transient)
	TObjectPtr<class AIGMissingFloorEvidence> Unit401Door;

	UPROPERTY(Transient)
	TWeakObjectPtr<AIGPlayerCharacter> Player;

	UPROPERTY(Transient)
	TWeakObjectPtr<AIGPrologueWorldScene> WorldScene;

	UPROPERTY(Transient)
	TObjectPtr<UIGNoiseSubsystem> NoiseSubsystem;

	enum class EProbeStep : uint8
	{
		Inactive,
		PuzzleOneContract,
		SealContract,
		MaskingContract,
		InvestigateOnFirstSound,
		ChaseOnSecondSound,
		CaptureOnTouch,
		ResetAfterCapture,
		Night1SightingStage,
		Night1SightingRestore,
		Night1Extinguisher,
		DayNightCycle,
		PuzzleTwoContract,
		Done
	};

	EProbeStep ProbeStep = EProbeStep::Inactive;
	float StepDeadlineSeconds = 0.0f;
	float SetupRetrySeconds = 0.0f;
	FVector ProbeNoiseLocation = FVector::ZeroVector;
	FVector ExpectedWakeLocation = FVector::ZeroVector;
	bool bStageReady = false;
	bool bProbeRequested = false;
	FTimerHandle SetupTimer;
	FTimerHandle ProbeTimer;
};
