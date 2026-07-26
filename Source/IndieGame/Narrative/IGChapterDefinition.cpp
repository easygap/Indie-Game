#include "Narrative/IGChapterDefinition.h"

FPrimaryAssetId UIGChapterDefinition::GetPrimaryAssetId() const
{
	return FPrimaryAssetId(FPrimaryAssetType(TEXT("IGChapter")), GetFName());
}
