#pragma once

#include "CoreMinimal.h"
#include "Narrative/IGRebirthNarrativeTypes.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "IGRebirthNarrativeSubsystem.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(
	FIGRebirthTruthChangedSignature,
	FGameplayTag, TruthTag,
	FName, SourceId,
	bool, bConfirmed);

/**
 * Data-driven REBIRTH narrative state.
 *
 * The legacy story-tag subsystem remains the compatibility event bus. This
 * subsystem owns evidence provenance, narrative debt, branch choices,
 * one-shot presentation, and chapter-local puzzle history.
 */
UCLASS()
class INDIEGAME_API UIGRebirthNarrativeSubsystem final
	: public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "REBIRTH|Truth")
	bool RegisterTruthSource(
		FGameplayTag TruthTag,
		FName SourceId,
		bool bConfirmsTruth = true);

	UFUNCTION(BlueprintPure, Category = "REBIRTH|Truth")
	bool HasTruth(FGameplayTag TruthTag) const;

	UFUNCTION(BlueprintPure, Category = "REBIRTH|Truth")
	int32 GetTruthSourceCount(FGameplayTag TruthTag) const;

	UFUNCTION(BlueprintPure, Category = "REBIRTH|Truth")
	TArray<FName> GetTruthSources(FGameplayTag TruthTag) const;

	UFUNCTION(BlueprintCallable, Category = "REBIRTH|Routing")
	void RecomputeChapterThreeDebt();

	UFUNCTION(BlueprintPure, Category = "REBIRTH|Routing")
	bool CanConverge(EIGRebirthConvergencePoint Point) const;

	UFUNCTION(BlueprintPure, Category = "REBIRTH|Routing")
	FGameplayTagContainer GetNarrativeDebt() const
	{
		return State.NarrativeDebt;
	}

	UFUNCTION(BlueprintCallable, Category = "REBIRTH|History")
	bool MarkLocationVisited(FName LocationId);

	UFUNCTION(BlueprintCallable, Category = "REBIRTH|History")
	bool MarkPuzzleResolved(FName PuzzleId);

	UFUNCTION(BlueprintCallable, Category = "REBIRTH|History")
	bool MarkPuzzleSkipped(FName PuzzleId);

	UFUNCTION(BlueprintCallable, Category = "REBIRTH|History")
	bool MarkOutfitEquipped(FName ChapterId);

	UFUNCTION(BlueprintCallable, Category = "REBIRTH|History")
	bool MarkOneShotBeatPlayed(FName BeatId);

	UFUNCTION(BlueprintPure, Category = "REBIRTH|History")
	bool WasOneShotBeatPlayed(FName BeatId) const;

	UFUNCTION(BlueprintCallable, Category = "REBIRTH|Choices")
	void SetChoices(const FIGRebirthChoiceState& Choices);

	UFUNCTION(BlueprintPure, Category = "REBIRTH|Choices")
	FIGRebirthChoiceState GetChoices() const
	{
		return State.Choices;
	}

	UFUNCTION(BlueprintCallable, Category = "REBIRTH|State")
	void SetTankOpenedEarly(bool bOpened)
	{
		State.bTankOpenedEarly = bOpened;
	}

	UFUNCTION(BlueprintCallable, Category = "REBIRTH|State")
	void SetHasMemoryFlashlight(bool bHasFlashlight)
	{
		State.bHasMemoryFlashlight = bHasFlashlight;
	}

	UFUNCTION(BlueprintCallable, Category = "REBIRTH|CH03")
	void SetChapterThreeState(const FIGRebirthChapterThreeState& ChapterThreeState);

	UFUNCTION(BlueprintPure, Category = "REBIRTH|CH03")
	FIGRebirthChapterThreeState GetChapterThreeState() const
	{
		return State.ChapterThree;
	}

	/** Commits exactly one branch and removes any stale opposite branch. */
	UFUNCTION(BlueprintCallable, Category = "REBIRTH|CH03")
	bool SelectChapterThreeEnding(EIGRebirthEndingChoice EndingChoice);

	/** Atomically commits actual-state restoration and the common discovery. */
	UFUNCTION(BlueprintCallable, Category = "REBIRTH|CH03")
	bool CommitChapterThreeCommonDiscovery();

	/** Clears only the replayable CH03 attempt; upstream truths and choices survive. */
	UFUNCTION(BlueprintCallable, Category = "REBIRTH|CH03")
	void ResetChapterThreeAttempt();

	/**
	 * Returns a stable, atomic save image. P3 keeps its exact pressure so an
	 * in-flight bleed can resume from the saved gauge value. The 0.6 s scratch
	 * tail alone commits forward; ending discovery and strong-cue guards retain
	 * their exact committed boundary.
	 */
	FIGRebirthNarrativeSnapshot BuildSnapshot() const;
	void RestoreSnapshot(const FIGRebirthNarrativeSnapshot& Snapshot);

	UFUNCTION(BlueprintCallable, Category = "REBIRTH|State")
	void ResetNarrative();

	UPROPERTY(BlueprintAssignable, Category = "REBIRTH|Truth")
	FIGRebirthTruthChangedSignature OnTruthChanged;

private:
	FIGRebirthTruthRecord* FindTruthRecord(FGameplayTag TruthTag);
	const FIGRebirthTruthRecord* FindTruthRecord(FGameplayTag TruthTag) const;
	static FGameplayTag Truth(const TCHAR* Leaf);
	void NormalizeState(bool bForStableSave);
	static void NormalizeSnapshot(
		FIGRebirthNarrativeSnapshot& Snapshot,
		bool bForStableSave);

	UPROPERTY(VisibleAnywhere, Category = "REBIRTH")
	FIGRebirthNarrativeSnapshot State;
};
