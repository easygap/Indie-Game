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
#include "EngineUtils.h"
#include "Environment/IGNeighborhoodLifeDirector.h"
#include "GameFramework/HUD.h"
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
#include "Interaction/IGStairTransition.h"
#include "Interaction/IGSwingDoor.h"
#include "Interaction/IGZoneTrigger.h"
#include "Materials/MaterialInterface.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"
#include "Narrative/IGApartmentStoryDressing.h"
#include "Narrative/IGItemContinuityDressing.h"
#include "Narrative/IGRebirthNarrativeSubsystem.h"
#include "Narrative/IGStoryHelpers.h"
#include "Narrative/IGStoryStateSubsystem.h"
#include "Player/IGHorrorHUD.h"
#include "Sequence/IGDemoDirector.h"
#include "Sequence/IGChapterOneIncidentDirector.h"
#include "Sequence/IGChapterTwoHumanGateDirector.h"
#include "Sequence/IGMorningRoutineDirector.h"
#include "Sequence/IGSecondMorningDirector.h"
#include "Sequence/IGThirdMorningDirector.h"
#include "Player/IGStressComponent.h"
#include "Save/IGSaveGame.h"
#include "Save/IGSaveSubsystem.h"
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

	const FName PurchaseBagProxyTag(TEXT("REBIRTH.PurchaseBagProxy"));

	enum class EReceiptTimeline : uint8
	{
		ActualPurchase0431,
		DeathOverlay0444
	};

	struct FPurchaseProfileSpec
	{
		FText ProductName;
		int32 Quantity = 1;
		int32 UnitPrice = 0;
		FString ReceiptNumber;
		FString ApprovalNumber;
		FString BarcodeDigits;
		FVector BagSize = FVector(12.0f, 18.0f, 20.0f);
		float BagCenterY = 0.0f;
		float BagHandleHeight = 8.0f;
	};

	FPurchaseProfileSpec GetPurchaseProfileSpec(
		const EIGRebirthPurchaseProfile PurchaseProfile)
	{
		FPurchaseProfileSpec Spec;
		switch (PurchaseProfile)
		{
		case EIGRebirthPurchaseProfile::ProfileB1LX1:
			Spec.ProductName =
				NSLOCTEXT("IGReceipt", "ProfileBProduct", "한강수 1L");
			Spec.Quantity = 1;
			Spec.UnitPrice = 1500;
			Spec.ReceiptNumber = TEXT("31858");
			Spec.ApprovalNumber = TEXT("82716391");
			Spec.BarcodeDigits = TEXT("2903185815002");
			Spec.BagSize = FVector(13.0f, 12.0f, 28.0f);
			Spec.BagCenterY = 0.0f;
			Spec.BagHandleHeight = 10.0f;
			break;
		case EIGRebirthPurchaseProfile::ProfileC2LX2:
			Spec.ProductName =
				NSLOCTEXT("IGReceipt", "ProfileCProduct", "맑은산 2L");
			Spec.Quantity = 2;
			Spec.UnitPrice = 2000;
			Spec.ReceiptNumber = TEXT("31859");
			Spec.ApprovalNumber = TEXT("82716392");
			Spec.BarcodeDigits = TEXT("2903185940003");
			Spec.BagSize = FVector(16.0f, 25.0f, 31.0f);
			Spec.BagCenterY = 5.5f;
			Spec.BagHandleHeight = 15.0f;
			break;
		case EIGRebirthPurchaseProfile::ProfileA500MlX2:
		case EIGRebirthPurchaseProfile::Unset:
		default:
			Spec.ProductName =
				NSLOCTEXT("IGReceipt", "ProfileAProduct", "새벽샘물 500mL");
			Spec.Quantity = 2;
			Spec.UnitPrice = 1000;
			Spec.ReceiptNumber = TEXT("31857");
			Spec.ApprovalNumber = TEXT("82716390");
			Spec.BarcodeDigits = TEXT("2903185720001");
			Spec.BagSize = FVector(12.0f, 18.0f, 20.0f);
			Spec.BagCenterY = 4.0f;
			Spec.BagHandleHeight = 8.0f;
			break;
		}
		return Spec;
	}

	FText FormatWon(const int32 Amount)
	{
		return FText::Format(
			NSLOCTEXT("IGReceipt", "WonFormat", "{0}원"),
			FText::AsNumber(Amount));
	}

	FText GetReceiptDateTime(const EReceiptTimeline Timeline)
	{
		return Timeline == EReceiptTimeline::ActualPurchase0431
			? NSLOCTEXT(
				"IGReceipt",
				"ActualPurchaseDateTime",
				"2024/07/26(금) 04:31")
			: NSLOCTEXT(
				"IGReceipt",
				"DeathOverlayDateTime",
				"2024/07/26(금) 04:44");
	}

	TArray<FText> BuildPurchaseReceiptSummary(
		const EIGRebirthPurchaseProfile PurchaseProfile,
		const EReceiptTimeline Timeline)
	{
		const FPurchaseProfileSpec Spec =
			GetPurchaseProfileSpec(PurchaseProfile);
		const int32 Total = Spec.Quantity * Spec.UnitPrice;
		return {
			FText::Format(
				NSLOCTEXT("IGReceipt", "SummaryDate", "{0} POS-01"),
				GetReceiptDateTime(Timeline)),
			FText::Format(
				NSLOCTEXT(
					"IGReceipt",
					"SummaryItem",
					"{0} / {1} / {2}"),
				Spec.ProductName,
				FText::AsNumber(Spec.Quantity),
				FormatWon(Total)),
			FText::Format(
				NSLOCTEXT(
					"IGReceipt",
					"SummaryApproval",
					"체크카드 승인 {0}"),
				FText::FromString(Spec.ApprovalNumber)),
			FText::Format(
				NSLOCTEXT("IGReceipt", "SummaryNumber", "거래NO. {0}"),
				FText::FromString(Spec.ReceiptNumber)),
		};
	}

	FIGThermalReceiptData BuildPurchaseReceiptData(
		const EIGRebirthPurchaseProfile PurchaseProfile,
		const EReceiptTimeline Timeline)
	{
		const FPurchaseProfileSpec Spec =
			GetPurchaseProfileSpec(PurchaseProfile);
		const int32 Total = Spec.Quantity * Spec.UnitPrice;

		FIGThermalReceiptData Data;
		Data.StoreName =
			NSLOCTEXT("IGReceipt", "Store", "새벽24");
		Data.StoreSubtitle =
			NSLOCTEXT("IGReceipt", "StoreSubtitle", "무영로점  ·  24 HOURS");
		Data.StoreDetailLines = {
			NSLOCTEXT("IGReceipt", "Branch", "새벽24 무영로점"),
			// Fictional identifiers use plausible Korean formatting without
			// repeating 4s or pointing at an actual business.
			NSLOCTEXT(
				"IGReceipt",
				"Business",
				"사업자등록번호 110-81-32765"),
			NSLOCTEXT("IGReceipt", "Owner", "대표 박해원"),
			NSLOCTEXT(
				"IGReceipt",
				"Address",
				"주소 서울 은평구 무영로17길 8"),
			NSLOCTEXT("IGReceipt", "Telephone", "TEL 02-3157-0826"),
		};
		Data.PolicyLines = {
			NSLOCTEXT("IGReceipt", "Policy1", "교환/환불은 구입 후 30일 이내"),
			NSLOCTEXT("IGReceipt", "Policy2", "영수증과 결제카드를 지참해 주세요."),
			NSLOCTEXT("IGReceipt", "Policy3", "일부 행사·신선식품은 제외됩니다."),
		};
		Data.TransactionDateTime = GetReceiptDateTime(Timeline);
		Data.PosLabel =
			NSLOCTEXT("IGReceipt", "Pos", "POS-01");
		Data.ReceiptNumber = FText::FromString(Spec.ReceiptNumber);

		FIGReceiptItemLine WaterItem;
		WaterItem.ProductName = Spec.ProductName;
		WaterItem.Quantity = Spec.Quantity;
		WaterItem.UnitPrice = Spec.UnitPrice;
		WaterItem.Amount = Total;
		Data.Items.Add(MoveTemp(WaterItem));

		Data.Subtotal = Total;
		Data.Vat = FMath::RoundToInt(static_cast<float>(Total) / 11.0f);
		Data.TaxableSupply = Total - Data.Vat;
		Data.Total = Total;
		Data.PaymentHeading =
			NSLOCTEXT(
				"IGReceipt",
				"PaymentHeading",
				"******** 체크카드(일시불) ********");

		FIGReceiptKeyValueLine CardLine;
		CardLine.Label =
			NSLOCTEXT("IGReceipt", "CardLabel", "카드번호");
		CardLine.Value =
			NSLOCTEXT("IGReceipt", "CardValue", "5417-****-****-2719");
		Data.PaymentLines.Add(MoveTemp(CardLine));

		FIGReceiptKeyValueLine AcquirerLine;
		AcquirerLine.Label =
			NSLOCTEXT("IGReceipt", "AcquirerLabel", "매입사");
		AcquirerLine.Value =
			NSLOCTEXT("IGReceipt", "AcquirerValue", "해온카드");
		Data.PaymentLines.Add(MoveTemp(AcquirerLine));

		FIGReceiptKeyValueLine ApprovalLine;
		ApprovalLine.Label =
			NSLOCTEXT("IGReceipt", "ApprovalLabel", "승인번호");
		ApprovalLine.Value = FText::FromString(Spec.ApprovalNumber);
		Data.PaymentLines.Add(MoveTemp(ApprovalLine));

		FIGReceiptKeyValueLine PaymentAmountLine;
		PaymentAmountLine.Label =
			NSLOCTEXT("IGReceipt", "PaymentAmountLabel", "결제금액");
		PaymentAmountLine.Value = FormatWon(Total);
		Data.PaymentLines.Add(MoveTemp(PaymentAmountLine));

		FIGReceiptKeyValueLine InstallmentLine;
		InstallmentLine.Label =
			NSLOCTEXT("IGReceipt", "InstallmentLabel", "할부");
		InstallmentLine.Value =
			NSLOCTEXT("IGReceipt", "InstallmentValue", "일시불");
		Data.PaymentLines.Add(MoveTemp(InstallmentLine));

		Data.FooterLines = {
			NSLOCTEXT("IGReceipt", "FooterThanks", "이용해 주셔서 감사합니다."),
			FText::Format(
				NSLOCTEXT(
					"IGReceipt",
					"FooterClerk",
					"담당:07  거래NO:{0}  재출력:0"),
				FText::FromString(Spec.ReceiptNumber)),
			NSLOCTEXT(
				"IGReceipt",
				"FooterService",
				"고객센터 080-315-0726"),
		};
		// These EAN-13-like digits intentionally have invalid check digits.
		Data.BarcodeDigits = Spec.BarcodeDigits;
		return Data;
	}

	FText BuildCheckoutPrompt(
		const EIGRebirthPurchaseProfile PurchaseProfile,
		const bool bWalletInHand)
	{
		const FPurchaseProfileSpec Spec =
			GetPurchaseProfileSpec(PurchaseProfile);
		return FText::Format(
			bWalletInHand
				? NSLOCTEXT(
					"IGReceipt",
					"CheckoutPromptWallet",
					"{0} 결제하기 ({1})")
				: NSLOCTEXT(
					"IGReceipt",
					"CheckoutPromptPocket",
					"{0} 후드 주머니 카드로 결제하기 ({1})"),
			Spec.ProductName,
			FormatWon(Spec.Quantity * Spec.UnitPrice));
	}
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
	// These values are EV100 because extended luminance range is enabled.
	// The old -1.2..1.1 clamp was below the actual fluorescent/store range,
	// so the camera could not stop down and white fixtures clipped. Retain a
	// bounded adaptation range, but let bright practicals reach a sane EV.
	PostProcess->Settings.AutoExposureMinBrightness = -0.5f;
	PostProcess->Settings.AutoExposureMaxBrightness = 5.0f;
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
	if (SizeCentimeters.GetMin() < 3.0f || Material == GlassMaterial)
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
	Prop->SetUseCCD(true);
	Prop->SetPhysicsMaxAngularVelocityInDegrees(720.0f);
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

void AIGPrologueWorldScene::AddStaticPurchaseBagProxy(
	AIGPickupItem* WaterBottle,
	const EIGRebirthPurchaseProfile PurchaseProfile)
{
	if (!WaterBottle || !CubeMesh)
	{
		return;
	}

	UStaticMeshComponent* BottleRoot = WaterBottle->GetMeshComponent();
	if (!BottleRoot)
	{
		return;
	}

	const IGPrologueWorld::FPurchaseProfileSpec Spec =
		IGPrologueWorld::GetPurchaseProfileSpec(PurchaseProfile);
	const FVector RootScale = BottleRoot->GetRelativeScale3D().GetAbs();
	const FVector SafeRootScale(
		FMath::Max(RootScale.X, KINDA_SMALL_NUMBER),
		FMath::Max(RootScale.Y, KINDA_SMALL_NUMBER),
		FMath::Max(RootScale.Z, KINDA_SMALL_NUMBER));
	const auto DivideByRootScale = [SafeRootScale](const FVector& Value)
	{
		return FVector(
			Value.X / SafeRootScale.X,
			Value.Y / SafeRootScale.Y,
			Value.Z / SafeRootScale.Z);
	};
	const auto AddProxyPart =
		[this, BottleRoot, &DivideByRootScale](
			UMaterialInterface* Material,
			const FVector& ActorSpaceLocation,
			const FVector& ActorSpaceSize)
		{
			UStaticMeshComponent* Part = CreateDecoOnComponent(
				BottleRoot,
				CubeMesh,
				Material,
				DivideByRootScale(ActorSpaceLocation),
				FRotator::ZeroRotator,
				DivideByRootScale(ActorSpaceSize / 100.0f));
			if (Part)
			{
				Part->ComponentTags.AddUnique(
					IGPrologueWorld::PurchaseBagProxyTag);
				Part->SetVisibility(false, true);
				Part->SetCastShadow(false);
			}
			return Part;
		};

	const float Depth = Spec.BagSize.X;
	const float Width = Spec.BagSize.Y;
	const float Height = Spec.BagSize.Z;
	const float CenterY = Spec.BagCenterY;
	const float PanelThickness = 0.35f;
	UMaterialInterface* BagFilmMaterial =
		TexMat(TEXT("M_CarrierBagFilm"), GlassMaterial);

	// Five translucent sheets preserve the selected bottles while giving the
	// carried actor a readable convenience-store-bag silhouette.
	AddProxyPart(
		BagFilmMaterial,
		FVector(Depth * 0.5f, CenterY, Height * 0.5f),
		FVector(PanelThickness, Width, Height));
	AddProxyPart(
		BagFilmMaterial,
		FVector(-Depth * 0.5f, CenterY, Height * 0.5f),
		FVector(PanelThickness, Width, Height));
	AddProxyPart(
		BagFilmMaterial,
		FVector(0.0f, CenterY + Width * 0.5f, Height * 0.5f),
		FVector(Depth, PanelThickness, Height));
	AddProxyPart(
		BagFilmMaterial,
		FVector(0.0f, CenterY - Width * 0.5f, Height * 0.5f),
		FVector(Depth, PanelThickness, Height));
	AddProxyPart(
		BagFilmMaterial,
		FVector(0.0f, CenterY, PanelThickness * 0.5f),
		FVector(Depth, Width, PanelThickness));

	const float HandleHalfWidth = Width * 0.29f;
	const float HandleCenterZ = Height + Spec.BagHandleHeight * 0.5f;
	for (const float HandleY :
		{CenterY - HandleHalfWidth, CenterY + HandleHalfWidth})
	{
		AddProxyPart(
			SignWhiteMaterial,
			FVector(0.0f, HandleY, HandleCenterZ),
			FVector(0.9f, 0.8f, Spec.BagHandleHeight));
	}
	AddProxyPart(
		SignWhiteMaterial,
		FVector(0.0f, CenterY, Height + Spec.BagHandleHeight),
		FVector(0.9f, HandleHalfWidth * 2.0f, 0.8f));
}

void AIGPrologueWorldScene::RefreshPurchaseProfilePresentation()
{
	EIGRebirthPurchaseProfile PurchaseProfile =
		EIGRebirthPurchaseProfile::ProfileA500MlX2;
	if (const UGameInstance* GameInstance = GetGameInstance())
	{
		if (const UIGRebirthNarrativeSubsystem* RebirthState =
			GameInstance->GetSubsystem<UIGRebirthNarrativeSubsystem>())
		{
			const EIGRebirthPurchaseProfile SavedProfile =
				RebirthState->GetChoices().PurchaseProfile;
			if (SavedProfile != EIGRebirthPurchaseProfile::Unset)
			{
				PurchaseProfile = SavedProfile;
			}
		}
	}

	const auto ConfigureReceipt =
		[PurchaseProfile](
			AIGReadableNote* Receipt,
			const IGPrologueWorld::EReceiptTimeline Timeline)
		{
			if (!Receipt)
			{
				return;
			}
			Receipt->SetNoteText(
				NSLOCTEXT("IGReceipt", "ReceiptTitle", "영수증"),
				IGPrologueWorld::BuildPurchaseReceiptSummary(
					PurchaseProfile,
					Timeline));
			Receipt->SetThermalReceiptData(
				IGPrologueWorld::BuildPurchaseReceiptData(
					PurchaseProfile,
					Timeline));
		};

	ConfigureReceipt(
		ChapterOneReceipt,
		IGPrologueWorld::EReceiptTimeline::ActualPurchase0431);
	ConfigureReceipt(
		ExistingReceipt,
		IGPrologueWorld::EReceiptTimeline::DeathOverlay0444);
	ConfigureReceipt(
		DuplicateReceipt,
		IGPrologueWorld::EReceiptTimeline::DeathOverlay0444);
	if (DuplicateReceipt)
	{
		// The functional P2 proxy must put both observed values on the paper.
		// Without this line Ji-un's follow-up thought would know 04:31 even
		// when the player skipped the optional CH01 receipt.
		FIGThermalReceiptData Comparison =
			IGPrologueWorld::BuildPurchaseReceiptData(
				PurchaseProfile,
				IGPrologueWorld::EReceiptTimeline::DeathOverlay0444);
		Comparison.StoreDetailLines.Insert(
			NSLOCTEXT(
				"IGReceipt",
				"DuplicateComparisonTimes",
				"[대조] 원거래 04:31 / 현재 04:44 중복"),
			0);
		DuplicateReceipt->SetThermalReceiptData(MoveTemp(Comparison));
	}

	if (Checkout && !bChapterTwoActive)
	{
		const bool bWalletInHand = IGStory::HasState(
			this,
			FGameplayTag::RequestGameplayTag(
				FName(TEXT("State.CH01.Morning.HasWallet")),
				false));
		Checkout->SetInteractionPrompt(
			IGPrologueWorld::BuildCheckoutPrompt(
				PurchaseProfile,
				bWalletInHand));
	}

	const FGameplayTag PurchaseTag = FGameplayTag::RequestGameplayTag(
		FName(
			bChapterTwoActive
				? TEXT("State.CH02.Loop.WaterPurchased")
				: TEXT("State.CH01.Morning.WaterPurchased")),
		false);
	const bool bPurchaseCommitted = IGStory::HasState(this, PurchaseTag);
	const TArray<TObjectPtr<AIGPickupItem>>& ActiveBottles =
		bChapterTwoActive ? ChapterTwoWaterBottles : WaterBottles;
	for (AIGPickupItem* WaterBottle : ActiveBottles)
	{
		if (!WaterBottle)
		{
			continue;
		}
		const bool bShowBag =
			bPurchaseCommitted
			&& WaterBottle->RebirthPurchaseProfileOnPickup == PurchaseProfile;
		TArray<UStaticMeshComponent*> Components;
		WaterBottle->GetComponents<UStaticMeshComponent>(Components);
		for (UStaticMeshComponent* Component : Components)
		{
			if (Component
				&& Component->ComponentTags.Contains(
					IGPrologueWorld::PurchaseBagProxyTag))
			{
				Component->SetVisibility(bShowBag, true);
			}
		}
	}
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
		TEXT("M_CarrierBagFilm"), TEXT("M_WetHoodieUV"),
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
	SpawnStairTransition();

	// These ordinary possessions are persistent anchors: CH02 repeats the
	// same desk and entryway instead of spawning its clues into view.
	FActorSpawnParameters StoryDressingParameters;
	StoryDressingParameters.Owner = this;
	StoryDressingParameters.SpawnCollisionHandlingOverride =
		ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	const FTransform StoryDressingTransform =
		FTransform(FRotator::ZeroRotator, FVector(0.0f, 0.0f, IGPrologueWorld::FourthFloorZ))
		* GetActorTransform();
	ApartmentStoryDressing = GetWorld()->SpawnActor<AIGApartmentStoryDressing>(
		AIGApartmentStoryDressing::StaticClass(),
		StoryDressingTransform,
		StoryDressingParameters);
	if (ApartmentStoryDressing)
	{
		ApartmentStoryDressing->ConfigurePrototypeVisuals(
			CubeMesh,
			PropMesh(TEXT("SM_CrackedPhone"), CubeMesh),
			TexMat(TEXT("M_CarrierBagFilm"), GlassMaterial),
			PlasticDarkMaterial,
			SnackBlueMaterial,
			SignMintMaterial,
			SignWhiteMaterial,
			ScreenGlowMaterial,
			TexMat(TEXT("M_NoteFridge"), SignWhiteMaterial),
			Fridge ? Fridge->GetDoorPivot() : nullptr);
	}

	SpawnChapterTwoInteractables();
	RefreshPurchaseProfilePresentation();
	SpawnDirectors();
	CreateAmbience();

	// Ordinary street life is a narrative baseline, not decoration. CH01
	// establishes a car, a delivery motorcycle, wind-blown leaves and a
	// peripheral stray cat; later chapters can vary or remove that grammar
	// without allocating a second set of actors.
	FActorSpawnParameters NeighborhoodParameters;
	NeighborhoodParameters.Owner = this;
	NeighborhoodParameters.SpawnCollisionHandlingOverride =
		ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	NeighborhoodLifeDirector = GetWorld()->SpawnActor<AIGNeighborhoodLifeDirector>(
		AIGNeighborhoodLifeDirector::StaticClass(),
		FTransform::Identity,
		NeighborhoodParameters);
	if (NeighborhoodLifeDirector)
	{
		NeighborhoodLifeDirector->ConfigureNeighborhood(
			GetActorTransform().TransformPosition(FVector(-360.0f, -555.0f, -5.0f)),
			GetActorTransform().TransformPosition(FVector(2390.0f, -555.0f, -5.0f)),
			4040444);
	}

	const bool bIgnoreDirectStart =
		GetWorld()->URL.HasOption(TEXT("IGIgnoreDirectStart"));
	const bool bDirectChapterThree = !bIgnoreDirectStart
		&& (FParse::Param(FCommandLine::Get(), TEXT("IGChapterThree"))
			|| FParse::Param(FCommandLine::Get(), TEXT("IGCaptureCH03"))
			|| GetWorld()->URL.HasOption(TEXT("IGChapterThree")));
	if (bDirectChapterThree)
	{
		EnterChapterThree();
	}
	else if (!bIgnoreDirectStart
		&& (FParse::Param(FCommandLine::Get(), TEXT("IGChapterTwo"))
			|| FParse::Param(FCommandLine::Get(), TEXT("IGCaptureCH02"))
			|| GetWorld()->URL.HasOption(TEXT("IGChapterTwo"))))
	{
		EnterChapterTwo();
	}
	else
	{
		SpawnDemoDirectorIfRequested();
	}
	bRebirthEndToEndValidation =
		FParse::Param(
			FCommandLine::Get(),
			TEXT("IGRebirthEndToEndValidation"));
	if (bRebirthEndToEndValidation)
	{
		if (bDirectChapterThree || bChapterTwoActive)
		{
			FailRebirthEndToEndValidation(
				TEXT("end-to-end validation must start from CH01"));
			return;
		}
		GetWorldTimerManager().SetTimer(
			RebirthEndToEndHandle,
			this,
			&ThisClass::StartRebirthEndToEndValidation,
			0.25f,
			false);
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

	ReconcileLoadedCheckpoint();
	UE_LOG(LogIndieGame, Display, TEXT("Prologue world ready: apartment, alley and store assembled."));
}

void AIGPrologueWorldScene::ReconcileLoadedCheckpoint()
{
	UWorld* World = GetWorld();
	if (!World
		|| !World->URL.HasOption(TEXT("IGResumeSave"))
		|| bChapterTwoActive
		|| bChapterThreeActive)
	{
		return;
	}

	const UGameInstance* GameInstance = GetGameInstance();
	const UIGSaveSubsystem* SaveSubsystem = GameInstance
		? GameInstance->GetSubsystem<UIGSaveSubsystem>()
		: nullptr;
	const UIGSaveGame* LoadedSave =
		SaveSubsystem ? SaveSubsystem->GetLastLoadedSave() : nullptr;
	if (!LoadedSave)
	{
		return;
	}

	const FGameplayTag ChapterOne = FGameplayTag::RequestGameplayTag(
		FName(TEXT("Chapter.CH01")),
		false);
	if (!LoadedSave->Progress.ChapterId.MatchesTagExact(ChapterOne))
	{
		return;
	}

	const FGameplayTag Checkpoint = LoadedSave->Progress.CheckpointTag;
	const auto IsCheckpoint = [&Checkpoint](const TCHAR* TagName)
	{
		return Checkpoint.MatchesTagExact(FGameplayTag::RequestGameplayTag(
			FName(TagName),
			false));
	};
	const bool bAtStore =
		IsCheckpoint(TEXT("Checkpoint.CH01.StoreEntrance"))
		|| IsCheckpoint(TEXT("Checkpoint.CH01.WaterPurchased"));
	const bool bOnReturnWalk =
		IsCheckpoint(TEXT("Checkpoint.CH01.CatApproach"))
		|| IsCheckpoint(TEXT("Checkpoint.CH01.CatSpot"))
		|| IsCheckpoint(TEXT("Checkpoint.CH01.ReturnAlley"));
	const bool bAtLobby =
		IsCheckpoint(TEXT("Checkpoint.CH01.Lobby"));
	const bool bAtFourthFloor =
		IsCheckpoint(TEXT("Checkpoint.CH01.FourthFloor"));
	const bool bOutside =
		bAtStore
		|| bOnReturnWalk
		|| bAtLobby
		|| IsCheckpoint(TEXT("Checkpoint.CH01.AlleyEntrance"));

	FVector SafeLocation = IGPrologueWorld::PlayerLocation;
	FRotator SafeActorRotation = IGPrologueWorld::PlayerActorRotation;
	FRotator SafeViewRotation = IGPrologueWorld::PlayerViewRotation;
	if (IsCheckpoint(TEXT("Checkpoint.CH01.BedroomDoor")))
	{
		SafeLocation = FVector(48.0f, -155.0f, 997.0f);
		SafeActorRotation = FRotator(0.0f, -90.0f, 0.0f);
		SafeViewRotation = FRotator(-4.0f, -90.0f, 0.0f);
	}
	else if (IsCheckpoint(TEXT("Checkpoint.CH01.FridgeChecked")))
	{
		SafeLocation = FVector(75.0f, 15.0f, 997.0f);
		SafeActorRotation = FRotator(0.0f, 0.0f, 0.0f);
		SafeViewRotation = FRotator(-4.0f, 0.0f, 0.0f);
	}
	else if (IsCheckpoint(TEXT("Checkpoint.CH01.AlleyEntrance")))
	{
		SafeLocation = FVector(705.0f, -445.0f, 110.0f);
		SafeActorRotation = FRotator(0.0f, 0.0f, 0.0f);
		SafeViewRotation = FRotator(-4.0f, 0.0f, 0.0f);
	}
	else if (bAtStore)
	{
		SafeLocation = IsCheckpoint(TEXT("Checkpoint.CH01.WaterPurchased"))
			? FVector(2525.0f, -330.0f, 110.0f)
			: FVector(2505.0f, -455.0f, 110.0f);
		SafeActorRotation = FRotator(0.0f, -90.0f, 0.0f);
		SafeViewRotation = FRotator(-4.0f, -90.0f, 0.0f);
	}
	else if (bOnReturnWalk)
	{
		SafeLocation = IsCheckpoint(TEXT("Checkpoint.CH01.CatApproach"))
			? FVector(1550.0f, -515.0f, 110.0f)
			: IsCheckpoint(TEXT("Checkpoint.CH01.CatSpot"))
				? FVector(1320.0f, -515.0f, 110.0f)
				: FVector(1005.0f, -515.0f, 110.0f);
		SafeActorRotation = FRotator(0.0f, 180.0f, 0.0f);
		SafeViewRotation = FRotator(-4.0f, 180.0f, 0.0f);
	}
	else if (bAtLobby)
	{
		SafeLocation = FVector(585.0f, -360.0f, 110.0f);
		SafeActorRotation = FRotator(0.0f, 180.0f, 0.0f);
		SafeViewRotation = FRotator(-4.0f, 180.0f, 0.0f);
	}
	else if (bAtFourthFloor)
	{
		SafeLocation = FVector(-80.0f, -305.0f, 1010.0f);
		SafeActorRotation = FRotator(0.0f, 180.0f, 0.0f);
		SafeViewRotation = FRotator(-4.0f, 180.0f, 0.0f);
	}

	APlayerController* PlayerController = World->GetFirstPlayerController();
	APawn* PlayerPawn = PlayerController ? PlayerController->GetPawn() : nullptr;
	if (PlayerPawn)
	{
		PlayerPawn->SetActorLocationAndRotation(
			SafeLocation,
			SafeActorRotation,
			false,
			nullptr,
			ETeleportType::TeleportPhysics);
	}
	if (PlayerController)
	{
		PlayerController->SetControlRotation(SafeViewRotation);
		if (APlayerCameraManager* Camera = PlayerController->PlayerCameraManager)
		{
			Camera->StopCameraFade();
		}
	}
	if (WakeDirector)
	{
		WakeDirector->RestoreStandingCheckpoint();
	}
	if (bOutside && Elevator)
	{
		Elevator->RestoreAtLobbyOpen();
	}

	const FGameplayTag PurchaseTag = FGameplayTag::RequestGameplayTag(
		FName(TEXT("State.CH01.Morning.WaterPurchased")),
		false);
	if (IGStory::HasState(this, PurchaseTag))
	{
		// The live change event happened before this newly built world existed.
		// Recreate the receipt and homeward boundary without replaying feedback.
		HandleStoryStateChanged(PurchaseTag, true);
	}

	UE_LOG(
		LogIndieGame,
		Display,
		TEXT("CH01 checkpoint reconciled at %s (%s)."),
		*SafeLocation.ToCompactString(),
		*Checkpoint.ToString());
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
	// The east wall projects ten centimetres past the south wall into the
	// corridor. Its exposed end is a Y-facing surface, so the wall's X-facing
	// material stretched into dense horizontal bands at the authored corridor
	// camera. Cap that return with the correctly oriented wallpaper material.
	CreateBlock(FVector(200, -245.6f, 115), FVector(20, 0.8f, 230), WallX, false);
	CreateBlock(FVector(0, 225, 115), FVector(440, 20, 230), WallX);
	// South wall with the entrance opening (X 98..186).
	CreateBlock(FVector(-61, -225, 115), FVector(318, 20, 230), WallX);
	CreateBlock(FVector(203, -225, 115), FVector(34, 20, 230), WallX);
	// A 210 cm clear opening leaves headroom above the 8 cm Korean shoe step.
	// The former 200 cm soffit exactly touched the 192 cm player capsule once
	// the pawn stood on the step, making the doorway look open but impassable.
	CreateBlock(FVector(142, -225, 220), FVector(88, 20, 20), WallX);
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
	// Hang the calendar on the clear wall directly above the desk. The old
	// west-wall position overlapped the wardrobe/bedside-table sightline and
	// made a normal wall calendar look wedged behind furniture.
	CreateBlock(
		FVector(-140, -213.2f, 154), FVector(1.5f, 31, 42),
		TexMat(TEXT("M_Calendar"), SignWhiteMaterial), false,
		nullptr, FRotator(0, 90, 0));
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
	// Thin cube UVs smear into horizontal bands on a tall jamb. The flat
	// powder-coated metal fallback reads like a Korean steel-door casing.
	UMaterialInterface* DoorTrim = PlasticDarkMaterial;
	// Directional stone/metal UVs smear across the 1–2 cm portal strips. A
	// matte charcoal powder coat is common on renovated Korean villa lifts and
	// stays visually stable on every thin reveal face.
	UMaterialInterface* LiftStone = PlasticDarkMaterial;

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
	// World-mapped stone portal and a live hall indicator keep the lift
	// opening distinct from the stucco without changing its collision.
	for (const float PortalY : {-245.0f, -365.0f})
	{
		CreateBlock(
			FVector(699, PortalY, 105), FVector(2, 10, 210),
			LiftStone, false);
	}
	// Cover the two side reveals and soffit as well as the corridor-facing
	// portal. Otherwise the directional stucco on a 20 cm return stretches
	// into conspicuous horizontal bands when the player looks into the car.
	CreateBlock(FVector(710, -250.5f, 105), FVector(20, 1, 210), LiftStone, false);
	CreateBlock(FVector(710, -359.5f, 105), FVector(20, 1, 210), LiftStone, false);
	CreateBlock(FVector(710, -305, 210.5f), FVector(20, 110, 1), LiftStone, false);
	CreateBlock(
		FVector(699, -305, 216), FVector(2, 130, 12),
		LiftStone, false);
	CreateBlock(
		FVector(697.8f, -305, 225), FVector(1.2f, 38, 19),
		TexMat(TEXT("M_LiftHall"), ScreenGlowMaterial), false);
	// Open stair throat. The former full-height wall made the five visible
	// treads a dead-end decoration while route tests claimed a stair choice.
	CreateBlock(FVector(-330, -237.5f, 120), FVector(20, 15, 240), PlasticDarkMaterial);
	CreateBlock(FVector(-330, -372.5f, 120), FVector(20, 15, 240), PlasticDarkMaterial);
	CreateBlock(FVector(-330, -305, 225), FVector(20, 120, 30), PlasticDarkMaterial);

	// North wall extensions beyond the apartment span, plus the height filler
	// strip above the apartment's 230 cm wall to the corridor's 240 cm. The
	// east extension is permanently split around the future 403 doorway.
	// During CH01 a removable wall plug makes the split read as an ordinary
	// uninterrupted wall; CH02 parks that plug and reveals the open room.
	// The west section is split around the permanent 4F->5F stair opening.
	// The stairs themselves exist from the first visit; only their interaction
	// surface is armed by the authored 4F light-and-cat cue.
	CreateBlock(FVector(-332.5f, -225, 120), FVector(15, 20, 240), CorridorWallX);
	CreateBlock(FVector(-225.0f, -225, 120), FVector(10, 20, 240), CorridorWallX);
	CreateBlock(FVector(-277.5f, -225, 225), FVector(95, 20, 30), CorridorWallX);
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
		// Powder-coated casing stays readable at this thin aspect ratio; the
		// former stucco UV stretched into conspicuous horizontal stripes.
		CreateBlock(FVector(DoorX - 46, -233, 102), FVector(8, 7, 208), DoorTrim, false);
		CreateBlock(FVector(DoorX + 46, -233, 102), FVector(8, 7, 208), DoorTrim, false);
		CreateBlock(FVector(DoorX, -233, 204), FVector(100, 7, 8), DoorTrim, false);
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
	CreateBlock(FVector(96, -233, 102), FVector(8, 7, 208), DoorTrim, false);
	CreateBlock(FVector(190, -233, 102), FVector(8, 7, 208), DoorTrim, false);
	CreateBlock(FVector(143, -233, 206), FVector(102, 7, 8), DoorTrim, false);
	// The actual 404 opening also needs its three inside returns capped. The
	// south-wall material is authored for the broad wall face and streaks when
	// seen edge-on through this 20 cm reveal.
	// Offset these caps a few millimetres into the opening. Making their outer
	// faces exactly coplanar with the wall return caused a striped z-fighting
	// pattern in the corridor capture.
	CreateBlock(FVector(99.1f, -225, 105), FVector(1.8f, 22, 210), DoorTrim, false);
	CreateBlock(FVector(184.4f, -225, 105), FVector(2.8f, 22, 210), DoorTrim, false);
	CreateBlock(FVector(142, -225, 209.7f), FVector(88, 22, 0.4f), DoorTrim, false);
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

	// Permanent first flight toward 5F. It turns north from the west landing,
	// so it never overlaps the parallel flight descending toward 3F. The
	// memory cut happens on the first tread, but the visible architecture is
	// always here and therefore cannot pop in when the story state changes.
	for (int32 UpperStepIndex = 0; UpperStepIndex < 4; ++UpperStepIndex)
	{
		const float StepY = -216.0f + UpperStepIndex * 22.0f;
		const float StepTop = 18.0f + UpperStepIndex * 18.0f;
		CreateBlock(
			FVector(-277.5f, StepY, StepTop * 0.5f),
			FVector(85, 22, StepTop),
			CorridorFloor);
		CreateBlock(
			FVector(-277.5f, StepY + 10.0f, StepTop + 0.8f),
			FVector(83, 2.5f, 1.6f),
			Skirting,
			false);
	}
	// Close the greybox flight beyond the authored boundary and keep the
	// unseen upper landing dark. Release art can extend it without changing
	// the interaction or narrative state contract.
	CreateBlock(
		FVector(-332.5f, -165.0f, 120),
		FVector(15, 120, 240),
		ConcreteDarkMaterial);
	CreateBlock(
		FVector(-222.5f, -165.0f, 120),
		FVector(15, 120, 240),
		ConcreteDarkMaterial);
	CreateBlock(
		FVector(-277.5f, -105.0f, 120),
		FVector(125, 15, 240),
		ConcreteDarkMaterial);
	CreateBlock(
		FVector(-277.5f, -165.0f, 250),
		FVector(125, 120, 20),
		CorridorCeil);

	// Down flight: five steps sinking west into darkness. Korean walk-ups have
	// a black steel balustrade with a flat cap rail and thin square balusters.
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
			FVector(FixtureX, -305, 226), 820.0f, 410.0f,
			FLinearColor(0.86f, 0.97f, 1.0f), true, 16.0f);
		CorridorLight->SetVolumetricScatteringIntensity(0.10f);
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
	UMaterialInterface* CorridorFloor =
		TexMat(TEXT("M_GraniteTile_XY"), ConcreteMaterial);
	UMaterialInterface* OfferingBowlMaterial =
		TexMat(TEXT("M_MetalUV"), MetalFrameMaterial);
	UMaterialInterface* DarkGloss = PlasticDarkMaterial;
	UMaterialInterface* DoorTrim = PlasticDarkMaterial;

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
	AddOverlay(FVector(384, -233, 102), FVector(8, 7, 208), DoorTrim, false);
	AddOverlay(FVector(476, -233, 102), FVector(8, 7, 208), DoorTrim, false);
	AddOverlay(FVector(430, -233, 204), FVector(100, 7, 8), DoorTrim, false);
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
	// Most of the figure is below the same duvet: using near-black concrete
	// for both lobes collapsed the bed and sleeper into one opaque cut-out.
	AddOverlay(
		FVector(300, 132, 62), FVector(70, 92, 21),
		Bedding, false, SphereMesh);
	AddOverlay(
		FVector(300, 91, 60), FVector(72, 56, 17),
		Bedding, false, SphereMesh);
	// A pale pillow gives the eye one human-scale reference before the player
	// understands the low mound beneath the otherwise dark duvet.
	AddOverlay(FVector(300, 182, 59), FVector(80, 47, 10), FridgeInteriorMaterial, false);
	AddOverlay(
		FVector(300, 188, 68), FVector(25, 23, 19),
		WalletBrownMaterial, false, SphereMesh);
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
	AddOverlay(FVector(380, 34, 70), FVector(12, 12, 28), DarkGloss, false, CylinderMesh);
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
		FVector(376, 42, 108), 620.0f, 330.0f,
		FLinearColor(1.0f, 0.54f, 0.23f), false, 30.0f);
	if (MirrorRoomLamp)
	{
		MirrorRoomLamp->SetVisibility(false);
		MirrorRoomLamp->SetSpecularScale(0.55f);
		MirrorRoomLamp->SetVolumetricScatteringIntensity(0.05f);
	}
	// Keep the non-specular fill on the camera side of the bed. Placing it
	// twenty centimetres from the rear wall made a cold white hotspot while
	// the outward-facing duvet and frame remained black.
	MirrorRoomBounce = CreateLight(
		FVector(430, -40, 125), 360.0f, 310.0f,
		FLinearColor(0.30f, 0.38f, 0.56f), false, 56.0f);
	if (MirrorRoomBounce)
	{
		MirrorRoomBounce->SetVisibility(false);
		MirrorRoomBounce->SetSpecularScale(0.0f);
		MirrorRoomBounce->SetVolumetricScatteringIntensity(0.0f);
	}

	// CH02 overlaps the 7/27 state after the management reply: 401 has already
	// removed the rice, spoon and incense. The salt was swept apart by hand,
	// leaving a deliberate central gap rather than a trampled line.
	for (int32 GrainIndex = 0; GrainIndex < 9; ++GrainIndex)
	{
		const float Alpha = static_cast<float>(GrainIndex) / 8.0f;
		const float GrainSize = 1.35f + (GrainIndex % 3) * 0.3f;
		for (const float Side : {-1.0f, 1.0f})
		{
			const float GrainX = -150.0f
				+ Side * (17.0f + (1.0f - Alpha) * 25.0f);
			const float GrainY = -248.0f - Alpha * 4.0f
				+ FMath::Sin(GrainIndex * 1.71f) * 0.8f;
			if (UStaticMeshComponent* Grain = AddOverlay(
				FVector(GrainX, GrainY, 0.55f),
				FVector(GrainSize, GrainSize * 0.72f, 0.7f),
				SignWhiteMaterial, false, SphereMesh))
			{
				Grain->SetCastShadow(false);
			}
		}
	}
	const FVector2D SweptSalt[] = {
		FVector2D(-198.0f, -250.0f), FVector2D(-202.0f, -254.0f),
		FVector2D(-102.0f, -250.0f), FVector2D(-98.0f, -254.0f)};
	for (int32 ScatterIndex = 0;
		ScatterIndex < UE_ARRAY_COUNT(SweptSalt);
		++ScatterIndex)
	{
		if (UStaticMeshComponent* Grain = AddOverlay(
			FVector(SweptSalt[ScatterIndex].X, SweptSalt[ScatterIndex].Y, 0.5f),
			FVector(1.2f, 1.0f, 0.6f),
			SignWhiteMaterial,
			false,
			SphereMesh))
		{
			Grain->SetCastShadow(false);
		}
	}

	// Only the dry circular trace of the removed rice bowl remains.
	if (UStaticMeshComponent* DryOuter = AddOverlay(
		FVector(-174, -280, 0.35f), FVector(24, 24, 0.5f),
		ConcreteDarkMaterial, false, CylinderMesh))
	{
		DryOuter->SetCastShadow(false);
	}
	if (UStaticMeshComponent* DryInner = AddOverlay(
		FVector(-174, -280, 0.65f), FVector(18, 18, 0.35f),
		CorridorFloor, false, CylinderMesh))
	{
		DryInner->SetCastShadow(false);
	}

	// The clear-water bowl sits outside the broken line. Its 24 cm steel body
	// matches the overturned bowl that reappears on the unfinished fifth floor.
	AddOverlay(
		FVector(-126, -280, 4.5f), FVector(24, 24, 9.0f),
		OfferingBowlMaterial, false, CylinderMesh);
	AddOverlay(
		FVector(-126, -280, 8.8f), FVector(26, 26, 1.2f),
		OfferingBowlMaterial, false, CylinderMesh);
	if (UStaticMeshComponent* WaterSurface = AddOverlay(
		FVector(-126, -280, 9.55f), FVector(18, 18, 0.5f),
		WaterBlueMaterial, false, CylinderMesh))
	{
		WaterSurface->SetCastShadow(false);
	}

	// A restrained corridor spill keeps the evidence legible without turning
	// this quiet act of waiting into a supernatural spotlight.
	OfferingLight = CreateLight(
		FVector(-150, -312, 78), 195.0f, 220.0f,
		FLinearColor(1.0f, 0.78f, 0.58f), false, 30.0f);
	if (OfferingLight)
	{
		OfferingLight->SetVisibility(false);
		OfferingLight->SetSpecularScale(0.35f);
		OfferingLight->SetVolumetricScatteringIntensity(0.04f);
	}

	ActiveParent = nullptr;

	// Wet steps begin inside the real 2F cab, from the narrow door gap to the
	// rider's back. Matching lower-cab/lobby steps are already waiting when
	// the doors reopen at 1F, so the impossible follower still obeys the
	// building's visible floors.
	const FVector FootprintLocations[] = {
		FVector(726, -286, 300), FVector(748, -326, 300),
		FVector(766, -284, 300), FVector(788, -326, 300),
		FVector(806, -286, 300), FVector(828, -326, 300),
		FVector(760, -286, 0), FVector(778, -326, 0),
		FVector(730, -284, 0), FVector(748, -326, 0),
		FVector(690, -286, 0), FVector(708, -326, 0),
		FVector(650, -300, 0), FVector(626, -338, 0),
		FVector(603, -350, 0), FVector(579, -374, 0),
	};
	for (int32 StepIndex = 0; StepIndex < UE_ARRAY_COUNT(FootprintLocations); ++StepIndex)
	{
		UStaticMeshComponent* Step = AddOverlay(
			FVector(
				FootprintLocations[StepIndex].X,
				FootprintLocations[StepIndex].Y,
				FootprintLocations[StepIndex].Z + 0.45f),
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
		Light->SetIntensity(bLive ? (bCorridor ? 820.0f : 920.0f) : 0.0f);
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
		MirrorRoomLamp->SetIntensity(bVisible ? 300.0f : 0.0f);
	}
	if (MirrorRoomBounce)
	{
		MirrorRoomBounce->SetVisibility(bVisible);
		MirrorRoomBounce->SetIntensity(bVisible ? 360.0f : 0.0f);
	}
	if (OfferingLight)
	{
		OfferingLight->SetVisibility(bVisible);
		OfferingLight->SetIntensity(bVisible ? 195.0f : 0.0f);
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
	SetChapterActorVisible(MirrorAlarmMemo);
	SetChapterActorVisible(MailboxBills);
	SetChapterActorVisible(OfferingNote);
	SetChapterActorVisible(NightRoster);

	// The restored copy does not exist until the unanswered employee call.
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
	// Refresh defensively at the reveal boundary. The CH02 paper must preserve
	// the CH01 product row while replacing only 04:31 with the 04:44 overlay.
	RefreshPurchaseProfilePresentation();
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
	if (NeighborhoodLifeDirector)
	{
		// When the player returns, even the ordinary city has stopped answering.
		// The distant alarm is allowed to own the entire sound field.
		NeighborhoodLifeDirector->SetChapterVariant(
			EIGNeighborhoodChapterVariant::ChapterTwoAbsent);
	}

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

					// The card is a transition, not the old demo's dead end.
					// Keep CH02 documentation capture finite, but an ordinary
					// playthrough now continues into the complete greybox ending.
					if (!FParse::Param(
						FCommandLine::Get(), TEXT("IGCaptureCH02")))
					{
						GetWorldTimerManager().SetTimer(
							ChapterTransitionHandle,
							this,
							&ThisClass::EnterChapterThree,
							4.85f,
							false);
					}

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
	UMaterialInterface* LiftStone = PlasticDarkMaterial;

	// Interior X 450..710, Y -375..-235, height 240 — directly under the lift.
	CreateBlock(FVector(580, -305, -10), FVector(300, 180, 20), LobbyFloor);
	CreateBlock(FVector(580, -305, 250), FVector(300, 180, 20), LobbyCeil);
	CreateBlock(FVector(580, -225, 120), FVector(300, 20, 240), LobbyWallX);
	// Split the west wall around a real stair-core doorway. The old solid
	// slab forced a player standing inside the lobby to walk back outdoors
	// and around the pilotis before the lower stair mouth became reachable.
	CreateBlock(FVector(440, -370, 120), FVector(20, 30, 240), LobbyWallY);
	CreateBlock(FVector(440, -240, 120), FVector(20, 30, 240), LobbyWallY);
	CreateBlock(FVector(440, -305, 225), FVector(20, 100, 30), LobbyWallY);
	// Enclosed ground-floor connector from the lobby doorway to the stair
	// core. The core is vertically aligned with the west end of the 4F hall;
	// leaving this span open made the new doorway lead into a five-metre void.
	CreateBlock(FVector(170.5f, -305, -10), FVector(519, 120, 20), LobbyFloor);
	CreateBlock(FVector(170.5f, -305, 250), FVector(519, 120, 20), LobbyCeil);
	CreateBlock(FVector(170.5f, -375, 120), FVector(519, 20, 240), LobbyWallX);
	CreateBlock(FVector(170.5f, -235, 120), FVector(519, 20, 240), LobbyWallX);
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
	for (const float PortalY : {-245.0f, -365.0f})
	{
		CreateBlock(
			FVector(699, PortalY, 105), FVector(2, 10, 210),
			LiftStone, false);
	}
	CreateBlock(FVector(710, -250.5f, 105), FVector(20, 1, 210), LiftStone, false);
	CreateBlock(FVector(710, -359.5f, 105), FVector(20, 1, 210), LiftStone, false);
	CreateBlock(FVector(710, -305, 210.5f), FVector(20, 110, 1), LiftStone, false);
	CreateBlock(
		FVector(699, -305, 216), FVector(2, 130, 12),
		LiftStone, false);
	CreateBlock(
		FVector(697.8f, -305, 225), FVector(1.2f, 38, 19),
		TexMat(TEXT("M_LiftHall"), ScreenGlowMaterial), false);

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
			FVector(FixtureX, -305, 226), 920.0f, 400.0f,
			FLinearColor(0.87f, 0.98f, 1.0f), true, 16.0f);
		LobbyLight->SetVolumetricScatteringIntensity(0.10f);
		LobbyLights.Add(LobbyLight);
	}

	// A physically separate 2F landing for the interrupted CH02 ride. The
	// previous prototype opened the 1F lobby doors and called that view "2F",
	// which broke the building's vertical logic. This enclosed landing shares
	// the 300 cm floor spacing of the rest of the villa and is visible only
	// through the authored 12 cm lift-door gap.
	constexpr float SecondFloorZ = 300.0f;
	CreateBlock(
		FVector(580, -305, SecondFloorZ - 10),
		FVector(300, 180, 20),
		TexMat(TEXT("M_Concrete_XY"), ConcreteDarkMaterial));
	CreateBlock(
		FVector(580, -305, SecondFloorZ + 250),
		FVector(300, 180, 20),
		LobbyCeil);
	CreateBlock(
		FVector(580, -225, SecondFloorZ + 120),
		FVector(300, 20, 240),
		LobbyWallX);
	CreateBlock(
		FVector(440, -305, SecondFloorZ + 120),
		FVector(20, 160, 240),
		LobbyWallY);

	// East wall and reveals around the actual 2F lift opening.
	CreateBlock(
		FVector(710, -242.5f, SecondFloorZ + 120),
		FVector(20, 15, 240),
		LobbyWallY);
	CreateBlock(
		FVector(710, -367.5f, SecondFloorZ + 120),
		FVector(20, 15, 240),
		LobbyWallY);
	CreateBlock(
		FVector(710, -305, SecondFloorZ + 225),
		FVector(20, 110, 30),
		LobbyWallY);
	for (const float PortalY : {-245.0f, -365.0f})
	{
		CreateBlock(
			FVector(699, PortalY, SecondFloorZ + 105),
			FVector(2, 10, 210),
			LiftStone,
			false);
	}
	CreateBlock(
		FVector(699, -305, SecondFloorZ + 216),
		FVector(2, 130, 12),
		LiftStone,
		false);

	// The closed service door and a dim floor plaque give the slit a readable
	// destination without turning the landing into an explorable branch.
	CreateBlock(
		FVector(451.0f, -305, SecondFloorZ + 100),
		FVector(4, 86, 200),
		TexMat(TEXT("M_SteelDoorUV"), DoorMaterial),
		false);
	CreateBlock(
		FVector(454.0f, -305, SecondFloorZ + 158),
		FVector(1.2f, 18, 24),
		ScreenGlowMaterial,
		false);
	UPointLightComponent* SecondFloorEmergencyLight = CreateLight(
		FVector(520, -305, SecondFloorZ + 205),
		18.0f,
		185.0f,
		FLinearColor(0.42f, 0.55f, 0.46f),
		false,
		4.0f);
	SecondFloorEmergencyLight->SetVolumetricScatteringIntensity(0.08f);

	// Lower mouth of the same occluded switchback stair. Five real treads and
	// a dark return wall make the floor compression happen behind a plausible
	// 180-degree corner instead of in the open lobby.
	for (int32 LowerStepIndex = 0; LowerStepIndex < 5; ++LowerStepIndex)
	{
		const float StepX = -100.0f - LowerStepIndex * 22.0f;
		const float StepTop = 18.0f + LowerStepIndex * 18.0f;
		CreateBlock(
			FVector(StepX, -305, StepTop * 0.5f),
			FVector(22, 112, StepTop),
			LobbyFloor);
	}
	CreateBlock(
		FVector(-214, -305, 48),
		FVector(44, 112, 96),
		LobbyFloor);
	CreateBlock(
		FVector(-244, -305, 120),
		FVector(12, 136, 240),
		ConcreteDarkMaterial);
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
			2400.0f,
			760.0f,
			FLinearColor(1.0f, 0.82f, 0.58f),
			true,
			18.0f);
		LampLight->SetVolumetricScatteringIntensity(0.55f);
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
	UMaterialInterface* PriceStrip = TexMat(TEXT("M_PriceStrip"), FridgeInteriorMaterial);

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
	// Kick rails stop at the automatic-door jambs. A continuous 24 cm bar
	// across the entrance looked plausible from afar but acted like a curb and
	// could catch the character capsule at the threshold.
	CreateBlock(FVector(2405, -295, 12), FVector(12, 200, 24), Metal);
	CreateBlock(FVector(2405, -590, 12), FVector(12, 150, 24), Metal);
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
							FVector(SnackX, GondolaY + FaceSign * 11.0f, TierZ + 9.5f),
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
					FVector(ItemX, -655, TierZ + (bKimbap ? 4.0f : 3.0f)),
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
		CreateCupRamyeon(FVector(CupX, -654, 167.5f), CupX * 3.0f);
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
			// Real label rails are only about four centimetres high. A dark
			// carrier breaks up the former eight-centimetre glowing white band.
			CreateBlock(
				FVector(2902.6f, BayY, StripZ), FVector(2.2f, 66, 6),
				PlasticDarkMaterial, false);
			CreateBlock(
				FVector(2901.2f, BayY, StripZ), FVector(0.8f, 62, 4.2f),
				PriceStrip, false);
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

void AIGPrologueWorldScene::SpawnStairTransition()
{
	if (!GetWorld() || StairTransition)
	{
		return;
	}

	const FTransform TransitionTransform = GetActorTransform();
	StairTransition = GetWorld()->SpawnActorDeferred<AIGStairTransition>(
		AIGStairTransition::StaticClass(),
		TransitionTransform,
		this,
		nullptr,
		ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (!StairTransition)
	{
		return;
	}

	const auto ToWorld = [this](const FVector& Local)
	{
		return GetActorTransform().TransformPosition(Local);
	};
	StairTransition->Configure(
		ToWorld(FVector(-352.0f, -305.0f, 915.0f)),
		ToWorld(FVector(-300.0f, -305.0f, 942.0f)),
		FRotator(0.0f, 0.0f, 0.0f),
		ToWorld(FVector(-214.0f, -305.0f, 193.0f)),
		ToWorld(FVector(-188.0f, -305.0f, 187.0f)),
		FRotator(0.0f, 0.0f, 0.0f));
	StairTransition->FinishSpawning(TransitionTransform);
	StairTransition->OnTransitionCompleted.AddUniqueDynamic(
		this,
		&ThisClass::HandleStairTransitionCompleted);
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

	// Front door: always available; room clues remain optional.
	HomeDoor = World->SpawnActor<AIGSwingDoor>(
		AIGSwingDoor::StaticClass(),
		FTransform(FRotator(0, -90, 0), IGPrologueWorld::HomeDoorLocation),
		SpawnParameters);
	if (HomeDoor)
	{
		HomeDoor->ConfigurePrototypeVisuals(
			CubeMesh,
			TexMat(TEXT("M_SteelDoorUV"), DoorMaterial),
			// The authored stainless UV is useful on broad lift panels but
			// compresses into horizontal bands on this 13 cm vertical inlay.
			MetalFrameMaterial,
			FVector(7, 84, 204));
		HomeDoor->SetLeverMesh(
			PropMesh(TEXT("SM_LeverHandle")),
			MetalFrameMaterial,
			FVector(7, 84, 204));
		// Korean entrance doors open outward — and it keeps the hallway clear.
		HomeDoor->SetOpenYaw(-95.0f);

		// Fridge and wallet are optional investigations. Leaving immediately
		// is the canonical PocketCard route, so objectives may suggest them but
		// the physical door must never enforce either state.
		TArray<FIGDoorRequirement> NoDoorRequirements;
		HomeDoor->SetRequirements(MoveTemp(NoDoorRequirements));

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
		// UV-brushed metal turned the tall side shells into stretched bands
		// and reflected the alley brick strongly enough to look like bare
		// masonry inside the cab. Use the restrained flat metal already used
		// by the villa frames for both the shell and rear inset.
		CabVisuals.StainlessMaterial = MetalFrameMaterial;
		// The story specifies no readable mirror in the safe CH01 car. A
		// brushed rear panel also prevents the outdoor facade/sky reflection
		// from appearing inside a closed elevator during the hidden transfer.
		CabVisuals.MirrorMaterial = MetalFrameMaterial;
		CabVisuals.FloorMaterial = TexMat(TEXT("M_MarbleFloor_XY"), StoreFloorMaterial);
		CabVisuals.InlayMaterial = PlasticDarkMaterial;
		CabVisuals.CopMaterial = TexMat(TEXT("M_LiftCOP"), ScreenGlowMaterial);
		CabVisuals.HallMaterial = TexMat(TEXT("M_LiftHall"), ScreenGlowMaterial);
		CabVisuals.DiffuserMaterial = LightPanelMaterial;
		Elevator->ConfigurePrototypeVisuals(CabVisuals, 900.0f);
		Elevator->OnReturnedToFourthFloor.AddUniqueDynamic(
			this,
			&ThisClass::HandleElevatorReturnedToFourthFloor);

		// The primary COP is correctly beside the door, but that places it
		// behind the first-person capture. A non-colliding secondary panel on
		// the opposite wall is common accessibility equipment in Korean lifts
		// and keeps the floor/buttons legible from inside both cab copies.
		for (const float CabBaseZ : {0.0f, -900.0f})
		{
			CreateDecoOnComponent(
				Elevator->GetRootComponent(), CubeMesh, MetalFrameMaterial,
				FVector(12, -74.2f, CabBaseZ + 108), FRotator::ZeroRotator,
				FVector(0.19f, 0.012f, 0.94f));
			CreateDecoOnComponent(
				Elevator->GetRootComponent(), CubeMesh, CabVisuals.CopMaterial,
				FVector(12, -73.4f, CabBaseZ + 108), FRotator::ZeroRotator,
				FVector(0.14f, 0.006f, 0.86f));
		}
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
		Wallet->OnPickedUp.AddUniqueDynamic(
			this,
			&ThisClass::HandlePurchaseSelectionChanged);
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

	// Three fixed purchase profiles share the cooler shelf. Picking one commits
	// its REBIRTH profile before the legacy HasWater event is broadcast.
	const float WaterYs[] = {-576.0f, -552.0f, -528.0f};
	const EIGRebirthPurchaseProfile WaterProfiles[] = {
		EIGRebirthPurchaseProfile::ProfileA500MlX2,
		EIGRebirthPurchaseProfile::ProfileB1LX1,
		EIGRebirthPurchaseProfile::ProfileC2LX2};
	for (int32 WaterIndex = 0; WaterIndex < UE_ARRAY_COUNT(WaterYs); ++WaterIndex)
	{
		const float WaterY = WaterYs[WaterIndex];
		const EIGRebirthPurchaseProfile PurchaseProfile =
			WaterProfiles[WaterIndex];
		AIGPickupItem* WaterBottle = World->SpawnActor<AIGPickupItem>(
			AIGPickupItem::StaticClass(),
			FTransform(FRotator::ZeroRotator, FVector(2925, WaterY, 106.5f)),
			SpawnParameters);
		if (WaterBottle)
		{
			WaterBottle->PickupMode = EIGPickupMode::CarryInHand;
			WaterBottle->RebirthPurchaseProfileOnPickup = PurchaseProfile;
			WaterBottle->StateTagOnPickup = FGameplayTag::RequestGameplayTag(
				FName(TEXT("State.CH01.Morning.HasWater")), false);
			FVector ProfileScale = FVector::OneVector;
			bool bHasSecondBottle = false;
			switch (PurchaseProfile)
			{
			case EIGRebirthPurchaseProfile::ProfileA500MlX2:
				WaterBottle->SetInteractionPrompt(NSLOCTEXT(
					"IGPrologue", "WaterProfileA", "새벽샘물 500mL × 2 고르기"));
				WaterBottle->ThoughtOnPickup = NSLOCTEXT(
					"IGPrologue", "WaterProfileAThought", "두 병이면 충분하겠지.");
				bHasSecondBottle = true;
				break;
			case EIGRebirthPurchaseProfile::ProfileB1LX1:
				WaterBottle->SetInteractionPrompt(NSLOCTEXT(
					"IGPrologue", "WaterProfileB", "한강수 1L × 1 고르기"));
				WaterBottle->ThoughtOnPickup = NSLOCTEXT(
					"IGPrologue", "WaterProfileBThought", "이거 하나면 되겠다.");
				ProfileScale = FVector(1.12f, 1.12f, 1.35f);
				WaterBottle->CarryOffset = FVector(36.0f, 15.0f, -31.0f);
				break;
			case EIGRebirthPurchaseProfile::ProfileC2LX2:
				WaterBottle->SetInteractionPrompt(NSLOCTEXT(
					"IGPrologue", "WaterProfileC", "맑은산 2L × 2 고르기"));
				WaterBottle->ThoughtOnPickup = NSLOCTEXT(
					"IGPrologue", "WaterProfileCThought", "무겁지만 한 번에 가져가자.");
				ProfileScale = FVector(1.34f, 1.34f, 1.72f);
				WaterBottle->CarryOffset = FVector(39.0f, 16.0f, -42.0f);
				bHasSecondBottle = true;
				break;
			default:
				break;
			}
			// Held low and to the side so the lathed bottle reads without
			// filling the view; the mesh pivot is at the bottle's base.
			if (PurchaseProfile == EIGRebirthPurchaseProfile::ProfileA500MlX2)
			{
				WaterBottle->CarryOffset = FVector(34.0f, 15.0f, -26.0f);
			}
			WaterBottle->CarryRotation = FRotator(-12.0f, -14.0f, 0.0f);
			// Not simulated on the shelf: a lathed bottle standing on a wire
			// shelf topples the instant physics settles, and this one is a
			// pickup target rather than a kickable prop.
			WaterBottle->ConfigurePrototypeVisuals(
				PropMesh(TEXT("SM_WaterBottle"), CylinderMesh),
				GlassMaterial, ProfileScale, false);
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
			if (bHasSecondBottle)
			{
				UStaticMeshComponent* SecondBottle = CreateDecoOnComponent(
					WaterBottle->GetMeshComponent(),
					PropMesh(TEXT("SM_WaterBottle"), CylinderMesh),
					GlassMaterial,
					FVector(0, 8.0f, 0),
					FRotator(0, 7.0f, 0),
					FVector::OneVector);
				CreateDecoOnComponent(
					SecondBottle,
					PropMesh(TEXT("SM_BottleCap"), CylinderMesh),
					SnackBlueMaterial,
					FVector(0, 0, 20.1f),
					FRotator::ZeroRotator,
					FVector::OneVector);
				CreateDecoOnComponent(
					SecondBottle,
					PropMesh(TEXT("SM_LabelSleeve"), CylinderMesh),
					TexMat(TEXT("M_LabelWater"), WaterBlueMaterial),
					FVector(0, 0, 5.0f),
					FRotator(0.0f, -90.0f, 0.0f),
					FVector(3.36f, 3.36f, 8.6f));
			}
			AddStaticPurchaseBagProxy(WaterBottle, PurchaseProfile);
			WaterBottle->OnPickedUp.AddUniqueDynamic(
				this,
				&ThisClass::HandlePurchaseSelectionChanged);
			WaterBottles.Add(WaterBottle);
		}
	}
	CreateBlock(
		FVector(-352, -305, -90),
		FVector(48, 130, 18),
		TexMat(TEXT("M_GraniteTile_XY"), ConcreteMaterial));
	// The return wall hides the bounded transition at a genuine switchback
	// corner; the camera never sees the other floor or open sky.
	CreateBlock(
		FVector(-382, -305, 20),
		FVector(12, 150, 220),
		PlasticDarkMaterial);

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
		const FTransform ZoneTransform(FRotator::ZeroRotator, Location);
		AIGZoneTrigger* Zone = World->SpawnActorDeferred<AIGZoneTrigger>(
			AIGZoneTrigger::StaticClass(),
			ZoneTransform,
			this,
			nullptr,
			ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
		if (Zone)
		{
			Zone->SetZoneExtent(Extent);
			if (StateTagName)
			{
				Zone->StateTagOnEnter = FGameplayTag::RequestGameplayTag(FName(StateTagName), false);
			}
			Zone->ThoughtOnEnter = Thought;
			Zone->FinishSpawning(ZoneTransform);
		}
		return Zone;
	};

	// Canonical CH01 dressing happens when the player actually crosses the
	// 404 threshold. The ground-floor LeftHome volume remains an alley
	// checkpoint and legacy migration input, never the outfit authority.
	ChapterOneApartmentExitZone = SpawnZone(
		FVector(180, -305, 1010), FVector(72, 62, 110),
		TEXT("State.CH01.Morning.LeftApartment"),
		FText::GetEmpty());
	LeftHomeZone = SpawnZone(
		FVector(643, -435, 110), FVector(120, 55, 110),
		TEXT("State.CH01.Morning.LeftHome"),
		NSLOCTEXT("IGPrologue", "LeftHomeThought", "새벽 공기가 차다."));
	FlickerZone = SpawnZone(
		FVector(1000, -537, 110), FVector(80, 160, 110),
		nullptr,
		FText::GetEmpty());
	StoreEntryZone = SpawnZone(
		FVector(2450, -457, 116), FVector(35, 95, 110),
		TEXT("State.CH01.Morning.EnteredStore"),
		FText::GetEmpty());
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

	// The actual CH01 transaction and the CH02 death overlay share the
	// selected product data. Only the narrative timestamp differs.
	ChapterOneReceipt = SpawnNote(
		FVector(2582, -252, 100.5f),
		FRotator::ZeroRotator,
		FVector(8.5f, 15.0f, 0.25f),
		TexMat(TEXT("M_PaperClean"), SignWhiteMaterial),
		NSLOCTEXT("IGPrologue", "FirstReceiptPrompt", "출력된 영수증 읽기"),
		NSLOCTEXT("IGPrologue", "FirstReceiptTitle", "영수증"),
		IGPrologueWorld::BuildPurchaseReceiptSummary(
			EIGRebirthPurchaseProfile::ProfileA500MlX2,
			IGPrologueWorld::EReceiptTimeline::ActualPurchase0431));
	if (ChapterOneReceipt)
	{
		ChapterOneReceipt->SetThermalReceiptData(
			IGPrologueWorld::BuildPurchaseReceiptData(
				EIGRebirthPurchaseProfile::ProfileA500MlX2,
				IGPrologueWorld::EReceiptTimeline::ActualPurchase0431));
	}

	ExistingReceipt = SpawnNote(
		FVector(2582, -252, 100.5f),
		FRotator::ZeroRotator,
		FVector(8.5f, 15.0f, 0.25f),
		TexMat(TEXT("M_PaperClean"), SignWhiteMaterial),
		NSLOCTEXT("IGCH02", "ReceiptPrompt", "놓인 영수증 읽기"),
		NSLOCTEXT("IGCH02", "ReceiptTitle", "영수증"),
		IGPrologueWorld::BuildPurchaseReceiptSummary(
			EIGRebirthPurchaseProfile::ProfileA500MlX2,
			IGPrologueWorld::EReceiptTimeline::DeathOverlay0444));
	if (ExistingReceipt)
	{
		ExistingReceipt->SetThermalReceiptData(
			IGPrologueWorld::BuildPurchaseReceiptData(
				EIGRebirthPurchaseProfile::ProfileA500MlX2,
				IGPrologueWorld::EReceiptTimeline::DeathOverlay0444));
	}

	DuplicateReceipt = SpawnNote(
		FVector(2600, -252, 100.8f),
		FRotator(0, 0, 4),
		FVector(8.5f, 15.0f, 0.25f),
		TexMat(TEXT("M_PaperClean"), SignWhiteMaterial),
		NSLOCTEXT("IGCH02", "DuplicateReceiptPrompt", "새 영수증 읽기"),
		NSLOCTEXT("IGCH02", "DuplicateReceiptTitle", "영수증"),
		IGPrologueWorld::BuildPurchaseReceiptSummary(
			EIGRebirthPurchaseProfile::ProfileA500MlX2,
			IGPrologueWorld::EReceiptTimeline::DeathOverlay0444));
	if (DuplicateReceipt)
	{
		DuplicateReceipt->SetThermalReceiptData(
			IGPrologueWorld::BuildPurchaseReceiptData(
				EIGRebirthPurchaseProfile::ProfileA500MlX2,
				IGPrologueWorld::EReceiptTimeline::DeathOverlay0444));
	}

	MirrorAlarmMemo = SpawnNote(
		FVector(365, 42, IGPrologueWorld::FourthFloorZ + 61.0f),
		FRotator(0, 0, -3),
		FVector(14.0f, 10.0f, 0.3f),
		TexMat(TEXT("M_PaperFolded"), SignWhiteMaterial),
		NSLOCTEXT("IGCH02", "AlarmMemoPrompt", "집결 메모 읽기"),
		NSLOCTEXT("IGCH02", "AlarmMemoTitle", "[캠프 집결 메모]"),
		{
			NSLOCTEXT("IGCH02", "AlarmMemoL1", "내일  05:30  캠프 집결"),
			NSLOCTEXT("IGCH02", "AlarmMemoL2", "알람  :  집결 20분 전"),
			FText::GetEmpty(),
			NSLOCTEXT("IGCH02", "AlarmMemoL3", "※ 늦지 말 것"),
		});

	MailboxBills = SpawnNote(
		FVector(528, -222.5f, 157),
		FRotator::ZeroRotator,
		FVector(19, 1.3f, 27),
		TexMat(TEXT("M_PaperFolded"), SignWhiteMaterial),
		NSLOCTEXT("IGCH02", "MailboxPrompt", "관리 연락함 확인"),
		NSLOCTEXT("IGCH02", "MailboxTitle", "[민원 접수 사본]"),
		{
			NSLOCTEXT("IGCH02", "MailboxL1", "7/27  06:18  ·  401호 정막례"),
			FText::GetEmpty(),
			NSLOCTEXT("IGCH02", "MailboxL2", "“사람이 떨어진 것 같은 쿵 소리.”"),
			NSLOCTEXT("IGCH02", "MailboxL3", "처리 : 고양이로 추정 · 옥상 잠금 확인"),
			FText::GetEmpty(),
			NSLOCTEXT("IGCH02", "MailboxL4", "404호 연락 불가 · 가족 연락처 확인 요청"),
		});

	OfferingNote = SpawnNote(
		FVector(-94, -278, IGPrologueWorld::FourthFloorZ + 0.6f),
		FRotator(0, 0, -8),
		FVector(25, 18, 0.6f),
		TexMat(TEXT("M_PaperFolded"), SignWhiteMaterial),
		NSLOCTEXT("IGCH02", "OfferingNotePrompt", "물그릇 옆 쪽지 읽기"),
		NSLOCTEXT("IGCH02", "OfferingNoteTitle", "삐뚤한 글씨"),
		{
			NSLOCTEXT("IGCH02", "OfferingNoteL1", "목마른 사람은"),
			NSLOCTEXT("IGCH02", "OfferingNoteL2", "이 물을 드시오."),
			FText::GetEmpty(),
			NSLOCTEXT("IGCH02", "OfferingNoteL3", "— 401호"),
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

	// CH02 keeps matching product silhouettes on the shelf for continuity,
	// but they are evidence dressing rather than a second shopping errand.
	ChapterTwoWallet = World->SpawnActor<AIGPickupItem>(
		AIGPickupItem::StaticClass(),
		FTransform(FRotator(0, 20, 0), IGPrologueWorld::WalletLocation),
		SpawnParameters);
	if (ChapterTwoWallet)
	{
		// This is a non-authoritative memory prop. Its visibility is restored
		// from the CH01 payment choice and it can never be picked up again.
		ChapterTwoWallet->PickupMode = EIGPickupMode::Pocket;
		ChapterTwoWallet->SetInteractionPrompt(
			NSLOCTEXT("IGCH02", "SecondWalletPrompt", "놓인 지갑 확인"));
		ChapterTwoWallet->ConfigurePrototypeVisuals(
			CubeMesh, WalletBrownMaterial, FVector(0.14f, 0.09f, 0.03f), false);
		ParkActor(ChapterTwoWallet);
	}

	const float ChapterTwoWaterYs[] = {-576.0f, -552.0f, -528.0f};
	const EIGRebirthPurchaseProfile ChapterTwoProfiles[] = {
		EIGRebirthPurchaseProfile::ProfileA500MlX2,
		EIGRebirthPurchaseProfile::ProfileB1LX1,
		EIGRebirthPurchaseProfile::ProfileC2LX2};
	for (int32 WaterIndex = 0;
		WaterIndex < UE_ARRAY_COUNT(ChapterTwoWaterYs);
		++WaterIndex)
	{
		const float WaterY = ChapterTwoWaterYs[WaterIndex];
		const EIGRebirthPurchaseProfile PurchaseProfile =
			ChapterTwoProfiles[WaterIndex];
		AIGPickupItem* WaterBottle = World->SpawnActor<AIGPickupItem>(
			AIGPickupItem::StaticClass(),
			FTransform(FRotator::ZeroRotator, FVector(2925, WaterY, 106.5f)),
			SpawnParameters);
		if (!WaterBottle)
		{
			continue;
		}

		WaterBottle->PickupMode = EIGPickupMode::CarryInHand;
		WaterBottle->RebirthPurchaseProfileOnPickup = PurchaseProfile;
		WaterBottle->StateTagOnPickup = FGameplayTag::RequestGameplayTag(
			FName(TEXT("State.CH02.Loop.HasWater")), false);
		FVector ProfileScale = FVector::OneVector;
		bool bHasSecondBottle = false;
		switch (PurchaseProfile)
		{
		case EIGRebirthPurchaseProfile::ProfileA500MlX2:
			WaterBottle->SetInteractionPrompt(NSLOCTEXT(
				"IGCH02", "SecondWaterProfileA", "같은 새벽샘물 확인하기"));
			WaterBottle->CarryOffset = FVector(34.0f, 15.0f, -26.0f);
			bHasSecondBottle = true;
			break;
		case EIGRebirthPurchaseProfile::ProfileB1LX1:
			WaterBottle->SetInteractionPrompt(NSLOCTEXT(
				"IGCH02", "SecondWaterProfileB", "같은 한강수 확인하기"));
			WaterBottle->CarryOffset = FVector(36.0f, 15.0f, -31.0f);
			ProfileScale = FVector(1.12f, 1.12f, 1.35f);
			break;
		case EIGRebirthPurchaseProfile::ProfileC2LX2:
			WaterBottle->SetInteractionPrompt(NSLOCTEXT(
				"IGCH02", "SecondWaterProfileC", "같은 맑은산 두 병 확인하기"));
			WaterBottle->CarryOffset = FVector(39.0f, 16.0f, -42.0f);
			ProfileScale = FVector(1.34f, 1.34f, 1.72f);
			bHasSecondBottle = true;
			break;
		default:
			break;
		}
		WaterBottle->ThoughtOnPickup =
			NSLOCTEXT("IGCH02", "SecondWaterThought", "같은 자리. 내가 골랐던 물.");
		WaterBottle->CarryRotation = FRotator(-12.0f, -14.0f, 0.0f);
		WaterBottle->ConfigurePrototypeVisuals(
			PropMesh(TEXT("SM_WaterBottle"), CylinderMesh),
			GlassMaterial, ProfileScale, false);
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
		if (bHasSecondBottle)
		{
			UStaticMeshComponent* SecondBottle = CreateDecoOnComponent(
				WaterBottle->GetMeshComponent(),
				PropMesh(TEXT("SM_WaterBottle"), CylinderMesh),
				GlassMaterial,
				FVector(0, 8.0f, 0),
				FRotator(0, 7.0f, 0),
				FVector::OneVector);
			CreateDecoOnComponent(
				SecondBottle,
				PropMesh(TEXT("SM_BottleCap"), CylinderMesh),
				SnackBlueMaterial,
				FVector(0, 0, 20.1f),
				FRotator::ZeroRotator,
				FVector::OneVector);
			CreateDecoOnComponent(
				SecondBottle,
				PropMesh(TEXT("SM_LabelSleeve"), CylinderMesh),
				TexMat(TEXT("M_LabelWater"), WaterBlueMaterial),
				FVector(0, 0, 5.0f),
				FRotator(0.0f, -90.0f, 0.0f),
				FVector(3.36f, 3.36f, 8.6f));
		}
		AddStaticPurchaseBagProxy(WaterBottle, PurchaseProfile);
		ParkActor(WaterBottle);
		ChapterTwoWaterBottles.Add(WaterBottle);
	}

	auto SpawnParkedZone = [&](const FVector& Location,
		const FVector& Extent,
		const TCHAR* StateTagName,
		const FText& Thought) -> AIGZoneTrigger*
	{
		const FTransform ZoneTransform(FRotator::ZeroRotator, Location);
		AIGZoneTrigger* Zone = World->SpawnActorDeferred<AIGZoneTrigger>(
			AIGZoneTrigger::StaticClass(),
			ZoneTransform,
			this,
			nullptr,
			ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
		if (Zone)
		{
			Zone->SetZoneExtent(Extent);
			Zone->StateTagOnEnter = FGameplayTag::RequestGameplayTag(
				FName(StateTagName), false);
			Zone->ThoughtOnEnter = Thought;
			Zone->SetActorEnableCollision(false);
			Zone->FinishSpawning(ZoneTransform);
		}
		return Zone;
	};

	ChapterTwoLeftHomeZone = SpawnParkedZone(
		FVector(180, -305, 1010), FVector(72, 62, 110),
		TEXT("State.CH02.Loop.LeftHome"),
		FText::GetEmpty());
	ChapterTwoOutdoorZone = SpawnParkedZone(
		FVector(643, -435, 110), FVector(120, 55, 110),
		TEXT("State.CH02.Loop.EnteredAlley"),
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
	// C3 belongs to the return toward 404, not to a directionless lobby
	// crossing. The old ground-floor volume ended the chapter while a player
	// with P1+search evidence was still walking out, and could miss an
	// elevator return entirely. This 4F corridor anchor is crossed only after
	// the player has turned back toward the apartment.
	ChapterTwoReturnZone = SpawnParkedZone(
		FVector(300, -305, 1010), FVector(45, 62, 110),
		TEXT("State.CH02.Loop.Returned"),
		FText::GetEmpty());
}

void AIGPrologueWorldScene::SpawnChapterTwoItemContinuityDressing()
{
	if (IsValid(ChapterTwoItemContinuityDressing) || !GetWorld())
	{
		return;
	}

	EIGRebirthPurchaseProfile PurchaseProfile =
		EIGRebirthPurchaseProfile::ProfileA500MlX2;
	EIGRebirthBottleClosureState ClosureState =
		EIGRebirthBottleClosureState::Resealed;
	if (const UGameInstance* GameInstance = GetGameInstance())
	{
		if (const UIGRebirthNarrativeSubsystem* RebirthState =
			GameInstance->GetSubsystem<UIGRebirthNarrativeSubsystem>())
		{
			const FIGRebirthChoiceState Choices = RebirthState->GetChoices();
			if (Choices.PurchaseProfile != EIGRebirthPurchaseProfile::Unset)
			{
				PurchaseProfile = Choices.PurchaseProfile;
			}
			if (Choices.BottleClosureState
				!= EIGRebirthBottleClosureState::Unset)
			{
				ClosureState = Choices.BottleClosureState;
			}
		}
	}

	for (TActorIterator<AIGItemContinuityDressing> It(GetWorld()); It; ++It)
	{
		AIGItemContinuityDressing* Existing = *It;
		if (!Existing
			|| !Existing->ActorHasTag(
				FName(TEXT("REBIRTH.ItemContinuity.CH02RecycleSack"))))
		{
			continue;
		}
		if (Existing->MatchesContract(
			EIGItemContinuityPresentation::LobbyRecycleSack,
			PurchaseProfile,
			ClosureState))
		{
			ChapterTwoItemContinuityDressing = Existing;
			IGStory::AddState(
				this,
				FGameplayTag::RequestGameplayTag(
					FName(TEXT(
						"State.CH02.Loop.RecycleEvidenceRestored")),
					false));
			return;
		}
		Existing->Destroy();
	}

	const FTransform EvidenceTransform(
		GetActorTransform().TransformRotation(
			FRotator(0.0f, 14.0f, 0.0f).Quaternion()),
		GetActorTransform().TransformPosition(
			FVector(-210.0f, -350.0f, 0.8f)));
	FActorSpawnParameters Parameters;
	Parameters.Owner = this;
	Parameters.SpawnCollisionHandlingOverride =
		ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	ChapterTwoItemContinuityDressing =
		GetWorld()->SpawnActor<AIGItemContinuityDressing>(
			AIGItemContinuityDressing::StaticClass(),
			EvidenceTransform,
			Parameters);
	if (ChapterTwoItemContinuityDressing)
	{
		ChapterTwoItemContinuityDressing->Configure(
			EIGItemContinuityPresentation::LobbyRecycleSack,
			PurchaseProfile,
			ClosureState,
			CubeMesh,
			CylinderMesh,
			SignWhiteMaterial,
			GlassMaterial,
			WaterBlueMaterial,
			SnackBlueMaterial);
		IGStory::AddState(
			this,
			FGameplayTag::RequestGameplayTag(
				FName(TEXT("State.CH02.Loop.RecycleEvidenceRestored")),
				false));
	}
}

void AIGPrologueWorldScene::HandleFlashlightPickedUp(AIGPickupItem* Item)
{
	if (UGameInstance* GameInstance = GetGameInstance())
	{
		if (UIGRebirthNarrativeSubsystem* RebirthState =
			GameInstance->GetSubsystem<UIGRebirthNarrativeSubsystem>())
		{
			RebirthState->SetHasMemoryFlashlight(true);
		}
		if (UIGSaveSubsystem* SaveSubsystem =
			GameInstance->GetSubsystem<UIGSaveSubsystem>())
		{
			const FGameplayTag EnteredStoreTag =
				FGameplayTag::RequestGameplayTag(
					FName(TEXT("State.CH02.Loop.EnteredStore")),
					false);
			const FGameplayTag LeftHomeTag =
				FGameplayTag::RequestGameplayTag(
					FName(TEXT("State.CH02.Loop.LeftHome")),
					false);
			const FGameplayTag CheckpointTag =
				IGStory::HasState(this, EnteredStoreTag)
					? FGameplayTag::RequestGameplayTag(
						FName(TEXT("Checkpoint.CH02.Store")),
						false)
					: IGStory::HasState(this, LeftHomeTag)
						? FGameplayTag::RequestGameplayTag(
							FName(TEXT("Checkpoint.CH02.Corridor")),
							false)
						: FGameplayTag::RequestGameplayTag(
							FName(TEXT("Checkpoint.CH02.Woke")),
							false);
			SaveSubsystem->RequestAutosave(
				FGameplayTag::RequestGameplayTag(
					FName(TEXT("Chapter.CH02")),
					false),
				GetWorld() ? GetWorld()->GetOutermost()->GetFName() : NAME_None,
				CheckpointTag);
		}
	}

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

void AIGPrologueWorldScene::HandlePurchaseSelectionChanged(AIGPickupItem* Item)
{
	if (Item && !bChapterTwoActive)
	{
		RefreshPurchaseProfilePresentation();
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
		MorningDirector->SetSceneReferences(
			FlickerStreetlight,
			ChapterOneApartmentExitZone);

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
	if (!bAdded || bChapterTwoTransitionPending)
	{
		return;
	}

	const FGameplayTag ChapterOnePurchase = FGameplayTag::RequestGameplayTag(
		FName(TEXT("State.CH01.Morning.WaterPurchased")), false);
	const FGameplayTag ChapterOneHasWater = FGameplayTag::RequestGameplayTag(
		FName(TEXT("State.CH01.Morning.HasWater")), false);
	const FGameplayTag ChapterOneLeftHome = FGameplayTag::RequestGameplayTag(
		FName(TEXT("State.CH01.Morning.LeftHome")), false);
	const FGameplayTag ChapterTwoEnteredAlley = FGameplayTag::RequestGameplayTag(
		FName(TEXT("State.CH02.Loop.EnteredAlley")), false);
	const FGameplayTag ChapterTwoHasWater = FGameplayTag::RequestGameplayTag(
		FName(TEXT("State.CH02.Loop.HasWater")), false);
	const FGameplayTag ChapterTwoPurchase = FGameplayTag::RequestGameplayTag(
		FName(TEXT("State.CH02.Loop.WaterPurchased")), false);
	if (bChapterTwoActive)
	{
		if (StateTag.MatchesTagExact(ChapterTwoEnteredAlley) && NeighborhoodLifeDirector)
		{
			NeighborhoodLifeDirector->PrimeOutdoorSequence();
		}
		if (StateTag.MatchesTagExact(ChapterTwoHasWater)
			|| StateTag.MatchesTagExact(ChapterTwoPurchase))
		{
			RefreshPurchaseProfilePresentation();
		}
		return;
	}
	if (StateTag.MatchesTagExact(ChapterOneLeftHome) && NeighborhoodLifeDirector)
	{
		NeighborhoodLifeDirector->PrimeOutdoorSequence();
	}
	if (StateTag.MatchesTagExact(ChapterOneHasWater))
	{
		RefreshPurchaseProfilePresentation();
	}
	if (StateTag.MatchesTagExact(ChapterOnePurchase))
	{
		RefreshPurchaseProfilePresentation();
		if (ChapterOneReceipt)
		{
			ChapterOneReceipt->SetActorHiddenInGame(false);
			ChapterOneReceipt->SetActorEnableCollision(true);
			ChapterOneReceipt->SetInteractionEnabled(true);
		}
		SpawnChapterOneIncident();
		SpawnReturnBoundary();
	}
}

void AIGPrologueWorldScene::SpawnChapterOneIncident()
{
	if (ChapterOneIncidentDirector
		|| bChapterTwoActive
		|| bChapterThreeActive
		|| !GetWorld())
	{
		return;
	}

	FActorSpawnParameters Parameters;
	Parameters.Owner = this;
	Parameters.SpawnCollisionHandlingOverride =
		ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	ChapterOneIncidentDirector =
		GetWorld()->SpawnActor<AIGChapterOneIncidentDirector>(
			AIGChapterOneIncidentDirector::StaticClass(),
			FTransform::Identity,
			Parameters);
	if (!ChapterOneIncidentDirector)
	{
		return;
	}
	ChapterOneIncidentDirector->Configure(
		this,
		NeighborhoodLifeDirector,
		CubeMesh,
		CylinderMesh,
		PlasticDarkMaterial,
		TexMat(TEXT("M_CarrierBagFilm"), GlassMaterial),
		WaterBlueMaterial,
		GetActorTransform());
	ChapterOneIncidentDirector->OnMemoryBoundaryCompleted.AddUniqueDynamic(
		this,
		&ThisClass::HandleChapterOneMemoryBoundaryCompleted);
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
	const FTransform ReturnBoundaryTransform(
		FRotator::ZeroRotator,
		FVector(643, -360, 110));
	ReturnBoundaryZone = GetWorld()->SpawnActorDeferred<AIGZoneTrigger>(
		AIGZoneTrigger::StaticClass(),
		ReturnBoundaryTransform,
		this,
		nullptr,
		ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (ReturnBoundaryZone)
	{
		ReturnBoundaryZone->SetZoneExtent(FVector(72, 52, 110));
		ReturnBoundaryZone->FinishSpawning(ReturnBoundaryTransform);
		ReturnBoundaryZone->OnZoneTriggered.AddUniqueDynamic(
			this, &ThisClass::HandleReturnBoundaryTriggered);
	}
}

void AIGPrologueWorldScene::HandleReturnBoundaryTriggered(AIGZoneTrigger* Zone)
{
	if (Zone == ReturnBoundaryZone && ChapterOneIncidentDirector)
	{
		// Crossing the actual common entrance records only 1F arrival. The
		// player still chooses stairs or lift and physically reaches 4F before
		// the shared accident convergence can own input or fade the camera.
		ChapterOneIncidentDirector->RegisterLobbyReturn();
	}
}

void AIGPrologueWorldScene::HandleChapterOneMemoryBoundaryCompleted()
{
	BeginChapterTwoTransition();
}

void AIGPrologueWorldScene::HandleElevatorReturnedToFourthFloor(
	AIGElevator* ReturnedElevator)
{
	if (ReturnedElevator != Elevator
		|| bChapterTwoActive
		|| bChapterThreeActive
		|| !ChapterOneIncidentDirector)
	{
		return;
	}
	ChapterOneIncidentDirector->RegisterFourthFloorReturn();
}

void AIGPrologueWorldScene::HandleStairTransitionCompleted(
	const bool bGoingDown)
{
	if (bChapterThreeActive)
	{
		return;
	}
	if (bGoingDown)
	{
		// Floor transfer is not the outdoor boundary. The real common-door
		// volumes own LeftHome/EnteredAlley after the player actually exits.
		return;
	}

	if (!bChapterTwoActive
		&& ChapterOneIncidentDirector
		&& IGStory::HasState(
			this,
			FGameplayTag::RequestGameplayTag(
				FName(TEXT("State.CH01.Morning.WaterPurchased")),
				false)))
	{
		ChapterOneIncidentDirector->RegisterFourthFloorReturn();
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
			// The 1.8-second first-stair memory cut already owns black. Hold
			// it through the chapter swap instead of flashing back to the
			// corridor by restarting a 0->1 fade.
			CameraManager->StartCameraFade(
				1.0f, 1.0f, 1.2f, FLinearColor::Black, false, true);
		}
	}

	GetWorldTimerManager().SetTimer(
		ChapterTransitionHandle,
		this,
		&ThisClass::EnterChapterTwo,
		1.2f,
		false);
}

void AIGPrologueWorldScene::StartRebirthEndToEndValidation()
{
	if (!bRebirthEndToEndValidation
		|| !MorningDirector
		|| !Checkout
		|| !Wallet
		|| !GetWorld())
	{
		FailRebirthEndToEndValidation(TEXT("CH01 actors unavailable"));
		return;
	}

	UGameInstance* GameInstance = GetGameInstance();
	UIGStoryStateSubsystem* StoryState = GameInstance
		? GameInstance->GetSubsystem<UIGStoryStateSubsystem>()
		: nullptr;
	UIGRebirthNarrativeSubsystem* RebirthState = GameInstance
		? GameInstance->GetSubsystem<UIGRebirthNarrativeSubsystem>()
		: nullptr;
	APlayerController* PlayerController =
		GetWorld()->GetFirstPlayerController();
	AIGPlayerCharacter* Player = PlayerController
		? Cast<AIGPlayerCharacter>(PlayerController->GetPawn())
		: nullptr;
	if (!StoryState || !RebirthState || !Player)
	{
		FailRebirthEndToEndValidation(TEXT("CH01 subsystems or player unavailable"));
		return;
	}

	StoryState->ClearStates(false);
	RebirthState->ResetNarrative();
	const auto AddStoryState = [this](const TCHAR* Name)
	{
		IGStory::AddState(
			this,
			FGameplayTag::RequestGameplayTag(FName(Name), false));
	};
	AddStoryState(TEXT("State.CH01.Wake.Standing"));
	AddStoryState(TEXT("State.CH01.Morning.FridgeChecked"));

	FIGInteractionContext Context;
	Context.Interactor = Player;
	Context.TargetActor = Wallet;
	Wallet->CompleteInteraction_Implementation(Context);
	AddStoryState(TEXT("State.CH01.Morning.LeftApartment"));
	if (!MorningDirector->RunRebirthEndToEndFirstExit())
	{
		FailRebirthEndToEndValidation(TEXT("CH01 outfit first-exit contract"));
		return;
	}
	AddStoryState(TEXT("State.CH01.Morning.LeftHome"));
	AddStoryState(TEXT("State.CH01.Morning.EnteredStore"));

	AIGPickupItem* SelectedWater = nullptr;
	for (AIGPickupItem* Candidate : WaterBottles)
	{
		if (Candidate
			&& Candidate->RebirthPurchaseProfileOnPickup
				== EIGRebirthPurchaseProfile::ProfileA500MlX2)
		{
			SelectedWater = Candidate;
			break;
		}
	}
	if (!SelectedWater)
	{
		FailRebirthEndToEndValidation(TEXT("CH01 profile A pickup unavailable"));
		return;
	}
	Context.TargetActor = SelectedWater;
	SelectedWater->CompleteInteraction_Implementation(Context);
	Context.TargetActor = Checkout;
	Checkout->CompleteInteraction_Implementation(Context);

	if (!ChapterOneIncidentDirector
		|| !ChapterOneIncidentDirector->RunRebirthEndToEndReturnRoute())
	{
		FailRebirthEndToEndValidation(TEXT("CH01 return incident route"));
		return;
	}

	const FIGRebirthChoiceState Choices = RebirthState->GetChoices();
	const bool bPassed =
		MorningDirector->GetPhase() == EIGMorningPhase::Complete
		&& Choices.PurchaseProfile
			== EIGRebirthPurchaseProfile::ProfileA500MlX2
		&& Choices.PaymentMethod == EIGRebirthPaymentMethod::WalletCard
		&& Choices.CatWaterState == EIGRebirthCatWaterState::PaperCup
		&& Choices.BottleClosureState
			== EIGRebirthBottleClosureState::Resealed
		&& Choices.bHasPaperCup
		&& Choices.bWaitedForCat
		&& RebirthState->CanConverge(
			EIGRebirthConvergencePoint::C1StorePurchase)
		&& RebirthState->CanConverge(
			EIGRebirthConvergencePoint::C2FirstReturn);
	if (!bPassed)
	{
		FailRebirthEndToEndValidation(TEXT("CH01 router state mismatch"));
		return;
	}

	UE_LOG(
		LogIndieGame,
		Display,
		TEXT(
			"REBIRTH_E2E PASS ch01_router profile=A payment=wallet "
			"cat=paper_cup waited=1 outfit_once=1 c1=1 c2=1"));
}

void AIGPrologueWorldScene::ContinueRebirthEndToEndChapterTwo()
{
	if (!bRebirthEndToEndValidation
		|| !SecondMorningDirector
		|| !SecondMorningDirector->RunRebirthEndToEndRoute())
	{
		FailRebirthEndToEndValidation(TEXT("CH02 production event route"));
		return;
	}

	const UIGRebirthNarrativeSubsystem* RebirthState =
		GetGameInstance()
			? GetGameInstance()->GetSubsystem<UIGRebirthNarrativeSubsystem>()
			: nullptr;
	const FIGRebirthChoiceState Choices =
		RebirthState ? RebirthState->GetChoices() : FIGRebirthChoiceState();
	const bool bPassed =
		RebirthState
		&& Choices.PurchaseProfile
			== EIGRebirthPurchaseProfile::ProfileA500MlX2
		&& Choices.PaymentMethod == EIGRebirthPaymentMethod::WalletCard
		&& RebirthState->WasOneShotBeatPlayed(
			FName(TEXT("CH01.BagPlacedAtLadder")))
		&& ChapterTwoItemContinuityDressing
		&& ChapterTwoItemContinuityDressing->MatchesContract(
			EIGItemContinuityPresentation::LobbyRecycleSack,
			Choices.PurchaseProfile,
			Choices.BottleClosureState)
		&& RebirthState->CanConverge(
			EIGRebirthConvergencePoint::C3SecondMorning);
	if (!bPassed)
	{
		FailRebirthEndToEndValidation(TEXT("CH02 router state mismatch"));
		return;
	}

	UE_LOG(
		LogIndieGame,
		Display,
		TEXT(
			"REBIRTH_E2E PASS ch02_router p1=1 p2=1 searched=1 "
			"outfit_once=1 human_gate=1 c3=1 purchase_preserved=1 "
			"item_continuity=1"));
}

void AIGPrologueWorldScene::FailRebirthEndToEndValidation(
	const TCHAR* Reason) const
{
	UE_LOG(
		LogIndieGame,
		Error,
		TEXT("REBIRTH_E2E FAIL reason=%s"),
		Reason ? Reason : TEXT("unknown"));
	FPlatformMisc::RequestExitWithStatus(
		false,
		1,
		TEXT("REBIRTH end-to-end validation failed"));
}

void AIGPrologueWorldScene::EnterChapterTwo()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}
	if (ChapterOneIncidentDirector)
	{
		ChapterOneIncidentDirector->OnMemoryBoundaryCompleted.RemoveDynamic(
			this,
			&ThisClass::HandleChapterOneMemoryBoundaryCompleted);
		ChapterOneIncidentDirector->Destroy();
		ChapterOneIncidentDirector = nullptr;
	}
	const bool bResumeLoadedCheckpoint =
		World->URL.HasOption(TEXT("IGResumeSave"));
	const UIGSaveGame* LoadedSave = nullptr;
	if (const UGameInstance* GameInstance = GetGameInstance())
	{
		if (const UIGSaveSubsystem* SaveSubsystem =
			GameInstance->GetSubsystem<UIGSaveSubsystem>())
		{
			LoadedSave = SaveSubsystem->GetLastLoadedSave();
		}
	}
	const FGameplayTag LoadedCheckpoint =
		LoadedSave ? LoadedSave->Progress.CheckpointTag : FGameplayTag();
	const FGameplayTag StoreCheckpoint = FGameplayTag::RequestGameplayTag(
		FName(TEXT("Checkpoint.CH02.Store")),
		false);
	const FGameplayTag WokeCheckpoint = FGameplayTag::RequestGameplayTag(
		FName(TEXT("Checkpoint.CH02.Woke")),
		false);
	const bool bResumeAtStore =
		bResumeLoadedCheckpoint
		&& LoadedCheckpoint.MatchesTagExact(StoreCheckpoint);
	const bool bResumeAtWoke =
		bResumeLoadedCheckpoint
		&& LoadedCheckpoint.MatchesTagExact(WokeCheckpoint);

	if (NeighborhoodLifeDirector)
	{
		NeighborhoodLifeDirector->SetChapterVariant(
			EIGNeighborhoodChapterVariant::ChapterTwoUncanny);
	}
	if (ChapterOneReceipt)
	{
		ChapterOneReceipt->SetActorHiddenInGame(true);
		ChapterOneReceipt->SetActorEnableCollision(false);
		ChapterOneReceipt->SetInteractionEnabled(false);
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

	if (!bResumeLoadedCheckpoint)
	{
		if (UGameInstance* GameInstance = GetGameInstance())
		{
			if (UIGStoryStateSubsystem* StoryState =
				GameInstance->GetSubsystem<UIGStoryStateSubsystem>())
			{
				StoryState->ClearStates(false);
			}
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

	bool bWalletRemainsOnDesk = true;
	if (const UGameInstance* GameInstance = GetGameInstance())
	{
		if (const UIGRebirthNarrativeSubsystem* RebirthState =
			GameInstance->GetSubsystem<UIGRebirthNarrativeSubsystem>())
		{
			bWalletRemainsOnDesk =
				RebirthState->GetChoices().PaymentMethod
				!= EIGRebirthPaymentMethod::WalletCard;
		}
	}
	if (ChapterTwoWallet)
	{
		ChapterTwoWallet->SetActorHiddenInGame(!bWalletRemainsOnDesk);
		ChapterTwoWallet->SetActorEnableCollision(false);
		ChapterTwoWallet->SetInteractionEnabled(false);
	}
	for (AIGPickupItem* WaterBottle : ChapterTwoWaterBottles)
	{
		if (WaterBottle)
		{
			// CH02 is not a second shopping errand. Keep the old shelf actors
			// parked; the one lobby sack is the authoritative recovered
			// purchase keyed to the persisted A/B/C choice.
			WaterBottle->SetActorHiddenInGame(true);
			WaterBottle->SetActorEnableCollision(false);
			WaterBottle->SetInteractionEnabled(false);
		}
	}
	SpawnChapterTwoItemContinuityDressing();
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
		Checkout->ConfigureChapterAction(
			FGameplayTag::RequestGameplayTag(
				FName(TEXT("State.CH02.Loop.CalledEmployee")), false),
			NSLOCTEXT("IGCH02", "EmployeeCallPrompt", "직원 호출 버튼 누르기"),
			NSLOCTEXT(
				"IGCH02",
				"EmployeeCallResult",
				"직원 응답 없음. [보류 거래 자동 복원] 이미 결제된 상품입니다."));
		Checkout->ResetForNewChapter();
	}
	RefreshPurchaseProfilePresentation();

	if (HomeDoor)
	{
		HomeDoor->ForceOpenState(false);
		// CH02 supports an unlit route and optional repeat investigations.
		// Keep HUD guidance, but do not turn fridge, wallet or flashlight into
		// movement locks.
		TArray<FIGDoorRequirement> NoDoorRequirements;
		HomeDoor->SetRequirements(MoveTemp(NoDoorRequirements));
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
		if (bResumeAtStore)
		{
			Elevator->RestoreAtLobbyOpen();
		}
	}

	// CH01 volumes outlive their director. Disable them before the second
	// route starts so direct CH02 runs cannot emit first-morning thoughts or
	// repopulate State.CH01 tags after the story-state reset.
	for (AIGZoneTrigger* Zone :
		{ChapterOneApartmentExitZone.Get(), LeftHomeZone.Get(),
			FlickerZone.Get(), StoreEntryZone.Get(), ReturnBoundaryZone.Get()})
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
	// The 4F homecoming anchor stays dormant until the director confirms any
	// two C3 truths. It may then close a store route or a store-skipping route.
	SetChapterTwoReturnZoneArmed(false);

	// A load always resumes at an authored safe anchor, never in a moving lift
	// or inside a one-shot trigger. Corridor saves sit on the elevator side of
	// the 4F homecoming gate so walking back to 404 still crosses it naturally.
	APlayerController* PlayerController = World->GetFirstPlayerController();
	APawn* PlayerPawn = PlayerController ? PlayerController->GetPawn() : nullptr;
	if (PlayerPawn)
	{
		const FVector ResumeLocation = bResumeLoadedCheckpoint
			? (bResumeAtStore
				? FVector(2515.0f, -455.0f, 110.0f)
				: bResumeAtWoke
					? IGPrologueWorld::PlayerLocation
					: FVector(430.0f, -305.0f, 1010.0f))
			: IGPrologueWorld::PlayerLocation;
		const FRotator ResumeActorRotation = bResumeLoadedCheckpoint
			? FRotator(0.0f, -180.0f, 0.0f)
			: IGPrologueWorld::PlayerActorRotation;
		PlayerPawn->SetActorLocationAndRotation(
			ResumeLocation,
			ResumeActorRotation,
			false,
			nullptr,
			ETeleportType::TeleportPhysics);
	}
	if (PlayerController)
	{
		PlayerController->SetControlRotation(
			bResumeLoadedCheckpoint
				? FRotator(-4.0f, -180.0f, 0.0f)
				: IGPrologueWorld::PlayerViewRotation);
	}
	// This ground-floor zone overlaps the CH01 return boundary. Enabling it
	// before the pawn is moved back upstairs consumes the one-shot while the
	// camera is black and schedules every alley event from the wrong floor.
	if (ChapterTwoOutdoorZone)
	{
		ChapterTwoOutdoorZone->SetActorEnableCollision(true);
	}

	if (AIGPlayerCharacter* Player = Cast<AIGPlayerCharacter>(PlayerPawn))
	{
		if (UIGFlashlightComponent* Torch = Player->GetFlashlight())
		{
			bool bRestoreFlashlight = false;
			if (bResumeLoadedCheckpoint)
			{
				const FGameplayTag FlashlightTag =
					FGameplayTag::RequestGameplayTag(
						FName(TEXT("State.CH02.Loop.HasFlashlight")),
						false);
				const bool bLegacyOwnsFlashlight =
					IGStory::HasState(this, FlashlightTag);
				bRestoreFlashlight = bLegacyOwnsFlashlight;
				if (UGameInstance* GameInstance = GetGameInstance())
				{
					if (UIGRebirthNarrativeSubsystem* RebirthState =
						GameInstance->GetSubsystem<UIGRebirthNarrativeSubsystem>())
					{
						const bool bCanonicalOwnsFlashlight =
							RebirthState->BuildSnapshot().bHasMemoryFlashlight;
						bRestoreFlashlight |= bCanonicalOwnsFlashlight;
						// v2 saves used only the gameplay tag; v3 uses the
						// canonical narrative field as well. Normalize both
						// directions before any actor reconciles its pickup.
						if (bRestoreFlashlight && !bCanonicalOwnsFlashlight)
						{
							RebirthState->SetHasMemoryFlashlight(true);
						}
					}
				}
				if (bRestoreFlashlight && !bLegacyOwnsFlashlight)
				{
					IGStory::AddState(this, FlashlightTag);
				}
				if (bRestoreFlashlight && Flashlight && !Flashlight->WasPickedUp())
				{
					Flashlight->ApplyRestoredPickedUpState(false);
				}
			}
			Torch->SetAvailable(bRestoreFlashlight);
			Torch->RefillBattery(1.0f);
		}
	}

	FActorSpawnParameters DirectorParameters;
	DirectorParameters.Owner = this;
	ChapterTwoHumanGateDirector =
		World->SpawnActorDeferred<AIGChapterTwoHumanGateDirector>(
			AIGChapterTwoHumanGateDirector::StaticClass(),
			GetActorTransform(),
			this,
			nullptr,
			ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (ChapterTwoHumanGateDirector)
	{
		ChapterTwoHumanGateDirector->Configure(
			ApartmentStoryDressing
				? ApartmentStoryDressing->GetPhoneInspectable()
				: nullptr,
			CubeMesh,
			PlasticDarkMaterial,
			SignWhiteMaterial,
			ScreenGlowMaterial);
		ChapterTwoHumanGateDirector->FinishSpawning(GetActorTransform());
	}

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
			ChapterTwoHumanGateDirector,
			MirrorRoomDoor,
			MirrorRoomLamp,
			Elevator,
			StoreDoor,
			JingleComponent,
			ExistingReceipt,
			DuplicateReceipt,
			ApartmentStoryDressing
				? ApartmentStoryDressing->GetPlannerNote()
				: nullptr,
			MirrorAlarmMemo,
			MailboxBills,
			OfferingNote,
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
	if (!bCaptureChapterTwo && !bResumeLoadedCheckpoint)
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
		if (bResumeLoadedCheckpoint)
		{
			WakeDirector->RestoreStandingCheckpoint();
		}
		else
		{
			WakeDirector->ResetForNewChapter();
			WakeDirector->StartWakeUp();
			if (bCaptureChapterTwo)
			{
				WakeDirector->RestoreStandingCheckpoint();
			}
		}
	}

	if (bCaptureChapterTwo)
	{
		StartChapterTwoCaptureSequence();
	}
	if (bRebirthEndToEndValidation)
	{
		GetWorldTimerManager().SetTimer(
			RebirthEndToEndHandle,
			this,
			&ThisClass::ContinueRebirthEndToEndChapterTwo,
			0.25f,
			false);
	}

	UE_LOG(LogIndieGame, Display, TEXT("CH02 second morning entered in-session."));
}

void AIGPrologueWorldScene::EnterChapterThree()
{
	if (bChapterThreeActive || !GetWorld())
	{
		return;
	}
	const bool bResumeLoadedCheckpoint =
		GetWorld()->URL.HasOption(TEXT("IGResumeSave"));
	FGameplayTag LoadedCheckpoint;
	if (bResumeLoadedCheckpoint)
	{
		if (const UGameInstance* GameInstance = GetGameInstance())
		{
			if (const UIGSaveSubsystem* SaveSubsystem =
				GameInstance->GetSubsystem<UIGSaveSubsystem>())
			{
				if (const UIGSaveGame* LoadedSave =
					SaveSubsystem->GetLastLoadedSave())
				{
					LoadedCheckpoint = LoadedSave->Progress.CheckpointTag;
				}
			}
		}
	}
	bChapterThreeActive = true;
	bChapterTwoTransitionPending = false;
	// The 7/29 state moves the same bowl and note upstairs. Retire their CH02
	// placement before the fifth-floor copy is spawned so both cannot coexist.
	SetChapterTwoOverlayVisible(false);

	if (AIGReadableNote* OpenNote = AIGReadableNote::GetOpenNote())
	{
		OpenNote->Close();
	}
	if (DistantAlarmComponent)
	{
		DistantAlarmComponent->Stop();
		DistantAlarmComponent = nullptr;
	}
	if (JingleComponent)
	{
		JingleComponent->Stop();
	}
	if (NeighborhoodLifeDirector)
	{
		NeighborhoodLifeDirector->SetChapterVariant(
			EIGNeighborhoodChapterVariant::ChapterTwoAbsent);
	}

	// Retire chapter scripts and every one-shot volume.  The stage itself is
	// far away so the sky/fog remain shared, while stale 404 logic cannot add
	// CH01/02 state tags during the ending.
	for (AActor* Director :
		{static_cast<AActor*>(MorningDirector.Get()),
			static_cast<AActor*>(SecondMorningDirector.Get()),
			static_cast<AActor*>(ChapterTwoHumanGateDirector.Get()),
			static_cast<AActor*>(WakeDirector.Get()),
			static_cast<AActor*>(AlarmClock.Get()),
			static_cast<AActor*>(GetUpTarget.Get())})
	{
		if (Director)
		{
			Director->Destroy();
		}
	}
	MorningDirector = nullptr;
	SecondMorningDirector = nullptr;
	ChapterTwoHumanGateDirector = nullptr;
	WakeDirector = nullptr;
	AlarmClock = nullptr;
	GetUpTarget = nullptr;
	if (ChapterTwoItemContinuityDressing)
	{
		ChapterTwoItemContinuityDressing->Destroy();
		ChapterTwoItemContinuityDressing = nullptr;
	}
	for (AIGZoneTrigger* Zone :
		{ChapterOneApartmentExitZone.Get(), LeftHomeZone.Get(),
			FlickerZone.Get(), StoreEntryZone.Get(), ReturnBoundaryZone.Get(),
			ChapterTwoLeftHomeZone.Get(),
			ChapterTwoOutdoorZone.Get(), MirrorSightZone.Get(), MirrorEntryZone.Get(),
			ChapterTwoStoreEntryZone.Get(), ChapterTwoReturnZone.Get()})
	{
		if (Zone)
		{
			Zone->SetActorEnableCollision(false);
		}
	}

	if (UGameInstance* GameInstance = GetGameInstance())
	{
		if (UIGStoryStateSubsystem* StoryState =
			GameInstance->GetSubsystem<UIGStoryStateSubsystem>())
		{
			// Preserve the CH02 receipt comparison before retiring the legacy
			// chapter event bus. CH03 consumes the REBIRTH truth record after
			// the old per-chapter tags have been cleared.
			const FGameplayTag DuplicateReceiptTag =
				FGameplayTag::RequestGameplayTag(
					FName(TEXT("State.CH02.Loop.ReadDuplicateReceipt")),
					false);
			if (StoryState->HasState(DuplicateReceiptTag))
			{
				if (UIGRebirthNarrativeSubsystem* RebirthState =
					GameInstance->GetSubsystem<UIGRebirthNarrativeSubsystem>())
				{
					RebirthState->RegisterTruthSource(
						FGameplayTag::RequestGameplayTag(
							FName(TEXT("Truth.DeathOverlay")),
							false),
						FName(TEXT("CH02.DuplicateReceipt")));
				}
			}
			StoryState->ClearStates(false);
		}
	}

	FActorSpawnParameters Parameters;
	Parameters.Owner = this;
	Parameters.SpawnCollisionHandlingOverride =
		ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	ThirdMorningDirector = GetWorld()->SpawnActor<AIGThirdMorningDirector>(
		AIGThirdMorningDirector::StaticClass(),
		FTransform(
			FRotator::ZeroRotator,
			AIGThirdMorningDirector::GetStageOrigin()),
		Parameters);
	if (!ThirdMorningDirector)
	{
		UE_LOG(LogIndieGame, Error, TEXT("CH03 director failed to spawn."));
		return;
	}

	const bool bCapture =
		FParse::Param(FCommandLine::Get(), TEXT("IGCaptureCH03"));
	const bool bDirectStart =
		FParse::Param(FCommandLine::Get(), TEXT("IGChapterThree"))
		|| bCapture
		|| GetWorld()->URL.HasOption(TEXT("IGChapterThree"));
	ThirdMorningDirector->ConfigureAndStart(bDirectStart && !bCapture, bCapture);
	if (bResumeLoadedCheckpoint)
	{
		ThirdMorningDirector->RestoreCheckpointAnchor(LoadedCheckpoint);
	}
	if (bRebirthEndToEndValidation)
	{
		const UIGRebirthNarrativeSubsystem* RebirthState =
			GetGameInstance()
				? GetGameInstance()->GetSubsystem<UIGRebirthNarrativeSubsystem>()
				: nullptr;
		if (!RebirthState
			|| !RebirthState->CanConverge(
				EIGRebirthConvergencePoint::C3SecondMorning))
		{
			FailRebirthEndToEndValidation(TEXT("CH03 handoff lost CH01/CH02 state"));
			return;
		}
		UE_LOG(
			LogIndieGame,
			Display,
			TEXT(
				"REBIRTH_E2E PASS ch03_handoff c1=1 c2=1 c3=1 "
				"same_session=1"));
	}

	UE_LOG(LogIndieGame, Display, TEXT("CH03 third morning entered in-session."));
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
	if (AHUD* HUD = PlayerController->GetHUD())
	{
		// Environment stills stay clean; the receipt frame deliberately turns
		// the real HUD back on because that panel is the product being shown.
		HUD->bShowHUD = ChapterCaptureIndex == 2;
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
			MirrorRoomLamp->SetIntensity(620.0f);
		}
		if (MirrorRoomBounce)
		{
			MirrorRoomBounce->SetVisibility(true);
			MirrorRoomBounce->SetIntensity(360.0f);
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
		// 401 after the management reply: swept salt, a dry bowl ring and clear
		// water outside the line. Rice, spoon and incense are already gone.
		if (OfferingLight)
		{
			OfferingLight->SetVisibility(true);
			OfferingLight->SetIntensity(195.0f);
		}
		PlaceCaptureCamera(
			FVector(-150, -360, IGPrologueWorld::FourthFloorZ),
			FVector(-150, -276, IGPrologueWorld::FourthFloorZ + 5));
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
	// documentation frame. This is especially visible when the first two
	// captures jump between the 403 interior and the 401 corridor threshold.
	const TWeakObjectPtr<AIGPrologueWorldScene> WeakThis(this);
	FTimerDelegate CaptureDelegate;
	CaptureDelegate.BindLambda([WeakThis, ScreenshotPath]()
	{
		AIGPrologueWorldScene* Scene = WeakThis.Get();
		if (!Scene)
		{
			return;
		}

		FScreenshotRequest::RequestScreenshot(ScreenshotPath, true, false);
		UE_LOG(
			LogIndieGame,
			Display,
			TEXT("CH02 capture requested: %s"),
			*ScreenshotPath);

		Scene->GetWorldTimerManager().SetTimer(
			Scene->ChapterCaptureHandle,
			Scene,
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

	const TWeakObjectPtr<AIGPrologueWorldScene> WeakThis(this);
	FTimerDelegate ValidationDelegate;
	ValidationDelegate.BindLambda([WeakThis]()
	{
		AIGPrologueWorldScene* Scene = WeakThis.Get();
		if (!Scene)
		{
			return;
		}
		const bool bRideCompleted =
			Scene->Elevator && Scene->Elevator->IsRideComplete();
		APlayerController* Controller =
			Scene->GetWorld()
				? Scene->GetWorld()->GetFirstPlayerController()
				: nullptr;
		APawn* Pawn = Controller ? Controller->GetPawn() : nullptr;
		if (!bRideCompleted || !Scene->Elevator || !Pawn)
		{
			UE_LOG(
				LogIndieGame,
				Error,
				TEXT("CH02 elevator descent validation failed."));
			FPlatformMisc::RequestExit(false);
			return;
		}

		// Exercise the route that closes the actual chapter: call the same cab
		// at 1F, board it, transfer only behind shut doors, and reopen at 4F.
		Pawn->SetActorLocation(
			FVector(625, -305, 110),
			false,
			nullptr,
			ETeleportType::TeleportPhysics);
		FIGInteractionContext ReturnContext;
		ReturnContext.Interactor = Pawn;
		ReturnContext.TargetActor = Scene->Elevator;
		ReturnContext.HoldProgress = 1.0f;
		IIGInteractable::Execute_CompleteInteraction(
			Scene->Elevator,
			ReturnContext);

		const TWeakObjectPtr<APawn> WeakReturnPawn = Pawn;
		FTimerDelegate BoardReturnDelegate;
		BoardReturnDelegate.BindLambda([WeakReturnPawn]()
		{
			if (APawn* ReturnPawn = WeakReturnPawn.Get())
			{
				ReturnPawn->SetActorLocation(
					FVector(795, -305, 110),
					false,
					nullptr,
					ETeleportType::TeleportPhysics);
			}
		});
		FTimerHandle BoardReturnHandle;
		Scene->GetWorldTimerManager().SetTimer(
			BoardReturnHandle,
			BoardReturnDelegate,
			1.0f,
			false);

		FTimerDelegate ReturnValidationDelegate;
		ReturnValidationDelegate.BindLambda([WeakThis, WeakReturnPawn]()
		{
			AIGPrologueWorldScene* ReturnScene = WeakThis.Get();
			if (!ReturnScene)
			{
				return;
			}
			const APawn* ReturnPawn = WeakReturnPawn.Get();
			const bool bReturned =
				ReturnScene->Elevator
				&& ReturnScene->Elevator->HasReturnedToFourthFloor()
				&& ReturnPawn
				&& ReturnPawn->GetActorLocation().Z > 900.0f;
			if (bReturned)
			{
				UE_LOG(
					LogIndieGame,
					Display,
					TEXT("CH02 elevator roundtrip validation complete."));
			}
			else
			{
				UE_LOG(
					LogIndieGame,
					Error,
					TEXT("CH02 elevator return validation failed."));
			}
			FPlatformMisc::RequestExit(false);
		});
		Scene->GetWorldTimerManager().SetTimer(
			Scene->ChapterCaptureHandle,
			ReturnValidationDelegate,
			10.0f,
			false);
	});
	GetWorldTimerManager().SetTimer(
		ChapterCaptureHandle,
		ValidationDelegate,
		18.0f,
		false);
	UE_LOG(LogIndieGame, Display, TEXT("CH02 elevator roundtrip validation started."));
}

void AIGPrologueWorldScene::HandleFlickerZoneTriggered(AIGZoneTrigger* Zone)
{
	if (MorningDirector)
	{
		MorningDirector->TriggerAlleyLightFailure();
	}
}
