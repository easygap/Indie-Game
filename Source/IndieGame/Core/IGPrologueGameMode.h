#pragma once

#include "CoreMinimal.h"
#include "Core/IGGameMode.h"
#include "IGPrologueGameMode.generated.h"

class AIGPrologueWorldScene;

/**
 * Map-specific game mode for the prologue morning routine.
 * The generated world is intentionally isolated here so future story maps do
 * not inherit prototype geometry from the project's shared game mode.
 *
 * Verification/promo capture is handled by AIGDemoDirector, which the scene
 * spawns when -IGCapture / -IGDemo / -IGDemoFrames is on the command line:
 * the pawn physically walks the whole routine instead of teleporting.
 */
UCLASS()
class INDIEGAME_API AIGPrologueGameMode : public AIGGameMode
{
	GENERATED_BODY()

public:
	AIGPrologueGameMode();
	virtual void StartPlay() override;

private:
	UPROPERTY(Transient)
	TObjectPtr<AIGPrologueWorldScene> WorldScene;
};
