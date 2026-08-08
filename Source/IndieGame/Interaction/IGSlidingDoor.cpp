#include "Interaction/IGSlidingDoor.h"

#include "Audio/IGAudioHelpers.h"
#include "Audio/IGToneSequenceSoundWave.h"
#include "Components/BoxComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "Entity/IGNoiseSubsystem.h"
#include "Engine/CollisionProfile.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/Pawn.h"
#include "TimerManager.h"

AIGSlidingDoor::AIGSlidingDoor()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;

	DoorRoot = CreateDefaultSubobject<USceneComponent>(TEXT("DoorRoot"));
	SetRootComponent(DoorRoot);

	LeftPanel = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("LeftPanel"));
	LeftPanel->SetupAttachment(DoorRoot);
	LeftPanel->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
	LeftPanel->SetGenerateOverlapEvents(false);
	LeftPanel->SetCanEverAffectNavigation(false);
	LeftPanel->SetMobility(EComponentMobility::Movable);

	RightPanel = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("RightPanel"));
	RightPanel->SetupAttachment(DoorRoot);
	RightPanel->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
	RightPanel->SetGenerateOverlapEvents(false);
	RightPanel->SetCanEverAffectNavigation(false);
	RightPanel->SetMobility(EComponentMobility::Movable);

	ApproachTrigger = CreateDefaultSubobject<UBoxComponent>(TEXT("ApproachTrigger"));
	ApproachTrigger->SetupAttachment(DoorRoot);
	ApproachTrigger->SetBoxExtent(FVector(130.0f, 110.0f, 110.0f));
	ApproachTrigger->SetRelativeLocation(FVector(0.0f, 0.0f, 110.0f));
	ApproachTrigger->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	ApproachTrigger->SetCollisionResponseToAllChannels(ECR_Ignore);
	ApproachTrigger->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	ApproachTrigger->SetGenerateOverlapEvents(true);
}

void AIGSlidingDoor::ConfigurePrototypeVisuals(
	UStaticMesh* CubeMesh,
	UMaterialInterface* GlassMaterial,
	UMaterialInterface* FrameMaterial,
	const FVector& PanelSize)
{
	if (!CubeMesh)
	{
		return;
	}

	PanelSlideDistance = PanelSize.Y * 0.92f;

	// Panels meet at the actor center; each slides outward along its own Y sign.
	LeftPanel->SetStaticMesh(CubeMesh);
	LeftPanel->SetMaterial(0, GlassMaterial);
	// Translucent panes should not generate an opaque VSM silhouette. The
	// aluminum child frame still casts the door's readable shadow.
	LeftPanel->SetCastShadow(false);
	LeftPanel->SetRelativeLocation(FVector(0.0f, -PanelSize.Y * 0.5f, PanelSize.Z * 0.5f));
	LeftPanel->SetRelativeScale3D(PanelSize / 100.0f);

	RightPanel->SetStaticMesh(CubeMesh);
	RightPanel->SetMaterial(0, GlassMaterial);
	RightPanel->SetCastShadow(false);
	RightPanel->SetRelativeLocation(FVector(0.0f, PanelSize.Y * 0.5f, PanelSize.Z * 0.5f));
	RightPanel->SetRelativeScale3D(PanelSize / 100.0f);

	// Full aluminum framing per panel: stiles, rails, a mid rail and the dark
	// meeting-edge seam, so each leaf reads as a real storefront door.
	int32 DetailIndex = 0;
	auto AddPanelDetail = [this, CubeMesh, &DetailIndex, &PanelSize](
		UStaticMeshComponent* Panel, UMaterialInterface* Material,
		const FVector& LocalCenterCm, const FVector& SizeCm)
	{
		UStaticMeshComponent* Detail = NewObject<UStaticMeshComponent>(
			this,
			*FString::Printf(TEXT("PanelDetail%d"), DetailIndex++));
		Detail->SetupAttachment(Panel);
		// Children of a scaled panel: convert to the panel's local frame.
		Detail->SetRelativeLocation(FVector(
			LocalCenterCm.X / PanelSize.X * 100.0f,
			LocalCenterCm.Y / PanelSize.Y * 100.0f,
			LocalCenterCm.Z / PanelSize.Z * 100.0f));
		Detail->SetStaticMesh(CubeMesh);
		Detail->SetMaterial(0, Material);
		Detail->SetRelativeScale3D(FVector(
			SizeCm.X / PanelSize.X,
			SizeCm.Y / PanelSize.Y,
			SizeCm.Z / PanelSize.Z));
		Detail->SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);
		Detail->SetGenerateOverlapEvents(false);
		Detail->SetCanEverAffectNavigation(false);
		Detail->SetMobility(EComponentMobility::Movable);
		Detail->RegisterComponent();
	};

	for (int32 PanelIdx = 0; PanelIdx < 2; ++PanelIdx)
	{
		UStaticMeshComponent* Panel = PanelIdx == 0 ? LeftPanel.Get() : RightPanel.Get();
		const float InnerSide = PanelIdx == 0 ? 1.0f : -1.0f; // toward the meeting gap
		// Stiles (outer + meeting edge), top/bottom/mid rails.
		AddPanelDetail(Panel, FrameMaterial,
			FVector(0, -InnerSide * (PanelSize.Y * 0.5f - 3.5f), 0),
			FVector(PanelSize.X + 2.5f, 7, PanelSize.Z));
		AddPanelDetail(Panel, FrameMaterial,
			FVector(0, InnerSide * (PanelSize.Y * 0.5f - 3.5f), 0),
			FVector(PanelSize.X + 2.5f, 7, PanelSize.Z));
		AddPanelDetail(Panel, FrameMaterial,
			FVector(0, 0, PanelSize.Z * 0.5f - 4.0f),
			FVector(PanelSize.X + 2.5f, PanelSize.Y, 8));
		AddPanelDetail(Panel, FrameMaterial,
			FVector(0, 0, -PanelSize.Z * 0.5f + 6.0f),
			FVector(PanelSize.X + 2.5f, PanelSize.Y, 12));
		AddPanelDetail(Panel, FrameMaterial,
			FVector(0, 0, -PanelSize.Z * 0.10f),
			FVector(PanelSize.X + 3.0f, PanelSize.Y, 7));
		// Dark meeting-edge seam strip.
		AddPanelDetail(Panel, FrameMaterial,
			FVector(0, InnerSide * (PanelSize.Y * 0.5f - 0.8f), 0),
			FVector(PanelSize.X + 3.5f, 1.6f, PanelSize.Z));
	}

	// Floor guide rail and the overhead motion-sensor pod (static, on the root).
	auto AddRootDetail = [this, CubeMesh, &DetailIndex](
		UMaterialInterface* Material, const FVector& Center, const FVector& Size)
	{
		UStaticMeshComponent* Detail = NewObject<UStaticMeshComponent>(
			this,
			*FString::Printf(TEXT("DoorRootDetail%d"), DetailIndex++));
		Detail->SetupAttachment(DoorRoot);
		Detail->SetStaticMesh(CubeMesh);
		Detail->SetMaterial(0, Material);
		Detail->SetRelativeLocation(Center);
		Detail->SetRelativeScale3D(Size / 100.0f);
		Detail->SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);
		Detail->SetGenerateOverlapEvents(false);
		Detail->SetCanEverAffectNavigation(false);
		Detail->SetMobility(EComponentMobility::Movable);
		Detail->RegisterComponent();
	};
	AddRootDetail(FrameMaterial, FVector(0, 0, 1.2f), FVector(10, PanelSize.Y * 2.2f, 2.4f));
	AddRootDetail(FrameMaterial, FVector(-6, 0, PanelSize.Z + 6.0f), FVector(9, 17, 5));
}

void AIGSlidingDoor::BeginPlay()
{
	Super::BeginPlay();
	ApproachTrigger->OnComponentBeginOverlap.AddDynamic(this, &ThisClass::HandleTriggerBeginOverlap);
	ApproachTrigger->OnComponentEndOverlap.AddDynamic(this, &ThisClass::HandleTriggerEndOverlap);
}

void AIGSlidingDoor::Tick(const float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	const bool bFinished = SlideAnimation.Advance(DeltaSeconds);
	const float Offset = SlideAnimation.CurrentValue;
	const float PanelHalfWidth = PanelSlideDistance / 0.92f * 0.5f;
	LeftPanel->SetRelativeLocation(FVector(
		0.0f,
		-PanelHalfWidth - Offset,
		LeftPanel->GetRelativeLocation().Z));
	RightPanel->SetRelativeLocation(FVector(
		0.0f,
		PanelHalfWidth + Offset,
		RightPanel->GetRelativeLocation().Z));

	if (bFinished)
	{
		if (!bOpen)
		{
			LeftPanel->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
			RightPanel->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
		}
		SetActorTickEnabled(false);
	}
}

void AIGSlidingDoor::PlayChime(const float VolumeMultiplier)
{
	IGAudio::SpawnOneShotAt(
		this,
		UIGToneSequenceSoundWave::CreateDoorChime(this),
		GetActorLocation() + FVector(0.0f, 0.0f, 200.0f),
		VolumeMultiplier,
		1.0f,
		220.0f,
		2600.0f);
}

void AIGSlidingDoor::HandleTriggerBeginOverlap(
	UPrimitiveComponent* OverlappedComponent,
	AActor* OtherActor,
	UPrimitiveComponent* OtherComponent,
	int32 OtherBodyIndex,
	bool bFromSweep,
	const FHitResult& SweepResult)
{
	const APawn* Pawn = Cast<APawn>(OtherActor);
	if (!Pawn || !Pawn->IsPlayerControlled())
	{
		return;
	}

	++OverlappingPawnCount;
	GetWorldTimerManager().ClearTimer(CloseTimerHandle);
	SetDoorOpen(true);
}

void AIGSlidingDoor::HandleTriggerEndOverlap(
	UPrimitiveComponent* OverlappedComponent,
	AActor* OtherActor,
	UPrimitiveComponent* OtherComponent,
	int32 OtherBodyIndex)
{
	const APawn* Pawn = Cast<APawn>(OtherActor);
	if (!Pawn || !Pawn->IsPlayerControlled())
	{
		return;
	}

	OverlappingPawnCount = FMath::Max(0, OverlappingPawnCount - 1);
	if (OverlappingPawnCount == 0 && bOpen)
	{
		GetWorldTimerManager().SetTimer(
			CloseTimerHandle,
			this,
			&ThisClass::HandleCloseTimer,
			FMath::Max(CloseDelay, 0.05f),
			false);
	}
}

void AIGSlidingDoor::SetDoorOpen(const bool bInOpen)
{
	if (bOpen == bInOpen)
	{
		return;
	}

	bOpen = bInOpen;
	// The leaves move as child components and therefore cannot sweep. Keep
	// them non-blocking while opening/open/closing, then restore collision only
	// after both leaves are fully closed. This prevents the automatic door from
	// shoving or trapping a pawn at the sensor boundary.
	LeftPanel->SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);
	RightPanel->SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);
	SlideAnimation.Begin(
		SlideAnimation.CurrentValue,
		bOpen ? PanelSlideDistance : 0.0f,
		SlideDuration);
	SetActorTickEnabled(true);

	if (bOpen && bChimeEnabled)
	{
		PlayChime();
	}

	// The store's automatic door is not an interactable — a pawn walking into
	// the sensor is the whole verb — so the report belongs at the one place
	// motion is actually committed.
	if (UWorld* World = GetWorld())
	{
		if (UIGNoiseSubsystem* Noise = World->GetSubsystem<UIGNoiseSubsystem>())
		{
			Noise->ReportNoise(GetActorLocation(), 0.22f, this);
		}
	}
}

void AIGSlidingDoor::HandleCloseTimer()
{
	if (OverlappingPawnCount == 0)
	{
		SetDoorOpen(false);
	}
}
