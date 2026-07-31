#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GameplayTagContainer.h"
#include "Sequence/IGObjectiveProvider.h"
#include "IGMorningRoutineDirector.generated.h"

class UIGChapterOnePresenceAudioComponent;
class AIGZoneTrigger;
class UPointLightComponent;

/** Objective phases of the prologue morning, derived purely from story state. */
UENUM(BlueprintType)
enum class EIGMorningPhase : uint8
{
	/** Wake-up flow still owns the player. */
	Inactive,
	CheckFridge,
	TakeWallet,
	GoToStore,
	FindWater,
	PayAtCounter,
	Complete
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
	FIGMorningPhaseChangedSignature,
	EIGMorningPhase, PreviousPhase,
	EIGMorningPhase, NewPhase);

/**
 * Post-wake objective coordinator for the morning routine:
 * fridge reveal -> apartment exit -> alley -> store -> water -> checkout.
 * The desk wallet is optional route dressing, never a progression gate.
 *
 * The phase is a pure function of persistent story tags, so restores are
 * automatically consistent. CH01 deliberately establishes normal life:
 * objectives and checkpoint saves plus one plausible streetlight failure.
 * Every stronger audiovisual anomaly belongs to a later loop.
 */
UCLASS(Blueprintable)
class INDIEGAME_API AIGMorningRoutineDirector
	: public AActor
	, public IIGObjectiveProvider
{
	GENERATED_BODY()

public:
	AIGMorningRoutineDirector();

	/** The one CH01 anomaly: a failing alley streetlight. */
	void SetSceneReferences(
		UPointLightComponent* InFlickerLight,
		AIGZoneTrigger* InApartmentExitZone);

	/** One-shot alley streetlight failure; wired to a zone trigger. */
	UFUNCTION(BlueprintCallable, Category = "Morning Flow|Beats")
	void TriggerAlleyLightFailure();

	UFUNCTION(BlueprintPure, Category = "Morning Flow")
	EIGMorningPhase GetPhase() const { return Phase; }

	UFUNCTION(BlueprintPure, Category = "Morning Flow")
	bool IsActive() const { return Phase != EIGMorningPhase::Inactive; }

	/** Current objective line for the HUD; empty while inactive. */
	UFUNCTION(BlueprintPure, Category = "Morning Flow")
	virtual FText GetObjectiveText() const override;

	/** ASCII fallback objective used when no Korean-capable font is available. */
	UFUNCTION(BlueprintPure, Category = "Morning Flow")
	virtual FString GetObjectiveTextAscii() const override;

	/** Normalized progress through the six morning-routine phases. */
	UFUNCTION(BlueprintPure, Category = "Morning Flow")
	virtual float GetObjectiveProgress() const override;

	UPROPERTY(BlueprintAssignable, Category = "Morning Flow")
	FIGMorningPhaseChangedSignature OnPhaseChanged;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Morning Flow|Save")
	FGameplayTag ChapterId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Morning Flow|Save")
	bool bAutosaveAtCheckpoints = true;

private:
	UFUNCTION()
	void HandleStoryStateChanged(FGameplayTag StateTag, bool bAdded);

	UFUNCTION()
	void HandleApartmentExitZoneTriggered(AIGZoneTrigger* Zone);

	void ResolveDefaultTags();
	EIGMorningPhase EvaluatePhaseFromState() const;
	void RefreshPhase(bool bLiveTransition);
	void BootstrapRebirthStateFromLegacy();
	void BridgeRebirthState(const FGameplayTag& StateTag);
	void RestoreOutfitFromCanonical(bool bAllowLegacyMigration);
	void CommitOutfitAtFirstExit();
	void HandleLiveStateSideEffects(const FGameplayTag& StateTag);
	void RequestCheckpointAutosave(const FGameplayTag& CheckpointTag);
	void EnablePlayerCameraMotion();

	void HandleFlickerStep();

	// Story states consumed to derive the phase.
	FGameplayTag StandingStateTag;
	FGameplayTag FridgeCheckedStateTag;
	FGameplayTag HasWalletStateTag;
	FGameplayTag LeftApartmentStateTag;
	FGameplayTag LeftHomeStateTag;
	FGameplayTag EnteredStoreStateTag;
	FGameplayTag HasWaterStateTag;
	FGameplayTag WaterPurchasedStateTag;

	// Checkpoints written as the routine advances.
	FGameplayTag FridgeCheckpointTag;
	FGameplayTag AlleyCheckpointTag;
	FGameplayTag StoreCheckpointTag;
	FGameplayTag PurchasedCheckpointTag;

	UPROPERTY(Transient)
	TObjectPtr<UPointLightComponent> FlickerLight;

	UPROPERTY(Transient)
	TObjectPtr<AIGZoneTrigger> ApartmentExitZone;

	/** Dies with this CH01-only director when the second morning begins. */
	UPROPERTY(VisibleAnywhere, Category = "Morning Flow|Audio")
	TObjectPtr<UIGChapterOnePresenceAudioComponent> PresenceAudio;

	FTimerHandle FlickerTimerHandle;

	EIGMorningPhase Phase = EIGMorningPhase::Inactive;
	float FlickerBaseIntensity = 0.0f;
	int32 FlickerStepIndex = 0;
	bool bFlickerConsumed = false;
};
