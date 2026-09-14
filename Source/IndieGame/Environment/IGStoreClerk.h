#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "IGStoreClerk.generated.h"

/** 계산대 안에서 손님을 바라보는 나린. 가려진 동안에는 포즈 계산을 쉰다. */
UCLASS()
class INDIEGAME_API AIGStoreClerk : public AActor
{

	GENERATED_BODY()
public:
	AIGStoreClerk();
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
private:
	void UpdateCustomerFacing();
	FTimerHandle FacingTimer;
	UPROPERTY() TObjectPtr<class USkeletalMeshComponent> Body;
};
