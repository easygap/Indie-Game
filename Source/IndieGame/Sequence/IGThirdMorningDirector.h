#pragma once

#include "CoreMinimal.h"
#include "Interaction/IGInteractableActor.h"
#include "Sequence/IGObjectiveProvider.h"
#include "IGThirdMorningDirector.generated.h"

class AIGPlayerCharacter;
class AIGReadableNote;
class AIGThirdMorningDirector;
class AIGZoneTrigger;
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
	OpenFridge,
	OpenApartmentDoor,
	InspectElevator,
	OpenRoofDoor,
	InspectKeys,
	InspectGlasses,
	OpenTank,
	CloseTank,
	EnterTank
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

	virtual void CompleteInteraction_Implementation(
		const FIGInteractionContext& Context) override;

private:
	UPROPERTY(VisibleAnywhere, Category = "CH03")
	TObjectPtr<UStaticMeshComponent> PresentationMesh;

	UPROPERTY(Transient)
	TObjectPtr<AIGThirdMorningDirector> Director;

	EIGChapterThreeAction Action = EIGChapterThreeAction::OpenFridge;
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
	void BuildLoopingStairwell();
	void BuildFifthFloorAndRoof();
	void BuildWaterTank();
	void SpawnClueDocuments();
	void SpawnTriggers();

	UStaticMeshComponent* CreateBlock(
		const FVector& LocalCenter,
		const FVector& SizeCentimeters,
		UMaterialInterface* Material,
		bool bCollision = true,
		const FRotator& Rotation = FRotator::ZeroRotator,
		UStaticMesh* MeshOverride = nullptr);
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
	void UpdateStairSign();
	void SetPhase(EIGThirdMorningPhase NewPhase);
	void SetFloodMovement(bool bFlooded);

	// --- experience flow --------------------------------------------------
	void BeginAwakening();
	void StopAlarmByItself();
	void EndOpeningSilence();
	void TryUnlockApartmentExit();
	void RevealUpwardRoute();
	void EnterRoofSilence();
	void RevealTank();
	void FinishEndingA();
	void BeginEndingB();
	void FinishEndingB();
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
	void CaptureNextFrame();
	void FinishCaptureSequence();
	void PlaceCaptureCamera(const FVector& LocalPawnLocation, const FVector& LocalLookAt);

	UFUNCTION()
	void HandleCorridorEntered(AIGZoneTrigger* Zone);

	UFUNCTION()
	void HandleStairLoop(AIGZoneTrigger* Zone);

	UFUNCTION()
	void HandleFifthFloorEntered(AIGZoneTrigger* Zone);

	UFUNCTION()
	void HandleLadderEntered(AIGZoneTrigger* Zone);

	UFUNCTION()
	void HandleClueRead(AIGReadableNote* Note, bool bOpened);

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
	UPROPERTY(Transient) TObjectPtr<AIGChapterThreeAction> ApartmentDoorAction;
	UPROPERTY(Transient) TObjectPtr<AIGChapterThreeAction> ElevatorAction;
	UPROPERTY(Transient) TObjectPtr<AIGChapterThreeAction> RoofDoorAction;
	UPROPERTY(Transient) TObjectPtr<AIGChapterThreeAction> KeysAction;
	UPROPERTY(Transient) TObjectPtr<AIGChapterThreeAction> GlassesAction;
	UPROPERTY(Transient) TObjectPtr<AIGChapterThreeAction> TankLidAction;
	UPROPERTY(Transient) TObjectPtr<AIGChapterThreeAction> CloseChoiceAction;
	UPROPERTY(Transient) TObjectPtr<AIGChapterThreeAction> EnterChoiceAction;

	UPROPERTY(Transient) TObjectPtr<AIGReadableNote> PlannerNote;
	UPROPERTY(Transient) TObjectPtr<AIGReadableNote> MotherPhoneNote;
	UPROPERTY(Transient) TObjectPtr<AIGReadableNote> OfferingNote;
	UPROPERTY(Transient) TObjectPtr<AIGReadableNote> EstimateNote;

	UPROPERTY(Transient) TObjectPtr<AIGZoneTrigger> CorridorEntryZone;
	UPROPERTY(Transient) TArray<TObjectPtr<AIGZoneTrigger>> StairLoopZones;
	UPROPERTY(Transient) TObjectPtr<AIGZoneTrigger> FifthFloorZone;
	UPROPERTY(Transient) TObjectPtr<AIGZoneTrigger> LadderZone;

	UPROPERTY(Transient) TObjectPtr<UStaticMeshComponent> FridgeInteriorLightPanel;
	UPROPERTY(Transient) TObjectPtr<UPointLightComponent> FridgeInteriorLight;
	UPROPERTY(Transient) TArray<TObjectPtr<UStaticMeshComponent>> FridgeContents;
	UPROPERTY(Transient) TObjectPtr<UStaticMeshComponent> ApartmentDoorLeaf;
	UPROPERTY(Transient) TObjectPtr<UStaticMeshComponent> UpRouteGate;
	UPROPERTY(Transient) TObjectPtr<UStaticMeshComponent> DownRouteWaterBarrier;
	UPROPERTY(Transient) TObjectPtr<UStaticMeshComponent> RoofDoorLeaf;
	UPROPERTY(Transient) TObjectPtr<UStaticMeshComponent> TankLidVisual;
	UPROPERTY(Transient) TObjectPtr<UStaticMeshComponent> TankWaterSurface;
	UPROPERTY(Transient) TArray<TObjectPtr<UStaticMeshComponent>> BodySilhouette;
	UPROPERTY(Transient) TArray<TObjectPtr<UStaticMeshComponent>> StairLatinSignParts;
	UPROPERTY(Transient) TArray<TObjectPtr<UStaticMeshComponent>> StairSignParts;
	UPROPERTY(Transient) TObjectPtr<UTextRenderComponent> StairSignText;
	UPROPERTY(Transient) TObjectPtr<UTextRenderComponent> StairRoofText;

	UPROPERTY(Transient) TObjectPtr<UAudioComponent> AlarmComponent;
	UPROPERTY(Transient) TObjectPtr<UAudioComponent> WaterBedComponent;

	EIGThirdMorningPhase Phase = EIGThirdMorningPhase::Awakening;
	int32 StairLoopCount = 0;
	int32 CaptureIndex = 0;
	bool bStageBuilt = false;
	bool bAlarmStopped = false;
	bool bFridgeInspected = false;
	bool bPlannerRead = false;
	bool bMotherPhoneRead = false;
	bool bEstimateRead = false;
	bool bKeysInspected = false;
	bool bGlassesInspected = false;
	bool bTankOpened = false;
	bool bEndingFinished = false;
	bool bCaptureMode = false;
	double LastPlayerMovingTime = 0.0;
	double NextDelayedSplashTime = 0.0;

	FTimerHandle AlarmTimer;
	FTimerHandle FlowTimer;
	FTimerHandle MotionPollTimer;
	FTimerHandle CaptureTimer;
};
