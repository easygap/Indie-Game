#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GameplayTagContainer.h"
#include "Sequence/IGObjectiveProvider.h"
#include "IGSecondMorningDirector.generated.h"

class AIGElevator;
class AIGPrologueWorldScene;
class AIGReadableNote;
class AIGSlidingDoor;
class AIGSwingDoor;
class UAudioComponent;
class UPointLightComponent;

/**
 * Coordinates CH02 without owning level geometry.
 *
 * The first and second morning deliberately reuse one world. This director
 * consumes only story tags and a narrow set of scene verbs, which keeps the
 * loop cheap to reset and lets later chapters replace the presentation
 * without duplicating the apartment, lobby, alley or store.
 */
UCLASS(NotBlueprintable, Transient)
class INDIEGAME_API AIGSecondMorningDirector final
	: public AActor
	, public IIGObjectiveProvider
{
	GENERATED_BODY()

public:
	AIGSecondMorningDirector();

	void Configure(
		AIGPrologueWorldScene* InScene,
		AIGSwingDoor* InMirrorRoomDoor,
		UPointLightComponent* InMirrorRoomLamp,
		AIGElevator* InElevator,
		AIGSlidingDoor* InStoreDoor,
		UAudioComponent* InJingleComponent,
		AIGReadableNote* InExistingReceipt,
		AIGReadableNote* InDuplicateReceipt,
		AIGReadableNote* InMailboxBills,
		AIGReadableNote* InSaltMemo,
		AIGReadableNote* InNightRoster,
		AIGReadableNote* InManagementNotice);

	virtual FText GetObjectiveText() const override;
	virtual FString GetObjectiveTextAscii() const override;
	virtual float GetObjectiveProgress() const override;

	UFUNCTION(BlueprintPure, Category = "Second Morning")
	bool IsActive() const;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	void ResolveTags();
	void BindNote(AIGReadableNote* Note);
	bool HasState(const FGameplayTag& Tag) const;
	void AddState(const FGameplayTag& Tag) const;
	void RefreshObjective();
	void StartCorridorBlackout();
	void StartMirrorRoomBeat();
	void StartStoreBeat();
	void StartEndingBeat();
	void HandleDirectorStep();
	void PlayCorridorSnap(const FVector& Location, float ScareAmount);
	void SetThreat(float Pressure, float DurationSeconds);
	void ClearThreat();
	void CloseMirrorDoor();
	void KillMirrorLampAndLock();
	void RevealLiftFootprints();
	void ResumeLift();
	void PlaySecondStoreChime();
	void ApplyThreatPressure(float Pressure) const;

	UFUNCTION()
	void HandleStoryStateChanged(FGameplayTag StateTag, bool bAdded);

	UFUNCTION()
	void HandleIntermediateStopOpened(AIGElevator* StoppedElevator);

	UFUNCTION()
	void HandleNoteRead(AIGReadableNote* Note, bool bOpened);

	UPROPERTY(Transient)
	TObjectPtr<AIGPrologueWorldScene> Scene;

	UPROPERTY(Transient)
	TObjectPtr<AIGSwingDoor> MirrorRoomDoor;

	UPROPERTY(Transient)
	TObjectPtr<UPointLightComponent> MirrorRoomLamp;

	UPROPERTY(Transient)
	TObjectPtr<AIGElevator> Elevator;

	UPROPERTY(Transient)
	TObjectPtr<AIGSlidingDoor> StoreDoor;

	UPROPERTY(Transient)
	TObjectPtr<UAudioComponent> JingleComponent;

	UPROPERTY(Transient)
	TObjectPtr<AIGReadableNote> ExistingReceipt;

	UPROPERTY(Transient)
	TObjectPtr<AIGReadableNote> DuplicateReceipt;

	UPROPERTY(Transient)
	TObjectPtr<AIGReadableNote> MailboxBills;

	UPROPERTY(Transient)
	TObjectPtr<AIGReadableNote> SaltMemo;

	UPROPERTY(Transient)
	TObjectPtr<AIGReadableNote> NightRoster;

	UPROPERTY(Transient)
	TObjectPtr<AIGReadableNote> ManagementNotice;

	FGameplayTag StartedTag;
	FGameplayTag WakeAlarmStoppedTag;
	FGameplayTag FridgeCheckedTag;
	FGameplayTag HasWalletTag;
	FGameplayTag LeftHomeTag;
	FGameplayTag CorridorDarkTag;
	FGameplayTag SawMirrorRoomTag;
	FGameplayTag EnteredMirrorRoomTag;
	FGameplayTag LiftStoppedTag;
	FGameplayTag ReadNoticeTag;
	FGameplayTag EnteredStoreTag;
	FGameplayTag SawReceiptTag;
	FGameplayTag HasWaterTag;
	FGameplayTag WaterPurchasedTag;
	FGameplayTag ReturnedTag;

	FTimerHandle DirectorStepHandle;
	FTimerHandle ThreatHandle;
	FTimerHandle MirrorDoorHandle;
	FTimerHandle MirrorLampHandle;
	FTimerHandle LiftRevealHandle;
	FTimerHandle LiftResumeHandle;
	FTimerHandle StoreChimeHandle;

	double NextBlackoutAllowedTime = 0.0;
	double ThreatEndTime = 0.0;
	float ActiveThreatPressure = 0.0f;
	int32 NextCorridorFixture = INDEX_NONE;
	bool bMirrorBeatConsumed = false;
	bool bStoreBeatConsumed = false;
	bool bEndingConsumed = false;
};
