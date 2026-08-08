#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Sequence/IGObjectiveProvider.h"
#include "IGNightPhaseDirector.generated.h"

class AIGPrologueWorldScene;
class AIGPlayerCharacter;
class UIGMissingFloorNarrativeSubsystem;

/**
 * Owner of '그 시간' — the hour from half past four to half past five in which
 * 없는 층 actually takes place (STORY_BIBLE_MISSING_FLOOR.md §1).
 *
 * There is no clock anywhere in this project: every "04:44" in the codebase is
 * a string on a note or a receipt. So this director keeps its own elapsed
 * seconds and maps them onto the hour, and it is the single authority for when
 * the building is shut.
 *
 * While the hour holds, the envelope is sealed (the common entrance refuses,
 * the lift is dead, the connector shutter is down), the phone has no signal,
 * and the HUD shows no objective — the player is told nothing and has to
 * listen. Morning arrives either when the hour runs out or when the night's
 * goal is met, and it arrives as the world opening rather than as a notice.
 */
UCLASS(NotBlueprintable, Transient)
class INDIEGAME_API AIGNightPhaseDirector
	: public AActor
	, public IIGObjectiveProvider
{
	GENERATED_BODY()

public:
	AIGNightPhaseDirector();

	/** Binds the scene whose envelope this director seals. */
	void Configure(AIGPrologueWorldScene* InScene, AIGPlayerCharacter* InPlayer);

	/** Starts the hour: seals the building and begins counting. */
	UFUNCTION(BlueprintCallable, Category = "Night")
	void BeginTheHour(int32 NightIndex);

	/**
	 * Ends the hour early because the night's goal was met. Morning is the
	 * same exit as the timeout, so both routes leave identical world state.
	 */
	UFUNCTION(BlueprintCallable, Category = "Night")
	void CompleteNightGoal();

	UFUNCTION(BlueprintPure, Category = "Night")
	bool IsHourActive() const { return bHourActive; }

	UFUNCTION(BlueprintPure, Category = "Night")
	float GetHourElapsedSeconds() const { return HourElapsedSeconds; }

	/** Minutes left in story time, for the objective line outside the hour. */
	UFUNCTION(BlueprintPure, Category = "Night")
	int32 GetStoryMinutesRemaining() const;

	// IIGObjectiveProvider
	virtual FText GetObjectiveText() const override;
	virtual FString GetObjectiveTextAscii() const override;
	virtual float GetObjectiveProgress() const override;

	/**
	 * Real seconds the hour lasts. Story time runs 04:30 to 05:30, but the
	 * story hour compresses to twenty real minutes: the timeout is a mercy
	 * for a stuck player, and sixty real minutes of fallback would be a wall,
	 * not a mercy. Nights normally end early through CompleteNightGoal.
	 */
	static constexpr float HourDurationSeconds = 1200.0f;
	static constexpr int32 StoryStartMinutes = 4 * 60 + 30;
	static constexpr int32 StoryEndMinutes = 5 * 60 + 30;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	void TickHour();
	void ReleaseAtDawn();
	void ApplySealedPresentation(bool bSealed);
	UIGMissingFloorNarrativeSubsystem* GetNarrative() const;

	UPROPERTY(Transient)
	TWeakObjectPtr<AIGPrologueWorldScene> Scene;

	UPROPERTY(Transient)
	TWeakObjectPtr<AIGPlayerCharacter> Player;

	float HourElapsedSeconds = 0.0f;
	bool bHourActive = false;
	bool bGoalComplete = false;
	FTimerHandle HourTimer;
};
