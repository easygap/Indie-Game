#include "Narrative/IGStoryStateSubsystem.h"

bool UIGStoryStateSubsystem::HasState(const FGameplayTag StateTag, const bool bExactMatch) const
{
	if (!StateTag.IsValid())
	{
		return false;
	}

	return bExactMatch ? StateTags.HasTagExact(StateTag) : StateTags.HasTag(StateTag);
}

bool UIGStoryStateSubsystem::HasAllStates(
	const FGameplayTagContainer& RequiredStates,
	const bool bExactMatch) const
{
	return bExactMatch ? StateTags.HasAllExact(RequiredStates) : StateTags.HasAll(RequiredStates);
}

bool UIGStoryStateSubsystem::AddState(const FGameplayTag StateTag)
{
	if (!StateTag.IsValid())
	{
		return false;
	}

	if (bDispatchingStateChanges)
	{
		return QueueMutation(StateTag, true);
	}

	if (StateTags.HasTagExact(StateTag))
	{
		return false;
	}

	StateTags.AddTag(StateTag);
	DispatchStateChanges({{StateTag, true}});
	return true;
}

bool UIGStoryStateSubsystem::RemoveState(const FGameplayTag StateTag)
{
	if (!StateTag.IsValid())
	{
		return false;
	}

	if (bDispatchingStateChanges)
	{
		return QueueMutation(StateTag, false);
	}

	if (!StateTags.HasTagExact(StateTag))
	{
		return false;
	}

	StateTags.RemoveTag(StateTag);
	DispatchStateChanges({{StateTag, false}});
	return true;
}

void UIGStoryStateSubsystem::ClearStates(const bool bBroadcastChanges)
{
	RestoreStateSnapshot(FGameplayTagContainer(), bBroadcastChanges);
}

void UIGStoryStateSubsystem::RestoreStateSnapshot(
	const FGameplayTagContainer& Snapshot,
	const bool bBroadcastChanges)
{
	if (bDispatchingStateChanges)
	{
		if (!bBroadcastChanges)
		{
			PendingMutations.Reset();
			DeferredSilentSnapshot = Snapshot;
			bDeferredSilentRestore = true;
			return;
		}

		// A restore requested by a listener becomes the final desired state for this batch.
		bDeferredSilentRestore = false;
		DeferredSilentSnapshot.Reset();
		PendingMutations.Reset();
		TArray<FGameplayTag> CurrentTags;
		TArray<FGameplayTag> SnapshotTags;
		StateTags.GetGameplayTagArray(CurrentTags);
		Snapshot.GetGameplayTagArray(SnapshotTags);

		for (const FGameplayTag& CurrentTag : CurrentTags)
		{
			if (!Snapshot.HasTagExact(CurrentTag))
			{
				PendingMutations.Add({CurrentTag, false});
			}
		}

		for (const FGameplayTag& SnapshotTag : SnapshotTags)
		{
			if (!StateTags.HasTagExact(SnapshotTag))
			{
				PendingMutations.Add({SnapshotTag, true});
			}
		}
		return;
	}

	if (!bBroadcastChanges)
	{
		StateTags = Snapshot;
		return;
	}

	TArray<FGameplayTag> PreviousTags;
	TArray<FGameplayTag> NewTags;
	TArray<FIGStoryStateMutation> Changes;
	StateTags.GetGameplayTagArray(PreviousTags);
	Snapshot.GetGameplayTagArray(NewTags);

	for (const FGameplayTag& PreviousTag : PreviousTags)
	{
		if (!Snapshot.HasTagExact(PreviousTag))
		{
			Changes.Add({PreviousTag, false});
		}
	}

	for (const FGameplayTag& NewTag : NewTags)
	{
		if (!PreviousTags.Contains(NewTag))
		{
			Changes.Add({NewTag, true});
		}
	}

	StateTags = Snapshot;
	DispatchStateChanges(MoveTemp(Changes));
}

bool UIGStoryStateSubsystem::QueueMutation(const FGameplayTag StateTag, const bool bAdd)
{
	if (bDeferredSilentRestore)
	{
		const bool bCurrentlyAdded = DeferredSilentSnapshot.HasTagExact(StateTag);
		if (bCurrentlyAdded == bAdd)
		{
			return false;
		}

		if (bAdd)
		{
			DeferredSilentSnapshot.AddTag(StateTag);
		}
		else
		{
			DeferredSilentSnapshot.RemoveTag(StateTag);
		}
		return true;
	}

	const int32 ExistingIndex = PendingMutations.IndexOfByPredicate(
		[StateTag](const FIGStoryStateMutation& Mutation)
		{
			return Mutation.StateTag.MatchesTagExact(StateTag);
		});
	const bool bEffectiveState = ExistingIndex != INDEX_NONE
		? PendingMutations[ExistingIndex].bAdd
		: StateTags.HasTagExact(StateTag);

	if (bEffectiveState == bAdd)
	{
		return false;
	}

	if (ExistingIndex != INDEX_NONE)
	{
		PendingMutations.RemoveAt(ExistingIndex);
	}

	if (StateTags.HasTagExact(StateTag) != bAdd)
	{
		PendingMutations.Add({StateTag, bAdd});
	}

	return true;
}

void UIGStoryStateSubsystem::DispatchStateChanges(TArray<FIGStoryStateMutation> InitialChanges)
{
	if (InitialChanges.IsEmpty())
	{
		return;
	}

	check(!bDispatchingStateChanges);
	bDispatchingStateChanges = true;

	TArray<FIGStoryStateMutation> Changes = MoveTemp(InitialChanges);
	constexpr int32 MaxDispatchPasses = 64;
	int32 DispatchPass = 0;

	while (!Changes.IsEmpty() && DispatchPass++ < MaxDispatchPasses)
	{
		for (const FIGStoryStateMutation& Change : Changes)
		{
			OnStoryStateTagChanged.Broadcast(Change.StateTag, Change.bAdd);
		}

		Changes.Reset();
		if (bDeferredSilentRestore)
		{
			StateTags = DeferredSilentSnapshot;
			DeferredSilentSnapshot.Reset();
			bDeferredSilentRestore = false;
			PendingMutations.Reset();
			continue;
		}

		TArray<FIGStoryStateMutation> Mutations = MoveTemp(PendingMutations);
		PendingMutations.Reset();

		for (const FIGStoryStateMutation& Mutation : Mutations)
		{
			const bool bCurrentlyAdded = StateTags.HasTagExact(Mutation.StateTag);
			if (bCurrentlyAdded == Mutation.bAdd)
			{
				continue;
			}

			if (Mutation.bAdd)
			{
				StateTags.AddTag(Mutation.StateTag);
			}
			else
			{
				StateTags.RemoveTag(Mutation.StateTag);
			}

			Changes.Add(Mutation);
		}
	}

	bDispatchingStateChanges = false;
	ensureMsgf(
		Changes.IsEmpty() && PendingMutations.IsEmpty(),
		TEXT("Story state listeners exceeded the safe re-entrant mutation limit."));
	PendingMutations.Reset();
}
