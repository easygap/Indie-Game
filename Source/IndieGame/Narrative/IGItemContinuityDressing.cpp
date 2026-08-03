#include "Narrative/IGItemContinuityDressing.h"

#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/CollisionProfile.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"

namespace IGItemContinuity
{
	struct FProfileShape
	{
		float BottleDiameter = 6.4f;
		float BottleHeight = 20.5f;
		int32 BottleCount = 2;
		FVector BagSize = FVector(12.0f, 18.0f, 20.0f);
		float HandleHeight = 8.0f;
	};

	FProfileShape GetShape(const EIGRebirthPurchaseProfile Profile)
	{
		FProfileShape Shape;
		switch (Profile)
		{
		case EIGRebirthPurchaseProfile::ProfileB1LX1:
			Shape.BottleDiameter = 8.0f;
			Shape.BottleHeight = 28.0f;
			Shape.BottleCount = 1;
			Shape.BagSize = FVector(13.0f, 12.0f, 28.0f);
			Shape.HandleHeight = 10.0f;
			break;
		case EIGRebirthPurchaseProfile::ProfileC2LX2:
			Shape.BottleDiameter = 10.0f;
			Shape.BottleHeight = 36.0f;
			Shape.BottleCount = 2;
			Shape.BagSize = FVector(16.0f, 25.0f, 31.0f);
			Shape.HandleHeight = 15.0f;
			break;
		case EIGRebirthPurchaseProfile::ProfileA500MlX2:
		case EIGRebirthPurchaseProfile::Unset:
		default:
			break;
		}
		return Shape;
	}
}

AIGItemContinuityDressing::AIGItemContinuityDressing()
{
	PrimaryActorTick.bCanEverTick = false;
	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(SceneRoot);
	SetActorEnableCollision(false);
}

void AIGItemContinuityDressing::Configure(
	const EIGItemContinuityPresentation InPresentation,
	const EIGRebirthPurchaseProfile InPurchaseProfile,
	const EIGRebirthBottleClosureState InBottleClosureState,
	UStaticMesh* CubeMesh,
	UStaticMesh* CylinderMesh,
	UMaterialInterface* BagMaterial,
	UMaterialInterface* BottleMaterial,
	UMaterialInterface* WaterMaterial,
	UMaterialInterface* CapMaterial)
{
	if (bConfigured || !CubeMesh || !CylinderMesh)
	{
		return;
	}

	bConfigured = true;
	Presentation = InPresentation;
	PurchaseProfile =
		InPurchaseProfile == EIGRebirthPurchaseProfile::Unset
			? EIGRebirthPurchaseProfile::ProfileA500MlX2
			: InPurchaseProfile;
	BottleClosureState =
		InBottleClosureState == EIGRebirthBottleClosureState::Unset
			? EIGRebirthBottleClosureState::Resealed
			: InBottleClosureState;

	Tags.AddUnique(FName(TEXT("REBIRTH.ItemContinuity")));
	if (Presentation == EIGItemContinuityPresentation::AccidentBag)
	{
		Tags.AddUnique(FName(TEXT("REBIRTH.ItemContinuity.CH01AccidentBag")));
		BuildAccidentBag(
			CubeMesh,
			CylinderMesh,
			BagMaterial,
			BottleMaterial,
			WaterMaterial,
			CapMaterial);
	}
	else
	{
		Tags.AddUnique(FName(TEXT("REBIRTH.ItemContinuity.CH02RecycleSack")));
		BuildLobbyRecycleSack(
			CubeMesh,
			CylinderMesh,
			BagMaterial,
			BottleMaterial,
			WaterMaterial,
			CapMaterial);
	}
}

bool AIGItemContinuityDressing::MatchesContract(
	const EIGItemContinuityPresentation ExpectedPresentation,
	const EIGRebirthPurchaseProfile ExpectedPurchaseProfile,
	const EIGRebirthBottleClosureState ExpectedBottleClosureState) const
{
	const IGItemContinuity::FProfileShape ExpectedShape =
		IGItemContinuity::GetShape(ExpectedPurchaseProfile);
	const int32 ExpectedVisibleBottleCount =
		ExpectedPresentation == EIGItemContinuityPresentation::LobbyRecycleSack
			? 1
			: ExpectedShape.BottleCount;
	const int32 ExpectedCapCount =
		ExpectedBottleClosureState == EIGRebirthBottleClosureState::MissingCap
			? ExpectedVisibleBottleCount - 1
			: ExpectedVisibleBottleCount;
	return bConfigured
		&& Presentation == ExpectedPresentation
		&& PurchaseProfile == ExpectedPurchaseProfile
		&& BottleClosureState == ExpectedBottleClosureState
		&& BottleCount == ExpectedVisibleBottleCount
		&& CapCount == ExpectedCapCount
		&& !GetActorEnableCollision();
}

FTransform AIGItemContinuityDressing::GetCanonicalAccidentTransform(
	const EIGRebirthPurchaseProfile InPurchaseProfile)
{
	const IGItemContinuity::FProfileShape Shape =
		IGItemContinuity::GetShape(InPurchaseProfile);
	FRotator Rotation(0.0f, 20.0f, -8.0f);
	if (InPurchaseProfile == EIGRebirthPurchaseProfile::ProfileC2LX2)
	{
		Rotation = FRotator(78.0f, 20.0f, -8.0f);
	}
	const FQuat RotationQuat = Rotation.Quaternion();
	const float BagHalfHeight =
		FMath::Abs(RotationQuat.GetAxisX().Z) * Shape.BagSize.X * 0.5f
		+ FMath::Abs(RotationQuat.GetAxisY().Z) * Shape.BagSize.Y * 0.5f
		+ FMath::Abs(RotationQuat.GetAxisZ().Z) * Shape.BagSize.Z * 0.5f;
	const FVector StageOrigin(-4800.0f, 4200.0f, 0.0f);
	const FVector LocalLocation(
		1885.0f,
		-350.0f,
		240.0f + BagHalfHeight + 0.5f);
	return FTransform(Rotation, StageOrigin + LocalLocation);
}

UStaticMeshComponent* AIGItemContinuityDressing::AddPart(
	const TCHAR* BaseName,
	UStaticMesh* Mesh,
	UMaterialInterface* Material,
	const FVector& LocalCenter,
	const FVector& SizeCentimeters,
	const FRotator& LocalRotation)
{
	if (!Mesh || !SceneRoot)
	{
		return nullptr;
	}

	const FName PartName(*FString::Printf(TEXT("%s_%02d"), BaseName, PartSerial++));
	UStaticMeshComponent* Part =
		NewObject<UStaticMeshComponent>(this, PartName);
	Part->SetupAttachment(SceneRoot);
	Part->SetStaticMesh(Mesh);
	if (Material)
	{
		Part->SetMaterial(0, Material);
	}
	Part->SetRelativeLocation(LocalCenter);
	Part->SetRelativeRotation(LocalRotation);
	Part->SetRelativeScale3D(SizeCentimeters / 100.0f);
	Part->SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);
	Part->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Part->SetGenerateOverlapEvents(false);
	Part->SetCanEverAffectNavigation(false);
	Part->ComponentTags.AddUnique(FName(TEXT("REBIRTH.ItemContinuity.StaticEvidence")));
	Part->RegisterComponent();
	PresentationParts.Add(Part);
	return Part;
}

void AIGItemContinuityDressing::BuildAccidentBag(
	UStaticMesh* CubeMesh,
	UStaticMesh* CylinderMesh,
	UMaterialInterface* BagMaterial,
	UMaterialInterface* BottleMaterial,
	UMaterialInterface* WaterMaterial,
	UMaterialInterface* CapMaterial)
{
	const IGItemContinuity::FProfileShape Shape =
		IGItemContinuity::GetShape(PurchaseProfile);
	const float Depth = Shape.BagSize.X;
	const float Width = Shape.BagSize.Y;
	const float Height = Shape.BagSize.Z;
	const float Sheet = 0.45f;

	AddPart(
		TEXT("BagSide"),
		CubeMesh,
		BagMaterial,
		FVector(Depth * 0.5f, 0.0f, 0.0f),
		FVector(Sheet, Width, Height));
	AddPart(
		TEXT("BagSide"),
		CubeMesh,
		BagMaterial,
		FVector(-Depth * 0.5f, 0.0f, 0.0f),
		FVector(Sheet, Width, Height));
	AddPart(
		TEXT("BagSide"),
		CubeMesh,
		BagMaterial,
		FVector(0.0f, Width * 0.5f, 0.0f),
		FVector(Depth, Sheet, Height));
	AddPart(
		TEXT("BagSide"),
		CubeMesh,
		BagMaterial,
		FVector(0.0f, -Width * 0.5f, 0.0f),
		FVector(Depth, Sheet, Height));
	AddPart(
		TEXT("BagBase"),
		CubeMesh,
		BagMaterial,
		FVector(0.0f, 0.0f, -Height * 0.5f + Sheet * 0.5f),
		FVector(Depth, Width, Sheet));

	const float HandleHalfWidth = Width * 0.29f;
	for (const float HandleY : {-HandleHalfWidth, HandleHalfWidth})
	{
		AddPart(
			TEXT("BagHandle"),
			CubeMesh,
			BagMaterial,
			FVector(
				0.0f,
				HandleY,
				Height * 0.5f + Shape.HandleHeight * 0.5f),
			FVector(0.9f, 0.8f, Shape.HandleHeight));
	}
	AddPart(
		TEXT("BagHandle"),
		CubeMesh,
		BagMaterial,
		FVector(0.0f, 0.0f, Height * 0.5f + Shape.HandleHeight),
		FVector(0.9f, HandleHalfWidth * 2.0f, 0.8f));

	for (int32 BottleIndex = 0;
		BottleIndex < Shape.BottleCount;
		++BottleIndex)
	{
		const float Lateral =
			Shape.BottleCount == 1
				? 0.0f
				: (BottleIndex == 0 ? -Width * 0.20f : Width * 0.20f);
		AddBottle(
			CylinderMesh,
			BottleMaterial,
			WaterMaterial,
			CapMaterial,
			FVector(0.0f, Lateral, -Height * 0.5f + 0.6f),
			FRotator(0.0f, BottleIndex == 0 ? -5.0f : 6.0f, 0.0f),
			BottleIndex == 0);
	}
}

void AIGItemContinuityDressing::BuildLobbyRecycleSack(
	UStaticMesh* CubeMesh,
	UStaticMesh* CylinderMesh,
	UMaterialInterface* BagMaterial,
	UMaterialInterface* BottleMaterial,
	UMaterialInterface* WaterMaterial,
	UMaterialInterface* CapMaterial)
{
	const FVector SackSize(42.0f, 48.0f, 50.0f);
	const float Sheet = 0.8f;
	AddPart(
		TEXT("RecycleSack"),
		CubeMesh,
		BagMaterial,
		FVector(SackSize.X * 0.5f, 0.0f, SackSize.Z * 0.5f),
		FVector(Sheet, SackSize.Y, SackSize.Z));
	AddPart(
		TEXT("RecycleSack"),
		CubeMesh,
		BagMaterial,
		FVector(-SackSize.X * 0.5f, 0.0f, SackSize.Z * 0.5f),
		FVector(Sheet, SackSize.Y, SackSize.Z));
	AddPart(
		TEXT("RecycleSack"),
		CubeMesh,
		BagMaterial,
		FVector(0.0f, SackSize.Y * 0.5f, SackSize.Z * 0.5f),
		FVector(SackSize.X, Sheet, SackSize.Z));
	AddPart(
		TEXT("RecycleSack"),
		CubeMesh,
		BagMaterial,
		FVector(0.0f, -SackSize.Y * 0.5f, SackSize.Z * 0.5f),
		FVector(SackSize.X, Sheet, SackSize.Z));
	AddPart(
		TEXT("RecycleSackBase"),
		CubeMesh,
		BagMaterial,
		FVector(0.0f, 0.0f, Sheet * 0.5f),
		FVector(SackSize.X, SackSize.Y, Sheet));

	const IGItemContinuity::FProfileShape Shape =
		IGItemContinuity::GetShape(PurchaseProfile);
	const float BottleBaseZ =
		SackSize.Z - Shape.BottleHeight * 0.38f;
	AddBottle(
		CylinderMesh,
		BottleMaterial,
		WaterMaterial,
		CapMaterial,
		FVector(-3.0f, 2.0f, BottleBaseZ),
		FRotator(14.0f, -18.0f, 7.0f),
		true);
}

void AIGItemContinuityDressing::AddBottle(
	UStaticMesh* CylinderMesh,
	UMaterialInterface* BottleMaterial,
	UMaterialInterface* WaterMaterial,
	UMaterialInterface* CapMaterial,
	const FVector& BaseLocation,
	const FRotator& Rotation,
	const bool bPrimaryBottle)
{
	const IGItemContinuity::FProfileShape Shape =
		IGItemContinuity::GetShape(PurchaseProfile);
	const float Diameter = Shape.BottleDiameter;
	const float Height = Shape.BottleHeight;
	const FQuat BottleRotation = Rotation.Quaternion();
	const auto AlongBottle =
		[&BaseLocation, &BottleRotation](const float Distance)
		{
			return BaseLocation
				+ BottleRotation.RotateVector(FVector(0.0f, 0.0f, Distance));
		};
	AddPart(
		TEXT("Bottle"),
		CylinderMesh,
		BottleMaterial,
		AlongBottle(Height * 0.5f),
		FVector(Diameter, Diameter, Height),
		Rotation);
	++BottleCount;

	const float WaterHeight = Height * 0.58f;
	AddPart(
		TEXT("LowWater"),
		CylinderMesh,
		WaterMaterial,
		AlongBottle(1.0f + WaterHeight * 0.5f),
		FVector(Diameter * 0.72f, Diameter * 0.72f, WaterHeight),
		Rotation);

	AddPart(
		TEXT("BrokenSealRing"),
		CylinderMesh,
		CapMaterial,
		AlongBottle(Height - 1.1f),
		FVector(Diameter * 0.74f, Diameter * 0.74f, 0.65f),
		Rotation);

	const bool bMissingThisCap =
		bPrimaryBottle
		&& BottleClosureState == EIGRebirthBottleClosureState::MissingCap;
	if (!bMissingThisCap)
	{
		AddPart(
			TEXT("BottleCap"),
			CylinderMesh,
			CapMaterial,
			AlongBottle(Height + 0.8f),
			FVector(Diameter * 0.78f, Diameter * 0.78f, 1.6f),
			Rotation);
		++CapCount;
	}
}
