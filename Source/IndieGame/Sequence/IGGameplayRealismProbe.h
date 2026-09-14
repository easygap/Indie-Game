#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "IGGameplayRealismProbe.generated.h"

class AIGPlayerCharacter;
class APlayerController;
class UBoxComponent;

/** -IGGameplayRealismProbe 전용. 실제 입력·충돌·포획을 실행하고 종료한다. */
UCLASS()
class INDIEGAME_API AIGGameplayRealismProbe : public AActor
{
	GENERATED_BODY()
public:
	AIGGameplayRealismProbe();
	virtual void Tick(float DeltaSeconds) override;

private:
	void SendKey(const FKey& Key, bool bPressed);
	void Check(bool bCondition, const TCHAR* Name);
	void CheckInteractionsAndCapture();
	UBoxComponent* AddBlock(const FVector& Location, const FVector& Extent);
	TWeakObjectPtr<AIGPlayerCharacter> Player;
	TWeakObjectPtr<APlayerController> Controller;
	FVector BrakeStart = FVector::ZeroVector;
	float Seconds = 0.0f;
	int32 Phase = 0;
	int32 Failures = 0;
};
