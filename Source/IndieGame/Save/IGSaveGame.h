#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "GameplayTagContainer.h"
#include "Narrative/IGMissingFloorNarrativeTypes.h"
#include "Narrative/IGRebirthNarrativeTypes.h"
#include "IGSaveGame.generated.h"

USTRUCT(BlueprintType)
struct INDIEGAME_API FIGProgressSnapshot
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category = "Save")
	int32 SchemaVersion = 1;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category = "Save")
	FDateTime SavedAtUtc;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category = "Save")
	FGameplayTag ChapterId;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category = "Save")
	FName MapPackageName = NAME_None;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category = "Save")
	FGameplayTag CheckpointTag;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category = "Save")
	FGameplayTagContainer StoryStateTags;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category = "Save")
	FIGRebirthNarrativeSnapshot RebirthNarrative;

	/**
	 * 없는 층 state, added additively beside the legacy snapshot. Tagged
	 * property serialization leaves this default-constructed in v1..v3 saves,
	 * so no schema bump is needed — and CurrentSchemaVersion must stay 3,
	 * which the compatibility contract pins.
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category = "Save")
	FIGMissingFloorNarrativeSnapshot MissingFloorNarrative;
};

/** Versioned story progress. User settings belong in a separate save object. */
UCLASS()
class INDIEGAME_API UIGSaveGame : public USaveGame
{
	GENERATED_BODY()

public:
	static constexpr int32 CurrentSchemaVersion = 3;

	UIGSaveGame();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category = "Save")
	FIGProgressSnapshot Progress;
};
