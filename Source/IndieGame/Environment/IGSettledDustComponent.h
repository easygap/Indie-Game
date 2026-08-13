#pragma once

#include "CoreMinimal.h"
#include "Components/SceneComponent.h"
#include "IGSettledDustComponent.generated.h"

class UInstancedStaticMeshComponent;
class UIGDustSubsystem;

/**
 * 분진 퇴적 — the fifth floor's floor, and the marks left in it (§11 V2).
 *
 * Plaster dust that came off a year of covering a wall has settled here, and it
 * holds whatever presses it: the player's shoes and his dragged elbows. Nothing
 * in the game reads these marks except the player, which is exactly the value —
 * in a dark half-finished floor of identical bays, the trail behind you is the
 * only proof you have already searched somewhere.
 *
 * The field owns its own extent, so reporters never need to know where dust
 * lies. A footfall on bare corridor tile is still reported and simply never
 * drawn, which keeps the noise, dust and footstep systems independent.
 *
 * Follows the V2 decal contract: receiving planes float 0.15 cm off the floor,
 * cast no shadow, take no collision and receive no decals of their own. The
 * world computes light colour and contact shadow; the mark only wets the
 * material it lies on.
 */
UCLASS(ClassGroup = (IndieGame), meta = (BlueprintSpawnableComponent))
class INDIEGAME_API UIGSettledDustComponent final : public USceneComponent
{
	GENERATED_BODY()

public:
	UIGSettledDustComponent();

	virtual void BeginPlay() override;
	virtual void TickComponent(
		float DeltaSeconds,
		ELevelTick TickType,
		FActorComponentTickFunction* ThisTickFunction) override;

	/**
	 * Where dust actually lies, in world space. Marks outside are discarded.
	 * FloorZ is the surface the prints sit on; the box only needs to be right in
	 * plan, because a print is always laid flat on that one height.
	 */
	void ConfigureField(const FBox& WorldBoundsXY, float FloorZ);

	UFUNCTION(BlueprintPure, Category = "Dust")
	int32 GetDrawnFootfallCount() const { return DrawnFootfalls; }

	UFUNCTION(BlueprintPure, Category = "Dust")
	int32 GetDrawnDragCount() const { return DrawnDrags; }

	/** True once the field has bounds and both mark layers exist. */
	UFUNCTION(BlueprintPure, Category = "Dust")
	bool IsFieldReady() const;

	/** §11 V2: 수광 평면은 벽/바닥에서 0.15cm만 띄운다. */
	static constexpr float SurfaceOffset = 0.15f;

	/** A shoe sole at 1:1. Not a stylised arrow — this is a real print. */
	static constexpr float FootfallLengthCentimeters = 27.0f;
	static constexpr float FootfallWidthCentimeters = 10.0f;

	/** A dragged torso smears far wider than it is long. */
	static constexpr float DragLengthCentimeters = 62.0f;
	static constexpr float DragWidthCentimeters = 44.0f;

private:
	void RebuildMarks();
	static UInstancedStaticMeshComponent* CreateMarkLayer(
		USceneComponent* Parent,
		const TCHAR* Name,
		const TCHAR* MaterialPath);

	UPROPERTY(VisibleAnywhere, Category = "Dust", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UInstancedStaticMeshComponent> Footfalls;

	UPROPERTY(VisibleAnywhere, Category = "Dust", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UInstancedStaticMeshComponent> Drags;

	UPROPERTY(Transient)
	TObjectPtr<UIGDustSubsystem> DustSubsystem;

	FBox FieldBounds = FBox(ForceInit);
	float FieldFloorZ = 0.0f;
	float RebuildAccumulator = 0.0f;
	int32 DrawnFootfalls = 0;
	int32 DrawnDrags = 0;
	int32 LastPrintCount = -1;
	bool bFieldConfigured = false;
};
