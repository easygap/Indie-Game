#include "Interaction/IGElevator.h"

#include "Audio/IGAudioHelpers.h"
#include "Audio/IGToneSequenceSoundWave.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/World.h"
#include "Entity/IGNoiseSubsystem.h"
#include "Engine/CollisionProfile.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Pawn.h"
#include "IndieGame.h"
#include "TimerManager.h"

namespace IGElevator
{
	// Cab interior footprint (X depth into the shaft, Y width) and height.
	constexpr float CabDepth = 150.0f;
	constexpr float CabWidth = 150.0f;
	constexpr float CabHeight = 230.0f;
	constexpr float WallThickness = 10.0f;
	constexpr float DoorHeight = 210.0f;

	// §5.1: 호출 버튼을 누르면 승강로 전체가 한 번 운다. 문 여닫기보다는
	// 작고 소품 집는 것보다는 크다.
	constexpr float CallLoudness = 0.18f;
}

AIGElevator::AIGElevator()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;

	ElevatorRoot = CreateDefaultSubobject<USceneComponent>(TEXT("ElevatorRoot"));
	SetRootComponent(ElevatorRoot);
	ElevatorRoot->SetMobility(EComponentMobility::Static);

	CallButtonMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("CallButton"));
	CallButtonMesh->SetupAttachment(ElevatorRoot);
	CallButtonMesh->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
	CallButtonMesh->SetGenerateOverlapEvents(false);
	CallButtonMesh->SetCanEverAffectNavigation(false);

	LowerCallButtonMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("LowerCallButton"));
	LowerCallButtonMesh->SetupAttachment(ElevatorRoot);
	LowerCallButtonMesh->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
	LowerCallButtonMesh->SetGenerateOverlapEvents(false);
	LowerCallButtonMesh->SetCanEverAffectNavigation(false);

	UpperCabTrigger = CreateDefaultSubobject<UBoxComponent>(TEXT("UpperCabTrigger"));
	UpperCabTrigger->SetupAttachment(ElevatorRoot);
	// Keep the admission volume behind the door plane. A full-cab trigger
	// overlapped the capsule while the player was still outside, so the doors
	// could close on someone who had only approached the threshold.
	// Keep the volume behind the door plane. A separate full-capsule test
	// decides when the rider is actually far enough inside to depart.
	UpperCabTrigger->SetBoxExtent(FVector(48.0f, 62.0f, 105.0f));
	UpperCabTrigger->SetRelativeLocation(FVector(20.0f, 0.0f, 105.0f));
	UpperCabTrigger->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	UpperCabTrigger->SetCollisionResponseToAllChannels(ECR_Ignore);
	UpperCabTrigger->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	UpperCabTrigger->SetGenerateOverlapEvents(true);

	LowerCabTrigger = CreateDefaultSubobject<UBoxComponent>(TEXT("LowerCabTrigger"));
	LowerCabTrigger->SetupAttachment(ElevatorRoot);
	LowerCabTrigger->SetBoxExtent(FVector(48.0f, 62.0f, 105.0f));
	LowerCabTrigger->SetRelativeLocation(FVector(20.0f, 0.0f, -795.0f));
	LowerCabTrigger->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	LowerCabTrigger->SetCollisionResponseToAllChannels(ECR_Ignore);
	LowerCabTrigger->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	LowerCabTrigger->SetGenerateOverlapEvents(true);

	InteractionPrompt = NSLOCTEXT("IGElevator", "CallPrompt", "엘리베이터 호출");
}

UStaticMeshComponent* AIGElevator::MakePiece(
	USceneComponent* Parent,
	UStaticMesh* Mesh,
	UMaterialInterface* Material,
	const FVector& RelativeLocation,
	const FVector& SizeCentimeters,
	const bool bCollide,
	const FRotator& Rotation)
{
	UStaticMeshComponent* Piece = NewObject<UStaticMeshComponent>(
		this,
		*FString::Printf(TEXT("ElevatorPiece_%d"), PieceCounter++));
	Piece->SetupAttachment(Parent);
	Piece->SetStaticMesh(Mesh);
	if (Material) { Piece->SetMaterial(0, Material); }
	Piece->SetRelativeLocation(RelativeLocation);
	Piece->SetRelativeRotation(Rotation);
	Piece->SetRelativeScale3D(SizeCentimeters / 100.0f);
	Piece->SetMobility(Parent->GetMobility());
	Piece->SetGenerateOverlapEvents(false);
	Piece->SetCanEverAffectNavigation(false);
	Piece->SetCollisionProfileName(
		bCollide
			? UCollisionProfile::BlockAll_ProfileName
			: UCollisionProfile::NoCollision_ProfileName);
	Piece->RegisterComponent();
	return Piece;
}

void AIGElevator::BuildCabShell(USceneComponent* Parent, const float BaseZ)
{
	using namespace IGElevator;

	// Cab interior: back (+X), two sides, floor, ceiling; opening faces -X.
	// The rear wall is brushed stainless, not a readable player mirror.
	MakePiece(Parent, CachedCubeMesh, CachedMirrorMaterial,
		FVector(CabDepth * 0.5f + WallThickness * 0.5f, 0, BaseZ + CabHeight * 0.5f),
		FVector(WallThickness, CabWidth + 2 * WallThickness, CabHeight));
	MakePiece(Parent, CachedCubeMesh, CachedCabMaterial,
		FVector(0, -CabWidth * 0.5f - WallThickness * 0.5f, BaseZ + CabHeight * 0.5f),
		FVector(CabDepth, WallThickness, CabHeight));
	MakePiece(Parent, CachedCubeMesh, CachedCabMaterial,
		FVector(0, CabWidth * 0.5f + WallThickness * 0.5f, BaseZ + CabHeight * 0.5f),
		FVector(CabDepth, WallThickness, CabHeight));
	MakePiece(Parent, CachedCubeMesh, CachedFloorMaterial,
		FVector(0, 0, BaseZ - WallThickness * 0.5f),
		FVector(CabDepth, CabWidth, WallThickness));
	MakePiece(Parent, CachedCubeMesh, CachedCabMaterial,
		FVector(0, 0, BaseZ + CabHeight + WallThickness * 0.5f),
		FVector(CabDepth, CabWidth, WallThickness));
	// Frame header above the opening.
	MakePiece(Parent, CachedCubeMesh, CachedCabMaterial,
		FVector(-CabDepth * 0.5f - WallThickness * 0.5f, 0, BaseZ + DoorHeight + (CabHeight - DoorHeight) * 0.5f),
		FVector(WallThickness, CabWidth + 2 * WallThickness, CabHeight - DoorHeight));

	BuildCabInterior(Parent, BaseZ);
}

void AIGElevator::BuildCabInterior(USceneComponent* Parent, const float BaseZ)
{
	using namespace IGElevator;

	const float HalfDepth = CabDepth * 0.5f;
	const float HalfWidth = CabWidth * 0.5f;
	const float CeilingZ = BaseZ + CabHeight;

	// --- Ceiling: two diffuser panels flanking a recessed centre trough. ---
	for (const float PanelY : {-44.0f, 44.0f})
	{
		MakePiece(Parent, CachedCubeMesh, CachedDiffuserMaterial,
			FVector(0, PanelY, CeilingZ - 3.0f), FVector(124, 48, 4), false);
		// Stainless surround so the panel reads as set into the ceiling.
		for (const float EdgeY : {PanelY - 26.0f, PanelY + 26.0f})
		{
			MakePiece(Parent, CachedCubeMesh, CachedCabMaterial,
				FVector(0, EdgeY, CeilingZ - 4.5f), FVector(128, 4, 7), false);
		}
	}
	MakePiece(Parent, CachedCubeMesh, CachedCabMaterial,
		FVector(0, 0, CeilingZ - 4.0f), FVector(128, 24, 8), false);
	MakePiece(Parent, CachedCubeMesh, CachedDiffuserMaterial,
		FVector(0, 0, CeilingZ - 9.0f), FVector(52, 16, 2), false);

	// --- The actual light. Without this the cab was a black box: the mesh
	// strip above was emissive-looking but emitted nothing. ---
	UPointLightComponent* CabLight = NewObject<UPointLightComponent>(
		this, *FString::Printf(TEXT("ElevatorCabLight_%d"), PieceCounter++));
	CabLight->SetupAttachment(Parent);
	CabLight->SetRelativeLocation(FVector(0, 0, CeilingZ - 26.0f));
	// Keep the enamel and handrail values visible instead of flattening the
	// entire small cab to white after auto exposure adapts to the dark hall.
	CabLight->SetIntensity(520.0f);
	CabLight->SetAttenuationRadius(300.0f);
	CabLight->SetLightColor(FLinearColor(0.84f, 0.91f, 1.0f));
	CabLight->SetSourceRadius(46.0f);
	CabLight->SetSoftSourceRadius(70.0f);
	CabLight->SetCastShadows(true);
	// The visible diffuser meshes provide the fixture reflection. Suppress
	// the analytic point source itself so the brushed rear panel does not show
	// a giant circular highlight unrelated to the rectangular ceiling lights.
	CabLight->SetSpecularScale(0.18f);
	CabLight->SetMobility(EComponentMobility::Movable);
	CabLight->SetVolumetricScatteringIntensity(0.12f);
	CabLight->RegisterComponent();

	// Floor bounce so the marble and the rider's feet are not a black hole.
	UPointLightComponent* FloorFill = NewObject<UPointLightComponent>(
		this, *FString::Printf(TEXT("ElevatorCabFill_%d"), PieceCounter++));
	FloorFill->SetupAttachment(Parent);
	FloorFill->SetRelativeLocation(FVector(10.0f, 0, BaseZ + 55.0f));
	FloorFill->SetIntensity(36.0f);
	FloorFill->SetAttenuationRadius(220.0f);
	FloorFill->SetLightColor(FLinearColor(0.90f, 0.94f, 1.0f));
	FloorFill->SetSourceRadius(60.0f);
	FloorFill->SetCastShadows(false);
	FloorFill->SetSpecularScale(0.0f);
	FloorFill->SetMobility(EComponentMobility::Movable);
	FloorFill->RegisterComponent();

	// --- Floor: marble slab with the dark diamond inlay and a border band. ---
	MakePiece(Parent, CachedCubeMesh, CachedInlayMaterial,
		FVector(0, 0, BaseZ + 0.4f), FVector(46, 46, 1), false, FRotator(0, 45, 0));
	MakePiece(Parent, CachedCubeMesh, CachedFloorMaterial,
		FVector(0, 0, BaseZ + 0.8f), FVector(26, 26, 1), false, FRotator(0, 45, 0));
	for (const float BorderX : {-HalfDepth + 12.0f, HalfDepth - 12.0f})
	{
		MakePiece(Parent, CachedCubeMesh, CachedInlayMaterial,
			FVector(BorderX, 0, BaseZ + 0.4f), FVector(3, CabWidth - 24, 1), false);
	}
	for (const float BorderY : {-HalfWidth + 12.0f, HalfWidth - 12.0f})
	{
		MakePiece(Parent, CachedCubeMesh, CachedInlayMaterial,
			FVector(0, BorderY, BaseZ + 0.4f), FVector(CabDepth - 24, 3, 1), false);
	}

	// --- Side walls: kick plate, vertical fin panelling, handrail. ---
	for (const float Side : {-1.0f, 1.0f})
	{
		const float WallY = Side * (HalfWidth - 0.8f);
		MakePiece(Parent, CachedCubeMesh, CachedInlayMaterial,
			FVector(0, WallY, BaseZ + 6.0f), FVector(CabDepth, 1.6f, 12), false);
		// Fin panelling: the louvered strip beside the doors in the reference.
		for (float FinX = -HalfDepth + 12.0f; FinX <= -HalfDepth + 46.0f; FinX += 8.0f)
		{
			MakePiece(Parent, CachedCubeMesh, CachedMirrorMaterial,
				FVector(FinX, WallY - Side * 1.2f, BaseZ + CabHeight * 0.55f),
				FVector(3, 2.4f, CabHeight - 46), false);
		}
		// Handrail: the COP occupies the front of the right wall, so that rail
		// starts farther back instead of crossing the artwork and buttons.
		const bool bCopSide = Side > 0.0f;
		const float RailX = bCopSide ? 22.0f : 6.0f;
		const float RailLength = bCopSide ? 84.0f : CabDepth - 34.0f;
		MakePiece(Parent, CachedCylinderMesh, CachedDoorMaterial,
			FVector(RailX, WallY - Side * 6.0f, BaseZ + 92.0f),
			FVector(4, 4, RailLength), false, FRotator(90, 0, 0));
		const TArray<float> BracketPositions =
			bCopSide ? TArray<float>{5.0f, 54.0f} : TArray<float>{-42.0f, 54.0f};
		for (const float BracketX : BracketPositions)
		{
			MakePiece(Parent, CachedCubeMesh, CachedDoorMaterial,
				FVector(BracketX, WallY - Side * 3.0f, BaseZ + 92.0f),
				FVector(4, 7, 4), false);
		}
	}

	// Rear handrail and the darker brushed inset above it.
	MakePiece(Parent, CachedCylinderMesh, CachedDoorMaterial,
		FVector(HalfDepth - 6.0f, 0, BaseZ + 92.0f),
		FVector(4, 4, CabWidth - 34), false, FRotator(0, 0, 90));
	MakePiece(Parent, CachedCubeMesh, CachedMirrorMaterial,
		FVector(HalfDepth - 1.0f, 0, BaseZ + 152.0f),
		FVector(1.5f, CabWidth - 30, 96), false);
	MakePiece(Parent, CachedCubeMesh, CachedInlayMaterial,
		FVector(HalfDepth - 1.4f, 0, BaseZ + 6.0f),
		FVector(1.6f, CabWidth, 12), false);

	// --- Car operating panel on the rider's right, beside the doors. ---
	// A real car panel is a narrow strip: ~14 cm wide with 2 cm buttons, its
	// top at about 1.5 m. Sizing it off the artwork's aspect instead made the
	// buttons the size of a fist.
	const float CopY = HalfWidth - 1.4f;
	MakePiece(Parent, CachedCubeMesh, CachedCabMaterial,
		FVector(-HalfDepth + 30.0f, CopY, BaseZ + 108.0f),
		FVector(17, 2.0f, 92), false);
	MakePiece(Parent, CachedCubeMesh, CachedCopMaterial,
		FVector(-HalfDepth + 30.0f, CopY - 1.6f, BaseZ + 108.0f),
		FVector(14, 1.2f, 86), false);

	// --- Door jambs in mirror stainless, framing the opening. ---
	for (const float Side : {-1.0f, 1.0f})
	{
		MakePiece(Parent, CachedCubeMesh, CachedMirrorMaterial,
			FVector(-HalfDepth + 3.0f, Side * (HalfWidth - 4.0f), BaseZ + DoorHeight * 0.5f),
			FVector(7, 8, DoorHeight), false);
	}
	MakePiece(Parent, CachedCubeMesh, CachedMirrorMaterial,
		FVector(-HalfDepth + 3.0f, 0, BaseZ + DoorHeight + 4.0f),
		FVector(7, CabWidth, 8), false);
	// Sill track the doors run in.
	MakePiece(Parent, CachedCubeMesh, CachedDoorMaterial,
		FVector(-HalfDepth - 2.0f, 0, BaseZ + 1.0f),
		FVector(14, CabWidth, 2), false);
}

void AIGElevator::ConfigurePrototypeVisuals(
	const FIGElevatorVisuals& Visuals,
	const float InFloorDeltaZ)
{
	if (bVisualsConfigured || !Visuals.CubeMesh)
	{
		return;
	}
	bVisualsConfigured = true;

	using namespace IGElevator;

	CachedCubeMesh = Visuals.CubeMesh;
	CachedCylinderMesh = Visuals.CylinderMesh ? Visuals.CylinderMesh : Visuals.CubeMesh;
	CachedCabMaterial = Visuals.StainlessMaterial;
	CachedMirrorMaterial =
		Visuals.MirrorMaterial ? Visuals.MirrorMaterial : Visuals.StainlessMaterial;
	CachedDoorMaterial =
		Visuals.DoorMaterial ? Visuals.DoorMaterial : Visuals.StainlessMaterial;
	CachedFloorMaterial =
		Visuals.FloorMaterial ? Visuals.FloorMaterial : Visuals.StainlessMaterial;
	CachedInlayMaterial =
		Visuals.InlayMaterial ? Visuals.InlayMaterial : Visuals.StainlessMaterial;
	CachedCopMaterial = Visuals.CopMaterial;
	CachedDiffuserMaterial =
		Visuals.DiffuserMaterial ? Visuals.DiffuserMaterial : Visuals.CopMaterial;
	FloorDeltaZ = FMath::Abs(InFloorDeltaZ);
	DoorPanelWidth = CabWidth * 0.5f;

	const float IntermediateCabBaseZ = GetIntermediateCabBaseZ();
	BuildCabShell(ElevatorRoot, 0.0f);
	BuildCabShell(ElevatorRoot, IntermediateCabBaseZ);
	BuildCabShell(ElevatorRoot, -FloorDeltaZ);

	// Sliding door pairs at the cab opening plane for all authored stops. Each leaf
	// gets a hairline seam and a mirror stile so it reads as a lift door and
	// not a slab of metal.
	for (const float BaseZ : {0.0f, IntermediateCabBaseZ, -FloorDeltaZ})
	{
		constexpr float DoorCenterSeamWidth = 0.16f;
		static_assert(
			DoorCenterSeamWidth > 0.0f
				&& DoorCenterSeamWidth < CabWidth * 0.5f,
			"The lift centre seam must remain narrower than one door leaf.");
		for (const float Side : {-1.0f, 1.0f})
		{
			UStaticMeshComponent* Panel = MakePiece(
				ElevatorRoot, CachedCubeMesh, CachedDoorMaterial,
				FVector(-CabDepth * 0.5f - WallThickness * 0.5f,
					Side * DoorPanelWidth * 0.5f,
					BaseZ + DoorHeight * 0.5f),
				FVector(8, DoorPanelWidth - DoorCenterSeamWidth, DoorHeight));
			Panel->SetMobility(EComponentMobility::Movable);
			// 닫힌 문틈 뒤에는 검은 겹침판이 있다. 실내등이 정면으로 새지 않으며
			// 문이 열릴 때도 문짝과 함께 움직인다.
			UStaticMeshComponent* Seal = MakePiece(Panel, CachedCubeMesh, CachedInlayMaterial,
				FVector(4.2f / .08f, -Side * (DoorPanelWidth * .5f - .45f) / ((DoorPanelWidth - DoorCenterSeamWidth) / 100), 0),
				FVector(.8f, 2.4f, DoorHeight), false);
			Seal->SetAbsolute(false, false, true);
			Seal->SetRelativeScale3D(FVector(.008f, .024f, DoorHeight / 100));
			if (FMath::IsNearlyZero(BaseZ))
			{
				UpperDoorPanels.Add(Panel);
			}
			else if (FMath::IsNearlyEqual(BaseZ, IntermediateCabBaseZ))
			{
				IntermediateDoorPanels.Add(Panel);
			}
			else
			{
				LowerDoorPanels.Add(Panel);
			}
		}
		// 승강장 문선·안쪽 마감·표시판은 여기서만 만든다.
		for (const float Side : {-1.0f, 1.0f})
		{
			MakePiece(ElevatorRoot, CachedCubeMesh, CachedInlayMaterial,
				FVector(-82, Side * 61, BaseZ + 105), FVector(20, 12, 210), false);
		}
		MakePiece(ElevatorRoot, CachedCubeMesh, CachedInlayMaterial,
			FVector(-82, 0, BaseZ + 216), FVector(20, 134, 12), false);
		MakePiece(ElevatorRoot, CachedCubeMesh, CachedInlayMaterial,
			FVector(-90.8f, 0, BaseZ + 230), FVector(1.6f, 32, 16), false);
		MakePiece(ElevatorRoot, CachedCubeMesh, CachedInlayMaterial,
			FVector(-91.65f, 0, BaseZ + 230), FVector(.1f, 28, 14), false);
		UTextRenderComponent* Display = NewObject<UTextRenderComponent>(this);
		Display->SetupAttachment(ElevatorRoot);
		Display->SetMobility(EComponentMobility::Static);
		Display->SetRelativeLocation(FVector(-91.8f, 0, BaseZ + 230));
		Display->SetRelativeRotation(FRotator(0, 180, 0));
		Display->SetHorizontalAlignment(EHTA_Center);
		Display->SetVerticalAlignment(EVRTA_TextCenter);
		Display->SetWorldSize(11);
		Display->SetTextRenderColor(FColor(177, 202, 166));
		Display->SetText(FText::AsNumber(HallFloor));
		Display->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Display->SetCastShadow(false);
		Display->RegisterComponent();
		HallDisplays.Add(Display);
	}

	const auto ConfigureCallPlate = [&](UStaticMeshComponent* Plate, float Y, float BaseZ)
	{
		Plate->SetRelativeLocation(FVector(-90, Y, BaseZ + 108));
		if (Visuals.CallPlateMesh)
		{
			Plate->SetStaticMesh(Visuals.CallPlateMesh);
			Plate->SetRelativeScale3D(FVector::OneVector);
			Plate->SetRelativeRotation(FRotator(0, -90, 0));
		}
		else
		{
			Plate->SetStaticMesh(CachedCubeMesh);
			Plate->SetMaterial(0, CachedCabMaterial);
			Plate->SetRelativeScale3D(FVector(.014f, .09f, .24f));
		}
		// 벽면의 작은 철물이 이동 캡슐을 붙잡지 않게 한다. 조준 판정은 유지한다.
		Plate->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
		Plate->SetMobility(EComponentMobility::Static);
	};
	ConfigureCallPlate(CallButtonMesh, -85, 0);
	ConfigureCallPlate(LowerCallButtonMesh, 78, -FloorDeltaZ);
	LowerCabTrigger->SetRelativeLocation(FVector(20, 0, -FloorDeltaZ + 105));
	if (Visuals.CallPlateMesh)
	{
		MakePiece(ElevatorRoot, Visuals.CallPlateMesh, nullptr,
			FVector(-90, -65, IntermediateCabBaseZ + 108), FVector(100), false, FRotator(0, -90, 0));
	}

}

void AIGElevator::ResetForNewRide()
{
	// Kill anything in flight — a queued StartRide or a chime train would
	// otherwise fire into the new ride and desynchronise the doors.
	GetWorldTimerManager().ClearTimer(RideTimerHandle);
	GetWorldTimerManager().ClearTimer(ChimeTimerHandle);
	GetWorldTimerManager().ClearTimer(AdmissionPollHandle);
	GetWorldTimerManager().ClearTimer(ReturnAdmissionPollHandle);
	SetActorTickEnabled(false);
	PendingRider.Reset();
	ActiveRider.Reset();
	bUpperDoorDepartureStarted = false;

	// Every door pair shuts and the rider is counted as upstairs.
	ApplyDoorOffsets(0.0f, 0.0f, 0.0f);
	DoorAnimation = FIGDoorAnimation();
	ChimeFloor = 3;
	bRideComplete = false;
	SetState(EIGElevatorState::IdleClosed);
}

bool AIGElevator::HasReturnedToFourthFloor() const
{
	return State == EIGElevatorState::ReturnedToFourthFloor;
}

void AIGElevator::RestoreAtLobbyOpen()
{
	GetWorldTimerManager().ClearTimer(RideTimerHandle);
	GetWorldTimerManager().ClearTimer(ChimeTimerHandle);
	GetWorldTimerManager().ClearTimer(AdmissionPollHandle);
	GetWorldTimerManager().ClearTimer(ReturnAdmissionPollHandle);
	SetActorTickEnabled(false);
	PendingRider.Reset();
	ActiveRider.Reset();
	bUpperDoorDepartureStarted = false;

	ApplyDoorOffsets(0.0f, 0.0f, DoorPanelWidth);
	DoorAnimation = FIGDoorAnimation();
	ChimeFloor = 3;
	bRideComplete = true;
	SetState(EIGElevatorState::DoneAtLobby);
}

void AIGElevator::ConfigureIntermediateStop(
	const bool bEnabled,
	const float InDoorGapCentimeters)
{
	bIntermediateStopEnabled = bEnabled;
	IntermediateDoorOffset = FMath::Clamp(
		InDoorGapCentimeters * 0.5f,
		1.0f,
		DoorPanelWidth * 0.45f);
}

bool AIGElevator::IsHoldingAtIntermediateStop() const
{
	return State == EIGElevatorState::HoldingIntermediate;
}

FVector AIGElevator::GetIntermediateCabWorldLocation() const
{
	return GetActorTransform().TransformPosition(
		FVector(0.0f, 0.0f, GetIntermediateCabBaseZ()));
}

bool AIGElevator::ReleaseIntermediateStop()
{
	if (State != EIGElevatorState::HoldingIntermediate)
	{
		return false;
	}

	SetState(EIGElevatorState::ClosingIntermediate);
	DoorAnimation.Begin(IntermediateDoorOffset, 0.0f, DoorSlideDuration);
	SetActorTickEnabled(true);
	return true;
}

void AIGElevator::BeginPlay()
{
	Super::BeginPlay();

	if (!InteractionTag.IsValid())
	{
		InteractionTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Interaction.Door")), false);
	}

	UpperCabTrigger->OnComponentBeginOverlap.AddDynamic(
		this, &ThisClass::HandleCabBeginOverlap);
	UpperCabTrigger->OnComponentEndOverlap.AddDynamic(
		this, &ThisClass::HandleCabEndOverlap);
	LowerCabTrigger->OnComponentBeginOverlap.AddDynamic(
		this, &ThisClass::HandleLowerCabBeginOverlap);
	LowerCabTrigger->OnComponentEndOverlap.AddDynamic(
		this, &ThisClass::HandleLowerCabEndOverlap);
}

bool AIGElevator::CanInteract_Implementation(AActor* Interactor) const
{
	if (!Super::CanInteract_Implementation(Interactor) || !Interactor)
	{
		return false;
	}

	const float LocalZ =
		GetActorTransform().InverseTransformPosition(Interactor->GetActorLocation()).Z;
	const bool bAtLobby = LocalZ < -FloorDeltaZ * 0.5f;
	return bAtLobby
		? State == EIGElevatorState::DoneAtLobby
			|| State == EIGElevatorState::WaitingForReturnRider
			|| State == EIGElevatorState::IdleClosed
			|| State == EIGElevatorState::WaitingForRider
			|| State == EIGElevatorState::ReturnedToFourthFloor
		: State == EIGElevatorState::IdleClosed
			|| State == EIGElevatorState::DoneAtLobby
			|| State == EIGElevatorState::WaitingForReturnRider
			|| State == EIGElevatorState::WaitingForRider
			|| State == EIGElevatorState::ReturnedToFourthFloor;
}

FText AIGElevator::GetInteractionPrompt_Implementation(AActor* Interactor) const
{
	if (Interactor)
	{
		const float LocalZ =
			GetActorTransform().InverseTransformPosition(Interactor->GetActorLocation()).Z;
		if (LocalZ < -FloorDeltaZ * 0.5f)
		{
			if (State != EIGElevatorState::DoneAtLobby
				&& State != EIGElevatorState::WaitingForReturnRider)
			{
				return NSLOCTEXT(
					"IGElevator",
					"CallFromLobbyPrompt",
					"엘리베이터 호출");
			}
			return NSLOCTEXT("IGElevator", "ReturnPrompt", "4층으로 올라가기");
		}
	}
	return NSLOCTEXT("IGElevator", "CallPrompt", "엘리베이터 호출");
}

void AIGElevator::CompleteInteraction_Implementation(const FIGInteractionContext& Context)
{
	Super::CompleteInteraction_Implementation(Context);

	// Pressing a call button is a small, dry click, but it is a sound the one
	// upstairs can walk toward. Reported once here rather than inside SetState,
	// which chapter resets and checkpoint restores also route through.
	if (UWorld* World = GetWorld())
	{
		if (UIGNoiseSubsystem* Noise = World->GetSubsystem<UIGNoiseSubsystem>())
		{
			Noise->ReportNoise(GetActorLocation(), IGElevator::CallLoudness, Context.Interactor);
		}
	}

	const APawn* InteractingPawn = Cast<APawn>(Context.Interactor);
	const float LocalZ = InteractingPawn
		? GetActorTransform().InverseTransformPosition(
			InteractingPawn->GetActorLocation()).Z
		: 0.0f;
	const bool bAtLobby = LocalZ < -FloorDeltaZ * 0.5f;

	// The player is free to mix stairs and lift. When the cab was left at the
	// opposite landing, the hall button first calls an empty cab instead of
	// inheriting an impossible floor state from the route used earlier.
	if (bAtLobby
		&& (State == EIGElevatorState::IdleClosed
			|| State == EIGElevatorState::WaitingForRider
			|| State == EIGElevatorState::ReturnedToFourthFloor))
	{
		IGAudio::SpawnOneShotAt(
			this,
			UIGToneSequenceSoundWave::CreateScannerBeep(this),
			LowerCallButtonMesh->GetComponentLocation(),
			0.5f,
			0.7f);
		StartEmptyCallToLobby();
		return;
	}

	if (bAtLobby
		&& (State == EIGElevatorState::DoneAtLobby
			|| State == EIGElevatorState::WaitingForReturnRider))
	{
		IGAudio::SpawnOneShotAt(
			this,
			UIGToneSequenceSoundWave::CreateScannerBeep(this),
			LowerCallButtonMesh->GetComponentLocation(),
			0.5f,
			0.7f);
		SetState(EIGElevatorState::WaitingForReturnRider);
		GetWorldTimerManager().SetTimer(
			ReturnAdmissionPollHandle,
			this,
			&ThisClass::PollForReturnRider,
			0.08f,
			true);
		PollForReturnRider();
		return;
	}

	if (!bAtLobby
		&& (State == EIGElevatorState::DoneAtLobby
			|| State == EIGElevatorState::WaitingForReturnRider))
	{
		IGAudio::SpawnOneShotAt(
			this,
			UIGToneSequenceSoundWave::CreateScannerBeep(this),
			CallButtonMesh->GetComponentLocation(),
			0.5f,
			0.7f);
		StartEmptyCallToFourthFloor();
		return;
	}

	if (!bAtLobby
		&& (State == EIGElevatorState::ReturnedToFourthFloor
			|| State == EIGElevatorState::WaitingForRider))
	{
		bRideComplete = false;
		SetState(EIGElevatorState::WaitingForRider);
		GetWorldTimerManager().SetTimer(
			AdmissionPollHandle,
			this,
			&ThisClass::PollForRider,
			0.08f,
			true);
		PollForRider();
		return;
	}

	if (bAtLobby || State != EIGElevatorState::IdleClosed)
	{
		return;
	}

	IGAudio::SpawnOneShotAt(
		this,
		UIGToneSequenceSoundWave::CreateScannerBeep(this),
		CallButtonMesh->GetComponentLocation(),
		0.5f,
		0.7f);
	SetState(EIGElevatorState::OpeningUpper);
	DoorAnimation.Begin(0.0f, DoorPanelWidth, DoorSlideDuration);
	SetActorTickEnabled(true);
}

void AIGElevator::StartEmptyCallToLobby()
{
	GetWorldTimerManager().ClearTimer(RideTimerHandle);
	GetWorldTimerManager().ClearTimer(ChimeTimerHandle);
	GetWorldTimerManager().ClearTimer(AdmissionPollHandle);
	GetWorldTimerManager().ClearTimer(ReturnAdmissionPollHandle);
	PendingRider.Reset();
	ActiveRider.Reset();
	bUpperDoorDepartureStarted = false;
	bRideComplete = false;

	if (State == EIGElevatorState::WaitingForRider
		|| State == EIGElevatorState::ReturnedToFourthFloor)
	{
		SetState(EIGElevatorState::ClosingUpperForEmptyCall);
		DoorAnimation.Begin(DoorPanelWidth, 0.0f, DoorSlideDuration);
		SetActorTickEnabled(true);
		return;
	}

	StartEmptyTravelToLobby();
}

void AIGElevator::StartEmptyCallToFourthFloor()
{
	GetWorldTimerManager().ClearTimer(RideTimerHandle);
	GetWorldTimerManager().ClearTimer(ChimeTimerHandle);
	GetWorldTimerManager().ClearTimer(AdmissionPollHandle);
	GetWorldTimerManager().ClearTimer(ReturnAdmissionPollHandle);
	PendingRider.Reset();
	ActiveRider.Reset();
	bUpperDoorDepartureStarted = false;
	bRideComplete = false;

	SetState(EIGElevatorState::ClosingLowerForEmptyCall);
	DoorAnimation.Begin(DoorPanelWidth, 0.0f, DoorSlideDuration);
	SetActorTickEnabled(true);
}

void AIGElevator::StartEmptyTravelToLobby()
{
	ApplyDoorOffsets(0.0f, 0.0f, 0.0f);
	SetState(EIGElevatorState::EmptyDescending);
	const float TravelSeconds = SecondsPerFloor * 3.0f;
	PlayEmptyTravelHum(false);
	GetWorldTimerManager().SetTimer(
		RideTimerHandle,
		this,
		&ThisClass::BeginOpeningLowerAfterEmptyCall,
		TravelSeconds,
		false);
}

void AIGElevator::StartEmptyTravelToFourthFloor()
{
	ApplyDoorOffsets(0.0f, 0.0f, 0.0f);
	SetState(EIGElevatorState::EmptyAscending);
	const float TravelSeconds = SecondsPerFloor * 3.0f;
	PlayEmptyTravelHum(true);
	GetWorldTimerManager().SetTimer(
		RideTimerHandle,
		this,
		&ThisClass::BeginOpeningUpperAfterEmptyCall,
		TravelSeconds,
		false);
}

void AIGElevator::BeginOpeningLowerAfterEmptyCall()
{
	if (State != EIGElevatorState::EmptyDescending)
	{
		return;
	}

	IGAudio::SpawnOneShotAt(
		this,
		UIGToneSequenceSoundWave::CreateDoorChime(this),
		LowerCallButtonMesh->GetComponentLocation(),
		0.55f);
	SetState(EIGElevatorState::OpeningLowerForReturn);
	DoorAnimation.Begin(0.0f, DoorPanelWidth, DoorSlideDuration);
	SetActorTickEnabled(true);
}

void AIGElevator::BeginOpeningUpperAfterEmptyCall()
{
	if (State != EIGElevatorState::EmptyAscending)
	{
		return;
	}

	IGAudio::SpawnOneShotAt(
		this,
		UIGToneSequenceSoundWave::CreateDoorChime(this),
		CallButtonMesh->GetComponentLocation(),
		0.55f);
	// The caller already reached 4F by stairs, so this hall-call opening must
	// not emit the physical-rider-arrival delegate.
	SetState(EIGElevatorState::OpeningUpper);
	DoorAnimation.Begin(0.0f, DoorPanelWidth, DoorSlideDuration);
	SetActorTickEnabled(true);
}

void AIGElevator::PlayEmptyTravelHum(const bool bAscending)
{
	TArray<FIGToneNote> HumNotes;
	const float TravelSeconds = SecondsPerFloor * 3.0f;
	HumNotes.Add({0.0f, TravelSeconds, 38.0f, 0.13f, 0.15f, 0.6f, EIGToneWaveform::Sine});
	HumNotes.Add({0.0f, TravelSeconds, 76.0f, 0.05f, 0.15f, 0.6f, EIGToneWaveform::Sine});
	HumNotes.Add({0.0f, TravelSeconds, 240.0f, 0.018f, 0.20f, 0.8f, EIGToneWaveform::ValueNoise});
	UIGToneSequenceSoundWave* Hum = NewObject<UIGToneSequenceSoundWave>(this);
	Hum->ConfigureNotes(MoveTemp(HumNotes), false);
	const FVector ShaftSoundLocation = GetActorLocation()
		+ FVector(
			0.0f,
			0.0f,
			bAscending ? -FloorDeltaZ * 0.45f : -FloorDeltaZ * 0.55f);
	IGAudio::SpawnOneShotAt(
		this,
		Hum,
		ShaftSoundLocation,
		0.72f,
		1.0f,
		180.0f,
		950.0f);
}

void AIGElevator::HandleCabBeginOverlap(
	UPrimitiveComponent* OverlappedComponent,
	AActor* OtherActor,
	UPrimitiveComponent* OtherComponent,
	int32 OtherBodyIndex,
	bool bFromSweep,
	const FHitResult& SweepResult)
{
	TryAdmitRider(Cast<APawn>(OtherActor));
}

void AIGElevator::HandleCabEndOverlap(
	UPrimitiveComponent* OverlappedComponent,
	AActor* OtherActor,
	UPrimitiveComponent* OtherComponent,
	int32 OtherBodyIndex)
{
	APawn* Pawn = Cast<APawn>(OtherActor);
	if (!Pawn
		|| PendingRider.Get() != Pawn
		|| State != EIGElevatorState::ClosingUpper)
	{
		return;
	}

	// The rider backed out either during the settle beat or while the panels
	// were already closing. Child-component door motion cannot sweep, so abort
	// the departure and reopen instead of teleporting an outside pawn.
	GetWorldTimerManager().ClearTimer(RideTimerHandle);
	PendingRider.Reset();
	if (bUpperDoorDepartureStarted)
	{
		bUpperDoorDepartureStarted = false;
		SetState(EIGElevatorState::OpeningUpper);
		DoorAnimation.Begin(
			DoorAnimation.CurrentValue,
			DoorPanelWidth,
			FMath::Max(0.25f, DoorSlideDuration * 0.65f));
		SetActorTickEnabled(true);
	}
	else
	{
		SetState(EIGElevatorState::WaitingForRider);
		GetWorldTimerManager().SetTimer(
			AdmissionPollHandle,
			this,
			&ThisClass::PollForRider,
			0.08f,
			true);
	}
}

void AIGElevator::HandleLowerCabBeginOverlap(
	UPrimitiveComponent* OverlappedComponent,
	AActor* OtherActor,
	UPrimitiveComponent* OtherComponent,
	int32 OtherBodyIndex,
	bool bFromSweep,
	const FHitResult& SweepResult)
{
	TryAdmitReturnRider(Cast<APawn>(OtherActor));
}

void AIGElevator::HandleLowerCabEndOverlap(
	UPrimitiveComponent* OverlappedComponent,
	AActor* OtherActor,
	UPrimitiveComponent* OtherComponent,
	int32 OtherBodyIndex)
{
	APawn* Pawn = Cast<APawn>(OtherActor);
	if (!Pawn
		|| PendingRider.Get() != Pawn
		|| State != EIGElevatorState::ClosingLowerForReturn)
	{
		return;
	}

	GetWorldTimerManager().ClearTimer(RideTimerHandle);
	PendingRider.Reset();
	SetState(EIGElevatorState::OpeningLowerForReturn);
	DoorAnimation.Begin(
		DoorAnimation.CurrentValue,
		DoorPanelWidth,
		FMath::Max(0.25f, DoorSlideDuration * 0.65f));
	SetActorTickEnabled(true);
}

void AIGElevator::PollForRider()
{
	if (State != EIGElevatorState::WaitingForRider)
	{
		GetWorldTimerManager().ClearTimer(AdmissionPollHandle);
		return;
	}

	TArray<AActor*> OverlappingActors;
	UpperCabTrigger->GetOverlappingActors(OverlappingActors, APawn::StaticClass());
	for (AActor* Actor : OverlappingActors)
	{
		if (APawn* Pawn = Cast<APawn>(Actor))
		{
			TryAdmitRider(Pawn);
			if (State == EIGElevatorState::ClosingUpper)
			{
				return;
			}
		}
	}
}

void AIGElevator::PollForReturnRider()
{
	if (State != EIGElevatorState::WaitingForReturnRider)
	{
		GetWorldTimerManager().ClearTimer(ReturnAdmissionPollHandle);
		return;
	}

	TArray<AActor*> OverlappingActors;
	LowerCabTrigger->GetOverlappingActors(OverlappingActors, APawn::StaticClass());
	for (AActor* Actor : OverlappingActors)
	{
		if (APawn* Pawn = Cast<APawn>(Actor))
		{
			TryAdmitReturnRider(Pawn);
			if (State == EIGElevatorState::ClosingLowerForReturn)
			{
				return;
			}
		}
	}
}

void AIGElevator::TryAdmitRider(APawn* Pawn)
{
	if (!Pawn
		|| !Pawn->IsPlayerControlled()
		|| State != EIGElevatorState::WaitingForRider
		|| !UpperCabTrigger->IsOverlappingActor(Pawn)
		|| !IsRiderSafelyInsideUpperCab(Pawn))
	{
		return;
	}

	// Give the rider a beat to settle. End-overlap cancels this timer, so
	// merely grazing the volume can no longer start a phantom ride.
	GetWorldTimerManager().ClearTimer(AdmissionPollHandle);
	PendingRider = Pawn;
	bUpperDoorDepartureStarted = false;
	SetState(EIGElevatorState::ClosingUpper);
	GetWorldTimerManager().SetTimer(
		RideTimerHandle,
		this,
		&ThisClass::StartRide,
		1.1f,
		false);
}

void AIGElevator::TryAdmitReturnRider(APawn* Pawn)
{
	if (!Pawn
		|| !Pawn->IsPlayerControlled()
		|| State != EIGElevatorState::WaitingForReturnRider
		|| !LowerCabTrigger->IsOverlappingActor(Pawn)
		|| !IsRiderSafelyInsideLowerCab(Pawn))
	{
		return;
	}

	GetWorldTimerManager().ClearTimer(ReturnAdmissionPollHandle);
	PendingRider = Pawn;
	SetState(EIGElevatorState::ClosingLowerForReturn);
	GetWorldTimerManager().SetTimer(
		RideTimerHandle,
		this,
		&ThisClass::StartReturnRide,
		1.1f,
		false);
}

bool AIGElevator::IsRiderSafelyInsideUpperCab(const APawn* Pawn) const
{
	if (!Pawn)
	{
		return false;
	}

	float RiderRadius = 36.0f;
	if (const ACharacter* Character = Cast<ACharacter>(Pawn))
	{
		if (const UCapsuleComponent* Capsule = Character->GetCapsuleComponent())
		{
			RiderRadius = Capsule->GetScaledCapsuleRadius();
		}
	}

	const FVector LocalLocation =
		GetActorTransform().InverseTransformPosition(Pawn->GetActorLocation());
	constexpr float InteriorClearance = 2.0f;
	const float HalfDepth = IGElevator::CabDepth * 0.5f;
	const float HalfWidth = IGElevator::CabWidth * 0.5f;
	return LocalLocation.X - RiderRadius >= -HalfDepth + InteriorClearance
		&& LocalLocation.X + RiderRadius <= HalfDepth - InteriorClearance
		&& FMath::Abs(LocalLocation.Y) + RiderRadius
			<= HalfWidth - InteriorClearance;
}

bool AIGElevator::IsRiderSafelyInsideLowerCab(const APawn* Pawn) const
{
	if (!Pawn)
	{
		return false;
	}

	float RiderRadius = 36.0f;
	if (const ACharacter* Character = Cast<ACharacter>(Pawn))
	{
		if (const UCapsuleComponent* Capsule = Character->GetCapsuleComponent())
		{
			RiderRadius = Capsule->GetScaledCapsuleRadius();
		}
	}

	FVector LocalLocation =
		GetActorTransform().InverseTransformPosition(Pawn->GetActorLocation());
	LocalLocation.Z += FloorDeltaZ;
	constexpr float InteriorClearance = 2.0f;
	const float HalfDepth = IGElevator::CabDepth * 0.5f;
	const float HalfWidth = IGElevator::CabWidth * 0.5f;
	return LocalLocation.X - RiderRadius >= -HalfDepth + InteriorClearance
		&& LocalLocation.X + RiderRadius <= HalfDepth - InteriorClearance
		&& FMath::Abs(LocalLocation.Y) + RiderRadius
			<= HalfWidth - InteriorClearance;
}

float AIGElevator::GetIntermediateCabBaseZ() const
{
	// Four storeys use 300 cm floor-to-floor spacing: 4F=900, 2F=300, 1F=0.
	return -FloorDeltaZ * (2.0f / 3.0f);
}

bool AIGElevator::TransferRiderToCab(APawn* Rider, const float CabBaseZ)
{
	if (!Rider)
	{
		return false;
	}

	const FTransform ElevatorTransform = GetActorTransform();
	FVector LocalRider = ElevatorTransform.InverseTransformPosition(
		Rider->GetActorLocation());

	float RiderRadius = 36.0f;
	float RiderHalfHeight = 96.0f;
	UCharacterMovementComponent* Movement = nullptr;
	if (ACharacter* Character = Cast<ACharacter>(Rider))
	{
		if (const UCapsuleComponent* Capsule = Character->GetCapsuleComponent())
		{
			RiderRadius = Capsule->GetScaledCapsuleRadius();
			RiderHalfHeight = Capsule->GetScaledCapsuleHalfHeight();
		}
		Movement = Character->GetCharacterMovement();
	}

	// Preserve where the player stood inside the car, but never preserve an
	// accumulated falling offset. The destination is derived from the visible
	// floor plane and the actual capsule dimensions every time.
	constexpr float WallClearance = 3.0f;
	constexpr float FloorClearance = 2.0f;
	const float MaxLocalX =
		IGElevator::CabDepth * 0.5f - RiderRadius - WallClearance;
	const float MaxLocalY =
		IGElevator::CabWidth * 0.5f - RiderRadius - WallClearance;
	LocalRider.X = FMath::Clamp(LocalRider.X, -MaxLocalX, MaxLocalX);
	LocalRider.Y = FMath::Clamp(LocalRider.Y, -MaxLocalY, MaxLocalY);
	LocalRider.Z = CabBaseZ + RiderHalfHeight + FloorClearance;

	if (Movement)
	{
		Movement->StopMovementImmediately();
		Movement->DisableMovement();
	}

	const FVector Destination = ElevatorTransform.TransformPosition(LocalRider);
	const bool bTransferred = Rider->SetActorLocation(
		Destination,
		false,
		nullptr,
		ETeleportType::TeleportPhysics);

	if (Movement)
	{
		Movement->SetMovementMode(MOVE_Walking);
	}

	if (!bTransferred)
	{
		UE_LOG(
			LogIndieGame,
			Error,
			TEXT("Elevator refused safe rider transfer to cab base Z %.1f."),
			CabBaseZ);
		return false;
	}

	UE_LOG(
		LogIndieGame,
		Verbose,
		TEXT("Elevator rider transferred behind closed doors to %s."),
		*Destination.ToCompactString());
	return true;
}

void AIGElevator::StartRide()
{
	GetWorldTimerManager().ClearTimer(AdmissionPollHandle);
	APawn* Rider = PendingRider.Get();
	if (!Rider
		|| !UpperCabTrigger->IsOverlappingActor(Rider)
		|| !IsRiderSafelyInsideUpperCab(Rider))
	{
		PendingRider.Reset();
		bUpperDoorDepartureStarted = false;
		SetState(EIGElevatorState::WaitingForRider);
		GetWorldTimerManager().SetTimer(
			AdmissionPollHandle,
			this,
			&ThisClass::PollForRider,
			0.08f,
			true);
		return;
	}

	bUpperDoorDepartureStarted = true;
	DoorAnimation.Begin(DoorPanelWidth, 0.0f, DoorSlideDuration);
	SetActorTickEnabled(true);
}

void AIGElevator::StartReturnRide()
{
	APawn* Rider = PendingRider.Get();
	if (!Rider
		|| !LowerCabTrigger->IsOverlappingActor(Rider)
		|| !IsRiderSafelyInsideLowerCab(Rider))
	{
		PendingRider.Reset();
		SetState(EIGElevatorState::WaitingForReturnRider);
		GetWorldTimerManager().SetTimer(
			ReturnAdmissionPollHandle,
			this,
			&ThisClass::PollForReturnRider,
			0.08f,
			true);
		return;
	}

	DoorAnimation.Begin(DoorPanelWidth, 0.0f, DoorSlideDuration);
	SetActorTickEnabled(true);
}

void AIGElevator::BeginOpeningUpperAfterReturn()
{
	const FVector CabSoundLocation = ActiveRider.IsValid()
		? ActiveRider->GetActorLocation() + FVector(0, 0, 54.0f)
		: GetActorLocation() + FVector(0, 0, 150.0f);
	IGAudio::SpawnOneShotAt(
		this,
		UIGToneSequenceSoundWave::CreateDoorChime(this),
		CabSoundLocation,
		0.55f);
	SetState(EIGElevatorState::OpeningUpperForReturn);
	DoorAnimation.Begin(0.0f, DoorPanelWidth, DoorSlideDuration);
	SetActorTickEnabled(true);
}

void AIGElevator::Tick(const float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	const bool bFinished = DoorAnimation.Advance(DeltaSeconds);
	switch (State)
	{
	case EIGElevatorState::OpeningUpper:
	case EIGElevatorState::ClosingUpper:
	case EIGElevatorState::ClosingUpperForEmptyCall:
	case EIGElevatorState::OpeningUpperForReturn:
		ApplyDoorOffsets(DoorAnimation.CurrentValue, 0.0f, 0.0f);
		break;
	case EIGElevatorState::OpeningIntermediate:
	case EIGElevatorState::ClosingIntermediate:
		ApplyDoorOffsets(0.0f, DoorAnimation.CurrentValue, 0.0f);
		break;
	case EIGElevatorState::OpeningLower:
	case EIGElevatorState::ClosingLowerForReturn:
	case EIGElevatorState::ClosingLowerForEmptyCall:
	case EIGElevatorState::OpeningLowerForReturn:
		ApplyDoorOffsets(0.0f, 0.0f, DoorAnimation.CurrentValue);
		break;
	default:
		break;
	}

	if (!bFinished)
	{
		return;
	}
	SetActorTickEnabled(false);

	switch (State)
	{
	case EIGElevatorState::OpeningUpper:
		SetState(EIGElevatorState::WaitingForRider);
		// A quick player can enter while the doors are still opening. That
		// overlap may already have fired. Poll while the doors wait so a
		// capsule that crossed the volume edge too early is admitted as soon
		// as its full body clears the threshold.
		GetWorldTimerManager().SetTimer(
			AdmissionPollHandle,
			this,
			&ThisClass::PollForRider,
			0.08f,
			true);
		PollForRider();
		break;

	case EIGElevatorState::ClosingUpper:
	{
		APawn* Rider = PendingRider.Get();
		if (!Rider
			|| !UpperCabTrigger->IsOverlappingActor(Rider)
			|| !IsRiderSafelyInsideUpperCab(Rider))
		{
			// End-overlap normally catches this, but revalidate at the exact
			// commit point as protection against teleports and missed overlap
			// notifications during the closing animation.
			PendingRider.Reset();
			bUpperDoorDepartureStarted = false;
			SetState(EIGElevatorState::OpeningUpper);
			DoorAnimation.Begin(0.0f, DoorPanelWidth, DoorSlideDuration);
			SetActorTickEnabled(true);
			break;
		}

		SetState(EIGElevatorState::Descending);

		// Move only the pawn that actually entered this cab. The technical
		// transfer happens behind fully shut, identical doors. A CH02 ride
		// first lands in the physical 2F shell; a normal ride lands at 1F.
		const float DestinationCabBaseZ =
			bIntermediateStopEnabled ? GetIntermediateCabBaseZ() : -FloorDeltaZ;
		if (!TransferRiderToCab(Rider, DestinationCabBaseZ))
		{
			PendingRider.Reset();
			bUpperDoorDepartureStarted = false;
			SetState(EIGElevatorState::OpeningUpper);
			DoorAnimation.Begin(0.0f, DoorPanelWidth, DoorSlideDuration);
			SetActorTickEnabled(true);
			break;
		}
		ActiveRider = Rider;
		PendingRider.Reset();
		bUpperDoorDepartureStarted = false;

		// Machinery hum for the whole descent.
		TArray<FIGToneNote> HumNotes;
		const float RideSeconds = SecondsPerFloor * 3.0f + 1.2f;
		HumNotes.Add({0.0f, RideSeconds, 38.0f, 0.16f, 0.15f, 0.6f, EIGToneWaveform::Sine});
		HumNotes.Add({0.0f, RideSeconds, 76.0f, 0.06f, 0.15f, 0.6f, EIGToneWaveform::Sine});
		HumNotes.Add({0.0f, RideSeconds, 240.0f, 0.02f, 0.20f, 0.8f, EIGToneWaveform::ValueNoise});
		UIGToneSequenceSoundWave* Hum = NewObject<UIGToneSequenceSoundWave>(this);
		Hum->ConfigureNotes(MoveTemp(HumNotes), false);
		IGAudio::SpawnOneShotAt(
			this, Hum,
			GetActorLocation() - FVector(0, 0, FloorDeltaZ - 120.0f),
			0.9f, 1.0f, 200.0f, 900.0f);

		ChimeFloor = 3;
		GetWorldTimerManager().SetTimer(
			ChimeTimerHandle,
			this,
			&ThisClass::HandleFloorChime,
			SecondsPerFloor,
			true);
		break;
	}

	case EIGElevatorState::ClosingUpperForEmptyCall:
		StartEmptyTravelToLobby();
		break;

	case EIGElevatorState::OpeningIntermediate:
		SetState(EIGElevatorState::HoldingIntermediate);
		OnIntermediateStopOpened.Broadcast(this);
		break;

	case EIGElevatorState::ClosingIntermediate:
		if (!TransferRiderToCab(ActiveRider.Get(), -FloorDeltaZ))
		{
			UE_LOG(
				LogIndieGame,
				Error,
				TEXT("Elevator could not continue from the 2F stop."));
			SetState(EIGElevatorState::HoldingIntermediate);
			break;
		}
		SetState(EIGElevatorState::Descending);
		GetWorldTimerManager().SetTimer(
			ChimeTimerHandle,
			this,
			&ThisClass::HandleFloorChime,
			SecondsPerFloor,
			true);
		break;

	case EIGElevatorState::OpeningLower:
		SetState(EIGElevatorState::DoneAtLobby);
		bRideComplete = true;
		OnArrivedAtLobby.Broadcast(this);
		ActiveRider.Reset();
		break;

	case EIGElevatorState::ClosingLowerForReturn:
	{
		APawn* Rider = PendingRider.Get();
		if (!Rider
			|| !LowerCabTrigger->IsOverlappingActor(Rider)
			|| !IsRiderSafelyInsideLowerCab(Rider))
		{
			PendingRider.Reset();
			SetState(EIGElevatorState::OpeningLowerForReturn);
			DoorAnimation.Begin(0.0f, DoorPanelWidth, DoorSlideDuration);
			SetActorTickEnabled(true);
			break;
		}

		// Commit the state before teleporting. SetActorLocation can synchronously
		// emit LowerCabTrigger end-overlap; leaving PendingRider latched here
		// would let that callback mistake a valid closed-door transfer for an
		// aborted boarding attempt.
		PendingRider.Reset();
		SetState(EIGElevatorState::Ascending);
		if (!TransferRiderToCab(Rider, 0.0f))
		{
			SetState(EIGElevatorState::OpeningLowerForReturn);
			DoorAnimation.Begin(0.0f, DoorPanelWidth, DoorSlideDuration);
			SetActorTickEnabled(true);
			break;
		}

		ActiveRider = Rider;

		TArray<FIGToneNote> HumNotes;
		const float RideSeconds = SecondsPerFloor * 3.0f;
		HumNotes.Add({0.0f, RideSeconds, 38.0f, 0.16f, 0.15f, 0.6f, EIGToneWaveform::Sine});
		HumNotes.Add({0.0f, RideSeconds, 76.0f, 0.06f, 0.15f, 0.6f, EIGToneWaveform::Sine});
		HumNotes.Add({0.0f, RideSeconds, 240.0f, 0.02f, 0.20f, 0.8f, EIGToneWaveform::ValueNoise});
		UIGToneSequenceSoundWave* Hum = NewObject<UIGToneSequenceSoundWave>(this);
		Hum->ConfigureNotes(MoveTemp(HumNotes), false);
		IGAudio::SpawnOneShotAt(
			this, Hum,
			GetActorLocation() + FVector(0, 0, 120.0f),
			0.9f, 1.0f, 200.0f, 900.0f);

		GetWorldTimerManager().SetTimer(
			RideTimerHandle,
			this,
			&ThisClass::BeginOpeningUpperAfterReturn,
			RideSeconds,
			false);
		break;
	}

	case EIGElevatorState::ClosingLowerForEmptyCall:
		StartEmptyTravelToFourthFloor();
		break;

	case EIGElevatorState::OpeningLowerForReturn:
		SetState(EIGElevatorState::WaitingForReturnRider);
		GetWorldTimerManager().SetTimer(
			ReturnAdmissionPollHandle,
			this,
			&ThisClass::PollForReturnRider,
			0.08f,
			true);
		PollForReturnRider();
		break;

	case EIGElevatorState::OpeningUpperForReturn:
		SetState(EIGElevatorState::ReturnedToFourthFloor);
		bRideComplete = false;
		ActiveRider.Reset();
		OnReturnedToFourthFloor.Broadcast(this);
		break;

	default:
		break;
	}
}

void AIGElevator::HandleFloorChime()
{
	const FVector CabSoundLocation = ActiveRider.IsValid()
		? ActiveRider->GetActorLocation() + FVector(0, 0, 54.0f)
		: GetActorLocation() - FVector(0, 0, FloorDeltaZ - 150.0f);
	IGAudio::SpawnOneShotAt(
		this,
		UIGToneSequenceSoundWave::CreateScannerBeep(this),
		CabSoundLocation,
		0.35f,
		ChimeFloor == 1 ? 1.0f : 0.62f);

	if (bIntermediateStopEnabled && ChimeFloor == 2)
	{
		GetWorldTimerManager().ClearTimer(ChimeTimerHandle);
		ChimeFloor = 1;
		SetState(EIGElevatorState::OpeningIntermediate);
		DoorAnimation.Begin(0.0f, IntermediateDoorOffset, DoorSlideDuration);
		SetActorTickEnabled(true);
		return;
	}

	--ChimeFloor;
	if (ChimeFloor < 1)
	{
		GetWorldTimerManager().ClearTimer(ChimeTimerHandle);

		// Arrival "ding", then the lobby doors open.
		IGAudio::SpawnOneShotAt(
			this,
			UIGToneSequenceSoundWave::CreateDoorChime(this),
			CabSoundLocation,
			0.55f);
		SetState(EIGElevatorState::OpeningLower);
		DoorAnimation.Begin(0.0f, DoorPanelWidth, DoorSlideDuration);
		SetActorTickEnabled(true);
	}
}

void AIGElevator::ApplyDoorOffsets(
	const float UpperOffset,
	const float IntermediateOffset,
	const float LowerOffset)
{
	using namespace IGElevator;
	const float DoorX = -CabDepth * 0.5f - WallThickness * 0.5f;

	for (int32 PanelIndex = 0; PanelIndex < UpperDoorPanels.Num(); ++PanelIndex)
	{
		const float Side = PanelIndex == 0 ? -1.0f : 1.0f;
		UpperDoorPanels[PanelIndex]->SetRelativeLocation(FVector(
			DoorX,
			Side * (DoorPanelWidth * 0.5f + UpperOffset),
			DoorHeight * 0.5f));
	}
	for (int32 PanelIndex = 0; PanelIndex < LowerDoorPanels.Num(); ++PanelIndex)
	{
		const float Side = PanelIndex == 0 ? -1.0f : 1.0f;
		LowerDoorPanels[PanelIndex]->SetRelativeLocation(FVector(
			DoorX,
			Side * (DoorPanelWidth * 0.5f + LowerOffset),
			-FloorDeltaZ + DoorHeight * 0.5f));
	}
	for (int32 PanelIndex = 0; PanelIndex < IntermediateDoorPanels.Num(); ++PanelIndex)
	{
		const float Side = PanelIndex == 0 ? -1.0f : 1.0f;
		IntermediateDoorPanels[PanelIndex]->SetRelativeLocation(FVector(
			DoorX,
			Side * (DoorPanelWidth * 0.5f + IntermediateOffset),
			GetIntermediateCabBaseZ() + DoorHeight * 0.5f));
	}
}

void AIGElevator::SetHallFloor(const int32 Floor)
{
	HallFloor = FMath::Clamp(Floor, 1, 4);
	for (UTextRenderComponent* Display : HallDisplays)
	{
		if (Display) { Display->SetText(FText::AsNumber(HallFloor)); }
	}
	UE_LOG(LogIndieGame, Display, TEXT("ELEVATOR_DISPLAY floor=%d"), HallFloor);
}

void AIGElevator::AdvanceHallFloor()
{
	SetHallFloor(HallFloor + HallDirection);
	if (HallFloor == 1 || HallFloor == 4)
	{
		GetWorldTimerManager().ClearTimer(HallDisplayTimer);
	}
}

void AIGElevator::SetState(const EIGElevatorState NewState)
{
	State = NewState;
	// 운행 중에만 층 통과 간격으로 갱신한다. 중간 정차 때도 표시가 함께 멈춘다.
	GetWorldTimerManager().ClearTimer(HallDisplayTimer);
	switch (NewState)
	{
	case EIGElevatorState::IdleClosed:
	case EIGElevatorState::OpeningUpper:
	case EIGElevatorState::OpeningUpperForReturn:
		SetHallFloor(4); break;
	case EIGElevatorState::OpeningLower:
	case EIGElevatorState::OpeningLowerForReturn:
	case EIGElevatorState::DoneAtLobby:
		SetHallFloor(1); break;
	case EIGElevatorState::OpeningIntermediate:
		SetHallFloor(2); break;
	case EIGElevatorState::Descending:
	case EIGElevatorState::EmptyDescending:
	case EIGElevatorState::Ascending:
	case EIGElevatorState::EmptyAscending:
		HallDirection = (NewState == EIGElevatorState::Ascending || NewState == EIGElevatorState::EmptyAscending) ? 1 : -1;
		GetWorldTimerManager().SetTimer(HallDisplayTimer, this, &ThisClass::AdvanceHallFloor, SecondsPerFloor, true);
		break;
	default: break;
	}
	UE_LOG(
		LogIndieGame,
		Display,
		TEXT("Elevator state -> %d"),
		static_cast<int32>(NewState));
}
