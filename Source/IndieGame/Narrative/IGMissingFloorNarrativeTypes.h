#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "IGMissingFloorNarrativeTypes.generated.h"

/**
 * The ten truths of 없는 층 (STORY_BIBLE_MISSING_FLOOR.md §12).
 *
 * Values are numbered explicitly and must never be renumbered: they are not
 * serialized themselves, but they index the rule table whose source names are.
 */
UENUM(BlueprintType)
enum class EIGMissingFloorTruth : uint8
{
	None = 0,
	/** T1 — the fifth floor was lived in. */
	LivedUpstairs = 1,
	/** T2 — the tenant was Baek Doha. */
	TenantIdentity = 2,
	/** T3 — the pre-dawn noise was him coming home. */
	NoiseWasHomecoming = 3,
	/** T4 — there was a struggle on the landing that morning. */
	LandingStruggle = 4,
	/** T5 — Mok Hansu finished the wall that day. */
	WallSealedThatDay = 5,
	/** T6 — there is a person between the studs. */
	SomeoneInTheWall = 6,
	/** T7 — whatever was in the wall was still alive. */
	WasStillAlive = 7,
	/** T8 — he knocked for five nights and died of thirst. */
	FiveNightsOfThirst = 8,
	/** T9 — he is waiting for an answer. */
	WaitingForAnAnswer = 9,
	/** T10 — Mok Hansu is still covering it up. */
	StillCoveringIt = 10
};

/**
 * Independent evidence records. Names, not values, are serialized, so entries
 * may be appended freely; an unknown name in an old save simply matches no
 * category. Grouped by the truth whose categories they feed.
 */
UENUM(BlueprintType)
enum class EIGMissingFloorSource : uint8
{
	None = 0,

	// T1 — meter cabinet and the reading sheet.
	MeterFifthDial = 1,
	MeterReadingSheet = 2,

	// T2 — shipping labels and the notebook left on five.
	ShippingLabels = 3,
	TunerNotebookName = 4,

	// T3 — the noise forum and his work hours.
	NoiseForumPosts = 5,
	TunerWorkSchedule = 6,

	// T4 — the last post and the mark on the landing.
	ForumFinalPost = 7,
	LandingImpactMark = 8,

	// T5 — the board delivery receipt and the age of the plaster.
	BoardDeliveryReceipt = 9,
	FreshPlasterDating = 10,

	// T6 — the criterion, then the location (patiently by water, or by fist).
	PipeAuditionCriterion = 11,
	PipeWaterComparison = 12,
	WallEchoByHand = 13,

	// T7 — the carbon-copy original and the agent's move-out message.
	CarbonLedgerOriginal = 14,
	AgentMoveOutMessage = 15,

	// T8 — the tally that thins out, and two tonnes of water next door.
	KnockTallyJournal = 16,
	TankWaterAudition = 17,

	// T9 — knowing the rhythm, and being answered.
	AnswerRhythmMaterials = 18,
	AnswerReturned = 19,

	// T10 — the eviction notice and the hand on the breaker.
	EvictionWarning = 20,
	BreakerCutIntervention = 21
};

/** One truth and the provenance behind it. Confirmation is always derived. */
USTRUCT(BlueprintType)
struct INDIEGAME_API FIGMissingFloorTruthRecord
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category = "Narrative")
	FGameplayTag TruthTag;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category = "Narrative")
	TArray<FName> SourceIds;

	/**
	 * Recomputed from SourceIds on every write and on restore. Never set
	 * directly: a truth must not be able to survive without its sources.
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category = "Narrative")
	bool bConfirmed = false;
};

/**
 * The hour's runtime facts that must survive a quit (§5.4). The M1 entity and
 * night-loop director keep these in plain actor members, which silently reset
 * the pursuer to tier 0 on resume; this is where they belong instead.
 */
USTRUCT(BlueprintType)
struct INDIEGAME_API FIGMissingFloorNightState
{
	GENERATED_BODY()

	/** 0 = prologue evening, 1..4 = the four nights. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category = "Narrative")
	int32 NightIndex = 0;

	/** The one upstairs grows impatient with every capture; 0..3. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category = "Narrative")
	int32 AggressionTier = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category = "Narrative")
	int32 CaptureCount = 0;

	/** True while 04:30–05:30 holds the building shut. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category = "Narrative")
	bool bTheHourSealed = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category = "Narrative")
	float NightElapsedSeconds = 0.0f;

	/** Scripted beats already played once (§8), by author-facing name. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category = "Narrative")
	TArray<FName> CompletedBeats;

	/** P1..P5 solved, by author-facing name. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category = "Narrative")
	TArray<FName> SolvedPuzzles;
};

/**
 * The whole 없는 층 narrative state. Rides alongside the legacy REBIRTH
 * snapshot inside FIGProgressSnapshot rather than replacing it: the legacy
 * truth set is pinned by five validation contracts and by every existing
 * save, so the two coexist until the legacy directors are retired (§14).
 */
USTRUCT(BlueprintType)
struct INDIEGAME_API FIGMissingFloorNarrativeSnapshot
{
	GENERATED_BODY()

	/**
	 * Read before normalizing, unlike the legacy snapshot's write-only
	 * counterpart, so a future revision can actually migrate.
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category = "Narrative")
	int32 SchemaVersion = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category = "Narrative")
	TArray<FIGMissingFloorTruthRecord> Truths;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category = "Narrative")
	FIGMissingFloorNightState Night;
};
