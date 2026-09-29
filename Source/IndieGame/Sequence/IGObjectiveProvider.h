#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "IGObjectiveProvider.generated.h"

/**
 * Marker UObject interface for actors or objects that own the current
 * story objective. Implementations remain native so objective evaluation can
 * stay a small, allocation-free query from the HUD.
 */
UINTERFACE(MinimalAPI)
class UIGObjectiveProvider : public UInterface
{
	GENERATED_BODY()
};

class INDIEGAME_API IIGObjectiveProvider
{
	GENERATED_BODY()

public:
	/** 화면 위쪽에 잠깐 뜨는 목표 한 줄. 표시 언어를 따른다. */
	virtual FText GetObjectiveText() const = 0;

	/**
	 * Normalized progress through the provider's objective sequence.
	 * Inactive providers return zero; a completed sequence returns one.
	 */
	virtual float GetObjectiveProgress() const = 0;
};
