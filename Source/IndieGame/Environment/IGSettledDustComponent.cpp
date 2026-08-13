#include "Environment/IGSettledDustComponent.h"

#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/CollisionProfile.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Environment/IGDustSubsystem.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"

namespace IGSettledDust
{
	/** How often the field re-reads the subsystem, in seconds. */
	constexpr float RebuildIntervalSeconds = 0.25f;

	/**
	 * The same authored residue masks the fifth-floor walls use, so a print in
	 * the dust is visibly the same material as the smear on the wall beside it.
	 * Grey-white, rough, non-metallic: it reads only where light falls on it.
	 */
	const TCHAR* FootfallMaterialPath =
		TEXT("/Game/Prototype/Materials/M_MissingFloorHandprints."
			"M_MissingFloorHandprints");
	const TCHAR* DragMaterialPath =
		TEXT("/Game/Prototype/Materials/M_MissingFloorDragTrails."
			"M_MissingFloorDragTrails");

	/**
	 * The engine plane is 100 cm square in XY with no thickness, so a mark's
	 * scale is simply its size in centimetres over a hundred.
	 */
	constexpr float PlaneUnitCentimeters = 100.0f;
}

UIGSettledDustComponent::UIGSettledDustComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;

	Footfalls = CreateMarkLayer(
		this,
		TEXT("SettledDustFootfalls"),
		IGSettledDust::FootfallMaterialPath);
	Drags = CreateMarkLayer(
		this,
		TEXT("SettledDustDrags"),
		IGSettledDust::DragMaterialPath);
}

UInstancedStaticMeshComponent* UIGSettledDustComponent::CreateMarkLayer(
	USceneComponent* Parent,
	const TCHAR* Name,
	const TCHAR* MaterialPath)
{
	UInstancedStaticMeshComponent* Layer =
		NewObject<UInstancedStaticMeshComponent>(Parent, Name);
	if (!Layer)
	{
		return nullptr;
	}
	Layer->SetupAttachment(Parent);
	Layer->SetMobility(EComponentMobility::Movable);
	// Marks belong to the floor, not to this component's transform: absolute
	// space keeps component space equal to world space so the instances stay
	// exactly where they were pressed.
	Layer->SetUsingAbsoluteLocation(true);
	Layer->SetUsingAbsoluteRotation(true);
	Layer->SetUsingAbsoluteScale(true);
	// §11 V2: no shadow, no collision, no decals. The world already computes the
	// light and the contact shadow of the floor underneath; a mark that cast its
	// own would sit on top of the room instead of soaking into it.
	Layer->SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);
	Layer->SetGenerateOverlapEvents(false);
	Layer->SetCanEverAffectNavigation(false);
	Layer->SetCastShadow(false);
	Layer->bAffectDynamicIndirectLighting = false;
	Layer->bAffectDistanceFieldLighting = false;
	Layer->SetReceivesDecals(false);
	Layer->ComponentTags.AddUnique(FName(TEXT("MissingFloor.SettledDust")));

	static ConstructorHelpers::FObjectFinder<UStaticMesh> PlaneFinder(
		TEXT("/Engine/BasicShapes/Plane.Plane"));
	if (PlaneFinder.Succeeded())
	{
		Layer->SetStaticMesh(PlaneFinder.Object);
	}
	ConstructorHelpers::FObjectFinder<UMaterialInterface> MarkMaterial(MaterialPath);
	if (MarkMaterial.Succeeded())
	{
		Layer->SetMaterial(0, MarkMaterial.Object);
	}
	Layer->SetVisibility(false);
	return Layer;
}

void UIGSettledDustComponent::BeginPlay()
{
	Super::BeginPlay();
	if (Footfalls)
	{
		Footfalls->SetWorldTransform(FTransform::Identity);
	}
	if (Drags)
	{
		Drags->SetWorldTransform(FTransform::Identity);
	}
	if (const UWorld* World = GetWorld())
	{
		DustSubsystem = World->GetSubsystem<UIGDustSubsystem>();
	}
}

void UIGSettledDustComponent::ConfigureField(
	const FBox& WorldBoundsXY,
	const float FloorZ)
{
	FieldBounds = WorldBoundsXY;
	FieldFloorZ = FloorZ;
	bFieldConfigured = WorldBoundsXY.IsValid != 0;
	LastPrintCount = -1;
	SetComponentTickEnabled(bFieldConfigured);
	if (Footfalls)
	{
		Footfalls->SetVisibility(bFieldConfigured);
	}
	if (Drags)
	{
		Drags->SetVisibility(bFieldConfigured);
	}
}

bool UIGSettledDustComponent::IsFieldReady() const
{
	return bFieldConfigured && Footfalls != nullptr && Drags != nullptr;
}

void UIGSettledDustComponent::TickComponent(
	const float DeltaSeconds,
	const ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaSeconds, TickType, ThisTickFunction);
	if (!IsFieldReady())
	{
		return;
	}
	RebuildAccumulator += DeltaSeconds;
	if (RebuildAccumulator < IGSettledDust::RebuildIntervalSeconds)
	{
		return;
	}
	RebuildAccumulator = 0.0f;
	RebuildMarks();
}

void UIGSettledDustComponent::RebuildMarks()
{
	if (!DustSubsystem)
	{
		if (const UWorld* World = GetWorld())
		{
			DustSubsystem = World->GetSubsystem<UIGDustSubsystem>();
		}
		if (!DustSubsystem)
		{
			return;
		}
	}

	TArray<FIGDustPrint> Prints;
	DustSubsystem->CollectSettledPrints(Prints);
	// Nothing pressed the dust since the last pass, so nothing needs rebuilding.
	// Standing still in a dark corridor is the common case.
	if (Prints.Num() == LastPrintCount)
	{
		return;
	}
	LastPrintCount = Prints.Num();

	TArray<FTransform> FootfallTransforms;
	TArray<FTransform> DragTransforms;
	FootfallTransforms.Reserve(Prints.Num());
	DragTransforms.Reserve(Prints.Num());
	for (const FIGDustPrint& Print : Prints)
	{
		// The field owns its extent. A print pressed into bare tile was still
		// worth reporting; it simply has no dust to hold it.
		if (Print.Location.X < FieldBounds.Min.X
			|| Print.Location.X > FieldBounds.Max.X
			|| Print.Location.Y < FieldBounds.Min.Y
			|| Print.Location.Y > FieldBounds.Max.Y)
		{
			continue;
		}
		const bool bFootfall = Print.Kind == EIGDustPrintKind::Footfall;
		const FVector Scale = bFootfall
			? FVector(
				FootfallLengthCentimeters / IGSettledDust::PlaneUnitCentimeters,
				FootfallWidthCentimeters / IGSettledDust::PlaneUnitCentimeters,
				1.0f)
			: FVector(
				DragLengthCentimeters / IGSettledDust::PlaneUnitCentimeters,
				DragWidthCentimeters / IGSettledDust::PlaneUnitCentimeters,
				1.0f);
		const FTransform MarkTransform(
			FRotator(0.0f, Print.YawDegrees, 0.0f).Quaternion(),
			FVector(
				Print.Location.X,
				Print.Location.Y,
				FieldFloorZ + SurfaceOffset),
			Scale);
		if (bFootfall)
		{
			FootfallTransforms.Add(MarkTransform);
		}
		else
		{
			DragTransforms.Add(MarkTransform);
		}
	}

	DrawnFootfalls = FootfallTransforms.Num();
	DrawnDrags = DragTransforms.Num();
	// Prints appear and are evicted a handful at a time, so rebuilding the two
	// buffers outright is simpler than tracking instance identity and costs
	// nothing at this count.
	Footfalls->ClearInstances();
	if (DrawnFootfalls > 0)
	{
		Footfalls->AddInstances(FootfallTransforms, false, true);
	}
	Drags->ClearInstances();
	if (DrawnDrags > 0)
	{
		Drags->AddInstances(DragTransforms, false, true);
	}
}
