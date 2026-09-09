#pragma once

#include "CoreMinimal.h"
#include "Camera/CameraModifier.h"
#include "IGCameraFeelModifier.generated.h"

/**
 * 손맛 회전을 시점에 얹는 카메라 모디파이어.
 *
 * 노크 킥·포획 킥·공포 떨림은 AIGPlayerCharacter가 프레임마다 계산한다. 예전엔
 * 그 값을 카메라 컴포넌트의 상대 회전에 넣었는데, bUsePawnControlRotation이
 * GetCameraView에서 폰 제어 회전으로 덮어써 한 번도 화면에 나오지 않았다. 카메라
 * 매니저 모디파이어는 그 뒤에 돌아 확실히 보인다. 시점 기준 좌표계에서 곱하므로
 * 피치 킥은 어디를 보고 있든 「아래로 고개가 꺾이는」 느낌으로 온다.
 */
UCLASS()
class INDIEGAME_API UIGCameraFeelModifier : public UCameraModifier
{
	GENERATED_BODY()

public:
	virtual bool ModifyCamera(float DeltaTime, FMinimalViewInfo& InOutPOV) override;
};
