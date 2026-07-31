[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'
$projectRoot = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path

function Read-ProjectText {
	param([Parameter(Mandatory)][string]$RelativePath)

	$path = Join-Path $projectRoot $RelativePath
	if (-not (Test-Path -LiteralPath $path -PathType Leaf)) {
		throw "Required narrative-contract file is missing: $RelativePath"
	}
	return Get-Content -Raw -Encoding UTF8 -LiteralPath $path
}

function Assert-Contract {
	param(
		[Parameter(Mandatory)][bool]$Condition,
		[Parameter(Mandatory)][string]$Message
	)

	if (-not $Condition) {
		throw "REBIRTH narrative contract failed: $Message"
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
		"Could not isolate source block '$StartMarker'."
	return $Text.Substring($start, $end - $start)
}

$story = Read-ProjectText 'Docs/STORY_BIBLE_REBIRTH.md'
$gameplayTags = Read-ProjectText 'Config/DefaultGameplayTags.ini'
$router = Read-ProjectText 'Source/IndieGame/Narrative/IGRebirthNarrativeSubsystem.cpp'
$narrativeTypes = Read-ProjectText 'Source/IndieGame/Narrative/IGRebirthNarrativeTypes.h'
$director = Read-ProjectText 'Source/IndieGame/Sequence/IGThirdMorningDirector.cpp'
$directorHeader =
	Read-ProjectText 'Source/IndieGame/Sequence/IGThirdMorningDirector.h'
$chapterOneIncidentHeader =
	Read-ProjectText 'Source/IndieGame/Sequence/IGChapterOneIncidentDirector.h'
$chapterOneIncident =
	Read-ProjectText 'Source/IndieGame/Sequence/IGChapterOneIncidentDirector.cpp'
$morningDirector = Read-ProjectText 'Source/IndieGame/Sequence/IGMorningRoutineDirector.cpp'
$secondMorningDirector =
	Read-ProjectText 'Source/IndieGame/Sequence/IGSecondMorningDirector.cpp'
$pickup = Read-ProjectText 'Source/IndieGame/Interaction/IGPickupItem.cpp'
$elevatorHeader =
	Read-ProjectText 'Source/IndieGame/Interaction/IGElevator.h'
$elevator =
	Read-ProjectText 'Source/IndieGame/Interaction/IGElevator.cpp'
$zoneTrigger =
	Read-ProjectText 'Source/IndieGame/Interaction/IGZoneTrigger.cpp'
$stairTransition =
	Read-ProjectText 'Source/IndieGame/Interaction/IGStairTransition.cpp'
$checkout =
	Read-ProjectText 'Source/IndieGame/Interaction/IGCheckoutCounter.cpp'
$worldScene = Read-ProjectText 'Source/IndieGame/Core/IGPrologueWorldScene.cpp'
$playerCharacter =
	Read-ProjectText 'Source/IndieGame/Player/IGPlayerCharacter.cpp'
$flashlightHeader =
	Read-ProjectText 'Source/IndieGame/Player/IGFlashlightComponent.h'
$flashlight =
	Read-ProjectText 'Source/IndieGame/Player/IGFlashlightComponent.cpp'
$saveGame = Read-ProjectText 'Source/IndieGame/Save/IGSaveGame.h'
$saveSubsystem =
	Read-ProjectText 'Source/IndieGame/Save/IGSaveSubsystem.cpp'
$saveSubsystemHeader =
	Read-ProjectText 'Source/IndieGame/Save/IGSaveSubsystem.h'
$feasibility = Read-ProjectText 'Docs/FEASIBILITY.md'
$releaseValidation = Read-ProjectText 'Docs/RELEASE_VALIDATION.md'
$routeMatrix = Read-ProjectText 'Scripts/Test-Rebirth-RouteMatrix.ps1'
$signTextureGenerator = Read-ProjectText 'Scripts/Create-SignTextures.ps1'

# This script is a static story-contract oracle. It does not replace UHT/UBT,
# collision/physics rehearsal, packaged-build QA, or human playtesting.
$truthTags = @(
	'Truth.NeedWater',
	'Truth.Purchase0431',
	'Truth.Alarm0510',
	'Truth.DeathOverlay',
	'Truth.WasSearched',
	'Truth.Negligence',
	'Truth.CatSafe',
	'Truth.HoseCause',
	'Truth.Fall',
	'Truth.Identity',
	'Truth.RecheckScheduled0731',
	'Truth.Found0731'
)
$storyTruthNames = @{
	'Truth.NeedWater' = 'T_NEED_WATER'
	'Truth.Purchase0431' = 'T_PURCHASE_0431'
	'Truth.Alarm0510' = 'T_ALARM_0510'
	'Truth.DeathOverlay' = 'T_DEATH_OVERLAY'
	'Truth.WasSearched' = 'T_WAS_SEARCHED'
	'Truth.Negligence' = 'T_NEGLIGENCE'
	'Truth.CatSafe' = 'T_CAT_SAFE'
	'Truth.HoseCause' = 'T_HOSE_CAUSE'
	'Truth.Fall' = 'T_FALL'
	'Truth.Identity' = 'T_IDENTITY'
	'Truth.RecheckScheduled0731' = 'T_RECHECK_SCHEDULED_0731'
	'Truth.Found0731' = 'T_FOUND_0731'
}

foreach ($truthTag in $truthTags) {
	Assert-Contract ($gameplayTags.Contains("Tag=`"$truthTag`"")) `
		"Gameplay tag '$truthTag' is missing."
	Assert-Contract ($story.Contains($storyTruthNames[$truthTag])) `
		"Story master does not name the contract state for '$truthTag'."
}

Assert-Contract ($story.Contains('선택 전 11개 진실') -and
	$story.Contains('발견 뒤 `T_FOUND_0731`')) `
	'The story must distinguish 11 pre-choice truths from the post-discovery truth.'
Assert-Contract ($story.Contains('A와 B 모두') -and
	$story.Contains('실제 상태 복원')) `
	'Both endings must share the fixed real-world discovery before diverging emotionally.'
Assert-Contract ($story.Contains('7월 29일 업체 회신으로 작성된 CH03 미래 중첩 철문 안내') -and
	$story.Contains('[7/29 업체 재방문 안내]') -and
	$story.Contains('07/31 04:00 경찰·소방·업체 합동 개방') -and
	-not $story.Contains('CH01부터 붙어 있던')) `
	'The 07/31 recheck must be scheduled on 07/29 and visible only through the CH03 future overlap.'

$alleyBlock = Get-BlockBetween $story `
	'### 장면 6. 골목의 생활' `
	'### 장면 7. 새벽24 무영로점'
$storeBlock = Get-BlockBetween $story `
	'### 장면 7. 새벽24 무영로점' `
	'### 장면 8. 고양이에게 물'
Assert-Contract (-not $alleyBlock.Contains('두 번째 돌풍') -and
	$storeBlock.Contains('결제 후 출입문을 향해 돌아선 순간') -and
	$storeBlock.Contains('두 번째 돌풍')) `
	'The second gust must occur once, after payment, so the 04:37 gust remains third.'

$purchaseProfileBlock = Get-BlockBetween $narrativeTypes `
	'enum class EIGRebirthPurchaseProfile' `
	'};'
foreach ($profileName in @(
	'ProfileA500MlX2',
	'ProfileB1LX1',
	'ProfileC2LX2'
)) {
	Assert-Contract ($purchaseProfileBlock.Contains($profileName)) `
		"Fixed purchase profile '$profileName' is missing."
}
foreach ($profileValueContract in @(
	'ProfileA500MlX2\s*=\s*1',
	'ProfileB1LX1\s*=\s*2',
	'ProfileC2LX2\s*=\s*3'
)) {
	Assert-Contract ($purchaseProfileBlock -match $profileValueContract) `
		"Purchase profile numeric save contract '$profileValueContract' is missing."
}
foreach ($obsoleteProfileName in @(
	'WaterOnly',
	'WaterAndSnack',
	'WaterAndNoodles'
)) {
	Assert-Contract (-not $purchaseProfileBlock.Contains($obsoleteProfileName)) `
		"Obsolete free-form purchase profile '$obsoleteProfileName' remains."
}
Assert-Contract ($story.Contains('A 500mL × 2') -and
	$story.Contains('B 1L × 1') -and
	$story.Contains('C 2L × 2') -and
	$story.Contains('A/B/C 세 고정 프로필')) `
	'Story and state model must expose the same three fixed purchase profiles.'
Assert-Contract ($narrativeTypes.Contains('MissingCap') -and
	$narrativeTypes.Contains('Resealed') -and
	$narrativeTypes.Contains('bHasPaperCup') -and
	$narrativeTypes.Contains('bWaitedForCat')) `
	'Bottle-cap and cat-wait branch state must be serializable.'

Assert-Contract ($story.Contains('개정 3.7 — 제작 가능성·표현 계약 잠금') -and
	$story.Contains('손·발 스켈레탈 애니메이션 자체는') -and
	$story.Contains('순수 상태 모델 전수검사')) `
	'The story must define perceptual outcomes without requiring a skeletal pipeline.'
foreach ($feasibilityContract in @(
	'Keep — 출시 필수 기능',
	'Static Proxy — 허용 구현',
	'Cut or Post-launch',
	'합격 기준'
)) {
	Assert-Contract ($feasibility.Contains($feasibilityContract)) `
		"Feasibility contract is missing '$feasibilityContract'."
}
foreach ($manualAcceptanceCase in @(
	'S1 착의 소매',
	'S2 11cm 문·고양이',
	'S3 P3 중단·재개',
	'S4 엔딩 공통 복원',
	'S5 소지품 인과 연속성',
	'S6 CH02 도움 요청·P1/P2 전달',
	'S7 엔딩 최종 연출',
	'S8 사고 물리 리허설'
)) {
	Assert-Contract ($releaseValidation.Contains($manualAcceptanceCase)) `
		"Release validation procedure is missing '$manualAcceptanceCase'."
}
Assert-Contract ($routeMatrix.Contains('$routeCount -eq 7200') -and
	$routeMatrix.Contains('$endingCheckCount -eq 14400') -and
	$routeMatrix.Contains('$roundTripCount -eq 21600') -and
	$routeMatrix.Contains('$persistenceBoundaryCount -eq 7') -and
	$routeMatrix.Contains('$endingBoundaryCount -eq 6') -and
	$routeMatrix.Contains('$representativeRouteCount -eq 4') -and
	$routeMatrix.Contains('$p5PermutationCount -eq 24') -and
	$routeMatrix.Contains('$p5ObservationRoundTripCount -eq 24576') -and
	$routeMatrix.Contains('$mixedTransportRouteCount -eq 4') -and
	$routeMatrix.Contains('$emptyCabRecallCount -eq 2')) `
	'The pure route model must retain exhaustive routes, persistence boundaries, and P5 orders.'

Assert-Contract ($saveGame.Contains('CurrentSchemaVersion = 3') -and
	$narrativeTypes.Contains('int32 SchemaVersion = 3') -and
	$narrativeTypes.Contains('FIGRebirthChapterThreeState ChapterThree') -and
	$saveSubsystem.Contains('RebirthState->BuildSnapshot()') -and
	$saveSubsystem.Contains('RebirthState->RestoreSnapshot(')) `
	'Save v3 must persist and restore the complete REBIRTH snapshot v3.'
$restoreNarrativeIndex = $saveSubsystem.IndexOf(
	'RebirthState->RestoreSnapshot(')
$restoreStoryTagsIndex = $saveSubsystem.IndexOf(
	'StoryState->RestoreStateSnapshot(')
Assert-Contract ($restoreNarrativeIndex -ge 0 -and
	$restoreStoryTagsIndex -gt $restoreNarrativeIndex) `
	'REBIRTH state must restore before legacy tag callbacks rebuild world presentation.'
Assert-Contract ($saveSubsystemHeader.Contains('IsApplyingLoadedProgress') -and
	$saveSubsystemHeader.Contains('RequestLoadLatestAutosave') -and
	$saveSubsystem.Contains('ApplyLoadedProgressInternal(!bWillTravel)') -and
	$saveSubsystem.Contains('DoesSaveGameExist') -and
	$saveSubsystem.Contains('Candidate->Progress.SavedAtUtc > NewestTimestamp') -and
	$saveSubsystem.Contains('IGChapterThree=1?IGResumeSave=1') -and
	$saveSubsystem.Contains('IGChapterTwo=1?IGResumeSave=1') -and
	$saveSubsystem.Contains('IGIgnoreDirectStart=1?IGResumeSave=1') -and
	$saveSubsystem.Contains('QueuedAutosave = nullptr')) `
	'Load coordination must apply once, suppress stale autosaves, and identify resume travel.'
Assert-Contract ($playerCharacter.Contains('EKeys::F9') -and
	$playerCharacter.Contains('RequestLoadLatestAutosave()') -and
	$playerCharacter.Contains('최근 자동 저장을 불러옵니다.')) `
	'The player must have an actual latest-autosave load entry point.'
Assert-Contract ($worldScene.Contains(
	'void AIGPrologueWorldScene::ReconcileLoadedCheckpoint()') -and
	$worldScene.Contains('Checkpoint.CH01.WaterPurchased') -and
	$worldScene.Contains('HandleStoryStateChanged(PurchaseTag, true)') -and
	$worldScene.Contains('Elevator->RestoreAtLobbyOpen()') -and
	$worldScene.Contains('!bResumeLoadedCheckpoint') -and
	$worldScene.Contains('Checkpoint.CH02.Store') -and
	$worldScene.Contains('Checkpoint.CH02.Woke') -and
	$worldScene.Contains(
		'ThirdMorningDirector->RestoreCheckpointAnchor(LoadedCheckpoint)') -and
	$director.Contains(
		'void AIGThirdMorningDirector::RestoreCheckpointAnchor(') -and
	$director.Contains('Checkpoint.CH03.Flood') -and
	$director.Contains('Checkpoint.CH03.Roof')) `
	'CH01/02/03 loads must consume checkpoint tags and restore safe world anchors.'
Assert-Contract ($secondMorningDirector.Contains('WokeCheckpointTag') -and
	$secondMorningDirector -match
		'HasState\(LeftHomeTag\)\s*\?\s*CorridorCheckpointTag\s*:\s*WokeCheckpointTag') `
	'An in-apartment CH02 truth must save at Woke, not beyond the LeftHome trigger.'
Assert-Contract ($worldScene.Contains('SetHasMemoryFlashlight(true)') -and
	$worldScene.Contains('State.CH02.Loop.HasFlashlight') -and
	$worldScene.Contains('bHasMemoryFlashlight') -and
	$worldScene.Contains('Torch->SetAvailable(bRestoreFlashlight)')) `
	'The optional flashlight must retain both canonical ownership and functional availability.'
foreach ($requiredChapterThreeField in @(
	'bool bApartmentFridgeInspected = false',
	'float PressureKPa = 60.0f',
	'int32 ZeroConfirmationTicks = 0',
	'float HintElapsedSeconds = 0.0f',
	'int32 HintStage = 0',
	'EIGRebirthEvidenceId FocusedEvidence',
	'TArray<FName> ObservedP5Sources',
	'int32 AccidentScratchCount = 0',
	'bool bAccidentScratchTailSettled = false',
	'bool bLookedAwayAfterFirstScratch = false',
	'bool bActedAfterSecondScratch = false',
	'EIGRebirthEndingChoice EndingChoice',
	'bool bCommonDiscoveryCommitted = false'
)) {
	Assert-Contract ($narrativeTypes.Contains($requiredChapterThreeField)) `
		"CH03 snapshot field is missing '$requiredChapterThreeField'."
}
Assert-Contract ($director.Contains(
	'State.P3.PressureKPa = P3PressureKPa') -and
	$director.Contains('&ThisClass::UpdateP3PressureRelease') -and
	$director.Contains('ResumeRestoredEnding()') -and
	$router.Contains('NormalizeSnapshot(Snapshot, true)')) `
	'P3 in-flight state and ending boundaries must resume through the stable snapshot.'
Assert-Contract (
	$router.Contains('FName(TEXT("CH03.MotherMessages"))') -and
	$router.Contains('FName(TEXT("CH03.SearchPoster"))') -and
	$router.Contains('FName(TEXT("CH03.PhoneApprovalHistory"))')) `
	'Restarting CH03 must remove every CH03-only alternate truth source.'
$chapterThreeReset = Get-BlockBetween $router `
	'void UIGRebirthNarrativeSubsystem::ResetChapterThreeAttempt()' `
	'void UIGRebirthNarrativeSubsystem::NormalizeState('
Assert-Contract (
	$chapterThreeReset.Contains(
		'IGRebirthState::TankOpenedWithoutFlashlight') -and
	$chapterThreeReset.Contains(
		'IGRebirthState::TankRevealedAfterFlashlight')) `
	'Restarting CH03 must clear both attempt-local dark-tank reveal guards.'
$chapterThreeCommit = Get-BlockBetween $director `
	'void AIGThirdMorningDirector::CommitChapterThreeState()' `
	'void AIGThirdMorningDirector::RequestCheckpointAutosave('
$chapterThreeRestore = Get-BlockBetween $director `
	'bool AIGThirdMorningDirector::RestoreChapterThreeState()' `
	'void AIGThirdMorningDirector::ApplyChapterThreeWorldState()'
$persistenceAssignments = @(
	@{
		Commit = 'State.bApartmentFridgeInspected = bFridgeInspected'
		Restore = 'bFridgeInspected = State.bApartmentFridgeInspected'
	},
	@{
		Commit = 'State.P3.ZeroConfirmationTicks = P3ZeroConfirmationTicks'
		Restore = 'P3ZeroConfirmationTicks = State.P3.ZeroConfirmationTicks'
	},
	@{
		Commit = 'State.P3.HintElapsedSeconds = P3HintElapsedSeconds'
		Restore = 'P3HintElapsedSeconds = State.P3.HintElapsedSeconds'
	},
	@{
		Commit = 'State.P3.HintStage = P3HintStage'
		Restore = 'P3HintStage = State.P3.HintStage'
	},
	@{
		Commit = 'State.ObservedP5Sources = ObservedP5Sources'
		Restore = 'ObservedP5Sources = State.ObservedP5Sources'
	},
	@{
		Commit = 'State.bLookedAwayAfterFirstScratch ='
		Restore = 'bLookedAwayAfterFirstScratch ='
	},
	@{
		Commit = 'State.bActedAfterSecondScratch ='
		Restore = 'bActedAfterSecondScratch ='
	}
)
foreach ($assignment in $persistenceAssignments) {
	Assert-Contract (
		$chapterThreeCommit.Contains($assignment.Commit) -and
		$chapterThreeRestore.Contains($assignment.Restore)) `
		"CH03 commit/restore symmetry is missing '$($assignment.Commit)'."
}
$chapterThreeWorldApply = Get-BlockBetween $director `
	'void AIGThirdMorningDirector::ApplyChapterThreeWorldState()' `
	'void AIGThirdMorningDirector::ApplyRoofDoorState('
Assert-Contract (
	$chapterThreeWorldApply.Contains('if (bFridgeInspected)') -and
	$chapterThreeWorldApply.Contains(
		'SetVisibleInteractive(FridgeDoorAction, false)') -and
	$chapterThreeWorldApply.Contains('Bottle->SetVisibility(true)') -and
	$chapterThreeWorldApply.Contains(
		'FridgeInteriorLightPanel->SetVisibility(true)') -and
	$chapterThreeWorldApply.Contains(
		'FridgeInteriorLight->SetVisibility(true)')) `
	'The CH03 apartment checkpoint must restore the inspected full-fridge world state.'
Assert-Contract (
	$director.Contains('P3HintElapsedSeconds += 1.0f') -and
	[regex]::IsMatch(
		$director,
		'P3HintElapsedSeconds \+= 1\.0f;\s*.*?CommitChapterThreeState\(\);\s*const float Thresholds',
		[System.Text.RegularExpressions.RegexOptions]::Singleline)) `
	'P3 hint dwell time must enter the snapshot between authored thresholds.'

$chapterThreeBootstrap = Get-BlockBetween $director `
	'void AIGThirdMorningDirector::BootstrapRebirthContext()' `
	'void AIGThirdMorningDirector::CommitChapterThreeState()'
$chapterThreeDoor = Get-BlockBetween $director `
	'case EIGChapterThreeAction::OpenApartmentDoor:' `
	'case EIGChapterThreeAction::InspectElevator:'
$chapterOneBridge = Get-BlockBetween $morningDirector `
	'void AIGMorningRoutineDirector::BridgeRebirthState(' `
	'void AIGMorningRoutineDirector::RestoreOutfitFromCanonical('
Assert-Contract (-not $chapterThreeBootstrap.Contains('MarkOutfitEquipped') -and
	$chapterThreeDoor.Contains('MarkOutfitEquipped(FName(TEXT("CH03")))') -and
	$chapterThreeDoor -match
		'SetRebirthOutfitEquipped\s*\(\s*true\s*,\s*bFirstOutfitPresentation\s*\)' -and
	$morningDirector.Contains('const FName ChapterOutfitId(TEXT("CH01"))') -and
	$morningDirector.Contains('MarkOutfitEquipped(ChapterOutfitId)') -and
	$secondMorningDirector.Contains('const FName ChapterOutfitId(TEXT("CH02"))') -and
	$secondMorningDirector.Contains('MarkOutfitEquipped(ChapterOutfitId)') -and
	$morningDirector.Contains('HandleApartmentExitZoneTriggered') -and
	$worldScene.Contains('ChapterOneApartmentExitZone = SpawnZone') -and
	-not $chapterOneBridge.Contains('MarkOutfitEquipped') -and
	$morningDirector.Contains(
		'BuildSnapshot().EquippedOutfitChapters.Contains') -and
	$secondMorningDirector.Contains(
		'BuildSnapshot().EquippedOutfitChapters.Contains') -and
	$playerCharacter.Contains('OutfitRepairStitch') -and
	$playerCharacter.Contains('PresentationDurationSeconds = 1.2f') -and
	$playerCharacter.Contains('Stitch->SetupAttachment(OutfitSleeveProxy)') -and
	$playerCharacter.Contains('bPlayPresentation') -and
	-not $playerCharacter.Contains(
		'bOutfitPresentationPlayedForCurrentChapter') -and
	-not $playerCharacter.Contains('SetIgnoreMoveInput') -and
	-not $playerCharacter.Contains('SetIgnoreLookInput')) `
	'Each chapter must restore canonical outfit state and present its sleeve once at the real first exit.'

$chapterOnePhase = Get-BlockBetween $morningDirector `
	'EIGMorningPhase AIGMorningRoutineDirector::EvaluatePhaseFromState() const' `
	'void AIGMorningRoutineDirector::RefreshPhase('
Assert-Contract ($chapterOnePhase.Contains('LeftApartmentStateTag') -and
	$chapterOnePhase.Contains('LeftHomeStateTag') -and
	$chapterOnePhase.Contains('FridgeCheckedStateTag') -and
	-not ($chapterOnePhase -match
		'FridgeCheckedStateTag\)\s*&&\s*IGStory::HasState\(this,\s*HasWalletStateTag') -and
	$worldScene.Contains('State.CH01.Morning.LeftApartment') -and
	$secondMorningDirector.Contains('달라진 아침을 확인하거나 복도로 나가자') -and
	$secondMorningDirector.Contains(
		'Inspect the changed morning or step into the corridor') -and
	-not $secondMorningDirector.Contains('지갑과 손전등을 챙겨 편의점으로 가자')) `
	'Optional wallet/flashlight routes must not block CH01 or be phrased as mandatory in CH02.'

$purchaseBridgeBlock = Get-BlockBetween $morningDirector `
	'void AIGMorningRoutineDirector::BridgeRebirthState(' `
	'void AIGMorningRoutineDirector::HandleStoryStateChanged('
$setChoicesIndex = $purchaseBridgeBlock.IndexOf(
	'RebirthState->SetChoices(Choices);')
$purchaseTruthIndex = $purchaseBridgeBlock.IndexOf(
	'Truth(TEXT("Truth.Purchase0431"))')
Assert-Contract ($purchaseBridgeBlock.Contains('Truth.NeedWater') -and
	$purchaseBridgeBlock.Contains('CH01.Store') -and
	$setChoicesIndex -ge 0 -and
	$purchaseTruthIndex -gt $setChoicesIndex -and
	$purchaseBridgeBlock.Contains('EIGRebirthPaymentMethod::WalletCard') -and
	$purchaseBridgeBlock.Contains('EIGRebirthPaymentMethod::PocketCard')) `
	'CH01 must commit location and selected purchase data before Purchase0431.'
$pickupCommitBlock = Get-BlockBetween $pickup `
	'void AIGPickupItem::CompleteInteraction_Implementation(' `
	'void AIGPickupItem::CommitRebirthPurchaseProfile()'
Assert-Contract (
	$pickupCommitBlock.IndexOf('CommitRebirthPurchaseProfile();') -ge 0 -and
	$pickupCommitBlock.IndexOf('CommitRebirthPurchaseProfile();') -lt
		$pickupCommitBlock.IndexOf('IGStory::AddState')) `
	'The selected product profile must be committed before the legacy pickup event.'
Assert-Contract ($pickup.Contains('ReturnForProfileSwap') -and
	$pickup.Contains('ReleaseCarriedActor(this)') -and
	$pickup.Contains('!IsRebirthPurchaseCommitted()') -and
	$worldScene.Contains('HandlePurchaseSelectionChanged') -and
	$playerCharacter.Contains('bHeavyBagInteractionProxyActive') -and
	$playerCharacter.Contains('ProfileC2LX2') -and
	-not $playerCharacter.Contains('SetIgnoreMoveInput')) `
	'CH01 must support pre-checkout profile swaps and the non-blocking Profile C bag proxy.'
Assert-Contract (-not $flashlightHeader.Contains('BatterySeconds') -and
	-not ($flashlight -match
		'BatteryFraction\s*=\s*FMath::Max\([^;]*DeltaSeconds') -and
	$flashlight.Contains('presentation-only and always recover')) `
	'The flashlight may brown out for pressure but must never deplete into a progression lock.'
foreach ($profile in @(
	'ProfileA500MlX2',
	'ProfileB1LX1',
	'ProfileC2LX2'
)) {
	Assert-Contract ($worldScene.Contains(
		"EIGRebirthPurchaseProfile::$profile")) `
		"Store pickup wiring is missing '$profile'."
}
$chapterTwoObjectives = Get-BlockBetween $secondMorningDirector `
	'FText AIGSecondMorningDirector::GetObjectiveText() const' `
	'FString AIGSecondMorningDirector::GetObjectiveTextAscii() const'
$chapterTwoHandler = Get-BlockBetween $secondMorningDirector `
	'void AIGSecondMorningDirector::HandleStoryStateChanged(' `
	'void AIGSecondMorningDirector::RefreshObjective()'
$chapterTwoReturnZone = Get-BlockBetween $worldScene `
	'ChapterTwoReturnZone = SpawnParkedZone(' `
	'void AIGPrologueWorldScene::HandleFlashlightPickedUp('
Assert-Contract ($secondMorningDirector.Contains('Truth.Alarm0510') -and
	$secondMorningDirector.Contains('CH02.MirrorAlarmMemo') -and
	$secondMorningDirector.Contains('Truth.DeathOverlay') -and
	$secondMorningDirector.Contains('CH02.DuplicateReceipt') -and
	$secondMorningDirector.Contains('Truth.WasSearched') -and
	$secondMorningDirector.Contains('CH02.ManagementComplaint') -and
	$secondMorningDirector.Contains(
		'EIGRebirthConvergencePoint::C3SecondMorning') -and
	$chapterTwoObjectives.IndexOf('if (CanConvergeSecondMorning())') -lt
		$chapterTwoObjectives.IndexOf('HasState(SawMirrorRoomTag)') -and
	$chapterTwoObjectives.Contains('!HasState(EnteredAlleyTag)') -and
	$chapterTwoObjectives.Contains(
		'불 켜진 편의점에서 사람을 찾거나 다른 흔적을 확인하자') -and
	$chapterTwoObjectives.Contains('직원 호출 버튼을 눌러 보자') -and
	-not $chapterTwoObjectives.Contains('새벽샘물 500mL를 집자') -and
	-not $chapterTwoObjectives.Contains('계산하고 나가자') -and
	$chapterTwoHandler.Contains('CalledEmployeeTag') -and
	-not $chapterTwoHandler.Contains(
		'StateTag.MatchesTagExact(WaterPurchasedTag)') -and
	-not ($secondMorningDirector -match
		'HasState\(EnteredStoreTag\)\s*&&\s*CanConvergeSecondMorning') -and
	$checkout.Contains('ConfigureChapterAction') -and
	$worldScene.Contains('State.CH02.Loop.CalledEmployee') -and
	$worldScene.Contains('[대조] 원거래 04:31 / 현재 04:44 중복') -and
	$chapterTwoReturnZone.Contains('FVector(300, -305, 1010)') -and
	-not $chapterTwoReturnZone.Contains('FVector(643, -360, 110)') -and
	$worldScene.Contains('SetInteractionEnabled(false)')) `
	'CH02 must converge through optional alarm, overlay, and search truths without a live repurchase gate.'
$chapterTwoOverlay = Get-BlockBetween $worldScene `
	'void AIGPrologueWorldScene::BuildChapterTwoOverlay()' `
	'void AIGPrologueWorldScene::SetChapterTwoOverlayVisible('
$chapterTwoOverlayVisibility = Get-BlockBetween $worldScene `
	'void AIGPrologueWorldScene::SetChapterTwoOverlayVisible(' `
	'void AIGPrologueWorldScene::RevealElevatorFootprints()'
$chapterThreeEntry = Get-BlockBetween $worldScene `
	'void AIGPrologueWorldScene::EnterChapterThree()' `
	'void AIGPrologueWorldScene::StartChapterTwoCaptureSequence()'
Assert-Contract (
	$chapterTwoOverlay.Contains('OfferingBowlMaterial') -and
	$chapterTwoOverlay.Contains('DryOuter') -and
	$chapterTwoOverlay.Contains('DryInner') -and
	$chapterTwoOverlay.Contains('FVector(-126, -280, 4.5f)') -and
	$chapterTwoOverlay.Contains('FVector(24, 24, 9.0f)') -and
	-not $chapterTwoOverlay.Contains('Small rice bowl') -and
	-not $chapterTwoOverlay.Contains('incense sticks') -and
	$worldScene.Contains('OfferingNote = SpawnNote(') -and
	$worldScene.Contains('"OfferingNoteL1", "목마른 사람은"') -and
	$worldScene.Contains('"OfferingNoteL2", "이 물을 드시오."') -and
	$worldScene.Contains('"OfferingNoteL3", "— 401호"') -and
	-not $worldScene.Contains('SaltMemo') -and
	-not $secondMorningDirector.Contains('밥에 숟가락은') -and
	$chapterTwoOverlayVisibility.Contains(
		'SetChapterActorVisible(OfferingNote)') -and
	$chapterThreeEntry.Contains('SetChapterTwoOverlayVisible(false)') -and
	$director.Contains('FVector(24, 24, 10)') -and
	$director.Contains('"OfferingL1", "목마른 사람은"') -and
	$director.Contains('"OfferingL2", "이 물을 드시오."') -and
	$director.Contains('"OfferingL3", "— 401호"')) `
	'The 401 offering must move from the CH02 doorstep to CH03 without restoring rice, spoon, or incense.'
Assert-Contract ($elevatorHeader.Contains('LowerCallButtonMesh') -and
	$elevatorHeader.Contains('LowerCabTrigger') -and
	$elevatorHeader.Contains('OpeningLowerForReturn') -and
	$elevatorHeader.Contains('ReturnedToFourthFloor') -and
	$elevator.Contains('void AIGElevator::PollForReturnRider()') -and
	$elevator.Contains('void AIGElevator::StartReturnRide()') -and
	$elevator.Contains('void AIGElevator::BeginOpeningUpperAfterReturn()') -and
	$elevator.Contains('PendingRider.Reset();') -and
	$elevator.Contains('SetState(EIGElevatorState::Ascending);') -and
	$elevator.IndexOf('PendingRider.Reset();',
		$elevator.IndexOf('case EIGElevatorState::ClosingLowerForReturn:')) -lt
		$elevator.IndexOf('TransferRiderToCab(Rider, 0.0f)',
			$elevator.IndexOf('case EIGElevatorState::ClosingLowerForReturn:')) -and
	$chapterTwoReturnZone.Contains('FVector(300, -305, 1010)')) `
	'CH02 must provide a retry-safe physical lobby-to-4F elevator return before C3.'

$emptyCallBlock = Get-BlockBetween $elevator `
	'void AIGElevator::StartEmptyCallToLobby()' `
	'void AIGElevator::HandleCabBeginOverlap('
$emptyUpperArrivalBlock = Get-BlockBetween $elevator `
	'void AIGElevator::BeginOpeningUpperAfterEmptyCall()' `
	'void AIGElevator::PlayEmptyTravelHum('
foreach ($emptyState in @(
	'ClosingUpperForEmptyCall',
	'EmptyDescending',
	'ClosingLowerForEmptyCall',
	'EmptyAscending'
)) {
	Assert-Contract ($elevatorHeader.Contains($emptyState) -and
		$elevator.Contains($emptyState)) `
		"Elevator empty-call state '$emptyState' is missing."
}
Assert-Contract (
	$emptyCallBlock.Contains('SecondsPerFloor * 3.0f') -and
	$emptyCallBlock.Contains('PlayEmptyTravelHum(false)') -and
	$emptyCallBlock.Contains('PlayEmptyTravelHum(true)') -and
	$emptyCallBlock.Contains('BeginOpeningLowerAfterEmptyCall') -and
	$emptyCallBlock.Contains('BeginOpeningUpperAfterEmptyCall') -and
	$emptyCallBlock.Contains('OpeningLowerForReturn') -and
	$emptyUpperArrivalBlock.Contains(
		'SetState(EIGElevatorState::OpeningUpper);') -and
	-not $emptyUpperArrivalBlock.Contains('OnReturnedToFourthFloor') -and
	([regex]::Matches(
		$elevator,
		'OnReturnedToFourthFloor\.Broadcast')).Count -eq 1 -and
	$elevator.Contains(
		'State == EIGElevatorState::WaitingForRider') -and
	$elevator.Contains(
		'State == EIGElevatorState::WaitingForReturnRider')) `
	'Opposite-floor calls must close, travel for three floors, reopen without a false rider-arrival event, and remain recallable after abandonment.'

Assert-Contract (
	$gameplayTags.Contains('Tag="Checkpoint.CH01.CatApproach"') -and
	$gameplayTags.Contains('Tag="Checkpoint.CH01.CatSpot"') -and
	$gameplayTags.Contains('Tag="Checkpoint.CH01.ReturnAlley"') -and
	$gameplayTags.Contains('Tag="Checkpoint.CH01.Lobby"') -and
	$gameplayTags.Contains('Tag="Checkpoint.CH01.FourthFloor"') -and
	$chapterOneIncident.Contains('Checkpoint.CH01.CatApproach') -and
	$chapterOneIncident.Contains('Checkpoint.CH01.CatSpot') -and
	$chapterOneIncident.Contains('bCatChoiceCommitted') -and
	$worldScene.Contains('FVector(1550.0f, -515.0f, 110.0f)') -and
	$worldScene.Contains('FVector(1320.0f, -515.0f, 110.0f)') -and
	$worldScene.Contains('FVector(1005.0f, -515.0f, 110.0f)')) `
	'CH01 drink, cat-container, committed-return, lobby, and 4F saves must restore to distinct safe sides of their triggers.'
Assert-Contract (
	-not $checkout.Contains('CatWaterState') -and
	-not $checkout.Contains('BottleClosureState') -and
	$chapterOneIncident.Contains('PerformDrink()') -and
	$chapterOneIncident.Contains('RequestReturnCheckpointAutosave();')) `
	'Checkout must not invent a cat choice; the first drink and later player actions own their atomic saves.'

$fourthFloorCueBlock = Get-BlockBetween $chapterOneIncident `
	'void AIGChapterOneIncidentDirector::StartFourthFloorCueIfNeeded()' `
	'void AIGChapterOneIncidentDirector::AdvanceCorridorBlink()'
$memoryBoundaryBlock = Get-BlockBetween $chapterOneIncident `
	'bool AIGChapterOneIncidentDirector::BeginMemoryBoundary()' `
	'void AIGChapterOneIncidentDirector::CompleteMemoryBoundary()'
Assert-Contract (
	$chapterOneIncidentHeader.Contains('bFifthFloorStepArmed') -and
	$fourthFloorCueBlock.Contains('bFifthFloorStepArmed = false') -and
	$chapterOneIncident.Contains('bFifthFloorStepArmed = true') -and
	$chapterOneIncident.Contains('PlayAuthoredCatCall(') -and
	$memoryBoundaryBlock.Contains('if (!bFifthFloorStepArmed)') -and
	$memoryBoundaryBlock.Contains('1.8f') -and
	$chapterOneIncident.Contains('ConfigureAsInteractionSurface') -and
	$chapterOneIncident.Contains('ECollisionEnabled::QueryOnly') -and
	$chapterOneIncident.Contains('ECC_Visibility') -and
	$worldScene.Contains(
		'const float StepY = -216.0f + UpperStepIndex * 22.0f') -and
	$worldScene.Contains('FVector(170.5f, -305, -10)') -and
	$worldScene.Contains('FVector(519, 120, 20)') -and
	-not $chapterOneIncident.Contains(
		'State.CH01.Incident.AccidentOccurred') -and
	-not $chapterOneIncident.Contains(
		'State.CH01.Incident.BagSetDown')) `
	'CH01 must preserve permanent stair geometry, finish the normal blink and directed cat call, then cut on the first 4F-to-5F tread without revealing the accident.'
Assert-Contract (
	$stairTransition.Contains('0.12f') -and
	$worldScene.Contains('HandleElevatorReturnedToFourthFloor') -and
	$worldScene.Contains('HandleStairTransitionCompleted') -and
	([regex]::Matches(
		$worldScene,
		'ChapterOneIncidentDirector->RegisterFourthFloorReturn\(\)')).Count -eq 2) `
	'Both physical stairs and elevator returns must converge at the same CH01 fourth-floor cue.'
$zoneBeginPlayBlock = Get-BlockBetween $zoneTrigger `
	'void AIGZoneTrigger::BeginPlay()' `
	'void AIGZoneTrigger::EndPlay('
Assert-Contract ($zoneBeginPlayBlock.Contains('ReconcileWithStoryState()') -and
	$zoneTrigger.Contains('HandleStoryStateChanged') -and
	$worldScene.Contains('SpawnActorDeferred<AIGZoneTrigger>')) `
	'One-shot zones must reconcile tags after deferred configuration and during live restores.'
$chapterThreeClueHandler = Get-BlockBetween $director `
	'void AIGThirdMorningDirector::HandleClueRead(' `
	'void AIGThirdMorningDirector::HandleCorridorEntered('
Assert-Contract ($director.Contains('MotherPhoneApproval0431') -and
	$director.Contains('MotherPhoneApproval0444') -and
	$chapterThreeClueHandler.Contains('Truth.DeathOverlay') -and
	$chapterThreeClueHandler.Contains('CH03.PhoneApprovalHistory') -and
	$routeMatrix.Contains(
		"'Truth.DeathOverlay' = 'CH03.PhoneApprovalHistory'")) `
	'Skipping CH02 P2 must leave an actual 04:31/04:44 phone record in CH03.'

$c5Block = Get-BlockBetween $router `
	'case EIGRebirthConvergencePoint::C5FinalChoice:' `
	'case EIGRebirthConvergencePoint::C6AfterDiscovery:'
$c6Block = Get-BlockBetween $router `
	'case EIGRebirthConvergencePoint::C6AfterDiscovery:' `
	'default:'
$c5Truths = @(
	'Alarm0510',
	'DeathOverlay',
	'WasSearched',
	'Negligence',
	'CatSafe',
	'HoseCause',
	'Fall',
	'Identity',
	'RecheckScheduled0731'
)
foreach ($truth in $c5Truths) {
	Assert-Contract ($c5Block.Contains("IGRebirthTruth::$truth")) `
		"C5 omits required truth '$truth'."
}
Assert-Contract (-not $c5Block.Contains('IGRebirthTruth::Found0731')) `
	'C5 must not use the future completed-discovery state.'
Assert-Contract ($c6Block.Contains('IGRebirthTruth::Found0731') -and
	$c6Block.Contains('CH03.ActualStateRestored')) `
	'C6 must require actual-state restoration and the completed discovery.'

foreach ($sourceId in @(
	'P3.EmptyMeasurements',
	'ManagementApp.Photo0403',
	'Handover.Closed0358',
	'ManagementDb.FalseCompletion0620',
	'PoliceChecklist.PhotoAndAudit'
)) {
	Assert-Contract ($router.Contains($sourceId) -and $director.Contains($sourceId)) `
		"Negligence raw source '$sourceId' is not preserved across router and scene."
}
$negligenceFormulaBlock = Get-BlockBetween $router `
	'bool ConfirmsNegligence(' `
	'bool ConfirmsCatSafe('
Assert-Contract (
	$negligenceFormulaBlock.Contains('bPriorClosure') -and
	$negligenceFormulaBlock.Contains('bUnsafeReopen') -and
	$negligenceFormulaBlock.Contains('bFalseCompletion') -and
	$negligenceFormulaBlock.Contains('bIndependentPoliceAudit') -and
	$negligenceFormulaBlock.Contains('Handover0358') -and
	$negligenceFormulaBlock.Contains('Photo0403') -and
	$negligenceFormulaBlock.Contains('FalseCompletion0620') -and
	$negligenceFormulaBlock.Contains('PolicePhotoAndAudit') -and
	[regex]::IsMatch(
		$negligenceFormulaBlock,
		'return\s+\(bUnsafeReopen\s*&&\s*bFalseCompletion\)\s*\|\|\s*bIndependentPoliceAudit',
		[System.Text.RegularExpressions.RegexOptions]::Singleline)) `
	'Negligence must require prior closure plus raw photo/false or police-audit evidence.'

$p5FormulaSpecs = @(
	@{
		Start = 'bool ConfirmsCatSafe('
		End = 'bool ConfirmsHoseCause('
		Sources = @('CatEnteredPrints', 'CatExitedPrints')
		Pattern = 'return\s+Sources\.Contains\(CatEnteredPrints\)\s*&&\s*Sources\.Contains\(CatExitedPrints\)'
	},
	@{
		Start = 'bool ConfirmsHoseCause('
		End = 'bool ConfirmsFall('
		Sources = @(
			'HosePawCompression',
			'CouplingImpact',
			'InnerRimFriction'
		)
		Pattern = 'return\s+Sources\.Contains\(HosePawCompression\)\s*&&\s*Sources\.Contains\(CouplingImpact\)\s*&&\s*Sources\.Contains\(InnerRimFriction\)'
	},
	@{
		Start = 'bool ConfirmsFall('
		End = 'bool ConfirmsIdentity('
		Sources = @(
			'UpperSlipperEnd',
			'LiftedPad',
			'CorrodedClips',
			'InwardHandSmear'
		)
		Pattern = 'return\s+Sources\.Contains\(UpperSlipperEnd\)\s*&&\s*Sources\.Contains\(LiftedPad\)\s*&&\s*Sources\.Contains\(CorrodedClips\)\s*&&\s*Sources\.Contains\(InwardHandSmear\)'
	},
	@{
		Start = 'bool ConfirmsIdentity('
		End = '}'
		Sources = @(
			'CurrentSleeveRepair',
			'TankSleeveRepair',
			'TankHeelWear',
			'SearchPosterOutfit',
			'Glasses',
			'TankOutfit'
		)
		Pattern = 'return\s+bDirectRepairMatch\s*\|\|\s*bPosterTriangulation'
	}
)
foreach ($formulaSpec in $p5FormulaSpecs) {
	$formulaStart = $router.IndexOf($formulaSpec.Start)
	Assert-Contract ($formulaStart -ge 0) `
		"P5 router formula is missing '$($formulaSpec.Start)'."
	$formulaEnd = if ($formulaSpec.Start -eq 'bool ConfirmsIdentity(') {
		$router.IndexOf("`n}", $formulaStart)
	}
	else {
		$router.IndexOf($formulaSpec.End, $formulaStart)
	}
	Assert-Contract ($formulaEnd -gt $formulaStart) `
		"P5 router formula boundary is missing '$($formulaSpec.Start)'."
	$formulaBlock = $router.Substring(
		$formulaStart,
		$formulaEnd - $formulaStart)
	foreach ($sourceName in $formulaSpec.Sources) {
		Assert-Contract ($formulaBlock.Contains($sourceName)) `
			"P5 router formula '$($formulaSpec.Start)' omits '$sourceName'."
	}
	Assert-Contract ([regex]::IsMatch(
		$formulaBlock,
		$formulaSpec.Pattern,
		[System.Text.RegularExpressions.RegexOptions]::Singleline)) `
		"P5 router formula '$($formulaSpec.Start)' changed its required conjunction."
}
$estimateReadBlock = Get-BlockBetween $chapterThreeClueHandler `
	'else if (Note == EstimateNote && !bEstimateRead)' `
	'else if (Note == RecheckNoticeNote && !bRecheckNoticeRead)'
Assert-Contract ($director.Contains('ManagementDbNote = SpawnNote(') -and
	$director.Contains('RecheckNoticeNote = SpawnNote(') -and
	$director.Contains('PreservationNoticeNote = SpawnNote(') -and
	$director.Contains('PoliceChecklistNote = SpawnNote(') -and
	$director.Contains('실제 입력시각  07/26 06:20:42') -and
	$director.Contains('04:03  강만식  [사진 2장 전송]') -and
	$director.Contains('작성  2024년 7월 29일') -and
	$director.Contains('작성  2024년 7월 30일 23:18') -and
	$estimateReadBlock.Contains('Handover.Closed0358') -and
	-not $estimateReadBlock.Contains('04:10') -and
	-not $estimateReadBlock.Contains('7월 31일') -and
	-not $estimateReadBlock.Contains('ManagementApp.Photo0403') -and
	-not $estimateReadBlock.Contains('ManagementDb.FalseCompletion0620') -and
	$chapterThreeClueHandler.Contains('Note == ManagementDbNote') -and
	$chapterThreeClueHandler.Contains('PoliceChecklist.PhotoAndAudit')) `
	'03:58, 7/29 vendor, 7/30 preservation, photo, DB, and police records must stay temporally separate.'
Assert-Contract (-not ($director -match
	'RegisterTruth\s*\(\s*TEXT\("Truth\.Negligence"\)\s*,\s*TEXT\("P3\.EmptyRecordPlusReopenPhoto"\)')) `
	'P3 empty measurements plus the 04:03 photo must not directly confirm Negligence.'
Assert-Contract (
	$director.Contains('RoofFolderRevealTimer') -and
	$director -match
		'SetTimer\(\s*RoofFolderRevealTimer,\s*this,\s*&ThisClass::RevealRoofInvestigationFolder') `
	'The roof investigation reveal must not share the ladder/ending flow timer.'
$ladderBlock = Get-BlockBetween $director `
	'void AIGThirdMorningDirector::HandleLadderEntered(' `
	'void AIGThirdMorningDirector::RevealTank()'
Assert-Contract (
	$directorHeader.Contains('FTimerHandle LadderEchoTimer;') -and
	$ladderBlock.Contains('if (bEndingFinished)') -and
	$ladderBlock.Contains('SetTimer(') -and
	$ladderBlock.Contains('LadderEchoTimer') -and
	-not $ladderBlock.Contains('FlowTimer')) `
	'The ladder echo must not overwrite or play during the ending timer chain.'
$scratchScheduleBlock = Get-BlockBetween $director `
	'void AIGThirdMorningDirector::ScheduleAccidentScratch()' `
	'void AIGThirdMorningDirector::EvaluateAccidentScratchGate()'
Assert-Contract (
	$scratchScheduleBlock.Contains('CH03.TankOpenedWithoutFlashlight') -and
	$scratchScheduleBlock.Contains('CH03.TankRevealedAfterFlashlight') -and
	$scratchScheduleBlock -match
		'WasOneShotBeatPlayed\(\s*FName\(TEXT\("CH03\.TankOpenedWithoutFlashlight"\)\)\)\s*&&\s*!RebirthState->WasOneShotBeatPlayed') `
	'A restored dark-tank route must defer its first scratch until the rooftop return reveal.'
$checkpointRestoreBlock = Get-BlockBetween $director `
	'void AIGThirdMorningDirector::RestoreCheckpointAnchor(' `
	'void AIGThirdMorningDirector::EndPlay('
Assert-Contract (
	$checkpointRestoreBlock.Contains('bApartmentExitTraversed') -and
	$checkpointRestoreBlock.Contains('EquippedOutfitChapters.Contains(') -and
	$checkpointRestoreBlock.Contains('if (!bRestoreApartment && !bApartmentExitTraversed') -and
	$checkpointRestoreBlock.Contains('MarkOutfitEquipped(FName(TEXT("CH03")))') -and
	$checkpointRestoreBlock.Contains('SetVisibleInteractive(ApartmentDoorAction, true)') -and
	$checkpointRestoreBlock.Contains('TryUnlockApartmentExit()')) `
	'Apartment restores must preserve the unopened door or reconstruct its outfit side effects.'
Assert-Contract (
	$director -match
		'bApartmentDocument\s*\?\s*TEXT\("Checkpoint\.CH03\.Apartment"\)\s*:\s*Phase >= EIGThirdMorningPhase::Roof\s*\?\s*TEXT\("Checkpoint\.CH03\.Roof"\)') `
	'Roof documents must save to the roof anchor before the tank is opened.'
$greyboxStartBlock = Get-BlockBetween $director `
	'void AIGThirdMorningDirector::StartRebirthGreyboxValidation()' `
	'void AIGThirdMorningDirector::ContinueRebirthGreyboxValidation()'
Assert-Contract (
	$greyboxStartBlock.Contains(
		'EIGChapterThreeAction::OpenApartmentDoor') -and
	$director.Contains('ObservedP5Sources.Num() != 15')) `
	'Runtime greybox must expose the current sleeve and preserve all 15 raw observations.'

$pairBlock = Get-BlockBetween $director `
	'bool AIGThirdMorningDirector::IsEvidencePairValid(' `
	'bool AIGThirdMorningDirector::ResolveEvidencePair('
$directPairs = @(
	'EvidenceCatEntered|EvidenceCatExited',
	'EvidenceHosePaw|EvidenceHoseImpact',
	'EvidenceWetRung|EvidenceHandSmear',
	'EvidenceCurrentSleeve|EvidenceTankClothing'
)
$triangulationPairs = @(
	'EvidenceSearchPoster|EvidenceGlasses',
	'EvidenceSearchPoster|EvidenceTankClothing',
	'EvidenceGlasses|EvidenceTankClothing'
)
foreach ($pairKey in $directPairs + $triangulationPairs) {
	$pair = $pairKey.Split('|')
	$firstToken = [regex]::Escape(
		"EIGChapterThreeAction::$($pair[0])")
	$secondToken = [regex]::Escape(
		"EIGChapterThreeAction::$($pair[1])")
	Assert-Contract ($pairBlock -match
		"IsPair\(\s*$firstToken\s*,\s*$secondToken\s*\)") `
		"P5 valid-pair table omits '$($pair[0])/$($pair[1])'."
}
Assert-Contract (-not $pairBlock.Contains('EvidenceBag')) `
	'The convenience-store bag must remain motive evidence, not a P5 proof pair.'
Assert-Contract (
	$pairBlock.Contains('const bool bDirectPair') -and
	$pairBlock.Contains('const bool bPosterTriangulationPair') -and
	$pairBlock.Contains('P5.SearchPosterOutfit') -and
	$pairBlock.Contains('P5.Glasses') -and
	$pairBlock.Contains('P5.TankOutfit')) `
	'Identity triangulation must require all three observed raw sources.'

$evidenceSourceBlock = Get-BlockBetween $director `
	'TArray<FName> EvidenceSources(' `
	'// ---------------------------------------------------------------------------'
$expectedEvidenceBundles = [ordered]@{
	EvidenceCatEntered = @('P5.CatEnteredPrints')
	EvidenceCatExited = @('P5.CatExitedPrints')
	EvidenceHosePaw = @('P5.HosePawCompression')
	EvidenceHoseImpact = @('P5.CouplingImpact', 'P5.InnerRimFriction')
	EvidenceWetRung = @(
		'P5.UpperSlipperEnd',
		'P5.LiftedPad',
		'P5.CorrodedClips'
	)
	EvidenceHandSmear = @('P5.InwardHandSmear')
	EvidenceGlasses = @('P5.Glasses')
	EvidenceTankClothing = @(
		'P5.TankSleeveThreeStitches',
		'P5.TankHeelWear',
		'P5.TankOutfit'
	)
	EvidenceCurrentSleeve = @('P5.CurrentSleeveThreeStitches')
	EvidenceSearchPoster = @('P5.SearchPosterOutfit')
}
$mappedRawSourceCount = 0
foreach ($entry in $expectedEvidenceBundles.GetEnumerator()) {
	$actionToken = [regex]::Escape(
		"EIGChapterThreeAction::$($entry.Key)")
	$match = [regex]::Match(
		$evidenceSourceBlock,
		"case\s+$actionToken\s*:\s*return\s*\{(?<body>.*?)\};",
		[System.Text.RegularExpressions.RegexOptions]::Singleline)
	Assert-Contract $match.Success `
		"P5 raw-source bundle is missing '$($entry.Key)'."
	$body = $match.Groups['body'].Value
	foreach ($sourceId in $entry.Value) {
		Assert-Contract ($body.Contains($sourceId)) `
			"P5 '$($entry.Key)' bundle omits '$sourceId'."
		$mappedRawSourceCount++
	}
	Assert-Contract (
		([regex]::Matches($body, 'P5\.')).Count -eq
			$entry.Value.Count) `
		"P5 '$($entry.Key)' bundle contains an unexpected raw source."
}
Assert-Contract (
	$mappedRawSourceCount -eq 15 -and
	$evidenceSourceBlock -match
		'case\s+EIGChapterThreeAction::EvidenceBag:\s*case\s+EIGChapterThreeAction::None:\s*default:\s*return\s*\{\s*\};') `
	'P5 must map 11 nodes to exactly 15 raw sources while Bag maps to none.'
Assert-Contract ($director.Contains(
	'else if (IsEvidencePairValid(FocusedEvidence, Pair.Key))')) `
	'Compare prompt must only appear on evidence that can pair with the focus.'
Assert-Contract ($director.Contains('P5.InwardHandSmear') -and
	$director.Contains('P5.CouplingImpact') -and
	$director.Contains('P5.InnerRimFriction') -and
	$director.Contains('P5.UpperSlipperEnd') -and
	$director.Contains('P5.LiftedPad') -and
	$director.Contains('P5.CorrodedClips') -and
	$director.Contains('P5.CurrentSleeveThreeStitches') -and
	$director.Contains('P5.TankSleeveThreeStitches') -and
	$director.Contains('P5.TankHeelWear') -and
	$director.Contains('P5.TankOutfit')) `
	'P5 must preserve inward-direction, water-contact, and unique-outfit sources.'
Assert-Contract ($director.Contains('bComparisonWasArmed') -and
	$director.Contains('CandidateSources') -and
	$director.Contains('State.ObservedP5Sources = ObservedP5Sources') -and
	$director.Contains('CH03.SearchPoster')) `
	'P5 must separate observation from explicit comparison and let the search poster pay search debt.'
Assert-Contract (
	$director.Contains('NominalCapacityMl = 500.0f') -and
	$director.Contains('NominalCapacityMl = 1000.0f') -and
	$director.Contains('NominalCapacityMl = 2000.0f') -and
	$director.Contains('PurchaseBagRotation = FRotator(78.0f, 20.0f, -8.0f)') -and
	$director.Contains('PurchaseBagQuat.GetAxisX().Z') -and
	$director.Contains('PurchaseBagQuat.GetAxisY().Z') -and
	$director.Contains('PurchaseBagQuat.GetAxisZ().Z') -and
	$director.Contains(
		'0.94f - ConsumedMilliliters / NominalCapacityMl') -and
	$director.Contains('const float RingZ = 18.55f') -and
	$director.Contains('const bool bHasBottleMesh') -and
	$director.Contains('const bool bHasLabelMesh') -and
	$director.Contains('const bool bHasCapMesh') -and
	$director.Contains('EIGRebirthBottleClosureState::MissingCap') -and
	$director.Contains('P5.PurchaseChoiceContinuity')) `
	'P5 purchase evidence must retain profile dimensions, lay the 2L bag down on its OBB, consume absolute millilitres, show the broken tamper ring, and keep safe primitive fallbacks.'
Assert-Contract (
	$director.Contains('CH03.TankOpenedWithoutFlashlight') -and
	$director.Contains('CH03.TankRevealedAfterFlashlight') -and
	$director.Contains('FlashlightReturnRevealZone') -and
	$director.Contains('SetActorEnableCollision(false)')) `
	'Opening the tank in darkness must retain a one-shot flashlight recovery and roof-return reveal.'
Assert-Contract (
	$director.Contains('P3BleedTubeVisual') -and
	$director.Contains('P3BleedWaterVisual') -and
	$director.Contains('P3ZeroConfirmationTicks') -and
	$director.Contains('P3ZeroConfirmationTicks + 1') -and
	$director.Contains('P3ZeroConfirmationTicks >= 2') -and
	$director.Contains('P3HintElapsedSeconds') -and
	$director.Contains('90.0f, 150.0f, 210.0f') -and
	$director.Contains('TriggerBrownOut(1.2f)')) `
	'P3 must expose the bleed tube, two zero ticks, persisted 90/150/210 hints, and a 1.2-second mistake brownout.'
Assert-Contract ($director.Contains('AccidentScratchCount == 3') -and
	$director.Contains('bAccidentScratchTailSettled') -and
	$story.Contains('AccidentScratchCount==3')) `
	'Final choice must remain gated until three scratches and the 0.6-second tail have completed.'
$scratchBlock = Get-BlockBetween $director `
	'void AIGThirdMorningDirector::PlayAccidentScratch()' `
	'void AIGThirdMorningDirector::SettleAccidentScratchTail()'
$scratchGateBlock = Get-BlockBetween $director `
	'void AIGThirdMorningDirector::EvaluateAccidentScratchGate()' `
	'void AIGThirdMorningDirector::PlayAccidentScratch()'
Assert-Contract (
	$scratchGateBlock.Contains('TankAttention < 0.25f') -and
	$scratchGateBlock.Contains('Elapsed >= 3.0') -and
	$scratchGateBlock.Contains('Elapsed >= 4.0') -and
	$scratchGateBlock.Contains('!bScratchGatePlayerWasMoving') -and
	$director.Contains('State.bLookedAwayAfterFirstScratch') -and
	$director.Contains('State.bActedAfterSecondScratch') -and
	$director.Contains('ScheduleAccidentScratch();')) `
	'Scratches two and three must require persisted look-away/action gates rather than timer-only playback.'
$handleActionBlock = Get-BlockBetween $director `
	'void AIGThirdMorningDirector::HandleAction(' `
	'void AIGThirdMorningDirector::TryUnlockApartmentExit()'
$evidenceActionBlock = Get-BlockBetween $handleActionBlock `
	'case EIGChapterThreeAction::EvidenceCatEntered:' `
	'case EIGChapterThreeAction::OpenTank:'
$evidenceActionSetIndex =
	$evidenceActionBlock.IndexOf('bActedAfterSecondScratch = true;')
$evidenceActionCommitIndex = $evidenceActionBlock.IndexOf(
	'CommitChapterThreeState();',
	$evidenceActionSetIndex)
$evidenceActionSaveIndex = $evidenceActionBlock.IndexOf(
	'RequestCheckpointAutosave(TEXT("Checkpoint.CH03.Roof"))',
	$evidenceActionCommitIndex)
$evidenceActionContinueIndex =
	$evidenceActionBlock.IndexOf('FocusOrCompareEvidence(Action);')
$ladderActionSetIndex =
	$ladderBlock.IndexOf('bActedAfterSecondScratch = true;')
$ladderActionCommitIndex = $ladderBlock.IndexOf(
	'CommitChapterThreeState();',
	$ladderActionSetIndex)
$ladderActionSaveIndex = $ladderBlock.IndexOf(
	'RequestCheckpointAutosave(TEXT("Checkpoint.CH03.Roof"))',
	$ladderActionCommitIndex)
$ladderActionContinueIndex =
	$ladderBlock.IndexOf('ScheduleAccidentScratch();')
Assert-Contract (
	$evidenceActionBlock.Contains('!bActedAfterSecondScratch') -and
	$evidenceActionSetIndex -ge 0 -and
	$evidenceActionCommitIndex -gt $evidenceActionSetIndex -and
	$evidenceActionSaveIndex -gt $evidenceActionCommitIndex -and
	$evidenceActionSaveIndex -lt $evidenceActionContinueIndex -and
	$ladderBlock.Contains('!bActedAfterSecondScratch') -and
	$ladderActionSetIndex -ge 0 -and
	$ladderActionCommitIndex -gt $ladderActionSetIndex -and
	$ladderActionSaveIndex -gt $ladderActionCommitIndex -and
	$ladderActionSaveIndex -lt $ladderActionContinueIndex) `
	'Every physical second-scratch action path must persist before the next timer tick can advance it.'
$endingACueBlock = Get-BlockBetween $director `
	'void AIGThirdMorningDirector::FinishEndingAAfterDiscovery()' `
	'void AIGThirdMorningDirector::FinishEndingB()'
$endingBCueBlock = Get-BlockBetween $director `
	'void AIGThirdMorningDirector::BeginEndingB()' `
	'void AIGThirdMorningDirector::ShowCommonDiscoveryCard()'
$endingBGuardIndex = $endingBCueBlock.IndexOf('Ending.B.StrongCuePlayed')
$endingBSaveIndex = $endingBCueBlock.IndexOf(
	'RequestCheckpointAutosave(TEXT("Checkpoint.CH03.Roof"))',
	$endingBGuardIndex)
Assert-Contract ($scratchBlock.Contains(
	'RequestCheckpointAutosave(TEXT("Checkpoint.CH03.Roof"))') -and
	$endingACueBlock.IndexOf('Ending.A.StrongCuePlayed') -ge 0 -and
	$endingACueBlock.IndexOf(
		'RequestCheckpointAutosave(TEXT("Checkpoint.CH03.Roof"))') -gt
		$endingACueBlock.IndexOf('Ending.A.StrongCuePlayed') -and
	$endingACueBlock.IndexOf(
		'RequestCheckpointAutosave(TEXT("Checkpoint.CH03.Roof"))') -lt
		$endingACueBlock.IndexOf('CreateDoorChime') -and
	$endingBGuardIndex -ge 0 -and
	$endingBSaveIndex -gt $endingBGuardIndex -and
	$endingBSaveIndex -lt
		$endingBCueBlock.IndexOf('CreateWaterDripMetalRing')) `
	'Every scratch and both ending strong cues must persist before they can replay.'
Assert-Contract ($director.Contains('Ending.CommonDiscoveryCard')) `
	'Both endings must register the same common-discovery source.'
Assert-Contract (([regex]::Matches(
	$director,
	'&ThisClass::ShowCommonDiscoveryCard')).Count -eq 2 -and
	$director.Contains('FText::GetEmpty()')) `
	'Ending A and B must both enter the neutral common-discovery card before emotional divergence.'

$expectedDays = @{
	'2024-07-26' = [DayOfWeek]::Friday
	'2024-07-30' = [DayOfWeek]::Tuesday
	'2024-07-31' = [DayOfWeek]::Wednesday
}
foreach ($entry in $expectedDays.GetEnumerator()) {
	$actual = ([datetime]::ParseExact(
		$entry.Key,
		'yyyy-MM-dd',
		[Globalization.CultureInfo]::InvariantCulture)).DayOfWeek
	Assert-Contract ($actual -eq $entry.Value) `
		"Canonical date $($entry.Key) has the wrong weekday."
}
Assert-Contract (($story.Contains('2024년 7월 26일(금)') -or
	$story.Contains('2024년 7월 26일 금요일')) -and
	$story.Contains('7월 30일') -and
	$story.Contains('7월 31일 04:00')) `
	'Canonical dates are not consistently present in the story master.'
Assert-Contract (
	$signTextureGenerator.Contains("'2024  7월'") -and
	$signTextureGenerator.Contains('$day -eq 26') -and
	$signTextureGenerator.Contains('2024-07-01 was Monday') -and
	-not $signTextureGenerator.Contains("'2026  7월'")) `
	'The source calendar texture generator must show Friday, 2024-07-26 rather than a mismatched year.'

# Exhaustive model check: 2^9 C5 truth subsets × tank/identity/look booleans.
$c5StateCount = 0
$choiceOpenCount = 0
for ($mask = 0; $mask -lt (1 -shl $c5Truths.Count); $mask++) {
	foreach ($tankOpened in @($false, $true)) {
		foreach ($identityRecognized in @($false, $true)) {
			foreach ($lookingInside in @($false, $true)) {
				$c5StateCount++
				$allFacts = $mask -eq ((1 -shl $c5Truths.Count) - 1)
				$choiceAvailable =
					$allFacts -and $tankOpened -and
					$identityRecognized -and $lookingInside
				if ($choiceAvailable) {
					$choiceOpenCount++
				}
				Assert-Contract (-not $choiceAvailable -or
					($allFacts -and $tankOpened -and
						$identityRecognized -and $lookingInside)) `
					'Final choice opened outside the complete C5 state.'
			}
		}
	}
}
Assert-Contract ($c5StateCount -eq 4096 -and $choiceOpenCount -eq 1) `
	'C5 exhaustive model did not produce exactly one fully valid state.'

# Exhaustive final-presentation gate:
# facts, tank, identity, look, scratch 0..3, tail unsettled/settled.
$scratchGateCaseCount = 0
$scratchGateOpenCount = 0
foreach ($allFacts in @($false, $true)) {
	foreach ($tankOpened in @($false, $true)) {
		foreach ($identityRecognized in @($false, $true)) {
			foreach ($lookingInside in @($false, $true)) {
				foreach ($scratchCount in 0..3) {
					foreach ($tailSettled in @($false, $true)) {
						$scratchGateCaseCount++
						$choiceAvailable =
							$allFacts -and $tankOpened -and
							$identityRecognized -and $lookingInside -and
							$scratchCount -eq 3 -and $tailSettled
						if ($choiceAvailable) {
							$scratchGateOpenCount++
						}
						Assert-Contract (-not $choiceAvailable -or
							($allFacts -and $tankOpened -and
								$identityRecognized -and $lookingInside -and
								$scratchCount -eq 3 -and $tailSettled)) `
							'Final choice bypassed the three-scratch tail gate.'
					}
				}
			}
		}
	}
}
Assert-Contract ($scratchGateCaseCount -eq 128 -and
	$scratchGateOpenCount -eq 1) `
	'Final presentation matrix must contain 128 states and exactly one valid state.'

# Exhaustive P5 cold ordered-pair model: 11 × 10 = 110.
$evidence = @(
	'CatEntered',
	'CatExited',
	'HosePaw',
	'HoseImpact',
	'Bag',
	'WetRung',
	'HandSmear',
	'Glasses',
	'TankClothing',
	'CurrentSleeve',
	'SearchPoster'
)
$validPairKeys = @(
	'CatEntered|CatExited',
	'HoseImpact|HosePaw',
	'HandSmear|WetRung',
	'CurrentSleeve|TankClothing'
)
$orderedPairCount = 0
$validOrderedPairCount = 0
for ($firstIndex = 0; $firstIndex -lt $evidence.Count; $firstIndex++) {
	for ($secondIndex = $firstIndex + 1; $secondIndex -lt $evidence.Count; $secondIndex++) {
		$sortedPair = @($evidence[$firstIndex], $evidence[$secondIndex]) |
			Sort-Object
		$key = $sortedPair -join '|'
		foreach ($direction in 0..1) {
			$orderedPairCount++
			if ($validPairKeys -contains $key) {
				$validOrderedPairCount++
			}
		}
	}
}
Assert-Contract ($orderedPairCount -eq 110 -and $validOrderedPairCount -eq 8) `
	'P5 cold ordered-pair matrix must contain 110 attempts and exactly 8 direct valid directions.'
Assert-Contract (-not ($validPairKeys -contains 'Bag|WetRung')) `
	'Bag plus wet rung must never prove the fall.'
$selfPairCount = 0
foreach ($evidenceName in $evidence) {
	$selfPairCount++
	Assert-Contract (-not ($validPairKeys -contains "$evidenceName|$evidenceName")) `
		"P5 evidence '$evidenceName' must not prove itself."
}
Assert-Contract ($selfPairCount -eq 11) `
	'P5 self-pair matrix must cover all eleven evidence items.'

# Every order of poster, glasses, and tank outfit must resolve on the third
# held comparison. Candidate sources include the target currently under the
# reticle, so no fourth click is required.
$triangulationPermutationCount = 0
$triangulationItems = @('SearchPoster', 'Glasses', 'TankClothing')
foreach ($first in $triangulationItems) {
	foreach ($second in $triangulationItems | Where-Object { $_ -ne $first }) {
		$third = $triangulationItems |
			Where-Object { $_ -ne $first -and $_ -ne $second } |
			Select-Object -First 1
		$observed = [System.Collections.Generic.HashSet[string]]::new()
		[void]$observed.Add($first)
		[void]$observed.Add($second)
		$candidate = [System.Collections.Generic.HashSet[string]]::new($observed)
		[void]$candidate.Add($third)
		$resolved = @($triangulationItems |
			Where-Object { $candidate.Contains($_) }).Count -eq 3
		Assert-Contract $resolved `
			"Identity triangulation failed for $first -> $second -> $third."
		$triangulationPermutationCount++
	}
}
Assert-Contract ($triangulationPermutationCount -eq 6) `
	'Identity triangulation must cover all six acquisition orders.'

# Raw observation subsets never become truths by themselves. These predicates
# model committed comparison sources only.
$p5SubsetCaseCount = 0
$p5SubsetConfirmedCount = 0
foreach ($sourceCount in @(2, 3, 4)) {
	for ($mask = 0; $mask -lt (1 -shl $sourceCount); $mask++) {
		$p5SubsetCaseCount++
		$allRequired = $mask -eq ((1 -shl $sourceCount) - 1)
		if ($allRequired) {
			$p5SubsetConfirmedCount++
		}
	}
}
for ($mask = 0; $mask -lt 64; $mask++) {
	$p5SubsetCaseCount++
	$directIdentity =
		($mask -band 1) -ne 0 -and
		($mask -band 2) -ne 0 -and
		($mask -band 4) -ne 0
	$posterIdentity =
		($mask -band 8) -ne 0 -and
		($mask -band 16) -ne 0 -and
		($mask -band 32) -ne 0
	if ($directIdentity -or $posterIdentity) {
		$p5SubsetConfirmedCount++
	}
}
Assert-Contract ($p5SubsetCaseCount -eq 92 -and
	$p5SubsetConfirmedCount -eq 18) `
	'P5 raw category subsets must prove only the four fully committed formulas.'
Assert-Contract ($director.Contains(
	'REBIRTH_GREYBOX FAIL P5 observation-only save')) `
	'Runtime greybox must reject truth confirmation from observation-only save/load.'

# Exhaustive negligence source subsets: 03:58 prior closure is mandatory;
# either photo+false record or the independent police audit proves the rest.
$negligenceSourceCount = 0
$negligenceConfirmedCount = 0
for ($mask = 0; $mask -lt 32; $mask++) {
	$hasEmpty = ($mask -band 1) -ne 0
	$hasPhoto = ($mask -band 2) -ne 0
	$hasHandover = ($mask -band 4) -ne 0
	$hasFalseRecord = ($mask -band 8) -ne 0
	$hasPoliceAudit = ($mask -band 16) -ne 0
	$confirmed = $hasHandover -and (
		($hasPhoto -and $hasFalseRecord) -or $hasPoliceAudit)
	$negligenceSourceCount++
	if ($confirmed) {
		$negligenceConfirmedCount++
	}
	Assert-Contract (-not $confirmed -or $hasHandover) `
		'P3-only evidence prematurely confirmed Negligence.'
}
Assert-Contract ($negligenceSourceCount -eq 32 -and
	$negligenceConfirmedCount -eq 10) `
	'Negligence matrix must have exactly ten prior-closure-valid source subsets.'

Write-Host (
	'REBIRTH narrative contract passed ' +
	"(truths=12, c5_states=$c5StateCount, " +
	"scratch_gate_states=$scratchGateCaseCount, " +
	"p5_ordered_pairs=$orderedPairCount, p5_self_pairs=$selfPairCount, " +
	"p5_tri_orders=$triangulationPermutationCount, p5_subsets=$p5SubsetCaseCount, " +
	"negligence_subsets=$negligenceSourceCount)."
)
Write-Host (
	'Static oracle only: Unreal build, collision/physics, audio/art, save/load, ' +
	'packaging, and human playtests remain separate release gates.'
)
