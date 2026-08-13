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

	// -- §11 V5 밤 구간 히스토그램 (-IGNightHistogram) ----------------------
	//
	// Eight authored night viewpoints, each measured for the fraction of pixels
	// that fall below 5% luminance and above 98%. The design fixes those bands
	// per point because the whole V1 lighting rework is a claim about contrast:
	// darks that fall genuinely black, and no blown highlights anywhere. A
	// screenshot proves neither. Counting pixels does.
	//
	// This is a pre-filter, not the art approval. Ratios inside their band mean
	// the frame is not broken; whether it is *good* still needs eyes.
	void StartHistogramSweep();
	void AdvanceHistogramSweep();
	void EnterHistogramPoint(int32 PointIndex);
	/**
	 * Measures the frame the screenshot pipeline actually captured.
	 *
	 * Reading the viewport back buffer directly returns pure black under
	 * -RenderOffScreen: there is no swap chain to read from, which is exactly
	 * why no window appears. The screenshot delegate hands over the rendered
	 * frame instead, and it is the same path that already writes the capture
	 * tour's PNGs, so it is proven to work headless.
	 */
	void HandleHistogramScreenshot(
		int32 Width,
		int32 Height,
		const TArray<FColor>& Colors);
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

	/** §8 비트 2-1. Owns the door knock, the peephole and the dragging away. */
	UPROPERTY(Transient)
	TObjectPtr<class AIGMissingFloorNightTwoBeatDirector> NightTwoBeats;

	UPROPERTY(Transient)
	TObjectPtr<class AIGMissingFloorPuzzleTwoDirector> PuzzleTwo;

	UPROPERTY(Transient)
	TObjectPtr<class AIGMissingFloorNightThreeDirector> NightThree;

	UPROPERTY(Transient)
	TObjectPtr<class AIGMissingFloorFifthDawnDirector> FifthDawn;

	UPROPERTY(Transient)
	TObjectPtr<class AIGMissingFloorNightFourDirector> NightFour;

	/** §20.3's two automatic safety nets: the world moving when nothing else is. */
	UPROPERTY(Transient)
	TObjectPtr<class AIGMissingFloorMercyDirector> Mercy;

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
		/** §20.2 tuning table, §20.4 difficulty modes, §5.6 noise heatmap. */
		DifficultyContract,
		/** §10.4 reverb grammar and the §11 V1 airborne-dust world model. */
		PerceptionContract,
		/** The torch beam actually populating and thickening over his lane. */
		BeamDustContract,
		PuzzleOneContract,
		SealContract,
		MaskingContract,
		InvestigateOnFirstSound,
		ChaseOnSecondSound,
		CaptureOnTouch,
		ResetAfterCapture,
		/** §20.3's two automatic safety nets: the reset hint and the 90 s clock. */
		MercyNetContract,
		/** §5.5 기록되지 않는 시간: the recording rule and its one exception. */
		RecordingRuleContract,
		Night1SightingStage,
		Night1SightingRestore,
		Night1Extinguisher,
		DayNightCycle,
		/** §8 비트 2-1: the knock at 403's own door, and what it puts on tape. */
		NightTwoDoorBeatContract,
		PuzzleTwoContract,
		/** §14 CCTV 채널 5: the one-shot render target's whole life cycle. */
		CctvChannelContract,
		DayTwoContract,
		NightThreeContract,
		AnswerPairTap,
		AnswerFinalTap,
		AnswerContract,
		NightFourContract,
		NightFourFailureRetryContract,
		NightFourWallContract,
		NightFourEndingContract,
		Done
	};

	EProbeStep ProbeStep = EProbeStep::Inactive;
	float StepDeadlineSeconds = 0.0f;
	float SetupRetrySeconds = 0.0f;
	int32 FailureRetryCaptureCountBefore = 0;
	bool bNightFourFailureRetryVerified = false;
	/** Latch so the mercy step fires its nets once and then waits for the paper. */
	bool bMercyNetsFired = false;

	/**
	 * §14 CCTV 채널 5. The structural half of the contract runs anywhere: the
	 * channel must own nothing before the press, allocate one CIF target for the
	 * beat, cross the low shape through it, and release everything when it dies.
	 * The pixel half needs a real RHI and is measured only when asked for, because
	 * a scene capture under NullRHI returns black and black passes any floor —
	 * which is exactly how the §11 V5 sweep first fooled itself.
	 */
	int32 CctvCapturesAtLive = 0;
	int32 CctvCapturesAtDeath = 0;
	bool bCctvShapeSeen = false;
	bool bCctvFeedProbeRequested = false;
	bool bCctvFeedMeasured = false;
	float CctvFeedBrightestLuma = 0.0f;
	float CctvFeedLitFraction = 0.0f;
	void MeasureCctvFeed(const class AIGCctvChannelFive* Channel);

	/** Capture-tour state; inert unless -IGNightCapture is on the command line. */
	bool bNightCaptureRequested = false;
	/** §11 V5 sweep; inert unless -IGNightHistogram is on the command line. */
	bool bHistogramRequested = false;
	/** Reports measurements without enforcing bands, for authoring them. */
	bool bHistogramReportOnly = false;
	int32 HistogramPointIndex = -1;
	float HistogramPointSeconds = 0.0f;
	int32 HistogramFailures = 0;
	int32 HistogramMeasured = 0;
	/** Set while a point's screenshot is in flight, so the timer stands down. */
	bool bHistogramShotPending = false;
	float HistogramShotWaitSeconds = 0.0f;
	FDelegateHandle HistogramScreenshotHandle;
	FTimerHandle HistogramTimer;
	bool bMercyNoteProbeRequested = false;
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
	/** Trail sample the beam-dust step lit up, so it can be cleaned up after. */
	FVector ProbeDustTrailLocation = FVector::ZeroVector;
	/** Measured P3 wall discrimination, for the PASS receipt. */
	float ProbeHollowRingLevel = 0.0f;
	float ProbeHollowRingSeconds = 0.0f;
	float ProbeSolidRingSeconds = 0.0f;
	bool bStageReady = false;
	bool bProbeRequested = false;
	FTimerHandle SetupTimer;
	FTimerHandle ProbeTimer;
};
