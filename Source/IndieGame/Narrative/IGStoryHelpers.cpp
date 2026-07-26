#include "Narrative/IGStoryHelpers.h"

#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Narrative/IGStoryStateSubsystem.h"

namespace IGStory
{
	UIGStoryStateSubsystem* GetStorySubsystem(const UObject* WorldContext)
	{
		const UWorld* World = GEngine && WorldContext
			? GEngine->GetWorldFromContextObject(WorldContext, EGetWorldErrorMode::ReturnNull)
			: nullptr;
		UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
		return GameInstance ? GameInstance->GetSubsystem<UIGStoryStateSubsystem>() : nullptr;
	}

	bool HasState(const UObject* WorldContext, const FGameplayTag& StateTag)
	{
		if (!StateTag.IsValid())
		{
			return false;
		}

		const UIGStoryStateSubsystem* StoryState = GetStorySubsystem(WorldContext);
		return StoryState && StoryState->HasState(StateTag);
	}

	bool AddState(const UObject* WorldContext, const FGameplayTag& StateTag)
	{
		if (!StateTag.IsValid())
		{
			return false;
		}

		UIGStoryStateSubsystem* StoryState = GetStorySubsystem(WorldContext);
		return StoryState && StoryState->AddState(StateTag);
	}
}
