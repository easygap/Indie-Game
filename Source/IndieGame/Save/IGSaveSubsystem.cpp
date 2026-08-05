#include "Save/IGSaveSubsystem.h"

#include "IndieGame.h"
#include "Engine/GameInstance.h"
#include "Kismet/GameplayStatics.h"
#include "Narrative/IGRebirthNarrativeSubsystem.h"
#include "Narrative/IGStoryStateSubsystem.h"
#include "Save/IGSaveGame.h"
#include "Templates/UnrealTemplate.h"

namespace IGSave
{
	constexpr int32 AutosaveSlotCount = 2;
	const TCHAR* AutosaveSlotPrefix = TEXT("AutoSave");
}

bool UIGSaveSubsystem::RequestSave(
	const FString& SlotName,
	const FGameplayTag ChapterId,
	const FName MapPackageName,
	const FGameplayTag CheckpointTag)
{
	return BeginSave(
		SlotName,
		ChapterId,
		MapPackageName,
		CheckpointTag,
		false,
		INDEX_NONE);
}

bool UIGSaveSubsystem::ClearRotatingAutosaves()
{
	// An explicit R/M reset supersedes any coalesced checkpoint that belonged
	// to the old run. If an async write is already in flight, erase it from
	// its completion callback so it cannot win the race and resurrect an end.
	QueuedAutosave = nullptr;
	LastLoadedSave = nullptr;
	bLastLoadedProgressApplied = false;
	if (bLoadInProgress)
	{
		return false;
	}
	if (bSaveInProgress)
	{
		bClearAutosavesAfterActiveSave = true;
		return true;
	}
	return ClearRotatingAutosavesNow();
}

bool UIGSaveSubsystem::ClearRotatingAutosavesNow()
{
	bool bAllSlotsCleared = true;
	for (int32 SlotIndex = 0; SlotIndex < IGSave::AutosaveSlotCount; ++SlotIndex)
	{
		const FString SlotName = FString::Printf(
			TEXT("%s_%d"),
			IGSave::AutosaveSlotPrefix,
			SlotIndex);
		if (UGameplayStatics::DoesSaveGameExist(SlotName, LocalUserIndex))
		{
			if (!UGameplayStatics::DeleteGameInSlot(SlotName, LocalUserIndex))
			{
				bAllSlotsCleared = false;
				UE_LOG(
					LogIndieGame,
					Error,
					TEXT("Failed to delete autosave slot '%s' for local user %d."),
					*SlotName,
					LocalUserIndex);
			}
		}
	}
	if (bAllSlotsCleared)
	{
		NextAutosaveIndex = 0;
	}
	bClearAutosavesAfterActiveSave = false;
	return bAllSlotsCleared;
}

bool UIGSaveSubsystem::BeginSave(
	const FString& SlotName,
	const FGameplayTag ChapterId,
	const FName MapPackageName,
	const FGameplayTag CheckpointTag,
	const bool bIsAutosave,
	const int32 AutosaveIndex,
	UIGSaveGame* PrebuiltSnapshot)
{
	if (IsBusy() || SlotName.IsEmpty())
	{
		return false;
	}

	PendingSave = PrebuiltSnapshot
		? PrebuiltSnapshot
		: CreateSaveSnapshot(ChapterId, MapPackageName, CheckpointTag);
	if (!PendingSave)
	{
		return false;
	}

	bActiveSaveIsAutosave = bIsAutosave;
	ActiveAutosaveIndex = bIsAutosave ? AutosaveIndex : INDEX_NONE;
	bSaveInProgress = true;
	FAsyncSaveGameToSlotDelegate CompletionDelegate;
	CompletionDelegate.BindUObject(this, &ThisClass::HandleSaveComplete);
	UGameplayStatics::AsyncSaveGameToSlot(PendingSave, SlotName, LocalUserIndex, CompletionDelegate);
	return true;
}

UIGSaveGame* UIGSaveSubsystem::CreateSaveSnapshot(
	const FGameplayTag ChapterId,
	const FName MapPackageName,
	const FGameplayTag CheckpointTag) const
{
	UIGSaveGame* Snapshot = Cast<UIGSaveGame>(
		UGameplayStatics::CreateSaveGameObject(UIGSaveGame::StaticClass()));
	if (!Snapshot)
	{
		return nullptr;
	}

	Snapshot->Progress.SchemaVersion = UIGSaveGame::CurrentSchemaVersion;
	Snapshot->Progress.SavedAtUtc = FDateTime::UtcNow();
	Snapshot->Progress.ChapterId = ChapterId;
	Snapshot->Progress.MapPackageName = MapPackageName;
	Snapshot->Progress.CheckpointTag = CheckpointTag;

	if (const UGameInstance* GameInstance = GetGameInstance())
	{
		if (const UIGStoryStateSubsystem* StoryState =
			GameInstance->GetSubsystem<UIGStoryStateSubsystem>())
		{
			Snapshot->Progress.StoryStateTags = StoryState->GetStateSnapshot();
		}
		if (const UIGRebirthNarrativeSubsystem* RebirthState =
			GameInstance->GetSubsystem<UIGRebirthNarrativeSubsystem>())
		{
			Snapshot->Progress.RebirthNarrative =
				RebirthState->BuildSnapshot();
		}
	}

	return Snapshot;
}

bool UIGSaveSubsystem::RequestAutosave(
	const FGameplayTag ChapterId,
	const FName MapPackageName,
	const FGameplayTag CheckpointTag)
{
	if (!ChapterId.IsValid()
		|| MapPackageName.IsNone()
		|| !CheckpointTag.IsValid())
	{
		// The front end only offers navigable checkpoints. Reject malformed
		// writes here as well so a bad caller cannot poison both rotating slots.
		UE_LOG(
			LogIndieGame,
			Error,
			TEXT(
				"Rejected malformed autosave: chapter_valid=%d map='%s' "
				"checkpoint_valid=%d."),
			ChapterId.IsValid() ? 1 : 0,
			*MapPackageName.ToString(),
			CheckpointTag.IsValid() ? 1 : 0);
		return false;
	}
	if (bLoadInProgress || bApplyingLoadedProgress)
	{
		// Never mix a pre-load checkpoint with state that is being replaced by
		// a load, including synchronous tag callbacks during snapshot restore.
		return false;
	}

	if (bSaveInProgress || QueuedAutosave)
	{
		// Coalesce to a complete immutable snapshot captured at request time.
		QueuedAutosave = CreateSaveSnapshot(ChapterId, MapPackageName, CheckpointTag);
		if (!QueuedAutosave)
		{
			return false;
		}

		if (!bSaveInProgress)
		{
			ProcessQueuedAutosave();
		}

		return true;
	}

	const int32 AutosaveIndex = NextAutosaveIndex;
	const FString SlotName = FString::Printf(
		TEXT("%s_%d"),
		IGSave::AutosaveSlotPrefix,
		AutosaveIndex);

	return BeginSave(
		SlotName,
		ChapterId,
		MapPackageName,
		CheckpointTag,
		true,
		AutosaveIndex);
}

bool UIGSaveSubsystem::RequestLoad(const FString& SlotName)
{
	if (IsBusy() || SlotName.IsEmpty())
	{
		return false;
	}

	bLoadInProgress = true;
	LastLoadedSave = nullptr;
	bLastLoadedProgressApplied = false;
	// A queued snapshot was captured from the world that is about to be
	// replaced. It must never win the autosave race after load completion.
	QueuedAutosave = nullptr;

	FAsyncLoadGameFromSlotDelegate CompletionDelegate;
	CompletionDelegate.BindUObject(this, &ThisClass::HandleLoadComplete);
	UGameplayStatics::AsyncLoadGameFromSlot(SlotName, LocalUserIndex, CompletionDelegate);
	return true;
}

bool UIGSaveSubsystem::RequestLoadLatestAutosave()
{
	if (IsBusy())
	{
		return false;
	}

	FString NewestSlot;
	return FindNewestCompatibleAutosave(NewestSlot)
		&& RequestLoad(NewestSlot);
}

bool UIGSaveSubsystem::HasCompatibleAutosave() const
{
	FString NewestSlot;
	return FindNewestCompatibleAutosave(NewestSlot);
}

bool UIGSaveSubsystem::FindNewestCompatibleAutosave(
	FString& OutSlotName) const
{
	OutSlotName.Reset();
	FDateTime NewestTimestamp = FDateTime::MinValue();
	for (int32 AutosaveIndex = 0;
		AutosaveIndex < IGSave::AutosaveSlotCount;
		++AutosaveIndex)
	{
		const FString SlotName = FString::Printf(
			TEXT("%s_%d"),
			IGSave::AutosaveSlotPrefix,
			AutosaveIndex);
		if (!UGameplayStatics::DoesSaveGameExist(SlotName, LocalUserIndex))
		{
			continue;
		}

		const UIGSaveGame* Candidate = Cast<UIGSaveGame>(
			UGameplayStatics::LoadGameFromSlot(SlotName, LocalUserIndex));
		if (IsAutosaveLoadable(Candidate)
			&& (OutSlotName.IsEmpty()
				|| Candidate->Progress.SavedAtUtc > NewestTimestamp))
		{
			OutSlotName = SlotName;
			NewestTimestamp = Candidate->Progress.SavedAtUtc;
		}
	}

	return !OutSlotName.IsEmpty();
}

bool UIGSaveSubsystem::ApplyLoadedProgress()
{
	return ApplyLoadedProgressInternal(true);
}

bool UIGSaveSubsystem::ApplyLoadedProgressInternal(
	const bool bBroadcastStoryChanges)
{
	if (bLastLoadedProgressApplied)
	{
		return true;
	}
	if (!IsSaveCompatible(LastLoadedSave))
	{
		return false;
	}

	TGuardValue<bool> ApplyingGuard(bApplyingLoadedProgress, true);
	bool bApplied = false;
	// Restore the authoritative REBIRTH snapshot before replaying legacy story
	// tags. RestoreStateSnapshot broadcasts tag changes synchronously, and the
	// chapter directors use those callbacks to rebuild receipts, carried props,
	// C3 gates and ending presentation. Reversing this order exposes the
	// pre-load narrative state for one frame and can permanently reconcile the
	// wrong A/B/C profile or return gate.
	if (UIGRebirthNarrativeSubsystem* RebirthState =
		GetGameInstance()->GetSubsystem<UIGRebirthNarrativeSubsystem>())
	{
		if (LastLoadedSave->Progress.SchemaVersion >= 2)
		{
			RebirthState->RestoreSnapshot(
				LastLoadedSave->Progress.RebirthNarrative);
		}
		else
		{
			RebirthState->ResetNarrative();
		}
		bApplied = true;
	}
	if (UIGStoryStateSubsystem* StoryState =
		GetGameInstance()->GetSubsystem<UIGStoryStateSubsystem>())
	{
		StoryState->RestoreStateSnapshot(
			LastLoadedSave->Progress.StoryStateTags,
			bBroadcastStoryChanges);
		bApplied = true;
	}

	bLastLoadedProgressApplied = bApplied;
	return bApplied;
}

void UIGSaveSubsystem::HandleSaveComplete(
	const FString& SlotName,
	const int32 UserIndex,
	const bool bSuccess)
{
	bSaveInProgress = false;
	PendingSave = nullptr;
	if (!bSuccess)
	{
		UE_LOG(
			LogIndieGame,
			Error,
			TEXT("Save operation failed for slot '%s' (user %d)."),
			*SlotName,
			UserIndex);
	}
	if (bSuccess && bActiveSaveIsAutosave && ActiveAutosaveIndex != INDEX_NONE)
	{
		NextAutosaveIndex = (ActiveAutosaveIndex + 1) % IGSave::AutosaveSlotCount;
	}

	bActiveSaveIsAutosave = false;
	ActiveAutosaveIndex = INDEX_NONE;
	if (bClearAutosavesAfterActiveSave)
	{
		if (!ClearRotatingAutosavesNow())
		{
			UE_LOG(
				LogIndieGame,
				Error,
				TEXT("Deferred rotating autosave cleanup failed."));
		}
	}
	ProcessQueuedAutosave();
	OnSaveCompleted.Broadcast(bSuccess, SlotName);
}

void UIGSaveSubsystem::HandleLoadComplete(
	const FString& SlotName,
	const int32 UserIndex,
	USaveGame* LoadedObject)
{
	bLoadInProgress = false;
	UIGSaveGame* TypedSave = Cast<UIGSaveGame>(LoadedObject);
	const bool bLoaded = IsSaveCompatible(TypedSave);
	LastLoadedSave = bLoaded ? TypedSave : nullptr;
	bLastLoadedProgressApplied = false;

	const FName SavedMap = bLoaded
		? LastLoadedSave->Progress.MapPackageName
		: NAME_None;
	const bool bWillTravel = !SavedMap.IsNone();
	const bool bApplied = bLoaded
		&& ApplyLoadedProgressInternal(!bWillTravel);
	const bool bSuccess = bLoaded && bApplied;
	if (!bSuccess)
	{
		UE_LOG(
			LogIndieGame,
			Error,
			TEXT(
				"Load operation failed for slot '%s' (user %d, compatible=%d, "
				"applied=%d)."),
			*SlotName,
			UserIndex,
			bLoaded ? 1 : 0,
			bApplied ? 1 : 0);
	}
	OnLoadCompleted.Broadcast(bSuccess, SlotName, LastLoadedSave);

	if (bSuccess && bWillTravel)
	{
		const FGameplayTag ChapterTwo = FGameplayTag::RequestGameplayTag(
			FName(TEXT("Chapter.CH02")),
			false);
		const FGameplayTag ChapterThree = FGameplayTag::RequestGameplayTag(
			FName(TEXT("Chapter.CH03")),
			false);
		const FGameplayTag SavedChapter = LastLoadedSave->Progress.ChapterId;
		const FString TravelOptions =
			SavedChapter.MatchesTagExact(ChapterThree)
				? TEXT("IGChapterThree=1?IGResumeSave=1")
				: SavedChapter.MatchesTagExact(ChapterTwo)
					? TEXT("IGChapterTwo=1?IGResumeSave=1")
					: TEXT("IGIgnoreDirectStart=1?IGResumeSave=1");
		UGameplayStatics::OpenLevel(
			this,
			SavedMap,
			true,
			TravelOptions);
	}
	ProcessQueuedAutosave();
}

void UIGSaveSubsystem::ProcessQueuedAutosave()
{
	if (!QueuedAutosave || IsBusy())
	{
		return;
	}

	UIGSaveGame* Snapshot = QueuedAutosave;
	QueuedAutosave = nullptr;
	const int32 AutosaveIndex = NextAutosaveIndex;
	const FString SlotName = FString::Printf(
		TEXT("%s_%d"),
		IGSave::AutosaveSlotPrefix,
		AutosaveIndex);

	BeginSave(
		SlotName,
		Snapshot->Progress.ChapterId,
		Snapshot->Progress.MapPackageName,
		Snapshot->Progress.CheckpointTag,
		true,
		AutosaveIndex,
		Snapshot);
}

bool UIGSaveSubsystem::IsSaveCompatible(const UIGSaveGame* SaveGame) const
{
	return SaveGame
		&& SaveGame->Progress.SchemaVersion > 0
		&& SaveGame->Progress.SchemaVersion <= UIGSaveGame::CurrentSchemaVersion;
}

bool UIGSaveSubsystem::IsAutosaveLoadable(const UIGSaveGame* SaveGame) const
{
	return IsSaveCompatible(SaveGame)
		&& SaveGame->Progress.ChapterId.IsValid()
		&& !SaveGame->Progress.MapPackageName.IsNone()
		&& SaveGame->Progress.CheckpointTag.IsValid();
}
