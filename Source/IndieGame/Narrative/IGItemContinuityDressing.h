#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Narrative/IGRebirthNarrativeTypes.h"
#include "IGItemContinuityDressing.generated.h"

class UMaterialInterface;
class USceneComponent;
class UStaticMesh;
class UStaticMeshComponent;

enum class EIGItemContinuityPresentation : uint8
{
	AccidentBag,
	LobbyRecycleSack
};

/**
 * Non-interactive static evidence for the purchased water.
 *
 * One actor owns the complete presentation so a chapter transition cannot
 * leave a carried bottle and a second loose copy alive at the same time.
 * Profile and closure state are read from the persisted REBIRTH choices and
 * are never inferred again from the cat branch.
 */
UCLASS(NotBlueprintable, Transient)
class INDIEGAME_API AIGItemContinuityDressing final : public AActor
{
	GENERATED_BODY()

public:
	AIGItemContinuityDressing();

	void Configure(
		EIGItemContinuityPresentation InPresentation,
		EIGRebirthPurchaseProfile InPurchaseProfile,
		EIGRebirthBottleClosureState InBottleClosureState,
		UStaticMesh* CubeMesh,
		UStaticMesh* CylinderMesh,
		UMaterialInterface* BagMaterial,
		UMaterialInterface* BottleMaterial,
		UMaterialInterface* WaterMaterial,
		UMaterialInterface* CapMaterial);

	bool MatchesContract(
		EIGItemContinuityPresentation ExpectedPresentation,
		EIGRebirthPurchaseProfile ExpectedPurchaseProfile,
		EIGRebirthBottleClosureState ExpectedBottleClosureState) const;

	/** The same detached roof-stage pose used by CH03 P5 evidence. */
	static FTransform GetCanonicalAccidentTransform(
		EIGRebirthPurchaseProfile PurchaseProfile);

private:
	UStaticMeshComponent* AddPart(
		const TCHAR* BaseName,
		UStaticMesh* Mesh,
		UMaterialInterface* Material,
		const FVector& LocalCenter,
		const FVector& SizeCentimeters,
		const FRotator& LocalRotation = FRotator::ZeroRotator);
	void BuildAccidentBag(
		UStaticMesh* CubeMesh,
		UStaticMesh* CylinderMesh,
		UMaterialInterface* BagMaterial,
		UMaterialInterface* BottleMaterial,
		UMaterialInterface* WaterMaterial,
		UMaterialInterface* CapMaterial);
	void BuildLobbyRecycleSack(
		UStaticMesh* CubeMesh,
		UStaticMesh* CylinderMesh,
		UMaterialInterface* BagMaterial,
		UMaterialInterface* BottleMaterial,
		UMaterialInterface* WaterMaterial,
		UMaterialInterface* CapMaterial);
	void AddBottle(
		UStaticMesh* CylinderMesh,
		UMaterialInterface* BottleMaterial,
		UMaterialInterface* WaterMaterial,
		UMaterialInterface* CapMaterial,
		const FVector& BaseLocation,
		const FRotator& Rotation,
		bool bPrimaryBottle);

	UPROPERTY(VisibleAnywhere, Category = "Item Continuity")
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UStaticMeshComponent>> PresentationParts;

	EIGItemContinuityPresentation Presentation =
		EIGItemContinuityPresentation::AccidentBag;
	EIGRebirthPurchaseProfile PurchaseProfile =
		EIGRebirthPurchaseProfile::Unset;
	EIGRebirthBottleClosureState BottleClosureState =
		EIGRebirthBottleClosureState::Unset;
	bool bConfigured = false;
	int32 BottleCount = 0;
	int32 CapCount = 0;
	int32 PartSerial = 0;
};
