#include "Core/IGPrologueGameMode.h"

#include "Core/IGPrologueWorldScene.h"
#include "Engine/World.h"
#include "Entity/IGListenerGreyboxDirector.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Player/IGHorrorHUD.h"
#include "Sequence/IGRebirthPersistenceProbe.h"

AIGPrologueGameMode::AIGPrologueGameMode()
{
	HUDClass = AIGHorrorHUD::StaticClass();
}

void AIGPrologueGameMode::StartPlay()
{
	Super::StartPlay();

	UWorld* World = GetWorld();
	if (!World || IsValid(WorldScene))
	{
		return;
	}

	FString PersistenceProbeMode;
	if (FParse::Value(
			FCommandLine::Get(),
			TEXT("IGRebirthPersistenceProbe="),
			PersistenceProbeMode))
	{
		const bool bBuildWorldForProbe =
			(PersistenceProbeMode.Equals(
				TEXT("AnchorRead"),
				ESearchCase::IgnoreCase)
				&& World->URL.HasOption(TEXT("IGResumeSave")))
			|| PersistenceProbeMode.Equals(
				TEXT("CatChoiceRead"),
				ESearchCase::IgnoreCase);
		FActorSpawnParameters ProbeParameters;
		ProbeParameters.Name = TEXT("RebirthPersistenceProbe");
		ProbeParameters.SpawnCollisionHandlingOverride =
			ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		World->SpawnActor<AIGRebirthPersistenceProbe>(
			AIGRebirthPersistenceProbe::StaticClass(),
			FTransform::Identity,
			ProbeParameters);
		if (!bBuildWorldForProbe)
		{
			return;
		}
	}

	FActorSpawnParameters SpawnParameters;
	SpawnParameters.Name = TEXT("PrologueWorldScene");
	SpawnParameters.SpawnCollisionHandlingOverride =
		ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	WorldScene = World->SpawnActor<AIGPrologueWorldScene>(
		AIGPrologueWorldScene::StaticClass(),
		FTransform::Identity,
		SpawnParameters);

	// 프런트엔드에서 선택한 실제 게임은 "없는 층"으로 시작한다. 기존 그레이박스
	// 플래그는 야간 장면을 바로 검수할 때만 쓰고, 패키지 사용자는 실행 인자를
	// 입력하지 않아도 같은 경로로 진입해야 한다.
	if (FParse::Param(FCommandLine::Get(), TEXT("IGListenerGreybox"))
		|| World->URL.HasOption(TEXT("IGListenerGreybox"))
		|| FParse::Param(FCommandLine::Get(), TEXT("IGMissingFloor"))
		|| World->URL.HasOption(TEXT("IGMissingFloor")))
	{
		FActorSpawnParameters GreyboxParameters;
		GreyboxParameters.Name = TEXT("ListenerGreyboxDirector");
		GreyboxParameters.SpawnCollisionHandlingOverride =
			ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		World->SpawnActor<AIGListenerGreyboxDirector>(
			AIGListenerGreyboxDirector::StaticClass(),
			FTransform::Identity,
			GreyboxParameters);
	}
}
