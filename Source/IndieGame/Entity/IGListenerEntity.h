#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "Entity/IGListenerTuning.h"
#include "Entity/IGNoiseSubsystem.h"
#include "IGListenerEntity.generated.h"

class AIGPlayerCharacter;
class UAudioComponent;
class UCapsuleComponent;
class UIGDustSubsystem;
class UMaterialInstanceDynamic;
class UMaterialInterface;
class UStaticMeshComponent;
class USkeletalMeshComponent;
class UAnimSequence;

/** What the one upstairs is doing. See STORY_BIBLE_MISSING_FLOOR.md §4.5. */
UENUM(BlueprintType)
enum class EIGListenerState : uint8
{
	/** Crawling between patrol nodes. */
	Patrolling,
	/** Stationary, knocking three times. The player's masked window. */
	Banging,
	/** Stationary, listening. Hearing doubles. */
	Listening,
	/** Moving to the last heard sound. */
	Investigating,
	/** Arrived where it heard something; holding still and listening. */
	Holding,
	/** Burst pursuit toward the last sound. */
	Chasing,
	/** Lost the sound; short local sweep before giving up. */
	Searching,
	/** Frozen by an answer knock. Hope, while it lasts. */
	Waiting,
	/** Holding the caught player; the director owns the screen. */
	CaptureHold,
	/** Night-four authored pass: follows Mok, never diverts to or catches Yudam. */
	FinaleLured
};

DECLARE_MULTICAST_DELEGATE_OneParam(FIGPlayerCapturedSignature, APawn* /*Player*/);

/** 스켈레탈 몸이 재생하는 동작. rig_crawler.py의 액션 넷과 같은 이름이다. */
enum class EIGListenerBodyAnim : uint8
{
	None,
	Crawl,
	Listen,
	Bang,
	Lunge
};

/**
 * 위층 사람 — the one upstairs. Blind; hunts entirely by sound through the
 * IGNoiseSubsystem. It knocks, then listens; it investigates what it hears
 * and bursts into a chase when a sound answers twice. Catching the player is
 * not violence — it is an embrace and a walk toward the wall — and hands
 * control to the night-loop director, which resets the hour.
 *
 * 사진을 대조해 다듬은 스켈레탈 몸이 포복·듣기·노크·덮치기를 재생한다.
 * 접지 동작은 Blender에서 구우며, 거리별 LOD로 스키닝 정점 수를 줄인다.
 */
UCLASS()
class INDIEGAME_API AIGListenerEntity : public APawn
{
	GENERATED_BODY()

public:
	AIGListenerEntity();

	virtual void Tick(float DeltaSeconds) override;

	UFUNCTION(BlueprintPure, Category = "Listener")
	EIGListenerState GetListenerState() const { return State; }

	/** Capture escalation, 0..3. Raised by the director on each loop reset. */
	UFUNCTION(BlueprintPure, Category = "Listener")
	int32 GetAggressionTier() const { return AggressionTier; }

	UFUNCTION(BlueprintCallable, Category = "Listener")
	void SetAggressionTier(int32 Tier);

	/**
	 * Re-resolves §20.2 and §20.4 for the night that is starting. Called on
	 * waking and whenever the tier moves, so a difficulty change mid-run takes
	 * effect at the next night without touching saves (§20.4).
	 */
	void RefreshNightTuning();

	/** The numbers this night is actually running on. */
	const FIGListenerTuning& GetTuning() const { return Tuning; }

	EIGNightDifficulty GetDifficulty() const { return Difficulty; }

	/** Harness hook: forces a mode for one run without writing it back. */
	void SetDifficultyForTesting(EIGNightDifficulty NewDifficulty);

	/**
	 * World-space patrol stops. The entity crawls node to node, knocking and
	 * listening at each. With no nodes it haunts its spawn point in place.
	 */
	UFUNCTION(BlueprintCallable, Category = "Listener")
	void SetPatrolPoints(const TArray<FVector>& Points);

	/**
	 * The learned answer: two knocks, a rest, one. Freezes an approaching
	 * entity into Waiting — and tells it exactly where the answer came from.
	 * When hope runs out it investigates that spot.
	 */
	UFUNCTION(BlueprintCallable, Category = "Listener")
	void NotifyAnswerKnock(const FVector& KnockLocation);

	/**
	 * 둘-쉬고-하나의 인식기. 플레이어가 아무것도 조준하지 않고 두드릴 때
	 * 불리며, 세 번째 탭이 박자에 맞으면 NotifyAnswerKnock으로 넘긴다.
	 *
	 * §7 P4는 대답하는 법을 가르치고 §8 비트 3-7은 그것으로 복도를 지나가라고
	 * 한다. 그런데 이 게임에는 대답이 존재에게 닿는 경로가 아예 없었다 —
	 * Waiting 상태와 NotifyAnswerKnock은 구현돼 있었지만 부르는 사람이 없어서,
	 * 배운 문법을 P4의 지정된 벽 밖에서는 쓸 수 없었다.
	 *
	 * 대답은 공짜가 아니다. 기다림이 끝나면 그는 **대답이 온 자리**를 조사한다.
	 * 그래서 언제든 두드릴 수 있게 두어도 은신이 무너지지 않는다: 멈추게 하는
	 * 대가로 자기 위치를 준다.
	 *
	 * Returns true when the tap was taken as part of an answer — the caller then
	 * owns the sound and the feedback, and must not fall through to a door.
	 */
	bool TryAnswerKnock(const FVector& KnockLocation);

	/**
	 * 응답 박자의 유일한 정의. P4의 벽과 복도의 맨손 노크가 같은 창을 써야
	 * 하므로, 밤3 디렉터도 이 값을 참조한다.
	 */
	static constexpr double AnswerPairMinSeconds = 0.18;
	static constexpr double AnswerPairMaxSeconds = 0.65;
	static constexpr double AnswerRestMinSeconds = 0.68;
	static constexpr double AnswerRestMaxSeconds = 1.80;
	/** 마지막 탭에서 이만큼 지나면 시도가 처음부터 다시 시작된다. */
	static constexpr double AnswerSequenceResetSeconds = 3.0;

	/**
	 * Walks him to a spot and lets him hold there, silently, without any sound
	 * having called him.
	 *
	 * §20.3's first safety net is a *witnessed* thing, not a hint: he stops in
	 * front of the wall that matters and puts his ear to it. The player is shown
	 * where to look and told nothing at all. Deliberately not routed through the
	 * noise path, so this can never escalate into a chase — a player who is
	 * already stuck must not be punished for being helped.
	 */
	UFUNCTION(BlueprintCallable, Category = "Listener")
	void BeginObservationHold(const FVector& Target);

	/**
	 * 저작된 카메오를 위해 그를 한 자리에 세운다 — 위치, 방향, 그리고 **직전
	 * 소리에 대한 반응을 지운다.**
	 *
	 * 마지막 항목이 요점이다. 텔레포트만 하면 그는 여전히 조사 중이고, 다음
	 * 프레임부터 자기가 들은 자리를 향해 기어가 버린다 — 비트 2-1과 3-7은 둘
	 * 다 플레이어가 방금 소리를 낸 직후에 그를 세우므로, 지우지 않으면 카메오가
	 * 시작하자마자 화면 밖으로 걸어 나간다.
	 *
	 * 순찰 지점은 부르는 쪽이 먼저 넘긴다. 공격 티어는 건드리지 않는다: 연출은
	 * 실패가 아니다.
	 */
	void ParkForBeat(const FVector& Where, float Yaw);

	/** Returns the entity to its patrol start after a capture reset. */
	UFUNCTION(BlueprintCallable, Category = "Listener")
	void ResetToPatrolStart(bool bRaiseAggression);

	/**
	 * Day rest (§1: 낮 구간은 안전하다). Dormant, it is hidden, silent,
	 * tick-free and deaf — the daytime building must never knock. Waking
	 * puts it back at its patrol start with its earned impatience intact.
	 */
	UFUNCTION(BlueprintCallable, Category = "Listener")
	void SetDormant(bool bInDormant);

	UFUNCTION(BlueprintPure, Category = "Listener")
	bool IsDormant() const { return bDormant; }

	/**
	 * Runs the finale-only blind pass from StartLocation through RoutePoints.
	 * This is presentation locomotion, not a stealth failure: player collision,
	 * capture and ordinary noise retargeting stay disabled until the pawn exits.
	 */
	void BeginFinalePass(
		const FVector& StartLocation,
		const TArray<FVector>& RoutePoints);

	bool IsFinalePassActive() const
	{
		return State == EIGListenerState::FinaleLured && !bDormant;
	}

	/** Fired once per catch; the night-loop director listens. */
	FIGPlayerCapturedSignature OnPlayerCaptured;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** Crawl speed between patrol nodes, cm/s. Slow enough to walk away from. */
	UPROPERTY(EditAnywhere, Category = "Listener|Movement", meta = (ClampMin = "0.0"))
	float CrawlSpeed = 110.0f;

	UPROPERTY(EditAnywhere, Category = "Listener|Movement", meta = (ClampMin = "0.0"))
	float InvestigateSpeed = 240.0f;

	/** Burst speed. 밤2부터 달리기와 같거나 빠르다. 뛰어서 벗어나는 게 아니라 소리를 끊어야 한다. */
	UPROPERTY(EditAnywhere, Category = "Listener|Movement", meta = (ClampMin = "0.0"))
	float ChaseSpeed = 460.0f;

	/** Touching distance that ends the night, in centimeters. */
	UPROPERTY(EditAnywhere, Category = "Listener|Movement", meta = (ClampMin = "0.0"))
	float CaptureRadius = 110.0f;

	/** Cross-floor sounds read farther away than line distance says. */
	UPROPERTY(EditAnywhere, Category = "Listener|Hearing", meta = (ClampMin = "1.0"))
	float CrossFloorDistancePenalty = 1.4f;

	/** Height difference that counts as another floor, in centimeters. */
	UPROPERTY(EditAnywhere, Category = "Listener|Hearing", meta = (ClampMin = "0.0"))
	float FloorHeightThreshold = 240.0f;

private:
	// -- state machine ------------------------------------------------------
	void EnterState(EIGListenerState NewState);
	void TickState(float DeltaSeconds);
	void HandleNoise(const FIGNoiseEvent& Event);
	bool CanHear(const FIGNoiseEvent& Event) const;
	/** 대답 노크가 그에게 닿는가. 거리와 험 마스킹을 소음과 같은 귀로 잰다. */
	bool CanHearAnswerFrom(const FVector& KnockLocation) const;
	float HearingMultiplier() const;
	float ListenSecondsForTier() const;
	float WaitSecondsForTier() const;

	// -- locomotion ---------------------------------------------------------
	/** Sweeps toward Target; returns true on arrival (or when wedged). */
	bool CrawlTowards(const FVector& Target, float Speed, float DeltaSeconds);
	void FaceDirection(const FVector& Direction, float DeltaSeconds);
	const FVector* CurrentPatrolTarget() const;

	/**
	 * §5.6: picks the next patrol stop. With no heatmap weight it is the plain
	 * round of the route; as the weight rises the stops the player has been
	 * loud near start winning. Pure statistics, so the same play produces the
	 * same route.
	 */
	void AdvancePatrolIndex();

	/**
	 * §5.6 tier-3 ambush: goes to the hottest zone and waits there without
	 * knocking. The knock cycle disappearing is the tell — late in the game a
	 * quiet building is the dangerous one.
	 */
	bool TryBeginAmbush();

	// -- presentation -------------------------------------------------------
	void BuildGreyboxBody();
	/**
	 * 리깅된 몸. Scripts/blender/rig_crawler.py가 만든 SK_ListenerCrawler와
	 * 동작 넷(Crawl·Listen·Bang·Lunge)을 싣는다. 있으면 정적 셸과 스프라이트
	 * 카드는 만들지 않는다 — 카드는 정면에서만 사람이었고 옆에서는 판이었다.
	 */
	bool BuildSkeletalBody();
	void PlayBodyAnim(EIGListenerBodyAnim Anim, bool bLoop, float Rate);
	void UpdateSkeletalPose(float SpeedAlpha, float BodyRate, float DeltaSeconds);
	/** 기는 주기 재생 배율. 순찰 속도에서 1.0, 추격에서 상한. */
	float ComputeCrawlRate(float Speed) const;
	void UpdatePresentationLayer();
	void UpdatePresentationPose(float CurrentSpeed, float DeltaSeconds);
	void PlayKnockTriple();
	void PlayPlasterSettle();
	void UpdateDragLoop(float CurrentSpeed);

	/**
	 * §10.3 끌림 2종: swaps the crawl bed when he moves between tile and 장판.
	 * Traced from the floor beneath him with the same authored surface tags the
	 * §21.2 footstep matrix uses, so the two systems can never disagree about
	 * what he is dragging himself across.
	 */
	void RefreshDragSurface();
	void UpdateThreatPressure();

	/**
	 * Leaves the plaster dust his drag raises in the air (§11 V1). It is a
	 * report, not a render: the torch decides whether anyone ever sees it, and
	 * he does not know he is leaving a trail any more than he knows he is loud.
	 */
	void ReportDustTrail();

	void BeginCapture(APawn* Player);

	UPROPERTY(VisibleAnywhere, Category = "Listener|Components")
	TObjectPtr<UCapsuleComponent> Body;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UStaticMeshComponent>> BodyBlocks;

	/** Continuous close/side shell and authored long-corridor front layer. */
	UPROPERTY(Transient)
	TObjectPtr<UStaticMeshComponent> ListenerShell;

	UPROPERTY(Transient)
	TObjectPtr<UStaticMeshComponent> ListenerFrontCard;

	/** 리깅된 몸. 이것이 있으면 위 둘은 null이다. */
	UPROPERTY(Transient)
	TObjectPtr<USkeletalMeshComponent> ListenerSkeletal;

	UPROPERTY(Transient)
	TObjectPtr<UAnimSequence> CrawlAnim;

	UPROPERTY(Transient)
	TObjectPtr<UAnimSequence> ListenAnim;

	UPROPERTY(Transient)
	TObjectPtr<UAnimSequence> BangAnim;

	UPROPERTY(Transient)
	TObjectPtr<UAnimSequence> LungeAnim;

	EIGListenerBodyAnim ActiveBodyAnim = EIGListenerBodyAnim::None;
	/** 추격 중 두 팔 거리 안에 들어오면 덮치는 동작으로 바꾼다. */
	bool bLungeArmed = false;

	/**
	 * 셸 석고 재질의 인스턴스. 숨과 잔떨림은 재질 WPO가 만들고, 상태 머신은
	 * 여기로 진폭만 넘긴다. 카드 4단계는 저작된 정지 프레임이라 그대로 둔다.
	 */
	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> ListenerShellMid;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UMaterialInterface>> ListenerPhaseMaterials;

	bool bFrontCardActive = false;
	int32 ListenerPhaseIndex = INDEX_NONE;
	float ListenerPhase = 0.0f;
	float PresentationSpeed = 0.0f;
	/** 재질 기본값과 같은 순찰 기준치에서 시작해 상태에 따라 보간된다. */
	float ShellBreathAmplitude = 0.45f;
	float ShellTremorAmplitude = 0.1f;

	UPROPERTY(Transient)
	TObjectPtr<UAudioComponent> DragLoopComponent;

	UPROPERTY(Transient)
	TObjectPtr<UIGNoiseSubsystem> NoiseSubsystem;

	UPROPERTY(Transient)
	TObjectPtr<UIGDustSubsystem> DustSubsystem;

	FDelegateHandle NoiseHandle;

	UPROPERTY(EditAnywhere, Category = "Listener|Patrol")
	TArray<FVector> PatrolPoints;

	EIGListenerState State = EIGListenerState::Patrolling;
	FIGListenerTuning Tuning;
	EIGNightDifficulty Difficulty = EIGNightDifficulty::Standard;
	/** Set when the ambush node has been chosen for this tier-3 stretch. */
	FVector AmbushLocation = FVector::ZeroVector;
	bool bAmbushArmed = false;
	bool bDormant = false;
	int32 AggressionTier = 0;
	int32 PatrolIndex = 0;
	FVector SpawnLocation = FVector::ZeroVector;
	FVector LastHeardLocation = FVector::ZeroVector;
	double LastHeardTime = -1000.0;
	/** Hearing something while already reacting to a sound means a chase. */
	bool bReactingToSound = false;
	FVector SearchAnchor = FVector::ZeroVector;
	FVector SearchTarget = FVector::ZeroVector;
	FVector AnswerKnockLocation = FVector::ZeroVector;

	/** The player's in-progress answer. Never more than the last three taps. */
	TArray<double> AnswerTapTimes;
	/**
	 * 이 밤에 대답이 통한 횟수. 두 번째부터 기다림이 짧아지고 네 번째부터는
	 * 대답이 오지 않는다. 같은 박자를 6초마다 두드리면 밤새 안 잡히던 구멍을
	 * 막는다. 포획 리셋에는 남고 밤이 바뀔 때 0이 된다.
	 */
	int32 AnswersThisNight = 0;
	TArray<FVector> FinaleRoutePoints;
	int32 FinaleRouteIndex = 0;
	float StateSeconds = 0.0f;
	float SearchRetargetSeconds = 0.0f;
	float StuckSeconds = 0.0f;
	float LastMoveSpeed = 0.0f;
	FVector LastDustReportLocation = FVector::ZeroVector;
	float DustSiftCentimeters = 0.0f;
	float DragSurfacePollSeconds = 0.0f;
	bool bDustTrailSeeded = false;
	/** True while the crawl bed is the 장판 variant rather than tile. */
	bool bDragSurfaceIsVinyl = false;

	/** 그의 숨. 상태와 거리로 볼륨이 정해진다. 자는 동안은 0. */
	UPROPERTY(Transient)
	TObjectPtr<UAudioComponent> BreathLoopComponent;
	float BreathVolumeTarget = 0.0f;
	/** 기는 걸음 소리의 마지막 박자 칸. 네 자세 한 바퀴에 두 걸음. */
	int32 LastCrawlStepIndex = -1;
	/** 코앞에서 마주친 스팅어의 마지막 시각. 25초에 한 번. */
	double LastCloseCallSeconds = -1000.0;
	void UpdateBreathLoop(float Distance);
	void TryCloseCallStinger(const AIGPlayerCharacter* Player, float Distance);

	UPROPERTY(Transient)
	TWeakObjectPtr<APawn> CachedPlayer;
};
