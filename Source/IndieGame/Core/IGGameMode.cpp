#include "Core/IGGameMode.h"

#include "Player/IGPlayerCharacter.h"
#include "Player/IGPlayerController.h"

AIGGameMode::AIGGameMode()
{
	PrimaryActorTick.bCanEverTick = false;
	DefaultPawnClass = AIGPlayerCharacter::StaticClass();
	PlayerControllerClass = AIGPlayerController::StaticClass();
}
