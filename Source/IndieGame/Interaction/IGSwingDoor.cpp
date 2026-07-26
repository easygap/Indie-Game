#include "Interaction/IGSwingDoor.h"

#include "Audio/IGAudioHelpers.h"
#include "Audio/IGToneSequenceSoundWave.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/CollisionProfile.h"
#include "Engine/StaticMesh.h"
#include "Narrative/IGStoryHelpers.h"
#include "Player/IGHorrorHUD.h"

AIGSwingDoor::AIGSwingDoor()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;

	HingeRoot = CreateDefaultSubobject<USceneComponent>(TEXT("HingeRoot"));
	SetRootComponent(HingeRoot);

	DoorPivot = CreateDefaultSubobject<USceneComponent>(TEXT("DoorPivot"));
	DoorPivot->SetupAttachment(HingeRoot);

	DoorMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("DoorMesh"));
	DoorMesh->SetupAttachment(DoorPivot);
	DoorMesh->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
	DoorMesh->SetGenerateOverlapEvents(false);
	DoorMesh->SetCanEverAffectNavigation(false);
	DoorMesh->SetMobility(EComponentMobility::Movable);

	HandleMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("HandleMesh"));
	HandleMesh->SetupAttachment(DoorPivot);
	HandleMesh->SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);
	HandleMesh->SetGenerateOverlapEvents(false);
	HandleMesh->SetCanEverAffectNavigation(false);
	HandleMesh->SetMobility(EComponentMobility::Movable);

	OpenPrompt = NSLOCTEXT("IGSwingDoor", "OpenPrompt", "문 열기");
	ClosePrompt = NSLOCTEXT("IGSwingDoor", "ClosePrompt", "문 닫기");
}

void AIGSwingDoor::ConfigurePrototypeVisuals(
	UStaticMesh* CubeMesh,
	UMaterialInterface* DoorMaterial,
	UMaterialInterface* HandleMaterial,
	const FVector& PanelSize)
{
	if (!CubeMesh)
	{
		return;
	}

	DoorMesh->SetStaticMesh(CubeMesh);
	DoorMesh->SetMaterial(0, DoorMaterial);
	DoorMesh->SetRelativeLocation(FVector(0.0f, PanelSize.Y * 0.5f, PanelSize.Z * 0.5f));
	DoorMesh->SetRelativeScale3D(PanelSize / 100.0f);

	// Lever handle bar on a rose plate.
	HandleMesh->SetStaticMesh(CubeMesh);
	HandleMesh->SetMaterial(0, HandleMaterial);
	HandleMesh->SetRelativeLocation(FVector(
		-PanelSize.X * 0.5f - 4.0f,
		PanelSize.Y - 16.0f,
		PanelSize.Z * 0.47f));
	HandleMesh->SetRelativeScale3D(FVector(0.028f, 0.15f, 0.03f));

	// Korean fire-door detailing: recessed panel grooves, digital lock,
	// peephole, hinge knuckles and a door closer arm up top.
	int32 DetailIndex = 0;
	auto AddDetail = [this, CubeMesh, &DetailIndex](
		UMaterialInterface* Material, const FVector& Center, const FVector& Size,
		UStaticMesh* MeshOverride = nullptr)
	{
		UStaticMeshComponent* Detail = NewObject<UStaticMeshComponent>(
			this,
			*FString::Printf(TEXT("SwingDoorDetail%d"), DetailIndex++));
		Detail->SetupAttachment(DoorPivot);
		Detail->SetStaticMesh(MeshOverride ? MeshOverride : CubeMesh);
		Detail->SetMaterial(0, Material);
		Detail->SetRelativeLocation(Center);
		Detail->SetRelativeScale3D(Size / 100.0f);
		Detail->SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);
		Detail->SetGenerateOverlapEvents(false);
		Detail->SetCanEverAffectNavigation(false);
		Detail->SetMobility(EComponentMobility::Movable);
		Detail->RegisterComponent();
		return Detail;
	};

	// A Korean unit door is a flat charcoal slab broken by ONE brushed band
	// running its full height, inset from the handle edge, with small dark
	// squares punched down it. Horizontal grooves would read as a Western
	// panelled door, which is why there are none.
	const float FaceX = -PanelSize.X * 0.5f - 0.7f;
	const float BandY = PanelSize.Y * 0.66f;
	AddDetail(HandleMaterial,
		FVector(FaceX, BandY, PanelSize.Z * 0.5f),
		FVector(0.8f, 13.0f, PanelSize.Z - 16.0f));
	for (int32 SquareIndex = 0; SquareIndex < 5; ++SquareIndex)
	{
		AddDetail(DoorMaterial,
			FVector(FaceX - 0.8f, BandY, PanelSize.Z * (0.18f + 0.16f * SquareIndex)),
			FVector(0.7f, 5.0f, 5.0f));
	}
	// Digital door lock above the handle, with a faint standby diode.
	AddDetail(HandleMaterial,
		FVector(FaceX - 1.8f, PanelSize.Y - 16.0f, PanelSize.Z * 0.58f),
		FVector(4.5f, 9.0f, 16.0f));
	AddDetail(DoorMaterial,
		FVector(FaceX - 4.2f, PanelSize.Y - 16.0f, PanelSize.Z * 0.62f),
		FVector(0.6f, 5.5f, 7.0f));
	// Peephole.
	AddDetail(HandleMaterial,
		FVector(FaceX, PanelSize.Y * 0.5f, PanelSize.Z * 0.78f),
		FVector(1.2f, 3.0f, 3.0f));
	// Hinge knuckles on the pivot edge.
	AddDetail(HandleMaterial,
		FVector(0.0f, 1.5f, PanelSize.Z * 0.85f), FVector(4.5f, 4.5f, 9.0f));
	AddDetail(HandleMaterial,
		FVector(0.0f, 1.5f, PanelSize.Z * 0.15f), FVector(4.5f, 4.5f, 9.0f));
	// Door closer arm on the interior top corner.
	AddDetail(HandleMaterial,
		FVector(PanelSize.X * 0.5f + 2.5f, 18.0f, PanelSize.Z - 8.0f),
		FVector(5.0f, 24.0f, 6.0f));
	AddDetail(HandleMaterial,
		FVector(PanelSize.X * 0.5f + 5.0f, 34.0f, PanelSize.Z - 5.0f),
		FVector(2.5f, 30.0f, 2.5f));
}

void AIGSwingDoor::ConfigureFramedGlassVisuals(
	UStaticMesh* CubeMesh,
	UMaterialInterface* GlassMaterial,
	UMaterialInterface* FrameMaterial,
	const FVector& PanelSize)
{
	if (!CubeMesh)
	{
		return;
	}

	// Two glass insets in an aluminum frame with a mid rail and push bar.
	DoorMesh->SetStaticMesh(CubeMesh);
	DoorMesh->SetMaterial(0, GlassMaterial);
	DoorMesh->SetRelativeLocation(FVector(0.0f, PanelSize.Y * 0.5f, PanelSize.Z * 0.5f));
	DoorMesh->SetRelativeScale3D(
		FVector(PanelSize.X * 0.55f, PanelSize.Y - 14.0f, PanelSize.Z - 14.0f) / 100.0f);

	HandleMesh->SetStaticMesh(CubeMesh);
	HandleMesh->SetMaterial(0, FrameMaterial);
	HandleMesh->SetRelativeLocation(FVector(
		-PanelSize.X * 0.5f - 3.5f,
		PanelSize.Y * 0.5f,
		PanelSize.Z * 0.47f));
	HandleMesh->SetRelativeScale3D(FVector(0.03f, (PanelSize.Y - 26.0f) / 100.0f, 0.045f));

	struct FFramePiece
	{
		FVector Center;
		FVector Size;
	};
	const FFramePiece FramePieces[] = {
		// Hinge stile, lock stile, bottom rail, top rail, mid rail.
		{{0.0f, 4.0f, PanelSize.Z * 0.5f}, {PanelSize.X, 8.0f, PanelSize.Z}},
		{{0.0f, PanelSize.Y - 4.0f, PanelSize.Z * 0.5f}, {PanelSize.X, 8.0f, PanelSize.Z}},
		{{0.0f, PanelSize.Y * 0.5f, 5.0f}, {PanelSize.X, PanelSize.Y, 10.0f}},
		{{0.0f, PanelSize.Y * 0.5f, PanelSize.Z - 4.0f}, {PanelSize.X, PanelSize.Y, 8.0f}},
		{{0.0f, PanelSize.Y * 0.5f, PanelSize.Z * 0.42f}, {PanelSize.X + 1.0f, PanelSize.Y, 9.0f}},
	};
	int32 FramePieceIndex = 0;
	for (const FFramePiece& Piece : FramePieces)
	{
		UStaticMeshComponent* Frame = NewObject<UStaticMeshComponent>(
			this,
			*FString::Printf(TEXT("DoorFrame_%d"), FramePieceIndex++));
		Frame->SetupAttachment(DoorPivot);
		Frame->SetStaticMesh(CubeMesh);
		Frame->SetMaterial(0, FrameMaterial);
		Frame->SetRelativeLocation(Piece.Center);
		Frame->SetRelativeScale3D(Piece.Size / 100.0f);
		Frame->SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);
		Frame->SetGenerateOverlapEvents(false);
		Frame->SetCanEverAffectNavigation(false);
		Frame->SetMobility(EComponentMobility::Movable);
		Frame->RegisterComponent();
	}
}

void AIGSwingDoor::SetLeverMesh(
	UStaticMesh* LeverMesh,
	UMaterialInterface* Material,
	const FVector& PanelSize)
{
	if (!LeverMesh)
	{
		return;
	}

	// Pitch 90 lays the rose flat against the outer (-X) face; the lever arm
	// then sweeps back toward the hinge the way a real lever does.
	HandleMesh->SetStaticMesh(LeverMesh);
	HandleMesh->SetMaterial(0, Material);
	HandleMesh->SetRelativeLocation(FVector(
		-PanelSize.X * 0.5f,
		PanelSize.Y - 11.0f,
		PanelSize.Z * 0.47f));
	HandleMesh->SetRelativeRotation(FRotator(90.0f, 0.0f, 0.0f));
	HandleMesh->SetRelativeScale3D(FVector::OneVector);
}

void AIGSwingDoor::SetRequirements(TArray<FIGDoorRequirement>&& InRequirements)
{
	Requirements = MoveTemp(InRequirements);
}

void AIGSwingDoor::BeginPlay()
{
	Super::BeginPlay();

	if (!InteractionTag.IsValid())
	{
		InteractionTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Interaction.Door")), false);
	}
}

void AIGSwingDoor::Tick(const float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	const bool bFinished = DoorAnimation.Advance(DeltaSeconds);
	DoorPivot->SetRelativeRotation(FRotator(0.0f, DoorAnimation.CurrentValue, 0.0f));

	if (bFinished)
	{
		SetActorTickEnabled(false);
		if (!bOpen && !bSuppressNextCloseThud)
		{
			IGAudio::SpawnOneShotAt(
				this,
				UIGToneSequenceSoundWave::CreateDoorThud(this),
				DoorMesh->GetComponentLocation(),
				0.9f);
		}
		bSuppressNextCloseThud = false;
	}
}

bool AIGSwingDoor::IsLocked() const
{
	return FindUnmetRequirement() != nullptr;
}

const FIGDoorRequirement* AIGSwingDoor::FindUnmetRequirement() const
{
	for (const FIGDoorRequirement& Requirement : Requirements)
	{
		if (Requirement.RequiredState.IsValid()
			&& !IGStory::HasState(this, Requirement.RequiredState))
		{
			return &Requirement;
		}
	}
	return nullptr;
}

bool AIGSwingDoor::CanInteract_Implementation(AActor* Interactor) const
{
	return Super::CanInteract_Implementation(Interactor) && !DoorAnimation.bActive;
}

FText AIGSwingDoor::GetInteractionPrompt_Implementation(AActor* Interactor) const
{
	if (const FIGDoorRequirement* Unmet = FindUnmetRequirement(); Unmet && !bOpen)
	{
		return Unmet->LockedPrompt.IsEmpty() ? OpenPrompt : Unmet->LockedPrompt;
	}
	return bOpen ? ClosePrompt : OpenPrompt;
}

void AIGSwingDoor::CompleteInteraction_Implementation(const FIGInteractionContext& Context)
{
	Super::CompleteInteraction_Implementation(Context);

	if (!bOpen)
	{
		if (const FIGDoorRequirement* Unmet = FindUnmetRequirement())
		{
			IGAudio::SpawnOneShotAt(
				this,
				UIGToneSequenceSoundWave::CreateLockedRattle(this),
				HandleMesh->GetComponentLocation(),
				0.9f);
			if (!Unmet->LockedThought.IsEmpty())
			{
				AIGHorrorHUD::PushThought(this, Unmet->LockedThought, 3.2f);
			}
			return;
		}
	}

	BeginSwing(!bOpen, !bOpen, false);
}

void AIGSwingDoor::ForceOpenState(const bool bInOpen)
{
	DoorAnimation = FIGDoorAnimation();
	bOpen = bInOpen;
	bSuppressNextCloseThud = false;
	DoorPivot->SetRelativeRotation(FRotator(0.0f, bOpen ? OpenYaw : 0.0f, 0.0f));
	SetActorTickEnabled(false);
}

bool AIGSwingDoor::BeginScriptedSwing(
	const bool bInOpen,
	const bool bPlayCreak,
	const bool bSuppressCloseThud)
{
	return BeginSwing(bInOpen, bPlayCreak, bSuppressCloseThud);
}

bool AIGSwingDoor::BeginSwing(
	const bool bInOpen,
	const bool bPlayCreak,
	const bool bSuppressCloseThud)
{
	if (DoorAnimation.bActive || bOpen == bInOpen)
	{
		return false;
	}

	bOpen = bInOpen;
	bSuppressNextCloseThud = !bOpen && bSuppressCloseThud;
	DoorAnimation.Begin(
		DoorPivot->GetRelativeRotation().Yaw,
		bOpen ? OpenYaw : 0.0f,
		SwingDuration);
	SetActorTickEnabled(true);

	if (bPlayCreak)
	{
		IGAudio::SpawnOneShotAt(
			this,
			UIGToneSequenceSoundWave::CreateDoorCreak(this),
			DoorMesh->GetComponentLocation(),
			0.8f);
	}

	return true;
}
