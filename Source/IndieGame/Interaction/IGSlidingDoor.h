#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Interaction/IGDoorAnimation.h"
#include "IGSlidingDoor.generated.h"

class UBoxComponent;
class UMaterialInterface;
class USceneComponent;
class UStaticMesh;
class UStaticMeshComponent;

/**
 * Automatic two-panel sliding door with the classic convenience-store
 * entrance chime. A pawn overlap opens it; it closes shortly after the last
 * pawn leaves. PlayChime() lets directors ring it with the door shut.
 */
UCLASS(Blueprintable)
class INDIEGAME_API AIGSlidingDoor : public AActor
{
	GENERATED_BODY()

public:
	AIGSlidingDoor();
	virtual void Tick(float DeltaSeconds) override;

	/** Builds both panels; PanelSize is one panel (thickness X, width Y, height Z). */
	void ConfigurePrototypeVisuals(
		UStaticMesh* CubeMesh,
		UMaterialInterface* GlassMaterial,
		UMaterialInterface* FrameMaterial,
		const FVector& PanelSize);

	/** Rings the entrance chime without touching the panels. */
	UFUNCTION(BlueprintCallable, Category = "Door")
	void PlayChime(float VolumeMultiplier = 1.0f);

	UFUNCTION(BlueprintPure, Category = "Door")
	bool IsOpen() const { return bOpen; }

protected:
	virtual void BeginPlay() override;

	UFUNCTION()
	void HandleTriggerBeginOverlap(
		UPrimitiveComponent* OverlappedComponent,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComponent,
		int32 OtherBodyIndex,
		bool bFromSweep,
		const FHitResult& SweepResult);

	UFUNCTION()
	void HandleTriggerEndOverlap(
		UPrimitiveComponent* OverlappedComponent,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComponent,
		int32 OtherBodyIndex);

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Door|Components")
	TObjectPtr<USceneComponent> DoorRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Door|Components")
	TObjectPtr<UStaticMeshComponent> LeftPanel;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Door|Components")
	TObjectPtr<UStaticMeshComponent> RightPanel;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Door|Components")
	TObjectPtr<UBoxComponent> ApproachTrigger;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Door", meta = (ClampMin = "0.05", Units = "s"))
	float SlideDuration = 0.7f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Door", meta = (ClampMin = "0.0", Units = "s"))
	float CloseDelay = 2.4f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Door")
	bool bChimeEnabled = true;

private:
	void SetDoorOpen(bool bInOpen);
	void HandleCloseTimer();

	FIGDoorAnimation SlideAnimation;
	FTimerHandle CloseTimerHandle;
	float PanelSlideDistance = 55.0f;
	int32 OverlappingPawnCount = 0;
	bool bOpen = false;
};
