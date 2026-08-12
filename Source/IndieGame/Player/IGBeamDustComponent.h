#pragma once

#include "CoreMinimal.h"
#include "Components/SceneComponent.h"
#include "IGBeamDustComponent.generated.h"

class UInstancedStaticMeshComponent;
class UIGDustSubsystem;
class UStaticMesh;

/**
 * 빔 속 분진 — the motes that only exist because you are looking at them.
 *
 * STORY_BIBLE_MISSING_FLOOR.md §11 V1 asks for suspended plaster dust inside
 * the torch beam, twice as dense where the one upstairs has recently dragged
 * himself past. Both halves matter, and the second half is the reason this is a
 * system rather than a decoration: **빛으로 그의 최근 경로를 읽는다.** The
 * player lifts the torch, sees a thick lane of glittering dust, and learns
 * which way he went — without a marker, an outline or a line of text.
 *
 * The motes are lit, never emissive. A rough near-white non-metal speck is
 * invisible in a black corridor and blazes inside the cone, so the beam does
 * the whole reveal and darkness stays honestly dark. One instanced-mesh draw
 * carries the whole cloud; nothing ticks while the torch is off or browned out.
 */
UCLASS(ClassGroup = (IndieGame), meta = (BlueprintSpawnableComponent))
class INDIEGAME_API UIGBeamDustComponent final : public USceneComponent
{
	GENERATED_BODY()

public:
	UIGBeamDustComponent();

	virtual void BeginPlay() override;
	virtual void TickComponent(
		float DeltaSeconds,
		ELevelTick TickType,
		FActorComponentTickFunction* ThisTickFunction) override;

	/** Cone half-angle of the light that reveals the cloud, in degrees. */
	void SetBeamCone(float OuterConeDegrees);

	/**
	 * Drives the cloud from the torch. Origin and Direction are the beam's own
	 * world transform, so the dust follows the sway rather than the head.
	 * BeamStrength is the live flicker value: a browned-out beam reveals
	 * nothing, which keeps the darkest frames free of floating white specks.
	 */
	void UpdateBeam(
		const FVector& Origin,
		const FVector& Direction,
		float BeamStrength);

	/** Hides and parks the cloud. Called when the torch goes out. */
	void ClearBeam();

	UFUNCTION(BlueprintPure, Category = "Dust")
	int32 GetActiveMoteCount() const { return ActiveMoteCount; }

	/** 1.0 in ordinary air, 2.0 down a lane he has just crawled. */
	UFUNCTION(BlueprintPure, Category = "Dust")
	float GetBeamDensityMultiplier() const { return BeamDensityMultiplier; }

	/** Motes present in ordinary corridor air. */
	static constexpr int32 BaseMoteCount = 110;

	/** Motes added on top when the whole beam sits on a fresh lane. */
	static constexpr int32 TrailMoteCount = 110;

	static constexpr int32 MaxMoteCount = BaseMoteCount + TrailMoteCount;

	/** Beam depth the cloud occupies, in centimeters. */
	static constexpr float NearDistance = 55.0f;
	static constexpr float FarDistance = 540.0f;

private:
	struct FIGMote
	{
		FVector Location = FVector::ZeroVector;
		FVector Drift = FVector::ZeroVector;
		float SizeScale = 1.0f;
		bool bSeeded = false;
	};

	/**
	 * Places one mote inside the cone. A non-null Anchor is a trail sample the
	 * beam is currently crossing, and the mote is dropped near it — that is what
	 * turns extra density into a readable lane rather than an even fog.
	 */
	void RespawnMote(
		FIGMote& Mote,
		const FVector& Origin,
		const FVector& Direction,
		const FVector& Right,
		const FVector& Up,
		const FVector* Anchor);

	/** Rebuilds the trail anchors that currently sit inside the beam volume. */
	void RefreshTrailAnchors(const FVector& Origin, const FVector& Direction);

	/** Depth and cone test for one world point. OutDepth is along the beam. */
	bool IsInsideBeam(
		const FVector& Point,
		const FVector& Origin,
		const FVector& Direction,
		float& OutDepth) const;

	UIGDustSubsystem* ResolveDustSubsystem();

	UPROPERTY(VisibleAnywhere, Category = "Dust", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UInstancedStaticMeshComponent> Motes;

	UPROPERTY(Transient)
	TObjectPtr<UIGDustSubsystem> DustSubsystem;

	TArray<FIGMote> Pool;
	TArray<FTransform> InstanceTransforms;
	TArray<FVector> TrailAnchors;
	FRandomStream Random;

	FVector BeamOrigin = FVector::ZeroVector;
	FVector BeamDirection = FVector::ForwardVector;
	float CosOuterCone = 0.0f;
	float OuterConeDegrees = 34.0f;
	float BeamDensityMultiplier = 1.0f;
	float TrailRefreshAccumulator = 0.0f;
	int32 ActiveMoteCount = 0;
	bool bBeamLit = false;
};
