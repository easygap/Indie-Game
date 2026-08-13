#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "Entity/IGListenerTuning.h"
#include "Entity/IGNoiseSubsystem.h"
#include "IGListenerEntity.generated.h"

class UAudioComponent;
class UCapsuleComponent;
class UIGDustSubsystem;
class UMaterialInterface;
class UStaticMeshComponent;

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

/**
 * 위층 사람 — the one upstairs. Blind; hunts entirely by sound through the
 * IGNoiseSubsystem. It knocks, then listens; it investigates what it hears
 * and bursts into a chase when a sound answers twice. Catching the player is
 * not violence — it is an embrace and a walk toward the wall — and hands
 * control to the night-loop director, which resets the hour.
 *
 * Release body: a continuous static crawl shell provides contact/parallax and
 * a four-phase lit masked PBR layer preserves both generated human anatomy and
 * visible weight transfer in the authored head-on chase. Engine primitives
 * remain only as a missing-asset fallback. No skeletal pipeline is required.
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

	/** Burst speed. Faster than the player walks; the point of the rules. */
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

	UPROPERTY(Transient)
	TArray<TObjectPtr<UMaterialInterface>> ListenerPhaseMaterials;

	bool bFrontCardActive = false;
	int32 ListenerPhaseIndex = INDEX_NONE;
	float ListenerPhase = 0.0f;
	float PresentationSpeed = 0.0f;

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

	UPROPERTY(Transient)
	TWeakObjectPtr<APawn> CachedPlayer;
};
