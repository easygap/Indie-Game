#pragma once

#include "CoreMinimal.h"
#include "Interaction/IGInteractableActor.h"
#include "IGInspectable.generated.h"

class UMaterialInterface;
class UStaticMesh;
class UStaticMeshComponent;

/**
 * Re-readable flavor object: interacting pushes a single inner-voice line.
 * Used for the bathroom door, the window and similar dressing.
 */
UCLASS(Blueprintable)
class INDIEGAME_API AIGInspectable : public AIGInteractableActor
{
	GENERATED_BODY()

public:
	AIGInspectable();

	void ConfigurePrototypeVisuals(
		UStaticMesh* Mesh,
		UMaterialInterface* Material,
		const FVector& Scale);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Inspect")
	FText ThoughtText;

	virtual void CompleteInteraction_Implementation(const FIGInteractionContext& Context) override;

protected:
	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Inspect|Components")
	TObjectPtr<UStaticMeshComponent> MeshComponent;
};
