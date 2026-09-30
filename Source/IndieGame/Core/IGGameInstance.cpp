#include "Core/IGGameInstance.h"

#include "IndieGame.h"

void UIGGameInstance::Init()
{
	Super::Init();
	RuntimeProfile.Start(this);
	UE_LOG(LogIndieGame, Log, TEXT("IndieGame game instance initialized."));
}

void UIGGameInstance::Shutdown()
{
	RuntimeProfile.Stop();
	UE_LOG(LogIndieGame, Log, TEXT("IndieGame game instance shutting down."));
	Super::Shutdown();
}
