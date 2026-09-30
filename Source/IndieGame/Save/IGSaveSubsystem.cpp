#include "Save/IGSaveSubsystem.h"

#include "IndieGame.h"
#include "Engine/GameInstance.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/PackageName.h"
#include "Narrative/IGMissingFloorNarrativeSubsystem.h"
#include "Narrative/IGStoryStateSubsystem.h"
#include "Save/IGSaveGame.h"
#include "Templates/UnrealTemplate.h"

namespace IGSave
{
	constexpr int32 AutosaveSlotCount = 2;
	const TCHAR* AutosaveSlotPrefix = TEXT("AutoSave");
	// 이 빌드에서 진행을 복원하는 맵은 하나다. 다른 에셋이나 예전 맵을
	// 체크포인트로 받으면 불러오기에 성공한 뒤 맵 이동에서 멎는다.
	const FName PlayableMap(TEXT("/Game/Maps/Prologue_Morning"));

	/**
	 * 없는 층에서 쓴 저장인지. 없는 층의 자동 저장은 모두 이 챕터 태그를
	 * 달지만, 서사가 한 발이라도 나아간 저장도 같이 쳐 준다.
	 */
	static bool IsMissingFloorSave(const UIGSaveGame& SaveGame)
	{
		const FGameplayTag MissingFloor = FGameplayTag::RequestGameplayTag(
			FName(TEXT("Chapter.MissingFloor")),
			false);
		const FIGMissingFloorNarrativeSnapshot& MissingFloorSnapshot =
			SaveGame.Progress.MissingFloorNarrative;
		return SaveGame.Progress.ChapterId.MatchesTagExact(MissingFloor)
			|| MissingFloorSnapshot.Night.NightIndex > 0
			|| MissingFloorSnapshot.Night.CompletedBeats.Num() > 0
			|| MissingFloorSnapshot.Truths.Num() > 0;
	}
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
		if (const UIGMissingFloorNarrativeSubsystem* MissingFloorState =
			GameInstance->GetSubsystem<UIGMissingFloorNarrativeSubsystem>())
		{
			Snapshot->Progress.MissingFloorNarrative =
				MissingFloorState->GetSnapshot();
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

bool UIGSaveSubsystem::HasEndingBAutosave() const
{
	FString NewestSlot;
	if (!FindNewestCompatibleAutosave(NewestSlot))
	{
		return false;
	}
	const UIGSaveGame* Newest = Cast<UIGSaveGame>(
		UGameplayStatics::LoadGameFromSlot(NewestSlot, LocalUserIndex));
	return Newest
		&& Newest->Progress.MissingFloorNarrative.Night.EndingChoice
			== FName(TEXT("Ending.B"));
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
	if (!IsAutosaveLoadable(LastLoadedSave))
	{
		return false;
	}

	TGuardValue<bool> ApplyingGuard(bApplyingLoadedProgress, true);
	bool bApplied = false;
	// 서사 스냅샷을 스토리 태그보다 먼저 되돌린다. RestoreStateSnapshot은 태그
	// 변화를 그 자리에서 알리고, 밤 디렉터는 그 콜백 안에서 게이트를, 월드
	// 씬은 고른 물의 봉투를 다시 맞춘다. 순서를 뒤집으면 불러오기 전 상태로
	// 한 번 맞춰 버린다.
	if (UIGMissingFloorNarrativeSubsystem* MissingFloorState =
		GetGameInstance()->GetSubsystem<UIGMissingFloorNarrativeSubsystem>())
	{
		MissingFloorState->RestoreSnapshot(
			LastLoadedSave->Progress.MissingFloorNarrative);
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
	// 슬롯 이름을 직접 지정한 불러오기도 자동 선택과 같은 기준을 적용한다.
	const bool bLoaded = IsAutosaveLoadable(TypedSave);
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
		// 없는 층이 아닌 저장은 이어하기 목록에 오르지 않는다. 슬롯을 이름으로
		// 직접 불러온 경우에만 뒤쪽 갈래를 타고, 그때는 맵만 다시 연다.
		const bool bIsMissingFloorSave =
			IGSave::IsMissingFloorSave(*LastLoadedSave);
		const FString TravelOptions =
			bIsMissingFloorSave
				? TEXT("IGMissingFloor=1?IGResumeSave=1")
				: TEXT("IGResumeSave=1");
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
	// 예전 이야기(CH01~CH03)의 자동 저장은 돌아갈 장면이 없다. 목록에서 조용히
	// 빼서 타이틀이 이어하기를 권하지 않게 한다.
	return IsSaveCompatible(SaveGame)
		&& SaveGame->Progress.ChapterId.IsValid()
		&& !SaveGame->Progress.MapPackageName.IsNone()
		&& SaveGame->Progress.MapPackageName == IGSave::PlayableMap
		&& FPackageName::DoesPackageExist(SaveGame->Progress.MapPackageName.ToString())
		&& SaveGame->Progress.CheckpointTag.IsValid()
		&& IGSave::IsMissingFloorSave(*SaveGame);
}
