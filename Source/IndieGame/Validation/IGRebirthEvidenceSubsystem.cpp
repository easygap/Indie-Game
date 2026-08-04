#include "Validation/IGRebirthEvidenceSubsystem.h"

#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameplayTagContainer.h"
#include "HAL/FileManager.h"
#include "HAL/IConsoleManager.h"
#include "IndieGame.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Narrative/IGRebirthNarrativeSubsystem.h"
#include "Narrative/IGStoryStateSubsystem.h"
#include "Serialization/JsonWriter.h"

namespace IGRebirthEvidenceCapture
{
	constexpr int32 EvidenceSchemaVersion = 1;
	const TCHAR* ConsoleCommandName = TEXT("ig.Rebirth.DumpEvidence");
	const TCHAR* CaptureSwitch = TEXT("IGRebirthEvidenceCapture");
	const TCHAR* AutoDumpSwitch = TEXT("IGRebirthEvidenceAutoDump");
	const TCHAR* OutputPathSwitch = TEXT("IGRebirthEvidencePath=");

	const FName PuzzleP1(TEXT("P1"));
	const FName PuzzleP1Legacy(TEXT("P1.AlarmArithmeticProxy"));
	const FName PuzzleP2(TEXT("P2"));
	const FName PuzzleP2Legacy(TEXT("P2.ReceiptComparisonProxy"));
	const FName PuzzleP3(TEXT("P3"));
	const FName PuzzleP5(TEXT("P5"));
	const FName RoofLocation(TEXT("CH03.Roof"));
	const FName ActualStateRestored(TEXT("CH03.ActualStateRestored"));
	const FName EndingAStrongCue(TEXT("Ending.A.StrongCuePlayed"));
	const FName EndingBStrongCue(TEXT("Ending.B.StrongCuePlayed"));

	const TCHAR* TruthAlarm0510 = TEXT("Truth.Alarm0510");
	const TCHAR* TruthDeathOverlay = TEXT("Truth.DeathOverlay");
	const TCHAR* TruthNegligence = TEXT("Truth.Negligence");
	const TCHAR* TruthFound0731 = TEXT("Truth.Found0731");

	const FName EndingChoiceEvent(TEXT("Choice"));
	const FName ActualStateRestoredEvent(TEXT("ActualStateRestored"));
	const FName Found0731Event(TEXT("Found0731"));
	const FName CommonDiscoveryCardEvent(TEXT("CommonDiscoveryCard"));
	const FName BranchCodaEvent(TEXT("BranchCoda"));

	FString ToStableString(const EIGRebirthPurchaseProfile Value)
	{
		switch (Value)
		{
		case EIGRebirthPurchaseProfile::ProfileA500MlX2:
			return TEXT("profile_a_500ml_x2");
		case EIGRebirthPurchaseProfile::ProfileB1LX1:
			return TEXT("profile_b_1l_x1");
		case EIGRebirthPurchaseProfile::ProfileC2LX2:
			return TEXT("profile_c_2l_x2");
		default:
			return TEXT("unset");
		}
	}

	FString ToStableString(const EIGRebirthPaymentMethod Value)
	{
		switch (Value)
		{
		case EIGRebirthPaymentMethod::WalletCard:
			return TEXT("wallet_card");
		case EIGRebirthPaymentMethod::PocketCard:
			return TEXT("pocket_card");
		default:
			return TEXT("unset");
		}
	}

	FString ToStableString(const EIGRebirthCatWaterState Value)
	{
		switch (Value)
		{
		case EIGRebirthCatWaterState::BottleCap:
			return TEXT("bottle_cap");
		case EIGRebirthCatWaterState::PaperCup:
			return TEXT("paper_cup");
		case EIGRebirthCatWaterState::PassedBy:
			return TEXT("passed_by");
		default:
			return TEXT("unset");
		}
	}

	FString ToStableString(const EIGRebirthBottleClosureState Value)
	{
		switch (Value)
		{
		case EIGRebirthBottleClosureState::MissingCap:
			return TEXT("missing_cap");
		case EIGRebirthBottleClosureState::Resealed:
			return TEXT("resealed");
		default:
			return TEXT("unset");
		}
	}

	FString ToStableString(const EIGRebirthEvidenceId Value)
	{
		switch (Value)
		{
		case EIGRebirthEvidenceId::CatEntered:
			return TEXT("cat_entered");
		case EIGRebirthEvidenceId::CatExited:
			return TEXT("cat_exited");
		case EIGRebirthEvidenceId::HosePaw:
			return TEXT("hose_paw");
		case EIGRebirthEvidenceId::HoseImpact:
			return TEXT("hose_impact");
		case EIGRebirthEvidenceId::Bag:
			return TEXT("bag");
		case EIGRebirthEvidenceId::WetRung:
			return TEXT("wet_rung");
		case EIGRebirthEvidenceId::HandSmear:
			return TEXT("hand_smear");
		case EIGRebirthEvidenceId::Glasses:
			return TEXT("glasses");
		case EIGRebirthEvidenceId::TankClothing:
			return TEXT("tank_clothing");
		case EIGRebirthEvidenceId::CurrentSleeve:
			return TEXT("current_sleeve");
		case EIGRebirthEvidenceId::SearchPoster:
			return TEXT("search_poster");
		default:
			return TEXT("none");
		}
	}

	FString ToStableString(const EIGRebirthEndingChoice Value)
	{
		switch (Value)
		{
		case EIGRebirthEndingChoice::EndingA:
			return TEXT("A");
		case EIGRebirthEndingChoice::EndingB:
			return TEXT("B");
		default:
			return TEXT("none");
		}
	}

	TArray<FString> SortNames(const TArray<FName>& Names)
	{
		TArray<FString> Values;
		Values.Reserve(Names.Num());
		for (const FName Name : Names)
		{
			if (!Name.IsNone())
			{
				Values.Add(Name.ToString());
			}
		}
		Values.Sort();
		return Values;
	}

	TArray<FString> SortTags(const FGameplayTagContainer& Tags)
	{
		TArray<FGameplayTag> TagArray;
		Tags.GetGameplayTagArray(TagArray);
		TArray<FString> Values;
		Values.Reserve(TagArray.Num());
		for (const FGameplayTag& Tag : TagArray)
		{
			Values.Add(Tag.ToString());
		}
		Values.Sort();
		return Values;
	}

	const FIGRebirthTruthRecord* FindTruth(
		const FIGRebirthNarrativeSnapshot& Snapshot,
		const TCHAR* TruthName)
	{
		const FGameplayTag TruthTag = FGameplayTag::RequestGameplayTag(
			FName(TruthName),
			false);
		return Snapshot.TruthRecords.FindByPredicate(
			[TruthTag](const FIGRebirthTruthRecord& Record)
			{
				return Record.TruthTag.MatchesTagExact(TruthTag);
			});
	}

	bool HasTruth(
		const FIGRebirthNarrativeSnapshot& Snapshot,
		const TCHAR* TruthName)
	{
		const FIGRebirthTruthRecord* Record =
			FindTruth(Snapshot, TruthName);
		return Record && Record->bConfirmed;
	}

	template <typename WriterType>
	void WriteStringArray(
		WriterType& Writer,
		const TCHAR* Identifier,
		const TArray<FString>& Values)
	{
		Writer.WriteArrayStart(Identifier);
		for (const FString& Value : Values)
		{
			Writer.WriteValue(Value);
		}
		Writer.WriteArrayEnd();
	}

	template <typename WriterType>
	void WriteTruthEvidence(
		WriterType& Writer,
		const FIGRebirthNarrativeSnapshot& Snapshot,
		const TCHAR* TruthName)
	{
		const FIGRebirthTruthRecord* Record =
			FindTruth(Snapshot, TruthName);
		Writer.WriteValue(
			TEXT("truthConfirmed"),
			Record && Record->bConfirmed);
		WriteStringArray(
			Writer,
			TEXT("truthSources"),
			Record ? SortNames(Record->SourceIds) : TArray<FString>());
	}

	bool ContainsName(
		const TArray<FName>& Values,
		const FName Expected)
	{
		return Values.Contains(Expected);
	}

	FString SanitizeCaptureLabel(const FString& CaptureLabel)
	{
		FString Label = CaptureLabel.TrimStartAndEnd();
		if (Label.IsEmpty())
		{
			Label = TEXT("manual");
		}
		Label = FPaths::MakeValidFileName(Label);
		return Label.IsEmpty() ? TEXT("manual") : Label;
	}
}

void UIGRebirthEvidenceSubsystem::Initialize(
	FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	bAutoDumpOnEvent = FParse::Param(
		FCommandLine::Get(),
		IGRebirthEvidenceCapture::AutoDumpSwitch);
	FParse::Value(
		FCommandLine::Get(),
		IGRebirthEvidenceCapture::OutputPathSwitch,
		ConfiguredOutputPath);
	ConfiguredOutputPath.TrimStartAndEndInline();
	bCaptureEnabled = bAutoDumpOnEvent
		|| !ConfiguredOutputPath.IsEmpty()
		|| FParse::Param(
			FCommandLine::Get(),
			IGRebirthEvidenceCapture::CaptureSwitch);

	if (!IConsoleManager::Get().FindConsoleObject(
			IGRebirthEvidenceCapture::ConsoleCommandName))
	{
		DumpConsoleCommand = IConsoleManager::Get().RegisterConsoleCommand(
			IGRebirthEvidenceCapture::ConsoleCommandName,
			TEXT(
				"Dump deterministic REBIRTH runtime evidence. "
				"Usage: ig.Rebirth.DumpEvidence "
				"[label] [output.json|directory]"),
			FConsoleCommandWithArgsDelegate::CreateUObject(
				this,
				&ThisClass::HandleDumpConsoleCommand),
			ECVF_Default);
	}
}

void UIGRebirthEvidenceSubsystem::Deinitialize()
{
	if (DumpConsoleCommand)
	{
		IConsoleManager::Get().UnregisterConsoleObject(
			DumpConsoleCommand,
			false);
		DumpConsoleCommand = nullptr;
	}
	Super::Deinitialize();
}

void UIGRebirthEvidenceSubsystem::RecordEndingEvent(
	UObject* WorldContextObject,
	const FName EventId)
{
	const UWorld* World = WorldContextObject
		? WorldContextObject->GetWorld()
		: nullptr;
	UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
	UIGRebirthEvidenceSubsystem* Evidence = GameInstance
		? GameInstance->GetSubsystem<UIGRebirthEvidenceSubsystem>()
		: nullptr;
	if (Evidence)
	{
		Evidence->RecordEndingEventInternal(EventId);
	}
}

void UIGRebirthEvidenceSubsystem::RecordPuzzleFourObservation(
	UObject* WorldContextObject,
	const int32 DownwardLoopCount,
	const bool bUpwardRouteRevealed)
{
	const UWorld* World = WorldContextObject
		? WorldContextObject->GetWorld()
		: nullptr;
	UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
	UIGRebirthEvidenceSubsystem* Evidence = GameInstance
		? GameInstance->GetSubsystem<UIGRebirthEvidenceSubsystem>()
		: nullptr;
	if (Evidence)
	{
		Evidence->RecordPuzzleFourObservationInternal(
			DownwardLoopCount,
			bUpwardRouteRevealed);
	}
}

void UIGRebirthEvidenceSubsystem::HandleDumpConsoleCommand(
	const TArray<FString>& Arguments)
{
	bCaptureEnabled = true;
	const FString CaptureLabel = Arguments.IsValidIndex(0)
		? Arguments[0]
		: TEXT("manual");
	const FString OutputOverride = Arguments.IsValidIndex(1)
		? Arguments[1]
		: FString();
	DumpEvidence(CaptureLabel, OutputOverride);
}

void UIGRebirthEvidenceSubsystem::RecordEndingEventInternal(
	const FName EventId)
{
	if (!bCaptureEnabled || EventId.IsNone())
	{
		return;
	}

	const UGameInstance* GameInstance = GetGameInstance();
	const UIGRebirthNarrativeSubsystem* Narrative = GameInstance
		? GameInstance->GetSubsystem<UIGRebirthNarrativeSubsystem>()
		: nullptr;
	if (!Narrative)
	{
		return;
	}

	const FIGRebirthNarrativeSnapshot Snapshot =
		Narrative->BuildSnapshot();
	FIGRebirthEndingTimelineEntry& Entry =
		EndingTimeline.AddDefaulted_GetRef();
	Entry.Sequence = NextTimelineSequence++;
	Entry.EventId = EventId;
	Entry.EndingChoice = Snapshot.ChapterThree.EndingChoice;
	Entry.bActualStateRestored = IGRebirthEvidenceCapture::ContainsName(
		Snapshot.PlayedOneShotBeats,
		IGRebirthEvidenceCapture::ActualStateRestored);
	Entry.bFound0731 = IGRebirthEvidenceCapture::HasTruth(
		Snapshot,
		IGRebirthEvidenceCapture::TruthFound0731);
	Entry.bCommonDiscoveryCommitted =
		Snapshot.ChapterThree.bCommonDiscoveryCommitted;
	AutoDumpAfterEvent(EventId);
}

void UIGRebirthEvidenceSubsystem::RecordPuzzleFourObservationInternal(
	const int32 DownwardLoopCount,
	const bool bUpwardRouteRevealed)
{
	if (!bCaptureEnabled)
	{
		return;
	}

	bPuzzleFourObserved |= DownwardLoopCount > 0;
	PuzzleFourDownwardLoopCount = FMath::Max(
		PuzzleFourDownwardLoopCount,
		FMath::Max(0, DownwardLoopCount));
	bPuzzleFourUpwardRouteRevealed |= bUpwardRouteRevealed;
	if (bAutoDumpOnEvent)
	{
		AutoDumpAfterEvent(
			bUpwardRouteRevealed
				? FName(TEXT("P4.UpwardRoute"))
				: FName(TEXT("P4.DownwardLoop")));
	}
}

bool UIGRebirthEvidenceSubsystem::DumpEvidence(
	const FString& CaptureLabel,
	const FString& OutputPathOverride)
{
	const FString StableLabel =
		IGRebirthEvidenceCapture::SanitizeCaptureLabel(CaptureLabel);
	const FString OutputPath =
		ResolveOutputPath(StableLabel, OutputPathOverride);
	const FString OutputDirectory = FPaths::GetPath(OutputPath);
	if (OutputPath.IsEmpty()
		|| OutputDirectory.IsEmpty()
		|| !IFileManager::Get().MakeDirectory(
			*OutputDirectory,
			true))
	{
		UE_LOG(
			LogIndieGame,
			Error,
			TEXT("REBIRTH_EVIDENCE FAIL invalid output path: %s"),
			*OutputPath);
		return false;
	}

	const FString Json = BuildDeterministicJson(StableLabel);
	const FString TemporaryPath = OutputPath + TEXT(".tmp");
	IFileManager::Get().Delete(
		*TemporaryPath,
		false,
		true,
		true);
	if (!FFileHelper::SaveStringToFile(
			Json,
			*TemporaryPath,
			FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
	{
		UE_LOG(
			LogIndieGame,
			Error,
			TEXT("REBIRTH_EVIDENCE FAIL could not write temp file: %s"),
			*TemporaryPath);
		return false;
	}

	if (!IFileManager::Get().Move(
		*OutputPath,
		*TemporaryPath,
		true,
		true,
		false,
		false))
	{
		IFileManager::Get().Delete(
			*TemporaryPath,
			false,
			true,
			true);
		UE_LOG(
			LogIndieGame,
			Error,
			TEXT("REBIRTH_EVIDENCE FAIL atomic replace failed: %s"),
			*OutputPath);
		return false;
	}

	UE_LOG(
		LogIndieGame,
		Display,
		TEXT("REBIRTH_EVIDENCE PASS %s"),
		*OutputPath);
	return true;
}

FString UIGRebirthEvidenceSubsystem::BuildDeterministicJson(
	const FString& CaptureLabel) const
{
	using namespace IGRebirthEvidenceCapture;

	const UGameInstance* GameInstance = GetGameInstance();
	const UIGRebirthNarrativeSubsystem* Narrative = GameInstance
		? GameInstance->GetSubsystem<UIGRebirthNarrativeSubsystem>()
		: nullptr;
	const UIGStoryStateSubsystem* LegacyStory = GameInstance
		? GameInstance->GetSubsystem<UIGStoryStateSubsystem>()
		: nullptr;
	const FIGRebirthNarrativeSnapshot Snapshot = Narrative
		? Narrative->BuildSnapshot()
		: FIGRebirthNarrativeSnapshot();

	FString Json;
	const TSharedRef<TJsonWriter<>> Writer =
		TJsonWriterFactory<>::Create(&Json);
	Writer->WriteObjectStart();
	Writer->WriteValue(
		TEXT("evidenceSchemaVersion"),
		EvidenceSchemaVersion);
	Writer->WriteValue(TEXT("captureLabel"), CaptureLabel);
	Writer->WriteValue(
		TEXT("snapshotSchemaVersion"),
		Snapshot.SchemaVersion);

	Writer->WriteObjectStart(TEXT("convergence"));
	Writer->WriteValue(
		TEXT("C1"),
		Narrative && Narrative->CanConverge(
			EIGRebirthConvergencePoint::C1StorePurchase));
	Writer->WriteValue(
		TEXT("C2"),
		Narrative && Narrative->CanConverge(
			EIGRebirthConvergencePoint::C2FirstReturn));
	Writer->WriteValue(
		TEXT("C3"),
		Narrative && Narrative->CanConverge(
			EIGRebirthConvergencePoint::C3SecondMorning));
	Writer->WriteValue(
		TEXT("C4"),
		Narrative && Narrative->CanConverge(
			EIGRebirthConvergencePoint::C4RoofReached));
	Writer->WriteValue(
		TEXT("C5"),
		Narrative && Narrative->CanConverge(
			EIGRebirthConvergencePoint::C5FinalChoice));
	Writer->WriteValue(
		TEXT("C6"),
		Narrative && Narrative->CanConverge(
			EIGRebirthConvergencePoint::C6AfterDiscovery));
	Writer->WriteObjectEnd();

	WriteStringArray(
		*Writer,
		TEXT("narrativeDebt"),
		SortTags(Snapshot.NarrativeDebt));

	Writer->WriteObjectStart(TEXT("choices"));
	Writer->WriteValue(
		TEXT("purchaseProfile"),
		ToStableString(Snapshot.Choices.PurchaseProfile));
	Writer->WriteValue(
		TEXT("paymentMethod"),
		ToStableString(Snapshot.Choices.PaymentMethod));
	Writer->WriteValue(
		TEXT("catWaterState"),
		ToStableString(Snapshot.Choices.CatWaterState));
	Writer->WriteValue(
		TEXT("bottleClosureState"),
		ToStableString(Snapshot.Choices.BottleClosureState));
	Writer->WriteValue(
		TEXT("hasPaperCup"),
		Snapshot.Choices.bHasPaperCup);
	Writer->WriteValue(
		TEXT("waitedForCat"),
		Snapshot.Choices.bWaitedForCat);
	Writer->WriteObjectEnd();

	Writer->WriteObjectStart(TEXT("inventory"));
	Writer->WriteValue(
		TEXT("hasMemoryFlashlight"),
		Snapshot.bHasMemoryFlashlight);
	Writer->WriteValue(
		TEXT("tankOpenedEarly"),
		Snapshot.bTankOpenedEarly);
	Writer->WriteObjectEnd();

	Writer->WriteObjectStart(TEXT("history"));
	WriteStringArray(
		*Writer,
		TEXT("visitedLocations"),
		SortNames(Snapshot.VisitedLocations));
	WriteStringArray(
		*Writer,
		TEXT("resolvedPuzzles"),
		SortNames(Snapshot.ResolvedPuzzles));
	WriteStringArray(
		*Writer,
		TEXT("skippedPuzzles"),
		SortNames(Snapshot.SkippedPuzzles));
	WriteStringArray(
		*Writer,
		TEXT("equippedOutfitChapters"),
		SortNames(Snapshot.EquippedOutfitChapters));
	WriteStringArray(
		*Writer,
		TEXT("playedOneShotBeats"),
		SortNames(Snapshot.PlayedOneShotBeats));
	Writer->WriteObjectEnd();

	Writer->WriteObjectStart(TEXT("puzzles"));
	const bool bChapterTwoConverged = Narrative
		&& Narrative->CanConverge(
			EIGRebirthConvergencePoint::C3SecondMorning);
	const bool bRoofReached =
		ContainsName(Snapshot.VisitedLocations, RoofLocation);
	Writer->WriteObjectStart(TEXT("P1"));
	const bool bP1Resolved =
		ContainsName(Snapshot.ResolvedPuzzles, PuzzleP1)
		|| ContainsName(Snapshot.ResolvedPuzzles, PuzzleP1Legacy);
	const bool bP1ExplicitlySkipped =
		ContainsName(Snapshot.SkippedPuzzles, PuzzleP1)
		|| ContainsName(Snapshot.SkippedPuzzles, PuzzleP1Legacy);
	Writer->WriteValue(
		TEXT("resolved"),
		bP1Resolved);
	Writer->WriteValue(TEXT("observed"), bP1Resolved);
	Writer->WriteValue(TEXT("confirmed"), bP1Resolved);
	Writer->WriteValue(
		TEXT("explicitlySkipped"),
		bP1ExplicitlySkipped);
	Writer->WriteValue(
		TEXT("routeSkipped"),
		bP1ExplicitlySkipped
			|| (bChapterTwoConverged && !bP1Resolved));
	WriteTruthEvidence(*Writer, Snapshot, TruthAlarm0510);
	Writer->WriteObjectEnd();

	Writer->WriteObjectStart(TEXT("P2"));
	const bool bP2Resolved =
		ContainsName(Snapshot.ResolvedPuzzles, PuzzleP2)
		|| ContainsName(Snapshot.ResolvedPuzzles, PuzzleP2Legacy);
	const bool bP2ExplicitlySkipped =
		ContainsName(Snapshot.SkippedPuzzles, PuzzleP2)
		|| ContainsName(Snapshot.SkippedPuzzles, PuzzleP2Legacy);
	Writer->WriteValue(
		TEXT("resolved"),
		bP2Resolved);
	Writer->WriteValue(TEXT("observed"), bP2Resolved);
	Writer->WriteValue(TEXT("confirmed"), bP2Resolved);
	Writer->WriteValue(
		TEXT("explicitlySkipped"),
		bP2ExplicitlySkipped);
	Writer->WriteValue(
		TEXT("routeSkipped"),
		bP2ExplicitlySkipped
			|| (bChapterTwoConverged && !bP2Resolved));
	WriteTruthEvidence(*Writer, Snapshot, TruthDeathOverlay);
	Writer->WriteObjectEnd();

	const FIGRebirthP3State& P3 = Snapshot.ChapterThree.P3;
	const bool bP3Resolved =
		ContainsName(Snapshot.ResolvedPuzzles, PuzzleP3);
	const bool bP3ExplicitlySkipped =
		ContainsName(Snapshot.SkippedPuzzles, PuzzleP3);
	const bool bP3Observed = P3.bDirectInletClosed
		|| P3.bReserveInletClosed
		|| P3.bPressureReleaseOpen
		|| P3.MistakeCount > 0
		|| P3.HintElapsedSeconds > 0.0f
		|| bP3Resolved;
	Writer->WriteObjectStart(TEXT("P3"));
	Writer->WriteValue(TEXT("resolved"), bP3Resolved);
	Writer->WriteValue(TEXT("observed"), bP3Observed);
	Writer->WriteValue(TEXT("confirmed"), P3.bCompleted);
	Writer->WriteValue(
		TEXT("explicitlySkipped"),
		bP3ExplicitlySkipped);
	Writer->WriteValue(
		TEXT("routeSkipped"),
		bP3ExplicitlySkipped || (bRoofReached && !bP3Resolved));
	Writer->WriteValue(
		TEXT("directInletClosed"),
		P3.bDirectInletClosed);
	Writer->WriteValue(
		TEXT("reserveInletClosed"),
		P3.bReserveInletClosed);
	Writer->WriteValue(
		TEXT("pressureReleaseOpen"),
		P3.bPressureReleaseOpen);
	Writer->WriteValue(TEXT("pressureZero"), P3.bPressureZero);
	Writer->WriteValue(TEXT("completed"), P3.bCompleted);
	Writer->WriteValue(TEXT("pressureKPa"), P3.PressureKPa);
	Writer->WriteValue(TEXT("mistakeCount"), P3.MistakeCount);
	Writer->WriteValue(
		TEXT("zeroConfirmationTicks"),
		P3.ZeroConfirmationTicks);
	Writer->WriteValue(
		TEXT("hintElapsedSeconds"),
		P3.HintElapsedSeconds);
	Writer->WriteValue(TEXT("hintStage"), P3.HintStage);
	WriteTruthEvidence(*Writer, Snapshot, TruthNegligence);
	Writer->WriteObjectEnd();

	Writer->WriteObjectStart(TEXT("P4"));
	Writer->WriteValue(
		TEXT("observed"),
		bPuzzleFourObserved);
	Writer->WriteValue(
		TEXT("downwardLoopCount"),
		PuzzleFourDownwardLoopCount);
	Writer->WriteValue(
		TEXT("upwardRouteRevealed"),
		bPuzzleFourUpwardRouteRevealed);
	Writer->WriteValue(
		TEXT("confirmed"),
		bPuzzleFourUpwardRouteRevealed || bRoofReached);
	Writer->WriteValue(
		TEXT("resolved"),
		bPuzzleFourUpwardRouteRevealed || bRoofReached);
	Writer->WriteValue(
		TEXT("skipped"),
		bRoofReached && !bPuzzleFourObserved);
	Writer->WriteValue(
		TEXT("derivation"),
		TEXT("runtime_observation_or_roof_reached"));
	Writer->WriteObjectEnd();

	Writer->WriteObjectStart(TEXT("P5"));
	const bool bP5Resolved =
		ContainsName(Snapshot.ResolvedPuzzles, PuzzleP5);
	const bool bP5ExplicitlySkipped =
		ContainsName(Snapshot.SkippedPuzzles, PuzzleP5);
	Writer->WriteValue(
		TEXT("resolved"),
		bP5Resolved);
	Writer->WriteValue(
		TEXT("observed"),
		!Snapshot.ChapterThree.ObservedP5Sources.IsEmpty());
	Writer->WriteValue(TEXT("confirmed"), bP5Resolved);
	Writer->WriteValue(
		TEXT("explicitlySkipped"),
		bP5ExplicitlySkipped);
	Writer->WriteValue(
		TEXT("routeSkipped"),
		bP5ExplicitlySkipped);
	Writer->WriteValue(
		TEXT("focusedEvidence"),
		ToStableString(Snapshot.ChapterThree.FocusedEvidence));
	WriteStringArray(
		*Writer,
		TEXT("observedSources"),
		SortNames(Snapshot.ChapterThree.ObservedP5Sources));
	Writer->WriteValue(
		TEXT("catSafeConfirmed"),
		Snapshot.ChapterThree.bP5CatSafeConfirmed);
	Writer->WriteValue(
		TEXT("hoseCauseConfirmed"),
		Snapshot.ChapterThree.bP5HoseCauseConfirmed);
	Writer->WriteValue(
		TEXT("fallConfirmed"),
		Snapshot.ChapterThree.bP5FallConfirmed);
	Writer->WriteValue(
		TEXT("identityConfirmed"),
		Snapshot.ChapterThree.bP5IdentityConfirmed);
	Writer->WriteValue(
		TEXT("tankOpened"),
		Snapshot.ChapterThree.bTankOpened);
	Writer->WriteValue(
		TEXT("accidentScratchCount"),
		Snapshot.ChapterThree.AccidentScratchCount);
	Writer->WriteValue(
		TEXT("accidentScratchTailSettled"),
		Snapshot.ChapterThree.bAccidentScratchTailSettled);
	Writer->WriteValue(
		TEXT("lookedAwayAfterFirstScratch"),
		Snapshot.ChapterThree.bLookedAwayAfterFirstScratch);
	Writer->WriteValue(
		TEXT("actedAfterSecondScratch"),
		Snapshot.ChapterThree.bActedAfterSecondScratch);
	Writer->WriteObjectEnd();
	Writer->WriteObjectEnd();

	TArray<FIGRebirthTruthRecord> SortedTruths = Snapshot.TruthRecords;
	SortedTruths.Sort(
		[](const FIGRebirthTruthRecord& Left,
			const FIGRebirthTruthRecord& Right)
		{
			return Left.TruthTag.ToString() < Right.TruthTag.ToString();
		});
	Writer->WriteArrayStart(TEXT("truths"));
	for (const FIGRebirthTruthRecord& Record : SortedTruths)
	{
		Writer->WriteObjectStart();
		Writer->WriteValue(TEXT("tag"), Record.TruthTag.ToString());
		Writer->WriteValue(TEXT("confirmed"), Record.bConfirmed);
		WriteStringArray(
			*Writer,
			TEXT("sources"),
			SortNames(Record.SourceIds));
		Writer->WriteObjectEnd();
	}
	Writer->WriteArrayEnd();

	Writer->WriteObjectStart(TEXT("chapterThree"));
	Writer->WriteValue(
		TEXT("apartmentFridgeInspected"),
		Snapshot.ChapterThree.bApartmentFridgeInspected);
	Writer->WriteValue(
		TEXT("endingChoice"),
		ToStableString(Snapshot.ChapterThree.EndingChoice));
	Writer->WriteValue(
		TEXT("commonDiscoveryCommitted"),
		Snapshot.ChapterThree.bCommonDiscoveryCommitted);
	Writer->WriteObjectEnd();

	const TArray<FName> ExpectedEndingEvents = {
		EndingChoiceEvent,
		ActualStateRestoredEvent,
		Found0731Event,
		CommonDiscoveryCardEvent,
		BranchCodaEvent};
	int32 ExpectedEventIndex = 0;
	TSet<FName> SeenEndingEvents;
	bool bDuplicateFree = true;
	for (const FIGRebirthEndingTimelineEntry& Entry : EndingTimeline)
	{
		if (SeenEndingEvents.Contains(Entry.EventId))
		{
			bDuplicateFree = false;
		}
		SeenEndingEvents.Add(Entry.EventId);
		if (ExpectedEndingEvents.IsValidIndex(ExpectedEventIndex)
			&& Entry.EventId == ExpectedEndingEvents[ExpectedEventIndex])
		{
			++ExpectedEventIndex;
		}
	}

	Writer->WriteObjectStart(TEXT("ending"));
	Writer->WriteValue(
		TEXT("choice"),
		ToStableString(Snapshot.ChapterThree.EndingChoice));
	Writer->WriteValue(
		TEXT("actualStateRestored"),
		ContainsName(
			Snapshot.PlayedOneShotBeats,
			ActualStateRestored));
	Writer->WriteValue(
		TEXT("found0731"),
		HasTruth(Snapshot, TruthFound0731));
	Writer->WriteValue(
		TEXT("commonDiscoveryCommitted"),
		Snapshot.ChapterThree.bCommonDiscoveryCommitted);
	Writer->WriteValue(
		TEXT("endingAStrongCuePlayed"),
		ContainsName(Snapshot.PlayedOneShotBeats, EndingAStrongCue));
	Writer->WriteValue(
		TEXT("endingBStrongCuePlayed"),
		ContainsName(Snapshot.PlayedOneShotBeats, EndingBStrongCue));
	Writer->WriteArrayStart(TEXT("expectedTimeline"));
	for (const FName EventId : ExpectedEndingEvents)
	{
		Writer->WriteValue(EventId.ToString());
	}
	Writer->WriteArrayEnd();
	Writer->WriteArrayStart(TEXT("timeline"));
	for (const FIGRebirthEndingTimelineEntry& Entry : EndingTimeline)
	{
		Writer->WriteObjectStart();
		Writer->WriteValue(TEXT("sequence"), Entry.Sequence);
		Writer->WriteValue(TEXT("event"), Entry.EventId.ToString());
		Writer->WriteValue(
			TEXT("choice"),
			ToStableString(Entry.EndingChoice));
		Writer->WriteValue(
			TEXT("actualStateRestored"),
			Entry.bActualStateRestored);
		Writer->WriteValue(TEXT("found0731"), Entry.bFound0731);
		Writer->WriteValue(
			TEXT("commonDiscoveryCommitted"),
			Entry.bCommonDiscoveryCommitted);
		Writer->WriteObjectEnd();
	}
	Writer->WriteArrayEnd();
	Writer->WriteValue(
		TEXT("timelineCompleteInOrder"),
		ExpectedEventIndex == ExpectedEndingEvents.Num());
	Writer->WriteValue(TEXT("timelineDuplicateFree"), bDuplicateFree);
	Writer->WriteObjectEnd();

	const FGameplayTagContainer LegacyTags = LegacyStory
		? LegacyStory->GetStateSnapshot()
		: FGameplayTagContainer();
	WriteStringArray(
		*Writer,
		TEXT("legacyStoryTags"),
		SortTags(LegacyTags));
	Writer->WriteObjectEnd();
	Writer->Close();
	return Json;
}

FString UIGRebirthEvidenceSubsystem::ResolveOutputPath(
	const FString& CaptureLabel,
	const FString& OutputPathOverride) const
{
	const FString ValidationRoot = FPaths::ConvertRelativePathToFull(
		FPaths::Combine(
			FPaths::ProjectSavedDir(),
			TEXT("Validation"),
			TEXT("RebirthManual")));
	FString RequestedPath = OutputPathOverride.TrimStartAndEnd();
	if (RequestedPath.IsEmpty())
	{
		RequestedPath = ConfiguredOutputPath;
	}

	if (RequestedPath.IsEmpty())
	{
		RequestedPath = ValidationRoot;
	}
	const bool bWasRelative = FPaths::IsRelative(RequestedPath);
	if (bWasRelative)
	{
		RequestedPath = FPaths::Combine(ValidationRoot, RequestedPath);
	}
	RequestedPath = FPaths::ConvertRelativePathToFull(RequestedPath);
	FPaths::NormalizeFilename(RequestedPath);
	if (bWasRelative)
	{
		FString NormalizedRoot = ValidationRoot;
		FPaths::NormalizeFilename(NormalizedRoot);
		FString RootPrefix = NormalizedRoot;
		RootPrefix.AppendChar(TEXT('/'));
		if (!RequestedPath.Equals(
				NormalizedRoot,
				ESearchCase::IgnoreCase)
			&& !RequestedPath.StartsWith(
				RootPrefix,
				ESearchCase::IgnoreCase))
		{
			return FString();
		}
	}

	if (FPaths::GetExtension(RequestedPath, false).Equals(
		TEXT("json"),
		ESearchCase::IgnoreCase))
	{
		return RequestedPath;
	}
	return FPaths::Combine(
		RequestedPath,
		CaptureLabel + TEXT(".json"));
}

void UIGRebirthEvidenceSubsystem::AutoDumpAfterEvent(
	const FName EventId)
{
	if (!bAutoDumpOnEvent)
	{
		return;
	}

	const FString Label = FString::Printf(
		TEXT("event-%03d-%s"),
		NextAutoDumpSequence++,
		*EventId.ToString());
	DumpEvidence(Label);
}
