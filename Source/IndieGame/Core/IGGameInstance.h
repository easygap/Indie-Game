#pragma once

#include "CoreMinimal.h"
#include "Engine/GameInstance.h"
#include "IGGameInstance.generated.h"

/**
 * Persistent application-level state for IndieGame.
 * Story progression and save services can be added here without coupling them to a level.
 */
UCLASS()
class INDIEGAME_API UIGGameInstance : public UGameInstance
{
	GENERATED_BODY()

public:
	virtual void Init() override;
	virtual void Shutdown() override;
};
