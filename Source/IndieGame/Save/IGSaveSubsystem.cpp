#include "Save/IGSaveSubsystem.h"

#include "Engine/GameInstance.h"
#include "Kismet/GameplayStatics.h"
#include "Narrative/IGStoryStateSubsystem.h"
#include "Save/IGSaveGame.h"

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
	}

	return Snapshot;
}

bool UIGSaveSubsystem::RequestAutosave(
	const FGameplayTag ChapterId,
	const FName MapPackageName,
	const FGameplayTag CheckpointTag)
{
	if (bLoadInProgress)
	{
		// Never mix a pre-load checkpoint with state that is being replaced by a load.
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

	FAsyncLoadGameFromSlotDelegate CompletionDelegate;
	CompletionDelegate.BindUObject(this, &ThisClass::HandleLoadComplete);
	UGameplayStatics::AsyncLoadGameFromSlot(SlotName, LocalUserIndex, CompletionDelegate);
	return true;
}

bool UIGSaveSubsystem::ApplyLoadedProgress()
{
	if (!IsSaveCompatible(LastLoadedSave))
	{
		return false;
	}

	if (UIGStoryStateSubsystem* StoryState =
		GetGameInstance()->GetSubsystem<UIGStoryStateSubsystem>())
	{
		StoryState->RestoreStateSnapshot(LastLoadedSave->Progress.StoryStateTags);
		return true;
	}

	return false;
}

void UIGSaveSubsystem::HandleSaveComplete(
	const FString& SlotName,
	const int32 UserIndex,
	const bool bSuccess)
{
	bSaveInProgress = false;
	PendingSave = nullptr;
	if (bSuccess && bActiveSaveIsAutosave && ActiveAutosaveIndex != INDEX_NONE)
	{
		NextAutosaveIndex = (ActiveAutosaveIndex + 1) % IGSave::AutosaveSlotCount;
	}

	bActiveSaveIsAutosave = false;
	ActiveAutosaveIndex = INDEX_NONE;
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
	const bool bSuccess = IsSaveCompatible(TypedSave);
	LastLoadedSave = bSuccess ? TypedSave : nullptr;
	OnLoadCompleted.Broadcast(bSuccess, SlotName, LastLoadedSave);
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
