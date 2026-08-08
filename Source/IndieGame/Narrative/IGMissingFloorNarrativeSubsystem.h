#pragma once

#include "CoreMinimal.h"
#include "Narrative/IGMissingFloorNarrativeTypes.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "IGMissingFloorNarrativeSubsystem.generated.h"

DECLARE_MULTICAST_DELEGATE_OneParam(FIGMissingFloorTruthSignature, EIGMissingFloorTruth);

/**
 * The truth router for 없는 층 (STORY_BIBLE_MISSING_FLOOR.md §12).
 *
 * Every truth is confirmed the same way: by crossing **two independent
 * categories** of evidence. That uniformity is the whole reason this is a
 * table rather than a chain of bespoke predicates — see the rule table in the
 * .cpp, which is the single place the design's crossing rules live.
 *
 * Confirmation is always derived from the recorded sources, never latched, so
 * restoring a save (or hand-editing one) cannot produce a truth whose evidence
 * is missing. The final choice is gated on T6 + T7 + T9 and nothing else.
 *
 * This lives beside UIGRebirthNarrativeSubsystem rather than replacing it: the
 * legacy twelve truths are pinned by the REBIRTH contract scripts and by every
 * shipped save, and Config's ClearInvalidTags=True means deleting their tags
 * would silently rewrite old saves.
 */
UCLASS()
class INDIEGAME_API UIGMissingFloorNarrativeSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	/** The registered tag for a truth, or an invalid tag if unregistered. */
	UFUNCTION(BlueprintPure, Category = "Narrative|MissingFloor")
	static FGameplayTag GetTruthTag(EIGMissingFloorTruth Truth);

	/** The serialized name for an evidence record. */
	UFUNCTION(BlueprintPure, Category = "Narrative|MissingFloor")
	static FName GetSourceId(EIGMissingFloorSource Source);

	/**
	 * Records one piece of evidence and re-derives the truth it feeds.
	 * Returns true when this record was the one that completed the crossing.
	 */
	UFUNCTION(BlueprintCallable, Category = "Narrative|MissingFloor")
	bool RegisterTruthSource(EIGMissingFloorTruth Truth, EIGMissingFloorSource Source);

	UFUNCTION(BlueprintPure, Category = "Narrative|MissingFloor")
	bool HasTruth(EIGMissingFloorTruth Truth) const;

	/** True once this exact record has been observed. */
	UFUNCTION(BlueprintPure, Category = "Narrative|MissingFloor")
	bool HasSource(EIGMissingFloorTruth Truth, EIGMissingFloorSource Source) const;

	UFUNCTION(BlueprintPure, Category = "Narrative|MissingFloor")
	int32 GetConfirmedTruthCount() const;

	/** T6 + T7 + T9 — the wall, the life inside it, and the waiting. */
	UFUNCTION(BlueprintPure, Category = "Narrative|MissingFloor")
	bool IsFinalChoiceUnlocked() const;

	// -- the hour's persistent runtime facts (§5.4) ------------------------

	UFUNCTION(BlueprintCallable, Category = "Narrative|MissingFloor")
	void SetNightIndex(int32 NightIndex);

	UFUNCTION(BlueprintPure, Category = "Narrative|MissingFloor")
	int32 GetNightIndex() const { return Snapshot.Night.NightIndex; }

	UFUNCTION(BlueprintCallable, Category = "Narrative|MissingFloor")
	void SetHourSealed(bool bSealed);

	UFUNCTION(BlueprintPure, Category = "Narrative|MissingFloor")
	bool IsHourSealed() const { return Snapshot.Night.bTheHourSealed; }

	UFUNCTION(BlueprintCallable, Category = "Narrative|MissingFloor")
	void SetNightElapsedSeconds(float Seconds);

	UFUNCTION(BlueprintPure, Category = "Narrative|MissingFloor")
	float GetNightElapsedSeconds() const { return Snapshot.Night.NightElapsedSeconds; }

	/** Counts the capture and returns the resulting aggression tier (0..3). */
	UFUNCTION(BlueprintCallable, Category = "Narrative|MissingFloor")
	int32 RecordCapture();

	UFUNCTION(BlueprintPure, Category = "Narrative|MissingFloor")
	int32 GetAggressionTier() const { return Snapshot.Night.AggressionTier; }

	UFUNCTION(BlueprintCallable, Category = "Narrative|MissingFloor")
	void SetAggressionTier(int32 Tier);

	UFUNCTION(BlueprintPure, Category = "Narrative|MissingFloor")
	int32 GetCaptureCount() const { return Snapshot.Night.CaptureCount; }

	/** Returns false when the beat had already been played. */
	UFUNCTION(BlueprintCallable, Category = "Narrative|MissingFloor")
	bool MarkBeatPlayed(FName BeatId);

	UFUNCTION(BlueprintPure, Category = "Narrative|MissingFloor")
	bool HasBeatPlayed(FName BeatId) const;

	UFUNCTION(BlueprintCallable, Category = "Narrative|MissingFloor")
	bool MarkPuzzleSolved(FName PuzzleId);

	UFUNCTION(BlueprintPure, Category = "Narrative|MissingFloor")
	bool IsPuzzleSolved(FName PuzzleId) const;

	// -- persistence -------------------------------------------------------

	const FIGMissingFloorNarrativeSnapshot& GetSnapshot() const { return Snapshot; }
	void RestoreSnapshot(const FIGMissingFloorNarrativeSnapshot& InSnapshot);
	void ResetNarrative();

	/** Fires once, when a truth first crosses. */
	FIGMissingFloorTruthSignature OnTruthConfirmed;

	/** Current snapshot schema. Bumped only with a matching migration. */
	static constexpr int32 SnapshotSchemaVersion = 1;

private:
	/** Rebuilds bConfirmed on every record from its sources alone. */
	void RecomputeConfirmations(bool bBroadcastNewlyConfirmed);
	/** Drops unknown tags and duplicate records left by an older build. */
	void NormalizeSnapshot();
	FIGMissingFloorTruthRecord* FindRecord(const FGameplayTag& TruthTag);
	const FIGMissingFloorTruthRecord* FindRecord(const FGameplayTag& TruthTag) const;

	FIGMissingFloorNarrativeSnapshot Snapshot;
};
