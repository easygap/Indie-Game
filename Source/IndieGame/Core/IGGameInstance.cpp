#include "Core/IGGameInstance.h"

#include "IndieGame.h"

void UIGGameInstance::Init()
{
	Super::Init();
	UE_LOG(LogIndieGame, Log, TEXT("IndieGame game instance initialized."));
}

void UIGGameInstance::Shutdown()
{
	UE_LOG(LogIndieGame, Log, TEXT("IndieGame game instance shutting down."));
	Super::Shutdown();
}
