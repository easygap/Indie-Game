#include "Player/IGCameraSensorComponent.h"

#include "Accessibility/IGAccessibilitySubsystem.h"
#include "Components/PostProcessComponent.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"

namespace IGCameraSensor
{
	const TCHAR* MaterialPath =
		TEXT("/Game/Prototype/Materials/M_PP_CameraSensor.M_PP_CameraSensor");
}

UIGCameraSensorComponent::UIGCameraSensorComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UIGCameraSensorComponent::BeginPlay()
{
	Super::BeginPlay();

	AActor* Owner = GetOwner();
	UMaterialInterface* Material = LoadObject<UMaterialInterface>(
		nullptr, IGCameraSensor::MaterialPath);
	if (!Owner || !Material)
	{
		return;
	}

	SensorMaterial = UMaterialInstanceDynamic::Create(Material, this);
	// 월드 룩(우선순위 0)과 공포 램프(5) 사이에 둔다. 블렌더블은 볼륨마다
	// 더해지므로 순서보다 가중치가 중요하다. 이 볼륨은 늘 1이다.
	SensorPostProcess = NewObject<UPostProcessComponent>(Owner, TEXT("CameraSensorPostProcess"));
	SensorPostProcess->SetupAttachment(Owner->GetRootComponent());
	SensorPostProcess->bUnbound = true;
	SensorPostProcess->Priority = 4.0f;
	SensorPostProcess->BlendWeight = 1.0f;
	SensorPostProcess->Settings.WeightedBlendables.Array.Add(
		FWeightedBlendable(1.0f, SensorMaterial));
	SensorPostProcess->RegisterComponent();

	RefreshFromSettings();
}

void UIGCameraSensorComponent::RefreshFromSettings()
{
	if (!SensorMaterial || !SensorPostProcess)
	{
		return;
	}

	const UWorld* World = GetWorld();
	const UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
	const UIGAccessibilitySubsystem* Accessibility = GameInstance
		? GameInstance->GetSubsystem<UIGAccessibilitySubsystem>()
		: nullptr;
	const float Strength = Accessibility
		? Accessibility->GetCameraTextureStrength()
		: 1.0f;

	// 0이면 블렌더블째 뺀다. 세기 0인 재질도 화면을 한 번 더 그리기 때문이다.
	SensorPostProcess->BlendWeight = Strength > KINDA_SMALL_NUMBER ? 1.0f : 0.0f;
	if (!FMath::IsNearlyEqual(Strength, AppliedStrength))
	{
		SensorMaterial->SetScalarParameterValue(TEXT("Strength"), Strength);
		AppliedStrength = Strength;
	}
}
