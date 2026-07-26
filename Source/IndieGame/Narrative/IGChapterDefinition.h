#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "IGChapterDefinition.generated.h"

class UIGStoryBeatDefinition;
class UWorld;

/** Soft-referenced chapter manifest used by loading and save systems. */
UCLASS(BlueprintType)
class INDIEGAME_API UIGChapterDefinition : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	virtual FPrimaryAssetId GetPrimaryAssetId() const override;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Chapter")
	FGameplayTag ChapterId;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Chapter", meta = (AllowedClasses = "/Script/Engine.World", AssetBundles = "Chapter"))
	TSoftObjectPtr<UWorld> StartMap;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Chapter")
	FGameplayTag StartCheckpoint;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Chapter", meta = (AssetBundles = "Chapter"))
	TArray<TSoftObjectPtr<UIGStoryBeatDefinition>> StoryBeats;
};
