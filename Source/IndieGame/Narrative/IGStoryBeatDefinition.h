#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "IGStoryBeatDefinition.generated.h"

class ULevelSequence;
class USoundBase;

/** Data-only story cue that can be evaluated without a hard chapter dependency. */
UCLASS(BlueprintType)
class INDIEGAME_API UIGStoryBeatDefinition : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	virtual FPrimaryAssetId GetPrimaryAssetId() const override;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Story")
	FGameplayTag BeatId;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Story")
	FGameplayTagContainer RequiredStates;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Story")
	FGameplayTagContainer GrantedStates;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Story")
	bool bTriggerOnlyOnce = true;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Presentation", meta = (AssetBundles = "Scene"))
	TSoftObjectPtr<ULevelSequence> Sequence;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Presentation", meta = (AssetBundles = "Scene"))
	TSoftObjectPtr<USoundBase> CueSound;
};
