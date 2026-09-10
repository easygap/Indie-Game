#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "IGNightOneBeatDirector.generated.h"

class AIGListenerEntity;
class AIGPlayerCharacter;
class AIGPrologueWorldScene;
class AIGZoneTrigger;
class UIGMissingFloorNarrativeSubsystem;
class UIGNoiseSubsystem;

/**
 * 밤1의 두 스크립트 비트 (STORY_BIBLE_MISSING_FLOOR.md §8 밤1).
 *
 * 1-4 첫 목격 — the stair-throat zone stages the one upstairs on the new
 * 3.5F half-landing, past the moved teleport line, ear to the far wall. The
 * player descending to the lobby walks the landing and passes within arm's
 * reach of it; because capture only exists in its pursuit states, standing
 * still beside a listening thing is survivable — which is the lesson.
 *
 * 1-5 강제 조우 — the fire-cabinet zone knocks the extinguisher off its
 * bracket. The clatter is a 0.6 sound the whole corridor hears, INVESTIGATE
 * plays out on the real AI with no bespoke chase code, and the distribution
 * board's hum pocket is the escape the player is meant to discover. Either
 * outcome — slipping away or being caught and reset — teaches the rule.
 *
 * Both beats are once-per-run narrative facts (MarkBeatPlayed), so a capture
 * reset does not replay them as jump scares.
 */
UCLASS(NotBlueprintable, Transient)
class INDIEGAME_API AIGNightOneBeatDirector : public AActor
{
	GENERATED_BODY()

public:
	static FVector GetSightingZoneCenter();
	static FVector GetSightingStagePoint();
	static FVector GetSightingShufflePoint();
	AIGNightOneBeatDirector();

	/** Arms both beat zones against an already-built corridor. */
	bool Configure(
		AIGPrologueWorldScene* InScene,
		AIGListenerEntity* InEntity,
		AIGPlayerCharacter* InPlayer,
		const TArray<FVector>& InCorridorPatrolPoints);

	/** Probe queries. */
	bool IsSightingStaged() const { return bSightingStaged; }
	bool HasSightingCompleted() const { return bSightingCompleted; }
	bool HasExtinguisherBeatFired() const { return bExtinguisherBeatFired; }

protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	UFUNCTION()
	void HandleSightingZone(AIGZoneTrigger* Zone);

	UFUNCTION()
	void HandleExtinguisherZone(AIGZoneTrigger* Zone);

	UFUNCTION()
	void HandleStairTransitionCompleted(bool bGoingDown);

	void StageSighting();
	void RestoreSightingEntity();
	void PlayExtinguisherImpact();
	UIGMissingFloorNarrativeSubsystem* GetNarrative() const;

	UPROPERTY(Transient)
	TWeakObjectPtr<AIGPrologueWorldScene> Scene;

	UPROPERTY(Transient)
	TWeakObjectPtr<AIGListenerEntity> Entity;

	UPROPERTY(Transient)
	TWeakObjectPtr<AIGPlayerCharacter> Player;
	/** 소화기 뒤 복도 등이 한 번 죽는다. 한 밤에 한 번. */
	FTimerHandle FixtureDeathTimer;
	bool bFixtureDeathFired = false;
	void KillFixtureBehindPlayer();

	UPROPERTY(Transient)
	TObjectPtr<AIGZoneTrigger> SightingZone;

	UPROPERTY(Transient)
	TObjectPtr<AIGZoneTrigger> ExtinguisherZone;

	/** The route the entity returns to once its cameo on the landing ends. */
	TArray<FVector> CorridorPatrolPoints;

	int32 BreakerPanelHumHandle = 0;

	/** 배전반이 내는 소리. 마스킹과 같은 반경까지만 들린다. */
	UPROPERTY(Transient)
	TObjectPtr<UAudioComponent> BreakerPanelHumLoop;
	bool bSightingStaged = false;
	bool bSightingCompleted = false;
	bool bExtinguisherBeatFired = false;
	FTimerHandle SightingFallbackTimer;
	FTimerHandle SightingStepTimer;
	/** 계단 입구 위의 등. 그가 계단참에 있는 동안 죽어 있고, 끝나면 돌아온다. */
	int32 SightingThroatFixture = INDEX_NONE;
	FTimerHandle ImpactTimer;
};
