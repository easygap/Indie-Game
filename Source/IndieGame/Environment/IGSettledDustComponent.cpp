#include "Environment/IGSettledDustComponent.h"

#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/CollisionProfile.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Environment/IGDustSubsystem.h"
#include "GameFramework/Actor.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"

namespace IGSettledDust
{
	/** How often the field re-reads the subsystem, in seconds. */
	constexpr float RebuildIntervalSeconds = 0.25f;
	// 발자국은 발밑, 끌림은 바닥에서 약 40cm 위로 보고된다. 층간 흔적은 제외한다.
	constexpr float FloorReportToleranceCentimeters = 80.0f;

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
	PrimaryComponentTick.TickInterval = IGSettledDust::RebuildIntervalSeconds;

	static ConstructorHelpers::FObjectFinder<UStaticMesh> PlaneFinder(
		TEXT("/Engine/BasicShapes/Plane.Plane"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> FootfallMaterialFinder(
		IGSettledDust::FootfallMaterialPath);
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> DragMaterialFinder(
		IGSettledDust::DragMaterialPath);
	MarkPlaneMesh = PlaneFinder.Object;
	FootfallMaterial = FootfallMaterialFinder.Object;
	DragMaterial = DragMaterialFinder.Object;
}

UInstancedStaticMeshComponent* UIGSettledDustComponent::CreateMarkLayer(
	const TCHAR* Name,
	UMaterialInterface* Material)
{
	AActor* Owner = GetOwner();
	if (!Owner)
	{
		return nullptr;
	}

	// This component's class constructor also runs while its CDO is created.
	// Creating another component there with NewObject asks UE's typed-element
	// registry to represent an object before the registry exists.  Mark layers
	// are runtime instance components instead: their actor owns and registers
	// them after BeginPlay, when the world and registry are both available.
	UInstancedStaticMeshComponent* Layer =
		NewObject<UInstancedStaticMeshComponent>(Owner, Name);
	if (!Layer)
	{
		return nullptr;
	}
	Layer->SetupAttachment(this);
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

	if (MarkPlaneMesh)
	{
		Layer->SetStaticMesh(MarkPlaneMesh);
	}
	if (Material)
	{
		Layer->SetMaterial(0, Material);
	}
	Layer->SetVisibility(false);
	Owner->AddInstanceComponent(Layer);
	Layer->RegisterComponent();
	return Layer;
}

void UIGSettledDustComponent::BeginPlay()
{
	Super::BeginPlay();
	if (!Footfalls)
	{
		Footfalls = CreateMarkLayer(
			TEXT("SettledDustFootfalls"),
			FootfallMaterial);
	}
	if (!Drags)
	{
		Drags = CreateMarkLayer(
			TEXT("SettledDustDrags"),
			DragMaterial);
	}
	if (Footfalls)
	{
		Footfalls->SetWorldTransform(FTransform::Identity);
		Footfalls->SetVisibility(bFieldConfigured);
	}
	if (Drags)
	{
		Drags->SetWorldTransform(FTransform::Identity);
		Drags->SetVisibility(bFieldConfigured);
	}
	SetComponentTickEnabled(bFieldConfigured && Footfalls && Drags);
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
	bPrintsDirty = true;
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

	// 개수만 보면 96개가 찬 뒤의 교체와 같은 자리에서 방향을 튼 흔적을 놓친다.
	// 바뀌지 않은 동안은 배열 복사와 인스턴스 버퍼 갱신도 생략한다.
	const uint64 PrintRevision = DustSubsystem->GetSettledPrintRevision();
	if (!bPrintsDirty && PrintRevision == LastPrintRevision)
	{
		return;
	}
	LastPrintRevision = PrintRevision;
	bPrintsDirty = false;
	TArray<FIGDustPrint> Prints;
	DustSubsystem->CollectSettledPrints(Prints);

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
			|| Print.Location.Y > FieldBounds.Max.Y
			|| FMath::Abs(Print.Location.Z - FieldFloorZ)
				> IGSettledDust::FloorReportToleranceCentimeters)
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
	// 개수가 유지되는 갱신에서는 기존 버퍼를 재사용한다.
	const auto UpdateLayer = [](UInstancedStaticMeshComponent* Layer,
		const TArray<FTransform>& Transforms)
	{
		if (Layer->GetInstanceCount() == Transforms.Num())
		{
			if (!Transforms.IsEmpty())
			{
				Layer->BatchUpdateInstancesTransforms(0, Transforms, true, true, true);
			}
			return;
		}
		Layer->ClearInstances();
		if (!Transforms.IsEmpty())
		{
			Layer->AddInstances(Transforms, false, true);
		}
	};
	UpdateLayer(Footfalls, FootfallTransforms);
	UpdateLayer(Drags, DragTransforms);
}
