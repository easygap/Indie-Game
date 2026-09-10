#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Narrative/IGMissingFloorNarrativeTypes.h"
#include "IGMissingFloorPuzzleOneDirector.generated.h"

class AIGMissingFloorEvidence;
class AIGPrologueWorldScene;
class AIGReadableNote;
class UAudioComponent;
class UIGMissingFloorNarrativeSubsystem;

DECLARE_MULTICAST_DELEGATE(FIGPuzzleOneSolvedSignature);

/**
 * P1 「다섯 번째 바늘」 — the proof that the fifth floor is lived in
 * (STORY_BIBLE_MISSING_FLOOR.md §7).
 *
 * Four unit meters and a fifth with no nameplate whose disc never turns; a
 * reading sheet in the lobby with five columns, the fifth reading zero since
 * July 2024. Those two records cross into T1. The distribution panel's unnamed
 * breaker is the check the player performs themselves: throwing it is loud
 * enough to be heard upstairs, and what answers is a fluorescent ballast
 * starting somewhere above the fourth-floor ceiling.
 *
 * There is no success popup anywhere in here. The world confirms: a hum where
 * no hum should be.
 */
UCLASS(NotBlueprintable, Transient)
class INDIEGAME_API AIGMissingFloorPuzzleOneDirector : public AActor
{
	GENERATED_BODY()

public:
	AIGMissingFloorPuzzleOneDirector();

	/** Spawns the puzzle's interactables against an already-built lobby. */
	bool Configure(AIGPrologueWorldScene* InScene);

	UFUNCTION(BlueprintPure, Category = "Puzzle")
	bool IsBallastHumAudible() const { return bBallastHumAudible; }

	UFUNCTION(BlueprintPure, Category = "Puzzle")
	bool HasBreakerBeenThrown() const { return bBreakerThrown; }

	/**
	 * 낮에는 계전기가 곧장 되돌려서 아무 일도 없다(§7 P1). 위가 켜지는
	 * 것은 그 시간에만이다.
	 */
	void SetHourActive(bool bActive);

	/** Release probes: all three fixtures resolved and placed. */
	bool ValidateFixtures() const;

	/**
	 * 회로가 올라가고 T1까지 맞물렸을 때 한 번. 불만 켜고 계량기를 안 본
	 * 밤은 끝나지 않는다 — 퍼즐은 두 기록의 교차지 버튼이 아니다(§7).
	 */
	FIGPuzzleOneSolvedSignature OnSolved;

protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	void HandleMeterExamined(AIGMissingFloorEvidence* Evidence);
	void HandleBreakerThrown(AIGMissingFloorEvidence* Evidence);

	/** Dynamic delegate target, so it has to be reflected. */
	UFUNCTION()
	void HandleSheetRead(AIGReadableNote* Note, bool bOpened);
	void CreateBallastHum();
	void HandleTruthConfirmed(EIGMissingFloorTruth Truth);
	void AnnounceSolvedIfReady();
	UIGMissingFloorNarrativeSubsystem* GetNarrative() const;

	UPROPERTY(Transient)
	TWeakObjectPtr<AIGPrologueWorldScene> Scene;

	UPROPERTY(Transient)
	TObjectPtr<AIGMissingFloorEvidence> MeterDialEvidence;

	UPROPERTY(Transient)
	TObjectPtr<AIGMissingFloorEvidence> BreakerAction;

	UPROPERTY(Transient)
	TObjectPtr<AIGReadableNote> ReadingSheet;

	/** The hum from above the ceiling. Created silent, started on the throw. */
	UPROPERTY(Transient)
	TObjectPtr<UAudioComponent> BallastHum;

	bool bBreakerThrown = false;
	bool bBallastHumAudible = false;
	bool bHourActive = false;
	bool bSolvedAnnounced = false;
	FDelegateHandle TruthHandle;
};
