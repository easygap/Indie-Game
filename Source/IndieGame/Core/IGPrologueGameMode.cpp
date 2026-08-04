#include "Core/IGPrologueGameMode.h"

#include "Core/IGPrologueWorldScene.h"
#include "Engine/World.h"
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
}
