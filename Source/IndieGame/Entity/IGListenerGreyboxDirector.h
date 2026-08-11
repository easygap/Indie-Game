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

	// -- README/night capture tour (-IGNightCapture) -----------------------
	void StartNightCapture();
	void AdvanceNightCapture();
	void EnterCaptureStep(int32 StepIndex);
	void CaptureTeleportPlayer(const FVector& Location, float Yaw, float Pitch);
	void CaptureParkEntity(const FVector& Location, float Yaw);
	void CaptureShot(const TCHAR* BaseName) const;
	void CaptureBeginBurst(const TCHAR* DirectoryName, float Seconds);
	void HandleNightOneSolved();
	void HandleNightTwoSolved();
	void HandleNightThreeSolved();
	void HandleFifthDawnCompleted();
	void HandleNightFourResolved();
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

	UPROPERTY(Transient)
	TObjectPtr<class AIGMissingFloorNightThreeDirector> NightThree;

	UPROPERTY(Transient)
	TObjectPtr<class AIGMissingFloorFifthDawnDirector> FifthDawn;

	UPROPERTY(Transient)
	TObjectPtr<class AIGMissingFloorNightFourDirector> NightFour;

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
		AudioVisualContract,
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
		DayTwoContract,
		NightThreeContract,
		AnswerPairTap,
		AnswerFinalTap,
		AnswerContract,
		NightFourContract,
		NightFourWallContract,
		NightFourEndingContract,
		Done
	};

	EProbeStep ProbeStep = EProbeStep::Inactive;
	float StepDeadlineSeconds = 0.0f;
	float SetupRetrySeconds = 0.0f;

	/** Capture-tour state; inert unless -IGNightCapture is on the command line. */
	bool bNightCaptureRequested = false;
	int32 CaptureStepIndex = -1;
	float CaptureStepSeconds = 0.0f;
	bool bCaptureBurstActive = false;
	FString CaptureBurstDirectory;
	int32 CaptureBurstFrame = 0;
	float CaptureBurstEndsAt = 0.0f;
	float CaptureBurstAccumulator = 0.0f;
	/** One-shot latches for timed actions inside the current capture step. */
	bool bCaptureActionADone = false;
	bool bCaptureActionBDone = false;
	bool bCaptureActionCDone = false;
	FTimerHandle CaptureTimer;
	FVector ProbeNoiseLocation = FVector::ZeroVector;
	FVector ExpectedWakeLocation = FVector::ZeroVector;
	bool bStageReady = false;
	bool bProbeRequested = false;
	FTimerHandle SetupTimer;
	FTimerHandle ProbeTimer;
};
