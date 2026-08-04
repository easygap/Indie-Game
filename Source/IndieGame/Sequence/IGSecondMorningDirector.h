#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GameplayTagContainer.h"
#include "Sequence/IGObjectiveProvider.h"
#include "IGSecondMorningDirector.generated.h"

class AIGElevator;
class AIGChapterTwoHumanGateDirector;
class AIGPrologueWorldScene;
class AIGReadableNote;
class AIGSlidingDoor;
class AIGSwingDoor;
class AIGTimeEntryPuzzle;
class UAudioComponent;
class UMaterialInterface;
class UPointLightComponent;
class USceneComponent;
class UStaticMesh;
class UStaticMeshComponent;

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
		AIGChapterTwoHumanGateDirector* InHumanGateDirector,
		AIGSwingDoor* InMirrorRoomDoor,
		UPointLightComponent* InMirrorRoomLamp,
		AIGElevator* InElevator,
		AIGSlidingDoor* InStoreDoor,
		UAudioComponent* InJingleComponent,
		AIGReadableNote* InExistingReceipt,
		AIGReadableNote* InDuplicateReceipt,
		AIGReadableNote* InHomePlanner,
		AIGReadableNote* InMirrorAlarmMemo,
		AIGReadableNote* InMailboxBills,
		AIGReadableNote* InOfferingNote,
		AIGReadableNote* InNightRoster,
		AIGReadableNote* InManagementNotice,
		UStaticMesh* InCubeMesh,
		UStaticMesh* InAlarmHousingMesh,
		UStaticMesh* InPosHousingMesh,
		UMaterialInterface* InBodyMaterial,
		UMaterialInterface* InDisplayOffMaterial,
		UMaterialInterface* InDisplayGlassMaterial,
		UMaterialInterface* InAlarmDisplayOnMaterial,
		UMaterialInterface* InPosDisplayOnMaterial,
		UMaterialInterface* InButtonMaterial);

	void HandleTimeEntrySelectionChanged(AIGTimeEntryPuzzle* Puzzle);
	void HandleTimeEntryConfirmed(
		AIGTimeEntryPuzzle* Puzzle,
		bool bCorrect);
	/** Reveals the next hint rung for the currently approached CH02 puzzle. */
	bool RequestManualHint();

	virtual FText GetObjectiveText() const override;
	virtual FString GetObjectiveTextAscii() const override;
	virtual float GetObjectiveProgress() const override;

	UFUNCTION(BlueprintPure, Category = "Second Morning")
	bool IsActive() const;

	/**
	 * Drives one unattended CH02 path through the production story-tag and
	 * note handlers, then emits the same physical-return state used by the
	 * fourth-floor volume. Used only by the release end-to-end route.
	 */
	bool RunRebirthEndToEndRoute(
		bool bP2First = false,
		bool bSkipP1 = false,
		bool bSkipP2 = false);
	int32 GetTimeEntryPhysicalContractCount() const;
	int32 GetTimeEntryMeshComponentCount() const;
	int32 GetAuthoredTimeEntryHousingCount() const;
	int32 GetLayeredTimeEntryDisplayCount() const;

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
	void KillMirrorLampAndPlayKeypad();
	void RevealLiftFootprints();
	void ResumeLift();
	void PlaySecondStoreChime();
	void ApplyThreatPressure(float Pressure) const;
	void RegisterSecondMorningTruth(
		const TCHAR* TruthTagName,
		FName SourceId,
		FName PuzzleId = NAME_None) const;
	void RegisterDeathOverlayTruth() const;
	int32 GetSecondMorningTruthCount() const;
	bool CanConvergeSecondMorning() const;
	bool RunP1EndToEndStep();
	bool RunP2EndToEndStep();
	void RefreshReturnGate() const;
	void RestoreOutfitFromCanonical(bool bAllowLegacyMigration);
	void CommitOutfitAtFirstExit();
	void RequestCheckpointAutosave(const FGameplayTag& CheckpointTag) const;
	void SpawnTimeEntryPuzzles(
		UStaticMesh* CubeMesh,
		UStaticMesh* AlarmHousingMesh,
		UStaticMesh* PosHousingMesh,
		UMaterialInterface* BodyMaterial,
		UMaterialInterface* DisplayOffMaterial,
		UMaterialInterface* DisplayGlassMaterial,
		UMaterialInterface* AlarmDisplayOnMaterial,
		UMaterialInterface* PosDisplayOnMaterial,
		UMaterialInterface* ButtonMaterial);
	void RestoreTimeEntryPuzzles();
	void RefreshTimeEntryAvailability();
	void PersistTimeEntryState(const AIGTimeEntryPuzzle& Puzzle) const;
	void ClearTimeEntryStateTags(const AIGTimeEntryPuzzle& Puzzle) const;
	bool IsPuzzleResolved(FName PuzzleId) const;
	void ApplyP1PressureStage(int32 PressureStage);
	void ApplyP1SolvedLight();
	void RestoreP1LightAfterSolve();
	void SetP2ShutterStage(int32 PressureStage);
	void BeginP2ShutterRise();
	void AnimateP2ShutterRise();
	void PollPuzzlePressure();
	void RaiseTimedPuzzlePressure(AIGTimeEntryPuzzle* Puzzle, bool bP1);
	void PresentP1ManualHint();
	void PresentP2ManualHint();
	void RemoveState(const FGameplayTag& Tag) const;

	UFUNCTION()
	void HandleStoryStateChanged(FGameplayTag StateTag, bool bAdded);

	UFUNCTION()
	void HandleIntermediateStopOpened(AIGElevator* StoppedElevator);

	UFUNCTION()
	void HandleNoteRead(AIGReadableNote* Note, bool bOpened);

	UPROPERTY(VisibleAnywhere, Category = "Second Morning")
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(VisibleAnywhere, Category = "Second Morning|P2")
	TObjectPtr<UStaticMeshComponent> P2Shutter;

	UPROPERTY(Transient)
	TObjectPtr<AIGPrologueWorldScene> Scene;

	UPROPERTY(Transient)
	TObjectPtr<AIGChapterTwoHumanGateDirector> HumanGateDirector;

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
	TObjectPtr<AIGReadableNote> HomePlanner;

	UPROPERTY(Transient)
	TObjectPtr<AIGReadableNote> MirrorAlarmMemo;

	UPROPERTY(Transient)
	TObjectPtr<AIGReadableNote> MailboxBills;

	UPROPERTY(Transient)
	TObjectPtr<AIGReadableNote> OfferingNote;

	UPROPERTY(Transient)
	TObjectPtr<AIGReadableNote> NightRoster;

	UPROPERTY(Transient)
	TObjectPtr<AIGReadableNote> ManagementNotice;

	UPROPERTY(Transient)
	TObjectPtr<AIGTimeEntryPuzzle> P1TimeEntry;

	UPROPERTY(Transient)
	TObjectPtr<AIGTimeEntryPuzzle> P2TimeEntry;

	FGameplayTag StartedTag;
	FGameplayTag WakeAlarmStoppedTag;
	FGameplayTag WakeStandingTag;
	FGameplayTag FridgeCheckedTag;
	FGameplayTag HasWalletTag;
	FGameplayTag LeftHomeTag;
	FGameplayTag CorridorDarkTag;
	FGameplayTag SawMirrorRoomTag;
	FGameplayTag EnteredMirrorRoomTag;
	FGameplayTag LiftStoppedTag;
	FGameplayTag ReadNoticeTag;
	FGameplayTag EnteredAlleyTag;
	FGameplayTag EnteredStoreTag;
	FGameplayTag SawReceiptTag;
	FGameplayTag CalledEmployeeTag;
	FGameplayTag ReadDuplicateReceiptTag;
	FGameplayTag HumanChecksUnlockedTag;
	FGameplayTag HumanHelpAttemptedTag;
	FGameplayTag HumanLobbyWitnessedTag;
	FGameplayTag StoreHumanMotiveTag;
	/** Accepted only to migrate saves made by the old CH02 repurchase flow. */
	FGameplayTag LegacyWaterPurchasedTag;
	FGameplayTag ReturnedTag;
	FGameplayTag ChapterIdTag;
	FGameplayTag WokeCheckpointTag;
	FGameplayTag CorridorCheckpointTag;
	FGameplayTag StoreCheckpointTag;

	FTimerHandle DirectorStepHandle;
	FTimerHandle ThreatHandle;
	FTimerHandle MirrorDoorHandle;
	FTimerHandle MirrorLampHandle;
	FTimerHandle LiftRevealHandle;
	FTimerHandle LiftResumeHandle;
	FTimerHandle StoreChimeHandle;
	FTimerHandle P1SolvedLightHandle;
	FTimerHandle P2ShutterAnimationHandle;
	FTimerHandle PuzzlePressureHandle;

	double NextBlackoutAllowedTime = 0.0;
	double ThreatEndTime = 0.0;
	double P2ShutterAnimationStartTime = 0.0;
	float ActiveThreatPressure = 0.0f;
	float P2ShutterAnimationStartZ = 230.0f;
	float P1PressureElapsedSeconds = 0.0f;
	float P2PressureElapsedSeconds = 0.0f;
	int32 NextCorridorFixture = INDEX_NONE;
	int32 P1ManualHintStage = 0;
	int32 P2ManualHintStage = 0;
	bool bP1PressureArmed = false;
	bool bMirrorBeatConsumed = false;
	bool bStoreBeatConsumed = false;
	bool bEndingConsumed = false;
};
