#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
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
	void StartP3Write();
	void StartP3Read();
	void StartEndingWrite();
	void StartEndingRead();
	void PrepareEndingPrerequisites();
	void ExitSuccess(const FString& Marker);
	void ExitFailure(const TCHAR* Reason);

	static FIGRebirthP3State MakeP3Checkpoint(int32 CheckpointIndex);
	static bool MatchesP3Checkpoint(
		const FIGRebirthP3State& Actual,
		const FIGRebirthP3State& Expected);

	UFUNCTION()
	void HandleSaveCompleted(bool bSuccess, FString SlotName);

	UFUNCTION()
	void HandleLoadCompleted(
		bool bSuccess,
		FString SlotName,
		UIGSaveGame* SaveGame);

	FString ProbeMode;
	FString SlotName;
	int32 P3CheckpointIndex = INDEX_NONE;
	bool bEndingA = true;
	bool bSaveAfterEndingCommit = false;
	FTimerHandle StartTimer;
};
