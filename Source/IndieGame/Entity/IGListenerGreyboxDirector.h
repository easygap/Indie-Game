#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Sequence/IGObjectiveProvider.h"
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
class INDIEGAME_API AIGListenerGreyboxDirector
	: public AActor
	, public IIGObjectiveProvider
{
	GENERATED_BODY()

public:
	AIGListenerGreyboxDirector();

	virtual FText GetObjectiveText() const override;
	virtual FString GetObjectiveTextAscii() const override;
	virtual float GetObjectiveProgress() const override;

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
	void RunArrivalProbe();
	void StartArrivalCapture();
	void AdvanceArrivalCapture();

	/** Emits a synthetic sound near the entity, as the probe's stand-in ear bait. */
	void EmitProbeNoise();

	class UIGMissingFloorNarrativeSubsystem* GetNarrative() const;
	void InitializeArrivalSequence();
	void SpawnArrivalInteractables(UStaticMesh* CubeMesh);
	void SpawnOptionalWitnesses(UStaticMesh* CubeMesh);
	/** 편의점 카운터의 나린. 아는 것이 늘면 하는 말이 달라진다. */
	FText GetNarinCounterLine() const;
	UFUNCTION()
	void HandleUsedListingRead(class AIGReadableNote* Note, bool bOpened);
	void UpdateArrivalSequence();
	void HandleArrivalEvidence(class AIGMissingFloorEvidence* Evidence);
	void RequestArrivalAutosave();
	bool AreArrivalBoxesOpened() const;

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
	/**
	 * Writes one point's PNG from the bitmap that was just measured.
	 *
	 * The viewport client saves a screenshot itself only when nothing is bound to
	 * OnScreenshotCaptured; binding it to count pixels replaces the file write
	 * entirely. So the sweep had been measuring correctly and quietly leaving the
	 * eight frames on disk untouched from whatever build wrote them last.
	 */
	void WriteHistogramFrame(
		const TCHAR* PointName,
		int32 Width,
		int32 Height,
		const TArray<FColor>& Colors) const;
	void HandleNightOneSolved();
	void HandleNightTwoSolved();
	/** §8 비트 2-5: reaching 403 is what ends night two, not confirming T7. */
	void HandleNightTwoReturnedHome();
	void HandleNightThreeSolved();
	/** §8 비트 3-7: only 403's floor ends night three. */
	void HandleNightThreeReturnedHome();
	/** The 05:30 call belongs to dawn, whichever route brought it. */
	void MakeNightThreeFirstReport();
	void HandleFifthDawnCompleted();
	void HandleNightFourResolved();
	/** §22.3 선택적 목격 셋. 진실도 게이트도 건드리지 않는다. */
	void HandleWaterBowlExamined(class AIGMissingFloorEvidence* Evidence);
	void HandleSleepingPillsExamined(class AIGMissingFloorEvidence* Evidence);
	void HandleCigarettePackExamined(class AIGMissingFloorEvidence* Evidence);
	void HandleStoreRosterExamined(class AIGMissingFloorEvidence* Evidence);
	void HandleUnit401RadioExamined(class AIGMissingFloorEvidence* Evidence);
	void HandleUnit402ListenExamined(class AIGMissingFloorEvidence* Evidence);
	void HandleRoofDoorListenExamined(class AIGMissingFloorEvidence* Evidence);
	/** 소리 목격은 전부 같은 모양이다 — 부피만 세우고 그림은 두지 않는다. */
	class AIGMissingFloorEvidence* SpawnListeningVolume(
		class UStaticMesh* CubeMesh,
		const TCHAR* ActorName,
		const FVector& Location,
		const FVector& Extent,
		const FText& Prompt);
	/** 에필로그가 끝나면 게임은 타이틀로 돌아간다(§9). */
	void HandleEpilogueCompleted();
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

	UPROPERTY(Transient)
	TObjectPtr<class AIGMissingFloorEpilogueDirector> Epilogue;

	/**
	 * §22.3의 선택적 목격 프롭. 어느 것도 진행을 잠그지 않으므로 생성에
	 * 실패해도 스테이지는 유효하다 — ValidateFixtures가 이것들을 묻지 않는
	 * 이유이며, 그 사실 자체가 「없어도 되는 것」이라는 설계의 표현이다.
	 */
	UPROPERTY(Transient)
	TObjectPtr<class AIGMissingFloorEvidence> WaterBowl;

	UPROPERTY(Transient)
	TObjectPtr<class AIGMissingFloorEvidence> SleepingPills;

	UPROPERTY(Transient)
	TObjectPtr<class AIGMissingFloorEvidence> CigarettePack;

	UPROPERTY(Transient)
	TObjectPtr<class AIGMissingFloorEvidence> StoreRoster;

	UPROPERTY(Transient)
	TObjectPtr<class AIGMissingFloorEvidence> Unit401Radio;

	/** 소리로만 확인되는 목격 셋. 전부 부피만 있고 그림은 없다. */
	UPROPERTY(Transient)
	TObjectPtr<class AIGMissingFloorEvidence> Unit402Listen;

	UPROPERTY(Transient)
	TObjectPtr<class AIGMissingFloorEvidence> RoofDoorListen;

	/**
	 * §13 13행의 심기. 중고 거래 글 「달빛」에 오빠의 공구가 세트로 올라와
	 * 있다. 회수는 밤3의 자재 더미 위, 홀로 남은 조율 렌치다.
	 */
	UPROPERTY(Transient)
	TObjectPtr<class AIGReadableNote> UsedListingNote;

	/** §20.3's two automatic safety nets: the world moving when nothing else is. */
	UPROPERTY(Transient)
	TObjectPtr<class AIGMissingFloorMercyDirector> Mercy;

	/** Day interactions: the bed that ends the day, 401's door that talks. */
	UPROPERTY(Transient)
	TObjectPtr<class AIGMissingFloorEvidence> SleepTarget;

	UPROPERTY(Transient)
	TObjectPtr<class AIGMissingFloorEvidence> Unit401Door;

	/** 출시 경로의 안전한 입주 저녁 프롤로그 소품. */
	UPROPERTY(Transient)
	TObjectPtr<class AIGMissingFloorEvidence> ArrivalContract;
	UPROPERTY(Transient)
	TObjectPtr<class AIGMissingFloorEvidence> ArrivalParcelBox;
	UPROPERTY(Transient)
	TObjectPtr<class AIGMissingFloorEvidence> ArrivalNotebookBox;
	UPROPERTY(Transient)
	TObjectPtr<class AIGMissingFloorEvidence> ArrivalVoicemailBox;
	UPROPERTY(Transient)
	TObjectPtr<class AIGMissingFloorEvidence> ArrivalStoreBell;
	UPROPERTY(Transient)
	TObjectPtr<class AIGMissingFloorEvidence> ArrivalUnit402Note;
	UPROPERTY(Transient)
	TObjectPtr<class AIGMissingFloorEvidence> ArrivalRoofLock;

	UPROPERTY(Transient)
	TWeakObjectPtr<AIGPlayerCharacter> Player;

	UPROPERTY(Transient)
	TWeakObjectPtr<AIGPrologueWorldScene> WorldScene;

	UPROPERTY(Transient)
	TObjectPtr<UIGNoiseSubsystem> NoiseSubsystem;

	/** 404 냉장고 험. 실패한 시도가 남긴 것을 걷어 낼 수 있어야 한다. */
	int32 FridgeHumHandle = INDEX_NONE;

	/** 복도 동쪽 끝 설비 벽장의 험. 위와 같은 이유로 손잡이를 든다. */
	int32 BoilerHumHandle = INDEX_NONE;

	/** 험 둘이 실제로 내는 소리. 마스킹과 같은 반경까지만 들린다. */
	UPROPERTY(Transient)
	TObjectPtr<UAudioComponent> FridgeHumLoop;

	UPROPERTY(Transient)
	TObjectPtr<UAudioComponent> BoilerHumLoop;

	/** 밤의 환경 베드 셋: 4층 복도, 계단실, 5층. 험 반경 밖이 무음이던 것을 채운다. */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UAudioComponent>> NightAmbienceBeds;
	/** 밤 사이 위에서 한 번씩 나는 건물 소리. 45~110초마다. */
	FTimerHandle SettleTimerHandle;
	int32 SettleCounter = 0;
	void SpawnNightAmbienceBeds();
	void ScheduleNextSettle();
	void PlaySettleEvent();

	/**
	 * 지난 시도가 만들다 만 것을 치운다.
	 *
	 * 스폰에 이름을 지정하므로 같은 이름이 살아 있으면 다음 시도의 스폰이
	 * 실패한다. 치우지 않으면 재시도가 영영 통과하지 못한 채 6초 뒤
	 * 「월드 씬이나 플레이어가 없다」로 끝난다.
	 */
	void DestroyPartialStage();

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
		/** §8 비트 2-5: leaving the booth drops the stack and starts the chase. */
		NightTwoReturnChaseContract,
		/** §8 비트 2-5: only 403's floor ends night two. */
		NightTwoHomeContract,
		DayTwoContract,
		NightThreeContract,
		/** §8 비트 3-7: the learned answer, knocked at nothing, reaching him. */
		AnswerReachContract,
		AnswerReachCadence,
		AnswerPairTap,
		AnswerFinalTap,
		AnswerContract,
		/** §8 비트 3-7: the learned answer stops him in her corridor. */
		NightThreePassContract,
		/** §8 비트 3-7: only 403's floor ends night three. */
		NightThreeHomeContract,
		/** §24 즉시 차단 19: the sealed hour turns F9 and the journal down. */
		SealedHourUiContract,
		NightFourContract,
		NightFourFailureRetryContract,
		NightFourWallContract,
		NightFourEndingContract,
		/** §35: 선택 뒤의 87초와, 진행을 잠그지 않는 목격 넷. */
		EpilogueContract,
		Done
	};

	EProbeStep ProbeStep = EProbeStep::Inactive;
	float StepDeadlineSeconds = 0.0f;
	float SetupRetrySeconds = 0.0f;
	int32 FailureRetryCaptureCountBefore = 0;
	bool bNightFourFailureRetryVerified = false;
	/** Latch so the mercy step fires its nets once and then waits for the paper. */
	bool bMercyNetsFired = false;

	/** §8 비트 3-7's cadence walk: the probe has to leave real gaps between taps. */
	double AnswerReachTapTwoAt = 0.0;
	int32 AnswerReachTapsSent = 0;
	bool bAnswerReachWasDormant = false;

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
	bool bArrivalProbeRequested = false;
	bool bArrivalCaptureRequested = false;
	int32 ArrivalCaptureStep = 0;
	bool bProductionMode = false;
	FTimerHandle SetupTimer;
	FTimerHandle ArrivalCaptureTimer;
	FTimerHandle ProbeTimer;
};
