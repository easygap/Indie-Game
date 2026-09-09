#include "Player/IGCameraFeelModifier.h"

#include "Camera/PlayerCameraManager.h"
#include "GameFramework/PlayerController.h"
#include "Player/IGPlayerCharacter.h"

bool UIGCameraFeelModifier::ModifyCamera(const float DeltaTime, FMinimalViewInfo& InOutPOV)
{
	const APlayerController* Controller =
		CameraOwner ? CameraOwner->GetOwningPlayerController() : nullptr;
	const AIGPlayerCharacter* Player =
		Controller ? Cast<AIGPlayerCharacter>(Controller->GetPawn()) : nullptr;
	if (!Player)
	{
		return false;
	}
	// 감독이 다른 액터를 시점으로 잡은 동안은 폰의 손맛을 얹지 않는다.
	if (Controller->GetViewTarget() != Player)
	{
		return false;
	}
	const FRotator Offset = Player->GetCameraFeelRotation();
	if (Offset.IsNearlyZero())
	{
		return false;
	}
	InOutPOV.Rotation = (FQuat(InOutPOV.Rotation) * FQuat(Offset)).Rotator();
	return false;
}
