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
	/** Localized objective shown when the HUD has a Korean-capable font. */
	virtual FText GetObjectiveText() const = 0;

	/** ASCII fallback used when the localized font cannot be loaded. */
	virtual FString GetObjectiveTextAscii() const = 0;

	/**
	 * Normalized progress through the provider's objective sequence.
	 * Inactive providers return zero; a completed sequence returns one.
	 */
	virtual float GetObjectiveProgress() const = 0;
};
