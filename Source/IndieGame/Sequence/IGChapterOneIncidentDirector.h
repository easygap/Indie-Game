#pragma once

#include "CoreMinimal.h"
#include "Interaction/IGInteractableActor.h"
#include "Sequence/IGObjectiveProvider.h"
#include "IGChapterOneIncidentDirector.generated.h"

class AIGChapterOneIncidentDirector;
class AIGNeighborhoodLifeDirector;
class AIGPrologueWorldScene;
class AIGZoneTrigger;
class UMaterialInterface;
class UStaticMesh;
class UStaticMeshComponent;

UENUM()
enum class EIGChapterOneIncidentAction : uint8
{
	None,
	TakePaperCup,
	GiveWaterInCap,
	GiveWaterInPaperCup,
	WaitForCat,
	ClimbToFifthFloor
};

UCLASS(NotBlueprintable, Transient)
class INDIEGAME_API AIGChapterOneIncidentAction final
	: public AIGInteractableActor
{
	GENERATED_BODY()

public:
	AIGChapterOneIncidentAction();

	void Configure(
		AIGChapterOneIncidentDirector* InDirector,
		EIGChapterOneIncidentAction InAction,
		UStaticMesh* Mesh,
		UMaterialInterface* Material,
		const FVector& SizeCentimeters,
		const FText& Prompt,
		float HoldSeconds = 0.0f);

	/** Leaves only a visibility-query surface over permanent world geometry. */
	void ConfigureAsInteractionSurface();

	virtual void CompleteInteraction_Implementation(
		const FIGInteractionContext& Context) override;

private:
	UPROPERTY(VisibleAnywhere, Category = "CH01 Incident")
	TObjectPtr<UStaticMeshComponent> PresentationMesh;

	UPROPERTY(Transient)
	TObjectPtr<AIGChapterOneIncidentDirector> Director;

	EIGChapterOneIncidentAction Action =
		EIGChapterOneIncidentAction::None;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE(
	FIGChapterOneMemoryBoundaryCompletedSignature);

/**
 * Functional CH01 return-and-accident slice.
 *
 * The return walk owns the previously missing choice state: drink/reseal,
 * paper cup, cat water, waiting, the fixed third gust, a physical lobby
 * choice and the normal 4F corridor pattern. CH01 ends on the first 4F->5F
 * tread; the 04:44 accident itself remains hidden until CH03 evidence earns
 * that interpretation.
 */
UCLASS(NotBlueprintable, Transient)
class INDIEGAME_API AIGChapterOneIncidentDirector final
	: public AActor
	, public IIGObjectiveProvider
{
	GENERATED_BODY()

public:
	AIGChapterOneIncidentDirector();

	void Configure(
		AIGPrologueWorldScene* InWorldScene,
		AIGNeighborhoodLifeDirector* InNeighborhood,
		UStaticMesh* CubeMesh,
		UStaticMesh* CylinderMesh,
		UMaterialInterface* PlasticMaterial,
		UMaterialInterface* PaperMaterial,
		UMaterialInterface* WaterMaterial,
		const FTransform& InSceneTransform);

	void HandleAction(
		EIGChapterOneIncidentAction Action,
		AIGChapterOneIncidentAction* Source);

	/** Records physical entry through the 1F common lobby without skipping floors. */
	bool RegisterLobbyReturn();

	/** Records arrival on the physical 4F landing and plays the normal light pattern. */
	bool RegisterFourthFloorReturn();

	/** Starts the 1.8 s memory cut only after the player chooses the first upper tread. */
	bool BeginMemoryBoundary();

	virtual FText GetObjectiveText() const override;
	virtual FString GetObjectiveTextAscii() const override;
	virtual float GetObjectiveProgress() const override;

	UPROPERTY(BlueprintAssignable, Category = "CH01 Incident")
	FIGChapterOneMemoryBoundaryCompletedSignature OnMemoryBoundaryCompleted;

protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	AIGChapterOneIncidentAction* SpawnAction(
		EIGChapterOneIncidentAction Action,
		const FVector& LocalLocation,
		const FVector& SizeCentimeters,
		UStaticMesh* Mesh,
		UMaterialInterface* Material,
		const FText& Prompt,
		float HoldSeconds = 0.0f);
	AIGZoneTrigger* SpawnZone(
		const FVector& LocalLocation,
		const FVector& HalfExtent);
	FVector ToWorld(const FVector& LocalLocation) const;
	FVector GetListenerLocation() const;
	void SetVisibleInteractive(
		AIGChapterOneIncidentAction* Action,
		bool bVisible);
	void ReconcileState();
	void PerformDrink();
	void FinalizeCatChoice();
	void RequestReturnCheckpointAutosave() const;
	void StartFourthFloorCueIfNeeded();
	void AdvanceCorridorBlink();
	void CompleteFourthFloorCue();
	void CompleteMemoryBoundary();
	void PushObjectiveRefresh() const;

	UFUNCTION()
	void HandleDrinkZone(AIGZoneTrigger* Zone);

	UFUNCTION()
	void HandleCatPassZone(AIGZoneTrigger* Zone);

	UPROPERTY(Transient)
	TObjectPtr<AIGPrologueWorldScene> WorldScene;

	UPROPERTY(Transient)
	TObjectPtr<AIGNeighborhoodLifeDirector> Neighborhood;

	UPROPERTY(Transient)
	TObjectPtr<AIGZoneTrigger> DrinkZone;

	UPROPERTY(Transient)
	TObjectPtr<AIGZoneTrigger> CatPassZone;

	UPROPERTY(Transient)
	TObjectPtr<AIGChapterOneIncidentAction> PaperCupAction;

	UPROPERTY(Transient)
	TObjectPtr<AIGChapterOneIncidentAction> CapWaterAction;

	UPROPERTY(Transient)
	TObjectPtr<AIGChapterOneIncidentAction> CupWaterAction;

	UPROPERTY(Transient)
	TObjectPtr<AIGChapterOneIncidentAction> WaitAction;

	UPROPERTY(Transient)
	TObjectPtr<AIGChapterOneIncidentAction> FifthFloorStepAction;

	FTransform SceneTransform = FTransform::Identity;
	bool bConfigured = false;
	bool bFourthFloorCueStarted = false;
	bool bFifthFloorStepArmed = false;
	bool bMemoryBoundaryStarted = false;
	bool bMemoryBoundaryCompleted = false;
	int32 CorridorBlinkStep = 0;
	FTimerHandle BoundaryTimer;
	FTimerHandle CorridorBlinkTimer;
};
