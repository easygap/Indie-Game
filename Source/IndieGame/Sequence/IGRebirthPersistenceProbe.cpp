#include "Sequence/IGRebirthPersistenceProbe.h"

#include "CollisionQueryParams.h"
#include "CollisionShape.h"
#include "Core/IGPrologueWorldScene.h"
#include "Engine/GameInstance.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "HAL/PlatformMisc.h"
#include "IndieGame.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Materials/MaterialInterface.h"
#include "Narrative/IGRebirthNarrativeSubsystem.h"
#include "Narrative/IGStoryStateSubsystem.h"
#include "Save/IGSaveGame.h"
#include "Save/IGSaveSubsystem.h"
#include "Sequence/IGChapterOneIncidentDirector.h"
#include "Sequence/IGSecondMorningDirector.h"
#include "Sequence/IGThirdMorningDirector.h"
#include "TimerManager.h"

namespace IGRebirthProbe
{
	const FName ActualStateRestored(TEXT("CH03.ActualStateRestored"));

	FGameplayTag Tag(const TCHAR* Name)
	{
		return FGameplayTag::RequestGameplayTag(FName(Name), false);
	}

	void AddSources(
		UIGRebirthNarrativeSubsystem* State,
		const TCHAR* TruthName,
		const TArray<FName>& Sources,
		const bool bConfirm = true)
	{
		for (const FName Source : Sources)
		{
			State->RegisterTruthSource(Tag(TruthName), Source, bConfirm);
		}
	}
}

AIGRebirthPersistenceProbe::AIGRebirthPersistenceProbe()
{
	PrimaryActorTick.bCanEverTick = false;
}

void AIGRebirthPersistenceProbe::BeginPlay()
{
	Super::BeginPlay();
	GetWorldTimerManager().SetTimer(
		StartTimer,
		this,
		&ThisClass::StartProbe,
		0.10f,
		false);
}

void AIGRebirthPersistenceProbe::StartProbe()
{
	if (!FParse::Value(
			FCommandLine::Get(),
			TEXT("IGRebirthPersistenceProbe="),
			ProbeMode)
		|| !FParse::Value(
			FCommandLine::Get(),
			TEXT("IGRebirthValidationSlot="),
			SlotName)
		|| SlotName.IsEmpty())
	{
		ExitFailure(TEXT("probe mode or slot is missing"));
		return;
	}

	FString EndingValue;
	if (FParse::Value(
			FCommandLine::Get(),
			TEXT("IGRebirthEnding="),
			EndingValue))
	{
		bEndingA = EndingValue.Equals(TEXT("A"), ESearchCase::IgnoreCase);
	}
	FParse::Value(
		FCommandLine::Get(),
		TEXT("IGRebirthCH02TimeCheckpoint="),
		CH02TimeCheckpointIndex);
	FParse::Value(
		FCommandLine::Get(),
		TEXT("IGRebirthP5Checkpoint="),
		P5CheckpointIndex);
	FParse::Value(
		FCommandLine::Get(),
		TEXT("IGRebirthP3Checkpoint="),
		P3CheckpointIndex);
	FParse::Value(
		FCommandLine::Get(),
		TEXT("IGRebirthAnchorCase="),
		AnchorCase);
	FParse::Value(
		FCommandLine::Get(),
		TEXT("IGRebirthCatChoiceCase="),
		CatChoiceCase);

	if (ProbeMode.Equals(TEXT("CH02TimeWrite"), ESearchCase::IgnoreCase))
	{
		StartCH02TimeWrite();
	}
	else if (ProbeMode.Equals(
		TEXT("BoundaryBeforeWrite"),
		ESearchCase::IgnoreCase))
	{
		StartBoundaryWrite(false);
	}
	else if (ProbeMode.Equals(
		TEXT("BoundaryBeforeRead"),
		ESearchCase::IgnoreCase))
	{
		StartBoundaryRead(false);
	}
	else if (ProbeMode.Equals(
		TEXT("BoundaryAfterWrite"),
		ESearchCase::IgnoreCase))
	{
		StartBoundaryWrite(true);
	}
	else if (ProbeMode.Equals(
		TEXT("BoundaryAfterRead"),
		ESearchCase::IgnoreCase))
	{
		StartBoundaryRead(true);
	}
	else if (ProbeMode.Equals(TEXT("CatChoiceWrite"), ESearchCase::IgnoreCase))
	{
		StartCatChoiceWrite();
	}
	else if (ProbeMode.Equals(TEXT("CatChoiceRead"), ESearchCase::IgnoreCase))
	{
		StartCatChoiceRead();
	}
	else if (ProbeMode.Equals(TEXT("CH02TimeRead"), ESearchCase::IgnoreCase))
	{
		StartCH02TimeRead();
	}
	else if (ProbeMode.Equals(TEXT("P5Write"), ESearchCase::IgnoreCase))
	{
		StartP5Write();
	}
	else if (ProbeMode.Equals(TEXT("P5Read"), ESearchCase::IgnoreCase))
	{
		StartP5Read();
	}
	else if (ProbeMode.Equals(TEXT("P3Write"), ESearchCase::IgnoreCase))
	{
		StartP3Write();
	}
	else if (ProbeMode.Equals(TEXT("P3Read"), ESearchCase::IgnoreCase))
	{
		StartP3Read();
	}
	else if (ProbeMode.Equals(TEXT("AnchorWrite"), ESearchCase::IgnoreCase))
	{
		StartAnchorWrite();
	}
	else if (ProbeMode.Equals(TEXT("AnchorRead"), ESearchCase::IgnoreCase))
	{
		StartAnchorRead();
	}
	else if (ProbeMode.Equals(TEXT("EndingWrite"), ESearchCase::IgnoreCase))
	{
		StartEndingWrite();
	}
	else if (ProbeMode.Equals(TEXT("EndingCommit"), ESearchCase::IgnoreCase)
		|| ProbeMode.Equals(TEXT("EndingVerify"), ESearchCase::IgnoreCase))
	{
		StartEndingRead();
	}
	else
	{
		ExitFailure(TEXT("unknown probe mode"));
	}
}

void AIGRebirthPersistenceProbe::StartBoundaryWrite(
	const bool bAfterBoundary)
{
	UIGRebirthNarrativeSubsystem* Narrative =
		GetGameInstance()
			? GetGameInstance()->GetSubsystem<UIGRebirthNarrativeSubsystem>()
			: nullptr;
	UIGStoryStateSubsystem* StoryState =
		GetGameInstance()
			? GetGameInstance()->GetSubsystem<UIGStoryStateSubsystem>()
			: nullptr;
	UIGSaveSubsystem* Save =
		GetGameInstance()
			? GetGameInstance()->GetSubsystem<UIGSaveSubsystem>()
			: nullptr;
	if (!Narrative || !StoryState || !Save || Save->IsBusy())
	{
		ExitFailure(TEXT("memory-boundary writer subsystem unavailable"));
		return;
	}
	if (UGameplayStatics::DoesSaveGameExist(SlotName, 0))
	{
		UGameplayStatics::DeleteGameInSlot(SlotName, 0);
	}
	Narrative->ResetNarrative();
	StoryState->ClearStates(false);
	const FGameplayTag ReachedFourthFloor = IGRebirthProbe::Tag(
		TEXT("State.CH01.Incident.ReachedFourthFloor"));
	const FGameplayTag MemoryBoundary = IGRebirthProbe::Tag(
		TEXT("State.CH01.Incident.MemoryBoundary"));
	const FGameplayTag BagPlaced = IGRebirthProbe::Tag(
		TEXT("State.CH01.Incident.BagPlacedAtLadder"));
	const FGameplayTag ChapterTwoStarted = IGRebirthProbe::Tag(
		TEXT("State.CH02.Loop.Started"));
	const FGameplayTag ChapterTwoAlarmStopped = IGRebirthProbe::Tag(
		TEXT("State.CH02.Wake.AlarmStopped"));
	const FGameplayTag ChapterTwoStanding = IGRebirthProbe::Tag(
		TEXT("State.CH02.Wake.Standing"));
	if (bAfterBoundary)
	{
		StoryState->AddState(ChapterTwoStarted);
		StoryState->AddState(ChapterTwoAlarmStopped);
		StoryState->AddState(ChapterTwoStanding);
		Narrative->MarkOneShotBeatPlayed(
			FName(TEXT("CH01.BagPlacedAtLadder")));
	}
	else
	{
		StoryState->AddState(ReachedFourthFloor);
	}

	Save->OnSaveCompleted.AddUniqueDynamic(
		this,
		&ThisClass::HandleSaveCompleted);
	const bool bSaveAccepted = Save->RequestSave(
		SlotName,
		IGRebirthProbe::Tag(
			bAfterBoundary ? TEXT("Chapter.CH02") : TEXT("Chapter.CH01")),
		NAME_None,
		IGRebirthProbe::Tag(
			bAfterBoundary
				? TEXT("Checkpoint.CH02.Woke")
				: TEXT("Checkpoint.CH01.FourthFloor")));
	if (!bSaveAccepted)
	{
		ExitFailure(TEXT("memory-boundary writer save request rejected"));
		return;
	}
	if (!bAfterBoundary)
	{
		// Mirror BeginMemoryBoundary after the immutable save snapshot has been
		// captured. These transient mutations must not leak into the disk image.
		StoryState->AddState(MemoryBoundary);
		StoryState->AddState(BagPlaced);
		Narrative->MarkOneShotBeatPlayed(
			FName(TEXT("CH01.BagPlacedAtLadder")));
	}
}

void AIGRebirthPersistenceProbe::StartBoundaryRead(
	const bool bAfterBoundary)
{
	UIGSaveSubsystem* Save =
		GetGameInstance()
			? GetGameInstance()->GetSubsystem<UIGSaveSubsystem>()
			: nullptr;
	if (!Save || Save->IsBusy())
	{
		ExitFailure(TEXT("memory-boundary reader subsystem unavailable"));
		return;
	}
	Save->OnLoadCompleted.AddUniqueDynamic(
		this,
		&ThisClass::HandleLoadCompleted);
	if (!Save->RequestLoad(SlotName))
	{
		ExitFailure(TEXT("memory-boundary reader load request rejected"));
	}
}

bool AIGRebirthPersistenceProbe::ResolveCatChoiceContract(
	FIGRebirthChoiceState& OutChoices) const
{
	OutChoices = FIGRebirthChoiceState();
	OutChoices.PurchaseProfile = EIGRebirthPurchaseProfile::ProfileA500MlX2;
	OutChoices.PaymentMethod = EIGRebirthPaymentMethod::WalletCard;
	if (CatChoiceCase.Equals(TEXT("CapLeft"), ESearchCase::IgnoreCase))
	{
		OutChoices.CatWaterState = EIGRebirthCatWaterState::BottleCap;
		OutChoices.BottleClosureState =
			EIGRebirthBottleClosureState::MissingCap;
		return true;
	}
	if (CatChoiceCase.Equals(TEXT("CapWaited"), ESearchCase::IgnoreCase))
	{
		OutChoices.CatWaterState = EIGRebirthCatWaterState::BottleCap;
		OutChoices.BottleClosureState = EIGRebirthBottleClosureState::Resealed;
		OutChoices.bWaitedForCat = true;
		return true;
	}
	if (CatChoiceCase.Equals(TEXT("CupLeft"), ESearchCase::IgnoreCase)
		|| CatChoiceCase.Equals(TEXT("CupWaited"), ESearchCase::IgnoreCase))
	{
		OutChoices.bHasPaperCup = true;
		OutChoices.CatWaterState = EIGRebirthCatWaterState::PaperCup;
		OutChoices.BottleClosureState = EIGRebirthBottleClosureState::Resealed;
		OutChoices.bWaitedForCat = CatChoiceCase.Equals(
			TEXT("CupWaited"),
			ESearchCase::IgnoreCase);
		return true;
	}
	if (CatChoiceCase.Equals(TEXT("PassedBy"), ESearchCase::IgnoreCase))
	{
		OutChoices.CatWaterState = EIGRebirthCatWaterState::PassedBy;
		OutChoices.BottleClosureState = EIGRebirthBottleClosureState::Resealed;
		return true;
	}
	return false;
}

void AIGRebirthPersistenceProbe::StartCatChoiceWrite()
{
	FIGRebirthChoiceState Choices;
	if (!ResolveCatChoiceContract(Choices))
	{
		ExitFailure(TEXT("cat choice case is invalid"));
		return;
	}
	UIGRebirthNarrativeSubsystem* Narrative =
		GetGameInstance()
			? GetGameInstance()->GetSubsystem<UIGRebirthNarrativeSubsystem>()
			: nullptr;
	UIGStoryStateSubsystem* StoryState =
		GetGameInstance()
			? GetGameInstance()->GetSubsystem<UIGStoryStateSubsystem>()
			: nullptr;
	UIGSaveSubsystem* Save =
		GetGameInstance()
			? GetGameInstance()->GetSubsystem<UIGSaveSubsystem>()
			: nullptr;
	if (!Narrative || !StoryState || !Save || Save->IsBusy())
	{
		ExitFailure(TEXT("cat choice writer subsystem unavailable"));
		return;
	}
	if (UGameplayStatics::DoesSaveGameExist(SlotName, 0))
	{
		UGameplayStatics::DeleteGameInSlot(SlotName, 0);
	}

	Narrative->ResetNarrative();
	Narrative->SetChoices(Choices);
	StoryState->ClearStates(false);
	StoryState->AddState(IGRebirthProbe::Tag(
		TEXT("State.CH01.Incident.Drank")));
	StoryState->AddState(IGRebirthProbe::Tag(
		TEXT("State.CH01.Incident.CatChoiceCommitted")));
	StoryState->AddState(IGRebirthProbe::Tag(
		TEXT("State.CH01.Incident.ThirdGustOccurred")));
	Save->OnSaveCompleted.AddUniqueDynamic(
		this,
		&ThisClass::HandleSaveCompleted);
	if (!Save->RequestSave(
			SlotName,
			IGRebirthProbe::Tag(TEXT("Chapter.CH01")),
			NAME_None,
			IGRebirthProbe::Tag(TEXT("Checkpoint.CH01.ReturnAlley"))))
	{
		ExitFailure(TEXT("cat choice writer save request rejected"));
	}
}

void AIGRebirthPersistenceProbe::StartCatChoiceRead()
{
	FIGRebirthChoiceState Choices;
	if (!ResolveCatChoiceContract(Choices))
	{
		ExitFailure(TEXT("cat choice case is invalid"));
		return;
	}
	UIGSaveSubsystem* Save =
		GetGameInstance()
			? GetGameInstance()->GetSubsystem<UIGSaveSubsystem>()
			: nullptr;
	if (!Save || Save->IsBusy())
	{
		ExitFailure(TEXT("cat choice reader subsystem unavailable"));
		return;
	}
	Save->OnLoadCompleted.AddUniqueDynamic(
		this,
		&ThisClass::HandleLoadCompleted);
	if (!Save->RequestLoad(SlotName))
	{
		ExitFailure(TEXT("cat choice reader load request rejected"));
	}
}

bool AIGRebirthPersistenceProbe::ResolveAnchorContract(
	FGameplayTag& OutChapter,
	FGameplayTag& OutCheckpoint,
	FVector& OutLocation,
	FRotator& OutRotation) const
{
	const FVector ChapterThreeOrigin =
		AIGThirdMorningDirector::GetStageOrigin();
	if (AnchorCase.Equals(TEXT("CH02Corridor"), ESearchCase::IgnoreCase))
	{
		OutChapter = IGRebirthProbe::Tag(TEXT("Chapter.CH02"));
		OutCheckpoint = IGRebirthProbe::Tag(TEXT("Checkpoint.CH02.Corridor"));
		OutLocation = FVector(430.0f, -305.0f, 998.0f);
		OutRotation = FRotator(0.0f, -180.0f, 0.0f);
		return true;
	}
	if (AnchorCase.Equals(TEXT("CH02Store"), ESearchCase::IgnoreCase))
	{
		OutChapter = IGRebirthProbe::Tag(TEXT("Chapter.CH02"));
		OutCheckpoint = IGRebirthProbe::Tag(TEXT("Checkpoint.CH02.Store"));
		OutLocation = FVector(2515.0f, -455.0f, 104.0f);
		OutRotation = FRotator(0.0f, -180.0f, 0.0f);
		return true;
	}
	if (AnchorCase.Equals(TEXT("CH03Apartment"), ESearchCase::IgnoreCase))
	{
		OutChapter = IGRebirthProbe::Tag(TEXT("Chapter.CH03"));
		OutCheckpoint = IGRebirthProbe::Tag(TEXT("Checkpoint.CH03.Apartment"));
		OutLocation = ChapterThreeOrigin + FVector(35.0f, -55.0f, 98.0f);
		OutRotation = FRotator(0.0f, -142.0f, 0.0f);
		return true;
	}
	if (AnchorCase.Equals(TEXT("CH03Flood"), ESearchCase::IgnoreCase))
	{
		OutChapter = IGRebirthProbe::Tag(TEXT("Chapter.CH03"));
		OutCheckpoint = IGRebirthProbe::Tag(TEXT("Checkpoint.CH03.Flood"));
		OutLocation = ChapterThreeOrigin + FVector(520.0f, 0.0f, 98.0f);
		OutRotation = FRotator::ZeroRotator;
		return true;
	}
	if (AnchorCase.Equals(TEXT("CH03Roof"), ESearchCase::IgnoreCase))
	{
		OutChapter = IGRebirthProbe::Tag(TEXT("Chapter.CH03"));
		OutCheckpoint = IGRebirthProbe::Tag(TEXT("Checkpoint.CH03.Roof"));
		OutLocation = ChapterThreeOrigin + FVector(1815.0f, -400.0f, 338.0f);
		OutRotation = FRotator::ZeroRotator;
		return true;
	}
	return false;
}

void AIGRebirthPersistenceProbe::StartAnchorWrite()
{
	FGameplayTag Chapter;
	FGameplayTag Checkpoint;
	FVector ExpectedLocation;
	FRotator ExpectedRotation;
	if (!ResolveAnchorContract(
			Chapter,
			Checkpoint,
			ExpectedLocation,
			ExpectedRotation))
	{
		ExitFailure(TEXT("unknown checkpoint anchor case"));
		return;
	}
	UIGRebirthNarrativeSubsystem* Narrative =
		GetGameInstance()
			? GetGameInstance()->GetSubsystem<UIGRebirthNarrativeSubsystem>()
			: nullptr;
	UIGStoryStateSubsystem* StoryState =
		GetGameInstance()
			? GetGameInstance()->GetSubsystem<UIGStoryStateSubsystem>()
			: nullptr;
	UIGSaveSubsystem* Save =
		GetGameInstance()
			? GetGameInstance()->GetSubsystem<UIGSaveSubsystem>()
			: nullptr;
	if (!Narrative || !StoryState || !Save || Save->IsBusy() || !GetWorld())
	{
		ExitFailure(TEXT("checkpoint anchor writer subsystem unavailable"));
		return;
	}
	if (UGameplayStatics::DoesSaveGameExist(SlotName, 0))
	{
		UGameplayStatics::DeleteGameInSlot(SlotName, 0);
	}
	Narrative->ResetNarrative();
	StoryState->ClearStates(false);
	Save->OnSaveCompleted.AddUniqueDynamic(
		this,
		&ThisClass::HandleSaveCompleted);
	const FName MapPackageName = GetWorld()->GetOutermost()->GetFName();
	if (MapPackageName.IsNone()
		|| !Save->RequestSave(
			SlotName,
			Chapter,
			MapPackageName,
			Checkpoint))
	{
		ExitFailure(TEXT("checkpoint anchor writer save request rejected"));
	}
}

void AIGRebirthPersistenceProbe::StartAnchorRead()
{
	FGameplayTag Chapter;
	FGameplayTag Checkpoint;
	FVector ExpectedLocation;
	FRotator ExpectedRotation;
	if (!ResolveAnchorContract(
			Chapter,
			Checkpoint,
			ExpectedLocation,
			ExpectedRotation))
	{
		ExitFailure(TEXT("unknown checkpoint anchor case"));
		return;
	}
	if (GetWorld() && GetWorld()->URL.HasOption(TEXT("IGResumeSave")))
	{
		AnchorValidationAttempts = 0;
		GetWorldTimerManager().SetTimer(
			AnchorValidationTimer,
			this,
			&ThisClass::ValidateLoadedAnchor,
			0.25f,
			true,
			0.25f);
		return;
	}

	UIGSaveSubsystem* Save =
		GetGameInstance()
			? GetGameInstance()->GetSubsystem<UIGSaveSubsystem>()
			: nullptr;
	if (!Save || Save->IsBusy())
	{
		ExitFailure(TEXT("checkpoint anchor reader subsystem unavailable"));
		return;
	}
	Save->OnLoadCompleted.AddUniqueDynamic(
		this,
		&ThisClass::HandleLoadCompleted);
	if (!Save->RequestLoad(SlotName))
	{
		ExitFailure(TEXT("checkpoint anchor reader load request rejected"));
	}
}

void AIGRebirthPersistenceProbe::ValidateLoadedAnchor()
{
	++AnchorValidationAttempts;
	FGameplayTag ExpectedChapter;
	FGameplayTag ExpectedCheckpoint;
	FVector ExpectedLocation;
	FRotator ExpectedRotation;
	if (!ResolveAnchorContract(
			ExpectedChapter,
			ExpectedCheckpoint,
			ExpectedLocation,
			ExpectedRotation))
	{
		ExitFailure(TEXT("checkpoint anchor contract disappeared"));
		return;
	}
	UWorld* World = GetWorld();
	const UIGSaveSubsystem* Save =
		GetGameInstance()
			? GetGameInstance()->GetSubsystem<UIGSaveSubsystem>()
			: nullptr;
	const UIGSaveGame* LoadedSave = Save ? Save->GetLastLoadedSave() : nullptr;
	APlayerController* PlayerController =
		World ? World->GetFirstPlayerController() : nullptr;
	APawn* PlayerPawn = PlayerController ? PlayerController->GetPawn() : nullptr;
	bool bChapterGraphReady = false;
	if (World)
	{
		if (ExpectedChapter.MatchesTagExact(
				IGRebirthProbe::Tag(TEXT("Chapter.CH02"))))
		{
			for (TActorIterator<AIGSecondMorningDirector> It(World); It; ++It)
			{
				bChapterGraphReady = true;
				break;
			}
		}
		else
		{
			for (TActorIterator<AIGThirdMorningDirector> It(World); It; ++It)
			{
				bChapterGraphReady = true;
				break;
			}
		}
	}
	const FVector ActualLocation =
		PlayerPawn ? PlayerPawn->GetActorLocation() : FVector::ZeroVector;
	const bool bPositionReady =
		PlayerPawn
		&& FVector::DistSquared(ActualLocation, ExpectedLocation)
			<= FMath::Square(2.0f);
	if ((!LoadedSave || !bChapterGraphReady || !bPositionReady)
		&& AnchorValidationAttempts < 40)
	{
		return;
	}
	GetWorldTimerManager().ClearTimer(AnchorValidationTimer);
	if (!World
		|| !LoadedSave
		|| !PlayerPawn
		|| !bChapterGraphReady
		|| !bPositionReady
		|| !World->URL.HasOption(TEXT("IGResumeSave"))
		|| LoadedSave->Progress.MapPackageName
			!= World->GetOutermost()->GetFName()
		|| !LoadedSave->Progress.ChapterId.MatchesTagExact(ExpectedChapter)
		|| !LoadedSave->Progress.CheckpointTag.MatchesTagExact(
			ExpectedCheckpoint)
		|| FMath::Abs(FMath::FindDeltaAngleDegrees(
			PlayerPawn->GetActorRotation().Yaw,
			ExpectedRotation.Yaw)) > 1.0f)
	{
		UE_LOG(
			LogIndieGame,
			Error,
			TEXT(
				"REBIRTH_SPIKE ANCHOR_MISMATCH case=%s attempts=%d "
				"graph=%d expected=%s actual=%s yaw=%.2f"),
			*AnchorCase,
			AnchorValidationAttempts,
			bChapterGraphReady ? 1 : 0,
			*ExpectedLocation.ToCompactString(),
			*ActualLocation.ToCompactString(),
			PlayerPawn ? PlayerPawn->GetActorRotation().Yaw : 0.0f);
		ExitFailure(TEXT("checkpoint anchor map re-entry mismatch"));
		return;
	}

	FCollisionQueryParams QueryParams(
		SCENE_QUERY_STAT(RebirthCheckpointAnchor),
		false,
		PlayerPawn);
	const FCollisionShape ClearanceCapsule =
		FCollisionShape::MakeCapsule(30.0f, 92.0f);
	const bool bCapsuleBlocked = World->OverlapBlockingTestByChannel(
		ActualLocation,
		FQuat::Identity,
		ECC_Pawn,
		ClearanceCapsule,
		QueryParams);
	FHitResult FloorHit;
	const bool bFoundFloor = World->LineTraceSingleByChannel(
		FloorHit,
		ActualLocation + FVector(0.0f, 0.0f, 8.0f),
		ActualLocation - FVector(0.0f, 0.0f, 128.0f),
		ECC_Pawn,
		QueryParams);
	const float FloorClearance =
		bFoundFloor
			? ActualLocation.Z - FloorHit.ImpactPoint.Z
			: TNumericLimits<float>::Max();
	const bool bFloorSafe =
		bFoundFloor
		&& FloorHit.bBlockingHit
		&& FloorClearance >= 90.0f
		&& FloorClearance <= 104.0f;
	if (bCapsuleBlocked || !bFloorSafe)
	{
		UE_LOG(
			LogIndieGame,
			Error,
			TEXT(
				"REBIRTH_SPIKE ANCHOR_UNSAFE case=%s blocked=%d "
				"floor=%d clearance=%.2f location=%s"),
			*AnchorCase,
			bCapsuleBlocked ? 1 : 0,
			bFoundFloor ? 1 : 0,
			FloorClearance,
			*ActualLocation.ToCompactString());
		ExitFailure(TEXT("checkpoint anchor collision or floor mismatch"));
		return;
	}
	const bool bDeleted =
		UGameplayStatics::DeleteGameInSlot(SlotName, 0)
		&& !UGameplayStatics::DoesSaveGameExist(SlotName, 0);
	if (!bDeleted)
	{
		ExitFailure(TEXT("checkpoint anchor verification did not delete slot"));
		return;
	}
	ExitSuccess(FString::Printf(
		TEXT(
			"s7_anchor_resume case=%s location=%s floor_clearance=%.2f "
			"capsule_clear=1 map_reentered=1"),
		*AnchorCase,
		*ActualLocation.ToCompactString(),
		FloorClearance));
}

FGameplayTagContainer AIGRebirthPersistenceProbe::MakeCH02TimeCheckpoint(
	const int32 CheckpointIndex)
{
	FGameplayTagContainer Snapshot;
	const auto Add = [&Snapshot](const TCHAR* Name)
	{
		Snapshot.AddTag(IGRebirthProbe::Tag(Name));
	};
	if (CheckpointIndex == 0)
	{
		Add(TEXT("State.CH02.P1.Hour05"));
		Add(TEXT("State.CH02.P1.Minute31"));
		Add(TEXT("State.CH02.P1.Pressure2"));
	}
	else if (CheckpointIndex == 1)
	{
		Add(TEXT("State.CH02.P2.Hour05"));
		Add(TEXT("State.CH02.P2.Minute10"));
		Add(TEXT("State.CH02.P2.Pressure3"));
		Add(TEXT("State.CH02.Loop.CalledEmployee"));
		Add(TEXT("State.CH02.P2.SourceApprovalRead"));
	}
	return Snapshot;
}

bool AIGRebirthPersistenceProbe::MatchesCH02TimeCheckpoint(
	const int32 CheckpointIndex,
	const FGameplayTagContainer& Actual,
	const FIGRebirthNarrativeSnapshot& Narrative)
{
	const FGameplayTagContainer Expected =
		MakeCH02TimeCheckpoint(CheckpointIndex);
	return Actual.Num() == Expected.Num()
		&& Actual.HasAllExact(Expected)
		&& !Narrative.ResolvedPuzzles.Contains(FName(TEXT("P1")))
		&& !Narrative.ResolvedPuzzles.Contains(FName(TEXT("P2")))
		&& !Narrative.ResolvedPuzzles.Contains(
			FName(TEXT("P1.AlarmArithmeticProxy")))
		&& !Narrative.ResolvedPuzzles.Contains(
			FName(TEXT("P2.ReceiptComparisonProxy")));
}

void AIGRebirthPersistenceProbe::StartCH02TimeWrite()
{
	if (CH02TimeCheckpointIndex < 0 || CH02TimeCheckpointIndex > 1)
	{
		ExitFailure(TEXT("CH02 time checkpoint must be 0..1"));
		return;
	}
	UIGRebirthNarrativeSubsystem* Narrative =
		GetGameInstance()
			? GetGameInstance()->GetSubsystem<UIGRebirthNarrativeSubsystem>()
			: nullptr;
	UIGStoryStateSubsystem* StoryState =
		GetGameInstance()
			? GetGameInstance()->GetSubsystem<UIGStoryStateSubsystem>()
			: nullptr;
	UIGSaveSubsystem* Save =
		GetGameInstance()
			? GetGameInstance()->GetSubsystem<UIGSaveSubsystem>()
			: nullptr;
	if (!Narrative || !StoryState || !Save || Save->IsBusy())
	{
		ExitFailure(TEXT("CH02 time writer subsystem unavailable"));
		return;
	}
	if (UGameplayStatics::DoesSaveGameExist(SlotName, 0))
	{
		UGameplayStatics::DeleteGameInSlot(SlotName, 0);
	}

	Narrative->ResetNarrative();
	StoryState->RestoreStateSnapshot(
		MakeCH02TimeCheckpoint(CH02TimeCheckpointIndex),
		false);
	Save->OnSaveCompleted.AddUniqueDynamic(
		this,
		&ThisClass::HandleSaveCompleted);
	if (!Save->RequestSave(
			SlotName,
			IGRebirthProbe::Tag(TEXT("Chapter.CH02")),
			NAME_None,
			IGRebirthProbe::Tag(
				CH02TimeCheckpointIndex == 0
					? TEXT("Checkpoint.CH02.Corridor")
					: TEXT("Checkpoint.CH02.Store"))))
	{
		ExitFailure(TEXT("CH02 time writer save request rejected"));
	}
}

void AIGRebirthPersistenceProbe::StartCH02TimeRead()
{
	if (CH02TimeCheckpointIndex < 0 || CH02TimeCheckpointIndex > 1)
	{
		ExitFailure(TEXT("CH02 time checkpoint must be 0..1"));
		return;
	}
	UIGSaveSubsystem* Save =
		GetGameInstance()
			? GetGameInstance()->GetSubsystem<UIGSaveSubsystem>()
			: nullptr;
	if (!Save || Save->IsBusy())
	{
		ExitFailure(TEXT("CH02 time reader subsystem unavailable"));
		return;
	}
	Save->OnLoadCompleted.AddUniqueDynamic(
		this,
		&ThisClass::HandleLoadCompleted);
	if (!Save->RequestLoad(SlotName))
	{
		ExitFailure(TEXT("CH02 time reader load request rejected"));
	}
}

FIGRebirthChapterThreeState AIGRebirthPersistenceProbe::MakeP5Checkpoint(
	const int32 CheckpointIndex)
{
	FIGRebirthChapterThreeState State;
	State.bTankOpened = true;
	if (CheckpointIndex == 0)
	{
		State.FocusedEvidence = EIGRebirthEvidenceId::HosePaw;
		State.ObservedP5Sources = {
			FName(TEXT("P5.CatEnteredPrints")),
			FName(TEXT("P5.HosePawCompression"))};
	}
	else if (CheckpointIndex == 1)
	{
		State.FocusedEvidence = EIGRebirthEvidenceId::CatExited;
		State.ObservedP5Sources = {
			FName(TEXT("P5.CatEnteredPrints")),
			FName(TEXT("P5.CatExitedPrints"))};
		State.bP5CatSafeConfirmed = true;
		State.AccidentScratchCount = 1;
		State.bLookedAwayAfterFirstScratch = true;
	}
	else if (CheckpointIndex == 2)
	{
		State.FocusedEvidence = EIGRebirthEvidenceId::HandSmear;
		State.ObservedP5Sources = {
			FName(TEXT("P5.CatEnteredPrints")),
			FName(TEXT("P5.CatExitedPrints")),
			FName(TEXT("P5.HosePawCompression")),
			FName(TEXT("P5.CouplingImpact")),
			FName(TEXT("P5.InnerRimFriction")),
			FName(TEXT("P5.UpperSlipperEnd")),
			FName(TEXT("P5.InwardHandSmear"))};
		State.bP5CatSafeConfirmed = true;
		State.bP5HoseCauseConfirmed = true;
		State.AccidentScratchCount = 2;
		State.bLookedAwayAfterFirstScratch = true;
		State.bActedAfterSecondScratch = true;
	}
	else if (CheckpointIndex == 3)
	{
		State.FocusedEvidence = EIGRebirthEvidenceId::TankClothing;
		State.ObservedP5Sources = {
			FName(TEXT("P5.CatEnteredPrints")),
			FName(TEXT("P5.CatExitedPrints")),
			FName(TEXT("P5.HosePawCompression")),
			FName(TEXT("P5.CouplingImpact")),
			FName(TEXT("P5.InnerRimFriction")),
			FName(TEXT("P5.UpperSlipperEnd")),
			FName(TEXT("P5.LiftedPad")),
			FName(TEXT("P5.CorrodedClips")),
			FName(TEXT("P5.InwardHandSmear")),
			FName(TEXT("P5.CurrentSleeveThreeStitches")),
			FName(TEXT("P5.TankSleeveThreeStitches")),
			FName(TEXT("P5.TankHeelWear"))};
		State.bP5CatSafeConfirmed = true;
		State.bP5HoseCauseConfirmed = true;
		State.bP5FallConfirmed = true;
		State.bP5IdentityConfirmed = true;
		State.AccidentScratchCount = 3;
		State.bAccidentScratchTailSettled = true;
		State.bLookedAwayAfterFirstScratch = true;
		State.bActedAfterSecondScratch = true;
	}
	return State;
}

bool AIGRebirthPersistenceProbe::MatchesP5Checkpoint(
	const int32 CheckpointIndex,
	const FIGRebirthNarrativeSnapshot& Actual)
{
	const FIGRebirthChapterThreeState Expected =
		MakeP5Checkpoint(CheckpointIndex);
	const FIGRebirthChapterThreeState& State = Actual.ChapterThree;
	const bool bExpectedResolved = CheckpointIndex == 3;
	return State.bTankOpened == Expected.bTankOpened
		&& State.FocusedEvidence == Expected.FocusedEvidence
		&& State.ObservedP5Sources == Expected.ObservedP5Sources
		&& State.bP5CatSafeConfirmed == Expected.bP5CatSafeConfirmed
		&& State.bP5HoseCauseConfirmed == Expected.bP5HoseCauseConfirmed
		&& State.bP5FallConfirmed == Expected.bP5FallConfirmed
		&& State.bP5IdentityConfirmed == Expected.bP5IdentityConfirmed
		&& State.AccidentScratchCount == Expected.AccidentScratchCount
		&& State.bAccidentScratchTailSettled
			== Expected.bAccidentScratchTailSettled
		&& State.bLookedAwayAfterFirstScratch
			== Expected.bLookedAwayAfterFirstScratch
		&& State.bActedAfterSecondScratch
			== Expected.bActedAfterSecondScratch
		&& Actual.ResolvedPuzzles.Contains(FName(TEXT("P5")))
			== bExpectedResolved;
}

void AIGRebirthPersistenceProbe::StartP5Write()
{
	if (P5CheckpointIndex < 0 || P5CheckpointIndex > 3)
	{
		ExitFailure(TEXT("P5 checkpoint must be 0..3"));
		return;
	}
	UIGRebirthNarrativeSubsystem* State =
		GetGameInstance()
			? GetGameInstance()->GetSubsystem<UIGRebirthNarrativeSubsystem>()
			: nullptr;
	UIGSaveSubsystem* Save =
		GetGameInstance()
			? GetGameInstance()->GetSubsystem<UIGSaveSubsystem>()
			: nullptr;
	if (!State || !Save || Save->IsBusy())
	{
		ExitFailure(TEXT("P5 writer subsystem unavailable"));
		return;
	}
	if (UGameplayStatics::DoesSaveGameExist(SlotName, 0))
	{
		UGameplayStatics::DeleteGameInSlot(SlotName, 0);
	}

	State->ResetNarrative();
	if (P5CheckpointIndex >= 1)
	{
		IGRebirthProbe::AddSources(
			State,
			TEXT("Truth.CatSafe"),
			{FName(TEXT("P5.CatEnteredPrints")),
				FName(TEXT("P5.CatExitedPrints"))},
			false);
	}
	if (P5CheckpointIndex >= 2)
	{
		IGRebirthProbe::AddSources(
			State,
			TEXT("Truth.HoseCause"),
			{FName(TEXT("P5.HosePawCompression")),
				FName(TEXT("P5.CouplingImpact")),
				FName(TEXT("P5.InnerRimFriction"))},
			false);
	}
	if (P5CheckpointIndex == 3)
	{
		IGRebirthProbe::AddSources(
			State,
			TEXT("Truth.Fall"),
			{FName(TEXT("P5.UpperSlipperEnd")),
				FName(TEXT("P5.LiftedPad")),
				FName(TEXT("P5.CorrodedClips")),
				FName(TEXT("P5.InwardHandSmear"))},
			false);
		IGRebirthProbe::AddSources(
			State,
			TEXT("Truth.Identity"),
			{FName(TEXT("P5.CurrentSleeveThreeStitches")),
				FName(TEXT("P5.TankSleeveThreeStitches")),
				FName(TEXT("P5.TankHeelWear"))},
			false);
	}
	State->SetChapterThreeState(MakeP5Checkpoint(P5CheckpointIndex));
	Save->OnSaveCompleted.AddUniqueDynamic(
		this,
		&ThisClass::HandleSaveCompleted);
	if (!Save->RequestSave(
			SlotName,
			IGRebirthProbe::Tag(TEXT("Chapter.CH03")),
			NAME_None,
			IGRebirthProbe::Tag(TEXT("Checkpoint.CH03.Roof"))))
	{
		ExitFailure(TEXT("P5 writer save request rejected"));
	}
}

void AIGRebirthPersistenceProbe::StartP5Read()
{
	if (P5CheckpointIndex < 0 || P5CheckpointIndex > 3)
	{
		ExitFailure(TEXT("P5 checkpoint must be 0..3"));
		return;
	}
	UIGSaveSubsystem* Save =
		GetGameInstance()
			? GetGameInstance()->GetSubsystem<UIGSaveSubsystem>()
			: nullptr;
	if (!Save || Save->IsBusy())
	{
		ExitFailure(TEXT("P5 reader subsystem unavailable"));
		return;
	}
	Save->OnLoadCompleted.AddUniqueDynamic(
		this,
		&ThisClass::HandleLoadCompleted);
	if (!Save->RequestLoad(SlotName))
	{
		ExitFailure(TEXT("P5 reader load request rejected"));
	}
}

FIGRebirthP3State AIGRebirthPersistenceProbe::MakeP3Checkpoint(
	const int32 CheckpointIndex)
{
	FIGRebirthP3State State;
	State.MistakeCount = CheckpointIndex % 3;
	State.HintElapsedSeconds = 17.0f * CheckpointIndex;
	State.PressureRiseElapsedSeconds = 9.0f * CheckpointIndex;
	State.bPressureRiseArmed = CheckpointIndex > 0 && CheckpointIndex < 6;
	State.HintStage = FMath::Min(CheckpointIndex / 2, 3);
	switch (CheckpointIndex)
	{
	case 0:
		break;
	case 1:
		State.bDirectInletClosed = true;
		State.bReserveInletClosed = true;
		break;
	case 2:
		State.bDirectInletClosed = true;
		State.bReserveInletClosed = true;
		State.bPressureReleaseOpen = true;
		break;
	case 3:
		State.bDirectInletClosed = true;
		State.bReserveInletClosed = true;
		State.bPressureReleaseOpen = true;
		State.PressureKPa = 30.0f;
		break;
	case 4:
		State.bDirectInletClosed = true;
		State.bReserveInletClosed = true;
		State.bPressureReleaseOpen = true;
		State.PressureKPa = 0.0f;
		State.ZeroConfirmationTicks = 1;
		break;
	case 5:
		State.bDirectInletClosed = true;
		State.bReserveInletClosed = true;
		State.bPressureReleaseOpen = true;
		State.bPressureZero = true;
		State.PressureKPa = 0.0f;
		State.ZeroConfirmationTicks = 2;
		break;
	case 6:
		State.bDirectInletClosed = true;
		State.bReserveInletClosed = true;
		State.bPressureReleaseOpen = true;
		State.bPressureZero = true;
		State.bCompleted = true;
		State.PressureKPa = 0.0f;
		State.PressureRiseElapsedSeconds = 0.0f;
		State.ZeroConfirmationTicks = 2;
		break;
	default:
		break;
	}
	return State;
}

bool AIGRebirthPersistenceProbe::MatchesP3Checkpoint(
	const FIGRebirthP3State& Actual,
	const FIGRebirthP3State& Expected)
{
	return Actual.bDirectInletClosed == Expected.bDirectInletClosed
		&& Actual.bReserveInletClosed == Expected.bReserveInletClosed
		&& Actual.bPressureReleaseOpen == Expected.bPressureReleaseOpen
		&& Actual.bPressureZero == Expected.bPressureZero
		&& Actual.bCompleted == Expected.bCompleted
		&& FMath::IsNearlyEqual(Actual.PressureKPa, Expected.PressureKPa)
		&& Actual.MistakeCount == Expected.MistakeCount
		&& Actual.ZeroConfirmationTicks == Expected.ZeroConfirmationTicks
		&& FMath::IsNearlyEqual(
			Actual.HintElapsedSeconds,
			Expected.HintElapsedSeconds)
		&& FMath::IsNearlyEqual(
			Actual.PressureRiseElapsedSeconds,
			Expected.PressureRiseElapsedSeconds)
		&& Actual.bPressureRiseArmed == Expected.bPressureRiseArmed
		&& Actual.HintStage == Expected.HintStage;
}

FIGRebirthP4State AIGRebirthPersistenceProbe::MakeP4Checkpoint(
	const int32 CheckpointIndex)
{
	FIGRebirthP4State State;
	State.StairLoopCount = FMath::Min(CheckpointIndex / 2, 3);
	State.PressureStage = CheckpointIndex % 4;
	State.PressureRiseElapsedSeconds = 11.0f * CheckpointIndex;
	State.HintElapsedSeconds = 13.0f * CheckpointIndex;
	State.HintStage = FMath::Min(CheckpointIndex / 2, 3);
	State.bPressureArmed = CheckpointIndex > 0 && CheckpointIndex < 6;
	State.bCompleted = CheckpointIndex == 6;
	if (State.bCompleted)
	{
		State.bPressureArmed = false;
		State.PressureRiseElapsedSeconds = 0.0f;
	}
	return State;
}

bool AIGRebirthPersistenceProbe::MatchesP4Checkpoint(
	const FIGRebirthP4State& Actual,
	const FIGRebirthP4State& Expected)
{
	return Actual.StairLoopCount == Expected.StairLoopCount
		&& Actual.PressureStage == Expected.PressureStage
		&& FMath::IsNearlyEqual(
			Actual.PressureRiseElapsedSeconds,
			Expected.PressureRiseElapsedSeconds)
		&& FMath::IsNearlyEqual(
			Actual.HintElapsedSeconds,
			Expected.HintElapsedSeconds)
		&& Actual.HintStage == Expected.HintStage
		&& Actual.bPressureArmed == Expected.bPressureArmed
		&& Actual.bCompleted == Expected.bCompleted;
}

void AIGRebirthPersistenceProbe::StartP3Write()
{
	if (P3CheckpointIndex < 0 || P3CheckpointIndex > 6)
	{
		ExitFailure(TEXT("P3 checkpoint must be 0..6"));
		return;
	}
	UIGRebirthNarrativeSubsystem* State =
		GetGameInstance()
			? GetGameInstance()->GetSubsystem<UIGRebirthNarrativeSubsystem>()
			: nullptr;
	UIGSaveSubsystem* Save =
		GetGameInstance()
			? GetGameInstance()->GetSubsystem<UIGSaveSubsystem>()
			: nullptr;
	if (!State || !Save || Save->IsBusy())
	{
		ExitFailure(TEXT("P3 writer subsystem unavailable"));
		return;
	}
	if (UGameplayStatics::DoesSaveGameExist(SlotName, 0))
	{
		UGameplayStatics::DeleteGameInSlot(SlotName, 0);
	}

	State->ResetNarrative();
	FIGRebirthChapterThreeState ChapterThree;
	ChapterThree.P3 = MakeP3Checkpoint(P3CheckpointIndex);
	ChapterThree.P4 = MakeP4Checkpoint(P3CheckpointIndex);
	State->SetChapterThreeState(ChapterThree);
	Save->OnSaveCompleted.AddUniqueDynamic(
		this,
		&ThisClass::HandleSaveCompleted);
	if (!Save->RequestSave(
			SlotName,
			IGRebirthProbe::Tag(TEXT("Chapter.CH03")),
			NAME_None,
			IGRebirthProbe::Tag(TEXT("Checkpoint.CH03.Roof"))))
	{
		ExitFailure(TEXT("P3 writer save request rejected"));
	}
}

void AIGRebirthPersistenceProbe::StartP3Read()
{
	if (P3CheckpointIndex < 0 || P3CheckpointIndex > 6)
	{
		ExitFailure(TEXT("P3 checkpoint must be 0..6"));
		return;
	}
	UIGSaveSubsystem* Save =
		GetGameInstance()
			? GetGameInstance()->GetSubsystem<UIGSaveSubsystem>()
			: nullptr;
	if (!Save || Save->IsBusy())
	{
		ExitFailure(TEXT("P3 reader subsystem unavailable"));
		return;
	}
	Save->OnLoadCompleted.AddUniqueDynamic(
		this,
		&ThisClass::HandleLoadCompleted);
	if (!Save->RequestLoad(SlotName))
	{
		ExitFailure(TEXT("P3 reader load request rejected"));
	}
}

void AIGRebirthPersistenceProbe::PrepareEndingPrerequisites()
{
	UIGRebirthNarrativeSubsystem* State =
		GetGameInstance()->GetSubsystem<UIGRebirthNarrativeSubsystem>();
	State->ResetNarrative();
	IGRebirthProbe::AddSources(
		State,
		TEXT("Truth.Alarm0510"),
		{FName(TEXT("CH02.MirrorAlarmMemo"))});
	IGRebirthProbe::AddSources(
		State,
		TEXT("Truth.DeathOverlay"),
		{FName(TEXT("CH02.DuplicateReceipt"))});
	IGRebirthProbe::AddSources(
		State,
		TEXT("Truth.WasSearched"),
		{FName(TEXT("CH02.ManagementComplaint"))});
	IGRebirthProbe::AddSources(
		State,
		TEXT("Truth.RecheckScheduled0731"),
		{FName(TEXT("Vendor.Revisit0731"))});
	IGRebirthProbe::AddSources(
		State,
		TEXT("Truth.Negligence"),
		{
			FName(TEXT("Handover.Closed0358")),
			FName(TEXT("ManagementApp.Photo0403")),
			FName(TEXT("ManagementDb.FalseCompletion0620"))
		},
		false);

	const TArray<FName> CatSources = {
		FName(TEXT("P5.CatEnteredPrints")),
		FName(TEXT("P5.CatExitedPrints"))};
	const TArray<FName> HoseSources = {
		FName(TEXT("P5.HosePawCompression")),
		FName(TEXT("P5.CouplingImpact")),
		FName(TEXT("P5.InnerRimFriction"))};
	const TArray<FName> FallSources = {
		FName(TEXT("P5.UpperSlipperEnd")),
		FName(TEXT("P5.LiftedPad")),
		FName(TEXT("P5.CorrodedClips")),
		FName(TEXT("P5.InwardHandSmear"))};
	const TArray<FName> IdentitySources = {
		FName(TEXT("P5.CurrentSleeveThreeStitches")),
		FName(TEXT("P5.TankSleeveThreeStitches")),
		FName(TEXT("P5.TankHeelWear"))};
	IGRebirthProbe::AddSources(
		State,
		TEXT("Truth.CatSafe"),
		CatSources,
		false);
	IGRebirthProbe::AddSources(
		State,
		TEXT("Truth.HoseCause"),
		HoseSources,
		false);
	IGRebirthProbe::AddSources(
		State,
		TEXT("Truth.Fall"),
		FallSources,
		false);
	IGRebirthProbe::AddSources(
		State,
		TEXT("Truth.Identity"),
		IdentitySources,
		false);

	FIGRebirthChapterThreeState ChapterThree;
	ChapterThree.bTankOpened = true;
	ChapterThree.ObservedP5Sources.Append(CatSources);
	ChapterThree.ObservedP5Sources.Append(HoseSources);
	ChapterThree.ObservedP5Sources.Append(FallSources);
	ChapterThree.ObservedP5Sources.Append(IdentitySources);
	ChapterThree.ObservedP5Sources.Append({
		FName(TEXT("P5.SearchPosterOutfit")),
		FName(TEXT("P5.Glasses")),
		FName(TEXT("P5.TankOutfit"))});
	ChapterThree.AccidentScratchCount = 3;
	ChapterThree.bAccidentScratchTailSettled = true;
	ChapterThree.bLookedAwayAfterFirstScratch = true;
	ChapterThree.bActedAfterSecondScratch = true;
	State->SetChapterThreeState(ChapterThree);
	State->MarkLocationVisited(FName(TEXT("CH03.Roof")));
}

void AIGRebirthPersistenceProbe::StartEndingWrite()
{
	UIGRebirthNarrativeSubsystem* State =
		GetGameInstance()
			? GetGameInstance()->GetSubsystem<UIGRebirthNarrativeSubsystem>()
			: nullptr;
	UIGSaveSubsystem* Save =
		GetGameInstance()
			? GetGameInstance()->GetSubsystem<UIGSaveSubsystem>()
			: nullptr;
	if (!State || !Save || Save->IsBusy())
	{
		ExitFailure(TEXT("ending writer subsystem unavailable"));
		return;
	}
	if (UGameplayStatics::DoesSaveGameExist(SlotName, 0))
	{
		UGameplayStatics::DeleteGameInSlot(SlotName, 0);
	}
	PrepareEndingPrerequisites();
	if (!State->SelectChapterThreeEnding(
			bEndingA
				? EIGRebirthEndingChoice::EndingA
				: EIGRebirthEndingChoice::EndingB))
	{
		ExitFailure(TEXT("ending selection prerequisites rejected"));
		return;
	}
	Save->OnSaveCompleted.AddUniqueDynamic(
		this,
		&ThisClass::HandleSaveCompleted);
	if (!Save->RequestSave(
			SlotName,
			IGRebirthProbe::Tag(TEXT("Chapter.CH03")),
			NAME_None,
			IGRebirthProbe::Tag(TEXT("Checkpoint.CH03.Roof"))))
	{
		ExitFailure(TEXT("ending writer save request rejected"));
	}
}

void AIGRebirthPersistenceProbe::StartEndingRead()
{
	UIGSaveSubsystem* Save =
		GetGameInstance()
			? GetGameInstance()->GetSubsystem<UIGSaveSubsystem>()
			: nullptr;
	if (!Save || Save->IsBusy())
	{
		ExitFailure(TEXT("ending reader subsystem unavailable"));
		return;
	}
	Save->OnLoadCompleted.AddUniqueDynamic(
		this,
		&ThisClass::HandleLoadCompleted);
	if (!Save->RequestLoad(SlotName))
	{
		ExitFailure(TEXT("ending reader load request rejected"));
	}
}

void AIGRebirthPersistenceProbe::HandleSaveCompleted(
	const bool bSuccess,
	const FString CompletedSlot)
{
	if (CompletedSlot != SlotName)
	{
		return;
	}
	if (UIGSaveSubsystem* Save =
			GetGameInstance()->GetSubsystem<UIGSaveSubsystem>())
	{
		Save->OnSaveCompleted.RemoveDynamic(
			this,
			&ThisClass::HandleSaveCompleted);
	}
	if (!bSuccess
		|| !UGameplayStatics::DoesSaveGameExist(SlotName, 0))
	{
		ExitFailure(TEXT("probe disk write failed"));
		return;
	}
	if (ProbeMode.Equals(TEXT("P3Write"), ESearchCase::IgnoreCase))
	{
		ExitSuccess(FString::Printf(
			TEXT("s3_p3_write checkpoint=%d"),
			P3CheckpointIndex));
		return;
	}
	if (ProbeMode.Equals(TEXT("BoundaryBeforeWrite"), ESearchCase::IgnoreCase)
		|| ProbeMode.Equals(TEXT("BoundaryAfterWrite"), ESearchCase::IgnoreCase))
	{
		ExitSuccess(FString::Printf(
			TEXT("s5_boundary_write phase=%s immutable=1"),
			ProbeMode.Contains(TEXT("After"), ESearchCase::IgnoreCase)
				? TEXT("after")
				: TEXT("before")));
		return;
	}
	if (ProbeMode.Equals(TEXT("CatChoiceWrite"), ESearchCase::IgnoreCase))
	{
		ExitSuccess(FString::Printf(
			TEXT("s5_cat_write case=%s"),
			*CatChoiceCase));
		return;
	}
	if (ProbeMode.Equals(TEXT("AnchorWrite"), ESearchCase::IgnoreCase))
	{
		ExitSuccess(FString::Printf(
			TEXT("s7_anchor_write case=%s map_saved=1"),
			*AnchorCase));
		return;
	}
	if (ProbeMode.Equals(TEXT("CH02TimeWrite"), ESearchCase::IgnoreCase))
	{
		ExitSuccess(FString::Printf(
			TEXT("s6_ch02_time_write checkpoint=%d"),
			CH02TimeCheckpointIndex));
		return;
	}
	if (ProbeMode.Equals(TEXT("P5Write"), ESearchCase::IgnoreCase))
	{
		ExitSuccess(FString::Printf(
			TEXT("s3_p5_write checkpoint=%d"),
			P5CheckpointIndex));
		return;
	}
	if (ProbeMode.Equals(TEXT("EndingWrite"), ESearchCase::IgnoreCase))
	{
		ExitSuccess(FString::Printf(
			TEXT("s4_ending_write ending=%s"),
			bEndingA ? TEXT("A") : TEXT("B")));
		return;
	}
	if (bSaveAfterEndingCommit)
	{
		ExitSuccess(FString::Printf(
			TEXT("s4_common_commit ending=%s once=1"),
			bEndingA ? TEXT("A") : TEXT("B")));
		return;
	}
	ExitFailure(TEXT("unexpected save completion"));
}

void AIGRebirthPersistenceProbe::HandleLoadCompleted(
	const bool bSuccess,
	const FString CompletedSlot,
	UIGSaveGame* SaveGame)
{
	if (CompletedSlot != SlotName)
	{
		return;
	}
	if (UIGSaveSubsystem* Save =
			GetGameInstance()->GetSubsystem<UIGSaveSubsystem>())
	{
		Save->OnLoadCompleted.RemoveDynamic(
			this,
			&ThisClass::HandleLoadCompleted);
	}
	UIGRebirthNarrativeSubsystem* State =
		GetGameInstance()
			? GetGameInstance()->GetSubsystem<UIGRebirthNarrativeSubsystem>()
			: nullptr;
	UIGStoryStateSubsystem* StoryState =
		GetGameInstance()
			? GetGameInstance()->GetSubsystem<UIGStoryStateSubsystem>()
			: nullptr;
	if (!bSuccess
		|| !SaveGame
		|| SaveGame->Progress.SchemaVersion
			!= UIGSaveGame::CurrentSchemaVersion
		|| !State)
	{
		ExitFailure(TEXT("probe load failed or schema mismatched"));
		return;
	}
	if (ProbeMode.Equals(TEXT("AnchorRead"), ESearchCase::IgnoreCase))
	{
		FGameplayTag ExpectedChapter;
		FGameplayTag ExpectedCheckpoint;
		FVector ExpectedLocation;
		FRotator ExpectedRotation;
		const bool bContractValid = ResolveAnchorContract(
			ExpectedChapter,
			ExpectedCheckpoint,
			ExpectedLocation,
			ExpectedRotation);
		if (!bContractValid
			|| SaveGame->Progress.MapPackageName.IsNone()
			|| !SaveGame->Progress.ChapterId.MatchesTagExact(ExpectedChapter)
			|| !SaveGame->Progress.CheckpointTag.MatchesTagExact(
				ExpectedCheckpoint))
		{
			ExitFailure(TEXT("checkpoint anchor save contract mismatched"));
		}
		// The save subsystem opens the authored map after this broadcast. The
		// replacement probe validates the rebuilt chapter and physical anchor.
		return;
	}
	if (ProbeMode.Equals(TEXT("BoundaryBeforeRead"), ESearchCase::IgnoreCase)
		|| ProbeMode.Equals(TEXT("BoundaryAfterRead"), ESearchCase::IgnoreCase))
	{
		const bool bAfterBoundary = ProbeMode.Contains(
			TEXT("After"),
			ESearchCase::IgnoreCase);
		const FGameplayTagContainer StorySnapshot = StoryState
			? StoryState->GetStateSnapshot()
			: FGameplayTagContainer();
		const FIGRebirthNarrativeSnapshot Narrative = State->BuildSnapshot();
		int32 BagBeatCount = 0;
		for (const FName Beat : Narrative.PlayedOneShotBeats)
		{
			BagBeatCount += Beat == FName(TEXT("CH01.BagPlacedAtLadder"))
				? 1
				: 0;
		}
		const bool bBeforeMatched =
			!bAfterBoundary
			&& StoryState
			&& StorySnapshot.Num() == 1
			&& StorySnapshot.HasTagExact(IGRebirthProbe::Tag(
				TEXT("State.CH01.Incident.ReachedFourthFloor")))
			&& BagBeatCount == 0
			&& SaveGame->Progress.ChapterId.MatchesTagExact(
				IGRebirthProbe::Tag(TEXT("Chapter.CH01")))
			&& SaveGame->Progress.CheckpointTag.MatchesTagExact(
				IGRebirthProbe::Tag(TEXT("Checkpoint.CH01.FourthFloor")));
		const bool bAfterMatched =
			bAfterBoundary
			&& StoryState
			&& StorySnapshot.Num() == 3
			&& StorySnapshot.HasTagExact(IGRebirthProbe::Tag(
				TEXT("State.CH02.Loop.Started")))
			&& StorySnapshot.HasTagExact(IGRebirthProbe::Tag(
				TEXT("State.CH02.Wake.AlarmStopped")))
			&& StorySnapshot.HasTagExact(IGRebirthProbe::Tag(
				TEXT("State.CH02.Wake.Standing")))
			&& BagBeatCount == 1
			&& SaveGame->Progress.ChapterId.MatchesTagExact(
				IGRebirthProbe::Tag(TEXT("Chapter.CH02")))
			&& SaveGame->Progress.CheckpointTag.MatchesTagExact(
				IGRebirthProbe::Tag(TEXT("Checkpoint.CH02.Woke")));
		const bool bDeleted =
			UGameplayStatics::DeleteGameInSlot(SlotName, 0)
			&& !UGameplayStatics::DoesSaveGameExist(SlotName, 0);
		if ((!bBeforeMatched && !bAfterMatched) || !bDeleted)
		{
			ExitFailure(TEXT("memory-boundary atomic restore mismatch"));
			return;
		}
		ExitSuccess(FString::Printf(
			TEXT(
				"s5_boundary_resume phase=%s transient_tags=0 safe_states=%d "
				"beat_count=%d "
				"slot_deleted=1"),
			bAfterBoundary ? TEXT("after") : TEXT("before"),
			StorySnapshot.Num(),
			BagBeatCount));
		return;
	}
	if (ProbeMode.Equals(TEXT("CatChoiceRead"), ESearchCase::IgnoreCase))
	{
		FIGRebirthChoiceState ExpectedChoices;
		const FIGRebirthNarrativeSnapshot Narrative = State->BuildSnapshot();
		const FGameplayTagContainer StorySnapshot = StoryState
			? StoryState->GetStateSnapshot()
			: FGameplayTagContainer();
		const bool bStateMatched =
			ResolveCatChoiceContract(ExpectedChoices)
			&& StoryState
			&& SaveGame->Progress.ChapterId.MatchesTagExact(
				IGRebirthProbe::Tag(TEXT("Chapter.CH01")))
			&& SaveGame->Progress.CheckpointTag.MatchesTagExact(
				IGRebirthProbe::Tag(TEXT("Checkpoint.CH01.ReturnAlley")))
			&& StorySnapshot.Num() == 3
			&& StorySnapshot.HasTagExact(IGRebirthProbe::Tag(
				TEXT("State.CH01.Incident.Drank")))
			&& StorySnapshot.HasTagExact(IGRebirthProbe::Tag(
				TEXT("State.CH01.Incident.CatChoiceCommitted")))
			&& StorySnapshot.HasTagExact(IGRebirthProbe::Tag(
				TEXT("State.CH01.Incident.ThirdGustOccurred")))
			&& Narrative.Choices.PurchaseProfile
				== ExpectedChoices.PurchaseProfile
			&& Narrative.Choices.PaymentMethod == ExpectedChoices.PaymentMethod
			&& Narrative.Choices.bHasPaperCup == ExpectedChoices.bHasPaperCup
			&& Narrative.Choices.CatWaterState == ExpectedChoices.CatWaterState
			&& Narrative.Choices.bWaitedForCat == ExpectedChoices.bWaitedForCat
			&& Narrative.Choices.BottleClosureState
				== ExpectedChoices.BottleClosureState;

		UStaticMesh* CubeMesh = LoadObject<UStaticMesh>(
			nullptr,
			TEXT("/Engine/BasicShapes/Cube.Cube"));
		UStaticMesh* CylinderMesh = LoadObject<UStaticMesh>(
			nullptr,
			TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
		UMaterialInterface* BasicMaterial = LoadObject<UMaterialInterface>(
			nullptr,
			TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
		FActorSpawnParameters SpawnParameters;
		SpawnParameters.Name = TEXT("CatChoiceRestoreDirector");
		SpawnParameters.SpawnCollisionHandlingOverride =
			ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		AIGChapterOneIncidentDirector* Director =
			CubeMesh && CylinderMesh && BasicMaterial && GetWorld()
				? GetWorld()->SpawnActor<AIGChapterOneIncidentDirector>(
					AIGChapterOneIncidentDirector::StaticClass(),
					FTransform(
						FRotator::ZeroRotator,
						FVector(50000.0f, 50000.0f, 50000.0f)),
					SpawnParameters)
				: nullptr;
		if (Director)
		{
			Director->Configure(
				nullptr,
				nullptr,
				CubeMesh,
				CylinderMesh,
				BasicMaterial,
				BasicMaterial,
				BasicMaterial,
				FTransform(
					FRotator::ZeroRotator,
					FVector(50000.0f, 50000.0f, 50000.0f)));
		}
		const bool bChapterOnePhysicalMatched =
			Director && Director->ValidateCatWaterAftermath();
		AIGPrologueWorldScene* WorldScene = nullptr;
		if (GetWorld())
		{
			for (TActorIterator<AIGPrologueWorldScene> It(GetWorld()); It; ++It)
			{
				WorldScene = *It;
				break;
			}
		}
		if (WorldScene)
		{
			WorldScene->RefreshChapterTwoCatWaterAftermath();
		}
		const bool bChapterTwoPhysicalMatched =
			WorldScene && WorldScene->ValidateChapterTwoCatWaterAftermath();
		const bool bDeleted =
			UGameplayStatics::DeleteGameInSlot(SlotName, 0)
			&& !UGameplayStatics::DoesSaveGameExist(SlotName, 0);
		if (!bStateMatched
			|| !bChapterOnePhysicalMatched
			|| !bChapterTwoPhysicalMatched
			|| !bDeleted)
		{
			ExitFailure(TEXT("cat choice process-boundary restore mismatch"));
			return;
		}
		ExitSuccess(FString::Printf(
			TEXT(
				"s5_cat_resume case=%s ch01_physical=1 ch02_physical=1 "
				"slot_deleted=1"),
			*CatChoiceCase));
		return;
	}

	if (ProbeMode.Equals(TEXT("CH02TimeRead"), ESearchCase::IgnoreCase))
	{
		const FGameplayTag ExpectedCheckpoint = IGRebirthProbe::Tag(
			CH02TimeCheckpointIndex == 0
				? TEXT("Checkpoint.CH02.Corridor")
				: TEXT("Checkpoint.CH02.Store"));
		const bool bMatched = StoryState
			&& SaveGame->Progress.ChapterId.MatchesTagExact(
				IGRebirthProbe::Tag(TEXT("Chapter.CH02")))
			&& SaveGame->Progress.CheckpointTag.MatchesTagExact(
				ExpectedCheckpoint)
			&& MatchesCH02TimeCheckpoint(
				CH02TimeCheckpointIndex,
				StoryState->GetStateSnapshot(),
				State->BuildSnapshot());
		const bool bDeleted =
			UGameplayStatics::DeleteGameInSlot(SlotName, 0)
			&& !UGameplayStatics::DoesSaveGameExist(SlotName, 0);
		if (!bMatched || !bDeleted)
		{
			ExitFailure(TEXT("CH02 time process-boundary restore mismatch"));
			return;
		}
		ExitSuccess(FString::Printf(
			TEXT("s6_ch02_time_resume checkpoint=%d exact=1"),
			CH02TimeCheckpointIndex));
		return;
	}

	if (ProbeMode.Equals(TEXT("P5Read"), ESearchCase::IgnoreCase))
	{
		const FIGRebirthNarrativeSnapshot Actual = State->BuildSnapshot();
		const bool bMatched =
			SaveGame->Progress.ChapterId.MatchesTagExact(
				IGRebirthProbe::Tag(TEXT("Chapter.CH03")))
			&& SaveGame->Progress.CheckpointTag.MatchesTagExact(
				IGRebirthProbe::Tag(TEXT("Checkpoint.CH03.Roof")))
			&& MatchesP5Checkpoint(P5CheckpointIndex, Actual);
		const bool bDeleted =
			UGameplayStatics::DeleteGameInSlot(SlotName, 0)
			&& !UGameplayStatics::DoesSaveGameExist(SlotName, 0);
		if (!bMatched || !bDeleted)
		{
			const FIGRebirthChapterThreeState& P5 = Actual.ChapterThree;
			UE_LOG(
				LogIndieGame,
				Error,
				TEXT(
					"REBIRTH_SPIKE P5_MISMATCH checkpoint=%d observed=%d "
					"focus=%d truths=%d%d%d%d scratches=%d tail=%d "
					"looked=%d acted=%d resolved=%d deleted=%d"),
				P5CheckpointIndex,
				P5.ObservedP5Sources.Num(),
				static_cast<int32>(P5.FocusedEvidence),
				P5.bP5CatSafeConfirmed ? 1 : 0,
				P5.bP5HoseCauseConfirmed ? 1 : 0,
				P5.bP5FallConfirmed ? 1 : 0,
				P5.bP5IdentityConfirmed ? 1 : 0,
				P5.AccidentScratchCount,
				P5.bAccidentScratchTailSettled ? 1 : 0,
				P5.bLookedAwayAfterFirstScratch ? 1 : 0,
				P5.bActedAfterSecondScratch ? 1 : 0,
				Actual.ResolvedPuzzles.Contains(FName(TEXT("P5"))) ? 1 : 0,
				bDeleted ? 1 : 0);
			ExitFailure(TEXT("P5 process-boundary restore mismatch"));
			return;
		}
		ExitSuccess(FString::Printf(
			TEXT("s3_p5_resume checkpoint=%d exact=1"),
			P5CheckpointIndex));
		return;
	}

	if (ProbeMode.Equals(TEXT("P3Read"), ESearchCase::IgnoreCase))
	{
		const FIGRebirthP3State Expected =
			MakeP3Checkpoint(P3CheckpointIndex);
		const FIGRebirthP3State Actual =
			State->GetChapterThreeState().P3;
		const FIGRebirthP4State ExpectedP4 =
			MakeP4Checkpoint(P3CheckpointIndex);
		const FIGRebirthP4State ActualP4 =
			State->GetChapterThreeState().P4;
		const bool bMatched = MatchesP3Checkpoint(Actual, Expected)
			&& MatchesP4Checkpoint(ActualP4, ExpectedP4);
		const bool bDeleted =
			UGameplayStatics::DeleteGameInSlot(SlotName, 0)
			&& !UGameplayStatics::DoesSaveGameExist(SlotName, 0);
		if (!bMatched || !bDeleted)
		{
			UE_LOG(
				LogIndieGame,
				Error,
				TEXT(
					"REBIRTH_SPIKE P3_MISMATCH checkpoint=%d "
					"p3_flags=%d%d%d%d%d pressure=%.2f mistakes=%d "
					"zero_ticks=%d hint=%.2f rise=%.2f armed=%d stage=%d "
					"p4_loop=%d pressure_stage=%d rise=%.2f hint=%.2f "
					"hint_stage=%d armed=%d completed=%d deleted=%d"),
				P3CheckpointIndex,
				Actual.bDirectInletClosed ? 1 : 0,
				Actual.bReserveInletClosed ? 1 : 0,
				Actual.bPressureReleaseOpen ? 1 : 0,
				Actual.bPressureZero ? 1 : 0,
				Actual.bCompleted ? 1 : 0,
				Actual.PressureKPa,
				Actual.MistakeCount,
				Actual.ZeroConfirmationTicks,
				Actual.HintElapsedSeconds,
				Actual.PressureRiseElapsedSeconds,
				Actual.bPressureRiseArmed ? 1 : 0,
				Actual.HintStage,
				ActualP4.StairLoopCount,
				ActualP4.PressureStage,
				ActualP4.PressureRiseElapsedSeconds,
				ActualP4.HintElapsedSeconds,
				ActualP4.HintStage,
				ActualP4.bPressureArmed ? 1 : 0,
				ActualP4.bCompleted ? 1 : 0,
				bDeleted ? 1 : 0);
			ExitFailure(TEXT("P3 process-boundary restore mismatch"));
			return;
		}
		ExitSuccess(FString::Printf(
			TEXT("s3_p3_resume checkpoint=%d exact=1"),
			P3CheckpointIndex));
		return;
	}

	const EIGRebirthEndingChoice ExpectedEnding = bEndingA
		? EIGRebirthEndingChoice::EndingA
		: EIGRebirthEndingChoice::EndingB;
	const FIGRebirthNarrativeSnapshot Before = State->BuildSnapshot();
	if (Before.ChapterThree.EndingChoice != ExpectedEnding)
	{
		ExitFailure(TEXT("ending branch changed across process"));
		return;
	}
	if (ProbeMode.Equals(TEXT("EndingCommit"), ESearchCase::IgnoreCase))
	{
		if (Before.ChapterThree.bCommonDiscoveryCommitted
			|| State->HasTruth(
				IGRebirthProbe::Tag(TEXT("Truth.Found0731")))
			|| !State->CommitChapterThreeCommonDiscovery()
			|| State->CommitChapterThreeCommonDiscovery())
		{
			ExitFailure(TEXT("ending common prefix was not atomic"));
			return;
		}
		bSaveAfterEndingCommit = true;
		UIGSaveSubsystem* Save =
			GetGameInstance()->GetSubsystem<UIGSaveSubsystem>();
		Save->OnSaveCompleted.AddUniqueDynamic(
			this,
			&ThisClass::HandleSaveCompleted);
		if (!Save->RequestSave(
				SlotName,
				IGRebirthProbe::Tag(TEXT("Chapter.CH03")),
				NAME_None,
				IGRebirthProbe::Tag(TEXT("Checkpoint.CH03.Roof"))))
		{
			ExitFailure(TEXT("ending post-common save request rejected"));
		}
		return;
	}

	const FIGRebirthNarrativeSnapshot After = State->BuildSnapshot();
	int32 ActualStateRestoreCount = 0;
	for (const FName Beat : After.PlayedOneShotBeats)
	{
		ActualStateRestoreCount +=
			Beat == IGRebirthProbe::ActualStateRestored ? 1 : 0;
	}
	const bool bSelectedPuzzle =
		After.ResolvedPuzzles.Contains(
			bEndingA ? FName(TEXT("Ending.A")) : FName(TEXT("Ending.B")));
	const bool bOppositePuzzle =
		After.ResolvedPuzzles.Contains(
			bEndingA ? FName(TEXT("Ending.B")) : FName(TEXT("Ending.A")));
	const bool bPassed =
		After.ChapterThree.bCommonDiscoveryCommitted
		&& State->HasTruth(
			IGRebirthProbe::Tag(TEXT("Truth.Found0731")))
		&& ActualStateRestoreCount == 1
		&& bSelectedPuzzle
		&& !bOppositePuzzle
		&& !State->CommitChapterThreeCommonDiscovery();
	const bool bDeleted =
		UGameplayStatics::DeleteGameInSlot(SlotName, 0)
		&& !UGameplayStatics::DoesSaveGameExist(SlotName, 0);
	if (!bPassed || !bDeleted)
	{
		ExitFailure(TEXT("ending post-common restore mismatch"));
		return;
	}
	ExitSuccess(FString::Printf(
		TEXT(
			"s4_common_restore ending=%s common_once=1 "
			"branch_exclusive=1"),
		bEndingA ? TEXT("A") : TEXT("B")));
}

void AIGRebirthPersistenceProbe::ExitSuccess(const FString& Marker)
{
	UE_LOG(
		LogIndieGame,
		Display,
		TEXT("REBIRTH_SPIKE PASS %s"),
		*Marker);
	FPlatformMisc::RequestExitWithStatus(
		false,
		0,
		TEXT("REBIRTH persistence probe passed"));
}

void AIGRebirthPersistenceProbe::ExitFailure(const TCHAR* Reason)
{
	UE_LOG(
		LogIndieGame,
		Error,
		TEXT("REBIRTH_SPIKE FAIL reason=%s"),
		Reason ? Reason : TEXT("unknown"));
	FPlatformMisc::RequestExitWithStatus(
		false,
		1,
		TEXT("REBIRTH persistence probe failed"));
}
