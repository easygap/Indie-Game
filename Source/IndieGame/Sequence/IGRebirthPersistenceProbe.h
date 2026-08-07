#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GameplayTagContainer.h"
#include "Narrative/IGRebirthNarrativeTypes.h"
#include "IGRebirthPersistenceProbe.generated.h"

class UIGSaveGame;

/**
 * Process-boundary release probe.
 *
 * A writer process creates one real SaveGame slot and exits. A later reader
 * process loads the same slot, validates the exact state, and exits. It is
 * spawned only when -IGRebirthPersistenceProbe is present and never exists in
 * an ordinary playthrough.
 */
UCLASS(NotBlueprintable, Transient)
class INDIEGAME_API AIGRebirthPersistenceProbe final : public AActor
{
	GENERATED_BODY()

public:
	AIGRebirthPersistenceProbe();

protected:
	virtual void BeginPlay() override;

private:
	void StartProbe();
	void StartBoundaryWrite(bool bAfterBoundary);
	void StartBoundaryRead(bool bAfterBoundary);
	void StartCatChoiceWrite();
	void StartCatChoiceRead();
	void StartCH02TimeWrite();
	void StartCH02TimeRead();
	void StartP5Write();
	void StartP5Read();
	void StartP3Write();
	void StartP3Read();
	void StartAnchorWrite();
	void StartAnchorRead();
	void ValidateLoadedAnchor();
	void StartEndingWrite();
	void StartEndingRead();
	void PrepareEndingPrerequisites();
	bool WriteResultReceipt(const FString& Receipt) const;
	void ExitSuccess(const FString& Marker);
	void ExitFailure(const TCHAR* Reason);

	static FIGRebirthP3State MakeP3Checkpoint(int32 CheckpointIndex);
	static FIGRebirthP4State MakeP4Checkpoint(int32 CheckpointIndex);
	static FGameplayTagContainer MakeCH02TimeCheckpoint(
		int32 CheckpointIndex);
	static bool MatchesCH02TimeCheckpoint(
		int32 CheckpointIndex,
		const FGameplayTagContainer& Actual,
		const FIGRebirthNarrativeSnapshot& Narrative);
	static FIGRebirthChapterThreeState MakeP5Checkpoint(
		int32 CheckpointIndex);
	static bool MatchesP5Checkpoint(
		int32 CheckpointIndex,
		const FIGRebirthNarrativeSnapshot& Actual);
	static bool MatchesP3Checkpoint(
		const FIGRebirthP3State& Actual,
		const FIGRebirthP3State& Expected);
	static bool MatchesP4Checkpoint(
		const FIGRebirthP4State& Actual,
		const FIGRebirthP4State& Expected);
	bool ResolveCatChoiceContract(FIGRebirthChoiceState& OutChoices) const;
	bool ResolveAnchorContract(
		FGameplayTag& OutChapter,
		FGameplayTag& OutCheckpoint,
		FVector& OutLocation,
		FRotator& OutRotation) const;

	UFUNCTION()
	void HandleSaveCompleted(bool bSuccess, FString SlotName);

	UFUNCTION()
	void HandleLoadCompleted(
		bool bSuccess,
		FString SlotName,
		UIGSaveGame* SaveGame);

	FString ProbeMode;
	FString SlotName;
	FString CatChoiceCase;
	int32 CH02TimeCheckpointIndex = INDEX_NONE;
	int32 P5CheckpointIndex = INDEX_NONE;
	int32 P3CheckpointIndex = INDEX_NONE;
	FString AnchorCase;
	int32 AnchorValidationAttempts = 0;
	bool bEndingA = true;
	bool bSaveAfterEndingCommit = false;
	FTimerHandle StartTimer;
	FTimerHandle AnchorValidationTimer;
};
