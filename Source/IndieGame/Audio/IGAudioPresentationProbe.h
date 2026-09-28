#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "IGAudioPresentationProbe.generated.h"

class AIGListenerEntity;
class AIGPlayerCharacter;
class UIGMissingFloorAudioSubsystem;
class UIGStressComponent;
class UAudioComponent;
enum class EIGAudioBus : uint8;

/** 실제 믹서·충돌·상태 전환을 검사한다. -IGAudioPresentationProbe로만 실행한다. */
UCLASS()
class INDIEGAME_API AIGAudioPresentationProbe : public AActor
{
	GENERATED_BODY()
public:
	AIGAudioPresentationProbe();
	virtual void Tick(float DeltaSeconds) override;
private:
	void Check(bool bCondition, const TCHAR* Name);
	void CheckVisibility();
	void CheckChaseWave();
	void CheckMixerGain(EIGAudioBus Bus, float ExpectedDecibels, const TCHAR* Name);
	/** 발음 상한이 아직 이 소리를 세고 있는지. 밀려난 소리는 목록에서 빠진다. */
	bool IsTrackedVoice(const UAudioComponent* Component, EIGAudioBus Bus) const;
	TWeakObjectPtr<AIGPlayerCharacter> Player;
	TWeakObjectPtr<AIGListenerEntity> Entity;
	TWeakObjectPtr<UIGMissingFloorAudioSubsystem> Audio;
	TWeakObjectPtr<UIGStressComponent> Stress;
	TWeakObjectPtr<UAudioComponent> Chase;
	TWeakObjectPtr<UAudioComponent> InterruptedBreath;
	float Seconds = 0;
	int32 Phase = 0;
	int32 Failures = 0;
};
