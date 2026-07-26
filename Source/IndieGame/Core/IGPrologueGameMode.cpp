#include "Core/IGPrologueGameMode.h"

#include "Core/IGPrologueWorldScene.h"
#include "Engine/World.h"
#include "Player/IGHorrorHUD.h"

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

	FActorSpawnParameters SpawnParameters;
	SpawnParameters.Name = TEXT("PrologueWorldScene");
	SpawnParameters.SpawnCollisionHandlingOverride =
		ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	WorldScene = World->SpawnActor<AIGPrologueWorldScene>(
		AIGPrologueWorldScene::StaticClass(),
		FTransform::Identity,
		SpawnParameters);
}
