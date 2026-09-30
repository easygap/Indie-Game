#pragma once

#include "CoreMinimal.h"
#include "Engine/GameInstance.h"
#include "Core/IGRuntimeProfile.h"
#include "IGGameInstance.generated.h"

/** 맵 전환 동안 유지하는 게임 실행 상태. */
UCLASS()
class INDIEGAME_API UIGGameInstance : public UGameInstance
{
	GENERATED_BODY()

public:
	virtual void Init() override;
	virtual void Shutdown() override;
	void SetRuntimeProfileStage(int32 Stage) { RuntimeProfile.SetStage(Stage); }

private:
	FIGRuntimeProfile RuntimeProfile;
};
