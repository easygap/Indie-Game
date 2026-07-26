#include "Sequence/IGThirdMorningDirector.h"

#include "AssetCompilingManager.h"
#include "Audio/IGAlarmSoundWave.h"
#include "Audio/IGAudioHelpers.h"
#include "Audio/IGToneSequenceSoundWave.h"
#include "Camera/CameraComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/AudioComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/CollisionProfile.h"
#include "Engine/Engine.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/HUD.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "HAL/PlatformMisc.h"
#include "HighResScreenshot.h"
#include "IndieGame.h"
#include "Interaction/IGInteractable.h"
#include "Interaction/IGReadableNote.h"
#include "Interaction/IGZoneTrigger.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInterface.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Narrative/IGStoryHelpers.h"
#include "Narrative/IGStoryStateSubsystem.h"
#include "Player/IGFlashlightComponent.h"
#include "Player/IGHorrorHUD.h"
#include "Player/IGPlayerCharacter.h"
#include "ShaderCompiler.h"
#include "TimerManager.h"
#include "UObject/UObjectGlobals.h"

namespace IGThirdMorning
{
	const FVector StageOrigin(-4800.0f, 4200.0f, 0.0f);
	constexpr float StandingCapsuleCenter = 96.0f;
	constexpr float DefaultWalkSpeed = 300.0f;
	constexpr float FloodWalkSpeed = 225.0f;
	constexpr float RoofFloorZ = 240.0f;
	const FVector TankCenter(2500.0f, -300.0f, 0.0f);

	UMaterialInterface* LoadMaterial(const TCHAR* AssetPath)
	{
		return LoadObject<UMaterialInterface>(nullptr, AssetPath);
	}

	UStaticMesh* LoadMesh(const TCHAR* AssetPath)
	{
		return LoadObject<UStaticMesh>(nullptr, AssetPath);
	}
}

// ---------------------------------------------------------------------------
// Physical action
// ---------------------------------------------------------------------------

AIGChapterThreeAction::AIGChapterThreeAction()
{
	PrimaryActorTick.bCanEverTick = false;

	PresentationMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Presentation"));
	SetRootComponent(PresentationMesh);
	PresentationMesh->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
	PresentationMesh->SetGenerateOverlapEvents(false);
	PresentationMesh->SetCanEverAffectNavigation(false);
}

void AIGChapterThreeAction::Configure(
	AIGThirdMorningDirector* InDirector,
	const EIGChapterThreeAction InAction,
	UStaticMesh* Mesh,
	UMaterialInterface* Material,
	const FVector& SizeCentimeters,
	const FText& Prompt,
	const float HoldSeconds)
{
	Director = InDirector;
	Action = InAction;
	PresentationMesh->SetStaticMesh(Mesh);
	PresentationMesh->SetMaterial(0, Material);
	PresentationMesh->SetRelativeScale3D(SizeCentimeters / 100.0f);
	InteractionPrompt = Prompt;
	InteractionHoldDuration = FMath::Max(0.0f, HoldSeconds);
	InteractionTag = FGameplayTag::RequestGameplayTag(
		FName(TEXT("Interaction.Inspect")), false);
}

void AIGChapterThreeAction::CompleteInteraction_Implementation(
	const FIGInteractionContext& Context)
{
	Super::CompleteInteraction_Implementation(Context);
	if (Director)
	{
		Director->HandleAction(Action, this);
	}
}

// ---------------------------------------------------------------------------
// Director lifecycle
// ---------------------------------------------------------------------------

AIGThirdMorningDirector::AIGThirdMorningDirector()
{
	PrimaryActorTick.bCanEverTick = false;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("CH03Root"));
	SetRootComponent(SceneRoot);
	SceneRoot->SetMobility(EComponentMobility::Static);
}

FVector AIGThirdMorningDirector::GetStageOrigin()
{
	return IGThirdMorning::StageOrigin;
}

void AIGThirdMorningDirector::ConfigureAndStart(
	const bool bShowIntroCard,
	const bool bCaptureSequence)
{
	if (bStageBuilt || !GetWorld())
	{
		return;
	}

	bCaptureMode = bCaptureSequence;
	BuildStage();
	bStageBuilt = true;

	APlayerController* PlayerController = GetWorld()->GetFirstPlayerController();
	AIGPlayerCharacter* Player = PlayerController
		? Cast<AIGPlayerCharacter>(PlayerController->GetPawn())
		: nullptr;
	if (Player)
	{
		Player->SetActorLocationAndRotation(
			ToWorld(FVector(35.0f, -55.0f, IGThirdMorning::StandingCapsuleCenter)),
			FRotator(0.0f, -142.0f, 0.0f),
			false,
			nullptr,
			ETeleportType::TeleportPhysics);
		Player->SetCameraMotionEnabled(true);
		if (UIGFlashlightComponent* Flashlight = Player->GetFlashlight())
		{
			Flashlight->SetAvailable(true);
			Flashlight->SetOn(true);
		}
	}
	if (PlayerController)
	{
		PlayerController->SetControlRotation(FRotator(-9.0f, -142.0f, 0.0f));
		if (AIGHorrorHUD* HUD = Cast<AIGHorrorHUD>(PlayerController->GetHUD()))
		{
			HUD->SetObjectiveProvider(this);
		}
	}

	if (UGameInstance* GameInstance = GetGameInstance())
	{
		if (UIGStoryStateSubsystem* StoryState =
			GameInstance->GetSubsystem<UIGStoryStateSubsystem>())
		{
			StoryState->ClearStates(false);
		}
	}
	IGStory::AddState(
		this,
		FGameplayTag::RequestGameplayTag(
			FName(TEXT("State.CH03.Flood.Started")), false));

	if (bCaptureMode)
	{
		// The capture route is also a deterministic smoke test.  It does not
		// wait through the authored opening, but it uses the real stage and
		// interactions rather than a separate mock-up.
		SetPhase(EIGThirdMorningPhase::ApartmentClues);
		GetWorldTimerManager().SetTimer(
			CaptureTimer,
			this,
			&ThisClass::StartCaptureSequence,
			1.2f,
			false);
		return;
	}

	if (bShowIntroCard)
	{
		AIGHorrorHUD::ShowChapterCard(
			this,
			NSLOCTEXT("IGCH03", "IntroEyebrow", "CHAPTER 03"),
			NSLOCTEXT("IGCH03", "IntroTitle", "세 번째 아침"),
			NSLOCTEXT("IGCH03", "IntroSubtitle", "물이 온다"),
			3.2f);
		GetWorldTimerManager().SetTimer(
			FlowTimer,
			this,
			&ThisClass::BeginAwakening,
			3.25f,
			false);
	}
	else
	{
		BeginAwakening();
	}

	UE_LOG(LogIndieGame, Display, TEXT("CH03 third morning stage ready."));
}

void AIGThirdMorningDirector::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	StopAllChapterAudio();
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearAllTimersForObject(this);
	}
	Super::EndPlay(EndPlayReason);
}

FVector AIGThirdMorningDirector::ToWorld(const FVector& LocalLocation) const
{
	return GetActorTransform().TransformPosition(LocalLocation);
}

void AIGThirdMorningDirector::SetPhase(const EIGThirdMorningPhase NewPhase)
{
	Phase = NewPhase;
	UE_LOG(
		LogIndieGame,
		Display,
		TEXT("CH03 phase -> %d"),
		static_cast<int32>(Phase));
}

// ---------------------------------------------------------------------------
// Stage assembly
// ---------------------------------------------------------------------------

void AIGThirdMorningDirector::BuildStage()
{
	CubeMesh = IGThirdMorning::LoadMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
	CylinderMesh = IGThirdMorning::LoadMesh(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	SphereMesh = IGThirdMorning::LoadMesh(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	WaterBottleMesh = IGThirdMorning::LoadMesh(TEXT("/Game/Meshes/SM_WaterBottle.SM_WaterBottle"));
	BottleCapMesh = IGThirdMorning::LoadMesh(TEXT("/Game/Meshes/SM_BottleCap.SM_BottleCap"));
	LabelSleeveMesh = IGThirdMorning::LoadMesh(TEXT("/Game/Meshes/SM_LabelSleeve.SM_LabelSleeve"));

	ConcreteMaterial = IGThirdMorning::LoadMaterial(
		TEXT("/Game/Prototype/Materials/M_Concrete.M_Concrete"));
	DarkConcreteMaterial = IGThirdMorning::LoadMaterial(
		TEXT("/Game/Prototype/Materials/M_ConcreteDark.M_ConcreteDark"));
	RoomFloorMaterial = IGThirdMorning::LoadMaterial(
		TEXT("/Game/Prototype/Materials/M_RoomFloor.M_RoomFloor"));
	WallMaterial = IGThirdMorning::LoadMaterial(
		TEXT("/Game/Prototype/Materials/M_RoomWall.M_RoomWall"));
	MetalMaterial = IGThirdMorning::LoadMaterial(
		TEXT("/Game/Prototype/Materials/M_MetalUV.M_MetalUV"));
	PlasticMaterial = IGThirdMorning::LoadMaterial(
		TEXT("/Game/Prototype/Materials/M_PlasticDark.M_PlasticDark"));
	PaperMaterial = IGThirdMorning::LoadMaterial(
		TEXT("/Game/Prototype/Materials/M_PaperOld.M_PaperOld"));
	WetPaperMaterial = IGThirdMorning::LoadMaterial(
		TEXT("/Game/Prototype/Materials/M_PaperWet.M_PaperWet"));
	WaterMaterial = IGThirdMorning::LoadMaterial(
		TEXT("/Game/Prototype/Materials/M_WaterBlue.M_WaterBlue"));
	WetStepMaterial = IGThirdMorning::LoadMaterial(
		TEXT("/Game/Prototype/Materials/M_WetStep.M_WetStep"));
	ScreenMaterial = IGThirdMorning::LoadMaterial(
		TEXT("/Game/Prototype/Materials/M_ScreenGlow.M_ScreenGlow"));
	BeddingMaterial = IGThirdMorning::LoadMaterial(
		TEXT("/Game/Prototype/Materials/M_BeddingUV.M_BeddingUV"));
	GlassMaterial = IGThirdMorning::LoadMaterial(
		TEXT("/Game/Prototype/Materials/M_Glass.M_Glass"));
	BottleCapMaterial = IGThirdMorning::LoadMaterial(
		TEXT("/Game/Prototype/Materials/M_SnackBlue.M_SnackBlue"));
	WaterLabelMaterial = IGThirdMorning::LoadMaterial(
		TEXT("/Game/Prototype/Materials/M_LabelWater.M_LabelWater"));
	FridgeBodyMaterial = IGThirdMorning::LoadMaterial(
		TEXT("/Game/Prototype/Materials/M_FridgeBody.M_FridgeBody"));
	FridgeInteriorMaterial = IGThirdMorning::LoadMaterial(
		TEXT("/Game/Prototype/Materials/M_FridgeInterior.M_FridgeInterior"));
	EmergencyMaterial = IGThirdMorning::LoadMaterial(
		TEXT("/Game/Prototype/Materials/M_Alarm.M_Alarm"));

	// Missing cooked materials degrade to the engine default but never prevent
	// the chapter from being playable.
	UMaterialInterface* DefaultMaterial =
		LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/EngineMaterials/DefaultMaterial.DefaultMaterial"));
	auto FallBack = [DefaultMaterial](TObjectPtr<UMaterialInterface>& Material)
	{
		if (!Material)
		{
			Material = DefaultMaterial;
		}
	};
	FallBack(ConcreteMaterial);
	FallBack(DarkConcreteMaterial);
	FallBack(RoomFloorMaterial);
	FallBack(WallMaterial);
	FallBack(MetalMaterial);
	FallBack(PlasticMaterial);
	FallBack(PaperMaterial);
	FallBack(WetPaperMaterial);
	FallBack(WaterMaterial);
	FallBack(WetStepMaterial);
	FallBack(ScreenMaterial);
	FallBack(BeddingMaterial);
	FallBack(GlassMaterial);
	FallBack(BottleCapMaterial);
	FallBack(WaterLabelMaterial);
	FallBack(FridgeBodyMaterial);
	FallBack(FridgeInteriorMaterial);
	FallBack(EmergencyMaterial);

	BuildApartment();
	BuildFloodedCorridor();
	BuildLoopingStairwell();
	BuildFifthFloorAndRoof();
	BuildWaterTank();
	SpawnClueDocuments();
	SpawnTriggers();
}

UStaticMeshComponent* AIGThirdMorningDirector::CreateBlock(
	const FVector& LocalCenter,
	const FVector& SizeCentimeters,
	UMaterialInterface* Material,
	const bool bCollision,
	const FRotator& Rotation,
	UStaticMesh* MeshOverride)
{
	UStaticMesh* Mesh = MeshOverride ? MeshOverride : CubeMesh.Get();
	if (!Mesh)
	{
		return nullptr;
	}

	UStaticMeshComponent* Component = NewObject<UStaticMeshComponent>(
		this,
		*FString::Printf(TEXT("CH03_Block_%03d"), Geometry.Num()));
	Component->SetupAttachment(SceneRoot);
	Component->SetStaticMesh(Mesh);
	Component->SetMaterial(0, Material);
	Component->SetRelativeLocation(LocalCenter);
	Component->SetRelativeRotation(Rotation);
	Component->SetRelativeScale3D(SizeCentimeters / 100.0f);
	// The chapter set never changes transform after assembly.  Keeping the
	// shell static lets Unreal cache shadows and avoids paying a movable-mesh
	// cost for hundreds of stairs, walls, bottle parts and tank panels.
	Component->SetMobility(EComponentMobility::Static);
	Component->SetCollisionProfileName(
		bCollision
			? UCollisionProfile::BlockAll_ProfileName
			: UCollisionProfile::NoCollision_ProfileName);
	Component->SetGenerateOverlapEvents(false);
	Component->SetCanEverAffectNavigation(false);
	Component->SetCastShadow(SizeCentimeters.GetMin() > 2.0f);
	Component->RegisterComponent();
	Geometry.Add(Component);
	return Component;
}

UPointLightComponent* AIGThirdMorningDirector::CreatePointLight(
	const FVector& LocalLocation,
	const float Intensity,
	const float Radius,
	const FLinearColor& Color,
	const bool bCastShadows)
{
	UPointLightComponent* Light = NewObject<UPointLightComponent>(
		this,
		*FString::Printf(TEXT("CH03_Light_%02d"), Lights.Num()));
	Light->SetupAttachment(SceneRoot);
	Light->SetRelativeLocation(LocalLocation);
	Light->SetMobility(EComponentMobility::Movable);
	Light->SetIntensity(Intensity);
	Light->SetAttenuationRadius(Radius);
	Light->SetLightColor(Color);
	Light->SetCastShadows(bCastShadows);
	Light->SetSourceRadius(8.0f);
	Light->SetSoftSourceRadius(18.0f);
	Light->RegisterComponent();
	Lights.Add(Light);
	return Light;
}

AIGChapterThreeAction* AIGThirdMorningDirector::SpawnAction(
	const EIGChapterThreeAction Action,
	const FVector& LocalLocation,
	const FVector& SizeCentimeters,
	UMaterialInterface* Material,
	const FText& Prompt,
	const float HoldSeconds,
	const FRotator& Rotation,
	UStaticMesh* MeshOverride)
{
	if (!GetWorld())
	{
		return nullptr;
	}

	FActorSpawnParameters Parameters;
	Parameters.Owner = this;
	Parameters.SpawnCollisionHandlingOverride =
		ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AIGChapterThreeAction* Target = GetWorld()->SpawnActor<AIGChapterThreeAction>(
		AIGChapterThreeAction::StaticClass(),
		FTransform(Rotation, ToWorld(LocalLocation)),
		Parameters);
	if (Target)
	{
		Target->Configure(
			this,
			Action,
			MeshOverride ? MeshOverride : CubeMesh.Get(),
			Material,
			SizeCentimeters,
			Prompt,
			HoldSeconds);
	}
	return Target;
}

AIGReadableNote* AIGThirdMorningDirector::SpawnNote(
	const FVector& LocalLocation,
	const FRotator& Rotation,
	const FVector& PaperSize,
	const FText& Prompt,
	const FText& Title,
	TArray<FText> BodyLines,
	UMaterialInterface* InPaperMaterial)
{
	if (!GetWorld() || !CubeMesh)
	{
		return nullptr;
	}

	FActorSpawnParameters Parameters;
	Parameters.Owner = this;
	Parameters.SpawnCollisionHandlingOverride =
		ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AIGReadableNote* Note = GetWorld()->SpawnActor<AIGReadableNote>(
		AIGReadableNote::StaticClass(),
		FTransform(Rotation, ToWorld(LocalLocation)),
		Parameters);
	if (Note)
	{
		Note->ConfigurePrototypeVisuals(
			CubeMesh,
			InPaperMaterial ? InPaperMaterial : PaperMaterial.Get(),
			PaperSize);
		Note->SetInteractionPrompt(Prompt);
		Note->SetNoteText(Title, MoveTemp(BodyLines));
		Note->OnReadStateChanged.AddUniqueDynamic(
			this, &ThisClass::HandleClueRead);
	}
	return Note;
}

AIGZoneTrigger* AIGThirdMorningDirector::SpawnZone(
	const FVector& LocalLocation,
	const FVector& HalfExtent)
{
	if (!GetWorld())
	{
		return nullptr;
	}

	FActorSpawnParameters Parameters;
	Parameters.Owner = this;
	Parameters.SpawnCollisionHandlingOverride =
		ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AIGZoneTrigger* Zone = GetWorld()->SpawnActor<AIGZoneTrigger>(
		AIGZoneTrigger::StaticClass(),
		FTransform(FRotator::ZeroRotator, ToWorld(LocalLocation)),
		Parameters);
	if (Zone)
	{
		Zone->SetZoneExtent(HalfExtent);
	}
	return Zone;
}

void AIGThirdMorningDirector::BuildApartment()
{
	// 6.8 m x 4.4 m one-room, scaled like the established 404 set.
	CreateBlock(FVector(0, 0, -10), FVector(700, 440, 20), RoomFloorMaterial);
	CreateBlock(FVector(0, -220, 130), FVector(700, 20, 280), WallMaterial);
	CreateBlock(FVector(0, 220, 130), FVector(700, 20, 280), WallMaterial);
	CreateBlock(FVector(-350, 0, 130), FVector(20, 440, 280), WallMaterial);
	CreateBlock(FVector(0, 0, 270), FVector(700, 440, 20), WallMaterial);
	// East wall leaves a 110 cm Korean apartment doorway.
	CreateBlock(FVector(350, -165, 130), FVector(20, 110, 280), WallMaterial);
	CreateBlock(FVector(350, 165, 130), FVector(20, 110, 280), WallMaterial);
	CreateBlock(FVector(350, 0, 250), FVector(20, 110, 40), WallMaterial);

	// Bed and tired possessions: the same person, but a wetter room.
	CreateBlock(FVector(-105, -125, 18), FVector(205, 105, 36), PlasticMaterial);
	CreateBlock(FVector(-105, -125, 39), FVector(194, 96, 10), BeddingMaterial);
	CreateBlock(FVector(-185, -125, 57), FVector(48, 82, 28), BeddingMaterial);
	CreateBlock(FVector(-225, -190, 35), FVector(58, 45, 70), PlasticMaterial);
	CreateBlock(FVector(-210, -190, 75), FVector(25, 18, 12), PlasticMaterial);
	CreateBlock(FVector(-200, -190, 80), FVector(22, 3, 6), ScreenMaterial, false);

	// A single ceiling leak and the stain beneath it.
	CreateBlock(FVector(45, -20, 0.8f), FVector(82, 54, 1.5f), WetStepMaterial, false);
	CreateBlock(FVector(45, -20, 266), FVector(18, 18, 3), WetStepMaterial, false);

	// Fridge carcass.  The contents are pre-built and merely revealed when the
	// door moves; no bottle allocations occur during the scare.  The pale
	// enamel cavity and familiar PET silhouette keep this domestic rather than
	// reading as a shelf of debug cylinders.
	CreateBlock(
		FVector(-286, 115, 92),
		FVector(105, 126, 184),
		FridgeBodyMaterial);
	CreateBlock(
		FVector(-239, 115, 92),
		FVector(4, 112, 172),
		FridgeInteriorMaterial,
		false);
	CreateBlock(
		FVector(-232, 60, 92),
		FVector(18, 6, 172),
		FridgeInteriorMaterial,
		false);
	CreateBlock(
		FVector(-232, 170, 92),
		FVector(18, 6, 172),
		FridgeInteriorMaterial,
		false);

	// A layered enamel frame and dark rubber gasket keep the front edge from
	// reading as a freestanding display shelf once the interaction door hides.
	CreateBlock(
		FVector(-219, 61, 94),
		FVector(8, 8, 176),
		FridgeBodyMaterial,
		false);
	CreateBlock(
		FVector(-219, 169, 94),
		FVector(8, 8, 176),
		FridgeBodyMaterial,
		false);
	CreateBlock(
		FVector(-219, 115, 8),
		FVector(8, 116, 8),
		FridgeBodyMaterial,
		false);
	CreateBlock(
		FVector(-219, 115, 180),
		FVector(8, 116, 8),
		FridgeBodyMaterial,
		false);
	CreateBlock(
		FVector(-213, 66, 93),
		FVector(3, 4, 156),
		PlasticMaterial,
		false);
	CreateBlock(
		FVector(-213, 164, 93),
		FVector(3, 4, 156),
		PlasticMaterial,
		false);
	CreateBlock(
		FVector(-213, 115, 17),
		FVector(3, 96, 4),
		PlasticMaterial,
		false);
	CreateBlock(
		FVector(-213, 115, 169),
		FVector(3, 96, 4),
		PlasticMaterial,
		false);

	// The opened door is pre-assembled and revealed with the bottles.  Its
	// liner, seal, bins and edge handle make this read as a domestic fridge
	// without allocating or moving components during the scare.
	auto HideUntilFridgeOpened = [this](UStaticMeshComponent* Part)
	{
		if (Part)
		{
			Part->SetVisibility(false);
			FridgeContents.Add(Part);
		}
	};
	HideUntilFridgeOpened(CreateBlock(
		FVector(-158, 174, 94),
		FVector(116, 8, 180),
		FridgeBodyMaterial,
		false));
	HideUntilFridgeOpened(CreateBlock(
		FVector(-158, 168, 94),
		FVector(104, 3, 166),
		FridgeInteriorMaterial,
		false));
	HideUntilFridgeOpened(CreateBlock(
		FVector(-158, 165, 14),
		FVector(104, 3, 5),
		PlasticMaterial,
		false));
	HideUntilFridgeOpened(CreateBlock(
		FVector(-158, 165, 174),
		FVector(104, 3, 5),
		PlasticMaterial,
		false));
	HideUntilFridgeOpened(CreateBlock(
		FVector(-207, 165, 94),
		FVector(5, 3, 156),
		PlasticMaterial,
		false));
	HideUntilFridgeOpened(CreateBlock(
		FVector(-109, 165, 94),
		FVector(5, 3, 156),
		PlasticMaterial,
		false));
	for (const float BinZ : {46.0f, 94.0f, 142.0f})
	{
		HideUntilFridgeOpened(CreateBlock(
			FVector(-158, 156, BinZ),
			FVector(92, 20, 5),
			FridgeInteriorMaterial,
			false));
		HideUntilFridgeOpened(CreateBlock(
			FVector(-158, 147, BinZ + 12.0f),
			FVector(92, 4, 24),
			FridgeInteriorMaterial,
			false));
	}
	HideUntilFridgeOpened(CreateBlock(
		FVector(-102, 172, 104),
		FVector(5, 10, 100),
		MetalMaterial,
		false));

	auto CreateHiddenWaterBottle = [this](const FVector& BottleLocation)
	{
		UStaticMeshComponent* BottleBody = CreateBlock(
			BottleLocation,
			FVector(100.0f),
			GlassMaterial,
			false,
			FRotator::ZeroRotator,
			WaterBottleMesh ? WaterBottleMesh.Get() : CylinderMesh.Get());
		UStaticMeshComponent* BottleCap = CreateBlock(
			BottleLocation + FVector(0, 0, 20.1f),
			FVector(100.0f),
			BottleCapMaterial,
			false,
			FRotator::ZeroRotator,
			BottleCapMesh ? BottleCapMesh.Get() : CylinderMesh.Get());
		UStaticMeshComponent* BottleLabel = CreateBlock(
			BottleLocation + FVector(0, 0, 5.0f),
			// CreateBlock registers static geometry after applying this authored
			// sleeve scale, so no static transform changes at runtime.
			FVector(336.0f, 336.0f, 860.0f),
			WaterLabelMaterial,
			false,
			FRotator(0, -90, 0),
			LabelSleeveMesh ? LabelSleeveMesh.Get() : CylinderMesh.Get());
		for (UStaticMeshComponent* BottlePart : {BottleBody, BottleCap, BottleLabel})
		{
			if (BottlePart)
			{
				BottlePart->SetVisibility(false);
				FridgeContents.Add(BottlePart);
			}
		}
	};

	// The door bins are part of the reveal too. Leaving them empty made the
	// supposedly impossible "full fridge" look like an ordinary half-stocked
	// appliance from the documentation camera.
	for (int32 BinShelf = 0; BinShelf < 3; ++BinShelf)
	{
		for (int32 Slot = 0; Slot < 4; ++Slot)
		{
			CreateHiddenWaterBottle(FVector(
				-195.0f + Slot * 25.0f,
				154.0f,
				47.5f + BinShelf * 48.0f));
		}
	}

	for (int32 Shelf = 0; Shelf < 4; ++Shelf)
	{
		CreateBlock(
			FVector(-224, 115, 38.0f + Shelf * 38.0f),
			FVector(48, 108, 2),
			MetalMaterial,
			false);
		for (int32 Row = 0; Row < 3; ++Row)
		{
			for (int32 Column = 0; Column < 8; ++Column)
			{
				const FVector BottleLocation(
					-241.0f + Row * 16.5f,
					67.5f + Column * 13.5f + (Row % 2) * 1.5f,
					39.5f + Shelf * 38.0f);
				CreateHiddenWaterBottle(BottleLocation);
			}
		}
	}
	FridgeInteriorLightPanel = CreateBlock(
		FVector(-215, 115, 173),
		FVector(3, 72, 5),
		FridgeInteriorMaterial,
		false);
	if (FridgeInteriorLightPanel)
	{
		FridgeInteriorLightPanel->SetVisibility(false);
	}
	FridgeInteriorLight = CreatePointLight(
		FVector(-202, 115, 144),
		165.0f,
		210.0f,
		FLinearColor(0.70f, 0.79f, 0.90f),
		false);
	if (FridgeInteriorLight)
	{
		FridgeInteriorLight->SetVisibility(false);
	}
	FridgeDoorAction = SpawnAction(
		EIGChapterThreeAction::OpenFridge,
		FVector(-218, 115, 94),
		FVector(8, 120, 180),
		FridgeBodyMaterial,
		NSLOCTEXT("IGCH03", "OpenFullFridge", "냉장고 열기"));

	// Desk under the window: planner note is placed later so it can own text.
	CreateBlock(FVector(80, 150, 38), FVector(145, 62, 8), PlasticMaterial);
	CreateBlock(FVector(25, 150, 18), FVector(8, 54, 36), PlasticMaterial);
	CreateBlock(FVector(135, 150, 18), FVector(8, 54, 36), PlasticMaterial);

	ApartmentDoorAction = SpawnAction(
		EIGChapterThreeAction::OpenApartmentDoor,
		FVector(348, 0, 112),
		FVector(8, 106, 224),
		MetalMaterial,
		NSLOCTEXT("IGCH03", "ApartmentDoorLockedPrompt", "현관문 확인하기"));

	CreatePointLight(
		FVector(-30, 0, 225),
		465.0f,
		430.0f,
		FLinearColor(0.47f, 0.56f, 0.68f),
		true);
	CreatePointLight(
		FVector(65, 145, 138),
		95.0f,
		280.0f,
		FLinearColor(0.62f, 0.42f, 0.28f),
		false);
}

void AIGThirdMorningDirector::BuildFloodedCorridor()
{
	// The wet 4F corridor starts at the apartment threshold and ends at the
	// stairwell.  Water is visual/non-colliding; the dry floor carries physics.
	CreateBlock(FVector(675, 0, -10), FVector(650, 440, 20), ConcreteMaterial);
	CreateBlock(FVector(675, -220, 130), FVector(650, 20, 280), DarkConcreteMaterial);
	CreateBlock(FVector(675, 220, 130), FVector(650, 20, 280), DarkConcreteMaterial);
	CreateBlock(FVector(675, 0, 270), FVector(650, 440, 20), DarkConcreteMaterial);
	CreateBlock(FVector(675, 0, 2), FVector(640, 430, 3), WaterMaterial, false);

	// Dead elevator: black COP, shut steel leaves, water bleeding from the sill.
	CreateBlock(FVector(690, 210, 112), FVector(210, 12, 224), MetalMaterial);
	CreateBlock(FVector(690, 201, 110), FVector(4, 3, 205), DarkConcreteMaterial, false);
	ElevatorAction = SpawnAction(
		EIGChapterThreeAction::InspectElevator,
		FVector(812, 204, 116),
		FVector(18, 5, 36),
		PlasticMaterial,
		NSLOCTEXT("IGCH03", "DeadLiftPrompt", "엘리베이터 호출하기"));
	CreateBlock(FVector(690, 188, 2.5f), FVector(170, 32, 3), WetStepMaterial, false);

	for (int32 LightIndex = 0; LightIndex < 3; ++LightIndex)
	{
		const float X = 455.0f + LightIndex * 230.0f;
		CreateBlock(
			FVector(X, 0, 250),
			FVector(68, 20, 4),
			ScreenMaterial,
			false);
		CreatePointLight(
			FVector(X, 0, 235),
			145.0f,
			260.0f,
			FLinearColor(0.18f, 0.48f, 0.33f),
			true);
	}
}

void AIGThirdMorningDirector::BuildLoopingStairwell()
{
	// Entry landing.
	CreateBlock(FVector(1085, 0, -10), FVector(200, 260, 20), ConcreteMaterial);
	CreateBlock(FVector(975, 0, 130), FVector(20, 760, 280), DarkConcreteMaterial);
	CreateBlock(FVector(1210, 180, 20), FVector(20, 500, 520), DarkConcreteMaterial);

	// Down flight.  Every tread has a constant bottom so collision remains
	// stable and there are no thin physics slivers.
	for (int32 StepIndex = 0; StepIndex < 12; ++StepIndex)
	{
		const float TopZ = -15.0f * StepIndex;
		const float Height = 260.0f + TopZ;
		CreateBlock(
			FVector(1085, 45.0f + StepIndex * 29.0f, TopZ - Height * 0.5f),
			FVector(200, 30, Height),
			ConcreteMaterial);
	}
	CreateBlock(FVector(1265, 382, -190), FVector(560, 130, 20), ConcreteMaterial);
	CreateBlock(FVector(1265, 447, -55), FVector(560, 20, 290), DarkConcreteMaterial);
	CreateBlock(FVector(1265, 317, -55), FVector(560, 20, 290), DarkConcreteMaterial);
	CreateBlock(FVector(1545, 382, -55), FVector(20, 150, 290), DarkConcreteMaterial);

	// The third loop makes the lower route physically unreadable: a waist-high
	// sheet of water occupies the first tread while the opposite gate opens.
	DownRouteWaterBarrier = CreateBlock(
		FVector(1085, 48, 78),
		FVector(205, 18, 156),
		WaterMaterial,
		true);
	if (DownRouteWaterBarrier)
	{
		DownRouteWaterBarrier->SetVisibility(false);
		DownRouteWaterBarrier->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}

	// Up flight, initially hidden behind a maintenance gate.
	for (int32 StepIndex = 0; StepIndex < 12; ++StepIndex)
	{
		const float TopZ = 15.0f * (StepIndex + 1);
		CreateBlock(
			FVector(1085, -45.0f - StepIndex * 29.0f, TopZ * 0.5f),
			FVector(200, 30, TopZ),
			ConcreteMaterial);
	}
	UpRouteGate = CreateBlock(
		FVector(1085, -42, 118),
		FVector(205, 16, 236),
		MetalMaterial,
		true);
	CreateBlock(
		FVector(1110, -30, 252),
		FVector(62, 18, 4),
		ScreenMaterial,
		false);
	CreatePointLight(
		FVector(1110, -30, 232),
		1180.0f,
		560.0f,
		FLinearColor(0.34f, 0.46f, 0.39f),
		false);

	// A fixed wall bulkhead and handrail reveal the stair pitch without
	// removing the dark landing beyond it.  These are static scene components;
	// there is no per-frame animation or spawning as the loop changes.
	CreateBlock(
		FVector(1188, -205, 226),
		FVector(5, 42, 22),
		ScreenMaterial,
		false);
	CreatePointLight(
		FVector(1168, -205, 220),
		880.0f,
		470.0f,
		FLinearColor(0.30f, 0.39f, 0.34f),
		false);
	CreateBlock(
		FVector(1174, -205, 181),
		FVector(8, 390, 8),
		MetalMaterial,
		false,
		FRotator(0, 0, -27));
	for (int32 RailPostIndex = 0; RailPostIndex < 4; ++RailPostIndex)
	{
		const float PostY = -58.0f - RailPostIndex * 102.0f;
		const float StepRise = 15.0f * (1.0f + RailPostIndex * 3.5f);
		CreateBlock(
			FVector(1174, PostY, StepRise + 68.0f),
			FVector(8, 8, 136),
			MetalMaterial,
			false);
	}

	// A real apartment fire-stair sign: painted steel, a large floor number,
	// and a route arrow. The fallback 3D font has no CJK glyphs, so the
	// direction is communicated by geometry instead of an implausible English
	// "ROOF EXIT" label in an old Korean villa.
	CreateBlock(
		FVector(986, 0, 145),
		FVector(4, 180, 110),
		DarkConcreteMaterial,
		false);
	CreateBlock(
		FVector(989, 0, 145),
		FVector(1.5f, 176, 106),
		MetalMaterial,
		false);
	StairSignText = NewObject<UTextRenderComponent>(this, TEXT("CH03_StairSignText"));
	StairSignText->SetupAttachment(SceneRoot);
	StairSignText->SetRelativeLocation(FVector(991, 28, 157));
	StairSignText->SetRelativeRotation(FRotator::ZeroRotator);
	StairSignText->SetHorizontalAlignment(EHorizTextAligment::EHTA_Center);
	StairSignText->SetVerticalAlignment(EVerticalTextAligment::EVRTA_TextCenter);
	StairSignText->SetWorldSize(46.0f);
	StairSignText->SetTextRenderColor(FColor(188, 194, 184));
	StairSignText->SetText(FText::FromString(TEXT("4F")));
	StairSignText->RegisterComponent();

	StairRoofText = NewObject<UTextRenderComponent>(this, TEXT("CH03_StairRoofText"));
	StairRoofText->SetupAttachment(SceneRoot);
	StairRoofText->SetRelativeLocation(FVector(991, -23, 112));
	StairRoofText->SetRelativeRotation(FRotator::ZeroRotator);
	StairRoofText->SetHorizontalAlignment(EHorizTextAligment::EHTA_Center);
	StairRoofText->SetVerticalAlignment(EVerticalTextAligment::EVRTA_TextCenter);
	StairRoofText->SetWorldSize(14.0f);
	StairRoofText->SetTextRenderColor(FColor(180, 186, 176));
	StairRoofText->SetText(FText::GetEmpty());
	StairRoofText->SetVisibility(false);
	StairRoofText->RegisterComponent();

	// Three strokes form a familiar upward arrow. The old five-block pyramid
	// read as a cross at this camera angle.
	const struct
	{
		FVector Center;
		FVector Size;
		FRotator Rotation;
	} ArrowBlocks[] = {
		{FVector(991, 49, 118), FVector(2.8f, 6, 42), FRotator::ZeroRotator},
		{FVector(991, 41.5f, 132), FVector(2.8f, 22, 4), FRotator(0, 0, -42)},
		{FVector(991, 56.5f, 132), FVector(2.8f, 22, 4), FRotator(0, 0, 42)}
	};
	for (const auto& BlockData : ArrowBlocks)
	{
		if (UStaticMeshComponent* Stroke = CreateBlock(
			BlockData.Center,
			BlockData.Size,
			PaperMaterial,
			false,
			BlockData.Rotation))
		{
			Stroke->SetVisibility(false);
			StairSignParts.Add(Stroke);
		}
	}

	// A thin paint run appears on the second impossible return.
	if (UStaticMeshComponent* Drip = CreateBlock(
			FVector(991, 28, 116),
			FVector(2, 2, 20),
			WetPaperMaterial,
			false);
		Drip)
	{
		Drip->SetVisibility(false);
		StairLatinSignParts.Add(Drip);
	}
}

void AIGThirdMorningDirector::BuildFifthFloorAndRoof()
{
	// Unfinished fifth-floor landing.
	CreateBlock(FVector(1240, -400, 170), FVector(420, 250, 20), ConcreteMaterial);
	CreateBlock(FVector(1450, -400, 205), FVector(18, 250, 410), DarkConcreteMaterial);
	for (int32 BeamIndex = 0; BeamIndex < 4; ++BeamIndex)
	{
		CreateBlock(
			FVector(1100 + BeamIndex * 120, -510, 325),
			FVector(16, 16, 310),
			MetalMaterial);
	}
	// Hanging vinyl: a thin non-colliding sheet, motionless air made suspect
	// by its skewed pose.
	CreateBlock(
		FVector(1390, -335, 300),
		FVector(3, 155, 210),
		WetPaperMaterial,
		false,
		FRotator(0, 0, 7));

	// Four small risers connect the landing to the roof slab.
	for (int32 StepIndex = 0; StepIndex < 4; ++StepIndex)
	{
		const float TopZ = 195.0f + StepIndex * 15.0f;
		CreateBlock(
			FVector(1510.0f + StepIndex * 36.0f, -400, 180.0f + (TopZ - 180.0f) * 0.5f),
			FVector(38, 180, TopZ - 180.0f),
			ConcreteMaterial);
	}

	RoofDoorAction = SpawnAction(
		EIGChapterThreeAction::OpenRoofDoor,
		FVector(1662, -400, 355),
		FVector(12, 116, 230),
		MetalMaterial,
		NSLOCTEXT("IGCH03", "RoofDoorPrompt", "옥상 철문 열기"));
	KeysAction = SpawnAction(
		EIGChapterThreeAction::InspectKeys,
		FVector(1625, -342, 330),
		FVector(7, 18, 8),
		MetalMaterial,
		NSLOCTEXT("IGCH03", "KeysPrompt", "꽂힌 열쇠뭉치 확인하기"));

	// Wide roof: the absence of traffic and animals is legible because there
	// is room for the player to wait and hear nothing.
	CreateBlock(
		FVector(2360, -400, IGThirdMorning::RoofFloorZ - 10),
		FVector(1450, 1180, 20),
		ConcreteMaterial);
	CreateBlock(FVector(2360, -990, 290), FVector(1450, 20, 100), DarkConcreteMaterial);
	CreateBlock(FVector(2360, 190, 290), FVector(1450, 20, 100), DarkConcreteMaterial);
	CreateBlock(FVector(3085, -400, 290), FVector(20, 1180, 100), DarkConcreteMaterial);
	CreateBlock(FVector(1690, -820, 290), FVector(20, 340, 100), DarkConcreteMaterial);
	CreateBlock(FVector(1690, 20, 290), FVector(20, 340, 100), DarkConcreteMaterial);

	// Expansion joints, a grated drain and a code-like parapet rail give the
	// roof a readable scale even when the predawn sky remains almost black.
	CreateBlock(
		FVector(2380, -690, 241.5f),
		FVector(1160, 5, 3),
		DarkConcreteMaterial,
		false);
	CreateBlock(
		FVector(2070, -385, 241.5f),
		FVector(5, 610, 3),
		DarkConcreteMaterial,
		false);
	CreateBlock(
		FVector(2860, -510, 243),
		FVector(120, 24, 5),
		MetalMaterial,
		false);
	for (int32 DrainSlotIndex = 0; DrainSlotIndex < 5; ++DrainSlotIndex)
	{
		CreateBlock(
			FVector(2816.0f + DrainSlotIndex * 22.0f, -510, 246),
			FVector(10, 27, 2),
			DarkConcreteMaterial,
			false);
	}
	for (int32 RailPostIndex = 0; RailPostIndex < 7; ++RailPostIndex)
	{
		CreateBlock(
			FVector(1750.0f + RailPostIndex * 210.0f, 174, 390),
			FVector(9, 9, 110),
			MetalMaterial,
			false);
	}
	CreateBlock(
		FVector(2380, 174, 365),
		FVector(1350, 9, 9),
		MetalMaterial,
		false);
	CreateBlock(
		FVector(2380, 174, 435),
		FVector(1350, 9, 9),
		MetalMaterial,
		false);

	// Broken salt line across the threshold.
	CreateBlock(FVector(1690, -455, 242), FVector(3, 58, 2), PaperMaterial, false);
	CreateBlock(FVector(1690, -345, 242), FVector(3, 45, 2), PaperMaterial, false);

	// One wet, dry leaf where no tree exists.
	CreateBlock(
		FVector(1905, -250, 242),
		FVector(18, 6, 1),
		WetPaperMaterial,
		false,
		FRotator(0, 28, 11));

	// Distant Korean rooftop landmark: a muted red church cross.
	CreateBlock(FVector(3020, 120, 520), FVector(12, 12, 220), ScreenMaterial, false);
	CreateBlock(FVector(3020, 120, 555), FVector(85, 12, 12), ScreenMaterial, false);

	// A caged maintenance lamp and the stale red emergency lamp separate the
	// tank, service stair and parapet without flattening the predawn darkness.
	CreateBlock(
		FVector(2145, -815, 470),
		FVector(24, 10, 34),
		MetalMaterial,
		false);
	CreateBlock(
		FVector(2145, -808, 470),
		FVector(16, 4, 22),
		EmergencyMaterial,
		false);

	// The cool roof fill now has a visible maintenance mast instead of an
	// unexplained floating highlight.
	CreateBlock(
		FVector(2680, -590, 395),
		FVector(10, 10, 310),
		MetalMaterial,
		false);
	CreateBlock(
		FVector(2655, -568, 548),
		FVector(68, 12, 10),
		MetalMaterial,
		false,
		FRotator(0, -28, 0));
	CreateBlock(
		FVector(2630, -546, 542),
		FVector(42, 24, 8),
		ScreenMaterial,
		false,
		FRotator(0, -28, 0));
	CreatePointLight(
		FVector(2630, -546, 530),
		4200.0f,
		1480.0f,
		FLinearColor(0.36f, 0.47f, 0.68f),
		false);
	CreatePointLight(
		FVector(2160, -770, 490),
		940.0f,
		790.0f,
		FLinearColor(0.62f, 0.065f, 0.035f),
		true);
	CreatePointLight(
		FVector(2350, -120, 520),
		780.0f,
		620.0f,
		FLinearColor(0.16f, 0.30f, 0.46f),
		false);
}

void AIGThirdMorningDirector::BuildWaterTank()
{
	const FVector Tank = IGThirdMorning::TankCenter;

	// Concrete plinth and a sixteen-panel cylindrical shell.  Panels are
	// cheaper than a masked custom mesh and leave the open top genuinely open.
	CreateBlock(Tank + FVector(0, 0, 50 + IGThirdMorning::RoofFloorZ),
		FVector(360, 360, 100), ConcreteMaterial);
	for (int32 PanelIndex = 0; PanelIndex < 16; ++PanelIndex)
	{
		const float AngleDegrees = PanelIndex * 22.5f;
		const float AngleRadians = FMath::DegreesToRadians(AngleDegrees);
		const FVector Radial(
			FMath::Cos(AngleRadians) * 145.0f,
			FMath::Sin(AngleRadians) * 145.0f,
			0.0f);
		CreateBlock(
			Tank + Radial + FVector(0, 0, 470),
			FVector(58, 12, 260),
			MetalMaterial,
			true,
			FRotator(0, AngleDegrees + 90.0f, 0));
	}

	// Three proud reinforcement rings catch long highlights across several
	// facets, visually joining the sixteen panels into an industrial tank.
	for (const float BandZ : {352.0f, 470.0f, 588.0f})
	{
		for (int32 SegmentIndex = 0; SegmentIndex < 16; ++SegmentIndex)
		{
			const float AngleDegrees = SegmentIndex * 22.5f;
			const float AngleRadians = FMath::DegreesToRadians(AngleDegrees);
			const FVector Radial(
				FMath::Cos(AngleRadians) * 153.0f,
				FMath::Sin(AngleRadians) * 153.0f,
				0.0f);
			CreateBlock(
				Tank + Radial + FVector(0, 0, BandZ),
				FVector(61, 20, 12),
				MetalMaterial,
				false,
				FRotator(0, AngleDegrees + 90.0f, 0));
		}
	}

	// Visible inlet pipe, clamps and valve connect the tank to the building.
	// Cylinders are static and non-colliding, so this detail adds no gameplay
	// physics or per-frame work.
	CreateBlock(
		Tank + FVector(128, -128, 390),
		FVector(18, 18, 300),
		MetalMaterial,
		false,
		FRotator::ZeroRotator,
		CylinderMesh);
	CreateBlock(
		Tank + FVector(248, -128, 254),
		FVector(16, 16, 240),
		MetalMaterial,
		false,
		FRotator(90, 0, 0),
		CylinderMesh);
	for (const float ClampZ : {330.0f, 450.0f})
	{
		CreateBlock(
			Tank + FVector(128, -128, ClampZ),
			FVector(26, 26, 8),
			DarkConcreteMaterial,
			false,
			FRotator::ZeroRotator,
			CylinderMesh);
	}
	CreateBlock(
		Tank + FVector(128, -151, 410),
		FVector(46, 46, 8),
		MetalMaterial,
		false,
		FRotator(0, 0, 90),
		CylinderMesh);

	// A small service lamp on the pipe side lifts the lower shell and bands,
	// while the opposite side remains available for the flashlight scare.
	CreateBlock(
		Tank + FVector(145, -151, 570),
		FVector(34, 18, 24),
		MetalMaterial,
		false);
	CreateBlock(
		Tank + FVector(158, -164, 570),
		FVector(18, 6, 14),
		ScreenMaterial,
		false);
	CreatePointLight(
		Tank + FVector(170, -176, 560),
		1050.0f,
		660.0f,
		FLinearColor(0.34f, 0.45f, 0.58f),
		false);

	TankWaterSurface = CreateBlock(
		Tank + FVector(0, 0, 542),
		FVector(270, 270, 3),
		WaterMaterial,
		false);
	CreatePointLight(
		Tank + FVector(-38, -42, 586),
		390.0f,
		330.0f,
		FLinearColor(0.18f, 0.40f, 0.58f),
		false);
	CreatePointLight(
		Tank + FVector(72, 68, 576),
		90.0f,
		235.0f,
		FLinearColor(0.34f, 0.045f, 0.028f),
		false);

	// Service stair disguised with ladder rails/rungs.  The 20 cm risers are
	// within CharacterMovement step height, avoiding bespoke ladder physics.
	for (int32 StepIndex = 0; StepIndex < 18; ++StepIndex)
	{
		const float TopZ = IGThirdMorning::RoofFloorZ + 20.0f * (StepIndex + 1);
		const float Height = TopZ - IGThirdMorning::RoofFloorZ;
		CreateBlock(
			FVector(1915.0f + StepIndex * 20.0f, -300, IGThirdMorning::RoofFloorZ + Height * 0.5f),
			FVector(22, 105, Height),
			MetalMaterial);
	}
	CreateBlock(FVector(2265, -365, 455), FVector(12, 12, 430), MetalMaterial);
	CreateBlock(FVector(2265, -235, 455), FVector(12, 12, 430), MetalMaterial);
	for (int32 RungIndex = 0; RungIndex < 10; ++RungIndex)
	{
		CreateBlock(
			FVector(2265, -300, 285.0f + RungIndex * 34.0f),
			FVector(10, 136, 7),
			MetalMaterial);
	}
	CreateBlock(FVector(2325, -300, 610), FVector(150, 220, 20), MetalMaterial);

	GlassesAction = SpawnAction(
		EIGChapterThreeAction::InspectGlasses,
		FVector(1880, -250, 247),
		FVector(16, 5, 3),
		PlasticMaterial,
		NSLOCTEXT("IGCH03", "GlassesPrompt", "젖은 안경 확인하기"));
	// Two temple arms make the block immediately read as black horn-rimmed glasses.
	CreateBlock(FVector(1882, -242, 249), FVector(20, 2, 2), PlasticMaterial, false);
	CreateBlock(FVector(1882, -258, 249), FVector(20, 2, 2), PlasticMaterial, false);

	TankLidAction = SpawnAction(
		EIGChapterThreeAction::OpenTank,
		Tank + FVector(-25, 0, 655),
		FVector(310, 310, 8),
		MetalMaterial,
		NSLOCTEXT("IGCH03", "OpenTankPrompt", "물탱크 뚜껑 열기"),
		1.2f,
		FRotator(0, 0, 58),
		CylinderMesh);

	CloseChoiceAction = SpawnAction(
		EIGChapterThreeAction::CloseTank,
		Tank + FVector(-155, -82, 625),
		FVector(24, 10, 8),
		MetalMaterial,
		NSLOCTEXT("IGCH03", "CloseTankPrompt", "뚜껑을 닫는다"),
		0.8f);
	EnterChoiceAction = SpawnAction(
		EIGChapterThreeAction::EnterTank,
		Tank + FVector(-155, 105, 625),
		FVector(44, 28, 8),
		DarkConcreteMaterial,
		NSLOCTEXT(
			"IGCH03",
			"EnterTankPrompt",
			"안경을 물 위에 놓고 뚜껑을 열어 둔다"),
		1.2f);
	SetVisibleInteractive(CloseChoiceAction, false);
	SetVisibleInteractive(EnterChoiceAction, false);

	// Concentric broken highlights make the otherwise still surface read as
	// water rather than a blue floor.  They are deliberately sparse: the only
	// motion after the lid lifts is the player's own light and these ripples.
	for (int32 RingIndex = 0; RingIndex < 3; ++RingIndex)
	{
		const float RadiusX = 62.0f + RingIndex * 32.0f;
		const float RadiusY = 40.0f + RingIndex * 24.0f;
		const int32 SegmentCount = 12;
		for (int32 SegmentIndex = 0; SegmentIndex < SegmentCount; ++SegmentIndex)
		{
			// Missing segments stop the pattern looking like a UI reticle.
			if ((SegmentIndex + RingIndex * 2) % 5 == 0)
			{
				continue;
			}
			const float Angle = 2.0f * PI
				* static_cast<float>(SegmentIndex)
				/ static_cast<float>(SegmentCount);
			CreateBlock(
				Tank + FVector(
					-4.0f + FMath::Cos(Angle) * RadiusX,
					2.0f + FMath::Sin(Angle) * RadiusY,
					544.0f + RingIndex * 0.25f),
				FVector(24.0f + RingIndex * 3.0f, 1.8f, 0.7f),
				WetStepMaterial,
				false,
				FRotator(0, FMath::RadiansToDegrees(Angle) + 90.0f, 0));
		}
	}

	// A curled, back-facing human silhouette assembled from rounded primitives.
	// It stays non-graphic, but shoulders, elbows, knees and wet clothing make
	// it unmistakably human instead of a rectangular placeholder.
	auto AddBodyPiece =
		[this, &Tank](
			const FVector& Offset,
			const FVector& Size,
			UMaterialInterface* Material,
			UStaticMesh* Mesh,
			const FRotator& Rotation = FRotator::ZeroRotator)
	{
		if (UStaticMeshComponent* Piece = CreateBlock(
			Tank + Offset + FVector(0, 0, -10.0f),
			Size,
			Material,
			false,
			Rotation,
			Mesh))
		{
			Piece->SetVisibility(false);
			BodySilhouette.Add(Piece);
		}
	};
	auto AddLimb =
		[this, &Tank, &AddBodyPiece](
			const FVector& Start,
			const FVector& End,
			const float Diameter,
			UMaterialInterface* Material)
	{
		const FVector Delta = End - Start;
		const FRotator Rotation = FQuat::FindBetweenNormals(
			FVector::UpVector,
			Delta.GetSafeNormal()).Rotator();
		AddBodyPiece(
			(Start + End) * 0.5f,
			FVector(Diameter, Diameter, Delta.Size()),
			Material,
			CylinderMesh,
			Rotation);
		// A rounded joint hides the cylinder end and keeps bent limbs reading
		// as one submerged human silhouette instead of detached oval pieces.
		AddBodyPiece(
			End,
			FVector(Diameter * 1.08f),
			Material,
			SphereMesh);
	};

	AddBodyPiece(
		FVector(8, 4, 553),
		FVector(92, 46, 25),
		WetStepMaterial,
		SphereMesh,
		FRotator(0, 16, -3));
	AddBodyPiece(
		FVector(-34, -3, 551),
		FVector(52, 43, 27),
		BeddingMaterial,
		SphereMesh,
		FRotator(0, 12, 0));
	AddBodyPiece(
		FVector(62, 15, 556),
		FVector(31, 29, 29),
		WetStepMaterial,
		SphereMesh,
		FRotator(0, 9, 0));
	AddBodyPiece(
		FVector(66, 18, 563),
		FVector(36, 34, 17),
		PlasticMaterial,
		SphereMesh,
		FRotator(0, 14, 8));
	AddBodyPiece(
		FVector(50, 11, 553),
		FVector(18, 19, 18),
		WetStepMaterial,
		SphereMesh);

	// Arms folded in toward the chest.
	AddLimb(FVector(30, -10, 554), FVector(7, -38, 551), 13.5f, WetStepMaterial);
	AddLimb(FVector(7, -38, 551), FVector(-20, -24, 548), 11.5f, WetStepMaterial);
	AddBodyPiece(
		FVector(-23, -22, 548),
		FVector(13, 10, 8),
		WetStepMaterial,
		SphereMesh,
		FRotator(0, -12, 0));
	AddLimb(FVector(31, 20, 554), FVector(4, 44, 551), 13.5f, WetStepMaterial);
	AddLimb(FVector(4, 44, 551), FVector(-25, 31, 548), 11.5f, WetStepMaterial);
	AddBodyPiece(
		FVector(-28, 29, 548),
		FVector(13, 10, 8),
		WetStepMaterial,
		SphereMesh,
		FRotator(0, 10, 0));

	// Bent legs give the same uneasy foetal posture glimpsed in room 403.
	AddLimb(FVector(-30, -8, 551), FVector(-63, -42, 549), 20.0f, BeddingMaterial);
	AddLimb(FVector(-63, -42, 549), FVector(-107, -21, 547), 16.5f, BeddingMaterial);
	AddBodyPiece(
		FVector(-115, -17, 547),
		FVector(28, 16, 12),
		PlasticMaterial,
		SphereMesh,
		FRotator(0, -19, 0));
	AddLimb(FVector(-33, 8, 551), FVector(-62, 38, 550), 20.0f, BeddingMaterial);
	AddLimb(FVector(-62, 38, 550), FVector(-102, 27, 547), 16.5f, BeddingMaterial);
	AddBodyPiece(
		FVector(-111, 25, 547),
		FVector(28, 16, 12),
		PlasticMaterial,
		SphereMesh,
		FRotator(0, 11, 0));
}

void AIGThirdMorningDirector::SpawnClueDocuments()
{
	PlannerNote = SpawnNote(
		FVector(80, 150, 44),
		FRotator::ZeroRotator,
		FVector(30, 22, 1.2f),
		NSLOCTEXT("IGCH03", "PlannerPrompt", "수험 플래너 확인하기"),
		NSLOCTEXT("IGCH03", "PlannerTitle", "7월 기상·근무 플래너"),
		{
			NSLOCTEXT("IGCH03", "PlannerL1", "평일  05:10 기상  ·  05:30 캠프 집결"),
			NSLOCTEXT("IGCH03", "PlannerL2", "7/24(수)  모의고사 채점"),
			NSLOCTEXT("IGCH03", "PlannerL3", "7/25(목)  생수 · 골목 고양이 밥"),
			NSLOCTEXT("IGCH03", "PlannerL4", "7/26(금)  __________________")
		},
		PaperMaterial);

	MotherPhoneNote = SpawnNote(
		FVector(122, 150, 44.4f),
		FRotator(0, 0, -8),
		FVector(16, 8, 1.4f),
		NSLOCTEXT("IGCH03", "MotherPhonePrompt", "금 간 휴대폰 확인하기"),
		NSLOCTEXT("IGCH03", "MotherPhoneTitle", "엄마"),
		{
			NSLOCTEXT(
				"IGCH03",
				"MotherPhoneL1",
				"엄마  7/22  쌀 보냈다. 물 많이 마시고 다녀라."),
			NSLOCTEXT(
				"IGCH03",
				"MotherPhoneL2",
				"엄마  7/25  주말에 내려오니?"),
			NSLOCTEXT(
				"IGCH03",
				"MotherPhoneL3",
				"읽지 않음  7/26 07:02  지운아")
		},
		PlasticMaterial);
	// Two hairline screen cracks are enough to identify the object before the
	// reading panel opens, without making a bespoke phone UI asset.
	CreateBlock(
		FVector(121, 150, 45.25f),
		FVector(7, 0.5f, 0.25f),
		ScreenMaterial,
		false,
		FRotator(0, -17, 0));
	CreateBlock(
		FVector(124, 150, 45.28f),
		FVector(5, 0.5f, 0.25f),
		ScreenMaterial,
		false,
		FRotator(0, 23, 0));

	OfferingNote = SpawnNote(
		FVector(1265, -485, 184),
		FRotator::ZeroRotator,
		FVector(25, 18, 1.2f),
		NSLOCTEXT("IGCH03", "OfferingPrompt", "엎어진 정화수 옆 쪽지 읽기"),
		NSLOCTEXT("IGCH03", "OfferingTitle", "삐뚤한 글씨"),
		{
			NSLOCTEXT("IGCH03", "OfferingL1", "목마른 사람은"),
			NSLOCTEXT("IGCH03", "OfferingL2", "이 물을 드시오."),
			NSLOCTEXT("IGCH03", "OfferingL3", "— 401호")
		},
		WetPaperMaterial);
	// Overturned bowl and its spilled water.
	CreateBlock(
		FVector(1228, -485, 190),
		FVector(24, 24, 10),
		MetalMaterial,
		false,
		FRotator(20, 0, 18),
		CylinderMesh);
	CreateBlock(FVector(1215, -470, 182), FVector(55, 38, 2), WaterMaterial, false);

	EstimateNote = SpawnNote(
		FVector(1765, -505, 246),
		FRotator::ZeroRotator,
		FVector(31, 22, 1.2f),
		NSLOCTEXT("IGCH03", "EstimatePrompt", "젖은 작업 확인표 읽기"),
		NSLOCTEXT("IGCH03", "EstimateTitle", "저수조 작업·확인 기록"),
		{
			NSLOCTEXT("IGCH03", "EstimateL1", "미르워터텍  ·  2024년 7월 26일(금)"),
			NSLOCTEXT("IGCH03", "EstimateL2", "03:50  단수 밸브 잠금 / 옥상 개방"),
			NSLOCTEXT("IGCH03", "EstimateL3", "03:56  비용 재검토로 작업 연기"),
			NSLOCTEXT("IGCH03", "EstimateL4", "□ 열쇠 회수     □ 뚜껑 확인"),
			NSLOCTEXT("IGCH03", "EstimateL5", "7/29 제출  “옥상 잠김 / 작업 취소”"),
			NSLOCTEXT("IGCH03", "EstimateL6", "현장 담당  강만식          [미확인]")
		},
		WetPaperMaterial);
}

void AIGThirdMorningDirector::SpawnTriggers()
{
	CorridorEntryZone = SpawnZone(FVector(425, 0, 110), FVector(42, 92, 115));
	if (CorridorEntryZone)
	{
		CorridorEntryZone->OnZoneTriggered.AddUniqueDynamic(
			this, &ThisClass::HandleCorridorEntered);
	}

	for (int32 LoopIndex = 0; LoopIndex < 3; ++LoopIndex)
	{
		AIGZoneTrigger* LoopZone = SpawnZone(
			FVector(1465, 382, -84),
			FVector(48, 48, 95));
		if (LoopZone)
		{
			LoopZone->OnZoneTriggered.AddUniqueDynamic(
				this, &ThisClass::HandleStairLoop);
			LoopZone->SetActorEnableCollision(LoopIndex == 0);
			StairLoopZones.Add(LoopZone);
		}
	}

	FifthFloorZone = SpawnZone(FVector(1190, -385, 276), FVector(95, 62, 100));
	if (FifthFloorZone)
	{
		FifthFloorZone->OnZoneTriggered.AddUniqueDynamic(
			this, &ThisClass::HandleFifthFloorEntered);
	}

	LadderZone = SpawnZone(FVector(2075, -300, 405), FVector(65, 90, 155));
	if (LadderZone)
	{
		LadderZone->OnZoneTriggered.AddUniqueDynamic(
			this, &ThisClass::HandleLadderEntered);
	}
}

// ---------------------------------------------------------------------------
// Story flow and interactions
// ---------------------------------------------------------------------------

void AIGThirdMorningDirector::BeginAwakening()
{
	if (!GetWorld())
	{
		return;
	}

	SetPhase(EIGThirdMorningPhase::Awakening);
	if (APlayerController* PlayerController = GetWorld()->GetFirstPlayerController())
	{
		if (APlayerCameraManager* Camera = PlayerController->PlayerCameraManager)
		{
			Camera->StopCameraFade();
			Camera->StartCameraFade(
				1.0f, 0.0f, 1.2f, FLinearColor::Black, false, false);
		}
	}

	UIGAlarmSoundWave* Alarm = NewObject<UIGAlarmSoundWave>(this, TEXT("CH03SelfStoppingAlarm"));
	AlarmComponent = IGAudio::SpawnOneShotAt(
		this,
		Alarm,
		ToWorld(FVector(-205, -190, 82)),
		0.82f,
		0.983f,
		70.0f,
		500.0f);

	GetWorldTimerManager().SetTimer(
		AlarmTimer,
		this,
		&ThisClass::StopAlarmByItself,
		4.0f,
		false);
}

void AIGThirdMorningDirector::StopAlarmByItself()
{
	if (bAlarmStopped)
	{
		return;
	}
	bAlarmStopped = true;
	if (AlarmComponent)
	{
		AlarmComponent->Stop();
		AlarmComponent = nullptr;
	}

	// Let the absence land. Capture/smoke mode skips the wait, while the
	// playable route holds four full seconds before naming what happened.
	if (bCaptureMode)
	{
		SetPhase(EIGThirdMorningPhase::ApartmentClues);
		return;
	}
	GetWorldTimerManager().SetTimer(
		FlowTimer,
		this,
		&ThisClass::EndOpeningSilence,
		4.0f,
		false);
}

void AIGThirdMorningDirector::EndOpeningSilence()
{
	AIGHorrorHUD::PushThought(
		this,
		NSLOCTEXT("IGCH03", "AlarmStoppedThought", "…저절로 꺼졌다."),
		3.2f);
	SetPhase(EIGThirdMorningPhase::ApartmentClues);
}

void AIGThirdMorningDirector::HandleAction(
	const EIGChapterThreeAction Action,
	AIGChapterThreeAction* Source)
{
	switch (Action)
	{
	case EIGChapterThreeAction::OpenFridge:
		if (bFridgeInspected)
		{
			return;
		}
		bFridgeInspected = true;
		if (Source)
		{
			Source->SetActorHiddenInGame(true);
			Source->SetActorEnableCollision(false);
			Source->SetInteractionEnabled(false);
		}
		for (UStaticMeshComponent* Bottle : FridgeContents)
		{
			if (Bottle)
			{
				Bottle->SetVisibility(true);
			}
		}
		if (FridgeInteriorLightPanel)
		{
			FridgeInteriorLightPanel->SetVisibility(true);
		}
		if (FridgeInteriorLight)
		{
			FridgeInteriorLight->SetVisibility(true);
		}
		AIGHorrorHUD::PushThought(
			this,
			NSLOCTEXT("IGCH03", "FullFridgeThought", "누가… 새벽샘물로 가득 채워 놨어."),
			4.0f);
		IGStory::AddState(
			this,
			FGameplayTag::RequestGameplayTag(
				FName(TEXT("State.CH03.Flood.FridgeFull")), false));
		TryUnlockApartmentExit();
		break;

	case EIGChapterThreeAction::OpenApartmentDoor:
		if (!bAlarmStopped)
		{
			AIGHorrorHUD::PushThought(
				this,
				NSLOCTEXT("IGCH03", "WaitAlarmThought", "알람이… 내 손도 안 댔는데."),
				2.8f);
			return;
		}
		if (!bFridgeInspected || !bPlannerRead || !bMotherPhoneRead)
		{
			AIGHorrorHUD::PushThought(
				this,
				!bFridgeInspected
					? NSLOCTEXT("IGCH03", "NeedFridgeThought", "냉장고 소리가 이상하다.")
					: !bPlannerRead
						? NSLOCTEXT("IGCH03", "NeedPlannerThought", "책상 위 플래너부터 확인하자.")
						: NSLOCTEXT("IGCH03", "NeedPhoneThought", "플래너 옆 휴대폰이 켜져 있다."),
				3.2f);
			return;
		}
		if (Source)
		{
			Source->SetActorHiddenInGame(true);
			Source->SetActorEnableCollision(false);
			Source->SetInteractionEnabled(false);
		}
		break;

	case EIGChapterThreeAction::InspectElevator:
		AIGHorrorHUD::PushThought(
			this,
			NSLOCTEXT("IGCH03", "DeadLiftThought", "불도, 호출음도 없다. 문틈에서 물만 나온다."),
			4.0f);
		PlayDelayedSplash();
		break;

	case EIGChapterThreeAction::InspectKeys:
		if (!bKeysInspected)
		{
			bKeysInspected = true;
			AIGHorrorHUD::PushThought(
				this,
				NSLOCTEXT("IGCH03", "KeysThought", "관리사무소 열쇠다. 그날부터 꽂혀 있었던 건가."),
				4.2f);
			IGStory::AddState(
				this,
				FGameplayTag::RequestGameplayTag(
					FName(TEXT("State.CH03.Flood.FoundKeys")), false));
			if (RoofDoorAction)
			{
				RoofDoorAction->SetInteractionPrompt(
					NSLOCTEXT("IGCH03", "RoofDoorUnlockedPrompt", "열쇠로 옥상 철문 열기"));
			}
		}
		break;

	case EIGChapterThreeAction::OpenRoofDoor:
		if (StairLoopCount < 3)
		{
			AIGHorrorHUD::PushThought(
				this,
				NSLOCTEXT("IGCH03", "NeedLoopsThought", "아래쪽부터 확인해야 한다."),
				2.8f);
			return;
		}
		if (!bKeysInspected)
		{
			AIGHorrorHUD::PushThought(
				this,
				NSLOCTEXT("IGCH03", "NeedKeysThought", "자물쇠에 열쇠뭉치가 그대로 꽂혀 있다."),
				3.4f);
			return;
		}
		if (Source)
		{
			Source->SetActorHiddenInGame(true);
			Source->SetActorEnableCollision(false);
			Source->SetInteractionEnabled(false);
		}
		EnterRoofSilence();
		break;

	case EIGChapterThreeAction::InspectGlasses:
		if (!bGlassesInspected)
		{
			bGlassesInspected = true;
			AIGHorrorHUD::PushThought(
				this,
				NSLOCTEXT("IGCH03", "GlassesThought", "…내 안경은, 여기 떨어져 있는데."),
				4.4f);
			PlayMetalEcho(FVector(2265, -300, 360), 1.0f);
			IGStory::AddState(
				this,
				FGameplayTag::RequestGameplayTag(
					FName(TEXT("State.CH03.Flood.FoundGlasses")), false));
		}
		break;

	case EIGChapterThreeAction::OpenTank:
		if (!bEstimateRead)
		{
			AIGHorrorHUD::PushThought(
				this,
				NSLOCTEXT(
					"IGCH03",
					"NeedWorkSheetThought",
					"철문 안쪽에 젖은 작업표가 붙어 있다."),
				3.6f);
			return;
		}
		if (!bGlassesInspected)
		{
			AIGHorrorHUD::PushThought(
				this,
				NSLOCTEXT("IGCH03", "NeedGlassesThought", "사다리 아래에서 검은 것이 빛난다."),
				3.4f);
			return;
		}
		RevealTank();
		break;

	case EIGChapterThreeAction::CloseTank:
		FinishEndingA();
		break;

	case EIGChapterThreeAction::EnterTank:
		BeginEndingB();
		break;
	}
}

void AIGThirdMorningDirector::TryUnlockApartmentExit()
{
	if (ApartmentDoorAction
		&& bFridgeInspected
		&& bPlannerRead
		&& bMotherPhoneRead)
	{
		ApartmentDoorAction->SetInteractionPrompt(
			NSLOCTEXT("IGCH03", "ApartmentDoorReadyPrompt", "현관문 열기"));
	}
}

void AIGThirdMorningDirector::HandleClueRead(
	AIGReadableNote* Note,
	const bool bOpened)
{
	if (!bOpened || !Note)
	{
		return;
	}

	if (Note == PlannerNote && !bPlannerRead)
	{
		bPlannerRead = true;
		AIGHorrorHUD::PushThought(
			this,
			NSLOCTEXT("IGCH03", "PlannerThought", "내 알람은… 5시 10분이었는데."),
			4.0f);
		IGStory::AddState(
			this,
			FGameplayTag::RequestGameplayTag(
				FName(TEXT("State.CH03.Flood.ReadPlanner")), false));
		TryUnlockApartmentExit();
	}
	else if (Note == MotherPhoneNote && !bMotherPhoneRead)
	{
		bMotherPhoneRead = true;
		IGStory::AddState(
			this,
			FGameplayTag::RequestGameplayTag(
				FName(TEXT("State.CH03.Flood.ReadMotherPhone")), false));
		AIGHorrorHUD::PushThought(
			this,
			NSLOCTEXT(
				"IGCH03",
				"MotherPhoneThought",
				"답장을… 언제부터 못 했지."),
			4.0f);
		TryUnlockApartmentExit();
	}
	else if (Note == OfferingNote)
	{
		IGStory::AddState(
			this,
			FGameplayTag::RequestGameplayTag(
				FName(TEXT("State.CH03.Flood.ReadOffering")), false));
		AIGHorrorHUD::PushThought(
			this,
			NSLOCTEXT("IGCH03", "OfferingThought", "물그릇이 비었어. 할머니가 두고 갔던 건데."),
			3.8f);
	}
	else if (Note == EstimateNote && !bEstimateRead)
	{
		bEstimateRead = true;
		IGStory::AddState(
			this,
			FGameplayTag::RequestGameplayTag(
				FName(TEXT("State.CH03.Flood.ReadEstimate")), false));
		AIGHorrorHUD::PushThought(
			this,
			NSLOCTEXT(
				"IGCH03",
				"EstimateThought",
				"서류엔 잠겼다고 썼다. 열쇠는 아직 여기 있는데."),
			5.0f);
	}
}

void AIGThirdMorningDirector::HandleCorridorEntered(AIGZoneTrigger* Zone)
{
	SetPhase(EIGThirdMorningPhase::FloodedCorridor);
	SetFloodMovement(true);
	StartWaterBed();
	AIGHorrorHUD::PushThought(
		this,
		NSLOCTEXT("IGCH03", "FloodedHallThought", "이게… 내가 본 건물 맞아?"),
		3.6f);

	LastPlayerMovingTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
	NextDelayedSplashTime = LastPlayerMovingTime + 7.0;
	GetWorldTimerManager().SetTimer(
		MotionPollTimer,
		this,
		&ThisClass::PollPlayerMotion,
		0.1f,
		true);
}

void AIGThirdMorningDirector::HandleStairLoop(AIGZoneTrigger* Zone)
{
	const int32 ZoneIndex = StairLoopZones.IndexOfByKey(Zone);
	if (ZoneIndex == INDEX_NONE || ZoneIndex != StairLoopCount)
	{
		return;
	}

	++StairLoopCount;
	SetPhase(EIGThirdMorningPhase::StairLoops);
	IGStory::AddState(
		this,
		FGameplayTag::RequestGameplayTag(
			FName(TEXT("State.CH03.Flood.StairsLooped")), false));

	if (APlayerController* PlayerController = GetWorld()->GetFirstPlayerController())
	{
		if (APawn* Pawn = PlayerController->GetPawn())
		{
			Pawn->SetActorLocation(
				ToWorld(FVector(1085, -5, IGThirdMorning::StandingCapsuleCenter)),
				false,
				nullptr,
				ETeleportType::TeleportPhysics);
			PlayerController->SetControlRotation(FRotator(-5, 90, 0));
		}
	}

	UpdateStairSign();
	PlayMetalEcho(FVector(985, 24, 145), 1.0f - StairLoopCount * 0.045f);
	AIGHorrorHUD::PushThought(
		this,
		StairLoopCount < 3
			? NSLOCTEXT("IGCH03", "LoopAgainThought", "분명 내려왔는데… 또 4층이야.")
			: NSLOCTEXT("IGCH03", "UpOnlyThought", "…위로 갈 수밖에 없다."),
		3.8f);

	if (StairLoopCount < 3 && StairLoopZones.IsValidIndex(StairLoopCount))
	{
		StairLoopZones[StairLoopCount]->SetActorEnableCollision(true);
	}
	else
	{
		RevealUpwardRoute();
	}
}

void AIGThirdMorningDirector::UpdateStairSign()
{
	if (StairSignText)
	{
		StairSignText->SetVisibility(true);
		StairSignText->SetText(FText::FromString(TEXT("4F")));
		StairSignText->SetTextRenderColor(
			StairLoopCount >= 2
				? FColor(152, 164, 154)
				: FColor(188, 194, 184));
	}
	if (StairRoofText)
	{
		StairRoofText->SetVisibility(StairLoopCount >= 3);
	}
	for (UStaticMeshComponent* Drip : StairLatinSignParts)
	{
		if (Drip)
		{
			Drip->SetVisibility(StairLoopCount == 2);
		}
	}
	for (UStaticMeshComponent* ArrowStroke : StairSignParts)
	{
		if (ArrowStroke)
		{
			ArrowStroke->SetVisibility(StairLoopCount >= 3);
		}
	}
}

void AIGThirdMorningDirector::RevealUpwardRoute()
{
	SetPhase(EIGThirdMorningPhase::UpwardOnly);
	if (UpRouteGate)
	{
		UpRouteGate->SetVisibility(false);
		UpRouteGate->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}
	if (DownRouteWaterBarrier)
	{
		DownRouteWaterBarrier->SetVisibility(true);
		DownRouteWaterBarrier->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	}
}

void AIGThirdMorningDirector::HandleFifthFloorEntered(AIGZoneTrigger* Zone)
{
	SetPhase(EIGThirdMorningPhase::FifthFloor);
	AIGHorrorHUD::PushThought(
		this,
		NSLOCTEXT("IGCH03", "OfferingArrivalThought", "물그릇이 비었어. 할머니가 두고 갔던 건데."),
		4.0f);
}

void AIGThirdMorningDirector::EnterRoofSilence()
{
	SetPhase(EIGThirdMorningPhase::Roof);
	SetFloodMovement(false);
	if (WaterBedComponent)
	{
		WaterBedComponent->FadeOut(1.1f, 0.0f);
		WaterBedComponent = nullptr;
	}
	GetWorldTimerManager().ClearTimer(MotionPollTimer);

	IGStory::AddState(
		this,
		FGameplayTag::RequestGameplayTag(
			FName(TEXT("State.CH03.Flood.ReachedRoof")), false));

	// Only the low-pass tail of the familiar delivery chain survives the door.
	PlayMetalEcho(FVector(1660, -400, 285), 0.78f);
	AIGHorrorHUD::PushThought(
		this,
		NSLOCTEXT(
			"IGCH03",
			"RoofSilenceThought",
			"그날도 문은 열려 있었다. 위에서 고양이가 긁는 줄 알았는데… 지금은 아무것도 없다."),
		6.0f);
}

void AIGThirdMorningDirector::HandleLadderEntered(AIGZoneTrigger* Zone)
{
	// My rung, then one wetter answer from below.
	PlayMetalEcho(FVector(2075, -300, 350), 1.0f);
	FTimerDelegate EchoDelegate;
	EchoDelegate.BindLambda([this]()
	{
		if (IsValid(this))
		{
			PlayMetalEcho(FVector(1970, -300, 280), 0.92f);
		}
	});
	GetWorldTimerManager().SetTimer(FlowTimer, EchoDelegate, 0.4f, false);
}

void AIGThirdMorningDirector::RevealTank()
{
	if (bTankOpened)
	{
		return;
	}
	bTankOpened = true;
	SetPhase(EIGThirdMorningPhase::TankReveal);
	if (TankLidAction)
	{
		// The open disc sits beyond the player's sightline.  A 3.1 m lid parked
		// on the near hinge used to cut through the body like a render artifact.
		TankLidAction->SetActorHiddenInGame(true);
		TankLidAction->SetInteractionEnabled(false);
		TankLidAction->SetActorEnableCollision(false);
	}
	for (UStaticMeshComponent* Piece : BodySilhouette)
	{
		if (Piece)
		{
			Piece->SetVisibility(true);
		}
	}
	StopAllChapterAudio();
	IGStory::AddState(
		this,
		FGameplayTag::RequestGameplayTag(
			FName(TEXT("State.CH03.Flood.OpenedTank")), false));

	AIGHorrorHUD::PushThought(
		this,
		NSLOCTEXT("IGCH03", "BodyRevealThought", "…403호에서 봤잖아. 이 등."),
		5.0f);
	SetVisibleInteractive(CloseChoiceAction, true);
	SetVisibleInteractive(EnterChoiceAction, true);
	SetPhase(EIGThirdMorningPhase::Choice);
}

void AIGThirdMorningDirector::SetVisibleInteractive(
	AIGChapterThreeAction* Action,
	const bool bVisible)
{
	if (!Action)
	{
		return;
	}
	Action->SetActorHiddenInGame(!bVisible);
	Action->SetActorEnableCollision(bVisible);
	Action->SetInteractionEnabled(bVisible);
}

void AIGThirdMorningDirector::FinishEndingA()
{
	if (bEndingFinished)
	{
		return;
	}
	bEndingFinished = true;
	SetPhase(EIGThirdMorningPhase::Ending);
	SetVisibleInteractive(CloseChoiceAction, false);
	SetVisibleInteractive(EnterChoiceAction, false);
	for (UStaticMeshComponent* Piece : BodySilhouette)
	{
		if (Piece)
		{
			Piece->SetVisibility(false);
		}
	}
	if (TankLidAction)
	{
		TankLidAction->SetActorHiddenInGame(false);
		TankLidAction->SetActorLocation(
			ToWorld(IGThirdMorning::TankCenter + FVector(0, 0, 610)));
		TankLidAction->SetActorRotation(FRotator::ZeroRotator);
	}
	PlayTankSlam();

	// In black, the loop starts assembling itself again: store threshold,
	// then the alarm. The image does not need to reset for the player to
	// understand what closing the lid chose.
	FTimerDelegate LoopChimeDelegate;
	LoopChimeDelegate.BindLambda([this]()
	{
		if (IsValid(this))
		{
			IGAudio::SpawnOneShotAt(
				this,
				UIGToneSequenceSoundWave::CreateDoorChime(this),
				ToWorld(IGThirdMorning::TankCenter),
				0.17f,
				0.90f,
				80.0f,
				900.0f);
		}
	});
	FTimerHandle LoopChimeHandle;
	GetWorldTimerManager().SetTimer(
		LoopChimeHandle, LoopChimeDelegate, 0.75f, false);

	FTimerDelegate LoopAlarmDelegate;
	LoopAlarmDelegate.BindLambda([this]()
	{
		if (IsValid(this))
		{
			UIGAlarmSoundWave* LoopAlarm =
				NewObject<UIGAlarmSoundWave>(this, TEXT("CH03EndingLoopAlarm"));
			IGAudio::SpawnOneShotAt(
				this,
				LoopAlarm,
				ToWorld(FVector(-205, -190, 82)),
				0.30f,
				0.983f,
				80.0f,
				750.0f);
		}
	});
	FTimerHandle LoopAlarmHandle;
	GetWorldTimerManager().SetTimer(
		LoopAlarmHandle, LoopAlarmDelegate, 1.30f, false);

	if (APlayerController* PlayerController = GetWorld()->GetFirstPlayerController())
	{
		if (APlayerCameraManager* Camera = PlayerController->PlayerCameraManager)
		{
			Camera->StartCameraFade(
				0.0f, 1.0f, 2.2f, FLinearColor::Black, false, true);
		}
	}
	FTimerDelegate EndingDelegate;
	EndingDelegate.BindLambda([this]()
	{
		if (IsValid(this))
		{
			PresentEndingControls(
				NSLOCTEXT("IGCH03", "EndingATitle", "내일 또"),
				NSLOCTEXT(
					"IGCH03",
					"EndingASubtitle",
					"2024년 7월 26일 금요일, 오전 4시 44분."));
		}
	});
	GetWorldTimerManager().SetTimer(FlowTimer, EndingDelegate, 3.4f, false);
}

void AIGThirdMorningDirector::BeginEndingB()
{
	if (bEndingFinished)
	{
		return;
	}
	bEndingFinished = true;
	SetPhase(EIGThirdMorningPhase::Ending);
	SetVisibleInteractive(CloseChoiceAction, false);
	SetVisibleInteractive(EnterChoiceAction, false);
	if (GlassesAction)
	{
		GlassesAction->SetActorLocation(
			ToWorld(IGThirdMorning::TankCenter + FVector(-18, 28, 548)));
		GlassesAction->SetActorRotation(FRotator(0, 12, -3));
		GlassesAction->SetActorEnableCollision(false);
		GlassesAction->SetInteractionEnabled(false);
	}
	IGAudio::SpawnOneShotAt(
		this,
		UIGToneSequenceSoundWave::CreateWaterDripMetalRing(this),
		ToWorld(IGThirdMorning::TankCenter + FVector(0, 0, 545)),
		0.52f,
		0.62f,
		80.0f,
		650.0f);

	if (APlayerController* PlayerController = GetWorld()->GetFirstPlayerController())
	{
		if (APlayerCameraManager* Camera = PlayerController->PlayerCameraManager)
		{
			Camera->StartCameraFade(
				0.0f,
				1.0f,
				4.8f,
				FLinearColor(0.015f, 0.12f, 0.14f),
				false,
				true);
		}
	}
	GetWorldTimerManager().SetTimer(
		FlowTimer,
		this,
		&ThisClass::FinishEndingB,
		5.2f,
		false);
}

void AIGThirdMorningDirector::FinishEndingB()
{
	AIGHorrorHUD::ShowChapterCard(
		this,
		NSLOCTEXT("IGCH03", "DiscoveryDate", "2024년 8월 16일"),
		NSLOCTEXT("IGCH03", "DiscoveryTitle", "저수조 점검 중 실종자 한지운 발견"),
		NSLOCTEXT("IGCH03", "DiscoverySubtitle", "가족에게 돌아갔다."),
		6.0f);

	FTimerDelegate FinalCardDelegate;
	FinalCardDelegate.BindLambda([this]()
	{
		if (IsValid(this))
		{
			PresentEndingControls(
				NSLOCTEXT("IGCH03", "EndingBTitle", "돌려보내다"),
				NSLOCTEXT(
					"IGCH03",
					"EndingBSubtitle",
					"다음 봄, 404호 새 세입자는 물이 조금 달다고 했다."));
		}
	});
	GetWorldTimerManager().SetTimer(FlowTimer, FinalCardDelegate, 6.1f, false);
}

void AIGThirdMorningDirector::PresentEndingControls(
	const FText& EndingTitle,
	const FText& EndingSubtitle)
{
	AIGHorrorHUD::ShowChapterCard(
		this,
		NSLOCTEXT("IGCH03", "EndingEyebrow", "ENDING"),
		EndingTitle,
		FText::Format(
			NSLOCTEXT(
				"IGCH03",
				"EndingControls",
				"{0}\nR  이 아침 다시 시작  ·  M  처음으로"),
			EndingSubtitle),
		120.0f);
	EnableEndingInput();
}

void AIGThirdMorningDirector::EnableEndingInput()
{
	APlayerController* PlayerController = GetWorld()
		? GetWorld()->GetFirstPlayerController()
		: nullptr;
	if (!PlayerController)
	{
		return;
	}
	EnableInput(PlayerController);
	if (InputComponent)
	{
		InputComponent->BindAction(
			TEXT("RestartChapter"),
			IE_Pressed,
			this,
			&ThisClass::RestartChapter);
		InputComponent->BindAction(
			TEXT("ReturnToMenu"),
			IE_Pressed,
			this,
			&ThisClass::ReturnToBeginning);
	}
}

void AIGThirdMorningDirector::RestartChapter()
{
	if (!GetWorld())
	{
		return;
	}
	const FName LevelName(*UGameplayStatics::GetCurrentLevelName(this, true));
	UGameplayStatics::OpenLevel(this, LevelName, true, TEXT("IGChapterThree=1"));
}

void AIGThirdMorningDirector::ReturnToBeginning()
{
	if (!GetWorld())
	{
		return;
	}
	if (UGameInstance* GameInstance = GetGameInstance())
	{
		if (UIGStoryStateSubsystem* StoryState =
			GameInstance->GetSubsystem<UIGStoryStateSubsystem>())
		{
			// M means a true return to the beginning, not a CH01 load carrying
			// the flood chapter's autosaved tags.
			StoryState->ClearStates(false);
		}
	}
	const FName LevelName(*UGameplayStatics::GetCurrentLevelName(this, true));
	UGameplayStatics::OpenLevel(this, LevelName, true, TEXT("IGIgnoreDirectStart=1"));
}

void AIGThirdMorningDirector::SetFloodMovement(const bool bFlooded)
{
	APlayerController* PlayerController = GetWorld()
		? GetWorld()->GetFirstPlayerController()
		: nullptr;
	AIGPlayerCharacter* Player = PlayerController
		? Cast<AIGPlayerCharacter>(PlayerController->GetPawn())
		: nullptr;
	if (UCharacterMovementComponent* Movement = Player ? Player->GetCharacterMovement() : nullptr)
	{
		Movement->MaxWalkSpeed = bFlooded
			? IGThirdMorning::FloodWalkSpeed
			: IGThirdMorning::DefaultWalkSpeed;
	}
}

// ---------------------------------------------------------------------------
// Sound field
// ---------------------------------------------------------------------------

void AIGThirdMorningDirector::StartWaterBed()
{
	if (WaterBedComponent)
	{
		return;
	}

	UIGToneSequenceSoundWave* WaterBed =
		NewObject<UIGToneSequenceSoundWave>(this, TEXT("CH03WaterBed"));
	TArray<FIGToneNote> Notes;
	Notes.Add({0.0f, 9.0f, 96.0f, 0.025f, 0.20f, 0.8f, EIGToneWaveform::ValueNoise});
	Notes.Add({0.0f, 9.0f, 52.0f, 0.018f, 0.25f, 0.8f, EIGToneWaveform::Sine});
	Notes.Add({1.2f, 0.24f, 820.0f, 0.065f, 0.02f, 2.8f, EIGToneWaveform::ValueNoise});
	Notes.Add({4.7f, 0.36f, 1280.0f, 0.050f, 0.02f, 3.2f, EIGToneWaveform::Sine});
	Notes.Add({7.4f, 0.20f, 610.0f, 0.052f, 0.02f, 2.6f, EIGToneWaveform::ValueNoise});
	WaterBed->ConfigureNotes(MoveTemp(Notes), true, 9.0f);
	WaterBedComponent = IGAudio::SpawnOneShotAt(
		this,
		WaterBed,
		ToWorld(FVector(690, 0, 5)),
		0.42f,
		1.0f,
		260.0f,
		1450.0f);
	if (WaterBedComponent)
	{
		WaterBedComponent->SetLowPassFilterEnabled(true);
		WaterBedComponent->SetLowPassFilterFrequency(2100.0f);
	}
}

void AIGThirdMorningDirector::PollPlayerMotion()
{
	if (!GetWorld() || Phase < EIGThirdMorningPhase::FloodedCorridor
		|| Phase >= EIGThirdMorningPhase::Roof)
	{
		return;
	}
	APlayerController* PlayerController = GetWorld()->GetFirstPlayerController();
	APawn* Pawn = PlayerController ? PlayerController->GetPawn() : nullptr;
	if (!Pawn)
	{
		return;
	}

	const double Now = GetWorld()->GetTimeSeconds();
	if (Pawn->GetVelocity().Size2D() > 12.0f)
	{
		LastPlayerMovingTime = Now;
		return;
	}
	if (Now >= NextDelayedSplashTime && Now - LastPlayerMovingTime >= 0.4)
	{
		PlayDelayedSplash();
		NextDelayedSplashTime = Now + 40.0;
	}
}

void AIGThirdMorningDirector::PlayDelayedSplash()
{
	APlayerController* PlayerController = GetWorld()
		? GetWorld()->GetFirstPlayerController()
		: nullptr;
	APawn* Pawn = PlayerController ? PlayerController->GetPawn() : nullptr;
	if (!Pawn)
	{
		return;
	}

	UIGToneSequenceSoundWave* Splash =
		NewObject<UIGToneSequenceSoundWave>(this, TEXT("CH03DelayedSplash"));
	TArray<FIGToneNote> Notes;
	Notes.Add({0.00f, 0.14f, 420.0f, 0.13f, 0.03f, 2.2f, EIGToneWaveform::ValueNoise});
	Notes.Add({0.00f, 0.21f, 72.0f, 0.18f, 0.02f, 3.4f, EIGToneWaveform::Sine});
	Notes.Add({0.06f, 0.26f, 980.0f, 0.045f, 0.02f, 3.1f, EIGToneWaveform::ValueNoise});
	Splash->ConfigureNotes(MoveTemp(Notes), false);
	IGAudio::SpawnOneShotAt(
		this,
		Splash,
		Pawn->GetActorLocation() - Pawn->GetActorForwardVector() * 115.0f,
		0.46f,
		0.92f,
		45.0f,
		720.0f);
}

void AIGThirdMorningDirector::PlayMetalEcho(
	const FVector& LocalLocation,
	const float PitchMultiplier)
{
	UIGToneSequenceSoundWave* Ring =
		NewObject<UIGToneSequenceSoundWave>(
			this,
			MakeUniqueObjectName(
				this,
				UIGToneSequenceSoundWave::StaticClass(),
				TEXT("CH03MetalEcho")));
	TArray<FIGToneNote> Notes;
	Notes.Add({0.00f, 0.035f, 1550.0f, 0.13f, 0.02f, 1.4f, EIGToneWaveform::ValueNoise});
	Notes.Add({0.00f, 0.82f, 181.0f, 0.13f, 0.01f, 4.0f, EIGToneWaveform::Sine});
	Notes.Add({0.01f, 0.63f, 1180.0f, 0.060f, 0.01f, 4.4f, EIGToneWaveform::Sine});
	Ring->ConfigureNotes(MoveTemp(Notes), false);
	IGAudio::SpawnOneShotAt(
		this,
		Ring,
		ToWorld(LocalLocation),
		0.52f,
		PitchMultiplier,
		65.0f,
		1050.0f);
}

void AIGThirdMorningDirector::PlayTankSlam()
{
	UIGToneSequenceSoundWave* Slam =
		NewObject<UIGToneSequenceSoundWave>(this, TEXT("CH03TankSlam"));
	TArray<FIGToneNote> Notes;
	Notes.Add({0.0f, 0.075f, 2600.0f, 0.30f, 0.01f, 1.0f, EIGToneWaveform::ValueNoise});
	Notes.Add({0.0f, 0.38f, 46.0f, 0.58f, 0.01f, 3.8f, EIGToneWaveform::Sine});
	Notes.Add({0.015f, 0.62f, 164.0f, 0.32f, 0.01f, 4.2f, EIGToneWaveform::Triangle});
	Notes.Add({0.02f, 0.44f, 970.0f, 0.12f, 0.01f, 4.5f, EIGToneWaveform::Sine});
	Slam->ConfigureNotes(MoveTemp(Notes), false);
	IGAudio::SpawnOneShotAt(
		this,
		Slam,
		ToWorld(IGThirdMorning::TankCenter + FVector(0, 0, 620)),
		0.86f,
		1.0f,
		140.0f,
		1700.0f);
}

void AIGThirdMorningDirector::StopAllChapterAudio()
{
	if (AlarmComponent)
	{
		AlarmComponent->Stop();
		AlarmComponent = nullptr;
	}
	if (WaterBedComponent)
	{
		WaterBedComponent->Stop();
		WaterBedComponent = nullptr;
	}
}

// ---------------------------------------------------------------------------
// Objective provider
// ---------------------------------------------------------------------------

FText AIGThirdMorningDirector::GetObjectiveText() const
{
	switch (Phase)
	{
	case EIGThirdMorningPhase::Awakening:
		return NSLOCTEXT("IGCH03", "ObjectiveWake", "알람을 듣자");
	case EIGThirdMorningPhase::ApartmentClues:
		if (!bFridgeInspected || !bPlannerRead)
		{
			return NSLOCTEXT(
				"IGCH03", "ObjectiveApartment", "방 안의 물과 시간을 확인하자");
		}
		return !bMotherPhoneRead
			? NSLOCTEXT("IGCH03", "ObjectivePhone", "금 간 휴대폰을 확인하자")
			: NSLOCTEXT("IGCH03", "ObjectiveLeave", "물이 어디서 새는지 찾자");
	case EIGThirdMorningPhase::FloodedCorridor:
		return NSLOCTEXT("IGCH03", "ObjectiveDown", "아래로 내려가자");
	case EIGThirdMorningPhase::StairLoops:
		return NSLOCTEXT("IGCH03", "ObjectiveLoop", "아래층을 다시 확인하자");
	case EIGThirdMorningPhase::UpwardOnly:
		return NSLOCTEXT("IGCH03", "ObjectiveUp", "…위로 갈 수밖에 없다");
	case EIGThirdMorningPhase::FifthFloor:
		return NSLOCTEXT("IGCH03", "ObjectiveRoof", "옥상으로");
	case EIGThirdMorningPhase::Roof:
		if (!bEstimateRead)
		{
			return NSLOCTEXT(
				"IGCH03", "ObjectiveWorkSheet", "철문 안쪽 작업표를 확인하자");
		}
		return bGlassesInspected
			? NSLOCTEXT("IGCH03", "ObjectiveTank", "물탱크를 확인하자")
			: NSLOCTEXT("IGCH03", "ObjectiveGlasses", "사다리 아래를 확인하자");
	case EIGThirdMorningPhase::TankReveal:
	case EIGThirdMorningPhase::Choice:
		return NSLOCTEXT("IGCH03", "ObjectiveChoice", "직접 끝을 정하자");
	case EIGThirdMorningPhase::Ending:
	default:
		return FText::GetEmpty();
	}
}

FString AIGThirdMorningDirector::GetObjectiveTextAscii() const
{
	switch (Phase)
	{
	case EIGThirdMorningPhase::Awakening: return TEXT("LISTEN");
	case EIGThirdMorningPhase::ApartmentClues: return TEXT("CHECK THE WATER AND TIME");
	case EIGThirdMorningPhase::FloodedCorridor: return TEXT("GO DOWNSTAIRS");
	case EIGThirdMorningPhase::StairLoops: return TEXT("TRY THE STAIRS AGAIN");
	case EIGThirdMorningPhase::UpwardOnly: return TEXT("ONLY UP");
	case EIGThirdMorningPhase::FifthFloor: return TEXT("REACH THE ROOF");
	case EIGThirdMorningPhase::Roof: return TEXT("CHECK THE WATER TANK");
	case EIGThirdMorningPhase::TankReveal:
	case EIGThirdMorningPhase::Choice: return TEXT("CHOOSE IN THE WORLD");
	case EIGThirdMorningPhase::Ending:
	default: return FString();
	}
}

float AIGThirdMorningDirector::GetObjectiveProgress() const
{
	return FMath::Clamp(
		static_cast<float>(static_cast<uint8>(Phase))
			/ static_cast<float>(static_cast<uint8>(EIGThirdMorningPhase::Ending)),
		0.0f,
		1.0f);
}

// ---------------------------------------------------------------------------
// Deterministic CH03 capture/smoke route
// ---------------------------------------------------------------------------

void AIGThirdMorningDirector::StartCaptureSequence()
{
	FAssetCompilingManager::Get().FinishAllCompilation();
	if (GShaderCompilingManager)
	{
		GShaderCompilingManager->FinishAllCompilation();
	}
	if (GEngine)
	{
		GEngine->bEnableOnScreenDebugMessages = false;
	}
	if (APlayerController* PlayerController = GetWorld()->GetFirstPlayerController())
	{
		PlayerController->ConsoleCommand(TEXT("r.MotionBlurQuality 0"), true);
		PlayerController->ConsoleCommand(TEXT("DisableAllScreenMessages"), true);
		if (AHUD* HUD = PlayerController->GetHUD())
		{
			HUD->bShowHUD = false;
		}
		if (APlayerCameraManager* Camera = PlayerController->PlayerCameraManager)
		{
			Camera->StopCameraFade();
		}
		if (AIGPlayerCharacter* Player =
			Cast<AIGPlayerCharacter>(PlayerController->GetPawn()))
		{
			if (UIGFlashlightComponent* Flashlight = Player->GetFlashlight())
			{
				Flashlight->SetOn(true);
			}
		}
	}

	// Exercise the real opening state transition instead of setting its bool.
	StopAlarmByItself();
	CaptureIndex = 0;
	CaptureNextFrame();
}

void AIGThirdMorningDirector::PlaceCaptureCamera(
	const FVector& LocalPawnLocation,
	const FVector& LocalLookAt)
{
	APlayerController* PlayerController = GetWorld()
		? GetWorld()->GetFirstPlayerController()
		: nullptr;
	APawn* Pawn = PlayerController ? PlayerController->GetPawn() : nullptr;
	if (!Pawn || !PlayerController)
	{
		return;
	}
	Pawn->SetActorLocation(
		ToWorld(LocalPawnLocation),
		false,
		nullptr,
		ETeleportType::TeleportPhysics);
	FVector Eye = Pawn->GetActorLocation() + FVector(0, 0, 64);
	if (const AIGPlayerCharacter* Player = Cast<AIGPlayerCharacter>(Pawn))
	{
		if (const UCameraComponent* Camera = Player->GetFirstPersonCamera())
		{
			Eye = Camera->GetComponentLocation();
		}
	}
	const FRotator LookRotation = (ToWorld(LocalLookAt) - Eye).Rotation();
	Pawn->SetActorRotation(FRotator(0, LookRotation.Yaw, 0));
	PlayerController->SetControlRotation(LookRotation);
}

void AIGThirdMorningDirector::CaptureNextFrame()
{
	if (!GetWorld())
	{
		FinishCaptureSequence();
		return;
	}
	if (AIGReadableNote* OpenNote = AIGReadableNote::GetOpenNote())
	{
		OpenNote->Close();
	}

	FString BaseName;
	switch (CaptureIndex)
	{
	case 0:
		HandleAction(EIGChapterThreeAction::OpenFridge, FridgeDoorAction);
		HandleClueRead(PlannerNote, true);
		HandleClueRead(MotherPhoneNote, true);
		HandleAction(EIGChapterThreeAction::OpenApartmentDoor, ApartmentDoorAction);
		PlaceCaptureCamera(
			FVector(10, -35, IGThirdMorning::StandingCapsuleCenter + 18.0f),
			FVector(-208, 115, 132));
		BaseName = TEXT("ch03-full-fridge");
		break;
	case 1:
		HandleCorridorEntered(CorridorEntryZone);
		for (AIGZoneTrigger* LoopZone : StairLoopZones)
		{
			HandleStairLoop(LoopZone);
		}
		// The waist-high water sheet proves the lower route is blocked in
		// play, but it would cover the changing 4F/ROOF sign in this dedicated
		// documentation frame.
		if (DownRouteWaterBarrier)
		{
			DownRouteWaterBarrier->SetVisibility(false);
		}
		PlaceCaptureCamera(
			FVector(
				1180,
				135,
				IGThirdMorning::StandingCapsuleCenter + 20.0f),
			FVector(1040, -65, 145));
		BaseName = TEXT("ch03-stair-up");
		break;
	case 2:
		HandleAction(EIGChapterThreeAction::InspectKeys, KeysAction);
		HandleAction(EIGChapterThreeAction::OpenRoofDoor, RoofDoorAction);
		HandleClueRead(EstimateNote, true);
		PlaceCaptureCamera(
			FVector(
				3020,
				100,
				IGThirdMorning::RoofFloorZ + IGThirdMorning::StandingCapsuleCenter + 155.0f),
			IGThirdMorning::TankCenter + FVector(0, 0, 470));
		BaseName = TEXT("ch03-roof-tank");
		break;
	case 3:
		HandleAction(EIGChapterThreeAction::InspectGlasses, GlassesAction);
		HandleAction(EIGChapterThreeAction::OpenTank, TankLidAction);
		PlaceCaptureCamera(
			FVector(2420, -220, 1020),
			IGThirdMorning::TankCenter + FVector(-8, 2, 550));
		BaseName = TEXT("ch03-tank-reveal");
		break;
	default:
		FinishCaptureSequence();
		return;
	}

	const FString ScreenshotPath = FPaths::ConvertRelativePathToFull(
		FPaths::Combine(
			FPaths::ProjectDir(),
			FString::Printf(TEXT("Docs/Media/%s.png"), *BaseName)));
	++CaptureIndex;

	FTimerDelegate CaptureDelegate;
	CaptureDelegate.BindLambda([this, ScreenshotPath]()
	{
		if (!IsValid(this))
		{
			return;
		}
		FScreenshotRequest::RequestScreenshot(ScreenshotPath, true, false);
		UE_LOG(LogIndieGame, Display, TEXT("CH03 capture requested: %s"), *ScreenshotPath);
		GetWorldTimerManager().SetTimer(
			CaptureTimer,
			this,
			&ThisClass::CaptureNextFrame,
			4.0f,
			false);
	});
	GetWorldTimerManager().SetTimer(CaptureTimer, CaptureDelegate, 0.75f, false);
}

void AIGThirdMorningDirector::FinishCaptureSequence()
{
	const bool bChoiceTargetsReady =
		bTankOpened
		&& CloseChoiceAction
		&& EnterChoiceAction
		&& Phase == EIGThirdMorningPhase::Choice;
	if (!bChoiceTargetsReady)
	{
		UE_LOG(
			LogIndieGame,
			Error,
			TEXT("CH03 capture smoke route did not reach both physical choices."));
	}
	UE_LOG(
		LogIndieGame,
		Display,
		TEXT(
			"CH03_CAPTURE COMPLETE loops=%d roof=%d tank=%d "
			"choice_targets=%d visual_stills=4"),
		StairLoopCount,
		Phase >= EIGThirdMorningPhase::Roof ? 1 : 0,
		bTankOpened ? 1 : 0,
		bChoiceTargetsReady ? 2 : 0);
	FPlatformMisc::RequestExit(false);
}
