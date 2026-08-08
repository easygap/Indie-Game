#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "IGNightLoopDirector.generated.h"

class AIGListenerEntity;
class AIGPlayerCharacter;

/**
 * Owns the capture consequence of the hour: being caught is not a game-over
 * screen but a reset to the half-past-four bed. Fade to black over the
 * entity's close double knock, put the player back at the wake point, raise
 * the entity's aggression tier, fade back in.
 *
 * What survives a reset and what rolls back is defined in
 * STORY_BIBLE_MISSING_FLOOR.md §5.4. This first cut resets position only;
 * door/prop rollback and the plaster handprint decal come with M2.
 */
UCLASS()
class INDIEGAME_API AIGNightLoopDirector : public AActor
{
	GENERATED_BODY()

public:
	AIGNightLoopDirector();

	/** Where a caught player wakes: the bed side of unit 403. */
	UFUNCTION(BlueprintCallable, Category = "NightLoop")
	void SetWakeTransform(const FTransform& Transform);

	UFUNCTION(BlueprintCallable, Category = "NightLoop")
	void RegisterEntity(AIGListenerEntity* Entity);

	UFUNCTION(BlueprintPure, Category = "NightLoop")
	int32 GetCaptureCount() const { return CaptureCount; }

protected:
	virtual void BeginPlay() override;

	/** Blackout length before the player is moved, in seconds. */
	UPROPERTY(EditAnywhere, Category = "NightLoop", meta = (ClampMin = "0.0"))
	float FadeOutSeconds = 0.9f;

	/** Fade back up at the bed, in seconds. */
	UPROPERTY(EditAnywhere, Category = "NightLoop", meta = (ClampMin = "0.0"))
	float FadeInSeconds = 1.6f;

private:
	void HandlePlayerCaptured(APawn* Player);
	void FinishReset();
	class UIGMissingFloorNarrativeSubsystem* GetNarrative() const;

	UPROPERTY(Transient)
	TWeakObjectPtr<AIGListenerEntity> ListenerEntity;

	UPROPERTY(Transient)
	TWeakObjectPtr<AIGPlayerCharacter> CapturedPlayer;

	FTransform WakeTransform = FTransform::Identity;
	bool bWakeTransformSet = false;
	bool bResetInFlight = false;
	int32 CaptureCount = 0;
	FTimerHandle ResetTimer;
};
