#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "IGStoryStateSubsystem.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
	FIGStoryStateTagChangedSignature,
	FGameplayTag, StateTag,
	bool, bAdded);

struct FIGStoryStateMutation
{
	FGameplayTag StateTag;
	bool bAdd = false;
};

/** Persistent, map-independent story facts owned by the game instance. */
UCLASS()
class INDIEGAME_API UIGStoryStateSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintPure, Category = "Story|State")
	bool HasState(FGameplayTag StateTag, bool bExactMatch = true) const;

	UFUNCTION(BlueprintPure, Category = "Story|State")
	bool HasAllStates(const FGameplayTagContainer& RequiredStates, bool bExactMatch = true) const;

	UFUNCTION(BlueprintCallable, Category = "Story|State")
	bool AddState(FGameplayTag StateTag);

	UFUNCTION(BlueprintCallable, Category = "Story|State")
	bool RemoveState(FGameplayTag StateTag);

	UFUNCTION(BlueprintCallable, Category = "Story|State")
	void ClearStates(bool bBroadcastChanges = true);

	/** Replaces all state, normally after a save is loaded. */
	UFUNCTION(BlueprintCallable, Category = "Story|State")
	void RestoreStateSnapshot(const FGameplayTagContainer& Snapshot, bool bBroadcastChanges = true);

	UFUNCTION(BlueprintPure, Category = "Story|State")
	FGameplayTagContainer GetStateSnapshot() const { return StateTags; }

	UPROPERTY(BlueprintAssignable, Category = "Story|State")
	FIGStoryStateTagChangedSignature OnStoryStateTagChanged;

private:
	bool QueueMutation(FGameplayTag StateTag, bool bAdd);
	void DispatchStateChanges(TArray<FIGStoryStateMutation> InitialChanges);

	UPROPERTY(VisibleAnywhere, Category = "Story|State")
	FGameplayTagContainer StateTags;

	TArray<FIGStoryStateMutation> PendingMutations;
	FGameplayTagContainer DeferredSilentSnapshot;
	bool bDispatchingStateChanges = false;
	bool bDeferredSilentRestore = false;
};
