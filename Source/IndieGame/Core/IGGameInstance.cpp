#include "Core/IGGameInstance.h"

#include "Engine/Engine.h"
#include "GameFramework/GameUserSettings.h"
#include "IndieGame.h"
#include "Scalability.h"

void UIGGameInstance::Init()
{
	Super::Init();
	if (UGameUserSettings* Settings = GEngine ? GEngine->GetGameUserSettings() : nullptr)
	{
		float CurrentScaleNormalized = 0.0f;
		float CurrentScaleValue = 0.0f;
		float MinScaleValue = 0.0f;
		float MaxScaleValue = 0.0f;
		Settings->GetResolutionScaleInformationEx(
			CurrentScaleNormalized, CurrentScaleValue, MinScaleValue, MaxScaleValue);
		if (!FMath::IsNearlyEqual(CurrentScaleValue, 100.0f))
		{
			// 예전 사용자 설정의 내부 해상도만 보정한다. 품질·프레임 제한·VSync는
			// 저장된 값을 유지하며, 다음 실행에서도 네이티브 해상도로 시작한다.
			Settings->SetResolutionScaleValueEx(100.0f);
			// Init에서는 엔진 초기화가 덜 끝나 일반 적용 함수가 품질 적용을 건너뛴다.
			Scalability::SetQualityLevels(Settings->ScalabilityQuality);
			Settings->ApplyNonResolutionSettings();
			Settings->SaveSettings();
		}
	}
	RuntimeProfile.Start(this);
	UE_LOG(LogIndieGame, Log, TEXT("IndieGame game instance initialized."));
}

void UIGGameInstance::Shutdown()
{
	RuntimeProfile.Stop();
	UE_LOG(LogIndieGame, Log, TEXT("IndieGame game instance shutting down."));
	Super::Shutdown();
}
