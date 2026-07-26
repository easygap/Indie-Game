#include "Narrative/IGStoryBeatDefinition.h"

FPrimaryAssetId UIGStoryBeatDefinition::GetPrimaryAssetId() const
{
	return FPrimaryAssetId(FPrimaryAssetType(TEXT("IGStoryBeat")), GetFName());
}
