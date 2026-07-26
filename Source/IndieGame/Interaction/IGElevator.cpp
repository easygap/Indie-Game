#include "Interaction/IGElevator.h"

#include "Audio/IGAudioHelpers.h"
#include "Audio/IGToneSequenceSoundWave.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/CollisionProfile.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/Character.h"
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
}

AIGElevator::AIGElevator()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;

	ElevatorRoot = CreateDefaultSubobject<USceneComponent>(TEXT("ElevatorRoot"));
	SetRootComponent(ElevatorRoot);

	CallButtonMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("CallButton"));
	CallButtonMesh->SetupAttachment(ElevatorRoot);
	CallButtonMesh->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
	CallButtonMesh->SetGenerateOverlapEvents(false);
	CallButtonMesh->SetCanEverAffectNavigation(false);

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
	Piece->SetMaterial(0, Material);
	Piece->SetRelativeLocation(RelativeLocation);
	Piece->SetRelativeRotation(Rotation);
	Piece->SetRelativeScale3D(SizeCentimeters / 100.0f);
	Piece->SetMobility(EComponentMobility::Movable);
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
	// The rear wall is the near-mirror panel every Korean lift car has.
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
	CabLight->SetIntensity(2100.0f);
	CabLight->SetAttenuationRadius(430.0f);
	CabLight->SetLightColor(FLinearColor(0.94f, 0.98f, 1.0f));
	CabLight->SetSourceRadius(46.0f);
	CabLight->SetSoftSourceRadius(70.0f);
	CabLight->SetCastShadows(true);
	// The visible diffuser meshes provide the fixture reflection. Suppress
	// the analytic point source itself so the brushed rear panel does not show
	// a giant circular highlight unrelated to the rectangular ceiling lights.
	CabLight->SetSpecularScale(0.18f);
	CabLight->SetMobility(EComponentMobility::Movable);
	CabLight->SetVolumetricScatteringIntensity(0.28f);
	CabLight->RegisterComponent();

	// Floor bounce so the marble and the rider's feet are not a black hole.
	UPointLightComponent* FloorFill = NewObject<UPointLightComponent>(
		this, *FString::Printf(TEXT("ElevatorCabFill_%d"), PieceCounter++));
	FloorFill->SetupAttachment(Parent);
	FloorFill->SetRelativeLocation(FVector(10.0f, 0, BaseZ + 55.0f));
	FloorFill->SetIntensity(190.0f);
	FloorFill->SetAttenuationRadius(260.0f);
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

	// Rear handrail and the mirror inset above it.
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
	CachedDoorMaterial = Visuals.StainlessMaterial;
	CachedFloorMaterial =
		Visuals.FloorMaterial ? Visuals.FloorMaterial : Visuals.StainlessMaterial;
	CachedInlayMaterial =
		Visuals.InlayMaterial ? Visuals.InlayMaterial : Visuals.StainlessMaterial;
	CachedCopMaterial = Visuals.CopMaterial;
	CachedDiffuserMaterial =
		Visuals.DiffuserMaterial ? Visuals.DiffuserMaterial : Visuals.CopMaterial;
	FloorDeltaZ = FMath::Abs(InFloorDeltaZ);
	DoorPanelWidth = CabWidth * 0.5f;

	BuildCabShell(ElevatorRoot, 0.0f);
	BuildCabShell(ElevatorRoot, -FloorDeltaZ);

	// Sliding door pairs at the cab opening plane for both floors. Each leaf
	// gets a hairline seam and a mirror stile so it reads as a lift door and
	// not a slab of metal.
	for (const float BaseZ : {0.0f, -FloorDeltaZ})
	{
		for (const float Side : {-1.0f, 1.0f})
		{
			UStaticMeshComponent* Panel = MakePiece(
				ElevatorRoot, CachedCubeMesh, CachedCabMaterial,
				FVector(-CabDepth * 0.5f - WallThickness * 0.5f,
					Side * DoorPanelWidth * 0.5f,
					BaseZ + DoorHeight * 0.5f),
				FVector(8, DoorPanelWidth, DoorHeight));
			if (FMath::IsNearlyZero(BaseZ))
			{
				UpperDoorPanels.Add(Panel);
			}
			else
			{
				LowerDoorPanels.Add(Panel);
			}
		}
		// Hall lantern above each landing: the red floor digit and arrow.
		MakePiece(ElevatorRoot, CachedCubeMesh, CachedCabMaterial,
			FVector(-CabDepth * 0.5f - WallThickness - 1.0f, 0, BaseZ + DoorHeight + 15.0f),
			FVector(3, 48, 26), false);
		MakePiece(ElevatorRoot, CachedCubeMesh,
			Visuals.HallMaterial ? Visuals.HallMaterial : Visuals.CopMaterial,
			FVector(-CabDepth * 0.5f - WallThickness - 2.6f, 0, BaseZ + DoorHeight + 15.0f),
			FVector(1.6f, 38, 19), false);
		// Landing architrave: the stainless surround around the doorway.
		for (const float Side : {-1.0f, 1.0f})
		{
			MakePiece(ElevatorRoot, CachedCubeMesh, CachedCabMaterial,
				FVector(-CabDepth * 0.5f - WallThickness - 1.0f,
					Side * (CabWidth * 0.5f + 4.0f), BaseZ + DoorHeight * 0.5f),
				FVector(3, 12, DoorHeight + 8), false);
		}
	}

	// Hall call button on the corridor wall beside the doors. It has to sit
	// inside the hallway, which is narrower than the shaft.
	CallButtonMesh->SetStaticMesh(CachedCubeMesh);
	CallButtonMesh->SetMaterial(0, CachedCabMaterial);
	CallButtonMesh->SetRelativeLocation(FVector(
		-CabDepth * 0.5f - WallThickness - 4.0f,
		-CabWidth * 0.5f + 10.0f,
		105.0f));
	CallButtonMesh->SetRelativeScale3D(FVector(0.04f, 0.12f, 0.20f));
	// The two round call buttons on that plate, at both landings.
	for (const float BaseZ : {0.0f, -FloorDeltaZ})
	{
		for (int32 ButtonIndex = 0; ButtonIndex < 2; ++ButtonIndex)
		{
			MakePiece(ElevatorRoot, CachedCylinderMesh, CachedDiffuserMaterial,
				FVector(-CabDepth * 0.5f - WallThickness - 6.6f,
					-CabWidth * 0.5f + 10.0f,
					BaseZ + 110.0f - ButtonIndex * 14.0f),
				FVector(6, 6, 1.5f), false, FRotator(90, 0, 0));
		}
	}
}

void AIGElevator::ResetForNewRide()
{
	// Kill anything in flight — a queued StartRide or a chime train would
	// otherwise fire into the new ride and desynchronise the doors.
	GetWorldTimerManager().ClearTimer(RideTimerHandle);
	GetWorldTimerManager().ClearTimer(ChimeTimerHandle);
	GetWorldTimerManager().ClearTimer(AdmissionPollHandle);
	SetActorTickEnabled(false);
	PendingRider.Reset();
	bUpperDoorDepartureStarted = false;

	// Both door pairs shut, both cabs idle, the rider counted as upstairs.
	ApplyDoorOffsets(0.0f, 0.0f);
	DoorAnimation = FIGDoorAnimation();
	ChimeFloor = 3;
	bRideComplete = false;
	SetState(EIGElevatorState::IdleClosed);
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
}

bool AIGElevator::CanInteract_Implementation(AActor* Interactor) const
{
	return Super::CanInteract_Implementation(Interactor)
		&& State == EIGElevatorState::IdleClosed;
}

FText AIGElevator::GetInteractionPrompt_Implementation(AActor* Interactor) const
{
	return NSLOCTEXT("IGElevator", "CallPrompt", "엘리베이터 호출");
}

void AIGElevator::CompleteInteraction_Implementation(const FIGInteractionContext& Context)
{
	Super::CompleteInteraction_Implementation(Context);

	if (State != EIGElevatorState::IdleClosed)
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

void AIGElevator::Tick(const float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	const bool bFinished = DoorAnimation.Advance(DeltaSeconds);
	switch (State)
	{
	case EIGElevatorState::OpeningUpper:
	case EIGElevatorState::ClosingUpper:
		ApplyDoorOffsets(DoorAnimation.CurrentValue, 0.0f);
		break;
	case EIGElevatorState::OpeningIntermediate:
	case EIGElevatorState::ClosingIntermediate:
	case EIGElevatorState::OpeningLower:
		ApplyDoorOffsets(0.0f, DoorAnimation.CurrentValue);
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

		// Move only the pawn that actually entered this cab. Looking up the
		// world's first pawn could teleport a spectator or an outside player.
		Rider->SetActorLocation(
			Rider->GetActorLocation() - FVector(0, 0, FloorDeltaZ),
			false,
			nullptr,
			ETeleportType::TeleportPhysics);
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

	case EIGElevatorState::OpeningIntermediate:
		SetState(EIGElevatorState::HoldingIntermediate);
		OnIntermediateStopOpened.Broadcast(this);
		break;

	case EIGElevatorState::ClosingIntermediate:
		SetState(EIGElevatorState::Descending);
		GetWorldTimerManager().SetTimer(
			ChimeTimerHandle,
			this,
			&ThisClass::HandleFloorChime,
			SecondsPerFloor,
			true);
		break;

	case EIGElevatorState::OpeningLower:
		SetState(EIGElevatorState::Done);
		bRideComplete = true;
		OnArrivedAtLobby.Broadcast(this);
		break;

	default:
		break;
	}
}

void AIGElevator::HandleFloorChime()
{
	const FVector LowerCabLocation = GetActorLocation() - FVector(0, 0, FloorDeltaZ - 150.0f);
	IGAudio::SpawnOneShotAt(
		this,
		UIGToneSequenceSoundWave::CreateScannerBeep(this),
		LowerCabLocation,
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
			LowerCabLocation,
			0.55f);
		SetState(EIGElevatorState::OpeningLower);
		DoorAnimation.Begin(0.0f, DoorPanelWidth, DoorSlideDuration);
		SetActorTickEnabled(true);
	}
}

void AIGElevator::ApplyDoorOffsets(const float UpperOffset, const float LowerOffset)
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
}

void AIGElevator::SetState(const EIGElevatorState NewState)
{
	State = NewState;
	UE_LOG(
		LogIndieGame,
		Display,
		TEXT("Elevator state -> %d"),
		static_cast<int32>(NewState));
}
