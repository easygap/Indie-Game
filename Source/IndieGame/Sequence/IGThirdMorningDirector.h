#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Interaction/IGInteractableActor.h"
#include "Sequence/IGObjectiveProvider.h"
#include "IGThirdMorningDirector.generated.h"

class AIGPlayerCharacter;
class AIGReadableNote;
class AIGThirdMorningDirector;
class AIGZoneTrigger;
class UIGRebirthNarrativeSubsystem;
class UIGSaveGame;
class UAudioComponent;
class UMaterialInterface;
class UPointLightComponent;
class USceneComponent;
class UStaticMesh;
class UStaticMeshComponent;
class UTextRenderComponent;

/**
 * World-space verbs used by the third morning.  They deliberately stay
 * physical: the ending is selected by touching a lid handle or walking to
 * the opposite rim, never by choosing a menu item.
 */
UENUM()
enum class EIGChapterThreeAction : uint8
{
	None,
	OpenFridge,
	RecoverFlashlight,
	OpenApartmentDoor,
	InspectElevator,
	OpenRoofDoor,
	InspectKeys,
	P3CloseDirectInlet,
	P3CloseReserveInlet,
	P3OpenPressureRelease,
	P3OpenFloorDrain,
	EvidenceCatEntered,
	EvidenceCatExited,
	EvidenceHosePaw,
	EvidenceHoseImpact,
	EvidenceBag,
	EvidenceWetRung,
	EvidenceHandSmear,
	EvidenceGlasses,
	EvidenceTankClothing,
	EvidenceCurrentSleeve,
	EvidenceSearchPoster,
	OpenTank,
	CloseTank,
	SupportLid
};

/** Small native interaction target owned by AIGThirdMorningDirector. */
UCLASS(NotBlueprintable, Transient)
class INDIEGAME_API AIGChapterThreeAction final : public AIGInteractableActor
{
	GENERATED_BODY()

public:
	AIGChapterThreeAction();

	void Configure(
		AIGThirdMorningDirector* InDirector,
		EIGChapterThreeAction InAction,
		UStaticMesh* Mesh,
		UMaterialInterface* Material,
		const FVector& SizeCentimeters,
		const FText& Prompt,
		float HoldSeconds = 0.0f);

	UStaticMeshComponent* GetPresentationMesh() const { return PresentationMesh; }
	void SetHoldSeconds(float HoldSeconds)
	{
		InteractionHoldDuration = FMath::Max(0.0f, HoldSeconds);
	}

	virtual void CompleteInteraction_Implementation(
		const FIGInteractionContext& Context) override;

private:
	UPROPERTY(VisibleAnywhere, Category = "CH03")
	TObjectPtr<UStaticMeshComponent> PresentationMesh;

	UPROPERTY(Transient)
	TObjectPtr<AIGThirdMorningDirector> Director;

	EIGChapterThreeAction Action = EIGChapterThreeAction::None;
};

UENUM()
enum class EIGThirdMorningPhase : uint8
{
	Awakening,
	ApartmentClues,
	FloodedCorridor,
	StairLoops,
	UpwardOnly,
	FifthFloor,
	Roof,
	TankReveal,
	Choice,
	Ending
};

/**
 * Complete greybox vertical slice for CH03.
 *
 * The actor owns a detached, pooled runtime stage so the monolithic prologue
 * scene only needs one spawn/transition seam.  It has no monster, chase,
 * fail-state, or random scare.  Tension comes from deterministic spatial
 * repetition, water, missing city life, and the player's final physical act.
 */
UCLASS(NotBlueprintable, Transient)
class INDIEGAME_API AIGThirdMorningDirector final
	: public AActor
	, public IIGObjectiveProvider
{
	GENERATED_BODY()

public:
	AIGThirdMorningDirector();

	/** Builds the stage, positions the player, then starts the self-stopping alarm. */
	void ConfigureAndStart(bool bShowIntroCard, bool bCaptureSequence);

	/** Rehydrates a loaded save at a deterministic non-transient world anchor. */
	void RestoreCheckpointAnchor(FGameplayTag CheckpointTag);

	/** Called only by the director-owned physical interaction actors. */
	void HandleAction(EIGChapterThreeAction Action, AIGChapterThreeAction* Source);

	virtual FText GetObjectiveText() const override;
	virtual FString GetObjectiveTextAscii() const override;
	virtual float GetObjectiveProgress() const override;

	/** Stable origin used by validation and deterministic documentation capture. */
	static FVector GetStageOrigin();

protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	// --- stage assembly ---------------------------------------------------
	void BuildStage();
	void BuildApartment();
	void BuildFloodedCorridor();
	void BuildP3ServiceCabinet();
	void BuildLoopingStairwell();
	void BuildFifthFloorAndRoof();
	void BuildWaterTank();
	void BuildP5AccidentEvidence();
	void PresentPurchaseEvidenceContinuity();
	void SpawnClueDocuments();
	void SpawnTriggers();

	UStaticMeshComponent* CreateBlock(
		const FVector& LocalCenter,
		const FVector& SizeCentimeters,
		UMaterialInterface* Material,
		bool bCollision = true,
		const FRotator& Rotation = FRotator::ZeroRotator,
		UStaticMesh* MeshOverride = nullptr,
		bool bMovable = false);
	UPointLightComponent* CreatePointLight(
		const FVector& LocalLocation,
		float Intensity,
		float Radius,
		const FLinearColor& Color,
		bool bCastShadows);
	AIGChapterThreeAction* SpawnAction(
		EIGChapterThreeAction Action,
		const FVector& LocalLocation,
		const FVector& SizeCentimeters,
		UMaterialInterface* Material,
		const FText& Prompt,
		float HoldSeconds = 0.0f,
		const FRotator& Rotation = FRotator::ZeroRotator,
		UStaticMesh* MeshOverride = nullptr);
	AIGReadableNote* SpawnNote(
		const FVector& LocalLocation,
		const FRotator& Rotation,
		const FVector& PaperSize,
		const FText& Prompt,
		const FText& Title,
		TArray<FText> BodyLines,
		UMaterialInterface* PaperMaterial);
	AIGZoneTrigger* SpawnZone(const FVector& LocalLocation, const FVector& HalfExtent);
	FVector ToWorld(const FVector& LocalLocation) const;

	void SetVisibleInteractive(AIGChapterThreeAction* Action, bool bVisible);
	void SetDocumentAvailable(AIGReadableNote* Note, bool bAvailable);
	bool HasMemoryFlashlight() const;
	void ApplyTankRevealVisibility();
	void ArmFlashlightReturnReveal();
	void UpdateStairSign();
	void SetPhase(EIGThirdMorningPhase NewPhase);
	void SetFloodMovement(bool bFlooded);
	UIGRebirthNarrativeSubsystem* GetRebirthState() const;
	void BootstrapRebirthContext();
	void CommitChapterThreeState();
	void RequestCheckpointAutosave(const TCHAR* CheckpointTagName) const;
	bool RestoreChapterThreeState();
	void ApplyChapterThreeWorldState();
	void ApplyCommonDiscoveryWorldState();
	void ResumeRestoredEnding();
	void RegisterTruth(
		const TCHAR* TruthName,
		FName SourceId,
		bool bConfirmsTruth = true);
	void RegisterTruth(
		const TCHAR* TruthName,
		const TCHAR* SourceId,
		bool bConfirmsTruth = true);

	// --- experience flow --------------------------------------------------
	void BeginAwakening();
	void StopAlarmByItself();
	void EndOpeningSilence();
	void TryUnlockApartmentExit();
	void RevealUpwardRoute();
	void EnterRoofSilence();
	void RevealRoofInvestigationFolder();
	void RevealTank();
	void BeginP3PressureRelease();
	void UpdateP3PressureRelease();
	void PollP3Hint();
	void RestoreP3HintHighlight();
	void StopP3HintClock();
	void HandleP3Mistake();
	void CompleteP3();
	void FocusOrCompareEvidence(EIGChapterThreeAction EvidenceAction);
	void RegisterEvidenceObservation(EIGChapterThreeAction EvidenceAction);
	void RefreshEvidencePrompts();
	bool ResolveEvidencePair(
		EIGChapterThreeAction First,
		EIGChapterThreeAction Second);
	bool IsEvidencePairValid(
		EIGChapterThreeAction First,
		EIGChapterThreeAction Second) const;
	int32 GetAccidentTruthCount() const;
	bool IsEndingChoiceReady() const;
	void UpdateEndingAvailability();
	void ScheduleAccidentScratch();
	void EvaluateAccidentScratchGate();
	void PlayAccidentScratch();
	void SettleAccidentScratchTail();
	void FinishEndingA();
	void FinishEndingAAfterDiscovery();
	void BeginEndingB();
	void FinishEndingB();
	void ShowCommonDiscoveryCard();
	void PresentEndingControls(const FText& EndingTitle, const FText& EndingSubtitle);
	void EnableEndingInput();
	void RestartChapter();
	void ReturnToBeginning();

	void StartWaterBed();
	void StopAllChapterAudio();
	void PollPlayerMotion();
	void PlayDelayedSplash();
	void PlayMetalEcho(const FVector& LocalLocation, float PitchMultiplier = 1.0f);
	void PlayTankSlam();

	// --- capture/smoke hook ----------------------------------------------
	void StartCaptureSequence();
	void WaitForRebirthGreyboxScratches();
	void CaptureNextFrame();
	void FinishCaptureSequence();
	void PlaceCaptureCamera(const FVector& LocalPawnLocation, const FVector& LocalLookAt);
	void StartRebirthGreyboxValidation();
	void ContinueRebirthGreyboxValidation();
	void FinishRebirthGreyboxValidation();
	void StartRebirthReleaseValidation();
	bool ValidateRebirthCollisionRoute(
		int32& OutFloorSamples,
		int32& OutCapsuleSegments) const;
	bool ValidateRebirthAudioQueue(
		int32& OutGeneratedSamples,
		int32& OutGeneratedBytes,
		int32& OutNonZeroSamples);
	void BeginRebirthEndingValidation();
	void FinishRebirthEndingValidation();
	void FailRebirthReleaseValidation(const TCHAR* Reason);

	UFUNCTION()
	void HandleReleaseValidationSaveCompleted(bool bSuccess, FString SlotName);

	UFUNCTION()
	void HandleReleaseValidationLoadCompleted(
		bool bSuccess,
		FString SlotName,
		UIGSaveGame* SaveGame);

	UFUNCTION()
	void HandleCorridorEntered(AIGZoneTrigger* Zone);

	UFUNCTION()
	void HandleStairLoop(AIGZoneTrigger* Zone);

	UFUNCTION()
	void HandleFifthFloorEntered(AIGZoneTrigger* Zone);

	UFUNCTION()
	void HandleLadderEntered(AIGZoneTrigger* Zone);

	UFUNCTION()
	void HandleFlashlightReturnReveal(AIGZoneTrigger* Zone);

	UFUNCTION()
	void HandleClueRead(AIGReadableNote* Note, bool bOpened);

	UFUNCTION()
	void HandleRebirthTruthChanged(
		FGameplayTag TruthTag,
		FName SourceId,
		bool bConfirmed);

	UPROPERTY(VisibleAnywhere, Category = "CH03|Components")
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UStaticMeshComponent>> Geometry;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UPointLightComponent>> Lights;

	UPROPERTY(Transient) TObjectPtr<UStaticMesh> CubeMesh;
	UPROPERTY(Transient) TObjectPtr<UStaticMesh> CylinderMesh;
	UPROPERTY(Transient) TObjectPtr<UStaticMesh> SphereMesh;
	UPROPERTY(Transient) TObjectPtr<UStaticMesh> WaterBottleMesh;
	UPROPERTY(Transient) TObjectPtr<UStaticMesh> BottleCapMesh;
	UPROPERTY(Transient) TObjectPtr<UStaticMesh> LabelSleeveMesh;

	UPROPERTY(Transient) TObjectPtr<UMaterialInterface> ConcreteMaterial;
	UPROPERTY(Transient) TObjectPtr<UMaterialInterface> DarkConcreteMaterial;
	UPROPERTY(Transient) TObjectPtr<UMaterialInterface> RoomFloorMaterial;
	UPROPERTY(Transient) TObjectPtr<UMaterialInterface> WallMaterial;
	UPROPERTY(Transient) TObjectPtr<UMaterialInterface> MetalMaterial;
	UPROPERTY(Transient) TObjectPtr<UMaterialInterface> PlasticMaterial;
	UPROPERTY(Transient) TObjectPtr<UMaterialInterface> PaperMaterial;
	UPROPERTY(Transient) TObjectPtr<UMaterialInterface> WetPaperMaterial;
	UPROPERTY(Transient) TObjectPtr<UMaterialInterface> WaterMaterial;
	UPROPERTY(Transient) TObjectPtr<UMaterialInterface> WetStepMaterial;
	UPROPERTY(Transient) TObjectPtr<UMaterialInterface> ScreenMaterial;
	UPROPERTY(Transient) TObjectPtr<UMaterialInterface> BeddingMaterial;
	UPROPERTY(Transient) TObjectPtr<UMaterialInterface> GlassMaterial;
	UPROPERTY(Transient) TObjectPtr<UMaterialInterface> BottleCapMaterial;
	UPROPERTY(Transient) TObjectPtr<UMaterialInterface> WaterLabelMaterial;
	UPROPERTY(Transient) TObjectPtr<UMaterialInterface> FridgeBodyMaterial;
	UPROPERTY(Transient) TObjectPtr<UMaterialInterface> FridgeInteriorMaterial;
	UPROPERTY(Transient) TObjectPtr<UMaterialInterface> EmergencyMaterial;

	UPROPERTY(Transient) TObjectPtr<AIGChapterThreeAction> FridgeDoorAction;
	UPROPERTY(Transient) TObjectPtr<AIGChapterThreeAction> MemoryFlashlightAction;
	UPROPERTY(Transient) TObjectPtr<AIGChapterThreeAction> ApartmentDoorAction;
	UPROPERTY(Transient) TObjectPtr<AIGChapterThreeAction> ElevatorAction;
	UPROPERTY(Transient) TObjectPtr<AIGChapterThreeAction> RoofDoorAction;
	UPROPERTY(Transient) TObjectPtr<AIGChapterThreeAction> KeysAction;
	UPROPERTY(Transient) TObjectPtr<AIGChapterThreeAction> GlassesAction;
	UPROPERTY(Transient) TObjectPtr<AIGChapterThreeAction> CurrentSleeveAction;
	UPROPERTY(Transient) TObjectPtr<AIGChapterThreeAction> SearchPosterAction;
	UPROPERTY(Transient) TObjectPtr<AIGChapterThreeAction> P3DirectInletAction;
	UPROPERTY(Transient) TObjectPtr<AIGChapterThreeAction> P3ReserveInletAction;
	UPROPERTY(Transient) TObjectPtr<AIGChapterThreeAction> P3PressureReleaseAction;
	UPROPERTY(Transient) TObjectPtr<AIGChapterThreeAction> P3FloorDrainAction;
	UPROPERTY(Transient) TObjectPtr<AIGChapterThreeAction> P3HintHighlightedAction;
	UPROPERTY(Transient) TObjectPtr<AIGChapterThreeAction> TankLidAction;
	UPROPERTY(Transient) TObjectPtr<AIGChapterThreeAction> CloseChoiceAction;
	UPROPERTY(Transient) TObjectPtr<AIGChapterThreeAction> SupportChoiceAction;
	UPROPERTY(Transient)
	TMap<EIGChapterThreeAction, TObjectPtr<AIGChapterThreeAction>> EvidenceActions;

	UPROPERTY(Transient) TObjectPtr<AIGReadableNote> PlannerNote;
	UPROPERTY(Transient) TObjectPtr<AIGReadableNote> MotherPhoneNote;
	UPROPERTY(Transient) TObjectPtr<AIGReadableNote> OfferingNote;
	UPROPERTY(Transient) TObjectPtr<AIGReadableNote> EstimateNote;
	UPROPERTY(Transient) TObjectPtr<AIGReadableNote> RecheckNoticeNote;
	UPROPERTY(Transient) TObjectPtr<AIGReadableNote> PreservationNoticeNote;
	UPROPERTY(Transient) TObjectPtr<AIGReadableNote> PoliceChecklistNote;
	UPROPERTY(Transient) TObjectPtr<AIGReadableNote> P3RecordNote;
	UPROPERTY(Transient) TObjectPtr<AIGReadableNote> P3PhotoNote;
	UPROPERTY(Transient) TObjectPtr<AIGReadableNote> ManagementDbNote;

	UPROPERTY(Transient) TObjectPtr<AIGZoneTrigger> CorridorEntryZone;
	UPROPERTY(Transient) TArray<TObjectPtr<AIGZoneTrigger>> StairLoopZones;
	UPROPERTY(Transient) TObjectPtr<AIGZoneTrigger> FifthFloorZone;
	UPROPERTY(Transient) TObjectPtr<AIGZoneTrigger> LadderZone;
	UPROPERTY(Transient) TObjectPtr<AIGZoneTrigger> FlashlightReturnRevealZone;

	UPROPERTY(Transient) TObjectPtr<UStaticMeshComponent> FridgeInteriorLightPanel;
	UPROPERTY(Transient) TObjectPtr<UPointLightComponent> FridgeInteriorLight;
	UPROPERTY(Transient) TArray<TObjectPtr<UStaticMeshComponent>> FridgeContents;
	UPROPERTY(Transient) TObjectPtr<UStaticMeshComponent> ApartmentDoorLeaf;
	UPROPERTY(Transient) TObjectPtr<UStaticMeshComponent> UpRouteGate;
	UPROPERTY(Transient) TObjectPtr<UStaticMeshComponent> DownRouteWaterBarrier;
	UPROPERTY(Transient) TObjectPtr<UStaticMeshComponent> CorridorWaterVisual;
	UPROPERTY(Transient) TObjectPtr<UStaticMeshComponent> P3PressureNeedle;
	UPROPERTY(Transient) TObjectPtr<UStaticMeshComponent> P3BleedTubeVisual;
	UPROPERTY(Transient) TObjectPtr<UStaticMeshComponent> P3BleedWaterVisual;
	UPROPERTY(Transient) TObjectPtr<UStaticMeshComponent> RoofDoorLeaf;
	UPROPERTY(Transient) TObjectPtr<UStaticMeshComponent> TankLidVisual;
	UPROPERTY(Transient) TObjectPtr<UStaticMeshComponent> TankWaterSurface;
	UPROPERTY(Transient) TArray<TObjectPtr<UStaticMeshComponent>> BodySilhouette;
	UPROPERTY(Transient) TArray<TObjectPtr<UStaticMeshComponent>> StairLatinSignParts;
	UPROPERTY(Transient) TArray<TObjectPtr<UStaticMeshComponent>> StairSignParts;
	UPROPERTY(Transient) TObjectPtr<UTextRenderComponent> StairSignText;
	UPROPERTY(Transient) TObjectPtr<UTextRenderComponent> StairRoofText;
	UPROPERTY(Transient) TObjectPtr<UTextRenderComponent> P3PressureText;

	UPROPERTY(Transient) TObjectPtr<UAudioComponent> AlarmComponent;
	UPROPERTY(Transient) TObjectPtr<UAudioComponent> WaterBedComponent;

	EIGThirdMorningPhase Phase = EIGThirdMorningPhase::Awakening;
	int32 StairLoopCount = 0;
	int32 CaptureIndex = 0;
	int32 P3MistakeCount = 0;
	int32 AccidentScratchCount = 0;
	int32 GreyboxScratchWaitTicks = 0;
	bool bStageBuilt = false;
	bool bAlarmStopped = false;
	bool bFridgeInspected = false;
	bool bPlannerRead = false;
	bool bMotherPhoneRead = false;
	bool bEstimateRead = false;
	bool bRecheckNoticeRead = false;
	bool bPreservationNoticeRead = false;
	bool bPoliceChecklistRead = false;
	bool bP3RecordRead = false;
	bool bP3PhotoRead = false;
	bool bManagementDbRead = false;
	bool bKeysInspected = false;
	bool bGlassesInspected = false;
	bool bP3DirectClosed = false;
	bool bP3ReserveClosed = false;
	bool bP3PressureReleaseOpen = false;
	bool bP3PressureZero = false;
	bool bP3Solved = false;
	bool bTankOpened = false;
	bool bAccidentScratchTailSettled = false;
	bool bLookedAwayAfterFirstScratch = false;
	bool bActedAfterSecondScratch = false;
	bool bScratchGatePlayerWasMoving = false;
	bool bEndingFinished = false;
	bool bEndingASelected = false;
	bool bCaptureMode = false;
	bool bGreyboxValidationMode = false;
	bool bReleaseValidationMode = false;
	bool bGreyboxDocumentSkipRouteValid = false;
	bool bReleaseValidationInProgress = false;
	bool bReleaseValidationEndingA = false;
	bool bRestoredChapterThreeProgress = false;
	FString ReleaseValidationSlotName;
	float P3PressureKPa = 60.0f;
	float P3HintElapsedSeconds = 0.0f;
	int32 P3ZeroConfirmationTicks = 0;
	int32 P3HintStage = 0;
	EIGChapterThreeAction FocusedEvidence = EIGChapterThreeAction::None;
	TArray<FName> ObservedP5Sources;
	double AccidentScratchGateStartSeconds = -1.0;
	double LastPlayerMovingTime = 0.0;
	double NextDelayedSplashTime = 0.0;

	FTimerHandle AlarmTimer;
	FTimerHandle FlowTimer;
	FTimerHandle RoofFolderRevealTimer;
	FTimerHandle LadderEchoTimer;
	FTimerHandle MotionPollTimer;
	FTimerHandle CaptureTimer;
	FTimerHandle ReleaseValidationTimer;
	FTimerHandle P3PressureTimer;
	FTimerHandle P3HintTimer;
	FTimerHandle P3HintVisualTimer;
	FTimerHandle AccidentScratchTimer;
	FTimerHandle AccidentScratchTailTimer;
};
