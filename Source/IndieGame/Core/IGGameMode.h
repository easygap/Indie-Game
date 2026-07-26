#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "IGGameMode.generated.h"

/** Base game mode shared by story levels. */
UCLASS()
class INDIEGAME_API AIGGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	AIGGameMode();
};
