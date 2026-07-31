#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "IGApartmentStoryDressing.generated.h"

class AIGInspectable;
class AIGReadableNote;
class UMaterialInterface;
class USceneComponent;
class UStaticMesh;
class UStaticMeshComponent;

/**
 * Cheap, persistent evidence of Han Ji-woon's life in unit 404.
 *
 * The pieces are intentionally ordinary and stay in exactly the same places
 * through CH01 and CH02. Repetition makes them useful story anchors without
 * adding another objective or spoken explanation.
 */
UCLASS()
class INDIEGAME_API AIGApartmentStoryDressing : public AActor
{
	GENERATED_BODY()

public:
	AIGApartmentStoryDressing();

	/**
	 * Builds the desk and entryway dressing from meshes/materials already
	 * loaded by the runtime scene. FridgeDoorPivot may be null; the remaining
	 * clues are still created.
	 */
	void ConfigurePrototypeVisuals(
		UStaticMesh* CubeMesh,
		UMaterialInterface* PaperMaterial,
		UMaterialInterface* DarkPlasticMaterial,
		UMaterialInterface* BookCoverMaterial,
		UMaterialInterface* VestMaterial,
		UMaterialInterface* ReflectiveMaterial,
		UMaterialInterface* PhoneScreenMaterial,
		UMaterialInterface* WaterNoteMaterial,
		USceneComponent* FridgeDoorPivot);

	AIGReadableNote* GetPlannerNote() const { return PlannerNote; }

private:
	UStaticMeshComponent* AddVisual(
		UStaticMesh* Mesh,
		UMaterialInterface* Material,
		const FVector& RelativeLocation,
		const FVector& SizeCentimeters,
		const FRotator& RelativeRotation = FRotator::ZeroRotator);

	AIGInspectable* AddInspectable(
		UStaticMesh* Mesh,
		UMaterialInterface* Material,
		const FVector& RelativeLocation,
		const FVector& SizeCentimeters,
		const FRotator& RelativeRotation,
		const FText& Prompt,
		const FText& Thought);

	AIGReadableNote* AddReadable(
		UStaticMesh* Mesh,
		UMaterialInterface* Material,
		const FVector& RelativeLocation,
		const FVector& SizeCentimeters,
		const FRotator& RelativeRotation,
		const FText& Prompt,
		const FText& Title,
		TArray<FText>&& Lines);

	UPROPERTY(VisibleAnywhere, Category = "Story Dressing|Components")
	TObjectPtr<USceneComponent> StoryRoot;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UStaticMeshComponent>> VisualComponents;

	UPROPERTY(Transient)
	TArray<TObjectPtr<AActor>> StoryInteractables;

	UPROPERTY(Transient)
	TObjectPtr<AIGReadableNote> PlannerNote;

	bool bConfigured = false;
	int32 VisualCounter = 0;
};
