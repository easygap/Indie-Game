#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"

class UIGStoryStateSubsystem;

/** Convenience accessors for the persistent story state subsystem. */
namespace IGStory
{
	INDIEGAME_API UIGStoryStateSubsystem* GetStorySubsystem(const UObject* WorldContext);

	/** Returns false when the tag is invalid or the subsystem is unavailable. */
	INDIEGAME_API bool HasState(const UObject* WorldContext, const FGameplayTag& StateTag);

	/** No-op for invalid tags. Returns true when the state is newly added. */
	INDIEGAME_API bool AddState(const UObject* WorldContext, const FGameplayTag& StateTag);
}
