#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Narrative/IGMissingFloorNarrativeTypes.h"
#include "IGMissingFloorNightThreeDirector.generated.h"

class AIGMissingFloorEvidence;
class AIGPrologueWorldScene;
class AIGReadableNote;
class AIGStairTransition;
class AIGSwingDoor;
class UAudioComponent;
class UIGMissingFloorNarrativeSubsystem;

DECLARE_MULTICAST_DELEGATE(FIGNightThreeSolvedSignature);

/**
 * 밤3 「조율」 — the night the player climbs to the floor that is not there
 * (STORY_BIBLE_MISSING_FLOOR.md §7 P3/P4, §8 밤3).
 *
 * The route: the stair keyring hangs in the booth (open only during the
 * hour), the fifth-floor gate at the top of the stub honors it, and the
 * stair teleport behind the gate carries the player to the annex. The one
 * upstairs never follows — he cannot enter the room he was walled into,
 * which is why the fifth floor is the quietest place in the game.
 *
 * P3 is telling three identical walls apart by sound. The tuner's notebook
 * gives the criterion (a wall with resonance is a wall with a cavity); the
 * riser valve gives the patient instrument (water moving behind one bay);
 * a fist gives the reckless one. Either way the middle bay is the answer,
 * and with the criterion it confirms T6.
 *
 * P4 is the answer. With T6 known and the family rhythm learned, the wall
 * accepts 둘-쉬고-하나 — and after eight seconds of nothing, it comes back
 * through the studs. That is T9, and the night's goal.
 *
 * The director also owns the daytime truth papers (shipping labels, the
 * forum printout, Hwang Sun-geum's tally journal) because they feed the
 * same truth board this night completes.
 */
UCLASS(NotBlueprintable, Transient)
class INDIEGAME_API AIGMissingFloorNightThreeDirector : public AActor
{
	GENERATED_BODY()

public:
	AIGMissingFloorNightThreeDirector();

	/** Spawns the gate, the annex contents and the day papers. */
	bool Configure(AIGPrologueWorldScene* InScene);

	/** Day/night boundary: reveals the journal once T7 is known by day. */
	void SetHourActive(bool bHourActive);

	/** Fired once, when T9 crosses — the night-3 goal. */
	FIGNightThreeSolvedSignature OnSolved;

	/** Probe queries. */
	bool ValidateFixtures() const;
	AIGSwingDoor* GetStairGate() const { return StairGate; }
	AIGMissingFloorEvidence* GetKeyring() const { return Keyring; }
	AIGReadableNote* GetTunerNotebook() const { return TunerNotebook; }
	AIGMissingFloorEvidence* GetRiserValve() const { return RiserValve; }
	AIGMissingFloorEvidence* GetWallListen(int32 BayIndex) const;
	AIGMissingFloorEvidence* GetImpactMark() const { return ImpactMark; }
	AIGMissingFloorEvidence* GetAnswerTarget() const { return AnswerTarget; }
	AIGReadableNote* GetLabelsNote() const { return LabelsNote; }
	AIGReadableNote* GetForumNote() const { return ForumNote; }
	AIGReadableNote* GetJournalNote() const { return JournalNote; }
	bool IsValveOpen() const { return bValveOpen; }

protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	void HandleKeyringTaken(AIGMissingFloorEvidence* Evidence);
	void HandleValveOpened(AIGMissingFloorEvidence* Evidence);
	void HandleWallListened(int32 BayIndex);
	void HandleWallKnocked(int32 BayIndex);
	void HandleImpactMarkExamined(AIGMissingFloorEvidence* Evidence);
	void HandleAnswerKnock(AIGMissingFloorEvidence* Evidence);
	void DeliverWallAnswer();
	void HandleTruthConfirmed(EIGMissingFloorTruth Truth);
	void RefreshAnswerTargetAvailability();
	void RefreshJournalAvailability(bool bHourActive);

	UFUNCTION()
	void HandleNotebookRead(AIGReadableNote* Note, bool bOpened);

	UFUNCTION()
	void HandleLabelsRead(AIGReadableNote* Note, bool bOpened);

	UFUNCTION()
	void HandleForumRead(AIGReadableNote* Note, bool bOpened);

	UFUNCTION()
	void HandleJournalRead(AIGReadableNote* Note, bool bOpened);

	UIGMissingFloorNarrativeSubsystem* GetNarrative() const;

	UPROPERTY(Transient)
	TWeakObjectPtr<AIGPrologueWorldScene> Scene;

	UPROPERTY(Transient)
	TObjectPtr<AIGSwingDoor> StairGate;

	UPROPERTY(Transient)
	TObjectPtr<AIGStairTransition> AnnexTransition;

	UPROPERTY(Transient)
	TObjectPtr<AIGMissingFloorEvidence> Keyring;

	UPROPERTY(Transient)
	TObjectPtr<AIGReadableNote> TunerNotebook;

	UPROPERTY(Transient)
	TObjectPtr<AIGMissingFloorEvidence> TunerWand;

	UPROPERTY(Transient)
	TObjectPtr<AIGMissingFloorEvidence> RiserValve;

	UPROPERTY(Transient)
	TArray<TObjectPtr<AIGMissingFloorEvidence>> WallListens;

	UPROPERTY(Transient)
	TArray<TObjectPtr<AIGMissingFloorEvidence>> WallKnocks;

	UPROPERTY(Transient)
	TObjectPtr<AIGMissingFloorEvidence> ImpactMark;

	UPROPERTY(Transient)
	TObjectPtr<AIGMissingFloorEvidence> AnswerTarget;

	UPROPERTY(Transient)
	TObjectPtr<AIGReadableNote> LabelsNote;

	UPROPERTY(Transient)
	TObjectPtr<AIGReadableNote> ForumNote;

	UPROPERTY(Transient)
	TObjectPtr<AIGReadableNote> JournalNote;

	/** Water moving in the riser once the valve opens. */
	UPROPERTY(Transient)
	TObjectPtr<UAudioComponent> RiserFlow;

	FDelegateHandle TruthHandle;
	FTimerHandle AnswerTimer;
	bool bValveOpen = false;
	bool bAnswerPending = false;
	bool bAnswerDelivered = false;
	bool bSolvedAnnounced = false;
};
