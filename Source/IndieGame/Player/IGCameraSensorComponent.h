#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "IGCameraSensorComponent.generated.h"

class UMaterialInstanceDynamic;
class UPostProcessComponent;

/**
 * 1인칭 화면에 보급형 카메라의 질감(M_PP_CameraSensor)을 입힌다.
 *
 * 두 참고작이 사실감을 얻는 곳은 화면이다. 모델이 조금 단순해도 렌즈의 휘어짐,
 * 어두운 곳의 노이즈, 번지는 색이 같이 있으면 사람 눈은 「찍힌 화면」으로 읽는다.
 * 세기는 접근성 설정 「화면 질감」을 따르고, 빛 깜빡임 줄이기를 켜면 노이즈를
 * 느리게 뿌린다. 노이즈의 움직임은 재질의 Time이 맡아서 틱이 없다. 설정이
 * 바뀌면 플레이어가 RefreshFromSettings를 부른다.
 */
UCLASS(ClassGroup = (IndieGame))
class INDIEGAME_API UIGCameraSensorComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UIGCameraSensorComponent();

	virtual void BeginPlay() override;

	/** 접근성 설정을 다시 읽어 재질에 넘긴다. */
	void RefreshFromSettings();

	/** 마지막으로 재질에 넘긴 세기. 0이면 효과가 꺼져 있다. */
	float GetAppliedStrength() const { return AppliedStrength; }

private:

	UPROPERTY(Transient)
	TObjectPtr<UPostProcessComponent> SensorPostProcess;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> SensorMaterial;

	float AppliedStrength = -1.0f;
	float AppliedNoise = -1.0f;
	float AppliedNoiseRate = -1.0f;
};
