#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "IGMissingFloorNightTwoBeatDirector.generated.h"

class AIGListenerEntity;
class AIGMissingFloorEvidence;
class AIGPlayerCharacter;
class AIGPrologueWorldScene;
class UIGMissingFloorNarrativeSubsystem;
class UIGNoiseSubsystem;
class UIGRecordingSubsystem;

/** Where beat 2-1 has got to. */
UENUM()
enum class EIGNightTwoBeatStage : uint8
{
	/** Not night two, or the beat is already spent. */
	Idle,
	/** 04:30 has passed and the first knock is on its way. */
	Opening,
	/** One knock has landed. She has a peephole and a reason to use it. */
	AwaitingPeephole,
	/** She has seen him. The phone on the floor is now the pointed verb. */
	AwaitingPhone,
	/** 노크 3연, 기다림, 끌려가는 소리 — and the tape is running. */
	Answering,
	/** Played. Never again this run. */
	Spent
};

/**
 * 밤2 비트 2-1 「문 하나를 사이에 둔 첫 대면」
 * (STORY_BIBLE_MISSING_FLOOR.md §8 밤2, 【S】0.7).
 *
 * 04:30. 이번엔 노크가 403호 현관문에서 난다. 문구멍 — 복도에 그가 있다.
 * 유담이 폰 녹음을 켜고 문에 대어 둔다. 문 너머 노크 3연, 기다림,
 * 끌려가는 소리.
 *
 * Night one taught the rule from a distance: a thing in the stairwell, a
 * corridor away. This beat removes the distance and leaves one steel door, and
 * it is the first time the player is looked for rather than merely near.
 *
 * It is also §5.5's trigger. The recording rule needs something on the tape
 * worth refusing, and the design's original arming moment is this: she hears
 * him at her own door and decides to make evidence. Before this director the
 * phone was a prop the player had to think of unprompted, so the morning
 * playback could easily contain nothing but her own footsteps — the shape of
 * the beat without its content.
 *
 * Every step is patient rather than gated. A player who never looks through the
 * peephole, or never sets the phone down, still gets the three knocks and the
 * dragging; they simply do not get them on tape. §20.3's rule that the world
 * must keep moving applies to beats as well as to puzzles.
 *
 * The figure outside the door is the real entity, teleported and given a
 * two-point route, exactly as 1-4 stages the stairwell sighting. Opening the
 * door during the beat therefore shows him standing there, because he is.
 */
UCLASS(NotBlueprintable, Transient)
class INDIEGAME_API AIGMissingFloorNightTwoBeatDirector : public AActor
{
	GENERATED_BODY()

public:
	AIGMissingFloorNightTwoBeatDirector();

	/** Spawns the peephole against an already-built 403 and takes the route. */
	bool Configure(
		AIGPrologueWorldScene* InScene,
		AIGListenerEntity* InEntity,
		AIGPlayerCharacter* InPlayer,
		const TArray<FVector>& InCorridorPatrolPoints);

	/** The hour boundary. Night two arms the beat; anything else disarms it. */
	void SetHourActive(bool bHourActive);

	// -- receipts for the probe and the contracts ---------------------------
	EIGNightTwoBeatStage GetStage() const { return Stage; }
	bool HasPlayed() const { return bPlayed; }
	/** True while the figure is standing in the corridor outside 403. */
	bool IsFigureAtDoor() const { return bFigureStaged; }
	AIGMissingFloorEvidence* GetPeephole() const { return Peephole; }
	int32 GetKnockCount() const { return KnockCount; }
	/** True when the phone was running when the three knocks landed. */
	bool WasRecordingDuringAnswer() const { return bRecordedAnswer; }

	/** Harness hook: runs the beat without waiting out its patience timers. */
	void AdvanceForTesting();

	/** 【S】 for this beat, straight from the §8 table. */
	static constexpr float ScareAmount = 0.7f;

protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	void AdvanceStage();
	void EnterStage(EIGNightTwoBeatStage NextStage);
	void PlayFirstKnock();
	void PlayAnswer();
	void PlayDragAway();
	void StageFigure();
	void ReleaseFigure();
	void HandlePeepholeExamined(AIGMissingFloorEvidence* Evidence);

	UIGMissingFloorNarrativeSubsystem* GetNarrative() const;
	UIGNoiseSubsystem* GetNoise() const;
	UIGRecordingSubsystem* GetRecording() const;

	UPROPERTY(Transient)
	TWeakObjectPtr<AIGPrologueWorldScene> Scene;

	UPROPERTY(Transient)
	TWeakObjectPtr<AIGListenerEntity> Entity;

	UPROPERTY(Transient)
	TWeakObjectPtr<AIGPlayerCharacter> Player;

	UPROPERTY(Transient)
	TObjectPtr<AIGMissingFloorEvidence> Peephole;

	TArray<FVector> CorridorPatrolPoints;

	FTimerHandle StageTimer;
	EIGNightTwoBeatStage Stage = EIGNightTwoBeatStage::Idle;
	float StageSeconds = 0.0f;
	int32 KnockCount = 0;
	bool bPlayed = false;
	bool bFigureStaged = false;
	bool bPeepholeSeen = false;
	bool bRecordedAnswer = false;
};
