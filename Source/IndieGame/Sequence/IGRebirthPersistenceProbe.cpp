#include "Sequence/IGRebirthPersistenceProbe.h"

#include "Engine/GameInstance.h"
#include "HAL/PlatformMisc.h"
#include "IndieGame.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Narrative/IGRebirthNarrativeSubsystem.h"
#include "Save/IGSaveGame.h"
#include "Save/IGSaveSubsystem.h"
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
		TEXT("IGRebirthP3Checkpoint="),
		P3CheckpointIndex);

	if (ProbeMode.Equals(TEXT("P3Write"), ESearchCase::IgnoreCase))
	{
		StartP3Write();
	}
	else if (ProbeMode.Equals(TEXT("P3Read"), ESearchCase::IgnoreCase))
	{
		StartP3Read();
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

FIGRebirthP3State AIGRebirthPersistenceProbe::MakeP3Checkpoint(
	const int32 CheckpointIndex)
{
	FIGRebirthP3State State;
	State.MistakeCount = CheckpointIndex % 3;
	State.HintElapsedSeconds = 17.0f * CheckpointIndex;
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
		&& Actual.HintStage == Expected.HintStage;
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
	if (!bSuccess
		|| !SaveGame
		|| SaveGame->Progress.SchemaVersion
			!= UIGSaveGame::CurrentSchemaVersion
		|| !State)
	{
		ExitFailure(TEXT("probe load failed or schema mismatched"));
		return;
	}

	if (ProbeMode.Equals(TEXT("P3Read"), ESearchCase::IgnoreCase))
	{
		const FIGRebirthP3State Expected =
			MakeP3Checkpoint(P3CheckpointIndex);
		const FIGRebirthP3State Actual =
			State->GetChapterThreeState().P3;
		const bool bMatched = MatchesP3Checkpoint(Actual, Expected);
		const bool bDeleted =
			UGameplayStatics::DeleteGameInSlot(SlotName, 0)
			&& !UGameplayStatics::DoesSaveGameExist(SlotName, 0);
		if (!bMatched || !bDeleted)
		{
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
