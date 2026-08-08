#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Narrative/IGMissingFloorNarrativeTypes.h"
#include "IGMissingFloorPuzzleTwoDirector.generated.h"

class AIGMissingFloorEvidence;
class AIGPrologueWorldScene;
class AIGReadableNote;
class AIGSwingDoor;
class UIGMissingFloorNarrativeSubsystem;

DECLARE_MULTICAST_DELEGATE(FIGPuzzleTwoSolvedSignature);

/**
 * P2 「먹지 원장」 — the night the paper contradicts itself
 * (STORY_BIBLE_MISSING_FLOOR.md §7 P2, §8 밤2).
 *
 * The management booth's fair-copy ledger says "물탱크 배관 소음. 조치
 * 완료." Under it sits the carbon pad, and pressure does not lie: three
 * passes of frottage — each a sustained sound the one upstairs can hear —
 * restore the original complaints, dated after the day the tenant
 * "moved out". Crossed with the estate agent's move-out message, that is
 * T7: whatever was in the wall was still alive.
 *
 * The booth also holds the two beats that need no puzzle: the CCTV monitor
 * whose channel selector has one button too many, and the inner room's
 * door gap lined with egg-crate foam. Neither files evidence — they are
 * the night's images, played once.
 *
 * The booth door is the day/night valve: locked while Mok Hansu keeps his
 * daytime post, open in the hour when he hides in the soundproofed room.
 */
UCLASS(NotBlueprintable, Transient)
class INDIEGAME_API AIGMissingFloorPuzzleTwoDirector : public AActor
{
	GENERATED_BODY()

public:
	AIGMissingFloorPuzzleTwoDirector();

	/** Spawns the booth door and its contents against a built lobby. */
	bool Configure(AIGPrologueWorldScene* InScene);

	/** Day locks the booth; the hour opens it. */
	void SetHourActive(bool bHourActive);

	/** Fired once, when T7 crosses — the night-2 goal. */
	FIGPuzzleTwoSolvedSignature OnSolved;

	/** Probe queries. */
	bool ValidateFixtures() const;
	AIGMissingFloorEvidence* GetCarbonLedger() const { return CarbonLedger; }
	AIGReadableNote* GetAgentMessageNote() const { return AgentMessageNote; }
	AIGMissingFloorEvidence* GetCctvSelector() const { return CctvSelector; }

protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	void HandleCarbonRestored(AIGMissingFloorEvidence* Evidence);
	void HandleCctvExamined(AIGMissingFloorEvidence* Evidence);
	void HandleFoamExamined(AIGMissingFloorEvidence* Evidence);
	void HandleTruthConfirmed(EIGMissingFloorTruth Truth);

	UFUNCTION()
	void HandleAgentNoteRead(AIGReadableNote* Note, bool bOpened);

	UIGMissingFloorNarrativeSubsystem* GetNarrative() const;

	UPROPERTY(Transient)
	TWeakObjectPtr<AIGPrologueWorldScene> Scene;

	UPROPERTY(Transient)
	TObjectPtr<AIGSwingDoor> BoothDoor;

	UPROPERTY(Transient)
	TObjectPtr<AIGReadableNote> FairCopyLedger;

	UPROPERTY(Transient)
	TObjectPtr<AIGMissingFloorEvidence> CarbonLedger;

	UPROPERTY(Transient)
	TObjectPtr<AIGReadableNote> AgentMessageNote;

	UPROPERTY(Transient)
	TObjectPtr<AIGMissingFloorEvidence> CctvSelector;

	UPROPERTY(Transient)
	TObjectPtr<AIGMissingFloorEvidence> FoamGap;

	FDelegateHandle TruthHandle;
	bool bSolvedAnnounced = false;
};
