#include "Core/IGPrologueWorldScene.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetCompilingManager.h"
#include "Audio/IGAlarmSoundWave.h"
#include "Audio/IGAmbienceSoundWave.h"
#include "Audio/IGToneSequenceSoundWave.h"
#include "Camera/CameraComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/AudioComponent.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/SkyAtmosphereComponent.h"
#include "Components/SkyLightComponent.h"
#include "Components/PostProcessComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/CollisionProfile.h"
#include "Engine/Engine.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "HAL/PlatformMisc.h"
#include "IndieGame.h"
#include "HighResScreenshot.h"
#include "Interaction/IGCheckoutCounter.h"
#include "Interaction/IGElevator.h"
#include "Player/IGFlashlightComponent.h"
#include "Player/IGPlayerCharacter.h"
#include "Interaction/IGFridge.h"
#include "Interaction/IGInspectable.h"
#include "Interaction/IGInteractable.h"
#include "Interaction/IGInteractableActor.h"
#include "Interaction/IGPickupItem.h"
#include "Interaction/IGReadableNote.h"
#include "Interaction/IGSlidingDoor.h"
#include "Interaction/IGSwingDoor.h"
#include "Interaction/IGZoneTrigger.h"
#include "Materials/MaterialInterface.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"
#include "Narrative/IGStoryStateSubsystem.h"
#include "Player/IGHorrorHUD.h"
#include "Sequence/IGDemoDirector.h"
#include "Sequence/IGMorningRoutineDirector.h"
#include "Sequence/IGSecondMorningDirector.h"
#include "Player/IGStressComponent.h"
#include "ShaderCompiler.h"
#include "TimerManager.h"
#include "UObject/ConstructorHelpers.h"

namespace IGPrologueWorld
{
	constexpr int32 MaxPlayerPositionAttempts = 30;

	// The player wakes beside the bed facing the nightstand, so the ringing
	// alarm is the first thing in view and the first prompt is reachable.
	// Keep the 72 cm player capsule clear of the mattress edge. The old X=-85
	// start penetrated the bed by roughly 30 cm and produced a violent
	// depenetration/camera shove on both mornings.
	const FVector PlayerLocation(-48.0f, 60.0f, 997.0f);
	const FRotator PlayerActorRotation(0.0f, -128.0f, 0.0f);
	const FRotator PlayerViewRotation(-14.0f, -128.0f, 0.0f);

	// Unit 404 sits on the 4th floor, three slabs above the street.
	constexpr float FourthFloorZ = 900.0f;

	const FVector AlarmLocation(-160.0f, -35.0f, FourthFloorZ + 63.0f);
	const FVector GetUpTargetLocation(-140.0f, 183.0f, FourthFloorZ + 58.0f);
	const FVector FridgeLocation(155.0f, -20.0f, FourthFloorZ);
	const FVector HomeDoorLocation(101.0f, -225.0f, FourthFloorZ);
	// Far end of the hallway, so leaving 404 is a walk rather than a step.
	const FVector ElevatorLocation(790.0f, -305.0f, FourthFloorZ);
	const FVector StoreDoorLocation(2405.0f, -457.0f, 6.0f);
	const FVector CheckoutLocation(2620.0f, -255.0f, 96.0f);
	const FVector WalletLocation(-150.0f, -185.0f, FourthFloorZ + 78.0f);
}

// ---------------------------------------------------------------------------
// Prototype wake-flow adapters
// ---------------------------------------------------------------------------

UStaticMeshComponent* AIGPrologueAlarmClock::AddPart(
	UStaticMesh* Mesh,
	UMaterialInterface* Material,
	const FVector& RelativeLocation,
	const FVector& SizeCentimeters)
{
	UStaticMeshComponent* Part = NewObject<UStaticMeshComponent>(
		this,
		*FString::Printf(TEXT("ClockPart_%d"), PartCounter++));
	Part->SetupAttachment(SceneRoot);
	Part->SetStaticMesh(Mesh);
	Part->SetMaterial(0, Material);
	Part->SetRelativeLocation(RelativeLocation);
	Part->SetRelativeScale3D(SizeCentimeters / 100.0f);
	Part->SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);
	Part->SetGenerateOverlapEvents(false);
	Part->SetCanEverAffectNavigation(false);
	Part->RegisterComponent();
	return Part;
}

void AIGPrologueAlarmClock::ConfigurePrototype(
	UStaticMesh* InBodyMesh,
	UStaticMesh* InPanelMesh,
	UMaterialInterface* InBodyMaterial,
	UMaterialInterface* InDisplayMaterial,
	UMaterialInterface* InButtonMaterial,
	USoundBase* InAlarmSound)
{
	if (ClockMesh)
	{
		// Authored shell: beveled body, recessed face, buttons and feet all
		// modelled. Also the trace/interaction target.
		ClockMesh->SetStaticMesh(InBodyMesh);
		ClockMesh->SetMaterial(0, InBodyMaterial);
		ClockMesh->SetRelativeScale3D(FVector::OneVector);
		ClockMesh->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
		ClockMesh->SetGenerateOverlapEvents(false);
		ClockMesh->SetCanEverAffectNavigation(false);
	}

	// The lit LED face sits inside the modelled recess, aimed at the bed (+Y).
	AddPart(InPanelMesh, InDisplayMaterial, FVector(0.0f, 4.05f, 3.4f), FVector(9.4f, 0.3f, 4.0f));

	if (UAudioComponent* AudioComponent = GetAlarmAudioComponent())
	{
		AudioComponent->SetSound(InAlarmSound);
		AudioComponent->SetVolumeMultiplier(0.32f);
	}
}

void AIGPrologueWakeDirector::ConfigurePrototype(AIGAlarmClock* InAlarmClock)
{
	AlarmClock = InAlarmClock;
	// The scene binds presentation callbacks before starting the state machine.
	bAutoStart = false;
	FadeInDuration = 2.2f;
	GettingUpFallbackDuration = 2.7f; // sit up, breathe, then stand
}

AIGPrologueGetUpTarget::AIGPrologueGetUpTarget()
{
	TargetMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PrologueGetUpTarget"));
	SetRootComponent(TargetMesh);
	TargetMesh->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
	TargetMesh->SetGenerateOverlapEvents(false);
	TargetMesh->SetCanEverAffectNavigation(false);
}

void AIGPrologueGetUpTarget::ConfigurePrototype(
	AIGWakeUpDirector* InWakeUpDirector,
	UStaticMesh* InTargetMesh,
	UMaterialInterface* InTargetMaterial)
{
	WakeUpDirector = InWakeUpDirector;
	TargetMesh->SetStaticMesh(InTargetMesh);
	TargetMesh->SetMaterial(0, InTargetMaterial);
	TargetMesh->SetRelativeScale3D(FVector(0.46f, 0.30f, 0.11f));
}

// ---------------------------------------------------------------------------
// Scene
// ---------------------------------------------------------------------------

AIGPrologueWorldScene::AIGPrologueWorldScene()
{
	PrimaryActorTick.bCanEverTick = false;
	PrimaryActorTick.bStartWithTickEnabled = false;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SceneRoot->SetMobility(EComponentMobility::Static);
	SetRootComponent(SceneRoot);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMeshFinder(
		TEXT("/Engine/BasicShapes/Cube.Cube"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderMeshFinder(
		TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereMeshFinder(
		TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> ConeMeshFinder(
		TEXT("/Engine/BasicShapes/Cone.Cone"));
	CubeMesh = CubeMeshFinder.Object;
	CylinderMesh = CylinderMeshFinder.Object;
	SphereMesh = SphereMeshFinder.Object;
	ConeMesh = ConeMeshFinder.Object;

	auto FindMaterial = [](const TCHAR* Path) -> UMaterialInterface*
	{
		ConstructorHelpers::FObjectFinder<UMaterialInterface> Finder(Path);
		return Finder.Object;
	};

	WallMaterial = FindMaterial(TEXT("/Game/Prototype/Materials/M_RoomWall.M_RoomWall"));
	FloorMaterial = FindMaterial(TEXT("/Game/Prototype/Materials/M_RoomFloor.M_RoomFloor"));
	WoodMaterial = FindMaterial(TEXT("/Game/Prototype/Materials/M_DarkWood.M_DarkWood"));
	BeddingMaterial = FindMaterial(TEXT("/Game/Prototype/Materials/M_Bedding.M_Bedding"));
	DoorMaterial = FindMaterial(TEXT("/Game/Prototype/Materials/M_Door.M_Door"));
	AlarmMaterial = FindMaterial(TEXT("/Game/Prototype/Materials/M_Alarm.M_Alarm"));
	AsphaltMaterial = FindMaterial(TEXT("/Game/Prototype/Materials/M_Asphalt.M_Asphalt"));
	ConcreteMaterial = FindMaterial(TEXT("/Game/Prototype/Materials/M_Concrete.M_Concrete"));
	ConcreteDarkMaterial = FindMaterial(TEXT("/Game/Prototype/Materials/M_ConcreteDark.M_ConcreteDark"));
	StoreFloorMaterial = FindMaterial(TEXT("/Game/Prototype/Materials/M_StoreFloor.M_StoreFloor"));
	LightPanelMaterial = FindMaterial(TEXT("/Game/Prototype/Materials/M_LightPanel.M_LightPanel"));
	SignMintMaterial = FindMaterial(TEXT("/Game/Prototype/Materials/M_SignMint.M_SignMint"));
	SignWhiteMaterial = FindMaterial(TEXT("/Game/Prototype/Materials/M_SignWhite.M_SignWhite"));
	GlassMaterial = FindMaterial(TEXT("/Game/Prototype/Materials/M_Glass.M_Glass"));
	MetalFrameMaterial = FindMaterial(TEXT("/Game/Prototype/Materials/M_MetalFrame.M_MetalFrame"));
	FridgeBodyMaterial = FindMaterial(TEXT("/Game/Prototype/Materials/M_FridgeBody.M_FridgeBody"));
	FridgeInteriorMaterial = FindMaterial(TEXT("/Game/Prototype/Materials/M_FridgeInterior.M_FridgeInterior"));
	PlasticDarkMaterial = FindMaterial(TEXT("/Game/Prototype/Materials/M_PlasticDark.M_PlasticDark"));
	TrashBagMaterial = FindMaterial(TEXT("/Game/Prototype/Materials/M_TrashBag.M_TrashBag"));
	CardboardMaterial = FindMaterial(TEXT("/Game/Prototype/Materials/M_Cardboard.M_Cardboard"));
	WaterBlueMaterial = FindMaterial(TEXT("/Game/Prototype/Materials/M_WaterBlue.M_WaterBlue"));
	BottleGreenMaterial = FindMaterial(TEXT("/Game/Prototype/Materials/M_BottleGreen.M_BottleGreen"));
	BottleBrownMaterial = FindMaterial(TEXT("/Game/Prototype/Materials/M_BottleBrown.M_BottleBrown"));
	SnackRedMaterial = FindMaterial(TEXT("/Game/Prototype/Materials/M_SnackRed.M_SnackRed"));
	SnackYellowMaterial = FindMaterial(TEXT("/Game/Prototype/Materials/M_SnackYellow.M_SnackYellow"));
	SnackBlueMaterial = FindMaterial(TEXT("/Game/Prototype/Materials/M_SnackBlue.M_SnackBlue"));
	CupNoodleMaterial = FindMaterial(TEXT("/Game/Prototype/Materials/M_CupNoodle.M_CupNoodle"));
	WindowGlowMaterial = FindMaterial(TEXT("/Game/Prototype/Materials/M_WindowGlow.M_WindowGlow"));
	WindowDarkMaterial = FindMaterial(TEXT("/Game/Prototype/Materials/M_WindowDark.M_WindowDark"));
	NightSkyMaterial = FindMaterial(TEXT("/Game/Prototype/Materials/M_NightSky.M_NightSky"));
	StreetLampGlowMaterial = FindMaterial(TEXT("/Game/Prototype/Materials/M_StreetLampGlow.M_StreetLampGlow"));
	WalletBrownMaterial = FindMaterial(TEXT("/Game/Prototype/Materials/M_WalletBrown.M_WalletBrown"));
	CoolerBodyMaterial = FindMaterial(TEXT("/Game/Prototype/Materials/M_CoolerBody.M_CoolerBody"));
	CounterTopMaterial = FindMaterial(TEXT("/Game/Prototype/Materials/M_CounterTop.M_CounterTop"));
	ScreenGlowMaterial = FindMaterial(TEXT("/Game/Prototype/Materials/M_ScreenGlow.M_ScreenGlow"));

	PostProcess = CreateDefaultSubobject<UPostProcessComponent>(TEXT("PostProcess"));
	PostProcess->SetupAttachment(SceneRoot);
	PostProcess->bUnbound = true;
	// Adaptive exposure like an eye/camera: dark alleys stay dark but
	// readable, the fluorescent store genuinely blooms after the walk.
	PostProcess->Settings.bOverride_AutoExposureMethod = true;
	PostProcess->Settings.AutoExposureMethod = AEM_Histogram;
	PostProcess->Settings.bOverride_AutoExposureMinBrightness = true;
	PostProcess->Settings.bOverride_AutoExposureMaxBrightness = true;
	// Keep the eye adaptation useful without allowing a point light or white
	// ceiling to drive a five-stop swing.  The former -2.6..2.8 range blew
	// bedrooms and the shop to white, then crushed the next dark frame.
	PostProcess->Settings.AutoExposureMinBrightness = -1.2f;
	PostProcess->Settings.AutoExposureMaxBrightness = 1.1f;
	PostProcess->Settings.bOverride_AutoExposureBias = true;
	PostProcess->Settings.AutoExposureBias = -0.35f;
	PostProcess->Settings.bOverride_AutoExposureSpeedUp = true;
	PostProcess->Settings.AutoExposureSpeedUp = 2.0f;
	PostProcess->Settings.bOverride_AutoExposureSpeedDown = true;
	PostProcess->Settings.AutoExposureSpeedDown = 0.75f;
	// Grade stays close to neutral; the atmosphere supplies the palette.
	PostProcess->Settings.bOverride_VignetteIntensity = true;
	PostProcess->Settings.VignetteIntensity = 0.22f;
	PostProcess->Settings.bOverride_FilmGrainIntensity = true;
	PostProcess->Settings.FilmGrainIntensity = 0.02f;
	PostProcess->Settings.bOverride_ColorSaturation = true;
	PostProcess->Settings.ColorSaturation = FVector4(0.93f, 0.95f, 1.0f, 1.0f);
	// Restrained bloom keeps emissive signage readable instead of hazy.
	PostProcess->Settings.bOverride_BloomIntensity = true;
	PostProcess->Settings.BloomIntensity = 0.18f;
	// Low motion blur: walking stays smooth but frames remain readable.
	PostProcess->Settings.bOverride_MotionBlurAmount = true;
	PostProcess->Settings.MotionBlurAmount = 0.05f;
	// Ambient occlusion seats furniture and shelf stock into their corners.
	PostProcess->Settings.bOverride_AmbientOcclusionIntensity = true;
	PostProcess->Settings.AmbientOcclusionIntensity = 0.38f;
	PostProcess->Settings.bOverride_AmbientOcclusionRadius = true;
	PostProcess->Settings.AmbientOcclusionRadius = 62.0f;
}

void AIGPrologueWorldScene::BeginPlay()
{
	Super::BeginPlay();
	InitializePrologue();
}

// ---------------------------------------------------------------------------
// Assembly helpers
// ---------------------------------------------------------------------------

UStaticMeshComponent* AIGPrologueWorldScene::CreateBlock(
	const FVector& Center,
	const FVector& SizeCentimeters,
	UMaterialInterface* Material,
	const bool bEnableCollision,
	UStaticMesh* MeshOverride,
	const FRotator& Rotation,
	USceneComponent* Parent)
{
	UStaticMeshComponent* Block = NewObject<UStaticMeshComponent>(
		this,
		*FString::Printf(TEXT("Block_%d"), BlockCounter++));
	USceneComponent* ResolvedParent =
		Parent ? Parent : (ActiveParent ? ActiveParent.Get() : SceneRoot.Get());
	Block->SetupAttachment(ResolvedParent);
	Block->SetStaticMesh(MeshOverride ? MeshOverride : CubeMesh.Get());
	Block->SetMaterial(0, Material);
	Block->SetRelativeLocation(Center);
	Block->SetRelativeRotation(Rotation);
	Block->SetRelativeScale3D(SizeCentimeters / 100.0f);
	Block->SetMobility(EComponentMobility::Static);
	Block->SetGenerateOverlapEvents(false);
	Block->SetCanEverAffectNavigation(false);
	Block->SetCollisionProfileName(
		bEnableCollision
			? UCollisionProfile::BlockAll_ProfileName
			: UCollisionProfile::NoCollision_ProfileName);
	// Paper-thin dressing (posters, price rails, seams, panel grooves) sits
	// flush against its host surface; letting it cast shadows only produces
	// self-shadow acne and doubled contact lines.
	if (SizeCentimeters.GetMin() < 3.0f)
	{
		Block->SetCastShadow(false);
	}
	Block->RegisterComponent();
	GeometryComponents.Add(Block);
	return Block;
}

UStaticMeshComponent* AIGPrologueWorldScene::CreatePhysicsProp(
	UStaticMesh* Mesh,
	UMaterialInterface* Material,
	const FVector& Scale,
	const FVector& Location,
	const FRotator& Rotation,
	const float MassKg)
{
	UStaticMeshComponent* Prop = NewObject<UStaticMeshComponent>(
		this,
		*FString::Printf(TEXT("PhysProp_%d"), BlockCounter++));
	Prop->SetupAttachment(ActiveParent ? ActiveParent.Get() : SceneRoot.Get());
	Prop->SetStaticMesh(Mesh);
	Prop->SetMaterial(0, Material);
	Prop->SetRelativeLocation(Location);
	Prop->SetRelativeRotation(Rotation);
	Prop->SetRelativeScale3D(Scale);
	Prop->SetMobility(EComponentMobility::Movable);
	Prop->SetGenerateOverlapEvents(false);
	Prop->SetCanEverAffectNavigation(false);
	Prop->SetCollisionProfileName(UCollisionProfile::PhysicsActor_ProfileName);
	Prop->RegisterComponent();
	Prop->SetSimulatePhysics(true);
	Prop->SetMassOverrideInKg(NAME_None, FMath::Max(0.05f, MassKg));
	// Small household props should settle instead of skating and spinning for
	// seconds after a light capsule contact.
	Prop->SetAngularDamping(2.4f);
	Prop->SetLinearDamping(1.1f);
	GeometryComponents.Add(Prop);
	return Prop;
}

UPointLightComponent* AIGPrologueWorldScene::CreateLight(
	const FVector& Location,
	const float Intensity,
	const float Radius,
	const FLinearColor& Color,
	const bool bCastShadows,
	const float SourceRadius,
	USceneComponent* Parent)
{
	UPointLightComponent* Light = NewObject<UPointLightComponent>(
		this,
		*FString::Printf(TEXT("Light_%d"), BlockCounter++));
	USceneComponent* ResolvedParent =
		Parent ? Parent : (ActiveParent ? ActiveParent.Get() : SceneRoot.Get());
	Light->SetupAttachment(ResolvedParent);
	Light->SetMobility(EComponentMobility::Movable);
	Light->SetRelativeLocation(Location);
	Light->SetIntensity(Intensity);
	Light->SetAttenuationRadius(Radius);
	Light->SetLightColor(Color);
	Light->SetCastShadows(bCastShadows);
	// A physical source size softens penumbras; contact shadows ground props.
	// Real fixtures are area sources, so a light with no radius given still
	// gets a small one rather than a point-hard edge.
	const float EffectiveSourceRadius = FMath::Max(SourceRadius, 3.0f);
	Light->SetSourceRadius(EffectiveSourceRadius);
	Light->SetSoftSourceRadius(EffectiveSourceRadius * 1.6f);
	Light->ContactShadowLength = 0.08f;
	Light->ContactShadowLengthInWS = false;
	Light->ShadowSharpen = 0.0f;
	Light->SetSpecularScale(0.85f);
	Light->RegisterComponent();
	Lights.Add(Light);
	return Light;
}

UStaticMeshComponent* AIGPrologueWorldScene::CreateDecoOnComponent(
	USceneComponent* Parent,
	UStaticMesh* Mesh,
	UMaterialInterface* Material,
	const FVector& RelativeLocation,
	const FRotator& RelativeRotation,
	const FVector& RelativeScale)
{
	if (!Parent || !Mesh)
	{
		return nullptr;
	}

	AActor* OwnerActor = Parent->GetOwner();
	UStaticMeshComponent* Deco = NewObject<UStaticMeshComponent>(
		OwnerActor ? static_cast<UObject*>(OwnerActor) : static_cast<UObject*>(this),
		*FString::Printf(TEXT("Deco_%d"), BlockCounter++));
	Deco->SetupAttachment(Parent);
	Deco->SetStaticMesh(Mesh);
	Deco->SetMaterial(0, Material);
	Deco->SetRelativeLocation(RelativeLocation);
	Deco->SetRelativeRotation(RelativeRotation);
	Deco->SetRelativeScale3D(RelativeScale);
	Deco->SetMobility(EComponentMobility::Movable);
	Deco->SetGenerateOverlapEvents(false);
	Deco->SetCanEverAffectNavigation(false);
	Deco->SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);
	Deco->RegisterComponent();
	return Deco;
}

void AIGPrologueWorldScene::LoadTexturedMaterials()
{
	const TCHAR* MaterialNames[] = {
		TEXT("M_Jangpan"), TEXT("M_Wallpaper_X"), TEXT("M_Wallpaper_Y"),
		TEXT("M_WallpaperCeil"), TEXT("M_WoodFurnitureUV"), TEXT("M_BeddingUV"),
		TEXT("M_AsphaltWorld"), TEXT("M_Brick_X"), TEXT("M_Brick_Y"),
		TEXT("M_Concrete_XY"), TEXT("M_Concrete_X"), TEXT("M_Concrete_Y"),
		TEXT("M_ConcreteDark_X"), TEXT("M_ConcreteDark_Y"),
		TEXT("M_StoreTileWorld"), TEXT("M_StoreCeilWorld"),
		TEXT("M_StoreWall_X"), TEXT("M_StoreWall_Y"),
		TEXT("M_MetalUV"), TEXT("M_ShelfSteelUV"),
		TEXT("M_PosterSale"), TEXT("M_PosterRamyeon"), TEXT("M_PosterFlyer"),
		TEXT("M_NoteFridge"), TEXT("M_SignToilet"), TEXT("M_SignAutoDoor"),
		TEXT("M_PriceStrip"), TEXT("M_SignMainLit"), TEXT("M_SignBladeLit"),
		TEXT("M_SignVilla"), TEXT("M_Plate401"), TEXT("M_Plate403"), TEXT("M_Plate404"),
		TEXT("M_ElevatorPanel"), TEXT("M_ClockFace"),
		TEXT("M_Shutter_X"), TEXT("M_SignLaundry"), TEXT("M_SignHair"),
		TEXT("M_SignHof"), TEXT("M_SignSuper"), TEXT("M_Banner"),
		TEXT("M_NoticeA4"), TEXT("M_DoorAd"), TEXT("M_Calendar"),
		TEXT("M_FireBox"), TEXT("M_TobaccoNotice"), TEXT("M_ConeOrange"),
		TEXT("M_SignPC"), TEXT("M_SignKaraoke"),
		TEXT("M_LabelWater"), TEXT("M_LabelGreenTea"), TEXT("M_LabelBarley"),
		TEXT("M_LabelSoda"), TEXT("M_LabelSoju"), TEXT("M_LabelRamyeon"),
		TEXT("M_SnackShrimp"), TEXT("M_SnackPotato"),
		TEXT("M_SnackSquid"), TEXT("M_SnackCorn"),
		TEXT("M_SkyDawn"),
		// Villa surfaces and fittings from the reference photos.
		TEXT("M_Stucco_X"), TEXT("M_Stucco_Y"), TEXT("M_StuccoCeil"),
		TEXT("M_GraniteTile_XY"), TEXT("M_GranitePanel_X"), TEXT("M_GranitePanel_Y"),
		TEXT("M_MarbleFloor_XY"), TEXT("M_StainlessUV"), TEXT("M_CabMirrorUV"),
		TEXT("M_SteelDoorUV"), TEXT("M_KitchenGlossUV"), TEXT("M_CounterStoneUV"),
		TEXT("M_DoorLock"), TEXT("M_MeterBox"), TEXT("M_Intercom"),
		TEXT("M_LiftCOP"), TEXT("M_LiftHall"), TEXT("M_SwitchPlate"),
		// Aged paper stock for readable notes, and the rental notice.
		TEXT("M_PaperClean"), TEXT("M_PaperWet"), TEXT("M_PaperFolded"),
		TEXT("M_PaperOld"), TEXT("M_NoticeRent"), TEXT("M_WetStep"),
	};

	int32 LoadedCount = 0;
	for (const TCHAR* MaterialName : MaterialNames)
	{
		const FString AssetPath = FString::Printf(
			TEXT("/Game/Prototype/Materials/%s.%s"), MaterialName, MaterialName);
		if (UMaterialInterface* Material =
			LoadObject<UMaterialInterface>(nullptr, *AssetPath))
		{
			TexturedMaterials.Add(FName(MaterialName), Material);
			++LoadedCount;
		}
	}

	UE_LOG(
		LogIndieGame,
		Display,
		TEXT("Textured materials loaded: %d/%d (missing entries fall back to flat colors)"),
		LoadedCount,
		static_cast<int32>(UE_ARRAY_COUNT(MaterialNames)));
}

UMaterialInterface* AIGPrologueWorldScene::TexMat(
	const FName MaterialName,
	UMaterialInterface* Fallback) const
{
	const TObjectPtr<UMaterialInterface>* Found = TexturedMaterials.Find(MaterialName);
	return Found && *Found ? Found->Get() : Fallback;
}

UStaticMesh* AIGPrologueWorldScene::PropMesh(
	const TCHAR* MeshName,
	UStaticMesh* Fallback) const
{
	const FName Key(MeshName);
	if (const TObjectPtr<UStaticMesh>* Cached = PropMeshes.Find(Key))
	{
		return *Cached ? Cached->Get() : Fallback;
	}

	const FString AssetPath = FString::Printf(
		TEXT("/Game/Meshes/%s.%s"), MeshName, MeshName);
	UStaticMesh* Loaded = LoadObject<UStaticMesh>(nullptr, *AssetPath);
	PropMeshes.Add(Key, Loaded);
	if (!Loaded)
	{
		UE_LOG(
			LogIndieGame,
			Warning,
			TEXT("Prop mesh missing (run Scripts/generate_meshes.py): %s"),
			MeshName);
	}
	return Loaded ? Loaded : Fallback;
}

UStaticMeshComponent* AIGPrologueWorldScene::CreateProp(
	const TCHAR* MeshName,
	const FVector& BaseLocation,
	UMaterialInterface* Material,
	const float YawDegrees,
	const float UniformScale,
	const bool bEnableCollision)
{
	UStaticMesh* Mesh = PropMesh(MeshName);
	if (!Mesh)
	{
		return nullptr;
	}

	UStaticMeshComponent* Prop = NewObject<UStaticMeshComponent>(
		this,
		*FString::Printf(TEXT("Prop_%d"), BlockCounter++));
	Prop->SetupAttachment(ActiveParent ? ActiveParent.Get() : SceneRoot.Get());
	Prop->SetStaticMesh(Mesh);
	Prop->SetMaterial(0, Material);
	Prop->SetRelativeLocation(BaseLocation);
	Prop->SetRelativeRotation(FRotator(0.0f, YawDegrees, 0.0f));
	Prop->SetRelativeScale3D(FVector(UniformScale));
	Prop->SetMobility(EComponentMobility::Static);
	Prop->SetGenerateOverlapEvents(false);
	Prop->SetCanEverAffectNavigation(false);
	Prop->SetCollisionProfileName(
		bEnableCollision
			? UCollisionProfile::BlockAll_ProfileName
			: UCollisionProfile::NoCollision_ProfileName);
	Prop->RegisterComponent();
	GeometryComponents.Add(Prop);
	return Prop;
}

void AIGPrologueWorldScene::CreateCupRamyeon(
	const FVector& BaseLocation,
	const float YawDegrees)
{
	// Foam cup body. SM_CupNoodle models the taper, the rolled rim and the
	// domed lid; all of it is expanded polystyrene white except the lid, which
	// the foil disc below covers.
	CreateProp(
		TEXT("SM_CupNoodle"), BaseLocation, CupNoodleMaterial, YawDegrees, 1.0f, true);

	// The printed band. SM_CupSleeve is authored at unit size — base radius 1,
	// height 1 — for the same reason SM_LabelSleeve is: the cylindrical UV
	// projection normalises against the mesh's extent, so a sleeve modelled at
	// real centimetres tiles the artwork dozens of times around. X/Y carry the
	// cup's base radius, Z the band height.
	if (UStaticMeshComponent* Sleeve = CreateProp(
			TEXT("SM_CupSleeve"), BaseLocation + FVector(0.0f, 0.0f, 1.6f),
			TexMat(TEXT("M_LabelRamyeon"), CupNoodleMaterial), YawDegrees, 1.0f, false))
	{
		Sleeve->SetRelativeScale3D(FVector(4.10f, 4.10f, 7.2f));
		// It hugs the cup; its shadow would only fight the cup's own.
		Sleeve->SetCastShadow(false);
	}

	// Crimped foil lid.
	CreateProp(
		TEXT("SM_CupLid"), BaseLocation,
		TexMat(TEXT("M_StainlessUV"), MetalFrameMaterial), YawDegrees, 1.0f, false);
}

UStaticMeshComponent* AIGPrologueWorldScene::CreateBottleLabel(
	const FVector& BottleBase,
	const float Radius,
	const float BandBottomZ,
	const float BandHeight,
	const TCHAR* LabelMaterialName,
	const float YawDegrees)
{
	UStaticMesh* Sleeve = PropMesh(TEXT("SM_LabelSleeve"));
	if (!Sleeve)
	{
		return nullptr;
	}

	UStaticMeshComponent* Label = NewObject<UStaticMeshComponent>(
		this,
		*FString::Printf(TEXT("Label_%d"), BlockCounter++));
	Label->SetupAttachment(ActiveParent ? ActiveParent.Get() : SceneRoot.Get());
	Label->SetStaticMesh(Sleeve);
	Label->SetMaterial(0, TexMat(LabelMaterialName, FridgeInteriorMaterial));
	Label->SetRelativeLocation(BottleBase + FVector(0.0f, 0.0f, BandBottomZ));
	Label->SetRelativeRotation(FRotator(0.0f, YawDegrees, 0.0f));
	// The sleeve is a unit cylinder: X/Y carry the radius, Z the band height.
	Label->SetRelativeScale3D(FVector(Radius, Radius, BandHeight));
	Label->SetMobility(EComponentMobility::Static);
	Label->SetGenerateOverlapEvents(false);
	Label->SetCanEverAffectNavigation(false);
	Label->SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);
	// The sleeve hugs the bottle; its shadow would just fight the bottle's.
	Label->SetCastShadow(false);
	Label->RegisterComponent();
	GeometryComponents.Add(Label);
	return Label;
}

UStaticMesh* AIGPrologueWorldScene::FindPhotoPropMesh(const TCHAR* AssetId) const
{
	// Interchange nests generated meshes below the source folder.  Query the
	// registry first instead of probing a guessed object path; every failed
	// guess printed a scary runtime warning even when the asset existed.
	const FAssetRegistryModule& RegistryModule =
		FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
	TArray<FAssetData> Assets;
	RegistryModule.Get().GetAssetsByPath(
		*FString::Printf(TEXT("/Game/Photo/Props/%s"), AssetId), Assets, true);

	// Interchange can create several meshes in one source folder. Registry
	// iteration order is not stable, and choosing its first entry previously
	// turned a cash register into a loose drawer and an outdoor set into one
	// chair. Explicitly select the authored primary mesh.
	static const TMap<FName, FName> PreferredAssetNames = {
		{TEXT("CashRegister_01"), TEXT("CashRegister_01_body")},
		{TEXT("metal_office_desk"), TEXT("metal_office_desk")},
		{TEXT("outdoor_table_chair_set_01"), TEXT("outdoor_table_chair_set_01_table")},
		{TEXT("wine_bottles_01"), TEXT("wine_bottles_01_bordeaux")},
	};
	const FName AssetIdName(AssetId);
	if (AssetIdName == TEXT("modern_wooden_cabinet"))
	{
		// This source imported its body only as a skeletal mesh; its two static
		// entries are detached doors. Use the complete procedural cabinet.
		return nullptr;
	}
	const FName PreferredName = PreferredAssetNames.Contains(AssetIdName)
		? PreferredAssetNames[AssetIdName]
		: FName(FString::Printf(TEXT("%s_1k"), AssetId));

	TArray<FAssetData> StaticMeshes;
	for (const FAssetData& Asset : Assets)
	{
		if (Asset.AssetClassPath == UStaticMesh::StaticClass()->GetClassPathName())
		{
			StaticMeshes.Add(Asset);
			if (Asset.AssetName == PreferredName)
			{
				return Cast<UStaticMesh>(Asset.GetAsset());
			}
		}
	}

	// A newly imported source may use a different suffix. Keep that fallback
	// deterministic so two machines still build the same world.
	StaticMeshes.Sort([](const FAssetData& Left, const FAssetData& Right)
	{
		return Left.AssetName.LexicalLess(Right.AssetName);
	});
	if (!StaticMeshes.IsEmpty())
	{
		UE_LOG(
			LogIndieGame,
			Warning,
			TEXT("Photo prop '%s' missing preferred mesh '%s'; using '%s'."),
			AssetId,
			*PreferredName.ToString(),
			*StaticMeshes[0].AssetName.ToString());
		return Cast<UStaticMesh>(StaticMeshes[0].GetAsset());
	}
	return nullptr;
}

UStaticMeshComponent* AIGPrologueWorldScene::PlacePhotoProp(
	const TCHAR* AssetId,
	const FVector& FloorCenter,
	const FVector& TargetSize,
	const float YawDegrees,
	const bool bEnableCollision)
{
	UStaticMesh* Mesh = FindPhotoPropMesh(AssetId);
	if (!Mesh)
	{
		return nullptr;
	}

	const FBoxSphereBounds Bounds = Mesh->GetBounds();
	const FVector MeshSize = Bounds.BoxExtent * 2.0f;
	if (MeshSize.GetMin() <= KINDA_SMALL_NUMBER)
	{
		return nullptr;
	}

	// Uniform scale that fits the target box without distorting the scan.
	const float Scale = FMath::Min3(
		TargetSize.X / MeshSize.X,
		TargetSize.Y / MeshSize.Y,
		TargetSize.Z / MeshSize.Z);

	UStaticMeshComponent* Prop = NewObject<UStaticMeshComponent>(
		this,
		*FString::Printf(TEXT("PhotoProp_%d"), BlockCounter++));
	Prop->SetupAttachment(ActiveParent ? ActiveParent.Get() : SceneRoot.Get());
	Prop->SetStaticMesh(Mesh);
	Prop->SetMobility(EComponentMobility::Static);
	Prop->SetGenerateOverlapEvents(false);
	Prop->SetCanEverAffectNavigation(false);
	Prop->SetCollisionProfileName(
		bEnableCollision
			? UCollisionProfile::BlockAll_ProfileName
			: UCollisionProfile::NoCollision_ProfileName);

	const FRotator Rotation(0.0f, YawDegrees, 0.0f);
	const FVector RotatedOriginOffset =
		Rotation.RotateVector(FVector(Bounds.Origin.X, Bounds.Origin.Y, 0.0f)) * Scale;
	const FVector Location(
		FloorCenter.X - RotatedOriginOffset.X,
		FloorCenter.Y - RotatedOriginOffset.Y,
		FloorCenter.Z - (Bounds.Origin.Z - Bounds.BoxExtent.Z) * Scale);
	Prop->SetRelativeLocation(Location);
	Prop->SetRelativeRotation(Rotation);
	Prop->SetRelativeScale3D(FVector(Scale));
	Prop->RegisterComponent();
	GeometryComponents.Add(Prop);
	return Prop;
}

UAudioComponent* AIGPrologueWorldScene::CreateAmbientBed(
	USoundBase* Sound,
	const FVector& Location,
	const float Volume,
	const float InnerRadius,
	const float FalloffDistance)
{
	UAudioComponent* Bed = NewObject<UAudioComponent>(
		this,
		*FString::Printf(TEXT("AmbientBed_%d"), BlockCounter++));
	Bed->SetupAttachment(SceneRoot);
	Bed->SetRelativeLocation(Location);
	Bed->SetSound(Sound);
	Bed->SetVolumeMultiplier(Volume);
	Bed->bAutoActivate = false;
	Bed->bOverrideAttenuation = true;
	Bed->AttenuationOverrides.bAttenuate = true;
	Bed->AttenuationOverrides.bSpatialize = true;
	Bed->AttenuationOverrides.AttenuationShapeExtents = FVector(InnerRadius, 0.0f, 0.0f);
	Bed->AttenuationOverrides.FalloffDistance = FalloffDistance;
	Bed->AttenuationOverrides.DistanceAlgorithm = EAttenuationDistanceModel::NaturalSound;
	Bed->AttenuationOverrides.dBAttenuationAtMax = -60.0f;
	Bed->RegisterComponent();
	Bed->Play();
	AmbientBeds.Add(Bed);
	return Bed;
}

// ---------------------------------------------------------------------------
// Construction stages
// ---------------------------------------------------------------------------

void AIGPrologueWorldScene::InitializePrologue()
{
	if (bPrologueInitialized)
	{
		return;
	}

	if (!PositionPlayer())
	{
		if (++PlayerPositionAttempts < IGPrologueWorld::MaxPlayerPositionAttempts)
		{
			GetWorldTimerManager().SetTimerForNextTick(
				this,
				&ThisClass::InitializePrologue);
		}
		return;
	}

	bPrologueInitialized = true;

	// Interchange photo props are discovered asynchronously in editor-game
	// launches.  Building the world before the registry finished meant every
	// lookup missed and silently fell back to cubes on a cold run.
	FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"))
		.Get()
		.WaitForCompletion();
	LoadTexturedMaterials();

	// Unit 404 and its corridor live on the 4th floor, three slabs up.
	// Static mobility is required so the static wall blocks can attach.
	UpperFloorRoot = NewObject<USceneComponent>(this, TEXT("UpperFloorRoot"));
	UpperFloorRoot->SetupAttachment(SceneRoot);
	UpperFloorRoot->SetMobility(EComponentMobility::Static);
	UpperFloorRoot->SetRelativeLocation(FVector(0, 0, 900));
	UpperFloorRoot->RegisterComponent();

	BuildApartment();
	BuildCorridor();
	BuildChapterTwoOverlay();
	BuildLobby();
	BuildAlley();
	BuildStore();
	BuildSkyAndFog();
	SpawnInteractables();
	SpawnChapterTwoInteractables();
	SpawnDirectors();
	CreateAmbience();
	if (FParse::Param(FCommandLine::Get(), TEXT("IGChapterTwo"))
		|| FParse::Param(FCommandLine::Get(), TEXT("IGCaptureCH02")))
	{
		EnterChapterTwo();
	}
	else
	{
		SpawnDemoDirectorIfRequested();
	}

	// A tired ballast shimmer runs for the whole session.
	GetWorldTimerManager().SetTimer(
		CorridorFlickerHandle,
		this,
		&ThisClass::HandleCorridorFlicker,
		0.09f,
		true);

	if (UGameInstance* GameInstance = GetGameInstance())
	{
		if (UIGStoryStateSubsystem* StoryState =
			GameInstance->GetSubsystem<UIGStoryStateSubsystem>())
		{
			StoryState->OnStoryStateTagChanged.AddUniqueDynamic(
				this,
				&ThisClass::HandleStoryStateChanged);
		}
	}

	UE_LOG(LogIndieGame, Display, TEXT("Prologue world ready: apartment, alley and store assembled."));
}

bool AIGPrologueWorldScene::PositionPlayer()
{
	APlayerController* PlayerController = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;
	APawn* PlayerPawn = PlayerController ? PlayerController->GetPawn() : nullptr;
	if (!PlayerController || !PlayerPawn)
	{
		return false;
	}

	PlayerPawn->SetActorLocationAndRotation(
		IGPrologueWorld::PlayerLocation,
		IGPrologueWorld::PlayerActorRotation,
		false,
		nullptr,
		ETeleportType::TeleportPhysics);
	PlayerController->SetControlRotation(IGPrologueWorld::PlayerViewRotation);
	return true;
}

void AIGPrologueWorldScene::BuildApartment()
{
	// Everything here is 4th-floor local; the root lifts it into place.
	ActiveParent = UpperFloorRoot;

	UMaterialInterface* Jangpan = TexMat(TEXT("M_Jangpan"), FloorMaterial);
	UMaterialInterface* WallX = TexMat(TEXT("M_Wallpaper_X"), WallMaterial);
	UMaterialInterface* WallY = TexMat(TEXT("M_Wallpaper_Y"), WallMaterial);
	UMaterialInterface* CeilHome = TexMat(TEXT("M_WallpaperCeil"), WallMaterial);
	UMaterialInterface* Furniture = TexMat(TEXT("M_WoodFurnitureUV"), WoodMaterial);
	UMaterialInterface* Bedding = TexMat(TEXT("M_BeddingUV"), BeddingMaterial);
	UMaterialInterface* Metal = TexMat(TEXT("M_MetalUV"), MetalFrameMaterial);

	// Shell: interior 380 x 430 cm (about 5 pyeong), 230 cm ceiling.
	CreateBlock(FVector(0, 0, -10), FVector(440, 490, 20), Jangpan);
	CreateBlock(FVector(0, 0, 240), FVector(440, 490, 20), CeilHome);
	CreateBlock(FVector(-200, 0, 115), FVector(20, 490, 230), WallY);
	CreateBlock(FVector(200, 0, 115), FVector(20, 490, 230), WallY);
	CreateBlock(FVector(0, 225, 115), FVector(440, 20, 230), WallX);
	// South wall with the entrance opening (X 98..186).
	CreateBlock(FVector(-61, -225, 115), FVector(318, 20, 230), WallX);
	CreateBlock(FVector(203, -225, 115), FVector(34, 20, 230), WallX);
	CreateBlock(FVector(142, -225, 215), FVector(88, 20, 30), WallX);
	// Entrance shoe step.
	CreateBlock(FVector(143, -195, 4), FVector(80, 40, 8), TexMat(TEXT("M_Concrete_XY"), ConcreteDarkMaterial));

	// Bed: a real scanned frame when available, greybox otherwise. The
	// mattress/duvet dressing sits on top either way.
	if (!PlacePhotoProp(TEXT("old_bed_frame"), FVector(-140, 110, 0), FVector(108, 208, 100), 90.0f))
	{
		CreateBlock(FVector(-140, 110, 20), FVector(100, 200, 40), Furniture);
		CreateBlock(FVector(-140, 211, 55), FVector(100, 8, 110), Furniture);
	}
	CreateBlock(FVector(-140, 110, 47), FVector(94, 194, 18), Bedding);
	CreateBlock(FVector(-140, 60, 60), FVector(96, 112, 12), Bedding);
	CreateBlock(FVector(-140, 118, 63), FVector(96, 16, 8), Bedding);

	// Bedside table with an articulated lamp, desk with chair, wardrobe.
	if (!PlacePhotoProp(TEXT("side_table_01"), FVector(-160, -35, 0), FVector(58, 58, 60), 0.0f))
	{
		CreateBlock(FVector(-160, -35, 27.5f), FVector(55, 55, 55), Furniture);
	}
	PlacePhotoProp(TEXT("desk_lamp_arm_01"), FVector(-172, -52, 56), FVector(30, 30, 48), 35.0f, false);
	if (!PlacePhotoProp(TEXT("metal_office_desk"), FVector(-140, -185, 0), FVector(104, 58, 78), 0.0f))
	{
		CreateBlock(FVector(-140, -185, 37.5f), FVector(100, 55, 75), Furniture);
	}
	if (!PlacePhotoProp(TEXT("painted_wooden_chair_01"), FVector(-140, -138, 0), FVector(46, 46, 96), 180.0f))
	{
		CreateBlock(FVector(-140, -135, 22), FVector(40, 40, 44), Furniture);
	}
	if (!PlacePhotoProp(TEXT("modern_wooden_cabinet"), FVector(-170, -105, 0), FVector(40, 84, 186), 90.0f))
	{
		CreateBlock(FVector(-172, -105, 90), FVector(35, 80, 180), Furniture);
	}

	// --- Built-in kitchen line along the east wall ---------------------------
	// A real 원룸 is fitted, not furnished: one continuous run of white gloss
	// carcasses with finger-pull grooves instead of handles, a stone counter,
	// an inset stainless sink, an induction hob, a range hood, and the drum
	// washer that always lives under the same worktop.
	UMaterialInterface* Gloss = TexMat(TEXT("M_KitchenGlossUV"), SignWhiteMaterial);
	UMaterialInterface* CounterStone = TexMat(TEXT("M_CounterStoneUV"), StoreFloorMaterial);
	UMaterialInterface* Stainless = TexMat(TEXT("M_StainlessUV"), MetalFrameMaterial);

	// Carcass, recessed toe kick and the splashback upstand.
	CreateBlock(FVector(162, 128, 46), FVector(56, 176, 72), Gloss);
	CreateBlock(FVector(166, 128, 5), FVector(48, 176, 10), PlasticDarkMaterial);
	CreateBlock(FVector(187, 128, 96), FVector(6, 176, 20), CounterStone, false);
	// Worktop, split around the sink cut-out at Y 160..205.
	CreateBlock(FVector(161, 99, 84.5f), FVector(60, 122, 5), CounterStone);
	CreateBlock(FVector(161, 211, 84.5f), FVector(60, 14, 5), CounterStone);
	for (const float RailY : {160.0f, 205.0f})
	{
		CreateBlock(FVector(161, RailY, 84.5f), FVector(60, 4, 5), CounterStone, false);
	}

	// Drum washer built into the south end of the run.
	CreateBlock(FVector(162, 70, 46), FVector(56, 58, 72), Gloss);
	CreateBlock(FVector(133.4f, 70, 76), FVector(1.8f, 54, 11), PlasticDarkMaterial, false);
	CreateBlock(
		FVector(133.2f, 70, 44), FVector(42, 42, 4),
		Stainless, false, CylinderMesh, FRotator(90, 0, 0));
	CreateBlock(
		FVector(132.0f, 70, 44), FVector(33, 33, 3),
		GlassMaterial, false, CylinderMesh, FRotator(90, 0, 0));
	CreateBlock(FVector(133.6f, 46, 44), FVector(2, 4, 14), Stainless, false);

	// Cabinet fronts under the hob and the sink; the groove is the handle.
	for (const float DoorY : {129.0f, 186.0f})
	{
		CreateBlock(FVector(133.2f, DoorY, 46), FVector(1.8f, 54, 68), Gloss, false);
		CreateBlock(FVector(132.0f, DoorY, 78), FVector(1.4f, 50, 1.6f), PlasticDarkMaterial, false);
	}

	// Inset stainless sink: a real basin with walls, a drain and a rim flange.
	CreateBlock(FVector(160, 182, 63), FVector(46, 47, 2), Stainless);
	for (const float BasinY : {159.5f, 204.5f})
	{
		CreateBlock(FVector(160, BasinY, 75), FVector(46, 2, 26), Stainless, false);
	}
	CreateBlock(FVector(137.5f, 182, 75), FVector(2, 47, 26), Stainless, false);
	CreateBlock(FVector(182.5f, 182, 75), FVector(2, 47, 26), Stainless, false);
	CreateBlock(FVector(160, 182, 87.4f), FVector(54, 55, 1.4f), Stainless, false);
	CreateBlock(
		FVector(160, 182, 64.4f), FVector(9, 9, 1),
		PlasticDarkMaterial, false, CylinderMesh);
	// Gooseneck mixer tap: column, arc and spout, with the lever on the side.
	CreateBlock(FVector(178, 182, 100), FVector(4.4f, 4.4f, 26), Stainless, false, CylinderMesh);
	CreateBlock(
		FVector(169, 182, 113), FVector(4.4f, 4.4f, 20),
		Stainless, false, CylinderMesh, FRotator(90, 0, 0));
	CreateBlock(FVector(160, 182, 107), FVector(3.2f, 3.2f, 14), Stainless, false, CylinderMesh);
	CreateBlock(FVector(181, 182, 112), FVector(3, 9, 3), Stainless, false);

	// Induction hob: black glass, two element rings and the touch strip.
	CreateBlock(FVector(160, 128, 87.8f), FVector(46, 50, 1.4f), PlasticDarkMaterial, false);
	for (const FVector2D& Ring : {FVector2D(150, 116), FVector2D(170, 140)})
	{
		CreateBlock(
			FVector(Ring.X, Ring.Y, 88.6f), FVector(19, 19, 0.4f),
			ConcreteDarkMaterial, false, CylinderMesh);
	}
	CreateBlock(FVector(141, 128, 88.6f), FVector(6, 30, 0.4f), ConcreteDarkMaterial, false);

	// --- Wall units: carcass, gloss doors, valance with a live LED strip ----
	CreateBlock(FVector(174, 128, 178), FVector(32, 176, 68), Gloss);
	for (const float DoorY : {70.0f, 195.0f})
	{
		CreateBlock(FVector(157.4f, DoorY, 178), FVector(1.8f, 54, 64), Gloss, false);
		CreateBlock(FVector(156.2f, DoorY, 148), FVector(1.4f, 50, 1.6f), PlasticDarkMaterial, false);
	}
	CreateBlock(FVector(158, 128, 143), FVector(5, 176, 5), Gloss, false);
	CreateBlock(FVector(156.6f, 128, 142), FVector(2, 168, 1.6f), LightPanelMaterial, false);
	// Range hood between the wall units, over the hob.
	CreateBlock(FVector(172, 128, 168), FVector(36, 52, 26), Stainless, false);
	CreateBlock(FVector(166, 128, 151), FVector(24, 52, 9), Stainless, false);
	CreateBlock(FVector(154.5f, 128, 151), FVector(1.5f, 46, 4), PlasticDarkMaterial, false);
	CreateBlock(FVector(178, 128, 200), FVector(18, 26, 24), Stainless, false);

	// Microwave on the counter, next to the hob.
	CreateBlock(FVector(166, 47, 101), FVector(42, 34, 26), Stainless, false);
	CreateBlock(FVector(144.6f, 43, 101), FVector(1.4f, 22, 20), GlassMaterial, false);
	CreateBlock(FVector(144.6f, 60, 101), FVector(1.4f, 9, 20), PlasticDarkMaterial, false);

	// Bathroom door name plate.
	CreateBlock(FVector(20, -210.6f, 145), FVector(26, 1.5f, 13), TexMat(TEXT("M_SignToilet"), PlasticDarkMaterial), false);

	// Lived-in unit 404: wall AC unit, outlets, a July calendar, range hood.
	CreateBlock(FVector(40, -206, 196), FVector(82, 19, 27), FridgeBodyMaterial, false);
	CreateBlock(FVector(40, -196.2f, 188), FVector(70, 1.5f, 3), PlasticDarkMaterial, false);
	CreateBlock(FVector(-70, -212.5f, 32), FVector(7, 2, 11), FridgeInteriorMaterial, false);
	CreateBlock(FVector(186.5f, 40, 32), FVector(2, 7, 11), FridgeInteriorMaterial, false);
	CreateBlock(
		FVector(-187.2f, -60, 152), FVector(1.5f, 31, 42),
		TexMat(TEXT("M_Calendar"), SignWhiteMaterial), false);
	CreateBlock(FVector(170, 90, 182), FVector(46, 40, 22), Metal, false);
	CreateBlock(FVector(170, 90, 212), FVector(13, 13, 38), Metal, false, CylinderMesh);

	// Ceiling: the flush LED slab every 원룸 has, plus the perimeter molding
	// that finishes wallpaper to ceiling. Both are cold at this hour — the
	// room is lit by the lamp and the window, not by the fixture.
	CreateBlock(FVector(-30, 0, 227), FVector(96, 62, 6), SignWhiteMaterial, false);
	CreateBlock(FVector(-30, 0, 230.5f), FVector(104, 70, 3), Furniture, false);
	for (const float MoldY : {-213.0f, 213.0f})
	{
		CreateBlock(FVector(0, MoldY, 224), FVector(378, 5, 7), Furniture, false);
	}
	for (const float MoldX : {-187.0f, 187.0f})
	{
		CreateBlock(FVector(MoldX, 0, 224), FVector(5, 428, 7), Furniture, false);
	}

	// Apartment lighting: no ceiling light at this hour — only the warm
	// bedside lamp, the strip left on under the wall units, and the cool
	// spill through the window; the sky light carries the rest physically.
	CreateLight(FVector(-160, -35, 110), 175.0f, 330.0f, FLinearColor(1.0f, 0.48f, 0.22f), true, 12.0f);
	CreateLight(FVector(-100, 190, 160), 85.0f, 400.0f, FLinearColor(0.52f, 0.64f, 0.95f), true, 24.0f);
	UPointLightComponent* UnderCabinet = CreateLight(
		FVector(150, 128, 138), 115.0f, 280.0f,
		FLinearColor(0.92f, 0.96f, 1.0f), true, 10.0f);
	UnderCabinet->SetVolumetricScatteringIntensity(0.2f);

	// Window frame and cross bars turn the glow plane into a real window.
	CreateBlock(FVector(-100, 212, 196), FVector(130, 5, 7), PlasticDarkMaterial, false);
	CreateBlock(FVector(-100, 212, 104), FVector(130, 5, 7), PlasticDarkMaterial, false);
	CreateBlock(FVector(-163, 212, 150), FVector(7, 5, 99), PlasticDarkMaterial, false);
	CreateBlock(FVector(-37, 212, 150), FVector(7, 5, 99), PlasticDarkMaterial, false);
	CreateBlock(FVector(-100, 212, 150), FVector(124, 4, 5), PlasticDarkMaterial, false);
	CreateBlock(FVector(-100, 212, 150), FVector(5, 4, 92), PlasticDarkMaterial, false);
	// Venetian blind, drawn up into its stack: the headrail, the pulled-up
	// slat bundle and the cord. Leaving it up keeps the moonlight beam.
	CreateBlock(FVector(-100, 207, 202), FVector(134, 7, 8), SignWhiteMaterial, false);
	for (const float SlatZ : {188.0f, 191.5f, 195.0f})
	{
		CreateBlock(FVector(-100, 206.5f, SlatZ), FVector(130, 6, 2), SignWhiteMaterial, false);
	}
	CreateBlock(FVector(-36, 205, 160), FVector(1.2f, 1.2f, 76), SignWhiteMaterial, false);

	// Entrance wall: video intercom, the switch bank beside it, and the shoe
	// cabinet that stands against every Korean entryway.
	CreateBlock(
		FVector(62, -212.5f, 145), FVector(17, 5, 23),
		TexMat(TEXT("M_Intercom"), SignWhiteMaterial), false);
	CreateBlock(FVector(62, -214, 145), FVector(20, 4, 26), SignWhiteMaterial, false);
	CreateBlock(
		FVector(90, -213.4f, 128), FVector(10, 2, 10),
		TexMat(TEXT("M_SwitchPlate"), SignWhiteMaterial), false);
	CreateBlock(FVector(48, -198, 55), FVector(80, 32, 110), Gloss);
	for (const float ShelfZ : {28.0f, 82.0f})
	{
		CreateBlock(FVector(48, -181.6f, ShelfZ), FVector(76, 1.6f, 52), Gloss, false);
		CreateBlock(FVector(48, -180.6f, ShelfZ + 26), FVector(70, 1.2f, 1.6f), PlasticDarkMaterial, false);
	}
	CreateBlock(FVector(48, -198, 111.5f), FVector(84, 34, 3), Furniture, false);

	// Baseboard trim along the interior walls.
	CreateBlock(FVector(0, 213, 5), FVector(378, 4, 10), Furniture, false);
	CreateBlock(FVector(-188, 0, 5), FVector(4, 428, 10), Furniture, false);
	CreateBlock(FVector(188, 0, 5), FVector(4, 428, 10), Furniture, false);
	CreateBlock(FVector(-46, -213, 5), FVector(286, 4, 10), Furniture, false);

	// Entryway slippers the player can kick around.
	CreatePhysicsProp(
		CubeMesh, PlasticDarkMaterial,
		FVector(0.09f, 0.26f, 0.03f), FVector(120, -185, 3), FRotator(0, 15, 0), 0.2f);
	CreatePhysicsProp(
		CubeMesh, PlasticDarkMaterial,
		FVector(0.09f, 0.26f, 0.03f), FVector(148, -192, 3), FRotator(0, -8, 0), 0.2f);

	ActiveParent = nullptr;
}

void AIGPrologueWorldScene::BuildCorridor()
{
	// 4F hallway of the villa: unit doors on the north side, windows to the
	// alley on the south, the elevator at the east end and stairs going down
	// into darkness at the west end. All coordinates are 4th-floor local.
	ActiveParent = UpperFloorRoot;

	// Surfaces are the ones every Korean walk-up landing actually has:
	// troweled stucco on the walls, 600 mm speckled granite tile underfoot,
	// a dark granite skirting band, and a painted stucco soffit overhead.
	UMaterialInterface* CorridorFloor = TexMat(TEXT("M_GraniteTile_XY"), ConcreteMaterial);
	UMaterialInterface* CorridorCeil = TexMat(TEXT("M_StuccoCeil"), ConcreteMaterial);
	UMaterialInterface* CorridorWallX = TexMat(TEXT("M_Stucco_X"), ConcreteMaterial);
	UMaterialInterface* CorridorWallY = TexMat(TEXT("M_Stucco_Y"), ConcreteMaterial);
	UMaterialInterface* Skirting = TexMat(TEXT("M_ConcreteDark_X"), ConcreteDarkMaterial);
	UMaterialInterface* SteelDoor = TexMat(TEXT("M_SteelDoorUV"), DoorMaterial);
	UMaterialInterface* Stainless = TexMat(TEXT("M_StainlessUV"), MetalFrameMaterial);
	UMaterialInterface* Metal = TexMat(TEXT("M_MetalUV"), MetalFrameMaterial);

	// Floor and ceiling. The hallway runs well past our door so that leaving
	// 404 means actually walking the building, not stepping into the lift.
	// Interior X -320..700, Y -375..-235, height 240.
	CreateBlock(FVector(190, -305, -10), FVector(1040, 160, 20), CorridorFloor);
	CreateBlock(FVector(190, -305, 250), FVector(1040, 160, 20), CorridorCeil);

	// South wall is solid on this floor, with hopper windows onto the alley.
	// Bare glow planes read as blue rectangles stuck on the wall, so each one
	// gets a reveal, an aluminium frame and a sill like a real opening.
	CreateBlock(FVector(190, -385, 120), FVector(1040, 20, 240), CorridorWallX);
	for (const float WindowX : {-120.0f, 60.0f, 300.0f, 520.0f})
	{
		CreateBlock(FVector(WindowX, -376, 150), FVector(78, 3, 66), WindowDarkMaterial, false);
		CreateBlock(FVector(WindowX, -377.5f, 184), FVector(88, 6, 5), PlasticDarkMaterial, false);
		CreateBlock(FVector(WindowX, -377.5f, 116), FVector(88, 6, 5), PlasticDarkMaterial, false);
		CreateBlock(FVector(WindowX - 41, -377.5f, 150), FVector(5, 6, 66), PlasticDarkMaterial, false);
		CreateBlock(FVector(WindowX + 41, -377.5f, 150), FVector(5, 6, 66), PlasticDarkMaterial, false);
		CreateBlock(FVector(WindowX, -377.5f, 150), FVector(78, 5, 3.5f), PlasticDarkMaterial, false);
		CreateBlock(FVector(WindowX, -379, 111), FVector(94, 10, 5), Skirting, false);
	}

	// East end: elevator door opening (Y -360..-250); west end: dark stairwell.
	CreateBlock(FVector(710, -242.5f, 120), FVector(20, 15, 240), CorridorWallY);
	CreateBlock(FVector(710, -367.5f, 120), FVector(20, 15, 240), CorridorWallY);
	CreateBlock(FVector(710, -305, 225), FVector(20, 110, 30), CorridorWallY);
	CreateBlock(FVector(-330, -305, 120), FVector(20, 160, 240), PlasticDarkMaterial);

	// North wall extensions beyond the apartment span, plus the height filler
	// strip above the apartment's 230 cm wall to the corridor's 240 cm. The
	// east extension is permanently split around the future 403 doorway.
	// During CH01 a removable wall plug makes the split read as an ordinary
	// uninterrupted wall; CH02 parks that plug and reveals the open room.
	CreateBlock(FVector(-280, -225, 120), FVector(120, 20, 240), CorridorWallX);
	CreateBlock(FVector(302.5f, -225, 120), FVector(165, 20, 240), CorridorWallX);
	CreateBlock(FVector(582.5f, -225, 120), FVector(215, 20, 240), CorridorWallX);
	CreateBlock(FVector(430, -225, 225), FVector(90, 20, 30), CorridorWallX);
	ChapterOneMaskComponents.Add(CreateBlock(
		FVector(430, -225, 105), FVector(100, 20, 210), CorridorWallX));
	CreateBlock(FVector(0, -225, 235), FVector(440, 20, 10), CorridorWallX);

	// Neighbouring unit doors. The reference landing is a charcoal steel slab
	// with one brushed vertical band inset from the handle edge, small dark
	// squares punched down that band, a lever, a keypad lock and a peephole —
	// so that is exactly what gets built here, once per leaf.
	auto DressUnitDoor = [this, SteelDoor, Stainless, Metal](
		const float DoorX, const float FaceY)
	{
		CreateBlock(FVector(DoorX, FaceY, 100), FVector(84, 5, 200), SteelDoor);
		const float PlateY = FaceY - 2.9f;
		// Brushed band down the leaf, with the punched square inlays.
		CreateBlock(FVector(DoorX + 14, PlateY, 100), FVector(13, 0.8f, 188), Stainless, false);
		for (const float InlayZ : {36.0f, 68.0f, 100.0f, 132.0f, 164.0f})
		{
			CreateBlock(
				FVector(DoorX + 14, PlateY - 0.6f, InlayZ), FVector(5, 0.6f, 5),
				PlasticDarkMaterial, false);
		}
		// Lever handle on a rose, digital lock above it, peephole at eye level.
		CreateBlock(FVector(DoorX + 32, PlateY - 1.0f, 95), FVector(4, 2, 12), Metal, false);
		CreateBlock(FVector(DoorX + 32, PlateY - 3.5f, 95), FVector(3, 9, 3), Metal, false);
		CreateBlock(
			FVector(DoorX + 32, PlateY - 1.4f, 122), FVector(9, 3.2f, 24),
			TexMat(TEXT("M_DoorLock"), PlasticDarkMaterial), false);
		CreateBlock(
			FVector(DoorX, PlateY - 0.4f, 155), FVector(3, 1.2f, 3),
			Metal, false, CylinderMesh, FRotator(90, 0, 0));
		// Doorbell button and the video intercom plate beside the frame.
		CreateBlock(
			FVector(DoorX - 56, FaceY + 1.0f, 138), FVector(7, 2.5f, 11),
			TexMat(TEXT("M_Intercom"), SignWhiteMaterial), false);
		CreateBlock(
			FVector(DoorX - 56, FaceY - 0.6f, 122), FVector(3, 1.5f, 3),
			SnackRedMaterial, false, CylinderMesh, FRotator(90, 0, 0));
	};

	// West to east the landing reads 401, 403, 404. The plates used to be the
	// other way round, which put 401 next door to 404 and 403 at the far end —
	// wrong for a Korean walk-up, and chapter two depends on the player having
	// registered where 403 is.
	const float NeighborDoorXs[] = {-30.0f, -150.0f};
	const TCHAR* NeighborPlates[] = {TEXT("M_Plate403"), TEXT("M_Plate401")};
	for (int32 NeighborIndex = 0; NeighborIndex < 2; ++NeighborIndex)
	{
		const float DoorX = NeighborDoorXs[NeighborIndex];
		const int32 FirstDoorComponent = GeometryComponents.Num();
		DressUnitDoor(DoorX, -234.5f);
		// Door casing: two jamb strips and a head strip, in stucco like the
		// wall they are troweled into.
		CreateBlock(FVector(DoorX - 46, -233, 102), FVector(8, 7, 208), CorridorWallX, false);
		CreateBlock(FVector(DoorX + 46, -233, 102), FVector(8, 7, 208), CorridorWallX, false);
		CreateBlock(FVector(DoorX, -233, 204), FVector(100, 7, 8), CorridorWallX, false);
		CreateBlock(
			FVector(DoorX, -231.5f, 214), FVector(16, 2, 8),
			TexMat(NeighborPlates[NeighborIndex], FridgeInteriorMaterial), false);
		if (NeighborIndex == 0)
		{
			for (int32 ComponentIndex = FirstDoorComponent;
				ComponentIndex < GeometryComponents.Num();
				++ComponentIndex)
			{
				ChapterOneMaskComponents.Add(GeometryComponents[ComponentIndex]);
			}
		}
	}
	// Our 404 door casing and plate around the real swing door; the leaf
	// itself is the AIGSwingDoor actor, which dresses its own face.
	CreateBlock(FVector(96, -233, 102), FVector(8, 7, 208), CorridorWallX, false);
	CreateBlock(FVector(190, -233, 102), FVector(8, 7, 208), CorridorWallX, false);
	CreateBlock(FVector(143, -233, 206), FVector(102, 7, 8), CorridorWallX, false);
	CreateBlock(
		FVector(144, -233.5f, 214), FVector(16, 2, 8),
		TexMat(TEXT("M_Plate404"), FridgeInteriorMaterial), false);
	CreateBlock(
		FVector(88, -233.5f, 138), FVector(7, 2.5f, 11),
		TexMat(TEXT("M_Intercom"), SignWhiteMaterial), false);

	// Granite skirting, the way real landings finish the stucco to the tile.
	CreateBlock(FVector(-35, -233.4f, 6), FVector(580, 3.5f, 12), Skirting, false);
	CreateBlock(FVector(-35, -376.6f, 6), FVector(580, 3.5f, 12), Skirting, false);
	CreateBlock(FVector(302.5f, -233.4f, 6), FVector(165, 3.5f, 12), Skirting, false);
	CreateBlock(FVector(582.5f, -233.4f, 6), FVector(215, 3.5f, 12), Skirting, false);
	ChapterOneMaskComponents.Add(CreateBlock(
		FVector(430, -233.4f, 6), FVector(100, 3.5f, 12), Skirting, false));
	CreateBlock(FVector(455, -376.6f, 6), FVector(470, 3.5f, 12), Skirting, false);

	// Distribution board between the units, plus the fire cabinet.
	CreateBlock(
		FVector(-90, -232.4f, 155), FVector(34, 6, 50),
		TexMat(TEXT("M_MeterBox"), ConcreteDarkMaterial), false);
	CreateBlock(FVector(-90, -229.2f, 155), FVector(35, 1.2f, 51), Metal, false);
	CreateBlock(FVector(-75, -228.6f, 155), FVector(3, 1.5f, 6), PlasticDarkMaterial, false);
	CreateBlock(
		FVector(236, -371, 140), FVector(26, 9, 34),
		TexMat(TEXT("M_FireBox"), SnackRedMaterial), false);
	CreateBlock(
		FVector(232, -364, 26), FVector(15, 15, 48),
		SnackRedMaterial, true, CylinderMesh);
	CreateBlock(FVector(232, -364, 52), FVector(5, 5, 8), PlasticDarkMaterial, false);

	// Stairwell: five steps sinking west into darkness. Korean walk-ups have a
	// black steel balustrade with a flat cap rail and thin square balusters.
	int32 StepIndex = 0;
	for (const float StepX : {-230.0f, -252.0f, -274.0f, -296.0f, -318.0f})
	{
		CreateBlock(
			FVector(StepX, -305, -9.0f - StepIndex * 18.0f),
			FVector(22, 130, 18), CorridorFloor);
		// Stair nosing: a darker lip on every tread catches the hall light.
		CreateBlock(
			FVector(StepX - 10, -305, 0.4f - StepIndex * 18.0f),
			FVector(3, 128, 1.6f), Skirting, false);
		++StepIndex;
	}
	for (const float RailY : {-243.0f, -367.0f})
	{
		CreateBlock(
			FVector(-274, RailY, 60), FVector(120, 5, 5), PlasticDarkMaterial, false,
			nullptr, FRotator(39, 0, 0));
		CreateBlock(
			FVector(-274, RailY, 32), FVector(118, 3, 3), PlasticDarkMaterial, false,
			nullptr, FRotator(39, 0, 0));
		for (int32 BalusterIndex = 0; BalusterIndex < 5; ++BalusterIndex)
		{
			const float BalusterX = -232.0f - BalusterIndex * 21.0f;
			const float BalusterTop = 58.0f - BalusterIndex * 17.0f;
			CreateBlock(
				FVector(BalusterX, RailY, BalusterTop - 26.0f),
				FVector(2.6f, 2.6f, 56), PlasticDarkMaterial, false);
		}
	}
	// A tired green exit lamp glows at the stair throat.
	CreateBlock(FVector(-312, -305, 220), FVector(14, 8, 10),
		TexMat(TEXT("M_ScreenGlow"), ScreenGlowMaterial), false);

	// Ceiling fixtures down the whole hallway: flush round downlights, the way
	// the reference landing is lit. The far one has a dying ballast and never
	// stops shimmering.
	for (const float FixtureX : {-180.0f, 60.0f, 300.0f, 540.0f})
	{
		CreateBlock(
			FVector(FixtureX, -305, 237), FVector(26, 26, 4),
			Stainless, false, CylinderMesh);
		// Keep the emissive disc: putting a fixture out means darkening this
		// too, or a lit ring hangs on a black ceiling.
		CorridorLightDiscs.Add(CreateBlock(
			FVector(FixtureX, -305, 234.5f), FVector(21, 21, 2),
			LightPanelMaterial, false, CylinderMesh));
		UPointLightComponent* CorridorLight = CreateLight(
			FVector(FixtureX, -305, 226), 1200.0f, 520.0f,
			FLinearColor(0.86f, 0.97f, 1.0f), true, 16.0f);
		CorridorLight->SetVolumetricScatteringIntensity(0.16f);
		CorridorLights.Add(CorridorLight);
		if (FixtureX < 0.0f)
		{
			DegradedCorridorLight = CorridorLight;
			DegradedLightBaseIntensity = CorridorLight->Intensity;
		}
	}

	ActiveParent = nullptr;
}

void AIGPrologueWorldScene::BuildChapterTwoOverlay()
{
	// The second morning shares every streamed surface with CH01. Only the
	// impossible east-side 403 room and its clue dressing are additional.
	// Build those once, park them, and toggle visibility/collision at the loop
	// boundary; this avoids a hitch exactly where the cut must feel seamless.
	ActiveParent = UpperFloorRoot;

	UMaterialInterface* RoomWallX = TexMat(TEXT("M_Wallpaper_X"), WallMaterial);
	UMaterialInterface* RoomWallY = TexMat(TEXT("M_Wallpaper_Y"), WallMaterial);
	UMaterialInterface* RoomFloor = TexMat(TEXT("M_Jangpan"), FloorMaterial);
	UMaterialInterface* RoomCeiling = TexMat(TEXT("M_StuccoCeil"), ConcreteMaterial);
	UMaterialInterface* Furniture = TexMat(TEXT("M_WoodFurnitureUV"), WoodMaterial);
	UMaterialInterface* Bedding = TexMat(TEXT("M_BeddingUV"), BeddingMaterial);
	UMaterialInterface* DarkGloss = PlasticDarkMaterial;

	auto AddOverlay = [this](
		const FVector& Center,
		const FVector& Size,
		UMaterialInterface* Material,
		const bool bCollide = true,
		UStaticMesh* Mesh = nullptr,
		const FRotator& Rotation = FRotator::ZeroRotator,
		USceneComponent* Parent = nullptr) -> UStaticMeshComponent*
	{
		UStaticMeshComponent* Component =
			CreateBlock(Center, Size, Material, bCollide, Mesh, Rotation, Parent);
		if (!Component)
		{
			return nullptr;
		}

		ChapterTwoOverlayComponents.Add(Component);
		if (bCollide)
		{
			ChapterTwoCollisionComponents.Add(Component);
		}
		Component->SetVisibility(false, true);
		Component->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		return Component;
	};

	// 403 is a spatial echo of 404, shifted east. The geometry is sparse on
	// purpose: the open doorway frames the same bed/nightstand silhouette,
	// while darkness lets the player's memory complete the room.
	AddOverlay(FVector(430, 0, -10), FVector(440, 450, 20), RoomFloor);
	AddOverlay(FVector(430, 0, 240), FVector(440, 450, 20), RoomCeiling);
	AddOverlay(FVector(430, 225, 115), FVector(440, 20, 230), RoomWallX);
	AddOverlay(FVector(210, 0, 115), FVector(20, 450, 230), RoomWallY);
	AddOverlay(FVector(650, 0, 115), FVector(20, 450, 230), RoomWallY);
	AddOverlay(FVector(384, -233, 102), FVector(8, 7, 208), RoomWallX, false);
	AddOverlay(FVector(476, -233, 102), FVector(8, 7, 208), RoomWallX, false);
	AddOverlay(FVector(430, -233, 204), FVector(100, 7, 8), RoomWallX, false);
	AddOverlay(
		FVector(430, -233.5f, 214), FVector(16, 2, 8),
		TexMat(TEXT("M_Plate403"), FridgeInteriorMaterial), false);

	// Where 403 stood yesterday: four paler screw-shadow strips on bare wall,
	// not a replacement door. The player has to supply the memory.
	UMaterialInterface* OldPaint = TexMat(TEXT("M_Wallpaper_X"), WallMaterial);
	AddOverlay(FVector(-72, -232.0f, 100), FVector(2.5f, 1.4f, 190), OldPaint, false);
	AddOverlay(FVector(12, -232.0f, 100), FVector(2.5f, 1.4f, 190), OldPaint, false);
	AddOverlay(FVector(-30, -232.0f, 194), FVector(84, 1.4f, 2.5f), OldPaint, false);
	AddOverlay(FVector(-30, -232.0f, 7), FVector(84, 1.4f, 2.5f), OldPaint, false);

	// Bed, pillow and a human-scale mound kept low beneath the duvet. The old
	// 82 x 105 cm box plus a 55 cm sphere read as two construction blocks.
	AddOverlay(FVector(300, 110, 20), FVector(100, 200, 40), Furniture);
	AddOverlay(FVector(300, 110, 47), FVector(94, 194, 18), Bedding);
	AddOverlay(FVector(300, 132, 62), FVector(70, 92, 21), Bedding, false, SphereMesh);
	AddOverlay(FVector(300, 91, 60), FVector(72, 56, 17), Bedding, false, SphereMesh);
	AddOverlay(FVector(300, 182, 59), FVector(80, 47, 10), Bedding, false);
	AddOverlay(FVector(300, 188, 68), FVector(25, 23, 19), Bedding, false, SphereMesh);
	AddOverlay(FVector(365, 28, 28), FVector(55, 55, 56), Furniture);

	// Black horn-rim glasses at real scale (about 14 cm across).
	for (const float LensX : {361.2f, 368.8f})
	{
		AddOverlay(FVector(LensX, 21, 57.4f), FVector(5.5f, 0.8f, 0.8f), DarkGloss, false);
		AddOverlay(FVector(LensX, 21, 61.2f), FVector(5.5f, 0.8f, 0.8f), DarkGloss, false);
		AddOverlay(FVector(LensX - 2.75f, 21, 59.3f), FVector(0.8f, 0.8f, 4.6f), DarkGloss, false);
		AddOverlay(FVector(LensX + 2.75f, 21, 59.3f), FVector(0.8f, 0.8f, 4.6f), DarkGloss, false);
	}
	AddOverlay(FVector(365, 21, 59.3f), FVector(2.1f, 0.8f, 0.8f), DarkGloss, false);
	AddOverlay(
		FVector(356.0f, 24, 59), FVector(8.5f, 0.8f, 0.8f), DarkGloss, false,
		nullptr, FRotator(0, -18, 0));
	AddOverlay(
		FVector(374.0f, 24, 59), FVector(8.5f, 0.8f, 0.8f), DarkGloss, false,
		nullptr, FRotator(0, 18, 0));

	// The one inviting warm light in a dead corridor.
	AddOverlay(FVector(380, 34, 68), FVector(12, 12, 28), DarkGloss, false, CylinderMesh);
	UStaticMeshComponent* LampShade = AddOverlay(
		FVector(380, 34, 89), FVector(27, 27, 24),
		FridgeInteriorMaterial, false, ConeMesh);
	if (LampShade)
	{
		// The point light sits inside this solid prototype cone. Letting the
		// cone cast produced a large black triangular pool across the bed.
		LampShade->SetCastShadow(false);
	}
	// Keep the source above the shade's lower rim. A low point source threw
	// metre-long, nearly black shadows from the sleeper and bedside props,
	// which read as broken geometry rather than a dim occupied room.
	MirrorRoomLamp = CreateLight(
		FVector(376, 42, 108), 330.0f, 375.0f,
		FLinearColor(1.0f, 0.54f, 0.23f), true, 24.0f);
	if (MirrorRoomLamp)
	{
		MirrorRoomLamp->SetVisibility(false);
	}

	ActiveParent = nullptr;

	// A scattered line of salt and a small offering at the common entrance.
	// These are deliberately non-colliding so the return path cannot soft-lock.
	for (int32 GrainIndex = 0; GrainIndex < 31; ++GrainIndex)
	{
		const float GrainX = 600.0f + GrainIndex * 2.85f;
		const float GrainY = -399.0f + FMath::Sin(GrainIndex * 1.73f) * 1.25f;
		const float GrainSize = 1.1f + (GrainIndex % 4) * 0.28f;
		UStaticMeshComponent* Grain = AddOverlay(
			FVector(GrainX, GrainY, 0.55f),
			FVector(GrainSize, GrainSize * 0.75f, 0.7f),
			FridgeInteriorMaterial, false, SphereMesh,
			FRotator::ZeroRotator, SceneRoot);
		if (Grain)
		{
			Grain->SetCastShadow(false);
		}
	}
	AddOverlay(
		FVector(700, -412, 4.0f), FVector(20, 20, 6),
		FridgeInteriorMaterial, false,
		CylinderMesh, FRotator::ZeroRotator, SceneRoot);
	AddOverlay(
		FVector(700, -412, 8.2f), FVector(16, 16, 4),
		FridgeInteriorMaterial, false,
		SphereMesh, FRotator::ZeroRotator, SceneRoot);
	AddOverlay(
		FVector(700, -412, 18.0f), FVector(1.2f, 1.2f, 22),
		TexMat(TEXT("M_MetalUV"), MetalFrameMaterial), false,
		CylinderMesh, FRotator::ZeroRotator, SceneRoot);
	AddOverlay(
		FVector(675, -414, 4.5f), FVector(8, 8, 9),
		FridgeInteriorMaterial, false,
		CylinderMesh, FRotator::ZeroRotator, SceneRoot);
	// A very soft practical spill makes the bowl and salt legible from normal
	// eye height without turning the pre-dawn entrance into a spotlight. The
	// recessed lobby fixtures otherwise leave this story beat as an isolated
	// overbright pixel in black asphalt.
	OfferingLight = CreateLight(
		FVector(690, -447, 72), 210.0f, 225.0f,
		FLinearColor(1.0f, 0.72f, 0.46f), false, 24.0f);
	if (OfferingLight)
	{
		OfferingLight->SetVisibility(false);
		OfferingLight->SetSpecularScale(0.45f);
		OfferingLight->SetVolumetricScatteringIntensity(0.04f);
	}

	// Wet steps already inside the lower cab, then continuing through the
	// lobby toward the threshold. They stay separately parked until the 2F
	// doors are about to re-close.
	const FVector FootprintLocations[] = {
		FVector(760, -286, 3), FVector(778, -326, 3),
		FVector(730, -284, 3), FVector(748, -326, 3),
		FVector(690, -286, 3), FVector(708, -326, 3),
		FVector(650, -300, 3), FVector(626, -338, 3),
		FVector(603, -350, 3), FVector(579, -374, 3),
	};
	for (int32 StepIndex = 0; StepIndex < UE_ARRAY_COUNT(FootprintLocations); ++StepIndex)
	{
		UStaticMeshComponent* Step = AddOverlay(
			FVector(
				FootprintLocations[StepIndex].X,
				FootprintLocations[StepIndex].Y,
				0.45f),
			FVector(23, 9.5f, 0.7f),
			TexMat(TEXT("M_WetStep"), ConcreteDarkMaterial),
			false,
			SphereMesh,
			FRotator(0, StepIndex % 2 == 0 ? -12.0f : 12.0f, 0),
			SceneRoot);
		if (Step)
		{
			Step->SetCastShadow(false);
			ElevatorFootprintComponents.Add(Step);
		}
	}
}

void AIGPrologueWorldScene::SuspendCorridorFlicker(const bool bSuspend)
{
	bCorridorFlickerSuspended = bSuspend;
}

void AIGPrologueWorldScene::SetFixtureLive(
	const int32 Index,
	const bool bLive,
	const bool bCorridor)
{
	TArray<TObjectPtr<UPointLightComponent>>& FixtureLights =
		bCorridor ? CorridorLights : LobbyLights;
	TArray<TObjectPtr<UStaticMeshComponent>>& FixtureDiscs =
		bCorridor ? CorridorLightDiscs : LobbyLightDiscs;

	if (!FixtureLights.IsValidIndex(Index) || !FixtureDiscs.IsValidIndex(Index))
	{
		return;
	}

	// A fixture is the light AND the disc: kill both or the ceiling keeps a
	// glowing ring where the lamp used to be.
	if (UPointLightComponent* Light = FixtureLights[Index])
	{
		Light->SetIntensity(bLive ? (bCorridor ? 1200.0f : 1250.0f) : 0.0f);
	}
	if (UStaticMeshComponent* Disc = FixtureDiscs[Index])
	{
		Disc->SetMaterial(0, bLive ? LightPanelMaterial : PlasticDarkMaterial);
	}
}

FVector AIGPrologueWorldScene::GetCorridorFixtureLocation(const int32 Index) const
{
	return CorridorLights.IsValidIndex(Index) && CorridorLights[Index]
		? CorridorLights[Index]->GetComponentLocation()
		: FVector::ZeroVector;
}

void AIGPrologueWorldScene::SetChapterTwoOverlayVisible(const bool bVisible)
{
	for (UStaticMeshComponent* Component : ChapterTwoOverlayComponents)
	{
		if (!Component)
		{
			continue;
		}

		// The wet steps have their own late reveal inside the interrupted lift
		// beat. Everything else is ready when the second morning begins.
		const bool bIsDeferredFootprint = ElevatorFootprintComponents.Contains(Component);
		Component->SetVisibility(bVisible && !bIsDeferredFootprint, true);
		Component->SetCollisionEnabled(
			bVisible && ChapterTwoCollisionComponents.Contains(Component)
				? ECollisionEnabled::QueryAndPhysics
				: ECollisionEnabled::NoCollision);
	}

	if (bVisible)
	{
		for (UStaticMeshComponent* Mask : ChapterOneMaskComponents)
		{
			if (Mask)
			{
				Mask->SetVisibility(false, true);
				Mask->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			}
		}
	}

	if (MirrorRoomLamp)
	{
		MirrorRoomLamp->SetVisibility(bVisible);
		MirrorRoomLamp->SetIntensity(bVisible ? 330.0f : 0.0f);
	}
	if (OfferingLight)
	{
		OfferingLight->SetVisibility(bVisible);
		OfferingLight->SetIntensity(bVisible ? 210.0f : 0.0f);
	}

	auto SetChapterActorVisible = [bVisible](AIGInteractableActor* Actor)
	{
		if (!Actor)
		{
			return;
		}
		Actor->SetActorHiddenInGame(!bVisible);
		Actor->SetActorEnableCollision(bVisible);
		Actor->SetInteractionEnabled(bVisible);
	};

	SetChapterActorVisible(MirrorRoomDoor);
	SetChapterActorVisible(ExistingReceipt);
	SetChapterActorVisible(MailboxBills);
	SetChapterActorVisible(SaltMemo);
	SetChapterActorVisible(NightRoster);

	// The second copy does not exist for the player until they pay again.
	if (DuplicateReceipt)
	{
		DuplicateReceipt->SetActorHiddenInGame(true);
		DuplicateReceipt->SetActorEnableCollision(false);
		DuplicateReceipt->SetInteractionEnabled(false);
	}
}

void AIGPrologueWorldScene::RevealElevatorFootprints()
{
	for (UStaticMeshComponent* Footprint : ElevatorFootprintComponents)
	{
		if (Footprint)
		{
			Footprint->SetVisibility(true, true);
		}
	}
}

void AIGPrologueWorldScene::RevealSecondReceipt()
{
	if (!DuplicateReceipt)
	{
		return;
	}

	DuplicateReceipt->SetActorHiddenInGame(false);
	DuplicateReceipt->SetActorEnableCollision(true);
	DuplicateReceipt->SetInteractionEnabled(true);
}

void AIGPrologueWorldScene::SetChapterTwoReturnZoneArmed(const bool bArmed)
{
	if (ChapterTwoReturnZone)
	{
		ChapterTwoReturnZone->SetActorEnableCollision(bArmed);
	}
}

void AIGPrologueWorldScene::SetStoreNorthLightsLive(const bool bLive)
{
	// BuildStore appends in X-major/Y-minor order. Indices 0 and 2 are the
	// north row (Y=-300); the fifth StoreLights entry belongs to the cooler.
	for (const int32 LightIndex : {0, 2})
	{
		if (StoreLights.IsValidIndex(LightIndex) && StoreLights[LightIndex])
		{
			StoreLights[LightIndex]->SetIntensity(bLive ? 2850.0f : 0.0f);
		}
		if (StoreLightDiscs.IsValidIndex(LightIndex) && StoreLightDiscs[LightIndex])
		{
			StoreLightDiscs[LightIndex]->SetMaterial(
				0, bLive ? LightPanelMaterial : PlasticDarkMaterial);
		}
	}
}

void AIGPrologueWorldScene::FinishChapterTwo()
{
	if (bChapterTwoFinished)
	{
		return;
	}
	bChapterTwoFinished = true;

	for (int32 FixtureIndex = 0; FixtureIndex < GetCorridorFixtureCount(); ++FixtureIndex)
	{
		SetFixtureLive(FixtureIndex, true, true);
	}

	AIGHorrorHUD::PushThought(
		this,
		NSLOCTEXT("IGCH02", "ReturnedLightsThought", "…다 켜져 있네."),
		3.6f);

	GetWorldTimerManager().SetTimer(
		ChapterEndingHandle,
		[this]()
		{
			if (!IsValid(this))
			{
				return;
			}

			DistantAlarmComponent = CreateAmbientBed(
				AlarmSound,
				IGPrologueWorld::AlarmLocation,
				0.12f,
				90.0f,
				1750.0f);
			if (DistantAlarmComponent)
			{
				DistantAlarmComponent->SetLowPassFilterEnabled(true);
				DistantAlarmComponent->SetLowPassFilterFrequency(800.0f);
			}

			AIGHorrorHUD::PushThought(
				this,
				NSLOCTEXT("IGCH02", "DistantAlarmThought", "저건… 내 알람이잖아."),
				4.2f);

			if (APlayerController* PlayerController =
				GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr)
			{
				if (APlayerCameraManager* CameraManager = PlayerController->PlayerCameraManager)
				{
					CameraManager->StartCameraFade(
						0.0f, 1.0f, 2.2f, FLinearColor::Black, false, true);
				}
			}

			GetWorldTimerManager().SetTimer(
				ChapterEndingHandle,
				[this]()
				{
					if (!IsValid(this))
					{
						return;
					}
					AIGHorrorHUD::ShowChapterCard(
						this,
						NSLOCTEXT("IGCH02", "ChapterThreeEyebrow", "CHAPTER 03"),
						NSLOCTEXT("IGCH02", "ChapterThreeTitle", "세 번째 아침"),
						NSLOCTEXT("IGCH02", "ChapterThreeSubtitle", "물이 온다"),
						4.8f);

					// The alarm survives onto the card for two more seconds.
					GetWorldTimerManager().SetTimer(
						ChapterEndingHandle,
						[this]()
						{
							if (DistantAlarmComponent)
							{
								DistantAlarmComponent->Stop();
							}
						},
						2.0f,
						false);
				},
				2.2f,
				false);
		},
		1.0f,
		false);
}

void AIGPrologueWorldScene::HandleCorridorFlicker()
{
	// A chapter that owns the corridor lighting suspends this. The timer runs
	// for the whole session at 10 Hz and writes the intensity unconditionally,
	// so without the gate anything else done to the west fixture is undone
	// within a tenth of a second.
	if (bCorridorFlickerSuspended || !DegradedCorridorLight)
	{
		return;
	}

	uint32 Hash = ++FlickerHashCounter * 2654435761u;
	Hash ^= Hash >> 15;
	const float Uniform = (Hash & 0xFFFF) / 65535.0f;
	const float DropoutRoll = ((Hash >> 16) & 0xFFFF) / 65535.0f;
	const float Multiplier =
		DropoutRoll < 0.05f ? 0.12f : (0.86f + 0.20f * Uniform);
	DegradedCorridorLight->SetIntensity(DegradedLightBaseIntensity * Multiplier);
}

void AIGPrologueWorldScene::BuildLobby()
{
	// Ground-floor lobby: elevator on the east wall, mailboxes on the north,
	// and the framed-glass common entrance opening south onto the porch.
	UMaterialInterface* LobbyFloor = TexMat(TEXT("M_GraniteTile_XY"), StoreFloorMaterial);
	UMaterialInterface* LobbyWallX = TexMat(TEXT("M_Stucco_X"), ConcreteMaterial);
	UMaterialInterface* LobbyWallY = TexMat(TEXT("M_Stucco_Y"), ConcreteMaterial);
	UMaterialInterface* LobbyCeil = TexMat(TEXT("M_StuccoCeil"), ConcreteMaterial);
	UMaterialInterface* Skirting = TexMat(TEXT("M_ConcreteDark_X"), ConcreteDarkMaterial);
	UMaterialInterface* Stainless = TexMat(TEXT("M_StainlessUV"), MetalFrameMaterial);
	UMaterialInterface* Metal = TexMat(TEXT("M_MetalUV"), MetalFrameMaterial);
	UMaterialInterface* ShelfSteel = TexMat(TEXT("M_ShelfSteelUV"), CoolerBodyMaterial);

	// Interior X 450..710, Y -375..-235, height 240 — directly under the lift.
	CreateBlock(FVector(580, -305, -10), FVector(300, 180, 20), LobbyFloor);
	CreateBlock(FVector(580, -305, 250), FVector(300, 180, 20), LobbyCeil);
	CreateBlock(FVector(580, -225, 120), FVector(300, 20, 240), LobbyWallX);
	CreateBlock(FVector(440, -305, 120), FVector(20, 160, 240), LobbyWallY);
	// Granite skirting round the lobby, matching the landings upstairs.
	CreateBlock(FVector(580, -233.4f, 6), FVector(300, 3.5f, 12), Skirting, false);
	CreateBlock(FVector(448.4f, -305, 6), FVector(3.5f, 160, 12), Skirting, false);

	// Street wall of the lobby, with the common-entrance opening at X 600..686.
	// West of X 450 the ground floor is the open pilotis car park, which
	// BuildAlley puts in — Korean villas give the whole ground level to
	// parking and recess the entrance into it.
	CreateBlock(FVector(525, -385, 120), FVector(150, 20, 240), LobbyWallX);
	CreateBlock(FVector(703, -385, 120), FVector(34, 20, 240), LobbyWallX);
	CreateBlock(FVector(643, -385, 225), FVector(86, 20, 30), LobbyWallX);

	// East wall around the elevator opening (Y -360..-250).
	CreateBlock(FVector(710, -242.5f, 120), FVector(20, 15, 240), LobbyWallY);
	CreateBlock(FVector(710, -367.5f, 120), FVector(20, 15, 240), LobbyWallY);
	CreateBlock(FVector(710, -305, 225), FVector(20, 110, 30), LobbyWallY);

	// Mailboxes for the whole building on the north wall.
	CreateBlock(FVector(528, -237, 151), FVector(96, 4, 66), ShelfSteel, false);
	for (const float BoxZ : {131.0f, 151.0f, 171.0f})
	{
		for (const float BoxX : {500.0f, 528.0f, 556.0f})
		{
			CreateBlock(FVector(BoxX, -241.5f, BoxZ), FVector(26, 9, 18), Metal, false);
			CreateBlock(
				FVector(BoxX, -246.3f, BoxZ + 4), FVector(18, 1.2f, 2.5f),
				PlasticDarkMaterial, false);
			// Cam-lock keyhole.
			CreateBlock(
				FVector(BoxX + 8, -246.5f, BoxZ - 4), FVector(2.4f, 1.2f, 2.4f),
				PlasticDarkMaterial, false, CylinderMesh, FRotator(90, 0, 0));
		}
	}

	// Lobby fittings: the video intercom by the door, a notice board over the
	// mailboxes, and the umbrella stand nobody has emptied since the rains.
	CreateBlock(
		FVector(672, -381, 145), FVector(16, 5, 22),
		TexMat(TEXT("M_Intercom"), SignWhiteMaterial), false);
	CreateBlock(FVector(672, -383.5f, 145), FVector(19, 3, 25), Stainless, false);
	CreateBlock(
		FVector(600, -239.5f, 196), FVector(84, 3, 44),
		TexMat(TEXT("M_NoticeA4"), SignWhiteMaterial), false);
	CreateBlock(FVector(600, -237, 196), FVector(90, 3, 50), Metal, false);
	CreateBlock(FVector(468, -252, 24), FVector(26, 26, 48), Metal, true, CylinderMesh);

	// Lobby fluorescents: one over the mailboxes, one at the lift doors.
	for (const float FixtureX : {510.0f, 660.0f})
	{
		CreateBlock(
			FVector(FixtureX, -305, 237), FVector(28, 28, 4),
			Stainless, false, CylinderMesh);
		LobbyLightDiscs.Add(CreateBlock(
			FVector(FixtureX, -305, 234.5f), FVector(23, 23, 2),
			LightPanelMaterial, false, CylinderMesh));
		UPointLightComponent* LobbyLight = CreateLight(
			FVector(FixtureX, -305, 226), 1250.0f, 520.0f,
			FLinearColor(0.87f, 0.98f, 1.0f), true, 16.0f);
		LobbyLight->SetVolumetricScatteringIntensity(0.16f);
		LobbyLights.Add(LobbyLight);
	}
}

void AIGPrologueWorldScene::BuildAlley()
{
	UMaterialInterface* AsphaltWorld = TexMat(TEXT("M_AsphaltWorld"), AsphaltMaterial);
	UMaterialInterface* BrickX = TexMat(TEXT("M_Brick_X"), ConcreteMaterial);
	UMaterialInterface* DarkX = TexMat(TEXT("M_ConcreteDark_X"), ConcreteDarkMaterial);
	UMaterialInterface* DarkY = TexMat(TEXT("M_ConcreteDark_Y"), ConcreteDarkMaterial);
	UMaterialInterface* Metal = TexMat(TEXT("M_MetalUV"), MetalFrameMaterial);

	// Asphalt strip from the west dead end to the store front.
	CreateBlock(FVector(1040, -537.5f, -10), FVector(2720, 305, 20), AsphaltWorld);

	// The shop is deeper than the alley is wide, so the ground in front of its
	// northern half was missing entirely — from inside, looking out through
	// the glass showed a hole. Pave that corner and close it with a wall.
	CreateBlock(FVector(2300, -425, -10), FVector(220, 550, 20), AsphaltWorld);
	CreateBlock(FVector(2190, -270, 230), FVector(20, 240, 460), BrickX);
	CreateBlock(FVector(2300, -152, 230), FVector(240, 20, 460), BrickX);

	// Curb stones seat the facades onto the road.
	CreateBlock(FVector(1040, -410, 4), FVector(2720, 16, 12), TexMat(TEXT("M_Concrete_X"), ConcreteMaterial));
	CreateBlock(FVector(1040, -665, 4), FVector(2720, 16, 12), TexMat(TEXT("M_Concrete_X"), ConcreteMaterial));

	// Manhole covers and the drainage channel running down the alley center.
	// Keep the first cover clear of the villa threshold and CH02 offering.
	// The old 72 cm cover sat less than a metre from the bowl and dominated
	// the approach at ordinary first-person eye height.
	CreateBlock(FVector(430, -525, 1.5f), FVector(64, 64, 3), Metal, true, CylinderMesh);
	CreateBlock(FVector(1520, -560, 1.5f), FVector(72, 72, 3), Metal, true, CylinderMesh);
	CreateBlock(FVector(1040, -537, 0.8f), FVector(2720, 26, 2), Metal, false);

	// North side: the villa rises four storeys in granite cladding panels —
	// the grey speckled stone every newer Korean walk-up is faced with, not
	// brick. Band between the pilotis (0..240) and the 4F corridor wall
	// (900..1140), then a parapet above.
	UMaterialInterface* GranitePanelX = TexMat(TEXT("M_GranitePanel_X"), ConcreteMaterial);
	CreateBlock(FVector(190, -385, 570), FVector(1060, 20, 660), GranitePanelX);
	CreateBlock(FVector(190, -385, 1190), FVector(1060, 20, 100), GranitePanelX);
	// Panel joints: the recessed grid lines that make cladding read as stone
	// panels rather than a painted slab.
	for (float JointZ = 330.0f; JointZ < 1240.0f; JointZ += 95.0f)
	{
		CreateBlock(
			FVector(190, -395.6f, JointZ), FVector(1060, 2.4f, 1.3f),
			DarkX, false);
	}
	for (float JointX = -245.0f; JointX <= 665.0f; JointX += 190.0f)
	{
		CreateBlock(
			FVector(JointX, -395.6f, 700), FVector(1.3f, 2.4f, 1000),
			DarkX, false);
	}
	// Coping band at the parapet and a black roof railing above it.
	CreateBlock(
		FVector(190, -397, 1244), FVector(1068, 12, 10),
		TexMat(TEXT("M_Concrete_X"), ConcreteMaterial), false);
	CreateBlock(FVector(190, -392, 1296), FVector(1060, 4, 4), PlasticDarkMaterial, false);
	CreateBlock(FVector(190, -392, 1272), FVector(1060, 3, 3), PlasticDarkMaterial, false);
	for (float PostX = -320.0f; PostX <= 700.0f; PostX += 34.0f)
	{
		CreateBlock(FVector(PostX, -392, 1274), FVector(2.4f, 2.4f, 56), PlasticDarkMaterial, false);
	}

	// --- Ground-floor pilotis car park (X -340..440) ------------------------
	// The whole ground level is open parking on columns; the lobby entrance
	// sits recessed at its east end. This is what makes the building read as
	// a Korean villa from the alley instead of a wall with a door in it.
	{
		UMaterialInterface* ParkFloor = TexMat(TEXT("M_Concrete_XY"), ConcreteMaterial);
		CreateBlock(FVector(50, -310, -10), FVector(780, 170, 20), ParkFloor);
		CreateBlock(FVector(50, -310, 244), FVector(780, 170, 12), DarkX, false);
		CreateBlock(FVector(50, -232, 120), FVector(780, 16, 240), DarkX);
		CreateBlock(FVector(-348, -310, 120), FVector(16, 170, 240), DarkY);
		// Columns on the street line, each with a concrete capital.
		for (const float ColumnX : {-300.0f, -140.0f, 20.0f, 180.0f, 340.0f})
		{
			CreateBlock(FVector(ColumnX, -378, 118), FVector(38, 38, 236), GranitePanelX);
			CreateBlock(FVector(ColumnX, -378, 232), FVector(46, 46, 12), DarkX, false);
			CreateBlock(FVector(ColumnX, -378, 8), FVector(46, 46, 16), DarkX, false);
		}
		// Painted bay lines and the chain barrier slung between the columns.
		for (const float LineX : {-220.0f, -60.0f, 100.0f, 260.0f})
		{
			CreateBlock(FVector(LineX, -310, 0.6f), FVector(6, 160, 1.4f), SignWhiteMaterial, false);
		}
		for (const float SpanX : {-220.0f, -60.0f, 100.0f, 260.0f})
		{
			CreateBlock(
				FVector(SpanX, -378, 62), FVector(2.6f, 2.6f, 150),
				PlasticDarkMaterial, false, CylinderMesh, FRotator(0, 0, 90));
		}
		// Stair-core door at the back of the bay and a wall-mounted hose reel.
		CreateBlock(FVector(-120, -241, 100), FVector(88, 6, 200), DarkX, false);
		CreateBlock(FVector(-84, -244.5f, 96), FVector(4, 2, 14), Metal, false);
		CreateBlock(
			FVector(210, -240, 130), FVector(34, 12, 40),
			TexMat(TEXT("M_FireBox"), SnackRedMaterial), false);
		// A single sodium bulkhead keeps the bay from being a black hole.
		CreateBlock(FVector(-30, -244, 214), FVector(22, 14, 12), Metal, false);
		UPointLightComponent* PilotisLamp = CreateLight(
			FVector(-30, -252, 208), 300.0f, 480.0f,
			FLinearColor(0.98f, 0.78f, 0.48f), true, 12.0f);
		PilotisLamp->SetVolumetricScatteringIntensity(0.65f);
		// Rubbish bags and a bicycle nobody has moved in months.
		PlacePhotoProp(TEXT("trashbag"), FVector(-286, -262, 0), FVector(58, 58, 56), 20.0f);
		PlacePhotoProp(TEXT("cardboard_box_01"), FVector(-244, -256, 0), FVector(46, 38, 32), -35.0f);
	}
	// The north facade is split by a service gap, so the alley reads as a
	// junction rather than one long tube. The neighbouring block starts where
	// the villa ends.
	CreateBlock(FVector(935, -385, 230), FVector(430, 20, 460), BrickX);
	CreateBlock(FVector(1845, -385, 230), FVector(1110, 20, 460), BrickX);

	// Side alley running north between the two buildings: unlit, dead-ended,
	// and just wide enough to notice on the way past.
	{
		const float GapCenterX = 1220.0f;
		CreateBlock(FVector(GapCenterX, -250, -10), FVector(140, 290, 20), AsphaltWorld);
		CreateBlock(FVector(GapCenterX - 78, -250, 230), FVector(16, 290, 460), DarkY);
		CreateBlock(FVector(GapCenterX + 78, -250, 230), FVector(16, 290, 460), DarkY);
		CreateBlock(FVector(GapCenterX, -110, 230), FVector(140, 20, 460), DarkX);
		// Drain channel, gas meter cluster and a dead wall lamp.
		CreateBlock(FVector(GapCenterX, -250, 0.8f), FVector(24, 280, 2), Metal, false);
		for (const float MeterZ : {96.0f, 140.0f})
		{
			CreateBlock(FVector(GapCenterX - 62, -200, MeterZ), FVector(26, 30, 34),
				FridgeBodyMaterial, false);
			CreateBlock(FVector(GapCenterX - 62, -200, MeterZ + 20), FVector(8, 8, 12),
				Metal, false, CylinderMesh);
		}
		CreateBlock(FVector(GapCenterX + 62, -168, 214), FVector(20, 26, 12),
			PlasticDarkMaterial, false);
		// Rubbish nobody has collected.
		PlacePhotoProp(TEXT("trashbag"), FVector(GapCenterX - 34, -152, 0),
			FVector(58, 58, 56), 40.0f);
		PlacePhotoProp(TEXT("cardboard_box_01"), FVector(GapCenterX + 30, -172, 0),
			FVector(46, 38, 32), -25.0f);
		CreateBlock(FVector(GapCenterX + 40, -300, 24), FVector(30, 30, 48),
			TexMat(TEXT("M_ConeOrange"), SnackRedMaterial), true, ConeMesh);
		// One failing lamp deep inside: barely enough to show it dead-ends.
		UPointLightComponent* SideLamp = CreateLight(
			FVector(GapCenterX + 50, -172, 206), 150.0f, 300.0f,
			FLinearColor(0.95f, 0.72f, 0.45f), true, 9.0f);
		SideLamp->SetVolumetricScatteringIntensity(0.70f);
	}

	// Windows for floors two to four. Korean villa windows are tall sashes in
	// dark aluminium with a black railing across the lower half and a stone
	// sill under them — that combination is most of what makes the facade
	// read. One flat on the third floor is faintly awake.
	for (const float WindowZ : {390.0f, 690.0f, 990.0f})
	{
		for (const float WindowX : {-260.0f, -100.0f, 60.0f, 220.0f, 380.0f})
		{
			UMaterialInterface* Pane =
				(FMath::IsNearlyEqual(WindowX, 60.0f) && FMath::IsNearlyEqual(WindowZ, 690.0f))
					? WindowGlowMaterial
					: WindowDarkMaterial;
			CreateBlock(FVector(WindowX, -396, WindowZ), FVector(96, 4, 116), Pane, false);
			// Aluminium frame: head, cill, two jambs and the sliding mullion.
			CreateBlock(FVector(WindowX, -397.5f, WindowZ + 60), FVector(104, 5, 6), PlasticDarkMaterial, false);
			CreateBlock(FVector(WindowX, -397.5f, WindowZ - 60), FVector(104, 5, 6), PlasticDarkMaterial, false);
			CreateBlock(FVector(WindowX - 50, -397.5f, WindowZ), FVector(6, 5, 116), PlasticDarkMaterial, false);
			CreateBlock(FVector(WindowX + 50, -397.5f, WindowZ), FVector(6, 5, 116), PlasticDarkMaterial, false);
			CreateBlock(FVector(WindowX, -397.5f, WindowZ), FVector(4, 5, 112), PlasticDarkMaterial, false);
			// Stone sill with a drip edge.
			CreateBlock(
				FVector(WindowX, -400, WindowZ - 66), FVector(112, 12, 7),
				TexMat(TEXT("M_Concrete_X"), ConcreteMaterial), false);
			// Black railing across the lower half.
			CreateBlock(FVector(WindowX, -401, WindowZ - 6), FVector(108, 3.5f, 3.5f), PlasticDarkMaterial, false);
			CreateBlock(FVector(WindowX, -401, WindowZ - 34), FVector(108, 3, 3), PlasticDarkMaterial, false);
			for (int32 BarIndex = -4; BarIndex <= 4; ++BarIndex)
			{
				CreateBlock(
					FVector(WindowX + BarIndex * 12.0f, -401, WindowZ - 34),
					FVector(2.2f, 2.2f, 60), PlasticDarkMaterial, false);
			}
		}
	}

	// Rain downspouts pin the facade to the ground.
	for (const float PipeX : {-320.0f, 255.0f})
	{
		CreateBlock(
			FVector(PipeX, -400, 620), FVector(11, 11, 1240),
			DarkY, false, CylinderMesh);
	}

	// Common entrance dressing: canopy, name plate, keypad, threshold.
	CreateBlock(FVector(643, -405, 240), FVector(104, 44, 6), DarkX, false);
	CreateBlock(
		FVector(643, -396.5f, 258), FVector(80, 3, 24),
		TexMat(TEXT("M_SignVilla"), SignWhiteMaterial), false);
	CreateBlock(FVector(692, -394, 115), FVector(10, 4, 16), PlasticDarkMaterial, false);
	CreateBlock(FVector(695, -395.4f, 118), FVector(3, 1.2f, 3),
		TexMat(TEXT("M_ScreenGlow"), ScreenGlowMaterial), false);
	CreateBlock(FVector(643, -402, 4), FVector(94, 30, 8), TexMat(TEXT("M_Concrete_XY"), ConcreteMaterial));

	// Wall-mounted AC condenser units.
	for (const FVector& UnitCenter : {FVector(310, -403, 320), FVector(1100, -403, 142)})
	{
		CreateBlock(UnitCenter, FVector(56, 26, 52), FridgeBodyMaterial, false);
		CreateBlock(
			UnitCenter + FVector(0, -14.5f, 0), FVector(34, 3, 34),
			PlasticDarkMaterial, false, CylinderMesh, FRotator(90, 0, 0));
		for (int32 GrillIndex = -1; GrillIndex <= 1; ++GrillIndex)
		{
			CreateBlock(
				UnitCenter + FVector(0, -15.5f, GrillIndex * 10.0f),
				FVector(44, 1.5f, 2), FridgeBodyMaterial, false);
		}
	}

	// Rental flyers taped to the brick, and the landlord's banner overhead.
	CreateBlock(FVector(800, -396.5f, 170), FVector(58, 2, 78), TexMat(TEXT("M_PosterFlyer"), ConcreteMaterial), false);
	CreateBlock(FVector(1905, -396.5f, 168), FVector(58, 2, 78), TexMat(TEXT("M_PosterFlyer"), ConcreteMaterial), false);
	CreateBlock(
		FVector(90, -397.5f, 480), FVector(250, 2.5f, 32),
		TexMat(TEXT("M_Banner"), SignWhiteMaterial), false);


	// A red church cross on a far rooftop keeps watch over the district.
	CreateBlock(FVector(1750, -700, 545), FVector(10, 8, 96), AlarmMaterial, false);
	CreateBlock(FVector(1750, -700, 566), FVector(58, 8, 10), AlarmMaterial, false);

	// Parking cones and the AC drain pipes running to the street.
	CreateBlock(
		FVector(2280, -640, 24), FVector(30, 30, 48),
		TexMat(TEXT("M_ConeOrange"), SnackRedMaterial), true, ConeMesh);
	CreateBlock(
		FVector(1180, -430, 24), FVector(30, 30, 48),
		TexMat(TEXT("M_ConeOrange"), SnackRedMaterial), true, ConeMesh);
	for (const float PipeX : {350.0f, 1100.0f})
	{
		CreateBlock(
			FVector(PipeX + 34, -399, 70), FVector(6, 6, 140),
			DarkY, false, CylinderMesh);
	}

	// Dark upstairs windows; nobody is awake at this hour.
	const float NorthWindowXs[] = {300, 700, 1200, 1700, 2100};
	for (int32 WindowIndex = 0; WindowIndex < static_cast<int32>(UE_ARRAY_COUNT(NorthWindowXs)); ++WindowIndex)
	{
		const float OffsetZ = (WindowIndex % 2 == 0) ? 300.0f : 310.0f;
		CreateBlock(
			FVector(NorthWindowXs[WindowIndex], -236, OffsetZ),
			FVector(90, 4, 110),
			WindowDarkMaterial,
			false);
	}
	CreateBlock(FVector(0, -236, 345), FVector(120, 4, 100), WindowDarkMaterial, false);

	// South side: opposing building lined with shuttered shops — the
	// text-dense storefront wall that makes it read as a Korean back street.
	// Masonry, not flat render: brick above a dark painted plinth.
	CreateBlock(FVector(1040, -690, 310), FVector(2720, 20, 380), BrickX);
	CreateBlock(FVector(1040, -688, 60), FVector(2720, 22, 120), DarkX);
	CreateBlock(FVector(1040, -687, 122), FVector(2720, 24, 8),
		TexMat(TEXT("M_Concrete_X"), ConcreteMaterial), false);
	{
		// Four individualized storefronts: varied widths, awnings, blade
		// signs, a display window, an A-frame board, gas bottles, planters —
		// and second-storey PC-bang/noraebang signs, one of them still lit.
		UMaterialInterface* Shutter = TexMat(TEXT("M_Shutter_X"), PlasticDarkMaterial);
		struct FShopSpec
		{
			float X;
			float Width;
			const TCHAR* Sign;
			UMaterialInterface* Awning;
		};
		const FShopSpec Shops[] = {
			{400.0f, 150.0f, TEXT("M_SignLaundry"), SnackBlueMaterial},
			{900.0f, 128.0f, TEXT("M_SignHair"), SnackRedMaterial},
			{1500.0f, 172.0f, TEXT("M_SignHof"), SnackRedMaterial},
			{2050.0f, 150.0f, TEXT("M_SignSuper"), BottleGreenMaterial},
		};
		for (int32 ShopIndex = 0; ShopIndex < 4; ++ShopIndex)
		{
			const FShopSpec& Shop = Shops[ShopIndex];
			const float HalfWidth = Shop.Width * 0.5f;
			// Frame jambs and the fascia sign with its phone-number strip.
			CreateBlock(FVector(Shop.X - HalfWidth - 6, -677, 100), FVector(12, 10, 200), DarkX, false);
			CreateBlock(FVector(Shop.X + HalfWidth + 6, -677, 100), FVector(12, 10, 200), DarkX, false);
			CreateBlock(
				FVector(Shop.X, -672, 222), FVector(Shop.Width + 24, 16, 42),
				TexMat(Shop.Sign, PlasticDarkMaterial), false);
			CreateBlock(FVector(Shop.X, -670, 197), FVector(Shop.Width - 30, 2, 9),
				FridgeInteriorMaterial, false);
			// Striped awning over the entrance.
			CreateBlock(
				FVector(Shop.X, -662, 206), FVector(Shop.Width + 16, 42, 3.5f),
				Shop.Awning, false, nullptr, FRotator(-21, 0, 0));
			// Blade sign hung off the west jamb.
			CreateBlock(
				FVector(Shop.X - HalfWidth - 10, -664, 258), FVector(10, 24, 78),
				Shop.Awning, false);

			if (ShopIndex == 1)
			{
				// The hair salon keeps a dark display window instead of a shutter.
				CreateBlock(FVector(Shop.X, -676, 96), FVector(Shop.Width, 5, 192), GlassMaterial);
				CreateBlock(FVector(Shop.X, -683, 96), FVector(Shop.Width, 4, 192), PlasticDarkMaterial, false);
				CreateBlock(FVector(Shop.X, -679, 60), FVector(Shop.Width - 24, 6, 5), DarkX, false);
			}
			else
			{
				CreateBlock(FVector(Shop.X, -676, 96), FVector(Shop.Width, 8, 192), Shutter);
			}
		}
		// A-frame board in front of the hof, gas bottles and foam boxes for
		// the super, planters by the laundry.
		CreateBlock(FVector(1445, -648, 38), FVector(42, 3, 74), SnackYellowMaterial, false,
			nullptr, FRotator(-12, 0, 0));
		CreateBlock(FVector(1445, -640, 38), FVector(42, 3, 74), SnackYellowMaterial, false,
			nullptr, FRotator(12, 0, 0));
		CreateBlock(FVector(2110, -655, 40), FVector(26, 26, 80), Metal, true, CylinderMesh);
		CreateBlock(FVector(2136, -652, 40), FVector(26, 26, 80), Metal, true, CylinderMesh);
		CreateBlock(FVector(1985, -650, 12), FVector(40, 30, 20), FridgeInteriorMaterial);
		CreateBlock(FVector(1985, -650, 32), FVector(38, 28, 18), FridgeInteriorMaterial);
		CreateBlock(FVector(330, -652, 14), FVector(24, 24, 26), CoolerBodyMaterial, true, CylinderMesh);
		CreateBlock(FVector(330, -652, 44), FVector(34, 34, 30), BottleGreenMaterial, false, SphereMesh);
		CreateBlock(FVector(362, -655, 14), FVector(24, 24, 26), CoolerBodyMaterial, true, CylinderMesh);
		CreateBlock(FVector(362, -655, 44), FVector(34, 34, 30), BottleGreenMaterial, false, SphereMesh);
		// Upstairs: PC-bang sign dark, noraebang sign still glowing pink.
		CreateBlock(
			FVector(900, -672, 330), FVector(170, 14, 40),
			TexMat(TEXT("M_SignPC"), PlasticDarkMaterial), false);
		CreateBlock(
			FVector(1500, -672, 330), FVector(190, 14, 40),
			TexMat(TEXT("M_SignKaraoke"), PlasticDarkMaterial), false);
	}
	const float SouthWindowXs[] = {250, 650, 1150, 1750, 2150};
	for (int32 WindowIndex = 0; WindowIndex < static_cast<int32>(UE_ARRAY_COUNT(SouthWindowXs)); ++WindowIndex)
	{
		const float OffsetZ = (WindowIndex % 2 == 0) ? 320.0f : 330.0f;
		CreateBlock(
			FVector(SouthWindowXs[WindowIndex], -676, OffsetZ),
			FVector(80, 4, 90),
			WindowDarkMaterial,
			false);
	}

	// West dead end.
	CreateBlock(FVector(-330, -537.5f, 250), FVector(20, 305, 500), DarkY);

	// Streetlights; the middle one is wired to fail as the player passes.
	// A scanned lamp post stands in when the import exists.
	const float StreetlightXs[] = {150, 1000, 1850};
	for (const float PoleX : StreetlightXs)
	{
		// Concrete anchor plinth under every mast.
		CreateBlock(
			FVector(PoleX, -640, 14), FVector(34, 34, 28),
			TexMat(TEXT("M_Concrete_XY"), ConcreteMaterial), true, CylinderMesh);
		if (!PlacePhotoProp(
			TEXT("street_lamp_01"), FVector(PoleX, -640, 0), FVector(120, 120, 400), 90.0f))
		{
			CreateBlock(
				FVector(PoleX, -650, 190), FVector(12, 12, 380),
				DarkY, true, CylinderMesh);
			CreateBlock(FVector(PoleX, -605, 368), FVector(8, 90, 8), PlasticDarkMaterial, false);
			if (!CreateProp(
				TEXT("SM_LampShade"), FVector(PoleX, -565, 356), PlasticDarkMaterial))
			{
				CreateBlock(
					FVector(PoleX, -565, 366), FVector(46, 46, 26),
					PlasticDarkMaterial, false, ConeMesh, FRotator(180, 0, 0));
			}
			CreateBlock(
				FVector(PoleX, -565, 352), FVector(14, 14, 14),
				StreetLampGlowMaterial, false, SphereMesh);
		}
		UPointLightComponent* LampLight = CreateLight(
			FVector(PoleX, -600, 346),
			3200.0f,
			860.0f,
			FLinearColor(1.0f, 0.72f, 0.42f),
			true,
			18.0f);
		LampLight->SetVolumetricScatteringIntensity(0.9f);
		if (FMath::IsNearlyEqual(PoleX, 1000.0f))
		{
			FlickerStreetlight = LampLight;
		}
	}

	// Utility poles with junction boxes for the Korean-alley silhouette.
	for (const float PoleX : {600.0f, 1600.0f})
	{
		CreateBlock(
			FVector(PoleX, -422, 225), FVector(14, 14, 450),
			DarkY, true, CylinderMesh);
		if (!PlacePhotoProp(
			TEXT("utility_box_01"), FVector(PoleX, -428, 330), FVector(52, 42, 66), 90.0f, false))
		{
			CreateBlock(FVector(PoleX, -422, 390), FVector(45, 35, 60), DarkX, false);
		}
	}

	// (Moonlight now comes from the physically based directional moon.)

	// Trash and boxes: scanned bags/cartons where imported, plus a couple of
	// physics cubes that can still be kicked down the alley.
	if (!PlacePhotoProp(TEXT("trashbag"), FVector(-260, -520, 0), FVector(60, 60, 58), 20.0f))
	{
		CreatePhysicsProp(
			SphereMesh, TrashBagMaterial,
			FVector(0.42f, 0.42f, 0.33f), FVector(-260, -520, 17), FRotator::ZeroRotator, 1.2f);
	}
	PlacePhotoProp(TEXT("trashbag"), FVector(-232, -556, 0), FVector(54, 54, 52), 140.0f);
	PlacePhotoProp(TEXT("trashbag"), FVector(2282, -612, 0), FVector(62, 62, 60), 260.0f);
	if (!PlacePhotoProp(
		TEXT("cardboard_box_01"), FVector(2320, -585, 0), FVector(48, 40, 34), 20.0f))
	{
		CreatePhysicsProp(
			CubeMesh, CardboardMaterial,
			FVector(0.40f, 0.32f, 0.26f), FVector(2320, -585, 14), FRotator(0, 20, 0), 0.8f);
	}
	PlacePhotoProp(TEXT("cardboard_box_01"), FVector(1180, -645, 0), FVector(44, 36, 30), -35.0f);
	CreatePhysicsProp(
		CubeMesh, CardboardMaterial,
		FVector(0.36f, 0.30f, 0.24f), FVector(2255, -640, 13), FRotator(0, 65, 0), 0.7f);
}

void AIGPrologueWorldScene::BuildStore()
{
	UMaterialInterface* Tile = TexMat(TEXT("M_StoreTileWorld"), StoreFloorMaterial);
	UMaterialInterface* StoreWallX = TexMat(TEXT("M_StoreWall_X"), ConcreteMaterial);
	UMaterialInterface* StoreWallY = TexMat(TEXT("M_StoreWall_Y"), ConcreteMaterial);
	UMaterialInterface* StoreCeil = TexMat(TEXT("M_StoreCeilWorld"), ConcreteMaterial);
	UMaterialInterface* ShelfSteel = TexMat(TEXT("M_ShelfSteelUV"), CoolerBodyMaterial);
	UMaterialInterface* Metal = TexMat(TEXT("M_MetalUV"), MetalFrameMaterial);

	// Shell: a real convenience-store footprint (540 x 500 interior).
	CreateBlock(FVector(2680, -430, 2), FVector(544, 504, 8), Tile);
	CreateBlock(FVector(2680, -170, 130), FVector(560, 20, 260), StoreWallX);
	CreateBlock(FVector(2680, -690, 130), FVector(560, 20, 260), StoreWallX);
	CreateBlock(FVector(2960, -430, 130), FVector(20, 540, 260), StoreWallY);
	CreateBlock(FVector(2680, -430, 270), FVector(580, 540, 20), StoreCeil);

	// Storefront on the alley: glass the full width so the lit interior — the
	// counter, the aisles, the cooler glow — is visible from the street, with
	// a short masonry pier at the north corner.
	CreateBlock(FVector(2405, -196, 130), FVector(10, 32, 260), StoreWallX);
	CreateBlock(FVector(2405, -300, 106), FVector(8, 175, 200), GlassMaterial);
	CreateBlock(FVector(2405, -592.5f, 106), FVector(8, 151, 200), GlassMaterial);
	for (const float ColumnY : {-212.0f, -300.0f, -395.0f, -517.0f, -668.0f})
	{
		CreateBlock(FVector(2405, ColumnY, 105), FVector(14, 14, 210), Metal);
	}
	// Kick rail and head transom across the whole shopfront.
	CreateBlock(FVector(2405, -430, 12), FVector(12, 470, 24), Metal);
	CreateBlock(FVector(2405, -531.5f, 216), FVector(14, 297, 22), Metal);
	CreateBlock(FVector(2405, -531.5f, 245), FVector(10, 297, 36), StoreWallX);

	// Signage: the lettered fascia glows down the whole alley, plus a blade sign.
	CreateBlock(
		FVector(2399, -520, 262), FVector(12, 320, 72),
		TexMat(TEXT("M_SignMainLit"), SignMintMaterial), false);
	CreateBlock(
		FVector(2402, -404, 300), FVector(28, 10, 88),
		TexMat(TEXT("M_SignBladeLit"), SignWhiteMaterial), false);

	// Storefront paper: sale poster and the automatic-door sticker.
	CreateBlock(
		FVector(2400.2f, -560, 130), FVector(1.5f, 58, 80),
		TexMat(TEXT("M_PosterSale"), SignWhiteMaterial), false);
	CreateBlock(
		FVector(2400.2f, -628, 118), FVector(1.5f, 42, 21),
		TexMat(TEXT("M_SignAutoDoor"), SignWhiteMaterial), false);

	// Window-side snack bar with stools, looking out at the dark alley.
	CreateBlock(FVector(2424, -634, 101), FVector(16, 72, 5),
		TexMat(TEXT("M_WoodFurnitureUV"), WoodMaterial));
	if (!CreateProp(TEXT("SM_Stool"), FVector(2442, -634, 2), PlasticDarkMaterial, 0.0f, 1.0f, true))
	{
		CreateBlock(FVector(2442, -634, 31), FVector(24, 24, 62), PlasticDarkMaterial, true, CylinderMesh);
	}
	CreateProp(TEXT("SM_Stool"), FVector(2442, -586, 2), PlasticDarkMaterial, 18.0f, 1.0f, true);

	// Warm spill onto the pavement in front of the entrance.
	UPointLightComponent* SpillLight = CreateLight(
		FVector(2340, -457, 225), 1050.0f, 660.0f, FLinearColor(1.0f, 0.95f, 0.85f), true, 22.0f);
	SpillLight->SetVolumetricScatteringIntensity(0.45f);

	// Interior fluorescents in a 2x2 grid over the enlarged sales floor.
	for (const float PanelX : {2540.0f, 2820.0f})
	{
		for (const float PanelY : {-300.0f, -560.0f})
		{
			StoreLightDiscs.Add(CreateBlock(
				FVector(PanelX, PanelY, 264), FVector(120, 42, 6),
				LightPanelMaterial, false));
			UPointLightComponent* CeilingLight = CreateLight(
				FVector(PanelX, PanelY, 238), 2850.0f, 720.0f,
				FLinearColor(1.0f, 0.98f, 0.92f), true, 28.0f);
			CeilingLight->SetVolumetricScatteringIntensity(0.16f);
			StoreLights.Add(CeilingLight);
		}
	}

	// Long service counter against the north wall: laminate body, steel top,
	// kick recess, seams, card terminal, hot-snack warmer, tobacco wall.
	CreateBlock(FVector(2560, -250, 51), FVector(220, 60, 90), FridgeBodyMaterial);
	CreateBlock(FVector(2560, -250, 97.5f), FVector(224, 64, 3), Metal);
	CreateBlock(FVector(2560, -276, 5), FVector(216, 8, 10), PlasticDarkMaterial, false);
	CreateBlock(FVector(2560, -282.2f, 94), FVector(220, 2, 4), PlasticDarkMaterial, false);
	CreateBlock(FVector(2452, -281.2f, 50), FVector(1.5f, 2, 84), PlasticDarkMaterial, false);
	CreateBlock(FVector(2668, -281.2f, 50), FVector(1.5f, 2, 84), PlasticDarkMaterial, false);
	CreateBlock(FVector(2500, -268, 101.5f), FVector(12, 9, 5), PlasticDarkMaterial, false);
	CreateBlock(
		FVector(2500, -271, 106), FVector(10, 1.5f, 7),
		TexMat(TEXT("M_ScreenGlow"), ScreenGlowMaterial), false,
		nullptr, FRotator(-28, 0, 0));
	// Hot-snack warmer glowing at the counter's west end.
	CreateBlock(FVector(2452, -250, 116), FVector(36, 38, 36), PlasticDarkMaterial);
	CreateBlock(FVector(2433, -250, 116), FVector(2, 30, 28), GlassMaterial, false);
	CreateBlock(FVector(2445, -250, 130), FVector(20, 26, 2),
		TexMat(TEXT("M_StreetLampGlow"), StreetLampGlowMaterial), false);
	// Tobacco wall behind the counter.
	CreateBlock(FVector(2560, -190, 150), FVector(220, 14, 140), PlasticDarkMaterial);
	int32 CigaretteIndex = 0;
	for (const float RackZ : {126.0f, 154.0f, 182.0f})
	{
		for (float RackX = 2470.0f; RackX <= 2650.0f; RackX += 24.0f)
		{
			UMaterialInterface* RackMaterial =
				(CigaretteIndex % 3 == 0) ? SnackRedMaterial :
				(CigaretteIndex % 3 == 1) ? SnackYellowMaterial : SnackBlueMaterial;
			CreateBlock(FVector(RackX, -181, RackZ), FVector(14, 8, 12), RackMaterial, false);
			++CigaretteIndex;
		}
	}

	// Two double-sided gondolas built like real shop fixtures: a central back
	// panel, a kick base, and cantilevered shelf tiers on each face. Product
	// stands *on* a tier inside the bay instead of perching on the top cap.
	// Waist-height runs, as in a real store: you can see clear across the shop
	// floor over the tops of them, and the aisle between is wide enough to
	// pass someone.
	for (const float GondolaY : {-365.0f, -555.0f})
	{
		// Spine and structure.
		CreateBlock(FVector(2640, GondolaY, 68), FVector(300, 8, 136), ShelfSteel);
		CreateBlock(FVector(2640, GondolaY, 8), FVector(292, 46, 16), PlasticDarkMaterial);
		CreateBlock(FVector(2488, GondolaY, 70), FVector(6, 50, 140), Metal);
		CreateBlock(FVector(2792, GondolaY, 70), FVector(6, 50, 140), Metal);
		CreateBlock(FVector(2640, GondolaY, 140), FVector(304, 50, 4), Metal, false);

		const float TierHeights[] = {30.0f, 60.0f, 90.0f, 120.0f};
		const TCHAR* SnackLabels[] = {
			TEXT("M_SnackShrimp"), TEXT("M_SnackPotato"),
			TEXT("M_SnackSquid"), TEXT("M_SnackCorn")};

		for (const float FaceSign : {-1.0f, 1.0f})
		{
			int32 SnackIndex = static_cast<int32>(GondolaY * 0.1f + FaceSign);
			for (int32 TierIndex = 0; TierIndex < 4; ++TierIndex)
			{
				const float TierZ = TierHeights[TierIndex];
				const float ShelfY = GondolaY + FaceSign * 13.0f;
				// Shelf plate, its raised front lip and the price rail.
				CreateBlock(FVector(2640, ShelfY, TierZ), FVector(298, 22, 3), Metal);
				CreateBlock(FVector(2640, GondolaY + FaceSign * 23.5f, TierZ + 3.5f),
					FVector(298, 2, 5), Metal, false);
				CreateBlock(FVector(2640, GondolaY + FaceSign * 24.5f, TierZ + 2.0f),
					FVector(296, 2, 7), FridgeInteriorMaterial, false);

				// Bags stand on the tier, backs to the spine, faces to the
				// aisle — packed shoulder to shoulder the way a stocked shelf
				// actually looks, not spaced out like a museum case.
				for (float SnackX = 2500.0f; SnackX <= 2780.0f; SnackX += 14.0f)
				{
					UMaterialInterface* SnackMaterial = TexMat(
						SnackLabels[FMath::Abs(SnackIndex) % 4],
						(SnackIndex % 3 == 0) ? SnackRedMaterial :
						(SnackIndex % 3 == 1) ? SnackYellowMaterial : SnackBlueMaterial);
					if (!CreateProp(
						TEXT("SM_SnackBag"),
						// The bag mesh carries a crimped bottom seal, so its
						// pivot sits slightly below the body.
						FVector(SnackX, GondolaY + FaceSign * 11.0f, TierZ + 6.8f),
						SnackMaterial,
						FaceSign > 0.0f ? 90.0f : -90.0f,
						0.48f))
					{
						CreateBlock(
							FVector(SnackX, GondolaY + FaceSign * 11.0f, TierZ + 8.0f),
							FVector(15, 12, 16), SnackMaterial, false);
					}
					++SnackIndex;
				}
			}
		}
	}
	for (float CupX = 2540.0f; CupX <= 2700.0f; CupX += 40.0f)
	{
		// A cup ramyeon is three materials, not one. The foam cup, the printed
		// band around it, and the foil lid are separate props: putting the
		// label on the cup mesh painted the artwork over the lid and the
		// underside too, because the lid is unioned into the same mesh and
		// shares its only material slot.
		CreateCupRamyeon(FVector(CupX, -365, 142), CupX * 1.7f);
	}

	// South wall: chilled open showcase (kimbap/sandwich) flanked by scanned
	// steel racks with crate/bottle stock, plus the ramyeon corner and poster.
	CreateBlock(FVector(2700, -662, 90), FVector(240, 36, 170), ShelfSteel);
	for (const float TierZ : {70.0f, 105.0f, 140.0f})
	{
		CreateBlock(FVector(2694, -654, TierZ), FVector(228, 26, 3), Metal, false);
		CreateBlock(FVector(2688, -646.5f, TierZ + 3), FVector(228, 3, 5), FridgeInteriorMaterial, false);
	}
	CreateBlock(FVector(2700, -652, 166), FVector(230, 20, 3), LightPanelMaterial, false);
	int32 ChilledIndex = 0;
	for (const float TierZ : {71.5f, 106.5f, 141.5f})
	{
		for (float ItemX = 2600.0f; ItemX <= 2790.0f; ItemX += 27.0f)
		{
			// Kimbap trays lie flat; sandwich wedges stand on their long edge.
			const bool bKimbap = (ChilledIndex % 2) == 0;
			const bool bPlaced = bKimbap
				? CreateProp(TEXT("SM_KimbapPack"), FVector(ItemX, -655, TierZ),
					FridgeInteriorMaterial, 90.0f) != nullptr
				: CreateProp(TEXT("SM_SandwichPack"), FVector(ItemX, -655, TierZ),
					SnackYellowMaterial, 90.0f) != nullptr;
			if (!bPlaced)
			{
				CreateBlock(
					FVector(ItemX, -655, TierZ + 4.0f),
					bKimbap ? FVector(9, 8, 8) : FVector(13, 9, 6),
					bKimbap ? FridgeInteriorMaterial : SnackYellowMaterial, false);
			}
			++ChilledIndex;
		}
	}
	PlacePhotoProp(TEXT("steel_frame_shelves_01"), FVector(2530, -660, 6), FVector(105, 48, 170), 180.0f);
	PlacePhotoProp(TEXT("plastic_crate_01"), FVector(2560, -620, 6), FVector(44, 34, 28), 15.0f);
	PlacePhotoProp(TEXT("wine_bottles_01"), FVector(2510, -628, 6), FVector(40, 30, 36), 200.0f);
	CreateBlock(
		FVector(2470, -678.5f, 200), FVector(70, 1.5f, 48),
		TexMat(TEXT("M_PosterRamyeon"), SignWhiteMaterial), false);
	// Ramyeon corner stack by the window bar.
	CreateBlock(FVector(2452, -668, 86), FVector(70, 38, 160), ShelfSteel);
	for (float CupX = 2432.0f; CupX <= 2472.0f; CupX += 20.0f)
	{
		CreateCupRamyeon(FVector(CupX, -654, 166.0f), CupX * 3.0f);
	}

	// East wall: the walk-up reach-in cooler bank — six framed glass doors,
	// each bay lit and stocked; bay two stands open for restocking, and that
	// is where the last bottled water waits.
	CreateBlock(FVector(2955, -430, 116), FVector(10, 480, 220), ShelfSteel);
	CreateBlock(FVector(2925, -676, 116), FVector(60, 12, 220), ShelfSteel);
	CreateBlock(FVector(2925, -184, 116), FVector(60, 12, 220), ShelfSteel);
	CreateBlock(FVector(2925, -430, 222), FVector(60, 480, 12), ShelfSteel);
	CreateBlock(FVector(2925, -430, 16), FVector(60, 480, 20), ShelfSteel);

	const float BayCenters[] = {-630.0f, -552.0f, -474.0f, -396.0f, -318.0f, -240.0f};
	const int32 OpenBayIndex = 1; // y = -552: the open, half-restocked bay
	for (int32 BayIndex = 0; BayIndex < 6; ++BayIndex)
	{
		const float BayY = BayCenters[BayIndex];
		// Interior of the bay: liner, three shelves, light strip, drinks. The
		// liner stays off pure white so the bays do not blow out under the
		// ceiling fluorescents and wash the product labels away.
		CreateBlock(FVector(2948, BayY, 116), FVector(6, 72, 210), FridgeBodyMaterial, false);
		for (const float ShelfZ : {60.0f, 105.0f, 150.0f})
		{
			CreateBlock(FVector(2925, BayY, ShelfZ), FVector(44, 68, 3), Metal);
		}
		CreateBlock(FVector(2905, BayY, 200), FVector(3, 60, 3), ScreenGlowMaterial, false);
		int32 DrinkIndex = BayIndex;
		// Two rows deep and shoulder to shoulder: a stocked drinks cooler is
		// a solid wall of product, not a few bottles on a rail.
		for (const float ShelfTopZ : {61.5f, 106.5f, 151.5f})
		{
			for (float DrinkRowX : {2925.0f, 2941.0f})
			{
			for (float DrinkY = BayY - 30.0f; DrinkY <= BayY + 30.0f; DrinkY += 9.0f)
			{
				if (BayIndex == OpenBayIndex && FMath::IsNearlyEqual(ShelfTopZ, 106.5f))
				{
					continue; // the water pickups live on this shelf instead
				}
				// Lathed bottles: soda, tea and soju silhouettes alternate.
				const bool bTallBottle = (DrinkIndex % 3) == 2;
				UMaterialInterface* DrinkMaterial =
					(DrinkIndex % 3 == 0) ? BottleGreenMaterial :
					(DrinkIndex % 3 == 1) ? BottleBrownMaterial : SnackYellowMaterial;
				UMaterialInterface* CapMaterial =
					(DrinkIndex % 2 == 0) ? SnackRedMaterial : FridgeInteriorMaterial;
				const FVector BottleBase(DrinkRowX, DrinkY, ShelfTopZ);
				// Bottles face the aisle, so every label reads from the front.
				const float BottleYaw = -90.0f + (DrinkIndex % 3 - 1) * 7.0f;
				if (bTallBottle)
				{
					CreateProp(TEXT("SM_SojuBottle"), BottleBase, BottleGreenMaterial,
						BottleYaw);
					CreateProp(TEXT("SM_BottleCap"), BottleBase + FVector(0, 0, 21.0f),
						CapMaterial, 0.0f);
					CreateBottleLabel(BottleBase, 3.42f, 3.0f, 7.0f,
						TEXT("M_LabelSoju"), BottleYaw);
				}
				else
				{
					CreateProp(TEXT("SM_DrinkBottle"), BottleBase, DrinkMaterial,
						BottleYaw);
					CreateProp(TEXT("SM_BottleCap"), BottleBase + FVector(0, 0, 16.9f),
						CapMaterial, 0.0f);
					const TCHAR* LabelName = (DrinkIndex % 3 == 0)
						? TEXT("M_LabelGreenTea")
						: ((DrinkIndex % 2 == 0) ? TEXT("M_LabelBarley") : TEXT("M_LabelSoda"));
					CreateBottleLabel(BottleBase, 3.67f, 3.0f, 7.6f, LabelName, BottleYaw);
				}
				++DrinkIndex;
			}
			}
		}
		// Price strip on every shelf edge.
		for (const float StripZ : {63.0f, 108.0f, 153.0f})
		{
			CreateBlock(
				FVector(2902, BayY, StripZ), FVector(2, 64, 8),
				TexMat(TEXT("M_PriceStrip"), FridgeInteriorMaterial), false);
		}

		// Door frame; the open bay's leaf swings wide on its hinge.
		CreateBlock(FVector(2900, BayY - 37.0f, 110), FVector(8, 6, 214), Metal);
		CreateBlock(FVector(2900, BayY + 37.0f, 110), FVector(8, 6, 214), Metal);
		CreateBlock(FVector(2900, BayY, 214), FVector(8, 80, 8), Metal);
		CreateBlock(FVector(2900, BayY, 8), FVector(8, 80, 8), Metal);
		if (BayIndex == OpenBayIndex)
		{
			CreateBlock(
				FVector(2878, BayY - 62.0f, 110), FVector(4, 66, 196),
				GlassMaterial, true, nullptr, FRotator(0, -64, 0));
		}
		else
		{
			CreateBlock(FVector(2899, BayY, 110), FVector(4, 66, 196), GlassMaterial);
			CreateBlock(FVector(2894, BayY + 28.0f, 110), FVector(3, 4, 44), Metal, false);
		}
	}
	StoreLights.Add(CreateLight(
		FVector(2890, -430, 190), 320.0f, 480.0f, FLinearColor(0.75f, 0.85f, 1.0f), false, 8.0f));

	// Ice-cream chest freezer against the west glass, south of the door.
	CreateBlock(FVector(2445, -545, 46), FVector(58, 110, 80), FridgeBodyMaterial);
	CreateBlock(FVector(2445, -545, 88), FVector(52, 104, 4), GlassMaterial, false);
	CreateBlock(FVector(2445, -545, 84), FVector(56, 108, 3), Metal, false);

	// Tobacco notice over the cigarette wall, entrance mat, CCTV eye.
	CreateBlock(
		FVector(2560, -182.5f, 228), FVector(140, 2, 12),
		TexMat(TEXT("M_TobaccoNotice"), FridgeInteriorMaterial), false);
	CreateBlock(FVector(2435, -457, 7.2f), FVector(70, 95, 2), PlasticDarkMaterial, false);
	CreateBlock(FVector(2426, -398, 246), FVector(15, 10, 10), FridgeInteriorMaterial, false);
	CreateBlock(
		FVector(2434, -404, 241), FVector(7, 7, 10),
		PlasticDarkMaterial, false, CylinderMesh, FRotator(48, 35, 0));
	CreateBlock(
		FVector(2420, -398, 24), FVector(22, 22, 46),
		PlasticDarkMaterial, true, CylinderMesh);
	PlacePhotoProp(
		TEXT("outdoor_table_chair_set_01"), FVector(2350, -600, 0),
		FVector(170, 170, 110), 25.0f);
	PlacePhotoProp(
		TEXT("plastic_monobloc_chair_01"), FVector(2296, -556, 0),
		FVector(55, 55, 85), 160.0f);

	// Stacked hand baskets by the counter's west end (authored hollow shells).
	if (CreateProp(TEXT("SM_Basket"), FVector(2445, -305, 6), SnackRedMaterial, -8.0f, 1.0f, true))
	{
		CreateProp(TEXT("SM_Basket"), FVector(2446, -306, 27), SnackRedMaterial, 12.0f);
		CreateProp(TEXT("SM_Basket"), FVector(2444, -304, 48), SnackRedMaterial, -3.0f);
	}
	else if (PlacePhotoProp(TEXT("plastic_crate_01"), FVector(2445, -305, 6), FVector(44, 34, 28), -8.0f))
	{
		PlacePhotoProp(TEXT("plastic_crate_01"), FVector(2446, -306, 34), FVector(42, 32, 27), 12.0f);
	}
	else
	{
		CreateBlock(FVector(2445, -305, 17), FVector(40, 30, 22), SnackRedMaterial);
		CreateBlock(FVector(2445, -305, 39), FVector(38, 28, 20), SnackRedMaterial);
	}
}

void AIGPrologueWorldScene::BuildSkyAndFog()
{
	// Physically based pre-dawn: real atmospheric scattering with the sun
	// still 8 degrees below the eastern horizon, a genuine directional moon,
	// and a real-time sky light so every surface receives twilight ambience.
	SkyAtmosphere = NewObject<USkyAtmosphereComponent>(this, TEXT("SkyAtmosphere"));
	SkyAtmosphere->SetupAttachment(SceneRoot);
	SkyAtmosphere->RegisterComponent();

	PreDawnSun = NewObject<UDirectionalLightComponent>(this, TEXT("PreDawnSun"));
	PreDawnSun->SetupAttachment(SceneRoot);
	PreDawnSun->SetMobility(EComponentMobility::Movable);
	// The prologue's east is +X: nautical twilight, sun 14 degrees under.
	PreDawnSun->SetWorldRotation(FRotator(14.0f, 180.0f, 0.0f));
	PreDawnSun->SetIntensity(120000.0f); // physical sun illuminance in lux
	PreDawnSun->SetLightColor(FLinearColor(1.0f, 0.86f, 0.72f));
	// A shadowless 120 klux directional light shines straight through the
	// villa and overexposes every surface even while the sun is below the
	// horizon. Let the ground/building occlude it.
	PreDawnSun->SetCastShadows(true);
	PreDawnSun->SetAtmosphereSunLight(true);
	PreDawnSun->SetAtmosphereSunLightIndex(0);
	PreDawnSun->ForwardShadingPriority = 0;
	PreDawnSun->RegisterComponent();

	MoonLight = NewObject<UDirectionalLightComponent>(this, TEXT("MoonLight"));
	MoonLight->SetupAttachment(SceneRoot);
	MoonLight->SetMobility(EComponentMobility::Movable);
	MoonLight->SetWorldRotation(FRotator(-38.0f, 35.0f, 0.0f));
	MoonLight->SetIntensity(0.25f); // full-moon-ish illuminance in lux
	MoonLight->SetLightColor(FLinearColor(0.62f, 0.72f, 0.92f));
	MoonLight->SetCastShadows(true);
	MoonLight->ContactShadowLength = 0.05f;
	MoonLight->SetAtmosphereSunLight(true);
	MoonLight->SetAtmosphereSunLightIndex(1);
	MoonLight->SetVolumetricScatteringIntensity(0.15f);
	// The moon owns single-light effects (volumetric fog) at this hour.
	MoonLight->ForwardShadingPriority = 1;
	MoonLight->RegisterComponent();

	SkyAmbient = NewObject<USkyLightComponent>(this, TEXT("SkyAmbient"));
	SkyAmbient->SetupAttachment(SceneRoot);
	SkyAmbient->SetMobility(EComponentMobility::Movable);
	SkyAmbient->bRealTimeCapture = true;
	SkyAmbient->SetIntensity(0.62f);
	SkyAmbient->bLowerHemisphereIsBlack = true;
	SkyAmbient->RegisterComponent();

	// Thin pre-dawn haze; volumetric so the streetlights carve visible cones.
	HeightFog = NewObject<UExponentialHeightFogComponent>(this, TEXT("HeightFog"));
	HeightFog->SetupAttachment(SceneRoot);
	HeightFog->SetRelativeLocation(FVector(1000, -400, 0));
	HeightFog->SetFogDensity(0.012f);
	HeightFog->SetFogHeightFalloff(0.4f);
	HeightFog->SetFogInscatteringColor(FLinearColor(0.030f, 0.042f, 0.085f));
	HeightFog->SetVolumetricFog(true);
	HeightFog->SetVolumetricFogScatteringDistribution(0.55f);
	HeightFog->SetVolumetricFogExtinctionScale(1.0f);
	HeightFog->RegisterComponent();
}

void AIGPrologueWorldScene::SpawnInteractables()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	FActorSpawnParameters SpawnParameters;
	SpawnParameters.Owner = this;
	SpawnParameters.SpawnCollisionHandlingOverride =
		ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	// Fridge with its empty interior; the reveal drives the whole morning.
	Fridge = World->SpawnActor<AIGFridge>(
		AIGFridge::StaticClass(),
		FTransform(FRotator::ZeroRotator, IGPrologueWorld::FridgeLocation),
		SpawnParameters);
	if (Fridge)
	{
		AIGFridge::FIGFridgeContentMeshes FridgeContents;
		FridgeContents.KimchiTub = PropMesh(TEXT("SM_KimchiTub"));
		FridgeContents.MilkCarton = PropMesh(TEXT("SM_MilkCarton"));
		FridgeContents.SojuBottle = PropMesh(TEXT("SM_SojuBottle"));
		Fridge->SetContentMeshes(FridgeContents);

		Fridge->ConfigurePrototypeVisuals(
			CubeMesh, CylinderMesh,
			FridgeBodyMaterial, FridgeInteriorMaterial, PlasticDarkMaterial,
			GlassMaterial, TexMat(TEXT("M_MetalUV"), MetalFrameMaterial),
			SnackRedMaterial, SnackYellowMaterial, BottleGreenMaterial);

		// A single empty bottle lies on its side: the "no water" beat.
		CreatePhysicsProp(
			PropMesh(TEXT("SM_WaterBottle"), CylinderMesh), GlassMaterial,
			FVector::OneVector, FVector(150, -32, 986), FRotator(0, 0, 90), 0.15f);

		// The handwritten note on the door: "buy water" — someone already knew.
		CreateDecoOnComponent(
			Fridge->GetDoorPivot(), CubeMesh,
			TexMat(TEXT("M_NoteFridge"), SignWhiteMaterial),
			FVector(-6.9f, 36.0f, 18.0f), FRotator::ZeroRotator,
			FVector(0.012f, 0.20f, 0.20f));
	}

	// This morning's paper on the landing outside 404. The delivery-ad sticker
	// that belongs on the door itself is applied after the door is spawned,
	// further down — testing HomeDoor here silently did nothing, because the
	// actor does not exist yet at this point in the function.
	CreateBlock(
		FVector(130, -250, 901.2f), FVector(30, 21, 1.6f),
		TexMat(TEXT("M_NoticeA4"), SignWhiteMaterial), false,
		nullptr, FRotator(0, 24, 0));

	// Lobby: the monthly maintenance-fee notice by the mailboxes.
	CreateBlock(
		FVector(474, -226.4f, 150), FVector(22, 1.2f, 30),
		TexMat(TEXT("M_NoticeA4"), SignWhiteMaterial), false);

	// Front door: locked until the fridge is checked and the wallet is taken.
	HomeDoor = World->SpawnActor<AIGSwingDoor>(
		AIGSwingDoor::StaticClass(),
		FTransform(FRotator(0, -90, 0), IGPrologueWorld::HomeDoorLocation),
		SpawnParameters);
	if (HomeDoor)
	{
		HomeDoor->ConfigurePrototypeVisuals(
			CubeMesh,
			TexMat(TEXT("M_SteelDoorUV"), DoorMaterial),
			TexMat(TEXT("M_StainlessUV"), MetalFrameMaterial),
			FVector(7, 84, 204));
		HomeDoor->SetLeverMesh(
			PropMesh(TEXT("SM_LeverHandle")),
			TexMat(TEXT("M_MetalUV"), MetalFrameMaterial),
			FVector(7, 84, 204));
		// Korean entrance doors open outward — and it keeps the hallway clear.
		HomeDoor->SetOpenYaw(-95.0f);

		TArray<FIGDoorRequirement> DoorRequirements;
		{
			FIGDoorRequirement FridgeRequirement;
			FridgeRequirement.RequiredState = FGameplayTag::RequestGameplayTag(
				FName(TEXT("State.CH01.Morning.FridgeChecked")), false);
			FridgeRequirement.LockedPrompt =
				NSLOCTEXT("IGPrologue", "DoorNeedFridge", "…아직 나갈 이유가 없다");
			FridgeRequirement.LockedThought = NSLOCTEXT(
				"IGPrologue", "DoorNeedFridgeThought", "목말라. 냉장고부터 확인하자.");
			DoorRequirements.Add(MoveTemp(FridgeRequirement));

			FIGDoorRequirement WalletRequirement;
			WalletRequirement.RequiredState = FGameplayTag::RequestGameplayTag(
				FName(TEXT("State.CH01.Morning.HasWallet")), false);
			WalletRequirement.LockedPrompt =
				NSLOCTEXT("IGPrologue", "DoorNeedWallet", "지갑을 안 챙겼다");
			WalletRequirement.LockedThought = NSLOCTEXT(
				"IGPrologue", "DoorNeedWalletThought", "빈손으로는 못 가지. 지갑… 책상 위에 있었지.");
			DoorRequirements.Add(MoveTemp(WalletRequirement));
		}
		HomeDoor->SetRequirements(MoveTemp(DoorRequirements));

		// Delivery-ad sticker, on the leaf so it swings with the door. This
		// has to happen after the spawn above.
		CreateDecoOnComponent(
			HomeDoor->GetDoorPivot(), CubeMesh,
			TexMat(TEXT("M_DoorAd"), SignWhiteMaterial),
			FVector(-8.4f, 24.0f, 78.0f), FRotator::ZeroRotator,
			FVector(0.008f, 0.17f, 0.17f));
	}

	// Common entrance of the villa: a properly framed glass door off the lobby.
	BuildingDoor = World->SpawnActor<AIGSwingDoor>(
		AIGSwingDoor::StaticClass(),
		FTransform(FRotator(0, -90, 0), FVector(604, -385, 0)),
		SpawnParameters);
	if (BuildingDoor)
	{
		BuildingDoor->ConfigureFramedGlassVisuals(
			CubeMesh, GlassMaterial, TexMat(TEXT("M_MetalUV"), MetalFrameMaterial),
			FVector(6, 84, 204));
		BuildingDoor->SetOpenYaw(-95.0f);
		BuildingDoor->SetInteractionPrompt(
			NSLOCTEXT("IGPrologue", "BuildingDoorPrompt", "공동현관 열기"));
	}

	// --- Torch on the shoe cabinet by the door ----------------------------
	// It is a prop in chapter one and the only light source in chapter two,
	// so it sits somewhere you walk past on the way out either way.
	Flashlight = World->SpawnActor<AIGPickupItem>(
		AIGPickupItem::StaticClass(),
		FTransform(
			FRotator(0, 24, 0),
			FVector(30.0f, -190.0f, IGPrologueWorld::FourthFloorZ + 117.0f)),
		SpawnParameters);
	if (Flashlight)
	{
		Flashlight->ConfigurePrototypeVisuals(
			CylinderMesh, PlasticDarkMaterial, FVector(0.05f, 0.05f, 0.17f), false);
		Flashlight->GetMeshComponent()->SetRelativeRotation(FRotator(90.0f, 0.0f, 0.0f));
		Flashlight->PickupMode = EIGPickupMode::Pocket;
		Flashlight->StateTagOnPickup = FGameplayTag::RequestGameplayTag(
			FName(TEXT("State.CH02.Loop.HasFlashlight")), false);
		Flashlight->ThoughtOnPickup = NSLOCTEXT(
			"IGPrologue", "TorchTaken", "손전등. …배터리가 얼마 안 남았을 텐데.");
		Flashlight->SetInteractionPrompt(
			NSLOCTEXT("IGPrologue", "TorchPrompt", "손전등 챙기기"));
		Flashlight->OnPickedUp.AddUniqueDynamic(
			this, &AIGPrologueWorldScene::HandleFlashlightPickedUp);
		// It is environmental dressing on the first morning. The second loop
		// enables the exact same prop instead of spawning it into view.
		Flashlight->SetInteractionEnabled(false);
	}

	// --- Management notice taped beside the lift --------------------------
	// The seed of the whole story: a routine notice about the roof tank that
	// means nothing on the first morning and everything on the third.
	ManagementNotice = World->SpawnActor<AIGReadableNote>(
		AIGReadableNote::StaticClass(),
		FTransform(
			FRotator::ZeroRotator,
			FVector(676.0f, -232.0f, IGPrologueWorld::FourthFloorZ + 142.0f)),
		SpawnParameters);
	if (ManagementNotice)
	{
		// Taped up long enough that the bottom has gone wavy with damp — the
		// first hint, before anything is wrong, that water gets everywhere in
		// this building.
		ManagementNotice->ConfigurePrototypeVisuals(
			CubeMesh,
			TexMat(TEXT("M_PaperWet"), SignWhiteMaterial),
			FVector(21.0f, 1.2f, 29.7f));
		ManagementNotice->SetInteractionPrompt(
			NSLOCTEXT("IGPrologue", "NoticePrompt", "공지 읽기"));
		// Chapter three is already on this piece of paper; on the first
		// morning it is only a reason the tap ran dry.
		ManagementNotice->SetNoteText(
			NSLOCTEXT("IGPrologue", "NoticeTitle", "[관리사무소] 단수 안내"),
			{
				NSLOCTEXT("IGPrologue", "NoticeL1", "입주민 여러분께 알려 드립니다."),
				FText::GetEmpty(),
				NSLOCTEXT("IGPrologue", "NoticeL2", "옥상 물탱크 청소 및 수질 점검 관계로"),
				NSLOCTEXT("IGPrologue", "NoticeL3", "아래와 같이 단수를 실시합니다."),
				FText::GetEmpty(),
				NSLOCTEXT("IGPrologue", "NoticeL4", "   일시 :  7월 26일 (금)  04:00 ~ 06:00"),
				NSLOCTEXT("IGPrologue", "NoticeL5", "   대상 :  전 세대 (401호 ~ 404호)"),
				FText::GetEmpty(),
				NSLOCTEXT("IGPrologue", "NoticeL6", "점검 중에는 옥상 출입을 삼가 주시기"),
				NSLOCTEXT("IGPrologue", "NoticeL7", "바랍니다. 불편을 드려 죄송합니다."),
				FText::GetEmpty(),
				FText::GetEmpty(),
				NSLOCTEXT("IGPrologue", "NoticeL8", "            달빛빌라 관리사무소"),
			});
	}

	// The villa elevator: call it on 4F, ride down to the lobby.
	Elevator = World->SpawnActor<AIGElevator>(
		AIGElevator::StaticClass(),
		FTransform(FRotator::ZeroRotator, IGPrologueWorld::ElevatorLocation),
		SpawnParameters);
	if (Elevator)
	{
		AIGElevator::FIGElevatorVisuals CabVisuals;
		CabVisuals.CubeMesh = CubeMesh;
		CabVisuals.CylinderMesh = CylinderMesh;
		CabVisuals.StainlessMaterial = TexMat(TEXT("M_StainlessUV"), MetalFrameMaterial);
		CabVisuals.MirrorMaterial = TexMat(TEXT("M_CabMirrorUV"), MetalFrameMaterial);
		CabVisuals.FloorMaterial = TexMat(TEXT("M_MarbleFloor_XY"), StoreFloorMaterial);
		CabVisuals.InlayMaterial = PlasticDarkMaterial;
		CabVisuals.CopMaterial = TexMat(TEXT("M_LiftCOP"), ScreenGlowMaterial);
		CabVisuals.HallMaterial = TexMat(TEXT("M_LiftHall"), ScreenGlowMaterial);
		CabVisuals.DiffuserMaterial = LightPanelMaterial;
		Elevator->ConfigurePrototypeVisuals(CabVisuals, 900.0f);
	}

	// Wallet on the desk.
	Wallet = World->SpawnActor<AIGPickupItem>(
		AIGPickupItem::StaticClass(),
		FTransform(FRotator(0, 20, 0), IGPrologueWorld::WalletLocation),
		SpawnParameters);
	if (Wallet)
	{
		Wallet->PickupMode = EIGPickupMode::Pocket;
		Wallet->StateTagOnPickup = FGameplayTag::RequestGameplayTag(
			FName(TEXT("State.CH01.Morning.HasWallet")), false);
		Wallet->SetInteractionPrompt(NSLOCTEXT("IGPrologue", "WalletPrompt", "지갑 챙기기"));
		Wallet->ThoughtOnPickup =
			NSLOCTEXT("IGPrologue", "WalletThought", "지갑. 현금이 조금 있다.");
		Wallet->ConfigurePrototypeVisuals(
			CubeMesh, WalletBrownMaterial, FVector(0.14f, 0.09f, 0.03f), false);
	}

	// Bathroom door and window flavor.
	if (AIGInspectable* BathroomDoor = World->SpawnActor<AIGInspectable>(
		AIGInspectable::StaticClass(),
		FTransform(FRotator::ZeroRotator, FVector(20, -214, 1000)),
		SpawnParameters))
	{
		BathroomDoor->ConfigurePrototypeVisuals(CubeMesh, DoorMaterial, FVector(0.7f, 0.05f, 2.0f));
		BathroomDoor->SetInteractionPrompt(NSLOCTEXT("IGPrologue", "BathroomPrompt", "화장실 문"));
		BathroomDoor->ThoughtText =
			NSLOCTEXT("IGPrologue", "BathroomThought", "…지금은 급하지 않다.");
	}

	if (AIGInspectable* Window = World->SpawnActor<AIGInspectable>(
		AIGInspectable::StaticClass(),
		FTransform(FRotator::ZeroRotator, FVector(-100, 214, 1050)),
		SpawnParameters))
	{
		Window->ConfigurePrototypeVisuals(CubeMesh, WindowGlowMaterial, FVector(1.2f, 0.04f, 0.9f));
		Window->SetInteractionPrompt(NSLOCTEXT("IGPrologue", "WindowPrompt", "창문"));
		Window->ThoughtText =
			NSLOCTEXT("IGPrologue", "WindowThought", "아직 어둡다. 해 뜨려면 한참 남았는데.");
	}

	// Store sliding door.
	StoreDoor = World->SpawnActor<AIGSlidingDoor>(
		AIGSlidingDoor::StaticClass(),
		FTransform(FRotator::ZeroRotator, IGPrologueWorld::StoreDoorLocation),
		SpawnParameters);
	if (StoreDoor)
	{
		StoreDoor->ConfigurePrototypeVisuals(
			CubeMesh, GlassMaterial, MetalFrameMaterial, FVector(6, 60, 200));
	}

	// Water bottles on the open cooler bay's middle shelf; any one works.
	const float WaterYs[] = {-576.0f, -552.0f, -528.0f};
	for (const float WaterY : WaterYs)
	{
		AIGPickupItem* WaterBottle = World->SpawnActor<AIGPickupItem>(
			AIGPickupItem::StaticClass(),
			FTransform(FRotator::ZeroRotator, FVector(2925, WaterY, 106.5f)),
			SpawnParameters);
		if (WaterBottle)
		{
			WaterBottle->PickupMode = EIGPickupMode::CarryInHand;
			WaterBottle->StateTagOnPickup = FGameplayTag::RequestGameplayTag(
				FName(TEXT("State.CH01.Morning.HasWater")), false);
			WaterBottle->SetInteractionPrompt(
				NSLOCTEXT("IGPrologue", "WaterPrompt", "생수 집기 (500mL)"));
			WaterBottle->ThoughtOnPickup =
				NSLOCTEXT("IGPrologue", "WaterThought", "차갑다. 이거면 됐어.");
			// Held low and to the side so the lathed bottle reads without
			// filling the view; the mesh pivot is at the bottle's base.
			WaterBottle->CarryOffset = FVector(34.0f, 15.0f, -26.0f);
			WaterBottle->CarryRotation = FRotator(-12.0f, -14.0f, 0.0f);
			// Not simulated on the shelf: a lathed bottle standing on a wire
			// shelf topples the instant physics settles, and this one is a
			// pickup target rather than a kickable prop.
			WaterBottle->ConfigurePrototypeVisuals(
				PropMesh(TEXT("SM_WaterBottle"), CylinderMesh),
				GlassMaterial, FVector::OneVector, false);
			CreateDecoOnComponent(
				WaterBottle->GetMeshComponent(),
				PropMesh(TEXT("SM_BottleCap"), CylinderMesh), SnackBlueMaterial,
				FVector(0, 0, 20.1f), FRotator::ZeroRotator, FVector::OneVector);
			// The printed sleeve is what actually reads as "생수" in hand.
			CreateDecoOnComponent(
				WaterBottle->GetMeshComponent(),
				PropMesh(TEXT("SM_LabelSleeve"), CylinderMesh),
				TexMat(TEXT("M_LabelWater"), WaterBlueMaterial),
				FVector(0, 0, 5.0f), FRotator(0.0f, -90.0f, 0.0f),
				FVector(3.36f, 3.36f, 8.6f));
			WaterBottles.Add(WaterBottle);
		}
	}

	// Self-service checkout on the counter.
	Checkout = World->SpawnActor<AIGCheckoutCounter>(
		AIGCheckoutCounter::StaticClass(),
		FTransform(FRotator::ZeroRotator, IGPrologueWorld::CheckoutLocation),
		SpawnParameters);
	if (Checkout)
	{
		Checkout->ConfigurePrototypeVisuals(CubeMesh, PlasticDarkMaterial, ScreenGlowMaterial);

		// A scanned cash register stands in for the greybox cluster; the
		// hidden blocks keep providing the interaction collision.
		if (PlacePhotoProp(
			TEXT("CashRegister_01"),
			FVector(2620, -253, 99), FVector(48, 44, 40), 180.0f, false))
		{
			Checkout->SetVisualsHidden(true);
		}
	}

	// Progress volumes: stepping outside, mid-alley beat, entering the store.
	auto SpawnZone = [&](const FVector& Location, const FVector& Extent,
		const TCHAR* StateTagName, const FText& Thought) -> AIGZoneTrigger*
	{
		AIGZoneTrigger* Zone = World->SpawnActor<AIGZoneTrigger>(
			AIGZoneTrigger::StaticClass(),
			FTransform(FRotator::ZeroRotator, Location),
			SpawnParameters);
		if (Zone)
		{
			Zone->SetZoneExtent(Extent);
			if (StateTagName)
			{
				Zone->StateTagOnEnter = FGameplayTag::RequestGameplayTag(FName(StateTagName), false);
			}
			Zone->ThoughtOnEnter = Thought;
		}
		return Zone;
	};

	LeftHomeZone = SpawnZone(
		FVector(643, -435, 110), FVector(120, 55, 110),
		TEXT("State.CH01.Morning.LeftHome"),
		NSLOCTEXT("IGPrologue", "LeftHomeThought", "공기가 차다. …골목이 너무 조용한데."));
	FlickerZone = SpawnZone(
		FVector(1000, -537, 110), FVector(80, 160, 110),
		nullptr,
		FText::GetEmpty());
	StoreEntryZone = SpawnZone(
		FVector(2450, -457, 116), FVector(35, 95, 110),
		TEXT("State.CH01.Morning.EnteredStore"),
		NSLOCTEXT("IGPrologue", "EnteredStoreThought", "…“어서 오세요” 소리는 없었다."));
}

void AIGPrologueWorldScene::SpawnChapterTwoInteractables()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	FActorSpawnParameters SpawnParameters;
	SpawnParameters.Owner = this;
	SpawnParameters.SpawnCollisionHandlingOverride =
		ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	auto ParkActor = [](AIGInteractableActor* Actor)
	{
		if (!Actor)
		{
			return;
		}
		Actor->SetActorHiddenInGame(true);
		Actor->SetActorEnableCollision(false);
		Actor->SetInteractionEnabled(false);
	};

	// Open 403 at the east end: a normal interactable leaf, initially parked
	// behind CH01's removable wall plug.
	MirrorRoomDoor = World->SpawnActor<AIGSwingDoor>(
		AIGSwingDoor::StaticClass(),
		FTransform(
			FRotator(0, -90, 0),
			FVector(388.0f, -225.0f, IGPrologueWorld::FourthFloorZ)),
		SpawnParameters);
	if (MirrorRoomDoor)
	{
		MirrorRoomDoor->ConfigurePrototypeVisuals(
			CubeMesh,
			TexMat(TEXT("M_SteelDoorUV"), DoorMaterial),
			TexMat(TEXT("M_StainlessUV"), MetalFrameMaterial),
			FVector(7, 84, 204));
		// Swing into 403 (+Y), not across the 72 cm-wide corridor route.
		MirrorRoomDoor->SetOpenYaw(95.0f);
		MirrorRoomDoor->SetInteractionPrompt(
			NSLOCTEXT("IGCH02", "MirrorDoorPrompt", "403호 문"));
		MirrorRoomDoor->ForceOpenState(true);
		ParkActor(MirrorRoomDoor);
	}

	auto SpawnNote = [&](const FVector& Location,
		const FRotator& Rotation,
		const FVector& PaperSize,
		UMaterialInterface* PaperMaterial,
		const FText& Prompt,
		const FText& Title,
		TArray<FText>&& Lines) -> AIGReadableNote*
	{
		AIGReadableNote* Note = World->SpawnActor<AIGReadableNote>(
			AIGReadableNote::StaticClass(),
			FTransform(Rotation, Location),
			SpawnParameters);
		if (Note)
		{
			Note->ConfigurePrototypeVisuals(
				CubeMesh,
				PaperMaterial ? PaperMaterial : SignWhiteMaterial.Get(),
				PaperSize);
			Note->SetInteractionPrompt(Prompt);
			Note->SetNoteText(Title, MoveTemp(Lines));
			ParkActor(Note);
		}
		return Note;
	};

	auto ReceiptLines = []() -> TArray<FText>
	{
		return {
			NSLOCTEXT("IGCH02", "ReceiptDate", "2026-07-26 04:44"),
			NSLOCTEXT("IGCH02", "ReceiptWater", "새벽수 500mL / 1 / 1,100원"),
			NSLOCTEXT("IGCH02", "ReceiptCard", "체크카드 승인 4482**"),
			NSLOCTEXT("IGCH02", "ReceiptPoints", "적립 없음"),
		};
	};

	auto BuildReceiptData = []() -> FIGThermalReceiptData
	{
		FIGThermalReceiptData Data;
		Data.StoreName =
			NSLOCTEXT("IGCH02", "ReceiptStore", "새벽편의점");
		Data.StoreSubtitle =
			NSLOCTEXT("IGCH02", "ReceiptStoreSubtitle", "24 HOURS");
		Data.StoreDetailLines = {
			NSLOCTEXT("IGCH02", "ReceiptBusinessMasked", "사업자번호  ***-**-*****"),
			NSLOCTEXT("IGCH02", "ReceiptStoreMasked", "가맹점 정보  ***************"),
		};
		Data.PolicyLines = {
			NSLOCTEXT("IGCH02", "ReceiptPolicy1", "교환·환불 시 영수증을"),
			NSLOCTEXT("IGCH02", "ReceiptPolicy2", "지참해 주세요."),
		};
		Data.TransactionDateTime =
			NSLOCTEXT("IGCH02", "ReceiptDateStructured", "2026-07-26 04:44");
		Data.PosLabel =
			NSLOCTEXT("IGCH02", "ReceiptPos", "POS-01");
		Data.ReceiptNumber =
			NSLOCTEXT("IGCH02", "ReceiptNumber", "0444");

		FIGReceiptItemLine WaterItem;
		WaterItem.ProductName =
			NSLOCTEXT("IGCH02", "ReceiptProduct", "새벽수 500mL");
		WaterItem.Quantity = 1;
		WaterItem.Amount = 1100;
		Data.Items.Add(MoveTemp(WaterItem));

		Data.Subtotal = 1100;
		Data.TaxableSupply = 1000;
		Data.Vat = 100;
		Data.Total = 1100;
		Data.PaymentHeading =
			NSLOCTEXT("IGCH02", "ReceiptPaymentHeading", "체크카드 매출전표");

		FIGReceiptKeyValueLine CardLine;
		CardLine.Label =
			NSLOCTEXT("IGCH02", "ReceiptCardLabel", "체크카드 승인");
		CardLine.Value =
			NSLOCTEXT("IGCH02", "ReceiptCardValue", "4482**");
		Data.PaymentLines.Add(MoveTemp(CardLine));

		FIGReceiptKeyValueLine PaymentAmountLine;
		PaymentAmountLine.Label =
			NSLOCTEXT("IGCH02", "ReceiptPaymentAmountLabel", "결제금액");
		PaymentAmountLine.Value =
			NSLOCTEXT("IGCH02", "ReceiptPaymentAmountValue", "1,100");
		Data.PaymentLines.Add(MoveTemp(PaymentAmountLine));

		FIGReceiptKeyValueLine PointsLine;
		PointsLine.Label =
			NSLOCTEXT("IGCH02", "ReceiptPointsLabel", "적립");
		PointsLine.Value =
			NSLOCTEXT("IGCH02", "ReceiptPointsValue", "없음");
		Data.PaymentLines.Add(MoveTemp(PointsLine));

		Data.FooterLines = {
			NSLOCTEXT("IGCH02", "ReceiptFooter", "이용해 주셔서 감사합니다."),
		};
		// The HUD turns this into a deliberately non-standard visual pattern,
		// so it cannot be mistaken for a real store's scannable barcode.
		Data.BarcodeDigits = TEXT("2026072604441100");
		return Data;
	};

	ExistingReceipt = SpawnNote(
		FVector(2582, -252, 100.5f),
		FRotator::ZeroRotator,
		FVector(8.5f, 15.0f, 0.25f),
		TexMat(TEXT("M_PaperClean"), SignWhiteMaterial),
		NSLOCTEXT("IGCH02", "ReceiptPrompt", "놓인 영수증 읽기"),
		NSLOCTEXT("IGCH02", "ReceiptTitle", "영수증"),
		ReceiptLines());
	if (ExistingReceipt)
	{
		ExistingReceipt->SetThermalReceiptData(BuildReceiptData());
	}

	DuplicateReceipt = SpawnNote(
		FVector(2600, -252, 100.8f),
		FRotator(0, 0, 4),
		FVector(8.5f, 15.0f, 0.25f),
		TexMat(TEXT("M_PaperClean"), SignWhiteMaterial),
		NSLOCTEXT("IGCH02", "DuplicateReceiptPrompt", "새 영수증 읽기"),
		NSLOCTEXT("IGCH02", "DuplicateReceiptTitle", "영수증"),
		ReceiptLines());
	if (DuplicateReceipt)
	{
		DuplicateReceipt->SetThermalReceiptData(BuildReceiptData());
	}

	MailboxBills = SpawnNote(
		FVector(528, -222.5f, 157),
		FRotator::ZeroRotator,
		FVector(19, 1.3f, 27),
		TexMat(TEXT("M_PaperFolded"), SignWhiteMaterial),
		NSLOCTEXT("IGCH02", "MailboxPrompt", "403호 우편함 확인"),
		NSLOCTEXT("IGCH02", "MailboxTitle", "403호 우편함"),
		{
			NSLOCTEXT("IGCH02", "MailboxL1", "전기요금 납부 고지서"),
			NSLOCTEXT("IGCH02", "MailboxL2", "도시가스 사용량 안내"),
			NSLOCTEXT("IGCH02", "MailboxL3", "관리비 미납 안내"),
			FText::GetEmpty(),
			NSLOCTEXT("IGCH02", "MailboxL4", "수취인  404호  한지운"),
			NSLOCTEXT("IGCH02", "MailboxL5", "수취인  404호  한지운"),
			NSLOCTEXT("IGCH02", "MailboxL6", "수취인  404호  한지운"),
		});

	SaltMemo = SpawnNote(
		FVector(688, -383.0f, 125),
		FRotator::ZeroRotator,
		FVector(21, 1.2f, 29.7f),
		TexMat(TEXT("M_PaperClean"), SignWhiteMaterial),
		NSLOCTEXT("IGCH02", "SaltMemoPrompt", "메모 읽기"),
		NSLOCTEXT("IGCH02", "SaltMemoTitle", "관리사무소"),
		{
			NSLOCTEXT("IGCH02", "SaltMemoL1", "※ 현관에 소금 뿌리신 분,"),
			NSLOCTEXT("IGCH02", "SaltMemoL2", "치워 주세요."),
			NSLOCTEXT("IGCH02", "SaltMemoL3", "민원 들어옵니다."),
			FText::GetEmpty(),
			NSLOCTEXT("IGCH02", "SaltMemoL4", "— 관리사무소"),
		});

	NightRoster = SpawnNote(
		FVector(2838, -181.0f, 145),
		FRotator::ZeroRotator,
		FVector(21, 1.2f, 29.7f),
		TexMat(TEXT("M_PaperOld"), SignWhiteMaterial),
		NSLOCTEXT("IGCH02", "RosterPrompt", "야간 근무표 읽기"),
		NSLOCTEXT("IGCH02", "RosterTitle", "야간 근무표"),
		{
			NSLOCTEXT("IGCH02", "RosterL1", "목     최상구"),
			NSLOCTEXT("IGCH02", "RosterL2", "금 새벽          "),
			NSLOCTEXT("IGCH02", "RosterL3", "토     최상구"),
			FText::GetEmpty(),
			NSLOCTEXT("IGCH02", "RosterL4", "금 새벽 알바 구합니다"),
			NSLOCTEXT("IGCH02", "RosterL5", "문의: 점장"),
		});

	// A second set of pickups remains allocation-stable behind the loop cut.
	ChapterTwoWallet = World->SpawnActor<AIGPickupItem>(
		AIGPickupItem::StaticClass(),
		FTransform(FRotator(0, 20, 0), IGPrologueWorld::WalletLocation),
		SpawnParameters);
	if (ChapterTwoWallet)
	{
		ChapterTwoWallet->PickupMode = EIGPickupMode::Pocket;
		ChapterTwoWallet->StateTagOnPickup = FGameplayTag::RequestGameplayTag(
			FName(TEXT("State.CH02.Loop.HasWallet")), false);
		ChapterTwoWallet->SetInteractionPrompt(
			NSLOCTEXT("IGCH02", "SecondWalletPrompt", "지갑 챙기기"));
		ChapterTwoWallet->ThoughtOnPickup =
			NSLOCTEXT("IGCH02", "SecondWalletThought", "지갑. …분명 아까 챙겼는데.");
		ChapterTwoWallet->ConfigurePrototypeVisuals(
			CubeMesh, WalletBrownMaterial, FVector(0.14f, 0.09f, 0.03f), false);
		ParkActor(ChapterTwoWallet);
	}

	for (const float WaterY : {-576.0f, -552.0f, -528.0f})
	{
		AIGPickupItem* WaterBottle = World->SpawnActor<AIGPickupItem>(
			AIGPickupItem::StaticClass(),
			FTransform(FRotator::ZeroRotator, FVector(2925, WaterY, 106.5f)),
			SpawnParameters);
		if (!WaterBottle)
		{
			continue;
		}

		WaterBottle->PickupMode = EIGPickupMode::CarryInHand;
		WaterBottle->StateTagOnPickup = FGameplayTag::RequestGameplayTag(
			FName(TEXT("State.CH02.Loop.HasWater")), false);
		WaterBottle->SetInteractionPrompt(
			NSLOCTEXT("IGCH02", "SecondWaterPrompt", "생수 집기 (500mL)"));
		WaterBottle->ThoughtOnPickup =
			NSLOCTEXT("IGCH02", "SecondWaterThought", "같은 자리. 같은 물.");
		WaterBottle->CarryOffset = FVector(34.0f, 15.0f, -26.0f);
		WaterBottle->CarryRotation = FRotator(-12.0f, -14.0f, 0.0f);
		WaterBottle->ConfigurePrototypeVisuals(
			PropMesh(TEXT("SM_WaterBottle"), CylinderMesh),
			GlassMaterial, FVector::OneVector, false);
		CreateDecoOnComponent(
			WaterBottle->GetMeshComponent(),
			PropMesh(TEXT("SM_BottleCap"), CylinderMesh), SnackBlueMaterial,
			FVector(0, 0, 20.1f), FRotator::ZeroRotator, FVector::OneVector);
		CreateDecoOnComponent(
			WaterBottle->GetMeshComponent(),
			PropMesh(TEXT("SM_LabelSleeve"), CylinderMesh),
			TexMat(TEXT("M_LabelWater"), WaterBlueMaterial),
			FVector(0, 0, 5.0f), FRotator(0.0f, -90.0f, 0.0f),
			FVector(3.36f, 3.36f, 8.6f));
		ParkActor(WaterBottle);
		ChapterTwoWaterBottles.Add(WaterBottle);
	}

	auto SpawnParkedZone = [&](const FVector& Location,
		const FVector& Extent,
		const TCHAR* StateTagName,
		const FText& Thought) -> AIGZoneTrigger*
	{
		AIGZoneTrigger* Zone = World->SpawnActor<AIGZoneTrigger>(
			AIGZoneTrigger::StaticClass(),
			FTransform(FRotator::ZeroRotator, Location),
			SpawnParameters);
		if (Zone)
		{
			Zone->SetZoneExtent(Extent);
			Zone->StateTagOnEnter = FGameplayTag::RequestGameplayTag(
				FName(StateTagName), false);
			Zone->ThoughtOnEnter = Thought;
			Zone->SetActorEnableCollision(false);
		}
		return Zone;
	};

	ChapterTwoLeftHomeZone = SpawnParkedZone(
		FVector(180, -305, 1010), FVector(72, 62, 110),
		TEXT("State.CH02.Loop.LeftHome"),
		FText::GetEmpty());
	MirrorSightZone = SpawnParkedZone(
		FVector(350, -300, 1010), FVector(72, 72, 110),
		TEXT("State.CH02.Loop.SawMirrorRoom"),
		NSLOCTEXT("IGCH02", "MirrorMovedThought", "403호가… 여기였나?"));
	MirrorEntryZone = SpawnParkedZone(
		// Trigger only after the capsule has cleared the inward-swinging leaf.
		// Closing at the threshold made the door safety sensor reopen it and
		// left the scripted blackout playing against a visibly open doorway.
		FVector(430, -65, 1010), FVector(45, 20, 110),
		TEXT("State.CH02.Loop.EnteredMirrorRoom"),
		NSLOCTEXT("IGCH02", "MirrorEnteredThought", "…우리 집이랑 똑같네."));
	ChapterTwoStoreEntryZone = SpawnParkedZone(
		FVector(2450, -457, 116), FVector(35, 95, 110),
		TEXT("State.CH02.Loop.EnteredStore"),
		FText::GetEmpty());
	ChapterTwoReturnZone = SpawnParkedZone(
		FVector(643, -360, 110), FVector(72, 52, 110),
		TEXT("State.CH02.Loop.Returned"),
		FText::GetEmpty());
}

void AIGPrologueWorldScene::HandleFlashlightPickedUp(AIGPickupItem* Item)
{
	const UWorld* World = GetWorld();
	APlayerController* PlayerController = World ? World->GetFirstPlayerController() : nullptr;
	AIGPlayerCharacter* Player = PlayerController
		? Cast<AIGPlayerCharacter>(PlayerController->GetPawn())
		: nullptr;
	if (UIGFlashlightComponent* Torch = Player ? Player->GetFlashlight() : nullptr)
	{
		Torch->SetAvailable(true);
	}
}

void AIGPrologueWorldScene::SpawnDirectors()
{
	UWorld* World = GetWorld();
	if (!World || !CubeMesh)
	{
		return;
	}

	AlarmSound = NewObject<UIGAlarmSoundWave>(this, TEXT("PrologueAlarmTone"));

	const FTransform AlarmTransform(FRotator::ZeroRotator, IGPrologueWorld::AlarmLocation);
	AlarmClock = World->SpawnActorDeferred<AIGPrologueAlarmClock>(
		AIGPrologueAlarmClock::StaticClass(),
		AlarmTransform,
		this,
		nullptr,
		ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (!AlarmClock)
	{
		return;
	}
	AlarmClock->ConfigurePrototype(
		PropMesh(TEXT("SM_AlarmClock"), CubeMesh),
		CubeMesh,
		PlasticDarkMaterial,
		TexMat(TEXT("M_ClockFace"), AlarmMaterial),
		TexMat(TEXT("M_MetalUV"), MetalFrameMaterial),
		AlarmSound);
	AlarmClock->FinishSpawning(AlarmTransform);

	WakeDirector = World->SpawnActorDeferred<AIGPrologueWakeDirector>(
		AIGPrologueWakeDirector::StaticClass(),
		FTransform::Identity,
		this,
		nullptr,
		ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (!WakeDirector)
	{
		return;
	}
	WakeDirector->ConfigurePrototype(AlarmClock);
	WakeDirector->FinishSpawning(FTransform::Identity);

	const FTransform GetUpTransform(
		FRotator::ZeroRotator,
		IGPrologueWorld::GetUpTargetLocation);
	GetUpTarget = World->SpawnActorDeferred<AIGPrologueGetUpTarget>(
		AIGPrologueGetUpTarget::StaticClass(),
		GetUpTransform,
		this,
		nullptr,
		ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (GetUpTarget)
	{
		GetUpTarget->ConfigurePrototype(
			WakeDirector, CubeMesh, TexMat(TEXT("M_BeddingUV"), BeddingMaterial));
		GetUpTarget->FinishSpawning(GetUpTransform);
	}

	// Morning routine director wired to the world it narrates.
	FActorSpawnParameters DirectorParameters;
	DirectorParameters.Owner = this;
	MorningDirector = World->SpawnActor<AIGMorningRoutineDirector>(
		AIGMorningRoutineDirector::StaticClass(),
		FTransform::Identity,
		DirectorParameters);
	if (MorningDirector)
	{
		TArray<UPointLightComponent*> StoreLightPointers;
		for (const TObjectPtr<UPointLightComponent>& StoreLight : StoreLights)
		{
			StoreLightPointers.Add(StoreLight.Get());
		}
		MorningDirector->SetSceneReferences(
			StoreDoor,
			FlickerStreetlight,
			MoveTemp(StoreLightPointers),
			JingleComponent);

		if (FlickerZone)
		{
			FlickerZone->OnZoneTriggered.AddDynamic(
				this,
				&ThisClass::HandleFlickerZoneTriggered);
		}
	}

	WakeDirector->StartWakeUp();
	UE_LOG(LogIndieGame, Display, TEXT("Prologue wake flow started."));
}

void AIGPrologueWorldScene::CreateAmbience()
{
	// Apartment room tone (the fridge carries its own hum).
	UIGAmbienceSoundWave* RoomTone = NewObject<UIGAmbienceSoundWave>(this, TEXT("RoomToneWave"));
	RoomTone->Configure(EIGAmbienceMode::RoomTone, 0xA13F92C7u);
	CreateAmbientBed(RoomTone, FVector(0, 0, 1020), 0.6f, 320.0f, 850.0f);

	// Pre-dawn wind over the alley.
	UIGAmbienceSoundWave* Wind = NewObject<UIGAmbienceSoundWave>(this, TEXT("StreetWindWave"));
	Wind->Configure(EIGAmbienceMode::StreetWind, 0x5D2E77B1u);
	CreateAmbientBed(Wind, FVector(1040, -457, 240), 0.85f, 900.0f, 2600.0f);

	// Fluorescent ballast buzz inside the store.
	UIGAmbienceSoundWave* Buzz = NewObject<UIGAmbienceSoundWave>(this, TEXT("StoreBuzzWave"));
	Buzz->Configure(EIGAmbienceMode::StoreBuzz, 0x3C91D4E5u);
	CreateAmbientBed(Buzz, FVector(2595, -457, 250), 0.5f, 320.0f, 950.0f);

	// The store jingle: cheerful, looping, and completely indifferent.
	JingleComponent = CreateAmbientBed(
		UIGToneSequenceSoundWave::CreateStoreJingle(this),
		FVector(2600, -457, 252),
		0.45f,
		260.0f,
		1700.0f);

	if (MorningDirector && JingleComponent)
	{
		// Late wire: the jingle component is created after the director spawns.
		TArray<UPointLightComponent*> StoreLightPointers;
		for (const TObjectPtr<UPointLightComponent>& StoreLight : StoreLights)
		{
			StoreLightPointers.Add(StoreLight.Get());
		}
		MorningDirector->SetSceneReferences(
			StoreDoor,
			FlickerStreetlight,
			MoveTemp(StoreLightPointers),
			JingleComponent);
	}
}

void AIGPrologueWorldScene::SpawnDemoDirectorIfRequested()
{
	const TCHAR* CommandLine = FCommandLine::Get();
	const bool bCaptureStills = FParse::Param(CommandLine, TEXT("IGCapture"));
	const bool bDumpFrames = FParse::Param(CommandLine, TEXT("IGDemoFrames"));
	const bool bDemoOnly = FParse::Param(CommandLine, TEXT("IGDemo"));
	if (!bCaptureStills && !bDumpFrames && !bDemoOnly)
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	FActorSpawnParameters DemoParameters;
	DemoParameters.Owner = this;
	DemoDirector = World->SpawnActor<AIGDemoDirector>(
		AIGDemoDirector::StaticClass(),
		FTransform::Identity,
		DemoParameters);
	if (DemoDirector)
	{
		DemoDirector->ConfigureDemo(
			WakeDirector,
			AlarmClock,
			Fridge,
			Wallet,
			HomeDoor,
			Elevator,
			BuildingDoor,
			WaterBottles.Num() > 0 ? WaterBottles[0].Get() : nullptr,
			Checkout,
			bCaptureStills,
			bDumpFrames,
			bCaptureStills || bDumpFrames);
	}
}

void AIGPrologueWorldScene::HandleStoryStateChanged(
	const FGameplayTag StateTag,
	const bool bAdded)
{
	if (!bAdded || bChapterTwoActive || bChapterTwoTransitionPending)
	{
		return;
	}

	const FGameplayTag ChapterOnePurchase = FGameplayTag::RequestGameplayTag(
		FName(TEXT("State.CH01.Morning.WaterPurchased")), false);
	if (StateTag.MatchesTagExact(ChapterOnePurchase))
	{
		SpawnReturnBoundary();
	}
}

void AIGPrologueWorldScene::SpawnReturnBoundary()
{
	if (ReturnBoundaryZone || !GetWorld())
	{
		return;
	}

	// Promotional CH01 capture remains a stable, finite route. The natural
	// loop boundary is only armed in an interactive run.
	const TCHAR* CommandLine = FCommandLine::Get();
	if (FParse::Param(CommandLine, TEXT("IGCapture"))
		|| FParse::Param(CommandLine, TEXT("IGDemoFrames"))
		|| FParse::Param(CommandLine, TEXT("IGDemo")))
	{
		return;
	}

	FActorSpawnParameters SpawnParameters;
	SpawnParameters.Owner = this;
	SpawnParameters.SpawnCollisionHandlingOverride =
		ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	ReturnBoundaryZone = GetWorld()->SpawnActor<AIGZoneTrigger>(
		AIGZoneTrigger::StaticClass(),
		FTransform(FRotator::ZeroRotator, FVector(643, -360, 110)),
		SpawnParameters);
	if (ReturnBoundaryZone)
	{
		ReturnBoundaryZone->SetZoneExtent(FVector(72, 52, 110));
		ReturnBoundaryZone->OnZoneTriggered.AddUniqueDynamic(
			this, &ThisClass::HandleReturnBoundaryTriggered);
	}
}

void AIGPrologueWorldScene::HandleReturnBoundaryTriggered(AIGZoneTrigger* Zone)
{
	if (Zone == ReturnBoundaryZone)
	{
		BeginChapterTwoTransition();
	}
}

void AIGPrologueWorldScene::BeginChapterTwoTransition()
{
	if (bChapterTwoTransitionPending || bChapterTwoActive || !GetWorld())
	{
		return;
	}
	bChapterTwoTransitionPending = true;

	if (APlayerController* PlayerController = GetWorld()->GetFirstPlayerController())
	{
		if (APlayerCameraManager* CameraManager = PlayerController->PlayerCameraManager)
		{
			CameraManager->StartCameraFade(
				0.0f, 1.0f, 1.2f, FLinearColor::Black, false, true);
		}
	}

	GetWorldTimerManager().SetTimer(
		ChapterTransitionHandle,
		this,
		&ThisClass::EnterChapterTwo,
		1.2f,
		false);
}

void AIGPrologueWorldScene::EnterChapterTwo()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	bChapterTwoTransitionPending = false;
	bChapterTwoActive = true;

	if (AIGReadableNote* OpenNote = AIGReadableNote::GetOpenNote())
	{
		OpenNote->Close();
	}

	if (MorningDirector)
	{
		MorningDirector->Destroy();
		MorningDirector = nullptr;
	}

	if (UGameInstance* GameInstance = GetGameInstance())
	{
		if (UIGStoryStateSubsystem* StoryState =
			GameInstance->GetSubsystem<UIGStoryStateSubsystem>())
		{
			StoryState->ClearStates(false);
		}
	}

	// Consume/hide the first-loop inventory. Carry pointers are weak, so
	// destroying the held bottle releases the hand without another mutation.
	if (Wallet)
	{
		Wallet->SetActorHiddenInGame(true);
		Wallet->SetActorEnableCollision(false);
		Wallet->SetInteractionEnabled(false);
	}
	for (AIGPickupItem* WaterBottle : WaterBottles)
	{
		if (WaterBottle)
		{
			WaterBottle->Destroy();
		}
	}
	WaterBottles.Reset();

	auto RevealPickup = [](AIGPickupItem* Item)
	{
		if (!Item)
		{
			return;
		}
		Item->SetActorHiddenInGame(false);
		Item->SetActorEnableCollision(true);
		Item->SetInteractionEnabled(true);
	};
	RevealPickup(ChapterTwoWallet);
	for (AIGPickupItem* WaterBottle : ChapterTwoWaterBottles)
	{
		RevealPickup(WaterBottle);
	}
	if (Flashlight)
	{
		Flashlight->SetInteractionEnabled(true);
	}

	SetChapterTwoOverlayVisible(true);
	for (int32 FixtureIndex = 0; FixtureIndex < GetCorridorFixtureCount(); ++FixtureIndex)
	{
		SetFixtureLive(FixtureIndex, true, true);
	}
	SuspendCorridorFlicker(true);
	SetFixtureLive(0, false, true);

	// The ground lobby is not black — it is an exhausted quarter-light.
	for (UPointLightComponent* LobbyLight : LobbyLights)
	{
		if (LobbyLight)
		{
			LobbyLight->SetIntensity(400.0f);
		}
	}

	if (Fridge)
	{
		Fridge->ConfigureChapterState(
			FGameplayTag::RequestGameplayTag(
				FName(TEXT("State.CH02.Loop.FridgeChecked")), false),
			NSLOCTEXT("IGCH02", "SecondFridgeThought", "…또 물이 없다."));
		Fridge->ResetForNewChapter();
	}

	if (Checkout)
	{
		Checkout->ConfigureChapterPurchase(
			FGameplayTag::RequestGameplayTag(
				FName(TEXT("State.CH02.Loop.HasWater")), false),
			FGameplayTag::RequestGameplayTag(
				FName(TEXT("State.CH02.Loop.WaterPurchased")), false),
			NSLOCTEXT("IGCH02", "SecondCheckoutPrompt", "계산하기 (생수 1,100원)"),
			FText::GetEmpty());
		Checkout->ResetForNewChapter();
	}

	if (HomeDoor)
	{
		HomeDoor->ForceOpenState(false);
		TArray<FIGDoorRequirement> Requirements;

		FIGDoorRequirement FridgeRequirement;
		FridgeRequirement.RequiredState = FGameplayTag::RequestGameplayTag(
			FName(TEXT("State.CH02.Loop.FridgeChecked")), false);
		FridgeRequirement.LockedPrompt =
			NSLOCTEXT("IGCH02", "SecondDoorNeedFridge", "…또 냉장고부터");
		FridgeRequirement.LockedThought =
			NSLOCTEXT("IGCH02", "SecondDoorNeedFridgeThought", "목이 마르다. 확인부터 하자.");
		Requirements.Add(MoveTemp(FridgeRequirement));

		FIGDoorRequirement WalletRequirement;
		WalletRequirement.RequiredState = FGameplayTag::RequestGameplayTag(
			FName(TEXT("State.CH02.Loop.HasWallet")), false);
		WalletRequirement.LockedPrompt =
			NSLOCTEXT("IGCH02", "SecondDoorNeedWallet", "지갑을 안 챙겼다");
		WalletRequirement.LockedThought =
			NSLOCTEXT("IGCH02", "SecondDoorNeedWalletThought", "지갑은… 같은 자리에 있겠지.");
		Requirements.Add(MoveTemp(WalletRequirement));

		FIGDoorRequirement FlashlightRequirement;
		FlashlightRequirement.RequiredState = FGameplayTag::RequestGameplayTag(
			FName(TEXT("State.CH02.Loop.HasFlashlight")), false);
		FlashlightRequirement.LockedPrompt =
			NSLOCTEXT("IGCH02", "SecondDoorNeedTorch", "손전등을 챙겨야 한다");
		FlashlightRequirement.LockedThought =
			NSLOCTEXT("IGCH02", "SecondDoorNeedTorchThought", "복도 불이… 손전등을 챙기자.");
		Requirements.Add(MoveTemp(FlashlightRequirement));
		HomeDoor->SetRequirements(MoveTemp(Requirements));
	}
	if (BuildingDoor)
	{
		BuildingDoor->ForceOpenState(false);
	}
	if (MirrorRoomDoor)
	{
		MirrorRoomDoor->ForceOpenState(true);
	}

	if (Elevator)
	{
		Elevator->ResetForNewRide();
		Elevator->ConfigureIntermediateStop(true, 12.0f);
	}

	// CH01 volumes outlive their director. Disable them before the second
	// route starts so direct CH02 runs cannot emit first-morning thoughts or
	// repopulate State.CH01 tags after the story-state reset.
	for (AIGZoneTrigger* Zone :
		{LeftHomeZone.Get(), FlickerZone.Get(), StoreEntryZone.Get(),
			ReturnBoundaryZone.Get()})
	{
		if (Zone)
		{
			Zone->SetActorEnableCollision(false);
		}
	}

	for (AIGZoneTrigger* Zone :
		{ChapterTwoLeftHomeZone.Get(), MirrorSightZone.Get(), MirrorEntryZone.Get(),
			ChapterTwoStoreEntryZone.Get()})
	{
		if (Zone)
		{
			Zone->SetActorEnableCollision(true);
		}
	}
	// The route passes through this part of the lobby on the outbound trip.
	// Arming it here ended CH02 before the player ever reached the store.
	SetChapterTwoReturnZoneArmed(false);

	// Put the body back beside the same bed before the second alarm begins.
	APlayerController* PlayerController = World->GetFirstPlayerController();
	APawn* PlayerPawn = PlayerController ? PlayerController->GetPawn() : nullptr;
	if (PlayerPawn)
	{
		PlayerPawn->SetActorLocationAndRotation(
			IGPrologueWorld::PlayerLocation,
			IGPrologueWorld::PlayerActorRotation,
			false,
			nullptr,
			ETeleportType::TeleportPhysics);
	}
	if (PlayerController)
	{
		PlayerController->SetControlRotation(IGPrologueWorld::PlayerViewRotation);
	}

	if (AIGPlayerCharacter* Player = Cast<AIGPlayerCharacter>(PlayerPawn))
	{
		if (UIGFlashlightComponent* Torch = Player->GetFlashlight())
		{
			Torch->SetAvailable(false);
			Torch->RefillBattery(1.0f);
		}
	}

	FActorSpawnParameters DirectorParameters;
	DirectorParameters.Owner = this;
	SecondMorningDirector = World->SpawnActorDeferred<AIGSecondMorningDirector>(
		AIGSecondMorningDirector::StaticClass(),
		FTransform::Identity,
		this,
		nullptr,
		ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (SecondMorningDirector)
	{
		SecondMorningDirector->Configure(
			this,
			MirrorRoomDoor,
			MirrorRoomLamp,
			Elevator,
			StoreDoor,
			JingleComponent,
			ExistingReceipt,
			DuplicateReceipt,
			MailboxBills,
			SaltMemo,
			NightRoster,
			ManagementNotice);
		SecondMorningDirector->FinishSpawning(FTransform::Identity);

		if (PlayerController)
		{
			if (AIGHorrorHUD* HorrorHUD =
				Cast<AIGHorrorHUD>(PlayerController->GetHUD()))
			{
				HorrorHUD->SetObjectiveProvider(SecondMorningDirector);
			}
		}
	}

	if (UGameInstance* GameInstance = GetGameInstance())
	{
		if (UIGStoryStateSubsystem* StoryState =
			GameInstance->GetSubsystem<UIGStoryStateSubsystem>())
		{
			StoryState->AddState(FGameplayTag::RequestGameplayTag(
				FName(TEXT("State.CH02.Loop.Started")), false));
		}
	}

	const bool bCaptureChapterTwo =
		FParse::Param(FCommandLine::Get(), TEXT("IGCaptureCH02"));
	if (!bCaptureChapterTwo)
	{
		AIGHorrorHUD::ShowChapterCard(
			this,
			NSLOCTEXT("IGCH02", "ChapterTwoEyebrow", "CHAPTER 02"),
			NSLOCTEXT("IGCH02", "ChapterTwoTitle", "두 번째 아침"),
			NSLOCTEXT("IGCH02", "ChapterTwoSubtitle", "집이 아니다"),
			4.2f);
	}

	if (WakeDirector)
	{
		WakeDirector->ConfigureChapterSaveTags(
			FGameplayTag::RequestGameplayTag(FName(TEXT("Chapter.CH02")), false),
			FGameplayTag::RequestGameplayTag(
				FName(TEXT("State.CH02.Wake.AlarmStopped")), false),
			FGameplayTag::RequestGameplayTag(
				FName(TEXT("State.CH02.Wake.Standing")), false),
			FGameplayTag::RequestGameplayTag(
				FName(TEXT("Checkpoint.CH02.Woke")), false),
			false);
		WakeDirector->ResetForNewChapter();
		WakeDirector->StartWakeUp();
		if (bCaptureChapterTwo)
		{
			WakeDirector->RestoreStandingCheckpoint();
		}
	}

	if (bCaptureChapterTwo)
	{
		StartChapterTwoCaptureSequence();
	}

	UE_LOG(LogIndieGame, Display, TEXT("CH02 second morning entered in-session."));
}

void AIGPrologueWorldScene::StartChapterTwoCaptureSequence()
{
	// Deterministic documentation captures must never contain checkerboard
	// materials or the editor's "Preparing Shaders" overlay.
	FAssetCompilingManager::Get().FinishAllCompilation();
	if (GShaderCompilingManager)
	{
		GShaderCompilingManager->FinishAllCompilation();
	}
	if (GEngine)
	{
		GEngine->bEnableOnScreenDebugMessages = false;
	}
	if (const UWorld* World = GetWorld())
	{
		if (APlayerController* PlayerController = World->GetFirstPlayerController())
		{
			PlayerController->ConsoleCommand(TEXT("r.MotionBlurQuality 0"), true);
			PlayerController->ConsoleCommand(TEXT("DisableAllScreenMessages"), true);
		}
	}

	ChapterCaptureIndex = 0;
	GetWorldTimerManager().SetTimer(
		ChapterCaptureHandle,
		this,
		&ThisClass::CaptureNextChapterTwoFrame,
		1.5f,
		false);
}

void AIGPrologueWorldScene::CaptureNextChapterTwoFrame()
{
	UWorld* World = GetWorld();
	APlayerController* PlayerController = World ? World->GetFirstPlayerController() : nullptr;
	APawn* PlayerPawn = PlayerController ? PlayerController->GetPawn() : nullptr;
	if (!World || !PlayerController || !PlayerPawn)
	{
		FinishChapterTwoCaptureSequence();
		return;
	}
	// Material/PSO jobs can be queued on the first rendered frames after the
	// initial world build. Drain that second wave before each still.
	FAssetCompilingManager::Get().FinishAllCompilation();
	if (GShaderCompilingManager)
	{
		GShaderCompilingManager->FinishAllCompilation();
	}

	if (APlayerCameraManager* CameraManager = PlayerController->PlayerCameraManager)
	{
		CameraManager->StopCameraFade();
	}
	if (AIGReadableNote* OpenNote = AIGReadableNote::GetOpenNote())
	{
		OpenNote->Close();
	}

	auto PlaceCaptureCamera = [PlayerPawn, PlayerController](
		const FVector& PawnLocation,
		const FVector& LookAt)
	{
		PlayerPawn->SetActorLocation(
			PawnLocation,
			false,
			nullptr,
			ETeleportType::TeleportPhysics);

		FVector CameraLocation = PawnLocation + FVector(0, 0, 64.0f);
		if (const AIGPlayerCharacter* Player =
			Cast<AIGPlayerCharacter>(PlayerPawn))
		{
			if (const UCameraComponent* Camera = Player->GetFirstPersonCamera())
			{
				CameraLocation = Camera->GetComponentLocation();
			}
		}

		const FRotator LookRotation = (LookAt - CameraLocation).Rotation();
		PlayerPawn->SetActorRotation(FRotator(0, LookRotation.Yaw, 0));
		PlayerController->SetControlRotation(LookRotation);
	};

	FString BaseName;
	switch (ChapterCaptureIndex)
	{
	case 0:
		// The only warm light left in the hall is the impossible open 403.
		// Do not fire the room's scare during a documentation tour; its
		// 18-second stress hold would contaminate the following lobby frame.
		if (MirrorSightZone)
		{
			MirrorSightZone->SetActorEnableCollision(false);
		}
		if (MirrorEntryZone)
		{
			MirrorEntryZone->SetActorEnableCollision(false);
		}
		for (int32 FixtureIndex = 0; FixtureIndex < GetCorridorFixtureCount(); ++FixtureIndex)
		{
			SetFixtureLive(FixtureIndex, false, true);
		}
		if (MirrorRoomLamp)
		{
			MirrorRoomLamp->SetVisibility(true);
			MirrorRoomLamp->SetIntensity(560.0f);
		}
		if (MirrorRoomDoor)
		{
			MirrorRoomDoor->ForceOpenState(true);
		}
		PlaceCaptureCamera(
			FVector(430, -120, 1005),
			FVector(315, 105, 970));
		BaseName = TEXT("ch02-mirror-room");
		break;

	case 1:
		// Quarter-lit lobby, wet prints and the offering at the threshold.
		RevealElevatorFootprints();
		if (ChapterTwoReturnZone)
		{
			ChapterTwoReturnZone->SetActorEnableCollision(false);
		}
		if (BuildingDoor)
		{
			BuildingDoor->ForceOpenState(true);
			BuildingDoor->SetInteractionEnabled(false);
			BuildingDoor->SetActorHiddenInGame(true);
			BuildingDoor->SetActorEnableCollision(false);
		}
		if (OfferingLight)
		{
			OfferingLight->SetVisibility(true);
			OfferingLight->SetIntensity(235.0f);
		}
		PlaceCaptureCamera(
			// Frame the bowl at the end of the salt line, keeping the hinge
			// post to one side while the wet steps remain visible inside.
			FVector(735, -535, 88),
			FVector(665, -407, 10));
		BaseName = TEXT("ch02-lobby-offering");
		break;

	case 2:
		// The note panel is the actual in-game document UI, not a mock-up.
		PlaceCaptureCamera(
			FVector(2700, -390, 96),
			FVector(2582, -252, 100));
		if (ExistingReceipt)
		{
			FIGInteractionContext Context;
			Context.Interactor = PlayerPawn;
			Context.TargetActor = ExistingReceipt;
			Context.HoldProgress = 1.0f;
			IIGInteractable::Execute_CompleteInteraction(ExistingReceipt, Context);
		}
		BaseName = TEXT("ch02-receipt-0444");
		break;

	default:
		FinishChapterTwoCaptureSequence();
		return;
	}

	const FString ScreenshotPath = FPaths::ConvertRelativePathToFull(FPaths::Combine(
		FPaths::ProjectDir(),
		FString::Printf(TEXT("Docs/Media/%s.png"), *BaseName)));
	++ChapterCaptureIndex;

	// Let the teleported camera settle so temporal history cannot smear the
	// documentation frame. This is especially visible on the long 4F->lobby
	// move between the first two captures.
	FTimerDelegate CaptureDelegate;
	CaptureDelegate.BindLambda([this, ScreenshotPath]()
	{
		if (!IsValid(this))
		{
			return;
		}

		FScreenshotRequest::RequestScreenshot(ScreenshotPath, true, false);
		UE_LOG(
			LogIndieGame,
			Display,
			TEXT("CH02 capture requested: %s"),
			*ScreenshotPath);

		GetWorldTimerManager().SetTimer(
			ChapterCaptureHandle,
			this,
			&ThisClass::CaptureNextChapterTwoFrame,
			6.0f,
			false);
	});
	GetWorldTimerManager().SetTimer(
		ChapterCaptureHandle,
		CaptureDelegate,
		0.75f,
		false);
}

void AIGPrologueWorldScene::FinishChapterTwoCaptureSequence()
{
	if (AIGReadableNote* OpenNote = AIGReadableNote::GetOpenNote())
	{
		OpenNote->Close();
	}

	// The three stills also serve as a deterministic CH02 smoke run. Exercise
	// the complete intermediate-stop state machine before exiting so a visual
	// capture cannot pass while the chapter's one-off elevator ride is broken.
	UWorld* World = GetWorld();
	APlayerController* PlayerController = World ? World->GetFirstPlayerController() : nullptr;
	APawn* PlayerPawn = PlayerController ? PlayerController->GetPawn() : nullptr;
	if (!Elevator || !PlayerPawn)
	{
		UE_LOG(LogIndieGame, Error, TEXT("CH02 elevator validation could not start."));
		FPlatformMisc::RequestExit(false);
		return;
	}

	Elevator->ResetForNewRide();
	Elevator->ConfigureIntermediateStop(true, 12.0f);
	PlayerPawn->SetActorLocation(
		FVector(625, -305, IGPrologueWorld::FourthFloorZ),
		false,
		nullptr,
		ETeleportType::TeleportPhysics);

	FIGInteractionContext Context;
	Context.Interactor = PlayerPawn;
	Context.TargetActor = Elevator;
	Context.HoldProgress = 1.0f;
	IIGInteractable::Execute_CompleteInteraction(Elevator, Context);

	const TWeakObjectPtr<APawn> WeakPlayerPawn = PlayerPawn;
	FTimerDelegate EnterCabDelegate;
	EnterCabDelegate.BindLambda([WeakPlayerPawn]()
	{
		if (APawn* Pawn = WeakPlayerPawn.Get())
		{
			Pawn->SetActorLocation(
				FVector(795, -305, IGPrologueWorld::FourthFloorZ),
				false,
				nullptr,
				ETeleportType::TeleportPhysics);
		}
	});
	FTimerHandle EnterCabHandle;
	GetWorldTimerManager().SetTimer(EnterCabHandle, EnterCabDelegate, 1.35f, false);

	FTimerDelegate ValidationDelegate;
	ValidationDelegate.BindLambda([this]()
	{
		const bool bRideCompleted = Elevator && Elevator->IsRideComplete();
		if (bRideCompleted)
		{
			UE_LOG(
				LogIndieGame,
				Display,
				TEXT("CH02 capture and elevator validation complete."));
		}
		else
		{
			UE_LOG(
				LogIndieGame,
				Error,
				TEXT("CH02 capture and elevator validation failed."));
		}
		FPlatformMisc::RequestExit(false);
	});
	GetWorldTimerManager().SetTimer(
		ChapterCaptureHandle,
		ValidationDelegate,
		18.0f,
		false);
	UE_LOG(LogIndieGame, Display, TEXT("CH02 elevator validation started."));
}

void AIGPrologueWorldScene::HandleFlickerZoneTriggered(AIGZoneTrigger* Zone)
{
	if (MorningDirector)
	{
		MorningDirector->TriggerAlleyLightFailure();
	}
}
