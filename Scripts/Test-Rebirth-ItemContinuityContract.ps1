[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'
$projectRoot = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path

function Read-ProjectText {
	param([Parameter(Mandatory)][string]$RelativePath)

	$path = Join-Path $projectRoot $RelativePath
	if (-not (Test-Path -LiteralPath $path -PathType Leaf)) {
		throw "Required item-continuity file is missing: $RelativePath"
	}
	return Get-Content -Raw -Encoding UTF8 -LiteralPath $path
}

function Assert-Contract {
	param(
		[Parameter(Mandatory)][bool]$Condition,
		[Parameter(Mandatory)][string]$Message
	)

	if (-not $Condition) {
		throw "REBIRTH item-continuity contract failed: $Message"
	}
}

function Get-BlockBetween {
	param(
		[Parameter(Mandatory)][string]$Text,
		[Parameter(Mandatory)][string]$StartMarker,
		[Parameter(Mandatory)][string]$EndMarker
	)

	$start = $Text.IndexOf($StartMarker)
	$end = $Text.IndexOf($EndMarker, $start + $StartMarker.Length)
	Assert-Contract ($start -ge 0 -and $end -gt $start) `
		"Could not isolate '$StartMarker'."
	return $Text.Substring($start, $end - $start)
}

$tags = Read-ProjectText 'Config/DefaultGameplayTags.ini'
$chapterOne =
	Read-ProjectText 'Source/IndieGame/Sequence/IGChapterOneIncidentDirector.cpp'
$dressingHeader =
	Read-ProjectText 'Source/IndieGame/Narrative/IGItemContinuityDressing.h'
$dressing =
	Read-ProjectText 'Source/IndieGame/Narrative/IGItemContinuityDressing.cpp'
$worldScene =
	Read-ProjectText 'Source/IndieGame/Core/IGPrologueWorldScene.cpp'
$worldSceneHeader =
	Read-ProjectText 'Source/IndieGame/Core/IGPrologueWorldScene.h'
$chapterThree =
	Read-ProjectText 'Source/IndieGame/Sequence/IGThirdMorningDirector.cpp'
$status = Read-ProjectText 'Docs/IMPLEMENTATION_STATUS.md'

foreach ($tag in @(
	'State.CH01.Incident.Drank',
	'State.CH01.Incident.BagPlacedAtLadder',
	'State.CH02.Loop.RecycleEvidenceRestored'
)) {
	Assert-Contract ($tags.Contains("Tag=`"$tag`"")) `
		"Gameplay tag '$tag' is missing."
}

$drinkBlock = Get-BlockBetween $chapterOne `
	'void AIGChapterOneIncidentDirector::PerformDrink()' `
	'void AIGChapterOneIncidentDirector::HandleAction('
foreach ($invariant in @(
	'EIGRebirthBottleClosureState::Resealed',
	'State.CH01.Incident.Drank',
	'RequestReturnCheckpointAutosave()'
)) {
	Assert-Contract ($drinkBlock.Contains($invariant)) `
		"04:33 drink/reseal path is missing '$invariant'."
}

$boundaryBlock = Get-BlockBetween $chapterOne `
	'bool AIGChapterOneIncidentDirector::BeginMemoryBoundary()' `
	'void AIGChapterOneIncidentDirector::PlaceAccidentBagAndRetireCarriedPurchase()'
Assert-Contract ($boundaryBlock.Contains(
	'PlaceAccidentBagAndRetireCarriedPurchase();')) `
	'The final hand-to-floor transfer is not part of the memory boundary.'
Assert-Contract ($boundaryBlock.Contains(
	'State.CH01.Incident.BagPlacedAtLadder')) `
	'The 04:43 placement state is not recorded.'
Assert-Contract (-not $boundaryBlock.Contains(
	'RequestReturnCheckpointAutosave();')) `
	'The 1.8-second cut must not become a resumable mid-transition checkpoint.'

$placementBlock = Get-BlockBetween $chapterOne `
	'void AIGChapterOneIncidentDirector::PlaceAccidentBagAndRetireCarriedPurchase()' `
	'void AIGChapterOneIncidentDirector::CompleteMemoryBoundary()'
foreach ($invariant in @(
	'ReleaseCarriedActor',
	'WasPickedUp()',
	'SetActorHiddenInGame(true)',
	'GetCanonicalAccidentTransform',
	'SpawnActor<AIGItemContinuityDressing>',
	'CH01.BagPlacedAtLadder',
	'MarkOneShotBeatPlayed'
)) {
	Assert-Contract ($placementBlock.Contains($invariant)) `
		"Atomic CH01 transfer is missing '$invariant'."
}
$releaseIndex = $placementBlock.IndexOf('ReleaseCarriedActor')
$spawnIndex = $placementBlock.IndexOf(
	'SpawnActor<AIGItemContinuityDressing>')
$configureIndex = $placementBlock.IndexOf(
	'AccidentBagDressing->Configure(')
$persistentIndex = $placementBlock.IndexOf(
	'MarkOneShotBeatPlayed')
Assert-Contract (
	$releaseIndex -ge 0 -and
	$spawnIndex -gt $releaseIndex -and
	$configureIndex -gt $spawnIndex -and
	$persistentIndex -gt $configureIndex
) 'The carried copy must retire before one configured proxy is committed.'
Assert-Contract (
	-not $placementBlock.Contains('ToWorld(FVector(-225') -and
	-not $placementBlock.Contains('900.8f')
) 'The hidden roof event must never materialize on the fourth-floor stair.'

foreach ($profile in @(
	'ProfileA500MlX2',
	'ProfileB1LX1',
	'ProfileC2LX2'
)) {
	Assert-Contract ($dressing.Contains($profile)) `
		"Static evidence omits purchase profile '$profile'."
	Assert-Contract ($chapterThree.Contains($profile)) `
		"CH03 evidence omits purchase profile '$profile'."
}
foreach ($invariant in @(
	'EIGItemContinuityPresentation::AccidentBag',
	'EIGItemContinuityPresentation::LobbyRecycleSack',
	'GetCanonicalAccidentTransform',
	'EIGRebirthBottleClosureState::MissingCap',
	'UCollisionProfile::NoCollision_ProfileName',
	'ECollisionEnabled::NoCollision',
	'REBIRTH.ItemContinuity.StaticEvidence'
)) {
	Assert-Contract (
		$dressing.Contains($invariant) -or
		$dressingHeader.Contains($invariant)
	) "Static evidence contract is missing '$invariant'."
}

# The transient 04:43 proxy and the CH03 reconstruction must occupy the same
# detached roof-stage coordinate contract; neither is allowed on the 4F set.
foreach ($coordinate in @(
	'-4800.0f, 4200.0f, 0.0f',
	'1885.0f',
	'-350.0f',
	'240.0f + BagHalfHeight + 0.5f'
)) {
	Assert-Contract ($dressing.Contains($coordinate)) `
		"Canonical 04:43 transform is missing '$coordinate'."
}
foreach ($coordinate in @(
	'StageOrigin(-4800.0f, 4200.0f, 0.0f)',
	'1885.0f',
	'-350.0f',
	'RoofFloorZ + BagHalfHeight + 0.5f'
)) {
	Assert-Contract ($chapterThree.Contains($coordinate)) `
		"CH03 bag transform diverged at '$coordinate'."
}

$chapterTwoSpawnBlock = Get-BlockBetween $worldScene `
	'void AIGPrologueWorldScene::SpawnChapterTwoItemContinuityDressing()' `
	'void AIGPrologueWorldScene::HandleFlashlightPickedUp('
foreach ($invariant in @(
	'GetChoices()',
	'Choices.PurchaseProfile',
	'Choices.BottleClosureState',
	'TActorIterator<AIGItemContinuityDressing>',
	'REBIRTH.ItemContinuity.CH02RecycleSack',
	'MatchesContract(',
	'EIGItemContinuityPresentation::LobbyRecycleSack',
	'State.CH02.Loop.RecycleEvidenceRestored',
	'FVector(-210.0f, -350.0f, 0.8f)'
)) {
	Assert-Contract ($chapterTwoSpawnBlock.Contains($invariant)) `
		"CH02 recovered-item path is missing '$invariant'."
}
Assert-Contract ($worldSceneHeader.Contains(
	'TObjectPtr<AIGItemContinuityDressing> ChapterTwoItemContinuityDressing')) `
	'The CH02 evidence actor has no single owner pointer.'

$chapterTwoCatAftermathBlock = Get-BlockBetween $worldScene `
	'void AIGPrologueWorldScene::RefreshChapterTwoCatWaterAftermath()' `
	'void AIGPrologueWorldScene::HandleFlashlightPickedUp('
foreach ($invariant in @(
	'TActorIterator<AIGChapterOneIncidentAction>',
	'IGPrologueWorld::CatWaterAftermathTag',
	'EIGRebirthCatWaterState::BottleCap',
	'EIGRebirthCatWaterState::PaperCup',
	'Choices.bWaitedForCat',
	'FVector(1198.0f, -438.0f, 8.0f)',
	'FVector(1230.0f, -438.0f, 9.0f)',
	'FVector(1255.0f, -438.0f, 10.0f)',
	'SetActorEnableCollision(false)',
	'SetInteractionEnabled(false)',
	'LiveAftermathCount == 0',
	'LiveAftermathCount == 1'
)) {
	Assert-Contract ($chapterTwoCatAftermathBlock.Contains($invariant)) `
		"CH02 cat-water reconstruction is missing '$invariant'."
}
Assert-Contract ($worldSceneHeader.Contains(
	'TObjectPtr<AIGChapterOneIncidentAction> ChapterTwoCatWaterAftermath')) `
	'The CH02 cat-water aftermath has no single owner pointer.'
Assert-Contract ($worldScene.Contains(
	'REBIRTH.CatWaterAftermath.CH02')) `
	'The CH02 cat-water aftermath has no unique actor tag.'

$enterChapterTwoBlock = Get-BlockBetween $worldScene `
	'void AIGPrologueWorldScene::EnterChapterTwo()' `
	'void AIGPrologueWorldScene::EnterChapterThree()'
$shelfRetirementBlock = Get-BlockBetween $enterChapterTwoBlock `
	'for (AIGPickupItem* WaterBottle : ChapterTwoWaterBottles)' `
	'if (Flashlight)'
Assert-Contract (
	$shelfRetirementBlock.Contains('SetActorHiddenInGame(true)') -and
	$shelfRetirementBlock.Contains('SetActorEnableCollision(false)') -and
	$shelfRetirementBlock.Contains('SetInteractionEnabled(false)') -and
	$shelfRetirementBlock.Contains(
		'SpawnChapterTwoItemContinuityDressing();') -and
	$shelfRetirementBlock.Contains(
		'RefreshChapterTwoCatWaterAftermath();') -and
	-not $shelfRetirementBlock.Contains('SetActorHiddenInGame(false)')
) 'CH02 must retire shelf pickups and rebuild both persisted alley traces.'

$enterChapterThreeBlock = $worldScene.Substring(
	$worldScene.IndexOf('void AIGPrologueWorldScene::EnterChapterThree()'))
Assert-Contract (
	$enterChapterThreeBlock.Contains(
		'ChapterTwoItemContinuityDressing->Destroy();') -and
	$enterChapterThreeBlock.Contains(
		'ChapterTwoItemContinuityDressing = nullptr;') -and
	$enterChapterThreeBlock.Contains(
		'ChapterTwoCatWaterAftermath->Destroy();') -and
	$enterChapterThreeBlock.Contains(
		'ChapterTwoCatWaterAftermath = nullptr;')
) 'CH02 evidence must be retired before the CH03 reconstruction exists.'

$chapterThreeBagBlock = Get-BlockBetween $chapterThree `
	'void AIGThirdMorningDirector::BuildP5AccidentEvidence()' `
	'void AIGThirdMorningDirector::PresentPurchaseEvidenceContinuity()'
foreach ($invariant in @(
	'PurchaseChoices = RebirthState->GetChoices()',
	'PurchaseChoices.PurchaseProfile',
	'PurchaseChoices.BottleClosureState',
	'EIGRebirthBottleClosureState::MissingCap',
	'EIGChapterThreeAction::EvidenceBag'
)) {
	Assert-Contract ($chapterThreeBagBlock.Contains($invariant)) `
		"CH03 purchase reconstruction is missing '$invariant'."
}

$catAftermathBlock = Get-BlockBetween $chapterOne `
	'void AIGChapterOneIncidentDirector::ReconcileState()' `
	'void AIGChapterOneIncidentDirector::HandleDrinkZone('
foreach ($invariant in @(
	'SetVisibleDecorative',
	'bShowCapEvidence',
	'bShowCupEvidence',
	'bShowRecoveredCapWetRing',
	'Choices.CatWaterState == EIGRebirthCatWaterState::BottleCap',
	'Choices.CatWaterState == EIGRebirthCatWaterState::PaperCup',
	'&& !Choices.bWaitedForCat',
	'&& Choices.bWaitedForCat'
)) {
	Assert-Contract ($catAftermathBlock.Contains($invariant)) `
		"Cat-water physical aftermath is missing '$invariant'."
}
$aftermathValidationBlock = Get-BlockBetween $chapterOne `
	'bool AIGChapterOneIncidentDirector::ValidateCatWaterAftermath() const' `
	'void AIGChapterOneIncidentDirector::ReconcileState()'
foreach ($invariant in @(
	'CatChoiceCommitted',
	'bCapVisible',
	'bCupVisible',
	'bWetRingVisible',
	'bAllDecorative',
	'EIGRebirthCatWaterState::BottleCap',
	'EIGRebirthCatWaterState::PaperCup',
	'EIGRebirthCatWaterState::PassedBy'
)) {
	Assert-Contract ($aftermathValidationBlock.Contains($invariant)) `
		"Cat-water aftermath validator is missing '$invariant'."
}
$endToEndReturnBlock = Get-BlockBetween $chapterOne `
	'bool AIGChapterOneIncidentDirector::RunRebirthEndToEndReturnRoute()' `
	'void AIGChapterOneIncidentDirector::BeginRebirthEndToEndMemoryBoundary()'
Assert-Contract ($endToEndReturnBlock.Contains(
	'ValidateCatWaterAftermath()')) `
	'CH01 end-to-end route does not verify the physical cat-water aftermath.'
$chapterOneCleanupBlock = Get-BlockBetween $chapterOne `
	'void AIGChapterOneIncidentDirector::EndPlay(' `
	'AIGChapterOneIncidentAction*'
foreach ($invariant in @(
	'SpawnedIncidentActors',
	'SpawnedActor->Destroy();',
	'SpawnedIncidentActors.Reset();'
)) {
	Assert-Contract ($chapterOneCleanupBlock.Contains($invariant)) `
		"CH01 incident cleanup is missing '$invariant'."
}

$chapterTwoEndToEndBlock = Get-BlockBetween $worldScene `
	'void AIGPrologueWorldScene::ContinueRebirthEndToEndChapterTwo()' `
	'void AIGPrologueWorldScene::FailRebirthEndToEndValidation('
Assert-Contract (
	$chapterTwoEndToEndBlock.Contains(
		'ValidateChapterTwoCatWaterAftermath()') -and
	$chapterTwoEndToEndBlock.Contains('cat_aftermath=1')
) 'CH02 end-to-end route does not verify the restored cat-water aftermath.'

$runtimeValidationBlock = Get-BlockBetween $chapterThree `
	'bool AIGThirdMorningDirector::ValidateRebirthItemContinuity(' `
	'void AIGThirdMorningDirector::HandleReleaseValidationSaveCompleted('
foreach ($invariant in @(
	'EIGRebirthPurchaseProfile::ProfileA500MlX2',
	'EIGRebirthPurchaseProfile::ProfileB1LX1',
	'EIGRebirthPurchaseProfile::ProfileC2LX2',
	'EIGRebirthBottleClosureState::MissingCap',
	'EIGRebirthBottleClosureState::Resealed',
	'EIGItemContinuityPresentation::AccidentBag',
	'EIGItemContinuityPresentation::LobbyRecycleSack',
	'CountLiveContinuityActors() == 1',
	'OutCaseCount == 12',
	'GetCanonicalAccidentTransform(Profile)'
)) {
	Assert-Contract ($runtimeValidationBlock.Contains($invariant)) `
		"S5 runtime matrix is missing '$invariant'."
}
Assert-Contract (
	$chapterThree.Contains(
		'REBIRTH_RELEASE PASS s5_item_continuity profiles=3 closures=2 ') -and
	$chapterThree.Contains(
		'presentations=2 cases=%d duplicates=0')
) 'S5 runtime validation PASS marker is missing.'

Assert-Contract (
	$status.Contains('04:33') -and
	$status.Contains('BagPlacedAtLadder') -and
	$status.Contains('RecycleEvidenceRestored')
) 'Implementation status does not describe the completed S5 continuity path.'

Write-Host (
	'REBIRTH item-continuity contract passed ' +
	'(3 profiles x 2 closure states, atomic handoff, CH02 singleton, CH03 match).'
) -ForegroundColor Green
