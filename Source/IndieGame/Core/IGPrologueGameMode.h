#pragma once

#include "CoreMinimal.h"
#include "Core/IGGameMode.h"
#include "IGPrologueGameMode.generated.h"

class AIGPrologueWorldScene;

/**
 * Map-specific game mode for the prologue morning routine.
 * The generated world is intentionally isolated here so future story maps do
 * not inherit prototype geometry from the project's shared game mode.
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
