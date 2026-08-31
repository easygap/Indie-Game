#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Narrative/IGMissingFloorNarrativeTypes.h"
#include "IGMissingFloorNightThreeDirector.generated.h"

class AIGListenerEntity;
class AIGMissingFloorEvidence;
class AIGPlayerCharacter;
class AIGPrologueWorldScene;
class AIGReadableNote;
class AIGSwingDoor;
class UAudioComponent;
class UStaticMeshComponent;
class UIGMissingFloorNarrativeSubsystem;

DECLARE_MULTICAST_DELEGATE(FIGNightThreeSolvedSignature);
DECLARE_MULTICAST_DELEGATE(FIGNightThreeReturnedSignature);

/** Where beat 3-7 has got to. */
UENUM()
enum class EIGNightThreeReturnStage : uint8
{
	/** T9 is not in yet, or this is not night three. */
	Idle,
	/** He is in the corridor between the stair core and her door. */
	Passing,
	/** Back inside 403. The night's goal is done. */
	Home
};

/**
 * 밤3 「조율」 — the night the player climbs to the floor that is not there
 * (STORY_BIBLE_MISSING_FLOOR.md §7 P3/P4, §8 밤3).
 *
	 * The release route is physical: visible 4F stair -> roof door -> 6.4 m
	 * tank-side passage -> annex door, with no portal actor. The one upstairs
	 * never follows — he cannot enter the room he was walled into, which is why
	 * the fifth floor is the quietest place.
 *
 * P3 is telling three identical walls apart by sound. The tuner's notebook
 * gives the criterion (a wall with resonance is a wall with a cavity); the
 * riser valve gives the patient instrument (water moving behind one bay);
 * a fist gives the reckless one. Either way the middle bay is the answer,
 * and with the criterion it confirms the location recorded by T6.
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

	/**
	 * Spawns the gate, the annex contents and the day papers.
	 *
	 * Also takes the pursuer, the pawn and the corridor route, because §8 비트
	 * 3-7 stages the return past him and has to hand the route back afterwards.
	 */
	bool Configure(
		AIGPrologueWorldScene* InScene,
		AIGListenerEntity* InEntity = nullptr,
		AIGPlayerCharacter* InPlayer = nullptr,
		const TArray<FVector>& InCorridorPatrolPoints = TArray<FVector>());

	/**
	 * 비트 3-7 「귀환길」을 무장한다 — P4의 대답이 돌아온 순간에 불린다.
	 *
	 * §8's night three does not end at the wall either. T9 arms the walk home,
	 * and he is standing in the 4F corridor between the stair core and 403 —
	 * knocking, in a corridor 160 cm deep. She has just been taught 둘-쉬고-하나
	 * by P4; answering freezes him into Waiting and she walks past a thing that
	 * stopped to listen for her. 「회피 대상이 애도 대상으로」.
	 *
	 * Nothing forces the answer. Slipping past him unheard is a legitimate
	 * solution and always was — it simply is not this beat.
	 */
	void ArmReturnPass();

	/** 포획 리셋은 침대로 되돌린다. 그것이 도착으로 세어지면 안 된다. */
	void NotifyCaptureReset();

	/** Fired once, when she is back inside 403. This is what ends night three. */
	FIGNightThreeReturnedSignature OnReturnedHome;

	EIGNightThreeReturnStage GetReturnStage() const { return ReturnStage; }
	/** True while he is standing in the corridor on the way home. */
	bool IsFigureInCorridor() const { return bFigureStaged; }
	/** True once she has walked past him while he was waiting on an answer. */
	bool HasPassedWhileWaiting() const { return bPassedWhileWaiting; }
	FVector GetReturnPassPoint() const;

	/** Day/night boundary: reveals the journal once T7 is known by day. */
	void SetHourActive(bool bHourActive);

	/** Fired once, when T9 crosses — the night-3 goal. */
	FIGNightThreeSolvedSignature OnSolved;

	/**
	 * Context verbs used by the player's dedicated Q/B and listen inputs.
	 * Keeping these outside CompleteInteraction prevents opening a door and
	 * filing one of P4's timed taps from the same E/A press.
	 */
	bool IsPlayerKnockTarget(const AActor* FocusedActor) const;
	bool IsPlayerListenTarget(const AActor* FocusedActor) const;
	bool TryPlayerKnock(AActor* FocusedActor, AActor* NoiseInstigator = nullptr);
	bool TryPlayerListen(AActor* FocusedActor, AActor* NoiseInstigator = nullptr);

	/** Probe queries. */
	bool ValidateFixtures() const;
	AIGSwingDoor* GetStairGate() const { return StairGate; }
	AIGSwingDoor* GetAnnexGate() const { return AnnexGate; }
	AIGMissingFloorEvidence* GetKeyring() const { return Keyring; }
	AIGReadableNote* GetTunerNotebook() const { return TunerNotebook; }
	AIGMissingFloorEvidence* GetRiserValve() const { return RiserValve; }
	AIGMissingFloorEvidence* GetWallListen(int32 BayIndex) const;

	/**
	 * World position of the one bay with the cavity behind it, for §20.3's
	 * observation net. Returns false before the fifth floor is dressed, and
	 * false once P3 is solved — a player who already knows does not need to be
	 * shown, and showing them anyway would read as the game not listening.
	 */
	bool GetCavityWallObservationPoint(FVector& OutLocation) const;
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
	void HandlePlasterDatingExamined(AIGMissingFloorEvidence* Evidence);
	void HandleTankAuditionExamined(AIGMissingFloorEvidence* Evidence);

	/**
	 * Plays what the wall actually sounds like when an ear settles on it: the
	 * cavity's long mass-air-mass ring or a solid board's dead thud, plus the
	 * riser water at whatever band the structure left it in. The monologue that
	 * follows confirms this; it must never be the only thing that says it.
	 */
	void PlayWallListenResponse(int32 BayIndex, bool bHollow);
	void HandleWallKnocked(int32 BayIndex);
	void HandleImpactMarkExamined(AIGMissingFloorEvidence* Evidence);
	void HandleAnswerKnock(AIGMissingFloorEvidence* Evidence);
	void DeliverWallAnswer();
	void EndAnswerSilence();
	void HandleTruthConfirmed(EIGMissingFloorTruth Truth);
	void AdvanceReturn();
	void StageReturnFigure();
	void ReleaseReturnFigure();
	bool IsPlayerInsideUnit403() const;
	void RefreshAnswerTargetAvailability();
	void RefreshJournalAvailability(bool bHourActive);
	void RefreshDistantSeoVisibility();

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
	TWeakObjectPtr<AIGListenerEntity> Entity;

	UPROPERTY(Transient)
	TWeakObjectPtr<AIGPlayerCharacter> Player;

	/** §8 비트 3-7. Handed back to him when the beat lets go of the corridor. */
	TArray<FVector> CorridorPatrolPoints;

	FTimerHandle ReturnTimer;
	EIGNightThreeReturnStage ReturnStage = EIGNightThreeReturnStage::Idle;
	bool bFigureStaged = false;
	bool bPassedWhileWaiting = false;
	bool bWasWestOfHim = false;
	bool bMustLeaveHomeAgain = false;

	UPROPERTY(Transient)
	TObjectPtr<AIGSwingDoor> StairGate;

	UPROPERTY(Transient)
	TObjectPtr<AIGSwingDoor> AnnexGate;

	UPROPERTY(Transient)
	TObjectPtr<AIGMissingFloorEvidence> Keyring;

	UPROPERTY(Transient)
	TObjectPtr<AIGReadableNote> TunerNotebook;

	UPROPERTY(Transient)
	TObjectPtr<AIGMissingFloorEvidence> TuningHammer;

	/** ImageGen-derived, fully 3D workshop cart; never an interactive sprite. */
	UPROPERTY(Transient)
	TObjectPtr<UStaticMeshComponent> TunerToolCart;

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

	/** T5 두 번째 출처. 새 벽이 언제 발라졌는지는 벽 자신이 말한다. */
	UPROPERTY(Transient)
	TObjectPtr<AIGMissingFloorEvidence> PlasterDating;

	/** T8 두 번째 출처. 갈증으로 죽은 사람의 벽 하나 옆에 물 2톤이 있었다. */
	UPROPERTY(Transient)
	TObjectPtr<AIGMissingFloorEvidence> TankAudition;


	UPROPERTY(Transient)
	TObjectPtr<AIGReadableNote> LabelsNote;

	UPROPERTY(Transient)
	TObjectPtr<AIGReadableNote> ForumNote;

	UPROPERTY(Transient)
	TObjectPtr<AIGReadableNote> JournalNote;

	/** Water moving in the riser once the valve opens. */
	UPROPERTY(Transient)
	TObjectPtr<UAudioComponent> RiserFlow;

	/** Day-three fixed-camera figure; never used as a close or interactive NPC. */
	UPROPERTY(Transient)
	TObjectPtr<UStaticMeshComponent> DistantSeo;

	FDelegateHandle TruthHandle;
	FTimerHandle AnswerTimer;
	FTimerHandle AnswerSilenceReleaseTimer;
	TArray<double> AnswerTapTimes;
	bool bValveOpen = false;
	bool bAnswerPending = false;
	bool bAnswerDelivered = false;
	bool bSolvedAnnounced = false;
	bool bHourCurrentlyActive = true;
};
