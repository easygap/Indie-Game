#include "Narrative/IGRebirthNarrativeSubsystem.h"

namespace IGRebirthTruth
{
	const TCHAR* NeedWater = TEXT("Truth.NeedWater");
	const TCHAR* Purchase0431 = TEXT("Truth.Purchase0431");
	const TCHAR* Alarm0510 = TEXT("Truth.Alarm0510");
	const TCHAR* DeathOverlay = TEXT("Truth.DeathOverlay");
	const TCHAR* WasSearched = TEXT("Truth.WasSearched");
	const TCHAR* Negligence = TEXT("Truth.Negligence");
	const TCHAR* CatSafe = TEXT("Truth.CatSafe");
	const TCHAR* HoseCause = TEXT("Truth.HoseCause");
	const TCHAR* Fall = TEXT("Truth.Fall");
	const TCHAR* Identity = TEXT("Truth.Identity");
	const TCHAR* RecheckScheduled0731 = TEXT("Truth.RecheckScheduled0731");
	const TCHAR* Found0731 = TEXT("Truth.Found0731");
}

namespace IGRebirthEvidence
{
	const FName EmptyMeasurements(TEXT("P3.EmptyMeasurements"));
	const FName Photo0403(TEXT("ManagementApp.Photo0403"));
	const FName Handover0358(TEXT("Handover.Closed0358"));
	const FName FalseCompletion0620(TEXT("ManagementDb.FalseCompletion0620"));
	const FName PolicePhotoAndAudit(
		TEXT("PoliceChecklist.PhotoAndAudit"));
	const FName CatEnteredPrints(TEXT("P5.CatEnteredPrints"));
	const FName CatExitedPrints(TEXT("P5.CatExitedPrints"));
	const FName HosePawCompression(TEXT("P5.HosePawCompression"));
	const FName CouplingImpact(TEXT("P5.CouplingImpact"));
	const FName InnerRimFriction(TEXT("P5.InnerRimFriction"));
	const FName UpperSlipperEnd(TEXT("P5.UpperSlipperEnd"));
	const FName LiftedPad(TEXT("P5.LiftedPad"));
	const FName CorrodedClips(TEXT("P5.CorrodedClips"));
	const FName InwardHandSmear(TEXT("P5.InwardHandSmear"));
	const FName CurrentSleeveRepair(TEXT("P5.CurrentSleeveThreeStitches"));
	const FName SearchPosterOutfit(TEXT("P5.SearchPosterOutfit"));
	const FName Glasses(TEXT("P5.Glasses"));
	const FName TankSleeveRepair(TEXT("P5.TankSleeveThreeStitches"));
	const FName TankHeelWear(TEXT("P5.TankHeelWear"));
	const FName TankOutfit(TEXT("P5.TankOutfit"));

	bool ConfirmsNegligence(const TArray<FName>& Sources)
	{
		const bool bPriorClosure = Sources.Contains(Handover0358);
		const bool bUnsafeReopen =
			bPriorClosure && Sources.Contains(Photo0403);
		const bool bFalseCompletion =
			bPriorClosure && Sources.Contains(FalseCompletion0620);
		const bool bIndependentPoliceAudit =
			bPriorClosure && Sources.Contains(PolicePhotoAndAudit);
		return (bUnsafeReopen && bFalseCompletion)
			|| bIndependentPoliceAudit;
	}

	bool ConfirmsCatSafe(const TArray<FName>& Sources)
	{
		return Sources.Contains(CatEnteredPrints)
			&& Sources.Contains(CatExitedPrints);
	}

	bool ConfirmsHoseCause(const TArray<FName>& Sources)
	{
		return Sources.Contains(HosePawCompression)
			&& Sources.Contains(CouplingImpact)
			&& Sources.Contains(InnerRimFriction);
	}

	bool ConfirmsFall(const TArray<FName>& Sources)
	{
		return Sources.Contains(UpperSlipperEnd)
			&& Sources.Contains(LiftedPad)
			&& Sources.Contains(CorrodedClips)
			&& Sources.Contains(InwardHandSmear);
	}

	bool ConfirmsIdentity(const TArray<FName>& Sources)
	{
		const bool bDirectRepairMatch =
			Sources.Contains(CurrentSleeveRepair)
			&& Sources.Contains(TankSleeveRepair)
			&& Sources.Contains(TankHeelWear);
		const bool bPosterTriangulation =
			Sources.Contains(SearchPosterOutfit)
			&& Sources.Contains(Glasses)
			&& Sources.Contains(TankOutfit);
		return bDirectRepairMatch || bPosterTriangulation;
	}
}

namespace IGRebirthState
{
	constexpr int32 SnapshotSchemaVersion = 3;
	const FName PuzzleP3(TEXT("P3"));
	const FName PuzzleP5(TEXT("P5"));
	const FName EndingA(TEXT("Ending.A"));
	const FName EndingB(TEXT("Ending.B"));
	const FName ActualStateRestored(TEXT("CH03.ActualStateRestored"));
	const FName EndingAStrongCue(TEXT("Ending.A.StrongCuePlayed"));
	const FName EndingBStrongCue(TEXT("Ending.B.StrongCuePlayed"));
	const FName TankOpenedWithoutFlashlight(
		TEXT("CH03.TankOpenedWithoutFlashlight"));
	const FName TankRevealedAfterFlashlight(
		TEXT("CH03.TankRevealedAfterFlashlight"));
	const FName CommonDiscoverySource(TEXT("Ending.CommonDiscoveryCard"));

	FGameplayTag MakeTruth(const TCHAR* Name)
	{
		return FGameplayTag::RequestGameplayTag(FName(Name), false);
	}

	FIGRebirthTruthRecord* FindTruth(
		FIGRebirthNarrativeSnapshot& Snapshot,
		const TCHAR* Name)
	{
		const FGameplayTag TruthTag = MakeTruth(Name);
		return Snapshot.TruthRecords.FindByPredicate(
			[TruthTag](const FIGRebirthTruthRecord& Record)
			{
				return Record.TruthTag.MatchesTagExact(TruthTag);
			});
	}

	const FIGRebirthTruthRecord* FindTruth(
		const FIGRebirthNarrativeSnapshot& Snapshot,
		const TCHAR* Name)
	{
		const FGameplayTag TruthTag = MakeTruth(Name);
		return Snapshot.TruthRecords.FindByPredicate(
			[TruthTag](const FIGRebirthTruthRecord& Record)
			{
				return Record.TruthTag.MatchesTagExact(TruthTag);
			});
	}

	bool HasTruth(
		const FIGRebirthNarrativeSnapshot& Snapshot,
		const TCHAR* Name)
	{
		const FIGRebirthTruthRecord* Record = FindTruth(Snapshot, Name);
		return Record && Record->bConfirmed;
	}

	void EnsureTruth(
		FIGRebirthNarrativeSnapshot& Snapshot,
		const TCHAR* Name,
		const FName SourceId)
	{
		FIGRebirthTruthRecord* Record = FindTruth(Snapshot, Name);
		if (!Record)
		{
			FIGRebirthTruthRecord NewRecord;
			NewRecord.TruthTag = MakeTruth(Name);
			Snapshot.TruthRecords.Add(MoveTemp(NewRecord));
			Record = &Snapshot.TruthRecords.Last();
		}
		Record->SourceIds.AddUnique(SourceId);
		Record->bConfirmed = true;
	}

	void RemoveTruth(FIGRebirthNarrativeSnapshot& Snapshot, const TCHAR* Name)
	{
		const FGameplayTag TruthTag = MakeTruth(Name);
		Snapshot.TruthRecords.RemoveAll(
			[TruthTag](const FIGRebirthTruthRecord& Record)
			{
				return Record.TruthTag.MatchesTagExact(TruthTag);
			});
	}

	void RecomputeDerivedEvidenceTruths(
		FIGRebirthNarrativeSnapshot& Snapshot)
	{
		const auto Recompute = [&Snapshot](
			const TCHAR* TruthName,
			const TFunctionRef<bool(const TArray<FName>&)> Predicate)
		{
			if (FIGRebirthTruthRecord* Record =
					FindTruth(Snapshot, TruthName))
			{
				Record->bConfirmed = Predicate(Record->SourceIds);
			}
		};
		Recompute(
			IGRebirthTruth::Negligence,
			IGRebirthEvidence::ConfirmsNegligence);
		Recompute(
			IGRebirthTruth::CatSafe,
			IGRebirthEvidence::ConfirmsCatSafe);
		Recompute(
			IGRebirthTruth::HoseCause,
			IGRebirthEvidence::ConfirmsHoseCause);
		Recompute(
			IGRebirthTruth::Fall,
			IGRebirthEvidence::ConfirmsFall);
		Recompute(
			IGRebirthTruth::Identity,
			IGRebirthEvidence::ConfirmsIdentity);
	}

	bool HasAllC5Truths(const FIGRebirthNarrativeSnapshot& Snapshot)
	{
		return HasTruth(Snapshot, IGRebirthTruth::Alarm0510)
			&& HasTruth(Snapshot, IGRebirthTruth::DeathOverlay)
			&& HasTruth(Snapshot, IGRebirthTruth::WasSearched)
			&& HasTruth(Snapshot, IGRebirthTruth::Negligence)
			&& HasTruth(Snapshot, IGRebirthTruth::CatSafe)
			&& HasTruth(Snapshot, IGRebirthTruth::HoseCause)
			&& HasTruth(Snapshot, IGRebirthTruth::Fall)
			&& HasTruth(Snapshot, IGRebirthTruth::Identity)
			&& HasTruth(Snapshot, IGRebirthTruth::RecheckScheduled0731);
	}

	bool IsValidPurchaseProfile(const EIGRebirthPurchaseProfile Value)
	{
		switch (Value)
		{
		case EIGRebirthPurchaseProfile::Unset:
		case EIGRebirthPurchaseProfile::ProfileA500MlX2:
		case EIGRebirthPurchaseProfile::ProfileB1LX1:
		case EIGRebirthPurchaseProfile::ProfileC2LX2:
			return true;
		default:
			return false;
		}
	}

	bool IsValidPaymentMethod(const EIGRebirthPaymentMethod Value)
	{
		switch (Value)
		{
		case EIGRebirthPaymentMethod::Unset:
		case EIGRebirthPaymentMethod::WalletCard:
		case EIGRebirthPaymentMethod::PocketCard:
			return true;
		default:
			return false;
		}
	}

	bool IsValidCatWaterState(const EIGRebirthCatWaterState Value)
	{
		switch (Value)
		{
		case EIGRebirthCatWaterState::Unset:
		case EIGRebirthCatWaterState::BottleCap:
		case EIGRebirthCatWaterState::PaperCup:
		case EIGRebirthCatWaterState::PassedBy:
			return true;
		default:
			return false;
		}
	}

	bool IsValidBottleClosureState(const EIGRebirthBottleClosureState Value)
	{
		switch (Value)
		{
		case EIGRebirthBottleClosureState::Unset:
		case EIGRebirthBottleClosureState::MissingCap:
		case EIGRebirthBottleClosureState::Resealed:
			return true;
		default:
			return false;
		}
	}

	bool IsValidEvidenceId(const EIGRebirthEvidenceId Value)
	{
		const uint8 Raw = static_cast<uint8>(Value);
		return Raw <= static_cast<uint8>(EIGRebirthEvidenceId::SearchPoster);
	}

	bool IsValidEndingChoice(const EIGRebirthEndingChoice Value)
	{
		switch (Value)
		{
		case EIGRebirthEndingChoice::None:
		case EIGRebirthEndingChoice::EndingA:
		case EIGRebirthEndingChoice::EndingB:
			return true;
		default:
			return false;
		}
	}

	void AddUniqueName(TArray<FName>& Names, const FName Name)
	{
		if (!Name.IsNone())
		{
			Names.AddUnique(Name);
		}
	}
}

FGameplayTag UIGRebirthNarrativeSubsystem::Truth(const TCHAR* Leaf)
{
	return FGameplayTag::RequestGameplayTag(FName(Leaf), false);
}

FIGRebirthTruthRecord* UIGRebirthNarrativeSubsystem::FindTruthRecord(
	const FGameplayTag TruthTag)
{
	return State.TruthRecords.FindByPredicate(
		[TruthTag](const FIGRebirthTruthRecord& Record)
		{
			return Record.TruthTag.MatchesTagExact(TruthTag);
		});
}

const FIGRebirthTruthRecord* UIGRebirthNarrativeSubsystem::FindTruthRecord(
	const FGameplayTag TruthTag) const
{
	return State.TruthRecords.FindByPredicate(
		[TruthTag](const FIGRebirthTruthRecord& Record)
		{
			return Record.TruthTag.MatchesTagExact(TruthTag);
		});
}

bool UIGRebirthNarrativeSubsystem::RegisterTruthSource(
	const FGameplayTag TruthTag,
	const FName SourceId,
	const bool bConfirmsTruth)
{
	if (!TruthTag.IsValid() || SourceId.IsNone())
	{
		return false;
	}

	FIGRebirthTruthRecord* Record = FindTruthRecord(TruthTag);
	if (!Record)
	{
		FIGRebirthTruthRecord NewRecord;
		NewRecord.TruthTag = TruthTag;
		State.TruthRecords.Add(MoveTemp(NewRecord));
		Record = &State.TruthRecords.Last();
	}

	const bool bSourceAdded = !Record->SourceIds.Contains(SourceId);
	if (bSourceAdded)
	{
		Record->SourceIds.Add(SourceId);
	}

	const bool bWasConfirmed = Record->bConfirmed;
	if (TruthTag.MatchesTagExact(Truth(IGRebirthTruth::Negligence)))
	{
		// Negligence is a derived conclusion, never a caller-controlled flag.
		// The vendor's 03:58 closure is mandatory; the later reopen/false
		// record pair or an independent police audit must support the rest.
		Record->bConfirmed =
			IGRebirthEvidence::ConfirmsNegligence(Record->SourceIds);
	}
	else if (TruthTag.MatchesTagExact(Truth(IGRebirthTruth::CatSafe)))
	{
		Record->bConfirmed =
			IGRebirthEvidence::ConfirmsCatSafe(Record->SourceIds);
	}
	else if (TruthTag.MatchesTagExact(Truth(IGRebirthTruth::HoseCause)))
	{
		Record->bConfirmed =
			IGRebirthEvidence::ConfirmsHoseCause(Record->SourceIds);
	}
	else if (TruthTag.MatchesTagExact(Truth(IGRebirthTruth::Fall)))
	{
		Record->bConfirmed =
			IGRebirthEvidence::ConfirmsFall(Record->SourceIds);
	}
	else if (TruthTag.MatchesTagExact(Truth(IGRebirthTruth::Identity)))
	{
		Record->bConfirmed =
			IGRebirthEvidence::ConfirmsIdentity(Record->SourceIds);
	}
	else
	{
		Record->bConfirmed |= bConfirmsTruth;
	}
	const bool bConfirmationChanged = bWasConfirmed != Record->bConfirmed;

	if (bSourceAdded || bConfirmationChanged)
	{
		OnTruthChanged.Broadcast(TruthTag, SourceId, Record->bConfirmed);
	}
	NormalizeState(false);
	return bSourceAdded || bConfirmationChanged;
}

bool UIGRebirthNarrativeSubsystem::HasTruth(const FGameplayTag TruthTag) const
{
	const FIGRebirthTruthRecord* Record = FindTruthRecord(TruthTag);
	return Record && Record->bConfirmed;
}

int32 UIGRebirthNarrativeSubsystem::GetTruthSourceCount(
	const FGameplayTag TruthTag) const
{
	const FIGRebirthTruthRecord* Record = FindTruthRecord(TruthTag);
	return Record ? Record->SourceIds.Num() : 0;
}

TArray<FName> UIGRebirthNarrativeSubsystem::GetTruthSources(
	const FGameplayTag TruthTag) const
{
	const FIGRebirthTruthRecord* Record = FindTruthRecord(TruthTag);
	return Record ? Record->SourceIds : TArray<FName>();
}

void UIGRebirthNarrativeSubsystem::RecomputeChapterThreeDebt()
{
	State.NarrativeDebt.Reset();
	for (const TCHAR* RequiredTruth : {
		IGRebirthTruth::Alarm0510,
		IGRebirthTruth::DeathOverlay,
		IGRebirthTruth::WasSearched})
	{
		const FGameplayTag TruthTag = Truth(RequiredTruth);
		if (!HasTruth(TruthTag))
		{
			State.NarrativeDebt.AddTag(TruthTag);
		}
	}
}

bool UIGRebirthNarrativeSubsystem::CanConverge(
	const EIGRebirthConvergencePoint Point) const
{
	auto Has = [this](const TCHAR* Name)
	{
		return HasTruth(Truth(Name));
	};

	switch (Point)
	{
	case EIGRebirthConvergencePoint::C1StorePurchase:
		return Has(IGRebirthTruth::NeedWater)
			|| State.VisitedLocations.Contains(FName(TEXT("CH01.Store")));
	case EIGRebirthConvergencePoint::C2FirstReturn:
		return Has(IGRebirthTruth::Purchase0431);
	case EIGRebirthConvergencePoint::C3SecondMorning:
	{
		int32 Confirmed = 0;
		Confirmed += Has(IGRebirthTruth::Alarm0510) ? 1 : 0;
		Confirmed += Has(IGRebirthTruth::DeathOverlay) ? 1 : 0;
		Confirmed += Has(IGRebirthTruth::WasSearched) ? 1 : 0;
		return Confirmed >= 2;
	}
	case EIGRebirthConvergencePoint::C4RoofReached:
		return State.VisitedLocations.Contains(FName(TEXT("CH03.Roof")));
	case EIGRebirthConvergencePoint::C5FinalChoice:
		return Has(IGRebirthTruth::Alarm0510)
			&& Has(IGRebirthTruth::DeathOverlay)
			&& Has(IGRebirthTruth::WasSearched)
			&& Has(IGRebirthTruth::Negligence)
			&& Has(IGRebirthTruth::CatSafe)
			&& Has(IGRebirthTruth::HoseCause)
			&& Has(IGRebirthTruth::Fall)
			&& Has(IGRebirthTruth::Identity)
			&& Has(IGRebirthTruth::RecheckScheduled0731);
	case EIGRebirthConvergencePoint::C6AfterDiscovery:
		return (State.ResolvedPuzzles.Contains(FName(TEXT("Ending.A")))
				|| State.ResolvedPuzzles.Contains(FName(TEXT("Ending.B"))))
			&& State.PlayedOneShotBeats.Contains(
				FName(TEXT("CH03.ActualStateRestored")))
			&& Has(IGRebirthTruth::Found0731);
	default:
		return false;
	}
}

bool UIGRebirthNarrativeSubsystem::MarkLocationVisited(const FName LocationId)
{
	if (LocationId.IsNone() || State.VisitedLocations.Contains(LocationId))
	{
		return false;
	}
	State.VisitedLocations.Add(LocationId);
	return true;
}

bool UIGRebirthNarrativeSubsystem::MarkPuzzleResolved(const FName PuzzleId)
{
	if (PuzzleId == IGRebirthState::EndingA)
	{
		return SelectChapterThreeEnding(EIGRebirthEndingChoice::EndingA);
	}
	if (PuzzleId == IGRebirthState::EndingB)
	{
		return SelectChapterThreeEnding(EIGRebirthEndingChoice::EndingB);
	}
	if (PuzzleId.IsNone())
	{
		return false;
	}
	State.SkippedPuzzles.Remove(PuzzleId);
	if (State.ResolvedPuzzles.Contains(PuzzleId))
	{
		return false;
	}
	State.ResolvedPuzzles.Add(PuzzleId);
	NormalizeState(false);
	return true;
}

bool UIGRebirthNarrativeSubsystem::MarkPuzzleSkipped(const FName PuzzleId)
{
	if (PuzzleId.IsNone()
		|| State.ResolvedPuzzles.Contains(PuzzleId)
		|| State.SkippedPuzzles.Contains(PuzzleId))
	{
		return false;
	}
	State.SkippedPuzzles.Add(PuzzleId);
	return true;
}

bool UIGRebirthNarrativeSubsystem::MarkOutfitEquipped(const FName ChapterId)
{
	if (ChapterId.IsNone() || State.EquippedOutfitChapters.Contains(ChapterId))
	{
		return false;
	}
	State.EquippedOutfitChapters.Add(ChapterId);
	return true;
}

bool UIGRebirthNarrativeSubsystem::MarkOneShotBeatPlayed(const FName BeatId)
{
	if (BeatId.IsNone() || State.PlayedOneShotBeats.Contains(BeatId))
	{
		return false;
	}
	State.PlayedOneShotBeats.Add(BeatId);
	return true;
}

bool UIGRebirthNarrativeSubsystem::WasOneShotBeatPlayed(const FName BeatId) const
{
	return State.PlayedOneShotBeats.Contains(BeatId);
}

void UIGRebirthNarrativeSubsystem::SetChoices(
	const FIGRebirthChoiceState& Choices)
{
	State.Choices = Choices;
	NormalizeState(false);
}

void UIGRebirthNarrativeSubsystem::SetChapterThreeState(
	const FIGRebirthChapterThreeState& ChapterThreeState)
{
	State.ChapterThree = ChapterThreeState;
	NormalizeState(false);
}

bool UIGRebirthNarrativeSubsystem::SelectChapterThreeEnding(
	const EIGRebirthEndingChoice EndingChoice)
{
	if ((EndingChoice != EIGRebirthEndingChoice::EndingA
			&& EndingChoice != EIGRebirthEndingChoice::EndingB)
		|| State.ChapterThree.EndingChoice != EIGRebirthEndingChoice::None
		|| State.ChapterThree.bCommonDiscoveryCommitted
		|| !CanConverge(EIGRebirthConvergencePoint::C5FinalChoice)
		|| !State.ChapterThree.bTankOpened
		|| State.ChapterThree.AccidentScratchCount != 3
		|| !State.ChapterThree.bAccidentScratchTailSettled)
	{
		return false;
	}

	State.ChapterThree.EndingChoice = EndingChoice;
	State.ChapterThree.bCommonDiscoveryCommitted = false;
	State.ResolvedPuzzles.Remove(IGRebirthState::EndingA);
	State.ResolvedPuzzles.Remove(IGRebirthState::EndingB);
	State.PlayedOneShotBeats.Remove(IGRebirthState::ActualStateRestored);
	State.PlayedOneShotBeats.Remove(IGRebirthState::EndingAStrongCue);
	State.PlayedOneShotBeats.Remove(IGRebirthState::EndingBStrongCue);
	IGRebirthState::RemoveTruth(State, IGRebirthTruth::Found0731);
	IGRebirthState::AddUniqueName(
		State.ResolvedPuzzles,
		EndingChoice == EIGRebirthEndingChoice::EndingA
			? IGRebirthState::EndingA
			: IGRebirthState::EndingB);
	NormalizeState(false);
	return true;
}

bool UIGRebirthNarrativeSubsystem::CommitChapterThreeCommonDiscovery()
{
	if (State.ChapterThree.EndingChoice == EIGRebirthEndingChoice::None)
	{
		return false;
	}

	const bool bWasCommitted =
		State.ChapterThree.bCommonDiscoveryCommitted;
	State.ChapterThree.bCommonDiscoveryCommitted = true;
	IGRebirthState::AddUniqueName(
		State.PlayedOneShotBeats,
		IGRebirthState::ActualStateRestored);
	IGRebirthState::EnsureTruth(
		State,
		IGRebirthTruth::Found0731,
		IGRebirthState::CommonDiscoverySource);
	NormalizeState(false);
	return !bWasCommitted;
}

FIGRebirthNarrativeSnapshot
UIGRebirthNarrativeSubsystem::BuildSnapshot() const
{
	FIGRebirthNarrativeSnapshot Snapshot = State;
	NormalizeSnapshot(Snapshot, true);
	return Snapshot;
}

void UIGRebirthNarrativeSubsystem::RestoreSnapshot(
	const FIGRebirthNarrativeSnapshot& Snapshot)
{
	State = Snapshot;
	// Normalization re-derives every compound truth from its raw sources.
	NormalizeState(true);
}

void UIGRebirthNarrativeSubsystem::ResetChapterThreeAttempt()
{
	const auto RemoveTruthSource = [this](
		const TCHAR* TruthName,
		const FName SourceId)
	{
		if (FIGRebirthTruthRecord* Record = FindTruthRecord(Truth(TruthName)))
		{
			Record->SourceIds.Remove(SourceId);
			Record->bConfirmed = Record->SourceIds.Num() > 0;
			if (Record->SourceIds.IsEmpty())
			{
				State.TruthRecords.RemoveAll(
					[TruthTag = Truth(TruthName)](
						const FIGRebirthTruthRecord& Candidate)
					{
						return Candidate.TruthTag.MatchesTagExact(TruthTag);
					});
			}
		}
	};
	RemoveTruthSource(
		IGRebirthTruth::Alarm0510,
		FName(TEXT("CH03.Planner0510")));
	RemoveTruthSource(
		IGRebirthTruth::WasSearched,
		FName(TEXT("CH03.MotherMessages")));
	RemoveTruthSource(
		IGRebirthTruth::WasSearched,
		FName(TEXT("CH03.SearchPoster")));
	RemoveTruthSource(
		IGRebirthTruth::DeathOverlay,
		FName(TEXT("CH03.PhoneApprovalHistory")));

	for (const TCHAR* TruthName : {
		IGRebirthTruth::Negligence,
		IGRebirthTruth::CatSafe,
		IGRebirthTruth::HoseCause,
		IGRebirthTruth::Fall,
		IGRebirthTruth::Identity,
		IGRebirthTruth::RecheckScheduled0731,
		IGRebirthTruth::Found0731})
	{
		IGRebirthState::RemoveTruth(State, TruthName);
	}

	for (const FName PuzzleId : {
		IGRebirthState::PuzzleP3,
		IGRebirthState::PuzzleP5,
		IGRebirthState::EndingA,
		IGRebirthState::EndingB})
	{
		State.ResolvedPuzzles.Remove(PuzzleId);
		State.SkippedPuzzles.Remove(PuzzleId);
	}
	State.PlayedOneShotBeats.Remove(IGRebirthState::ActualStateRestored);
	State.PlayedOneShotBeats.Remove(IGRebirthState::EndingAStrongCue);
	State.PlayedOneShotBeats.Remove(IGRebirthState::EndingBStrongCue);
	// These guards describe the current CH03 attempt, not the upstream
	// flashlight item. Leaving the dark-open guard behind can suppress every
	// scratch in a restarted attempt that collects the flashlight first.
	State.PlayedOneShotBeats.Remove(
		IGRebirthState::TankOpenedWithoutFlashlight);
	State.PlayedOneShotBeats.Remove(
		IGRebirthState::TankRevealedAfterFlashlight);
	State.VisitedLocations.Remove(FName(TEXT("CH03.Apartment")));
	State.VisitedLocations.Remove(FName(TEXT("CH03.Roof")));
	State.bTankOpenedEarly = false;
	// The flashlight is an upstream CH02 choice (or a subsequently learned
	// memory item), not an ending-branch bit. Restarting CH03 must not put a
	// previously collected object back on the shoe cabinet.
	State.ChapterThree = FIGRebirthChapterThreeState();
	RecomputeChapterThreeDebt();
	NormalizeState(false);
}

void UIGRebirthNarrativeSubsystem::NormalizeState(
	const bool bForStableSave)
{
	NormalizeSnapshot(State, bForStableSave);
}

void UIGRebirthNarrativeSubsystem::NormalizeSnapshot(
	FIGRebirthNarrativeSnapshot& Snapshot,
	const bool bForStableSave)
{
	Snapshot.SchemaVersion = IGRebirthState::SnapshotSchemaVersion;

	if (!IGRebirthState::IsValidPurchaseProfile(
		Snapshot.Choices.PurchaseProfile))
	{
		Snapshot.Choices.PurchaseProfile =
			EIGRebirthPurchaseProfile::Unset;
	}
	if (!IGRebirthState::IsValidPaymentMethod(
		Snapshot.Choices.PaymentMethod))
	{
		Snapshot.Choices.PaymentMethod = EIGRebirthPaymentMethod::Unset;
	}
	if (!IGRebirthState::IsValidCatWaterState(
		Snapshot.Choices.CatWaterState))
	{
		Snapshot.Choices.CatWaterState = EIGRebirthCatWaterState::Unset;
	}
	if (!IGRebirthState::IsValidBottleClosureState(
		Snapshot.Choices.BottleClosureState))
	{
		Snapshot.Choices.BottleClosureState =
			EIGRebirthBottleClosureState::Unset;
	}

	FIGRebirthChapterThreeState& ChapterThree = Snapshot.ChapterThree;
	FIGRebirthP3State& P3 = ChapterThree.P3;
	P3.MistakeCount = FMath::Clamp(P3.MistakeCount, 0, 3);
	P3.ZeroConfirmationTicks = FMath::Clamp(P3.ZeroConfirmationTicks, 0, 2);
	P3.HintElapsedSeconds = FMath::IsFinite(P3.HintElapsedSeconds)
		? FMath::Max(0.0f, P3.HintElapsedSeconds)
		: 0.0f;
	P3.PressureRiseElapsedSeconds = FMath::IsFinite(
		P3.PressureRiseElapsedSeconds)
		? FMath::Max(0.0f, P3.PressureRiseElapsedSeconds)
		: 0.0f;
	P3.HintStage = FMath::Clamp(P3.HintStage, 0, 3);
	P3.PressureKPa = FMath::IsFinite(P3.PressureKPa)
		? FMath::Clamp(P3.PressureKPa, 0.0f, 60.0f)
		: 60.0f;
	if (Snapshot.ResolvedPuzzles.Contains(IGRebirthState::PuzzleP3))
	{
		P3.bCompleted = true;
	}
	if (!P3.bDirectInletClosed || !P3.bReserveInletClosed)
	{
		P3.bPressureReleaseOpen = false;
		P3.bPressureZero = false;
		P3.PressureKPa = 60.0f;
		P3.ZeroConfirmationTicks = 0;
	}
	if (P3.bCompleted)
	{
		P3.bDirectInletClosed = true;
		P3.bReserveInletClosed = true;
		P3.bPressureReleaseOpen = true;
		P3.bPressureZero = true;
		P3.bPressureRiseArmed = false;
		P3.PressureKPa = 0.0f;
		P3.PressureRiseElapsedSeconds = 0.0f;
		P3.ZeroConfirmationTicks = 2;
		IGRebirthState::AddUniqueName(
			Snapshot.ResolvedPuzzles,
			IGRebirthState::PuzzleP3);
	}
	else if (P3.bPressureZero)
	{
		P3.bPressureReleaseOpen = true;
		P3.PressureKPa = 0.0f;
		P3.ZeroConfirmationTicks = 2;
	}
	else if (P3.PressureKPa <= KINDA_SMALL_NUMBER)
	{
		P3.bPressureReleaseOpen = true;
		P3.PressureKPa = 0.0f;
		P3.bPressureZero = P3.ZeroConfirmationTicks >= 2;
	}

	FIGRebirthP4State& P4 = ChapterThree.P4;
	P4.StairLoopCount = FMath::Clamp(P4.StairLoopCount, 0, 3);
	P4.PressureStage = FMath::Clamp(P4.PressureStage, 0, 3);
	P4.HintStage = FMath::Clamp(P4.HintStage, 0, 3);
	P4.PressureRiseElapsedSeconds = FMath::IsFinite(
		P4.PressureRiseElapsedSeconds)
		? FMath::Max(0.0f, P4.PressureRiseElapsedSeconds)
		: 0.0f;
	P4.HintElapsedSeconds = FMath::IsFinite(P4.HintElapsedSeconds)
		? FMath::Max(0.0f, P4.HintElapsedSeconds)
		: 0.0f;
	if (P4.bCompleted)
	{
		P4.bPressureArmed = false;
		P4.PressureRiseElapsedSeconds = 0.0f;
	}

	TArray<FName> UniqueObservedSources;
	for (const FName SourceId : ChapterThree.ObservedP5Sources)
	{
		if (!SourceId.IsNone())
		{
			UniqueObservedSources.AddUnique(SourceId);
		}
	}
	ChapterThree.ObservedP5Sources = MoveTemp(UniqueObservedSources);

	// Raw P5 observations are authoritative on load. Recompute every category
	// formula so an old direct-confirm flag cannot bypass missing evidence.
	IGRebirthState::RecomputeDerivedEvidenceTruths(Snapshot);
	ChapterThree.bP5CatSafeConfirmed =
		IGRebirthState::HasTruth(Snapshot, IGRebirthTruth::CatSafe);
	ChapterThree.bP5HoseCauseConfirmed =
		IGRebirthState::HasTruth(Snapshot, IGRebirthTruth::HoseCause);
	ChapterThree.bP5FallConfirmed =
		IGRebirthState::HasTruth(Snapshot, IGRebirthTruth::Fall);
	ChapterThree.bP5IdentityConfirmed =
		IGRebirthState::HasTruth(Snapshot, IGRebirthTruth::Identity);
	const int32 P5TruthCount =
		(ChapterThree.bP5CatSafeConfirmed ? 1 : 0)
		+ (ChapterThree.bP5HoseCauseConfirmed ? 1 : 0)
		+ (ChapterThree.bP5FallConfirmed ? 1 : 0)
		+ (ChapterThree.bP5IdentityConfirmed ? 1 : 0);
	if (P5TruthCount == 4)
	{
		IGRebirthState::AddUniqueName(
			Snapshot.ResolvedPuzzles,
			IGRebirthState::PuzzleP5);
	}
	else
	{
		Snapshot.ResolvedPuzzles.Remove(IGRebirthState::PuzzleP5);
	}

	if (!IGRebirthState::IsValidEvidenceId(ChapterThree.FocusedEvidence))
	{
		ChapterThree.FocusedEvidence = EIGRebirthEvidenceId::None;
	}
	ChapterThree.bTankOpened |= Snapshot.bTankOpenedEarly;
	ChapterThree.AccidentScratchCount =
		FMath::Clamp(ChapterThree.AccidentScratchCount, 0, 3);
	if (!ChapterThree.bTankOpened)
	{
		ChapterThree.AccidentScratchCount = 0;
		ChapterThree.bLookedAwayAfterFirstScratch = false;
		ChapterThree.bActedAfterSecondScratch = false;
	}
	if (ChapterThree.AccidentScratchCount >= 2 && P5TruthCount < 1)
	{
		ChapterThree.AccidentScratchCount = 1;
	}
	if (ChapterThree.AccidentScratchCount >= 3 && P5TruthCount < 3)
	{
		ChapterThree.AccidentScratchCount = P5TruthCount >= 1 ? 2 : 1;
	}
	if (ChapterThree.AccidentScratchCount != 3)
	{
		ChapterThree.bAccidentScratchTailSettled = false;
	}
	else if (bForStableSave)
	{
		// The tail is only a 0.6-second no-input interval. Saving commits it
		// forward rather than replaying the third high-intensity scratch.
		ChapterThree.bAccidentScratchTailSettled = true;
	}
	if (ChapterThree.AccidentScratchCount < 1)
	{
		ChapterThree.bLookedAwayAfterFirstScratch = false;
	}
	if (ChapterThree.AccidentScratchCount < 2)
	{
		ChapterThree.bActedAfterSecondScratch = false;
	}

	if (!IGRebirthState::IsValidEndingChoice(ChapterThree.EndingChoice))
	{
		ChapterThree.EndingChoice = EIGRebirthEndingChoice::None;
	}
	const bool bHasEndingA =
		Snapshot.ResolvedPuzzles.Contains(IGRebirthState::EndingA);
	const bool bHasEndingB =
		Snapshot.ResolvedPuzzles.Contains(IGRebirthState::EndingB);
	if (ChapterThree.EndingChoice == EIGRebirthEndingChoice::None)
	{
		if (bHasEndingA != bHasEndingB)
		{
			ChapterThree.EndingChoice = bHasEndingA
				? EIGRebirthEndingChoice::EndingA
				: EIGRebirthEndingChoice::EndingB;
		}
		else if (bHasEndingA && bHasEndingB)
		{
			Snapshot.ResolvedPuzzles.Remove(IGRebirthState::EndingA);
			Snapshot.ResolvedPuzzles.Remove(IGRebirthState::EndingB);
		}
	}

	const bool bEndingPrerequisitesValid =
		IGRebirthState::HasAllC5Truths(Snapshot)
		&& ChapterThree.bTankOpened
		&& ChapterThree.AccidentScratchCount == 3
		&& ChapterThree.bAccidentScratchTailSettled;
	if (ChapterThree.EndingChoice != EIGRebirthEndingChoice::None
		&& !bEndingPrerequisitesValid)
	{
		ChapterThree.EndingChoice = EIGRebirthEndingChoice::None;
		ChapterThree.bCommonDiscoveryCommitted = false;
		Snapshot.ResolvedPuzzles.Remove(IGRebirthState::EndingA);
		Snapshot.ResolvedPuzzles.Remove(IGRebirthState::EndingB);
	}

	if (ChapterThree.EndingChoice == EIGRebirthEndingChoice::None)
	{
		ChapterThree.bCommonDiscoveryCommitted = false;
		Snapshot.PlayedOneShotBeats.Remove(
			IGRebirthState::ActualStateRestored);
		Snapshot.PlayedOneShotBeats.Remove(
			IGRebirthState::EndingAStrongCue);
		Snapshot.PlayedOneShotBeats.Remove(
			IGRebirthState::EndingBStrongCue);
		IGRebirthState::RemoveTruth(Snapshot, IGRebirthTruth::Found0731);
	}
	else
	{
		const bool bEndingASelected =
			ChapterThree.EndingChoice == EIGRebirthEndingChoice::EndingA;
		Snapshot.ResolvedPuzzles.Remove(
			bEndingASelected
				? IGRebirthState::EndingB
				: IGRebirthState::EndingA);
		IGRebirthState::AddUniqueName(
			Snapshot.ResolvedPuzzles,
			bEndingASelected
				? IGRebirthState::EndingA
				: IGRebirthState::EndingB);

		const bool bCommonFactsPresent =
			Snapshot.PlayedOneShotBeats.Contains(
				IGRebirthState::ActualStateRestored)
			&& IGRebirthState::HasTruth(
				Snapshot,
				IGRebirthTruth::Found0731);
		ChapterThree.bCommonDiscoveryCommitted |= bCommonFactsPresent;
		if (ChapterThree.bCommonDiscoveryCommitted)
		{
			IGRebirthState::AddUniqueName(
				Snapshot.PlayedOneShotBeats,
				IGRebirthState::ActualStateRestored);
			IGRebirthState::EnsureTruth(
				Snapshot,
				IGRebirthTruth::Found0731,
				IGRebirthState::CommonDiscoverySource);
		}
		else
		{
			// Common discovery is one atomic boundary. A partially migrated
			// pair is normalized back to the pre-card state.
			Snapshot.PlayedOneShotBeats.Remove(
				IGRebirthState::ActualStateRestored);
			IGRebirthState::RemoveTruth(
				Snapshot,
				IGRebirthTruth::Found0731);
		}

		if (bEndingASelected)
		{
			Snapshot.PlayedOneShotBeats.Remove(
				IGRebirthState::EndingBStrongCue);
			if (!ChapterThree.bCommonDiscoveryCommitted)
			{
				Snapshot.PlayedOneShotBeats.Remove(
					IGRebirthState::EndingAStrongCue);
			}
		}
		else
		{
			Snapshot.PlayedOneShotBeats.Remove(
				IGRebirthState::EndingAStrongCue);
		}
	}
}

void UIGRebirthNarrativeSubsystem::ResetNarrative()
{
	State = FIGRebirthNarrativeSnapshot();
}
