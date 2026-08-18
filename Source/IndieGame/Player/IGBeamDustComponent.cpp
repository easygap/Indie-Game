#include "Player/IGBeamDustComponent.h"

#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/CollisionProfile.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Environment/IGDustSubsystem.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"

namespace IGBeamDust
{
	/** Deterministic cloud: the same corridor stirs the same way every run. */
	constexpr int32 RandomSeed = 0x0D057;

	/**
	 * Mote size is held roughly constant on screen instead of in the world.
	 * Real dust that small would be sub-pixel past two meters, and a sub-pixel
	 * white speck under TSR does not read as dust — it reads as sensor noise.
	 * A speck about 2.5 px wide at any depth stays a mote and stops shimmering.
	 */
	constexpr float ScreenSizeAtOneMeter = 0.09f;
	constexpr float MinMoteCentimeters = 0.22f;
	constexpr float MaxMoteCentimeters = 1.05f;

	/** Slow, aimless air. Fast dust looks like rain or sparks. */
	constexpr float DriftSpeed = 4.2f;
	/** Plaster dust is heavy; it settles rather than rises. */
	constexpr float SettleSpeed = 1.4f;

	/** Cone margin so motes never sit on the visible edge of the falloff. */
	constexpr float ConeInsetDegrees = 3.0f;

	/** How often the crawled-lane anchors are re-queried, in seconds. */
	constexpr float TrailRefreshSeconds = 0.35f;

	/** Trail motes land within this radius of the sample they belong to. */
	constexpr float TrailScatterRadius = 78.0f;

	/** Below this the beam is browned out and reveals nothing. */
	constexpr float MinRevealingBeamStrength = 0.35f;

	/**
	 * Bright matte plaster. Reused rather than authored: this is exactly the
	 * rough near-white non-metal the motes need, it is already baked, and a
	 * mote is far too small to show which surface the albedo came from.
	 */
	const TCHAR* MoteMaterialPath =
		TEXT("/Game/Prototype/Materials/M_FridgeInterior.M_FridgeInterior");
}

UIGBeamDustComponent::UIGBeamDustComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;

	Motes = CreateDefaultSubobject<UInstancedStaticMeshComponent>(
		TEXT("BeamDustMotes"));
	Motes->SetupAttachment(this);
	Motes->SetMobility(EComponentMobility::Movable);
	// The cloud hangs in the building, not off the end of the torch. Absolute
	// transforms keep component space equal to world space, so the instances
	// stay put while the beam swings across them and their bounds stay tight
	// around the cloud itself.
	Motes->SetUsingAbsoluteLocation(true);
	Motes->SetUsingAbsoluteRotation(true);
	Motes->SetUsingAbsoluteScale(true);
	Motes->SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);
	Motes->SetGenerateOverlapEvents(false);
	Motes->SetCanEverAffectNavigation(false);
	Motes->SetCastShadow(false);
	Motes->bAffectDynamicIndirectLighting = false;
	Motes->bAffectDistanceFieldLighting = false;
	Motes->SetReceivesDecals(false);
	Motes->SetVisibility(false);
	Motes->ComponentTags.AddUnique(FName(TEXT("MissingFloor.BeamDust")));

	static ConstructorHelpers::FObjectFinder<UStaticMesh> MoteMeshFinder(
		TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> MoteMaterialFinder(
		IGBeamDust::MoteMaterialPath);
	if (MoteMeshFinder.Succeeded())
	{
		Motes->SetStaticMesh(MoteMeshFinder.Object);
	}
	if (MoteMaterialFinder.Succeeded())
	{
		Motes->SetMaterial(0, MoteMaterialFinder.Object);
	}
}

void UIGBeamDustComponent::BeginPlay()
{
	Super::BeginPlay();

	Random.Initialize(IGBeamDust::RandomSeed);
	Pool.SetNum(MaxMoteCount);
	InstanceTransforms.SetNum(MaxMoteCount);
	SetBeamCone(OuterConeDegrees);

	if (Motes)
	{
		Motes->SetWorldTransform(FTransform::Identity);
		// One instance per mote for the component's whole life. Parked motes are
		// scaled to nothing instead of removed, so the cloud never reallocates
		// its instance buffer mid-corridor.
		if (Motes->GetInstanceCount() != MaxMoteCount)
		{
			Motes->ClearInstances();
			for (int32 Index = 0; Index < MaxMoteCount; ++Index)
			{
				Motes->AddInstance(FTransform(FVector::ZeroVector), true);
			}
		}
	}
	ClearBeam();
}

void UIGBeamDustComponent::SetBeamCone(const float InOuterConeDegrees)
{
	OuterConeDegrees = FMath::Clamp(InOuterConeDegrees, 4.0f, 80.0f);
	const float InsetDegrees = FMath::Max(
		2.0f,
		OuterConeDegrees - IGBeamDust::ConeInsetDegrees);
	CosOuterCone = FMath::Cos(FMath::DegreesToRadians(InsetDegrees));
}

void UIGBeamDustComponent::ClearBeam()
{
	bBeamLit = false;
	ActiveMoteCount = 0;
	BeamDensityMultiplier = 1.0f;
	TrailAnchors.Reset();
	for (FIGMote& Mote : Pool)
	{
		Mote.bSeeded = false;
	}
	if (Motes)
	{
		Motes->SetVisibility(false);
	}
	SetComponentTickEnabled(false);
}

void UIGBeamDustComponent::UpdateBeam(
	const FVector& Origin,
	const FVector& Direction,
	const float BeamStrength)
{
	if (BeamStrength < IGBeamDust::MinRevealingBeamStrength
		|| !Direction.IsNormalized())
	{
		if (bBeamLit)
		{
			ClearBeam();
		}
		return;
	}

	BeamOrigin = Origin;
	BeamDirection = Direction;
	if (!bBeamLit)
	{
		bBeamLit = true;
		// A fresh switch-on re-seeds, so the first frame of light already has a
		// full cloud in it rather than filling in over the next second.
		TrailRefreshAccumulator = IGBeamDust::TrailRefreshSeconds;
		if (Motes)
		{
			Motes->SetVisibility(true);
		}
		SetComponentTickEnabled(true);
	}
}

void UIGBeamDustComponent::TickComponent(
	const float DeltaSeconds,
	const ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaSeconds, TickType, ThisTickFunction);

	if (!bBeamLit || !Motes || Pool.Num() != MaxMoteCount)
	{
		return;
	}

	const FVector Direction = BeamDirection;
	const FVector Origin = BeamOrigin;
	FVector Right = FVector::CrossProduct(Direction, FVector::UpVector);
	if (!Right.Normalize())
	{
		Right = FVector::CrossProduct(Direction, FVector::ForwardVector);
		Right.Normalize();
	}
	const FVector Up = FVector::CrossProduct(Right, Direction).GetSafeNormal();

	TrailRefreshAccumulator += DeltaSeconds;
	if (TrailRefreshAccumulator >= IGBeamDust::TrailRefreshSeconds)
	{
		TrailRefreshAccumulator = 0.0f;
		RefreshTrailAnchors(Origin, Direction);
	}

	// Extra motes are earned by the lane, not handed out. With no crawled trail
	// in the beam the cloud stays at its ordinary count.
	const float TrailWeight = FMath::Clamp(
		(BeamDensityMultiplier - 1.0f)
			/ (UIGDustSubsystem::MaxDensityMultiplier - 1.0f),
		0.0f,
		1.0f);
	ActiveMoteCount = BaseMoteCount
		+ FMath::RoundToInt(TrailMoteCount * TrailWeight);
	ActiveMoteCount = FMath::Clamp(ActiveMoteCount, 0, MaxMoteCount);

	for (int32 Index = 0; Index < MaxMoteCount; ++Index)
	{
		FIGMote& Mote = Pool[Index];
		if (Index >= ActiveMoteCount)
		{
			Mote.bSeeded = false;
			InstanceTransforms[Index] = FTransform(
				FQuat::Identity,
				FVector::ZeroVector,
				FVector::ZeroVector);
			continue;
		}

		// Motes past the base count belong to the lane. Anchoring them to real
		// trail samples is what makes the doubled density point somewhere.
		const FVector* Anchor = nullptr;
		if (Index >= BaseMoteCount && TrailAnchors.Num() > 0)
		{
			Anchor = &TrailAnchors[Index % TrailAnchors.Num()];
		}

		if (Mote.bSeeded)
		{
			Mote.Location += Mote.Drift * DeltaSeconds;
			Mote.Location.Z -= IGBeamDust::SettleSpeed * DeltaSeconds;
		}
		else
		{
			RespawnMote(Mote, Origin, Direction, Right, Up, Anchor);
		}

		float Depth = 0.0f;
		if (!IsInsideBeam(Mote.Location, Origin, Direction, Depth))
		{
			// Left the cone, either by drifting out or because the beam turned.
			RespawnMote(Mote, Origin, Direction, Right, Up, Anchor);
			IsInsideBeam(Mote.Location, Origin, Direction, Depth);
		}

		const float Meters = FMath::Max(0.35f, Depth * 0.01f);
		const float Size = FMath::Clamp(
			IGBeamDust::ScreenSizeAtOneMeter * Meters * Mote.SizeScale,
			IGBeamDust::MinMoteCentimeters,
			IGBeamDust::MaxMoteCentimeters);
		// 엔진 기본 도형의 지름은 100cm다. 센티미터로 계산한 크기를 그대로 스케일에
		// 넣으면 0.22~1.05cm 분진이 22~105cm 큐브가 된다. 이 큰 인스턴스가 복도를
		// 채우고 손전등으로 확인해야 할 바닥과 벽을 가리고 있었다. 구체 메시로 각진
		// 실루엣을 없애고 100으로 나눠 의도한 실제 지름을 복원한다.
		constexpr float BasicShapeDiameterCentimeters = 100.0f;
		InstanceTransforms[Index] = FTransform(
			FQuat::Identity,
			Mote.Location,
			FVector(Size / BasicShapeDiameterCentimeters));
	}

	// World space rather than component space, even though the two are equal
	// here: if anything ever moves this component, world space stays correct
	// while local space would silently drag the whole cloud along. UE 5.8 marks
	// the touched instances in PrimitiveInstanceDataManager and uploads deltas,
	// and with collision, navigation and shadows all off there is nothing else
	// for the update to walk. Per-instance motion vectors are deliberately not
	// authored: at 4 cm/s a mote moves well under a pixel per frame, and the one
	// frame after a respawn is a 2 px speck.
	Motes->BatchUpdateInstancesTransforms(
		0,
		InstanceTransforms,
		true,
		true,
		true);
}

void UIGBeamDustComponent::RespawnMote(
	FIGMote& Mote,
	const FVector& Origin,
	const FVector& Direction,
	const FVector& Right,
	const FVector& Up,
	const FVector* Anchor)
{
	// An anchor sits inside a slightly widened cone, so scattering 78 cm around
	// it can land outside the strict one. Left unchecked that mote would be
	// rejected and re-placed every single frame — an invisible mote burning a
	// respawn — so fall through to ordinary cone placement when it happens.
	bool bAnchored = false;
	if (Anchor)
	{
		const FVector Candidate = *Anchor + Random.GetUnitVector()
			* (IGBeamDust::TrailScatterRadius * Random.GetFraction());
		float CandidateDepth = 0.0f;
		if (IsInsideBeam(Candidate, Origin, Direction, CandidateDepth))
		{
			Mote.Location = Candidate;
			bAnchored = true;
		}
	}

	if (!bAnchored)
	{
		// Square-rooted depth bias puts more motes in the middle of the throw,
		// where the cone is wide enough to read and the light is still strong.
		const float DepthAlpha = FMath::Sqrt(Random.GetFraction());
		const float Depth = FMath::Lerp(NearDistance, FarDistance, DepthAlpha);
		const float ConeRadius =
			Depth * FMath::Tan(FMath::DegreesToRadians(
				FMath::Max(2.0f, OuterConeDegrees - IGBeamDust::ConeInsetDegrees)));
		const float Angle = Random.GetFraction() * 2.0f * PI;
		const float Radial = ConeRadius * FMath::Sqrt(Random.GetFraction());
		Mote.Location = Origin
			+ Direction * Depth
			+ Right * (FMath::Cos(Angle) * Radial)
			+ Up * (FMath::Sin(Angle) * Radial);
	}

	Mote.Drift = Random.GetUnitVector() * (IGBeamDust::DriftSpeed
		* FMath::Lerp(0.35f, 1.0f, Random.GetFraction()));
	Mote.SizeScale = FMath::Lerp(0.55f, 1.6f, Random.GetFraction());
	Mote.bSeeded = true;
}

bool UIGBeamDustComponent::IsInsideBeam(
	const FVector& Point,
	const FVector& Origin,
	const FVector& Direction,
	float& OutDepth) const
{
	const FVector ToPoint = Point - Origin;
	OutDepth = static_cast<float>(FVector::DotProduct(ToPoint, Direction));
	if (OutDepth < NearDistance || OutDepth > FarDistance)
	{
		return false;
	}
	return FVector::DotProduct(ToPoint.GetSafeNormal(), Direction)
		>= CosOuterCone;
}

void UIGBeamDustComponent::RefreshTrailAnchors(
	const FVector& Origin,
	const FVector& Direction)
{
	TrailAnchors.Reset();
	BeamDensityMultiplier = 1.0f;

	UIGDustSubsystem* Subsystem = ResolveDustSubsystem();
	if (!Subsystem)
	{
		return;
	}

	// Query around the middle of the throw so one sphere covers the whole cone
	// the player can actually see into.
	const float MidDepth = (NearDistance + FarDistance) * 0.5f;
	const FVector Center = Origin + Direction * MidDepth;
	TArray<FIGDustDisturbance> Samples;
	Subsystem->CollectDisturbances(
		Center,
		FarDistance * 0.75f + UIGDustSubsystem::DisturbanceRadius,
		Samples);

	float Strongest = 0.0f;
	for (const FIGDustDisturbance& Sample : Samples)
	{
		const FVector ToSample = Sample.Location - Origin;
		const float Depth = FVector::DotProduct(ToSample, Direction);
		if (Depth < NearDistance - UIGDustSubsystem::DisturbanceRadius
			|| Depth > FarDistance + UIGDustSubsystem::DisturbanceRadius)
		{
			continue;
		}
		if (FVector::DotProduct(ToSample.GetSafeNormal(), Direction)
			< CosOuterCone * 0.82f)
		{
			// Just outside the cone still counts: the dust a stir raised does
			// not stop at the sample point, and a hard edge would pop.
			continue;
		}
		TrailAnchors.Add(Sample.Location);
		Strongest = FMath::Max(Strongest, Sample.Strength);
	}

	if (TrailAnchors.Num() > 0)
	{
		BeamDensityMultiplier = 1.0f
			+ (UIGDustSubsystem::MaxDensityMultiplier - 1.0f)
				* FMath::Clamp(Strongest, 0.0f, 1.0f);
	}
}

UIGDustSubsystem* UIGBeamDustComponent::ResolveDustSubsystem()
{
	if (DustSubsystem)
	{
		return DustSubsystem;
	}
	if (const UWorld* World = GetWorld())
	{
		DustSubsystem = World->GetSubsystem<UIGDustSubsystem>();
	}
	return DustSubsystem;
}
