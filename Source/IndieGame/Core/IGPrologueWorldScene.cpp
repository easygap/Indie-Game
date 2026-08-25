#include "Core/IGPrologueWorldScene.h"
#include "Accessibility/IGAccessibilitySubsystem.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetCompilingManager.h"
#include "Audio/IGAlarmSoundWave.h"
#include "Audio/IGAmbienceSoundWave.h"
#include "Audio/IGMissingFloorAudioSubsystem.h"
#include "Audio/IGToneSequenceSoundWave.h"
#include "Camera/CameraComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/AudioComponent.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Environment/IGSettledDustComponent.h"
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

	// Unit 403 sits on the 4th floor, three slabs above the street.
	constexpr float FourthFloorZ = 900.0f;
	constexpr float MissingFloorRoofZ = 1200.0f;
	constexpr int32 MissingFloorUpperStepCount = 14;
	constexpr float MissingFloorRouteLengthCentimeters = 640.0f;
	const FVector MissingFloorRouteStart(-277.5f, 220.0f, MissingFloorRoofZ);
	const FVector MissingFloorRouteCorner(130.0f, 220.0f, MissingFloorRoofZ);
	const FVector MissingFloorRouteEnd(130.0f, 452.5f, MissingFloorRoofZ);

	/**
	 * §14 CCTV 채널 5's vantage — corner-mounted high in the annex, looking
	 * diagonally across the stalled material toward bay B. 92° is a 2.8 mm lens
	 * on a third-inch sensor, which is what actually gets installed in a corridor
	 * this narrow, and it is wide enough that the low shape can cross an edge
	 * instead of walking through the middle of the picture.
	 *
	 * §17 requires the player to recognise this framing by eye in 밤3, so the
	 * camera prop, the scene capture and the night-3 observation point all read
	 * these three values. There is exactly one shot and this is it.
	 */
	const FVector CctvCameraLocation(-355.0f, 486.0f, 1404.0f);
	const FRotator CctvCameraRotation(-25.0f, 29.0f, 0.0f);
	/**
	 * 78° is a 4 mm lens, not the 2.8 mm the first pass used. The wider glass put
	 * a third of the frame on the ceiling 36 cm above the housing, which measured
	 * as a perfectly bright picture and showed nothing. Judged from the exported
	 * frame at Docs/Media/cctv5-feed.png, not from the focal length.
	 */
	constexpr float CctvCameraFieldOfView = 78.0f;

	constexpr float BedsideTableTopZ = 60.0f;
	constexpr float DeskWorkSurfaceHeight = 74.0f;
	// The generated mesh audit fixes the continuous chassis at local Z=-0.40 cm.
	// Sink it by 1 mm into the measured tabletop instead of adding an air gap.
	// The tabletop scan is
	// uniformly fitted, so its actual height is read
	// from component bounds in BuildApartment rather than assumed to be 60 cm.
	constexpr float AlarmContactBottomLocalZ = -0.40f;
	constexpr float PropContactEmbedZ = 0.10f;
	const FVector AlarmHorizontalLocation(-160.0f, -35.0f, 0.0f);
	const FVector GetUpTargetLocation(-140.0f, 183.0f, FourthFloorZ + 58.0f);
	const FVector FridgeLocation(155.0f, -20.0f, FourthFloorZ);
	const FVector HomeDoorLocation(101.0f, -225.0f, FourthFloorZ);
	// Far end of the hallway, so leaving 403 is a walk rather than a step.
	const FVector ElevatorLocation(790.0f, -305.0f, FourthFloorZ);
	const FVector StoreDoorLocation(2405.0f, -457.0f, 6.0f);
	const FVector CheckoutLocation(2620.0f, -255.0f, 96.0f);
	const FVector WalletHorizontalLocation(-125.0f, -183.0f, 0.0f);

	const FName PurchaseBagProxyTag(TEXT("REBIRTH.PurchaseBagProxy"));
	const FName NotFoundEasterEggTag(TEXT("EasterEgg.404NotFound"));
	const FName CatWaterAftermathTag(TEXT("REBIRTH.CatWaterAftermath.CH02"));
	const FName CatWaterCapTag(TEXT("REBIRTH.CatWaterAftermath.Cap"));
	const FName CatWaterCupTag(TEXT("REBIRTH.CatWaterAftermath.Cup"));
	const FName CatWaterWetRingTag(TEXT("REBIRTH.CatWaterAftermath.WetRing"));
	const FName FootstepVinylTag(TEXT("Footstep.Vinyl"));
	const FName FootstepConcreteTag(TEXT("Footstep.Concrete"));
	const FName FootstepMetalStairTag(TEXT("Footstep.MetalStair"));
	const FName FootstepRooftopTag(TEXT("Footstep.Rooftop"));
	const FName FootstepGypsumTag(TEXT("Footstep.GypsumDebris"));

	void TagFootstepSurface(UStaticMeshComponent* Component, const FName Tag)
	{
		if (Component && !Component->ComponentHasTag(Tag))
		{
			Component->ComponentTags.Add(Tag);
		}
	}

	// These products are never interacted with; the evidence bottles spawned
	// later remain individual actors. At 16 m a 5-20 cm package is already a
	// handful of pixels, so it can fade before the store itself disappears.
	constexpr int32 StoreStockCullStartCentimeters = 1600;
	constexpr int32 StoreStockCullEndCentimeters = 2200;
	// 627 fixed aisle/chilled-food instances plus 524 cooler instances.  The
	// cooler total counts each PET/glass bottle's body, cap and label as three
	// render instances while cans and cartons are single instances.
	constexpr int32 ExpectedStoreStockInstances = 1151;
	constexpr int32 MaximumStoreStockBatches = 24;

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
	AlarmWorldLocation = FVector(
		IGPrologueWorld::AlarmHorizontalLocation.X,
		IGPrologueWorld::AlarmHorizontalLocation.Y,
		IGPrologueWorld::FourthFloorZ + IGPrologueWorld::BedsideTableTopZ
			- IGPrologueWorld::AlarmContactBottomLocalZ
			- IGPrologueWorld::PropContactEmbedZ);
	DeskSurfaceWorldZ = IGPrologueWorld::FourthFloorZ
		+ IGPrologueWorld::DeskWorkSurfaceHeight;
	WalletWorldLocation = FVector(
		IGPrologueWorld::WalletHorizontalLocation.X,
		IGPrologueWorld::WalletHorizontalLocation.Y,
		DeskSurfaceWorldZ + 1.4f);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMeshFinder(
		TEXT("/Engine/BasicShapes/Cube.Cube"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderMeshFinder(
		TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> PlaneMeshFinder(
		TEXT("/Engine/BasicShapes/Plane.Plane"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereMeshFinder(
		TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> ConeMeshFinder(
		TEXT("/Engine/BasicShapes/Cone.Cone"));
	CubeMesh = CubeMeshFinder.Object;
	CylinderMesh = CylinderMeshFinder.Object;
	PlaneMesh = PlaneMeshFinder.Object;
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
	// The apartment deliberately contains a hot tungsten pool and an almost
	// unlit wardrobe in the same frame. Bilateral local exposure preserves the
	// wallpaper emboss and furniture silhouette without raising the global
	// exposure until the night scene looks like daylight.
	PostProcess->Settings.bOverride_LocalExposureMethod = true;
	PostProcess->Settings.LocalExposureMethod = ELocalExposureMethod::Bilateral;
	PostProcess->Settings.bOverride_LocalExposureHighlightContrastScale = true;
	PostProcess->Settings.LocalExposureHighlightContrastScale = 0.84f;
	PostProcess->Settings.bOverride_LocalExposureShadowContrastScale = true;
	// Preserve readable floor and door silhouettes in the unlit corridor while
	// local detail enhancement carries the plaster response inside the beam.
	PostProcess->Settings.LocalExposureShadowContrastScale = 0.76f;
	PostProcess->Settings.bOverride_LocalExposureDetailStrength = true;
	PostProcess->Settings.LocalExposureDetailStrength = 1.12f;
	PostProcess->Settings.bOverride_LocalExposureBlurredLuminanceBlend = true;
	PostProcess->Settings.LocalExposureBlurredLuminanceBlend = 0.52f;
	PostProcess->Settings.bOverride_LocalExposureBlurredLuminanceKernelSizePercent = true;
	PostProcess->Settings.LocalExposureBlurredLuminanceKernelSizePercent = 48.0f;
	PostProcess->Settings.bOverride_LocalExposureMiddleGreyBias = true;
	PostProcess->Settings.LocalExposureMiddleGreyBias = -0.18f;
	// Grade stays close to neutral; the atmosphere supplies the palette.
	PostProcess->Settings.bOverride_VignetteIntensity = true;
	PostProcess->Settings.VignetteIntensity = 0.17f;
	PostProcess->Settings.bOverride_FilmGrainIntensity = true;
	PostProcess->Settings.FilmGrainIntensity = 0.02f;
	PostProcess->Settings.bOverride_ColorSaturation = true;
	PostProcess->Settings.ColorSaturation = FVector4(0.93f, 0.95f, 1.0f, 1.0f);
	// A restrained film curve gives PBR roughness and normal changes somewhere
	// to read.  The toe is kept below the engine default so the unlit corridor
	// retains material information instead of crushing into a single black,
	// while the shoulder rolls practicals off before their fixture detail clips.
	PostProcess->Settings.bOverride_ColorContrast = true;
	PostProcess->Settings.ColorContrast = FVector4(1.025f, 1.025f, 1.025f, 1.0f);
	PostProcess->Settings.bOverride_FilmSlope = true;
	PostProcess->Settings.FilmSlope = 0.90f;
	PostProcess->Settings.bOverride_FilmToe = true;
	PostProcess->Settings.FilmToe = 0.53f;
	PostProcess->Settings.bOverride_FilmShoulder = true;
	PostProcess->Settings.FilmShoulder = 0.24f;
	PostProcess->Settings.bOverride_FilmBlackClip = true;
	PostProcess->Settings.FilmBlackClip = 0.0f;
	PostProcess->Settings.bOverride_FilmWhiteClip = true;
	PostProcess->Settings.FilmWhiteClip = 0.035f;
	// Restrained bloom keeps emissive signage readable instead of hazy.
	PostProcess->Settings.bOverride_BloomIntensity = true;
	PostProcess->Settings.BloomIntensity = 0.18f;
	// Low motion blur: walking stays smooth but frames remain readable.
	PostProcess->Settings.bOverride_MotionBlurAmount = true;
	PostProcess->Settings.MotionBlurAmount = 0.05f;
	// Ambient occlusion seats furniture and shelf stock into their corners.
	PostProcess->Settings.bOverride_AmbientOcclusionIntensity = true;
	PostProcess->Settings.AmbientOcclusionIntensity = 0.48f;
	PostProcess->Settings.bOverride_AmbientOcclusionRadius = true;
	PostProcess->Settings.AmbientOcclusionRadius = 48.0f;
	PostProcess->Settings.bOverride_LumenAmbientOcclusionIntensity = true;
	PostProcess->Settings.LumenAmbientOcclusionIntensity = 0.55f;
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

UStaticMeshComponent* AIGPrologueWorldScene::CreatePrintedBlock(
	const FVector& Center,
	const FVector& SizeCentimeters,
	UMaterialInterface* BodyMaterial,
	UMaterialInterface* PrintMaterial,
	const FVector& PrintFacing,
	const bool bEnableCollision,
	const bool bPrintBothFaces)
{
	UStaticMeshComponent* Body =
		CreateBlock(Center, SizeCentimeters, BodyMaterial, bEnableCollision);
	if (!Body || !PrintMaterial)
	{
		return Body;
	}

	// 인쇄판은 4 mm다. 이 씬이 게시물·명판·가격표에 이미 쓰는 두께이고,
	// 그만한 마구리는 어느 각도에서도 두 번째 인쇄로 읽히지 않는다.
	constexpr float PlateThickness = 0.4f;
	const FVector Facing = PrintFacing.GetSafeNormal();
	int32 Axis = 0;
	for (int32 Index = 1; Index < 3; ++Index)
	{
		if (FMath::Abs(Facing[Index]) > FMath::Abs(Facing[Axis]))
		{
			Axis = Index;
		}
	}
	if (FMath::Abs(Facing[Axis]) < 0.99f)
	{
		// 축에 붙지 않은 방향은 이 방식으로 덮을 수 없다. 예전처럼 몸통에
		// 인쇄를 주고, 호출부가 알아채도록 이름을 남긴다.
		Body->SetMaterial(0, PrintMaterial);
		UE_LOG(
			LogIndieGame,
			Warning,
			TEXT("IG_PRINTED_BLOCK facing is not axis aligned: %s"),
			*Facing.ToString());
		return Body;
	}

	FVector PlateSize = SizeCentimeters;
	PlateSize[Axis] = PlateThickness;
	FVector Step = FVector::ZeroVector;
	Step[Axis] = (SizeCentimeters[Axis] + PlateThickness) * 0.5f
		* FMath::Sign(Facing[Axis]);

	CreateBlock(Center + Step, PlateSize, PrintMaterial, false);
	if (bPrintBothFaces)
	{
		// 큐브 여섯 면의 UV 손잡이는 모두 같다(Scripts/probe_cube_face_uvs.py로
		// 실측). 반대쪽 판을 돌릴 필요 없이 같은 자세로 한 장 더 붙이면
		// 양면 간판이 양쪽에서 똑같이 읽힌다.
		CreateBlock(Center - Step, PlateSize, PrintMaterial, false);
	}
	return Body;
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
	Light->ContactShadowLength = bCastShadows ? 0.12f : 0.0f;
	Light->ContactShadowLengthInWS = false;
	Light->ShadowSharpen = 0.0f;
	// Do not mute the BRDF at the light.  Material roughness and specular now
	// own highlight width/energy, so brushed steel, plastic film and plaster no
	// longer receive the same flattened response.
	Light->SetSpecularScale(1.0f);
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
		TEXT("M_WallpaperCeil"), TEXT("M_ApartmentWallPatina"),
		// 403's paper. TexMat only reads this map, so a material missing from
		// this list is a material the scene can never reach.
		TEXT("M_WallpaperEmboss_X"), TEXT("M_WallpaperEmboss_Y"),
		TEXT("M_WoodFurnitureUV"), TEXT("M_BeddingUV"),
		TEXT("M_AsphaltWorld"), TEXT("M_Brick_X"), TEXT("M_Brick_Y"),
		TEXT("M_VillaStucco_X"), TEXT("M_VillaStucco_Y"),
		TEXT("M_Concrete_XY"), TEXT("M_Concrete_X"), TEXT("M_Concrete_Y"),
		TEXT("M_ConcreteDark_X"), TEXT("M_ConcreteDark_Y"),
		TEXT("M_ConcreteDark_XY"),
		TEXT("M_StoreTileWorld"), TEXT("M_StoreCeilWorld"),
		TEXT("M_StoreWall_X"), TEXT("M_StoreWall_Y"),
		TEXT("M_MetalUV"), TEXT("M_ShelfSteelUV"),
		TEXT("M_PosterSale"), TEXT("M_PosterRamyeon"), TEXT("M_PosterFlyer"),
		TEXT("M_NoteFridge"), TEXT("M_Note404NotFound"),
		TEXT("M_SignToilet"), TEXT("M_SignAutoDoor"),
		TEXT("M_PriceStrip"), TEXT("M_SignMainLit"), TEXT("M_SignBladeLit"),
		TEXT("M_SignVilla"), TEXT("M_Plate401"), TEXT("M_Plate402"),
		TEXT("M_Plate403"), TEXT("M_PlateCommon"),
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
		TEXT("M_SteelDoorUV"), TEXT("M_UnitDoorPaintedSteel"),
		TEXT("M_KitchenGlossUV"), TEXT("M_CounterStoneUV"),
		TEXT("M_DoorLock"), TEXT("M_MeterBox"), TEXT("M_UtilityMeterDial"),
		TEXT("M_Intercom"),
		TEXT("M_LiftCOP"), TEXT("M_LiftHall"), TEXT("M_SwitchPlate"),
		// 「없는 층」 dry-plaster architecture and authored residue layers.
		TEXT("M_MissingFloorPlaster_X"), TEXT("M_MissingFloorPlaster_Y"),
		TEXT("M_MissingFloorPlaster_XY"), TEXT("M_MissingFloorHandprints"),
		TEXT("M_MissingFloorDragTrails"), TEXT("M_MissingFloorDustJoint"),
		TEXT("M_MissingFloorCavityScratches"),
		TEXT("M_DecalDampWallpaper"), TEXT("M_DecalRustFasteners"),
		// §11 규칙 2가 고르게 만드는 발소리 표면들. 이름이 여기 없으면
		// 소리만 다르고 그림은 복도 콘크리트 그대로다.
		TEXT("M_MissingFloorSteelStair"), TEXT("M_RooftopWaterproofing_XY"),
		TEXT("M_MissingFloorGypsumDebris_XY"),
		TEXT("M_WaterTankMetalUV"), TEXT("M_TankWaterReveal"),
		TEXT("M_SpriteSeo"), TEXT("M_SpriteMok"),
		TEXT("M_SpriteHwang"), TEXT("M_SpriteNarin"),
		// Aged paper stock for readable notes, and the rental notice.
		TEXT("M_PaperClean"), TEXT("M_PaperWet"), TEXT("M_PaperFolded"),
		TEXT("M_PaperOld"), TEXT("M_NoticeRent"), TEXT("M_WetStep"),
		TEXT("M_MovingBoxCardboardUV"),
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

bool AIGPrologueWorldScene::AddStoreStockInstance(
	UStaticMesh* Mesh,
	UMaterialInterface* Material,
	const FTransform& RelativeTransform,
	const bool bCastShadow)
{
	if (!Mesh)
	{
		return false;
	}

	USceneComponent* ResolvedParent =
		ActiveParent ? ActiveParent.Get() : SceneRoot.Get();
	const FString BatchKey = FString::Printf(
		TEXT("%s|%s|%s|shadow=%d"),
		*Mesh->GetPathName(),
		*GetPathNameSafe(Material),
		*GetPathNameSafe(ResolvedParent),
		bCastShadow ? 1 : 0);

	UInstancedStaticMeshComponent* Batch = nullptr;
	if (const TObjectPtr<UInstancedStaticMeshComponent>* Existing =
			StoreStockBatches.Find(BatchKey))
	{
		Batch = Existing->Get();
	}
	else
	{
		Batch = NewObject<UInstancedStaticMeshComponent>(
			this,
			*FString::Printf(
				TEXT("StoreStockBatch_%02d"),
				StoreStockBatches.Num()));
		Batch->SetupAttachment(ResolvedParent);
		Batch->SetStaticMesh(Mesh);
		Batch->SetMaterial(0, Material);
		Batch->SetMobility(EComponentMobility::Static);
		Batch->SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);
		Batch->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Batch->SetGenerateOverlapEvents(false);
		Batch->SetCanEverAffectNavigation(false);
		Batch->SetReceivesDecals(false);
		Batch->SetCastShadow(bCastShadow);
		// Tiny packages do not justify entries in the Lumen distance-field scene.
		// They still receive direct light, material response and screen traces.
		Batch->SetAffectDistanceFieldLighting(false);
		// Start/end are kept as one authoring contract so a stock material may
		// consume PerInstanceFadeAmount later. Opaque materials that do not use
		// that node are still GPU-culled at the end distance; do not describe
		// this interval as a visual blend until the material has been verified.
		Batch->SetCullDistances(
			IGPrologueWorld::StoreStockCullStartCentimeters,
			IGPrologueWorld::StoreStockCullEndCentimeters);
		StoreStockBatches.Add(BatchKey, Batch);
	}

	return Batch && Batch->AddInstance(RelativeTransform) != INDEX_NONE;
}

bool AIGPrologueWorldScene::AddStoreStockProp(
	const TCHAR* MeshName,
	const FVector& BaseLocation,
	UMaterialInterface* Material,
	const float YawDegrees,
	const float UniformScale,
	const bool bCastShadow)
{
	return AddStoreStockInstance(
		PropMesh(MeshName),
		Material,
		FTransform(
			FRotator(0.0f, YawDegrees, 0.0f),
			BaseLocation,
			FVector(UniformScale)),
		bCastShadow);
}

void AIGPrologueWorldScene::AddStoreStockBlock(
	const FVector& Center,
	const FVector& SizeCentimeters,
	UMaterialInterface* Material,
	const bool bCastShadow,
	const FRotator& Rotation)
{
	const bool bAdded = AddStoreStockInstance(
		CubeMesh,
		Material,
		FTransform(Rotation, Center, SizeCentimeters / 100.0f),
		bCastShadow);
	ensureMsgf(bAdded, TEXT("The store-stock cube fallback must always be available."));
}

void AIGPrologueWorldScene::AddStoreStockCup(
	const FVector& BaseLocation,
	const float YawDegrees)
{
	// Foam cup body. The fallback keeps the batch contract intact when an
	// artist is rebuilding an authored mesh locally.
	if (!AddStoreStockProp(
			TEXT("SM_CupNoodle"),
			BaseLocation,
			CupNoodleMaterial,
			YawDegrees))
	{
		AddStoreStockBlock(
			BaseLocation + FVector(0.0f, 0.0f, 5.5f),
			FVector(10.8f, 10.8f, 11.0f),
			CupNoodleMaterial,
			true,
			FRotator(0.0f, YawDegrees, 0.0f));
	}

	// The wrapper is authored at the cup's real dimensions and carries one
	// explicit 360-degree UV seam. Runtime scaling would reintroduce the broken
	// vertical strips that made the printed film look torn.
	UMaterialInterface* LabelMaterial =
		TexMat(TEXT("M_LabelRamyeon"), CupNoodleMaterial);
	const bool bAddedSleeve = AddStoreStockInstance(
			PropMesh(TEXT("SM_CupSleeve")),
			LabelMaterial,
			FTransform(
				FRotator(0.0f, YawDegrees, 0.0f),
				BaseLocation + FVector(0.0f, 0.0f, 1.6f),
				FVector::OneVector),
			false);
	ensureMsgf(
		bAddedSleeve,
		TEXT("SM_CupSleeve is release-required; a flat box is not a valid wrapper."));

	UMaterialInterface* LidMaterial =
		TexMat(TEXT("M_StainlessUV"), MetalFrameMaterial);
	if (!AddStoreStockProp(
			TEXT("SM_CupLid"),
			BaseLocation,
			LidMaterial,
			YawDegrees,
			1.0f,
			false))
	{
		AddStoreStockBlock(
			BaseLocation + FVector(0.0f, 0.0f, 10.85f),
			FVector(10.9f, 10.9f, 0.3f),
			LidMaterial,
			false,
			FRotator(0.0f, YawDegrees, 0.0f));
	}
}

void AIGPrologueWorldScene::AddStoreStockBottleLabel(
	const FVector& BottleBase,
	const float Radius,
	const float BandBottomZ,
	const float BandHeight,
	const TCHAR* LabelMaterialName,
	const float YawDegrees)
{
	UMaterialInterface* LabelMaterial =
		TexMat(LabelMaterialName, FridgeInteriorMaterial);
	const bool bAddedSleeve = AddStoreStockInstance(
			PropMesh(TEXT("SM_LabelSleeve")),
			LabelMaterial,
			FTransform(
				FRotator(0.0f, YawDegrees, 0.0f),
				BottleBase + FVector(0.0f, 0.0f, BandBottomZ),
				FVector(Radius, Radius, BandHeight)),
			false);
	ensureMsgf(
		bAddedSleeve,
		TEXT("SM_LabelSleeve is release-required; a flat panel cannot replace wrap film."));
}

void AIGPrologueWorldScene::FinalizeStoreStockBatches()
{
	// Register once per batch. Registering before every AddInstance rebuilds
	// render state repeatedly during BeginPlay and produces an avoidable hitch.
	for (const TPair<FString, TObjectPtr<UInstancedStaticMeshComponent>>& Pair :
		StoreStockBatches)
	{
		UInstancedStaticMeshComponent* Batch = Pair.Value.Get();
		if (!Batch || Batch->IsRegistered())
		{
			continue;
		}
		Batch->RegisterComponent();
		GeometryComponents.Add(Batch);
	}
}

bool AIGPrologueWorldScene::ValidateStoreStockBatches(
	int32& OutBatchCount,
	int32& OutInstanceCount) const
{
	OutBatchCount = StoreStockBatches.Num();
	OutInstanceCount = 0;
	bool bConfigurationValid = true;

	for (const TPair<FString, TObjectPtr<UInstancedStaticMeshComponent>>& Pair :
		StoreStockBatches)
	{
		const UInstancedStaticMeshComponent* Batch = Pair.Value.Get();
		if (!Batch)
		{
			bConfigurationValid = false;
			continue;
		}

		OutInstanceCount += Batch->GetInstanceCount();
		int32 CullStart = 0;
		int32 CullEnd = 0;
		Batch->GetCullDistances(CullStart, CullEnd);
		bConfigurationValid = bConfigurationValid
			&& Batch->IsRegistered()
			&& Batch->GetMobility() == EComponentMobility::Static
			&& Batch->GetCollisionEnabled() == ECollisionEnabled::NoCollision
			&& CullStart == IGPrologueWorld::StoreStockCullStartCentimeters
			&& CullEnd == IGPrologueWorld::StoreStockCullEndCentimeters;
	}

	return bConfigurationValid
		&& OutInstanceCount == IGPrologueWorld::ExpectedStoreStockInstances
		&& OutBatchCount > 0
		&& OutBatchCount <= IGPrologueWorld::MaximumStoreStockBatches;
}

UStaticMesh* AIGPrologueWorldScene::FindPhotoPropMesh(const TCHAR* AssetId) const
{
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

	// Required photo props must be available while the world is constructed,
	// including a first run with an empty Asset Registry discovery cache. The
	// import pipeline owns this stable package convention, so load the authored
	// primary object directly before falling back to a registry search for
	// future import variants.
	const FString PreferredObjectPath = FString::Printf(
		TEXT("/Game/Photo/Props/%s/%s_1k/StaticMeshes/%s.%s"),
		AssetId,
		AssetId,
		*PreferredName.ToString(),
		*PreferredName.ToString());
	if (UStaticMesh* DirectMesh = LoadObject<UStaticMesh>(
		nullptr,
		*PreferredObjectPath,
		nullptr,
		LOAD_NoWarn))
	{
		return DirectMesh;
	}

	const FAssetRegistryModule& RegistryModule =
		FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
	TArray<FAssetData> Assets;
	RegistryModule.Get().GetAssetsByPath(
		*FString::Printf(TEXT("/Game/Photo/Props/%s"), AssetId), Assets, true);

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
	return PlacePhotoPropInternal(
		AssetId, FloorCenter, TargetSize, YawDegrees, bEnableCollision, true);
}

UStaticMeshComponent* AIGPrologueWorldScene::PlacePhotoPropExactSize(
	const TCHAR* AssetId,
	const FVector& FloorCenter,
	const FVector& TargetSize,
	const float YawDegrees,
	const bool bEnableCollision)
{
	return PlacePhotoPropInternal(
		AssetId, FloorCenter, TargetSize, YawDegrees, bEnableCollision, false);
}

UStaticMeshComponent* AIGPrologueWorldScene::PlacePhotoPropInternal(
	const TCHAR* AssetId,
	const FVector& FloorCenter,
	const FVector& TargetSize,
	const float YawDegrees,
	const bool bEnableCollision,
	const bool bPreserveAspectRatio)
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

	FVector Scale = TargetSize / MeshSize;
	if (bPreserveAspectRatio)
	{
		const float UniformScale = Scale.GetMin();
		Scale = FVector(UniformScale);
	}

	UStaticMeshComponent* Prop = NewObject<UStaticMeshComponent>(
		this,
		*FString::Printf(TEXT("PhotoProp_%d"), BlockCounter++));
	Prop->SetupAttachment(ActiveParent ? ActiveParent.Get() : SceneRoot.Get());
	Prop->SetStaticMesh(Mesh);
	// Imported photo props already carry the project's audited fallback LODs.
	// Force that path instead of asking every third-party material (including
	// translucent glass slots) for a Nanite shader permutation it cannot use.
	// Without this, a cooked build may replace the scan with the grey default
	// material even though it looked correct after an editor-side recompile.
	Prop->bDisallowNanite = true;
	Prop->SetMobility(EComponentMobility::Static);
	Prop->SetGenerateOverlapEvents(false);
	Prop->SetCanEverAffectNavigation(false);
	Prop->SetCollisionProfileName(
		bEnableCollision
			? UCollisionProfile::BlockAll_ProfileName
			: UCollisionProfile::NoCollision_ProfileName);

	const FRotator Rotation(0.0f, YawDegrees, 0.0f);
	const FVector RotatedOriginOffset =
		Rotation.RotateVector(FVector(
			Bounds.Origin.X * Scale.X,
			Bounds.Origin.Y * Scale.Y,
			0.0f));
	const FVector Location(
		FloorCenter.X - RotatedOriginOffset.X,
		FloorCenter.Y - RotatedOriginOffset.Y,
		FloorCenter.Z - (Bounds.Origin.Z - Bounds.BoxExtent.Z) * Scale.Z);
	Prop->SetRelativeLocation(Location);
	Prop->SetRelativeRotation(Rotation);
	Prop->SetRelativeScale3D(Scale);
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
	if (UWorld* World = GetWorld())
	{
		if (UIGMissingFloorAudioSubsystem* AudioDirector =
			World->GetSubsystem<UIGMissingFloorAudioSubsystem>())
		{
			AudioDirector->RegisterComponent(Bed, EIGAudioBus::World);
		}
	}
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
	CardboardMaterial = TexMat(
		TEXT("M_MovingBoxCardboardUV"),
		CardboardMaterial);

	// Unit 403 and its corridor live on the 4th floor, three slabs up.
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
	BuildFifthFloorAnnex();
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
	const bool bMissingFloorRuntime =
		GetWorld()->URL.HasOption(TEXT("IGMissingFloor"))
		|| GetWorld()->URL.HasOption(TEXT("IGListenerGreybox"))
		|| FParse::Param(FCommandLine::Get(), TEXT("IGMissingFloor"))
		|| FParse::Param(FCommandLine::Get(), TEXT("IGListenerGreybox"));
	// 두 스토리가 같은 빌라를 사용한다. 없는 층에서 기존 REBIRTH 디렉터까지
	// 실행하면 문, HUD, 자동 저장을 서로 갱신하므로 현재 게임의 디렉터만 둔다.
	if (!bMissingFloorRuntime)
	{
		SpawnDirectors();
	}
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
			|| FParse::Param(
				FCommandLine::Get(),
				TEXT("IGCaptureCH03LensDroplet"))
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
		|| World->URL.HasOption(TEXT("IGMissingFloor"))
		|| FParse::Param(FCommandLine::Get(), TEXT("IGMissingFloor"))
		|| FParse::Param(FCommandLine::Get(), TEXT("IGListenerGreybox"))
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
	UMaterialInterface* WallPatina = TexMat(TEXT("M_ApartmentWallPatina"), nullptr);
	UMaterialInterface* Furniture = TexMat(TEXT("M_WoodFurnitureUV"), WoodMaterial);
	UMaterialInterface* Bedding = TexMat(TEXT("M_BeddingUV"), BeddingMaterial);
	UMaterialInterface* Metal = TexMat(TEXT("M_MetalUV"), MetalFrameMaterial);

	// Shell: interior 380 x 430 cm (about 5 pyeong), 230 cm ceiling.
	IGPrologueWorld::TagFootstepSurface(
		CreateBlock(FVector(0, 0, -10), FVector(440, 490, 20), Jangpan),
		IGPrologueWorld::FootstepVinylTag);
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

	// Localised wear is layered, never baked across every wall. Two masked
	// planes are enough to establish humidity at the cold exterior corner and
	// behind the desk while leaving the living surfaces recognisably cared for.
	// Small overlays cull outside the apartment and never enter collision or
	// the Lumen distance field.
	if (PlaneMesh && WallPatina)
	{
		if (UStaticMeshComponent* SouthPatina = CreateBlock(
			FVector(-82.0f, -214.4f, 54.0f),
			FVector(148.0f, 104.0f, 1.0f),
			WallPatina,
			false,
			PlaneMesh,
			FRotator(0.0f, 0.0f, 90.0f)))
		{
			SouthPatina->SetCullDistance(950.0f);
			SouthPatina->SetAffectDistanceFieldLighting(false);
		}
		if (UStaticMeshComponent* WestPatina = CreateBlock(
			FVector(-189.4f, 58.0f, 48.0f),
			FVector(96.0f, 126.0f, 1.0f),
			WallPatina,
			false,
			PlaneMesh,
			FRotator(90.0f, 0.0f, 0.0f)))
		{
			WestPatina->SetCullDistance(950.0f);
			WestPatina->SetAffectDistanceFieldLighting(false);
		}
	}

	// §11 V2: 403호는 프롤로그(깨끗) → 밤4(천장 모서리 균열 진행)로 3단계
	// 노화한다. Every plane is built now and hidden; SetUnit403AgeStage reveals
	// them. Building them later would make the room pop the first time the story
	// state changed, and the whole point is that the damage was always coming.
	//
	// The cracks start in the two ceiling corners over the wall he is behind. A
	// player who never looks up never learns it, and one who does gets the only
	// warning the apartment ever gives: this is spreading toward you.
	Unit403AgeStageOne.Reset();
	Unit403AgeStageTwo.Reset();
	if (PlaneMesh)
	{
		UMaterialInterface* Cracks =
			TexMat(TEXT("M_MissingFloorCavityScratches"), nullptr);
		UMaterialInterface* DampLift =
			TexMat(TEXT("M_DecalDampWallpaper"), nullptr);
		const auto AddAging = [this](
			TArray<TObjectPtr<UStaticMeshComponent>>& Stage,
			const FVector& Center,
			const FVector& Size,
			UMaterialInterface* Material,
			const FRotator& Rotation)
		{
			if (!Material)
			{
				return;
			}
			if (UStaticMeshComponent* Plane = CreateBlock(
				Center,
				Size,
				Material,
				false,
				PlaneMesh,
				Rotation))
			{
				// V2 contract: 0.15 cm off the surface, no shadow, no collision,
				// no distance field. The trace only wets what is already there.
				Plane->SetCastShadow(false);
				Plane->SetCanEverAffectNavigation(false);
				Plane->SetAffectDistanceFieldLighting(false);
				Plane->SetCullDistance(950.0f);
				Plane->SetHiddenInGame(true);
				Stage.Add(Plane);
			}
		};

		// Stage one: hairline cracks in the north-east ceiling corner, and the
		// first lift of wallpaper where the damp behind it has started to work.
		AddAging(
			Unit403AgeStageOne,
			FVector(150.0f, 200.0f, 229.85f),
			FVector(72.0f, 78.0f, 1.0f),
			Cracks,
			FRotator::ZeroRotator);
		AddAging(
			Unit403AgeStageOne,
			FVector(189.85f, 196.0f, 196.0f),
			FVector(58.0f, 46.0f, 1.0f),
			DampLift,
			FRotator(90.0f, 0.0f, 0.0f));

		// Stage two: it has crossed the ceiling and come down the corner. 곰팡이
		// 모서리 is the same damp overlay taken further down the wall, because
		// mould follows the water and the water has had four more nights.
		AddAging(
			Unit403AgeStageTwo,
			FVector(96.0f, 200.0f, 229.85f),
			FVector(96.0f, 84.0f, 1.0f),
			Cracks,
			FRotator(0.0f, 12.0f, 0.0f));
		AddAging(
			Unit403AgeStageTwo,
			FVector(189.85f, 214.0f, 122.0f),
			FVector(128.0f, 62.0f, 1.0f),
			DampLift,
			FRotator(90.0f, 0.0f, 0.0f));
		AddAging(
			Unit403AgeStageTwo,
			FVector(178.0f, 213.85f, 34.0f),
			FVector(74.0f, 58.0f, 1.0f),
			DampLift,
			FRotator(0.0f, 0.0f, 90.0f));
	}
	ApplyUnit403AgeStage();

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

	// Bedside table with an articulated lamp, desk with chair, wardrobe. Scanned
	// props keep their aspect ratio, so "fit in 58x58x60" does not guarantee a
	// 60 cm result. Measure the registered component and use its real upper
	// bound for every object that rests on it.
	UStaticMeshComponent* BedsideTable = PlacePhotoProp(
		TEXT("side_table_01"),
		FVector(-160, -35, 0),
		FVector(58, 58, 60),
		0.0f);
	if (!BedsideTable)
	{
		BedsideTable = CreateBlock(
			FVector(-160, -35, IGPrologueWorld::BedsideTableTopZ * 0.5f),
			FVector(55, 55, IGPrologueWorld::BedsideTableTopZ),
			Furniture);
	}
	float BedsideSurfaceLocalZ = IGPrologueWorld::BedsideTableTopZ;
	if (BedsideTable)
	{
		const FBoxSphereBounds TableBounds = BedsideTable->CalcBounds(
			BedsideTable->GetComponentTransform());
		const float BedsideSurfaceWorldZ =
			TableBounds.Origin.Z + TableBounds.BoxExtent.Z;
		const float UpperFloorWorldZ = UpperFloorRoot
			? UpperFloorRoot->GetComponentLocation().Z
			: IGPrologueWorld::FourthFloorZ;
		BedsideSurfaceLocalZ = BedsideSurfaceWorldZ - UpperFloorWorldZ;
		AlarmWorldLocation = FVector(
			IGPrologueWorld::AlarmHorizontalLocation.X,
			IGPrologueWorld::AlarmHorizontalLocation.Y,
			BedsideSurfaceWorldZ - IGPrologueWorld::AlarmContactBottomLocalZ
				- IGPrologueWorld::PropContactEmbedZ);
	}
	PlacePhotoProp(
		TEXT("desk_lamp_arm_01"),
		FVector(-172, -52, BedsideSurfaceLocalZ),
		FVector(30, 30, 48),
		35.0f,
		false);
	// Furniture is dimensioned around a seated adult: 74 cm work surface and a
	// roughly 43 cm chair seat. The source desk is two metres wide, so the usual
	// aspect-preserving fit would shrink its height to about 41 cm.
	UStaticMeshComponent* Desk = PlacePhotoPropExactSize(
		TEXT("metal_office_desk"),
		FVector(-100, -179, 0),
		FVector(145, 68, IGPrologueWorld::DeskWorkSurfaceHeight),
		0.0f);
	if (!Desk)
	{
		Desk = CreateBlock(
			FVector(-100, -179, 72.5f), FVector(145, 68, 3), Furniture);
		for (const float LegX : {-164.0f, -36.0f})
		{
			for (const float LegY : {-207.0f, -151.0f})
			{
				CreateBlock(FVector(LegX, LegY, 36), FVector(4, 4, 72), Metal);
			}
		}
	}
	float DeskSurfaceLocalZ = IGPrologueWorld::DeskWorkSurfaceHeight;
	if (Desk)
	{
		const FBoxSphereBounds DeskBounds = Desk->CalcBounds(Desk->GetComponentTransform());
		DeskSurfaceWorldZ = DeskBounds.Origin.Z + DeskBounds.BoxExtent.Z;
		const float UpperFloorWorldZ = UpperFloorRoot
			? UpperFloorRoot->GetComponentLocation().Z
			: IGPrologueWorld::FourthFloorZ;
		DeskSurfaceLocalZ = DeskSurfaceWorldZ - UpperFloorWorldZ;
		WalletWorldLocation = FVector(
			IGPrologueWorld::WalletHorizontalLocation.X,
			IGPrologueWorld::WalletHorizontalLocation.Y,
			DeskSurfaceWorldZ + 1.4f);
	}
	if (!PlacePhotoPropExactSize(
			TEXT("painted_wooden_chair_01"),
			FVector(-100, -118, 0),
			FVector(48, 50, 88),
			180.0f))
	{
		CreateBlock(FVector(-100, -118, 44), FVector(46, 46, 4), Furniture);
		CreateBlock(FVector(-100, -139, 66), FVector(46, 4, 48), Furniture);
		for (const float LegX : {-120.0f, -80.0f})
		{
			for (const float LegY : {-137.0f, -99.0f})
			{
				CreateBlock(FVector(LegX, LegY, 21), FVector(3, 3, 42), Furniture);
			}
		}
	}
	if (!PlacePhotoProp(TEXT("modern_wooden_cabinet"), FVector(-170, -70, 0), FVector(40, 84, 186), 90.0f))
	{
		CreateBlock(FVector(-172, -70, 90), FVector(35, 80, 180), Furniture);
	}

	// Eye-level dressing gives the room a personal history without blocking a
	// route or becoming false evidence. Each tiny mesh has a short draw range;
	// at corridor distance it is sub-pixel and should not cost a draw call.
	auto AddApartmentDressing = [this](
		const FVector& Center,
		const FVector& Size,
		UMaterialInterface* Material,
		UStaticMesh* Mesh,
		const FRotator& Rotation)
	{
		UStaticMeshComponent* Component = CreateBlock(
			Center, Size, Material, false, Mesh, Rotation);
		if (Component)
		{
			Component->SetCullDistance(850.0f);
			Component->SetAffectDistanceFieldLighting(false);
		}
		return Component;
	};
	// Pencil cup and three uneven pencils on the back corner of the desk.
	AddApartmentDressing(
		FVector(-152.0f, -187.0f, DeskSurfaceLocalZ + 5.9f), FVector(8.0f, 8.0f, 12.0f),
		PlasticDarkMaterial, CylinderMesh, FRotator::ZeroRotator);
	AddApartmentDressing(
		FVector(-154.0f, -187.0f, DeskSurfaceLocalZ + 17.0f), FVector(1.0f, 1.0f, 18.0f),
		SnackRedMaterial, CylinderMesh, FRotator(2.0f, 0.0f, -4.0f));
	AddApartmentDressing(
		FVector(-151.0f, -187.0f, DeskSurfaceLocalZ + 16.0f), FVector(1.0f, 1.0f, 16.0f),
		SnackYellowMaterial, CylinderMesh, FRotator(-3.0f, 0.0f, 3.0f));
	AddApartmentDressing(
		FVector(-148.5f, -187.0f, DeskSurfaceLocalZ + 15.0f), FVector(1.0f, 1.0f, 14.0f),
		SnackBlueMaterial, CylinderMesh, FRotator(1.0f, 0.0f, 5.0f));
	// Two used notebooks break the otherwise perfect horizontal desk surface.
	AddApartmentDressing(
		FVector(-75.0f, -183.0f, DeskSurfaceLocalZ + 1.0f), FVector(25.0f, 18.0f, 2.2f),
		Bedding, nullptr, FRotator(0.0f, -4.0f, 0.0f));
	AddApartmentDressing(
		FVector(-75.0f, -183.0f, DeskSurfaceLocalZ + 3.05f), FVector(22.0f, 16.0f, 2.0f),
		SignWhiteMaterial, nullptr, FRotator(0.0f, 3.0f, 0.0f));
	// The practical has a visible emitting face and a routed power lead, so the
	// warm pool reads as light from a real object rather than a floating point.
	AddApartmentDressing(
		FVector(-164.0f, -45.0f, 96.0f), FVector(9.0f, 9.0f, 1.0f),
		LightPanelMaterial, CylinderMesh, FRotator::ZeroRotator);
	AddApartmentDressing(
		FVector(-175.0f, -55.0f, 61.0f), FVector(1.2f, 1.2f, 22.0f),
		PlasticDarkMaterial, CylinderMesh, FRotator(0.0f, 18.0f, 18.0f));

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

	// Lived-in unit 403: wall AC unit, outlets, a July calendar, range hood.
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
	UPointLightComponent* BedsideLamp = CreateLight(
		FVector(-164, -45, 101), 255.0f, 340.0f,
		FLinearColor(1.0f, 0.53f, 0.25f), true, 25.0f);
	BedsideLamp->SetVolumetricScatteringIntensity(0.28f);
	CreateLight(
		FVector(-100, 190, 160), 132.0f, 460.0f,
		FLinearColor(0.42f, 0.58f, 0.90f), true, 32.0f);
	// A weak, shadowless cool bounce approximates the window contribution that
	// would otherwise disappear behind the wardrobe at this small scale. It is
	// intentionally too dim to flatten the lamp shadow or reveal the whole room.
	UPointLightComponent* WindowBounce = CreateLight(
		FVector(-55, -98, 132), 22.0f, 270.0f,
		FLinearColor(0.34f, 0.46f, 0.68f), false, 46.0f);
	WindowBounce->SetSpecularScale(0.12f);
	WindowBounce->SetVolumetricScatteringIntensity(0.0f);
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
	CreatePrintedBlock(
		FVector(62, -212.5f, 145), FVector(17, 5, 23),
		FridgeInteriorMaterial,
		TexMat(TEXT("M_Intercom"), SignWhiteMaterial),
		FVector(0, 1, 0));
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
	// On the 8 cm shoe step, not inside it. At Z 3 both slippers spawned
	// three centimetres into the step's collision, and a simulated body that
	// starts inside static geometry does not settle onto it -- Chaos resolves
	// the penetration by shoving it out, in the first second of the level.
	CreatePhysicsProp(
		CubeMesh, PlasticDarkMaterial,
		FVector(0.09f, 0.26f, 0.03f), FVector(120, -190, 9.5f), FRotator(0, 15, 0), 0.2f);
	CreatePhysicsProp(
		CubeMesh, PlasticDarkMaterial,
		FVector(0.09f, 0.26f, 0.03f), FVector(148, -192, 9.5f), FRotator(0, -8, 0), 0.2f);

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
	// 철제 계단은 복도 화강석 타일과 다른 물건이다. §11 규칙 2가 발밑을
	// 선택으로 만드는데, 지금까지 이 계단은 소리만 금속이고 그림은 복도
	// 바닥과 같았다 — 들리는 거리가 다른 두 표면을 눈으로 구분할 수 없으면
	// 「어느 바닥을 고르느냐」는 선택이 아니라 우연이 된다.
	UMaterialInterface* StairSteel =
		TexMat(TEXT("M_MissingFloorSteelStair"), CorridorFloor);
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
	IGPrologueWorld::TagFootstepSurface(
		CreateBlock(FVector(190, -305, -10), FVector(1040, 160, 20), CorridorFloor),
		IGPrologueWorld::FootstepConcreteTag);
	CreateBlock(FVector(190, -305, 250), FVector(1040, 160, 20), CorridorCeil);

	// §11 V2 끌린 자국 (복도 러너). A year of a hand cart and a rolled tarp being
	// dragged from the stair core to 403's door has worn a lane down the middle
	// of the hallway. It is the oldest mark in the building and the quietest
	// clue in it: the route the covering-up took, worn in before she moved in.
	if (PlaneMesh)
	{
		UMaterialInterface* Runner =
			TexMat(TEXT("M_MissingFloorDragTrails"), nullptr);
		UMaterialInterface* Rust = TexMat(TEXT("M_DecalRustFasteners"), nullptr);
		const auto AddCorridorResidue = [this](
			const FVector& Center,
			const FVector& Size,
			UMaterialInterface* Material,
			const FRotator& Rotation)
		{
			if (!Material)
			{
				return;
			}
			if (UStaticMeshComponent* Plane = CreateBlock(
				Center,
				Size,
				Material,
				false,
				PlaneMesh,
				Rotation))
			{
				// V2 contract: 0.15 cm clear of the surface, no shadow, no
				// collision, out of the distance field.
				Plane->SetCastShadow(false);
				Plane->SetCanEverAffectNavigation(false);
				Plane->SetAffectDistanceFieldLighting(false);
				Plane->SetCullDistance(1600.0f);
			}
		};
		// Three overlapping segments rather than one long plane: the lane wanders
		// where the cart was steered, and a single straight stripe down a hallway
		// reads as a painted line.
		AddCorridorResidue(
			FVector(-160.0f, -300.0f, 0.15f),
			FVector(320.0f, 96.0f, 1.0f),
			Runner,
			FRotator(0.0f, 3.0f, 0.0f));
		AddCorridorResidue(
			FVector(120.0f, -308.0f, 0.15f),
			FVector(300.0f, 88.0f, 1.0f),
			Runner,
			FRotator(0.0f, -4.0f, 0.0f));
		AddCorridorResidue(
			FVector(390.0f, -302.0f, 0.15f),
			FVector(280.0f, 82.0f, 1.0f),
			Runner,
			FRotator(0.0f, 2.0f, 0.0f));
		// §11 V2 계량기함 녹. The distribution board's steel face has been
		// weeping down its own door since long before any of this.
		AddCorridorResidue(
			FVector(-90.0f, -228.45f, 148.0f),
			FVector(30.0f, 44.0f, 1.0f),
			Rust,
			FRotator(90.0f, 0.0f, 0.0f));
	}

	// South wall with hopper windows onto the alley. The openings are cut out
	// of the masonry rather than drawn on it: a 20 cm slab with a glass plane
	// and an aluminium frame buried inside it left the frame standing half a
	// centimetre proud of solid concrete, which is the "blue rectangle stuck
	// on the wall" this dressing exists to avoid. Piers, sill course and head
	// course are separate blocks, so the reveal a player leans into is real
	// depth and the pane sits in the outer half of it like a real sash.
	{
		const float WindowXs[] = {-120.0f, 60.0f, 300.0f, 520.0f};
		constexpr float OpeningHalfWidth = 44.0f;
		constexpr float OpeningBottomZ = 114.0f;
		constexpr float OpeningTopZ = 186.0f;
		constexpr float WallWestX = -330.0f;
		constexpr float WallEastX = 710.0f;

		// Piers: masonry between the openings, plus the two end returns.
		const int32 WindowCount = static_cast<int32>(UE_ARRAY_COUNT(WindowXs));
		for (int32 PierIndex = 0; PierIndex < WindowCount + 1; ++PierIndex)
		{
			const float PierStartX = (PierIndex == 0)
				? WallWestX
				: WindowXs[PierIndex - 1] + OpeningHalfWidth;
			const float PierEndX = (PierIndex == WindowCount)
				? WallEastX
				: WindowXs[PierIndex] - OpeningHalfWidth;
			CreateBlock(
				FVector((PierStartX + PierEndX) * 0.5f, -385, 120),
				FVector(PierEndX - PierStartX, 20, 240),
				CorridorWallX);
		}

		for (const float WindowX : WindowXs)
		{
			// Sill course under the opening and head course over it.
			CreateBlock(
				FVector(WindowX, -385, OpeningBottomZ * 0.5f),
				FVector(OpeningHalfWidth * 2.0f, 20, OpeningBottomZ),
				CorridorWallX);
			CreateBlock(
				FVector(WindowX, -385, (OpeningTopZ + 240.0f) * 0.5f),
				FVector(OpeningHalfWidth * 2.0f, 20, 240.0f - OpeningTopZ),
				CorridorWallX);
			// The sash sits in the outer half of the reveal, lapping 1 cm into
			// the masonry on every edge so it is held by the opening. It keeps
			// collision: these windows never open, and the envelope has to
			// stay sealed now that the opening is a real hole in the wall.
			CreateBlock(
				FVector(WindowX, -390, 150),
				FVector(90, 3, 74),
				WindowDarkMaterial);
			// Aluminium trim ring on the corridor face of the reveal.
			CreateBlock(FVector(WindowX, -377.5f, 184), FVector(88, 6, 5), PlasticDarkMaterial, false);
			CreateBlock(FVector(WindowX, -377.5f, 116), FVector(88, 6, 5), PlasticDarkMaterial, false);
			CreateBlock(FVector(WindowX - 41, -377.5f, 150), FVector(5, 6, 66), PlasticDarkMaterial, false);
			CreateBlock(FVector(WindowX + 41, -377.5f, 150), FVector(5, 6, 66), PlasticDarkMaterial, false);
			CreateBlock(FVector(WindowX, -377.5f, 150), FVector(78, 5, 3.5f), PlasticDarkMaterial, false);
			CreateBlock(FVector(WindowX, -379, 111), FVector(94, 10, 5), Skirting, false);
		}
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
	// 문짝은 브러시드 스테인리스가 아니라 무광 도장 강판이다. 밴드·인레이·
	// 레버·도어록·도어스코프는 이미 실제 기하이므로 표면만 바꾼다.
	UMaterialInterface* UnitDoorLeaf =
		TexMat(TEXT("M_UnitDoorPaintedSteel"), SteelDoor);
	// IntercomSide is +1 when the intercom hangs east of the leaf. 401 needs
	// that: 56 cm west of its door is the ten-centimetre return of 403's west
	// wall, and the plate ended up inside the masonry.
	auto DressUnitDoor = [this, UnitDoorLeaf, Stainless, Metal](
		const float DoorX, const float FaceY, const float IntercomSide)
	{
		CreateBlock(FVector(DoorX, FaceY, 100), FVector(84, 5, 200), UnitDoorLeaf);
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
		CreatePrintedBlock(
			FVector(DoorX + 32, PlateY - 1.4f, 122), FVector(9, 3.2f, 24),
			PlasticDarkMaterial,
			TexMat(TEXT("M_DoorLock"), PlasticDarkMaterial),
			FVector(0, -1, 0));
		CreateBlock(
			FVector(DoorX, PlateY - 0.4f, 155), FVector(3, 1.2f, 3),
			Metal, false, CylinderMesh, FRotator(90, 0, 0));
		// Doorbell button and the video intercom plate beside the frame. Both
		// are screwed to the landing face of the wall at Y -235, not to the
		// leaf: at FaceY + 1 the whole intercom sat inside the masonry and the
		// button showed a couple of millimetres of itself.
		CreateBlock(
			FVector(DoorX + IntercomSide * 56.0f, FaceY - 1.75f, 138),
			FVector(7, 2.5f, 11),
			TexMat(TEXT("M_Intercom"), SignWhiteMaterial), false);
		CreateBlock(
			FVector(DoorX + IntercomSide * 56.0f, FaceY - 1.25f, 122),
			FVector(3, 1.5f, 3),
			SnackRedMaterial, false, CylinderMesh, FRotator(90, 0, 0));
	};

	// West to east the landing reads 401, 402, 403. Keeping the ordinary
	// sequence matters: 403 is Yudam's home, while 404 exists only as the tiny
	// optional joke beside the last frame and never becomes a horror number.
	const float NeighborDoorXs[] = {-30.0f, -150.0f};
	const TCHAR* NeighborPlates[] = {TEXT("M_Plate402"), TEXT("M_Plate401")};
	for (int32 NeighborIndex = 0; NeighborIndex < 2; ++NeighborIndex)
	{
		const float DoorX = NeighborDoorXs[NeighborIndex];
		const int32 FirstDoorComponent = GeometryComponents.Num();
		DressUnitDoor(DoorX, -234.5f, NeighborIndex == 0 ? -1.0f : 1.0f);
		// Powder-coated casing stays readable at this thin aspect ratio; the
		// former stucco UV stretched into conspicuous horizontal stripes.
		CreateBlock(FVector(DoorX - 46, -233, 102), FVector(8, 7, 208), DoorTrim, false);
		CreateBlock(FVector(DoorX + 46, -233, 102), FVector(8, 7, 208), DoorTrim, false);
		CreateBlock(FVector(DoorX, -233, 204), FVector(100, 7, 8), DoorTrim, false);
		// Unit number, on the landing face of the wall above the head trim.
		// At Y -231.5 the plate was two centimetres deep inside the wall.
		CreateBlock(
			FVector(DoorX, -236, 214), FVector(16, 2, 8),
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
	// Our 403 door casing and plate around the real swing door; the leaf
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
	// Both read from the landing, so both have to clear the wall face at
	// Y -235; at Y -233.5 the plate and the intercom were inside the wall.
	CreateBlock(
		FVector(144, -236, 214), FVector(16, 2, 8),
		TexMat(TEXT("M_Plate403"), FridgeInteriorMaterial), false);
	CreateBlock(
		FVector(88, -236.25f, 138), FVector(7, 2.5f, 11),
		TexMat(TEXT("M_Intercom"), SignWhiteMaterial), false);

	// One dry joke before the building starts lying: a real 76 mm memo sits on
	// the empty wall where the next unit would continue. It has no collision,
	// prompt, outline, subtitle or state change; close inspection is the whole
	// reward. The curled mesh and rough paper material keep it grounded in the
	// corridor light instead of reading as a flat UI sticker.
	if (UStaticMesh* StickyNoteMesh = PropMesh(TEXT("SM_StickyNote76mm")))
	{
		if (UStaticMeshComponent* NotFoundNote = CreateDecoOnComponent(
				ActiveParent.Get(),
				StickyNoteMesh,
				TexMat(TEXT("M_Note404NotFound"), SignWhiteMaterial),
				FVector(216.0f, -235.12f, 171.0f),
				FRotator(0.0f, 90.0f, 0.0f),
				FVector::OneVector))
		{
			NotFoundNote->ComponentTags.AddUnique(
				IGPrologueWorld::NotFoundEasterEggTag);
			NotFoundNote->SetCullDistance(520.0f);
			NotFoundNote->SetAffectDistanceFieldLighting(false);
		}
	}

	// Granite skirting, the way real landings finish the stucco to the tile.
	// It stands on the landing side of each wall: the north face is at
	// Y -235 and the south face at Y -375. Authored at -233.4 and -376.6 the
	// whole 3.5 cm course was inside the wall it was supposed to finish, with
	// a millimetre and a half showing.
	CreateBlock(FVector(-35, -236.75f, 6), FVector(580, 3.5f, 12), Skirting, false);
	CreateBlock(FVector(-35, -373.25f, 6), FVector(580, 3.5f, 12), Skirting, false);
	CreateBlock(FVector(302.5f, -236.75f, 6), FVector(165, 3.5f, 12), Skirting, false);
	CreateBlock(FVector(582.5f, -236.75f, 6), FVector(215, 3.5f, 12), Skirting, false);
	ChapterOneMaskComponents.Add(CreateBlock(
		FVector(430, -236.75f, 6), FVector(100, 3.5f, 12), Skirting, false));
	CreateBlock(FVector(455, -373.25f, 6), FVector(470, 3.5f, 12), Skirting, false);

	// Distribution board between the units, plus the fire cabinet. The board
	// is flush-mounted, so its case belongs inside the wall with the door
	// proud of the plaster. It sits at chest-to-head height: at Z 155 its case
	// occupied the same patch of wall as 402's intercom.
	// 케이스는 민무늬 강판이고 「분전반」은 문짝에만 인쇄된다. 케이스에
	// 직접 주면 6 cm 옆면에도 같은 글자가 눌려 찍힌다.
	CreateBlock(FVector(-90, -232.4f, 180), FVector(34, 6, 50), Metal, false);
	// 문짝과 손잡이는 케이스와 같은 높이여야 한다. 케이스만 Z 155에서 180으로
	// 올라가고 이 둘이 남아, 문이 상자 아래로 25 cm 흘러내려 있었다.
	CreateBlock(
		FVector(-90, -229.2f, 180), FVector(35, 1.2f, 51),
		TexMat(TEXT("M_MeterBox"), Metal), false);
	CreateBlock(FVector(-75, -228.6f, 180), FVector(3, 1.5f, 6), PlasticDarkMaterial, false);
	CreatePrintedBlock(
		FVector(236, -371, 140), FVector(26, 9, 34),
		SnackRedMaterial,
		TexMat(TEXT("M_FireBox"), SnackRedMaterial),
		FVector(0, 1, 0));
	// The extinguisher is the one corridor prop authored to fall (밤1 beat
	// 1-5). A physics body from birth, but kinematic until the scripted drop:
	// visually identical to the old static block and free at rest.
	CorridorExtinguisher = CreatePhysicsProp(
		CylinderMesh,
		SnackRedMaterial,
		FVector(0.15f, 0.15f, 0.48f),
		FVector(232, -364, 26),
		FRotator::ZeroRotator,
		6.0f);
	if (CorridorExtinguisher)
	{
		CorridorExtinguisher->SetSimulatePhysics(false);
		// Valve stub rides the body so the silhouette survives the fall.
		UStaticMeshComponent* Valve = NewObject<UStaticMeshComponent>(
			this, TEXT("CorridorExtinguisherValve"));
		Valve->SetupAttachment(CorridorExtinguisher);
		Valve->SetStaticMesh(CylinderMesh);
		Valve->SetMaterial(0, PlasticDarkMaterial);
		// Child scale compounds with the parent's (0.15, 0.15, 0.48).
		Valve->SetRelativeScale3D(FVector(0.33f, 0.33f, 0.17f));
		Valve->SetRelativeLocation(FVector(0.0f, 0.0f, 54.0f));
		Valve->SetMobility(EComponentMobility::Movable);
		Valve->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Valve->SetGenerateOverlapEvents(false);
		Valve->SetCanEverAffectNavigation(false);
		Valve->RegisterComponent();
		GeometryComponents.Add(Valve);
	}

	// Permanent first flight toward 5F. It turns north from the west landing,
	// so it never overlaps the parallel flight descending toward 3F. The
	// memory cut happens on the first tread, but the visible architecture is
	// always here and therefore cannot pop in when the story state changes.
	for (int32 UpperStepIndex = 0; UpperStepIndex < 4; ++UpperStepIndex)
	{
		const float StepY = -216.0f + UpperStepIndex * 22.0f;
		const float StepTop = 18.0f + UpperStepIndex * 18.0f;
		IGPrologueWorld::TagFootstepSurface(CreateBlock(
			FVector(-277.5f, StepY, StepTop * 0.5f),
			FVector(85, 22, StepTop),
			StairSteel), IGPrologueWorld::FootstepMetalStairTag);
		CreateBlock(
			FVector(-277.5f, StepY + 10.0f, StepTop + 0.8f),
			FVector(83, 2.5f, 1.6f),
			Skirting,
			false);
	}
	// 계단은 바닥에서 자란 한 덩어리라 마지막 단의 북쪽 면이 통째로 드러난다.
	// 체커플레이트는 XY로 읽으므로 그 세로 면에서는 무늬가 전혀 변하지 않는다
	// — 85 x 54 cm가 한 줄로 늘어난 민무늬였다. 실제 철제 계단에서 그 자리는
	// 디딤판이 아니라 도장 강판 마구리이므로 UV 재질로 덮는다.
	CreateBlock(
		FVector(-277.5f, -138.7f, 27.0f),
		FVector(85, 0.6f, 54),
		TexMat(TEXT("M_SteelDoorUV"), MetalFrameMaterial),
		false);
	// Threshold slab flush with the fourth tread. The former north closure and
	// hidden portal are gone: BuildFifthFloorAnnex continues this exact shaft
	// with fourteen physical treads to the roof.
	IGPrologueWorld::TagFootstepSurface(CreateBlock(
		FVector(-277.5f, -125.0f, 63.0f),
		FVector(85, 28, 18),
		StairSteel), IGPrologueWorld::FootstepMetalStairTag);
	CreateBlock(
		FVector(-332.5f, -165.0f, 120),
		FVector(15, 120, 240),
		ConcreteDarkMaterial);
	CreateBlock(
		FVector(-222.5f, -165.0f, 120),
		FVector(15, 120, 240),
		ConcreteDarkMaterial);
	CreateBlock(
		FVector(-277.5f, -165.0f, 250),
		FVector(125, 120, 20),
		CorridorCeil);

	// Down flight into the throat, sinking west into darkness. Korean walk-ups
	// have a black steel balustrade with a flat cap rail and thin square
	// balusters.
	//
	// The treads used to be authored from X -219 to -329, which is inside the
	// corridor: the floor slab reaches X -330, so the whole flight was buried
	// under it and only the upper half of the balustrade showed — a raked
	// handrail rising out of flat concrete with no stair beneath it. The 72 cm
	// to the half-landing was a bare drop off the floor's west edge.
	//
	// The throat west of X -330 is where the flight belongs, and four treads
	// at the authored 18 cm rise land exactly on the half-landing at local
	// Z -72. Each tread's underside meets the top of the one below, so the
	// flight is a solid stepped mass standing on the landing and butting the
	// corridor floor — nothing here is cantilevered and nothing is cut out of
	// the corridor, so the walk west and the turn north onto the 5F flight are
	// exactly as they were.
	for (int32 StepIndex = 0; StepIndex < 4; ++StepIndex)
	{
		const float StepX = -341.0f - StepIndex * 22.0f;
		const float StepTopZ = -18.0f * StepIndex;
		IGPrologueWorld::TagFootstepSurface(CreateBlock(
			FVector(StepX, -305, StepTopZ - 9.0f),
			FVector(22, 130, 18), StairSteel),
			IGPrologueWorld::FootstepMetalStairTag);
		// Stair nosing: a darker lip on every tread catches the hall light.
		CreateBlock(
			FVector(StepX - 10, -305, StepTopZ + 0.4f),
			FVector(3, 128, 1.6f), Skirting, false);
	}
	// Enclose the descending flight beyond the transition volume. Without the
	// far wall and side returns the sky dome filled the stair throat, making an
	// ordinary interior landing look like a blue portal from the corridor.
	CreateBlock(
		FVector(-392.5f, -385, 80), FVector(125, 20, 340),
		CorridorWallX);
	CreateBlock(
		FVector(-392.5f, -225, 80), FVector(125, 20, 340),
		CorridorWallX);
	CreateBlock(
		FVector(-455, -305, 80), FVector(20, 200, 340),
		CorridorWallY);
	CreateBlock(
		FVector(-392.5f, -305, 250), FVector(125, 160, 20),
		CorridorCeil);
	// The 3.5F half-landing at local Z -72, filling what used to be open shaft
	// void. The down flight now stands on it and its lowest tread's underside
	// meets it, so the landing is both what the stair arrives on and the strip
	// of floor west of it. In the legacy chapters the stair portal fires
	// before a player can reach it, so this slab is only ever walked during
	// 없는 층 nights — where it is the stage for the first sighting
	// (STORY_BIBLE_MISSING_FLOOR.md §8 밤1 1-4).
	IGPrologueWorld::TagFootstepSurface(CreateBlock(
		FVector(-392.5f, -305, -81), FVector(125, 160, 18),
		StairSteel), IGPrologueWorld::FootstepMetalStairTag);
	// Balustrade over that flight. Each baluster stands on its own tread and
	// reaches the cap rail: the old set used a 17 cm rise under an 18 cm
	// stair and started four centimetres above the treads, so it drifted out
	// of the flight as it descended and met the rail nowhere.
	for (const float RailY : {-243.0f, -367.0f})
	{
		CreateBlock(
			FVector(-383, RailY, 55.6f), FVector(132, 5, 5), PlasticDarkMaterial,
			false, nullptr, FRotator(39, 0, 0));
		CreateBlock(
			FVector(-383, RailY, 27.6f), FVector(130, 3, 3), PlasticDarkMaterial,
			false, nullptr, FRotator(39, 0, 0));
		// Four on the treads and one on the landing at the foot.
		for (int32 BalusterIndex = 0; BalusterIndex < 5; ++BalusterIndex)
		{
			const float BalusterX = -341.0f - BalusterIndex * 22.0f;
			const float BalusterFootZ = -18.0f * BalusterIndex;
			CreateBlock(
				FVector(BalusterX, RailY, BalusterFootZ + 45.0f),
				FVector(2.6f, 2.6f, 90), PlasticDarkMaterial, false);
		}
	}
	// A tired green exit lamp glows at the stair throat, screwed to the head
	// of the stair opening rather than hanging a centimetre clear of it.
	CreateBlock(FVector(-313, -305, 220), FVector(14, 8, 10),
		ScreenGlowMaterial, false);

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
			FVector(FixtureX, -305, 226), 1020.0f, 410.0f,
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

	// 403 is papered differently from the player's own flat. It was using the
	// same floral V2, which quietly said the impossible room is a copy of your
	// room -- and the whole beat is that it is somebody else's. Plain embossed
	// vinyl is the other common Korean villa paper, so it reads as a different
	// household at a glance without reading as a different building. Falls
	// back to WallMaterial until the art build has produced the texture.
	UMaterialInterface* RoomWallX =
		TexMat(TEXT("M_WallpaperEmboss_X"), WallMaterial);
	UMaterialInterface* RoomWallY =
		TexMat(TEXT("M_WallpaperEmboss_Y"), WallMaterial);
	UMaterialInterface* RoomFloor = TexMat(TEXT("M_Jangpan"), FloorMaterial);
	UMaterialInterface* RoomCeiling = TexMat(TEXT("M_StuccoCeil"), ConcreteMaterial);
	UMaterialInterface* Furniture = TexMat(TEXT("M_WoodFurnitureUV"), WoodMaterial);
	UMaterialInterface* Bedding = TexMat(TEXT("M_BeddingUV"), BeddingMaterial);
	UMaterialInterface* Hoodie = TexMat(TEXT("M_WetHoodieUV"), BeddingMaterial);
	UMaterialInterface* CorridorFloor =
		TexMat(TEXT("M_GraniteTile_XY"), ConcreteMaterial);
	UMaterialInterface* OfferingBowlMaterial =
		TexMat(TEXT("M_StainlessUV"), MetalFrameMaterial);
	UMaterialInterface* OfferingWaterMaterial =
		TexMat(TEXT("M_TankWaterReveal"), GlassMaterial);
	UMaterialInterface* LampShadeMaterial =
		TexMat(TEXT("M_PaperOld"), SignWhiteMaterial);
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

	// Bed, pillow and the same curled, fully clothed anatomy used by the tank
	// reveal. Engine spheres made the sleeper read as a featureless creature;
	// the authored mesh keeps a neck notch, shoulder line, elbows and covered
	// hands while showing no skin.
	AddOverlay(FVector(300, 110, 20), FVector(100, 200, 40), Furniture);
	AddOverlay(FVector(300, 110, 47), FVector(94, 194, 18), Bedding);
	AddOverlay(FVector(300, 182, 59), FVector(80, 47, 10), FridgeInteriorMaterial, false);
	const FVector SleeperOrigin(300.0f, 127.0f, 70.0f);
	const FRotator SleeperRotation(0.0f, 90.0f, 0.0f);
	AddOverlay(
		SleeperOrigin, FVector(100.0f), Hoodie, false,
		PropMesh(TEXT("SM_SubmergedHoodieCurl")), SleeperRotation);
	// The lower body remains under the blanket, but follows two bent legs and
	// knees instead of one mathematically smooth oval.
	AddOverlay(
		SleeperOrigin, FVector(100.0f), Bedding, false,
		PropMesh(TEXT("SM_SubmergedPantsCurl")), SleeperRotation);
	// Three stitches sit on the authored camera-side forearm. They are derived
	// from the same local points as the tank identity evidence, not floated by
	// eye above the duvet.
	const FVector StitchBase = SleeperOrigin
		+ SleeperRotation.RotateVector(FVector(8.0f, -27.0f, 15.2f));
	const FVector StitchStep =
		SleeperRotation.RotateVector(FVector(1.8f, 1.2f, 0.0f));
	for (int32 StitchIndex = 0; StitchIndex < 3; ++StitchIndex)
	{
		AddOverlay(
			StitchBase + StitchStep * StitchIndex,
			FVector(0.8f, 3.2f, 0.5f),
			PlasticDarkMaterial,
			false,
			nullptr,
			FRotator(0.0f, 90.0f, 18.0f));
	}
	AddOverlay(FVector(365, 28, 28), FVector(55, 55, 56), Furniture);

	// Black horn-rim glasses at real scale (about 14 cm across).
	for (const float LensX : {343.5f, 351.5f})
	{
		AddOverlay(FVector(LensX, 21, 57.4f), FVector(5.5f, 0.8f, 0.8f), DarkGloss, false);
		AddOverlay(FVector(LensX, 21, 61.2f), FVector(5.5f, 0.8f, 0.8f), DarkGloss, false);
		AddOverlay(FVector(LensX - 2.75f, 21, 59.3f), FVector(0.8f, 0.8f, 4.6f), DarkGloss, false);
		AddOverlay(FVector(LensX + 2.75f, 21, 59.3f), FVector(0.8f, 0.8f, 4.6f), DarkGloss, false);
	}
	AddOverlay(FVector(347.5f, 21, 59.3f), FVector(2.1f, 0.8f, 0.8f), DarkGloss, false);
	AddOverlay(
		FVector(338.3f, 24, 59), FVector(8.5f, 0.8f, 0.8f), DarkGloss, false,
		nullptr, FRotator(0, -18, 0));
	AddOverlay(
		FVector(356.7f, 24, 59), FVector(8.5f, 0.8f, 0.8f), DarkGloss, false,
		nullptr, FRotator(0, 18, 0));

	// The small nightstand already carries the alarm, memo and glasses. A wall
	// sconce above it keeps that real 55 cm surface physically usable and gives
	// the otherwise dead corridor one believable warm source.
	AddOverlay(FVector(380, 216, 157), FVector(14, 4, 22), DarkGloss, false);
	AddOverlay(FVector(380, 206, 157), FVector(4, 18, 4), DarkGloss, false);
	UStaticMeshComponent* LampShade = AddOverlay(
		FVector(380, 197, 143), FVector(24, 24, 20),
		LampShadeMaterial, false, ConeMesh);
	if (LampShade)
	{
		// The point light sits inside this solid prototype cone. Letting the
		// cone cast produced a large black triangular pool across the bed.
		LampShade->SetCastShadow(false);
	}
	// The bulb sits just below the wall shade. A small soft source keeps the
	// sleeper readable without flattening the far corner or lighting the hall.
	MirrorRoomLamp = CreateLight(
		FVector(380, 194, 132), 560.0f, 380.0f,
		FLinearColor(1.0f, 0.52f, 0.22f), false, 34.0f);
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
	for (int32 GrainIndex = 0; GrainIndex < 17; ++GrainIndex)
	{
		const float Alpha = static_cast<float>(GrainIndex) / 16.0f;
		const float GrainSize = 0.38f + (GrainIndex % 4) * 0.05f;
		for (const float Side : {-1.0f, 1.0f})
		{
			const float GrainX = -150.0f
				+ Side * (17.0f + (1.0f - Alpha) * 25.0f);
			const float GrainY = -248.0f - Alpha * 4.0f
				+ FMath::Sin(GrainIndex * 1.71f) * 1.05f;
			if (UStaticMeshComponent* Grain = AddOverlay(
				FVector(GrainX, GrainY, 0.27f),
				FVector(GrainSize, GrainSize * 0.72f, 0.38f),
				LampShadeMaterial, false, SphereMesh))
			{
				Grain->SetCastShadow(false);
			}
		}
	}
	// Thin, asymmetric deposits keep the swept salt legible without turning the
	// threshold into an evenly spaced row of pebble-like props.
	struct FSaltDepositSpec
	{
		FVector2D Position;
		FVector Scale;
		float Yaw;
	};
	const FSaltDepositSpec SaltDeposits[] = {
		{FVector2D(-193.0f, -249.4f), FVector(8.2f, 1.55f, 0.42f), -11.0f},
		{FVector2D(-183.5f, -246.9f), FVector(3.6f, 1.15f, 0.32f),   7.0f},
		{FVector2D(-175.0f, -250.2f), FVector(9.2f, 1.85f, 0.48f),  -4.0f},
		{FVector2D(-166.5f, -247.6f), FVector(3.0f, 1.00f, 0.30f),  15.0f},
		{FVector2D(-133.0f, -249.0f), FVector(4.1f, 1.10f, 0.33f), -12.0f},
		{FVector2D(-124.0f, -246.5f), FVector(9.6f, 1.75f, 0.48f),   6.0f},
		{FVector2D(-113.0f, -250.7f), FVector(3.2f, 0.95f, 0.30f), -18.0f},
		{FVector2D(-103.5f, -247.8f), FVector(8.7f, 1.50f, 0.42f),   9.0f}};
	for (const FSaltDepositSpec& Deposit : SaltDeposits)
	{
		if (UStaticMeshComponent* Cluster = AddOverlay(
			FVector(Deposit.Position.X, Deposit.Position.Y, 0.28f),
			Deposit.Scale,
			LampShadeMaterial,
			false,
			SphereMesh,
			FRotator(0, Deposit.Yaw, 0)))
		{
			Cluster->SetCastShadow(false);
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
			FVector(SweptSalt[ScatterIndex].X, SweptSalt[ScatterIndex].Y, 0.20f),
			FVector(0.42f, 0.34f, 0.30f),
			LampShadeMaterial,
			false,
			SphereMesh))
		{
			Grain->SetCastShadow(false);
		}
	}

	// Only the dry circular trace of the removed rice bowl remains.
	if (UStaticMeshComponent* DryOuter = AddOverlay(
		FVector(-174, -280, 0.16f), FVector(24, 24, 0.24f),
		ConcreteMaterial, false, CylinderMesh))
	{
		DryOuter->SetCastShadow(false);
	}
	if (UStaticMeshComponent* DryInner = AddOverlay(
		FVector(-174, -280, 0.24f), FVector(23.1f, 23.1f, 0.18f),
		CorridorFloor, false, CylinderMesh))
	{
		DryInner->SetCastShadow(false);
	}
	// Foot traffic and wiping interrupt the mineral ring at three points. The
	// residue remains recognizable without looking like a puzzle UI outline.
	AddOverlay(FVector(-186.0f, -280.0f, 0.36f), FVector(4.0f, 5.0f, 0.12f), CorridorFloor, false);
	AddOverlay(FVector(-174.0f, -268.0f, 0.36f), FVector(5.0f, 4.0f, 0.12f), CorridorFloor, false);
	AddOverlay(FVector(-164.5f, -287.0f, 0.36f), FVector(4.0f, 5.0f, 0.12f), CorridorFloor, false);

	// The clear-water bowl sits outside the broken line. Its 24 cm steel body
	// matches the overturned bowl that reappears on the unfinished fifth floor.
	AddOverlay(
		FVector(-126, -280, 0.08f), FVector(100.0f),
		OfferingBowlMaterial, false,
		PropMesh(TEXT("SM_OfferingWaterBowl"), CylinderMesh));
	if (UStaticMeshComponent* WaterSurface = AddOverlay(
		FVector(-126, -280, 7.72f), FVector(20.6f, 20.6f, 0.16f),
		OfferingWaterMaterial, false, CylinderMesh))
	{
		WaterSurface->SetCastShadow(false);
	}
	// Two broken reflections are enough to communicate a liquid surface in the
	// dim corridor without turning the bowl into an emissive objective marker.
	AddOverlay(
		FVector(-129.0f, -276.5f, 7.86f), FVector(7.2f, 0.85f, 0.08f),
		FridgeInteriorMaterial, false, SphereMesh, FRotator(0, -18, 0));
	AddOverlay(
		FVector(-122.5f, -282.5f, 7.87f), FVector(4.2f, 0.62f, 0.06f),
		FridgeInteriorMaterial, false, SphereMesh, FRotator(0, 24, 0));

	// A restrained corridor spill keeps the evidence legible without turning
	// this quiet act of waiting into a supernatural spotlight.
	OfferingLight = CreateLight(
		FVector(-150, -312, 78), 175.0f, 220.0f,
		FLinearColor(1.0f, 0.78f, 0.58f), false, 30.0f);
	if (OfferingLight)
	{
		OfferingLight->SetVisibility(false);
		OfferingLight->SetSpecularScale(0.18f);
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
		Light->SetIntensity(bLive ? (bCorridor ? 1020.0f : 920.0f) : 0.0f);
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
		OfferingLight->SetIntensity(bVisible ? 145.0f : 0.0f);
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

void AIGPrologueWorldScene::BuildFifthFloorAnnex()
{
	// Release topology, all in one world: the existing 4F stair continues to
	// the roof slab, a 1.2 m maintenance lane passes the real tank, and the
	// second fire door enters the illegal annex. No portal or camera cut is
	// allowed to stand in for this walk.
	ActiveParent = nullptr;

	// 수평 슬래브에는 XY 매핑을 쓴다. _X는 (X, Z) 마스킹이라 Z가 일정한
	// 바닥에서 텍스처가 한 줄로 잘려 늘어난다 — 별관 바닥이 그 상태였다.
	UMaterialInterface* AnnexFloor = TexMat(TEXT("M_ConcreteDark_XY"), ConcreteDarkMaterial);
	UMaterialInterface* AnnexWallX = TexMat(TEXT("M_MissingFloorPlaster_X"), ConcreteMaterial);
	UMaterialInterface* AnnexWallY = TexMat(TEXT("M_MissingFloorPlaster_Y"), ConcreteMaterial);
	UMaterialInterface* AnnexCeiling = TexMat(TEXT("M_MissingFloorPlaster_XY"), ConcreteMaterial);
	// 노출된 경량 스터드. 여기에 M_MeterBox를 쓰고 있었는데 그 재질은
	// 분전반 문짝 도장면이라 「분전반」·「취급주의」 활자가 인쇄돼 있다.
	// 폭 4 cm 스터드 여섯 개에 그 글자가 잘려 실려서, 손전등이 스치면
	// 흰색·노란색 획이 세로로 흩어진 노이즈처럼 보였다.
	UMaterialInterface* Stud = TexMat(TEXT("M_MetalUV"), MetalFrameMaterial);
	UMaterialInterface* Board = TexMat(TEXT("M_ShelfSteelUV"), PlasticDarkMaterial);
	// 계단과 옥상 바닥은 서로 다른 발소리 표면인데 한 변수를 공유하고
	// 있었다. 옥상 방수층은 조용하고 철제 계단은 길게 울린다 — 같은
	// 콘크리트로 그리면 그 차이를 볼 방법이 없다.
	UMaterialInterface* RoofFloor = TexMat(TEXT("M_Concrete_XY"), ConcreteMaterial);
	UMaterialInterface* StairSteel =
		TexMat(TEXT("M_MissingFloorSteelStair"), RoofFloor);
	UMaterialInterface* RoofDeck =
		TexMat(TEXT("M_RooftopWaterproofing_XY"), RoofFloor);
	UMaterialInterface* RoofMetal = TexMat(TEXT("M_WaterTankMetalUV"), MetalFrameMaterial);
	UMaterialInterface* RailMetal = TexMat(TEXT("M_StainlessUV"), MetalFrameMaterial);

	MissingFloorUpperStairSteps.Reset();
	MissingFloorRooftopRouteFloors.Reset();
	MissingFloorAnnexLights.Reset();
	MissingFloorCavityWallPanel = nullptr;
	MissingFloorCavityWallResidue = nullptr;
	bMissingFloorCavityOpen = false;

	// Fourteen 17 cm risers continue directly from the authored 4F threshold.
	// Each box grows from the same 963 cm base, producing a solid, sweep-safe
	// stair rather than independent floating treads.
	constexpr float StairBaseZ = 963.0f;
	constexpr float RiserHeight = 17.0f;
	constexpr float TreadDepth = 22.0f;
	for (int32 StepIndex = 0;
		StepIndex < IGPrologueWorld::MissingFloorUpperStepCount;
		++StepIndex)
	{
		const float Height = RiserHeight * (StepIndex + 1);
		UStaticMeshComponent* Step = CreateBlock(
			FVector(
				-277.5f,
				-100.0f + TreadDepth * StepIndex,
				StairBaseZ + Height * 0.5f),
			FVector(85.0f, TreadDepth, Height),
			StairSteel);
		IGPrologueWorld::TagFootstepSurface(
			Step,
			IGPrologueWorld::FootstepMetalStairTag);
		MissingFloorUpperStairSteps.Add(Step);
	}
	// 4층 계단과 같은 이유로 마지막 단의 북쪽 마구리를 덮는다. 이쪽은
	// 2.2 m가 통째로 드러나 있어 계단참에서 올려다보면 바로 보인다.
	// 열네 번째 단은 Y=186에서 끝나므로 그 북쪽 면은 Y=197, 위로는 계단참
	// 밑면 Z=1182까지다.
	CreateBlock(
		FVector(-277.5f, 197.3f, 1072.5f),
		FVector(85.0f, 0.6f, 219.0f),
		TexMat(TEXT("M_SteelDoorUV"), MetalFrameMaterial),
		false);
	IGPrologueWorld::TagFootstepSurface(CreateBlock(
		FVector(-277.5f, 208.5f, 1191.0f),
		FVector(85.0f, 23.0f, 18.0f),
		StairSteel), IGPrologueWorld::FootstepMetalStairTag);

	// Enclose the upper flight. The side walls overlap the existing stub by
	// 10 cm, so there is no blue-sky seam when looking up from 4F.
	CreateBlock(
		FVector(-332.5f, 52.5f, 1200.0f),
		FVector(15.0f, 325.0f, 480.0f),
		AnnexWallY);
	CreateBlock(
		FVector(-222.5f, 52.5f, 1200.0f),
		FVector(15.0f, 325.0f, 480.0f),
		AnnexWallY);

	// The reinforced roof slab is split around the stair opening. All three
	// pieces collide; their top plane and the annex floor are exactly Z=1200.
	MissingFloorRooftopRouteFloors.Add(CreateBlock(
		FVector(-420.0f, -40.0f, 1190.0f),
		FVector(160.0f, 520.0f, 20.0f),
		RoofDeck));
	MissingFloorRooftopRouteFloors.Add(CreateBlock(
		FVector(142.5f, -40.0f, 1190.0f),
		FVector(715.0f, 520.0f, 20.0f),
		RoofDeck));
	MissingFloorRooftopRouteFloors.Add(CreateBlock(
		FVector(0.0f, 360.0f, 1190.0f),
		FVector(1000.0f, 280.0f, 20.0f),
		RoofDeck));
	for (UStaticMeshComponent* RouteFloor : MissingFloorRooftopRouteFloors)
	{
		IGPrologueWorld::TagFootstepSurface(
			RouteFloor,
			IGPrologueWorld::FootstepRooftopTag);
	}

	// A 2-ton-class cylindrical rooftop tank at authored scale. The detailed
	// shell is visual-only; a hidden simple cylinder owns dependable collision.
	const FVector TankCenter(0.0f, -25.0f, 0.0f);
	CreateBlock(
		TankCenter + FVector(0.0f, 0.0f, 1250.0f),
		FVector(360.0f, 360.0f, 100.0f),
		AnnexFloor);
	if (UStaticMesh* TankShell = PropMesh(TEXT("SM_RooftopWaterTankShell")))
	{
		CreateBlock(
			TankCenter + FVector(0.0f, 0.0f, 1430.0f),
			FVector(100.0f),
			RoofMetal,
			false,
			TankShell);
	}
	else
	{
		CreateBlock(
			TankCenter + FVector(0.0f, 0.0f, 1430.0f),
			FVector(306.0f, 306.0f, 260.0f),
			RoofMetal,
			false,
			CylinderMesh);
	}
	if (UStaticMeshComponent* TankCollision = CreateBlock(
			TankCenter + FVector(0.0f, 0.0f, 1430.0f),
			FVector(306.0f, 306.0f, 260.0f),
			RoofMetal,
			true,
			CylinderMesh))
	{
		TankCollision->SetVisibility(false, true);
		TankCollision->SetHiddenInGame(true, true);
		TankCollision->SetCastShadow(false);
	}

	// The route centerline is exactly 407.5 + 232.5 = 640 cm. Rail placement
	// leaves a 120 cm clear lane and makes the corner physical, not a waypoint.
	auto AddRail = [this, RailMetal](
		const FVector& Start,
		const FVector& End)
	{
		const FVector Delta = End - Start;
		const float Length = Delta.Size2D();
		if (Length <= KINDA_SMALL_NUMBER)
		{
			return;
		}
		const FVector Midpoint = (Start + End) * 0.5f;
		const float Yaw = FMath::RadiansToDegrees(FMath::Atan2(Delta.Y, Delta.X));
		for (const float Z : {1248.0f, 1294.0f})
		{
			CreateBlock(
				FVector(Midpoint.X, Midpoint.Y, Z),
				FVector(Length, 4.0f, 4.0f),
				RailMetal,
				true,
				nullptr,
				FRotator(0.0f, Yaw, 0.0f));
		}
		// Roughly one-metre post bays match ordinary Korean rooftop guardrails.
		// The two continuous horizontal bars own collision between the posts, so
		// denser greybox pickets only add visual noise without improving safety.
		const int32 PostCount = FMath::Max(2, FMath::CeilToInt(Length / 100.0f) + 1);
		for (int32 PostIndex = 0; PostIndex < PostCount; ++PostIndex)
		{
			const float Alpha = PostCount > 1
				? static_cast<float>(PostIndex) / static_cast<float>(PostCount - 1)
				: 0.0f;
			const FVector Point = FMath::Lerp(Start, End, Alpha);
			CreateBlock(
				FVector(Point.X, Point.Y, 1247.0f),
				FVector(4.0f, 4.0f, 94.0f),
				RailMetal);
		}
	};
	AddRail(FVector(-277.5f, 160.0f, 0.0f), FVector(-180.0f, 160.0f, 0.0f));
	AddRail(FVector(-277.5f, 280.0f, 0.0f), FVector(70.0f, 280.0f, 0.0f));
	AddRail(FVector(190.0f, 160.0f, 0.0f), FVector(190.0f, 452.5f, 0.0f));
	AddRail(FVector(70.0f, 280.0f, 0.0f), FVector(70.0f, 452.5f, 0.0f));

	// Door frames make both openings legible even before their interactive
	// leaves are spawned by the night-three director.
	for (const float DoorJambX : {-327.5f, -227.5f})
	{
		CreateBlock(
			FVector(DoorJambX, 220.0f, 1305.0f),
			FVector(10.0f, 12.0f, 210.0f),
			RailMetal);
	}
	CreateBlock(
		FVector(-277.5f, 220.0f, 1412.5f),
		FVector(110.0f, 12.0f, 15.0f),
		RailMetal);

	// 석고 파편이 깔린 슬래브. 어두운 콘크리트로 그리면 5층 바닥과 복도가
	// 같은 물건이 되는데, 이 바닥은 걷기만 해도 크게 들리는 자리다.
	IGPrologueWorld::TagFootstepSurface(
		CreateBlock(
			FVector(0, 700, 1195), FVector(800, 500, 10),
			TexMat(TEXT("M_MissingFloorGypsumDebris_XY"), AnnexFloor)),
		IGPrologueWorld::FootstepGypsumTag);
	// §11 V2 분진 퇴적: this is the one floor in the building deep enough in
	// plaster dust to hold a print. The field covers the slab exactly, so a
	// footfall in the stairwell or the corridor is reported and simply not drawn.
	if (!SettledDust)
	{
		SettledDust = NewObject<UIGSettledDustComponent>(
			this,
			TEXT("MissingFloorSettledDust"));
		if (SettledDust)
		{
			SettledDust->SetupAttachment(GetRootComponent());
			SettledDust->RegisterComponent();
		}
	}
	if (SettledDust)
	{
		constexpr float AnnexFloorTopZ = 1200.0f;
		SettledDust->ConfigureField(
			FBox(
				FVector(-400.0f, 450.0f, AnnexFloorTopZ),
				FVector(400.0f, 950.0f, AnnexFloorTopZ)),
			AnnexFloorTopZ);
	}
	CreateBlock(FVector(0, 700, 1445), FVector(800, 500, 10), AnnexCeiling);
	CreateBlock(FVector(0, 947.5f, 1320), FVector(800, 15, 240), AnnexWallX);
	// South wall split around the second 90 cm fire door (X=85..175).
	CreateBlock(FVector(-157.5f, 452.5f, 1320), FVector(485, 15, 240), AnnexWallX);
	CreateBlock(FVector(287.5f, 452.5f, 1320), FVector(225, 15, 240), AnnexWallX);
	CreateBlock(FVector(130.0f, 452.5f, 1422.5f), FVector(90, 15, 35), AnnexWallX);
	// Steel casings lapping the opening on the annex side. In the plane of the
	// wall they were thinner than it and disappeared into it completely.
	for (const float DoorJambX : {80.0f, 180.0f})
	{
		CreateBlock(
			FVector(DoorJambX, 466.0f, 1305.0f),
			FVector(10.0f, 12.0f, 210.0f),
			RailMetal);
	}
	CreateBlock(FVector(-397.5f, 700, 1320), FVector(15, 480, 240), AnnexWallY);
	CreateBlock(FVector(397.5f, 700, 1320), FVector(15, 480, 240), AnnexWallY);

	// Three finished bays in a row: only the middle one hides a cavity, and
	// only sound can tell them apart. Gypsum faces with exposed stud edges.
	for (const float BayY : {560.0f, 700.0f, 840.0f})
	{
		UStaticMeshComponent* BayPanel = CreateBlock(
			FVector(257.5f, BayY, 1320),
			FVector(15, 120, 240),
			AnnexWallY);
		if (FMath::IsNearlyEqual(BayY, 700.0f))
		{
			MissingFloorCavityWallPanel = BayPanel;
		}
		CreateBlock(FVector(249.0f, BayY - 62.0f, 1320), FVector(4, 6, 240), Stud, false);
		CreateBlock(FVector(249.0f, BayY + 62.0f, 1320), FVector(4, 6, 240), Stud, false);
	}
	// The riser: two tonnes of water passing behind bay B on its way down.
	CreateBlock(
		FVector(310, 700, 1320), FVector(24, 24, 240),
		TexMat(TEXT("M_StainlessUV"), MetalFrameMaterial), false,
		CylinderMesh);

	// Stopped-construction dressing: pallet stacks, boards, a bare hanging
	// bulb block so the room reads without a flashlight.
	CreateBlock(FVector(0, 590, 1230), FVector(120, 80, 60), Board);
	CreateBlock(FVector(-80, 780, 1215), FVector(140, 60, 30), Board);
	CreateBlock(FVector(-90, 770, 1236), FVector(60, 40, 12), Stud, false);
	CreateBlock(FVector(120, 880, 1224), FVector(90, 50, 48), Board);
	CreateBlock(FVector(0, 700, 1436), FVector(10, 10, 8), PlasticDarkMaterial, false);

	// 비닐. §8 비트 2-2's shot list names three things and this is the second:
	// 자재 더미, 비닐, 낮은 형체. Translucent film over stalled material is what
	// makes the frame read as a floor somebody stopped building rather than a
	// corridor somebody swept. Shadow casting stays off — the room has one bulb
	// and translucent shadows cost more than they say here.
	UMaterialInterface* Sheeting = TexMat(TEXT("M_CarrierBagFilm"), GlassMaterial);
	auto AddSheeting = [this, Sheeting](
		const FVector& Center,
		const FVector& Size,
		const FRotator& Rotation)
	{
		if (UStaticMeshComponent* Film = CreateBlock(
				Center, Size, Sheeting, false, nullptr, Rotation))
		{
			Film->SetCastShadow(false);
			Film->SetCanEverAffectNavigation(false);
		}
	};
	// Draped over the tallest stack, slumping off one corner.
	AddSheeting(
		FVector(0.0f, 588.0f, 1262.0f),
		FVector(132.0f, 96.0f, 1.5f),
		FRotator(2.5f, 6.0f, -3.5f));
	// A second sheet already pulled off and left where it fell. Two overlapping
	// quads at different angles rather than one rectangle: a single slab of film
	// on a dark floor reads as a board somebody leaned there, which is what the
	// first pass of the CCTV frame showed. Kept clear of the camera's near lane
	// so the low shape crosses floor, not plastic.
	AddSheeting(
		FVector(-296.0f, 842.0f, 1201.6f),
		FVector(104.0f, 86.0f, 1.2f),
		FRotator(1.5f, 24.0f, -2.0f));
	AddSheeting(
		FVector(-244.0f, 878.0f, 1203.4f),
		FVector(78.0f, 62.0f, 1.0f),
		FRotator(-2.5f, -14.0f, 3.0f));
	// Hung off the bay studs at the far right of the camera's frame, so the shot
	// has depth on that side instead of ending on a flat gypsum face.
	AddSheeting(
		FVector(238.0f, 552.0f, 1318.0f),
		FVector(1.5f, 148.0f, 232.0f),
		FRotator(0.0f, 0.0f, 1.5f));

	// §14 CCTV 채널 5's camera, and it is a permanent fixture rather than part of
	// the beat: §17's payoff table requires the player to stand at this exact
	// vantage in 밤3 and recognise the framing they were shown in 밤2. If the
	// camera only existed while the channel was live, that recognition would have
	// nothing to land on. AIGCctvChannelFive reads MissingFloorCctvCamera's world
	// transform, so this transform *is* the shot — moving it moves both.
	// Corner mount flat to the ceiling, then a short arm out to the housing.
	const FVector CctvMount(-372.0f, 478.0f, 1438.0f);
	if (UStaticMeshComponent* MountPlate = CreateBlock(
			CctvMount + FVector(0.0f, 0.0f, 1.0f),
			FVector(16.0f, 16.0f, 2.0f),
			RailMetal,
			false))
	{
		MountPlate->SetCanEverAffectNavigation(false);
	}
	const FVector CctvArm = IGPrologueWorld::CctvCameraLocation - CctvMount;
	if (UStaticMeshComponent* Bracket = CreateBlock(
			CctvMount + CctvArm * 0.5f,
			FVector(CctvArm.Size(), 2.6f, 2.6f),
			RailMetal,
			false,
			nullptr,
			CctvArm.Rotation()))
	{
		Bracket->SetCanEverAffectNavigation(false);
	}
	MissingFloorCctvCamera = CreateBlock(
		IGPrologueWorld::CctvCameraLocation,
		FVector(15.0f, 9.0f, 8.5f),
		PlasticDarkMaterial,
		false,
		nullptr,
		IGPrologueWorld::CctvCameraRotation);
	if (MissingFloorCctvCamera)
	{
		MissingFloorCctvCamera->SetCanEverAffectNavigation(false);
		// The camera's own illuminator, and it is permanent for the same reason
		// the housing is: a light that only exists while the channel is live would
		// make 밤3's vantage a different place than the one 밤2 showed. Every
		// corridor camera of this class has an IR ring, and it is what gives the
		// shot its shape — near material readable, the far end of the room gone.
		// Deliberately not in MissingFloorAnnexLights: the night-four breaker cuts
		// the ceiling bulb, and this runs off the camera's own supply.
		CreateLight(
			IGPrologueWorld::CctvCameraLocation
				+ IGPrologueWorld::CctvCameraRotation.Vector() * 12.0f,
			2400.0f,
			640.0f,
			FLinearColor(0.62f, 0.66f, 0.72f),
			false);
		// The lens housing, so the thing reads as pointed rather than as a box.
		CreateBlock(
			IGPrologueWorld::CctvCameraLocation
				+ IGPrologueWorld::CctvCameraRotation.Vector() * 9.0f,
			FVector(5.0f, 5.5f, 5.5f),
			TexMat(TEXT("M_StainlessUV"), MetalFrameMaterial),
			false,
			CylinderMesh,
			IGPrologueWorld::CctvCameraRotation + FRotator(90.0f, 0.0f, 0.0f))
			->SetCanEverAffectNavigation(false);
	}
	// §5.5 물리적 확인의 나머지 절반. One coax leaves this camera and never
	// reaches the NVR: it runs the ceiling to the stair core and drops straight
	// to the spare BNC input on the booth monitor. The channel is visible and
	// unrecorded because of this cable, and the cable is here to be followed.
	const FVector CoaxRun[] = {
		CctvMount + FVector(0.0f, 0.0f, 0.2f),
		FVector(-372.0f, 466.0f, 1438.2f),
		FVector(-300.0f, 464.0f, 1438.2f),
		FVector(-277.5f, 461.0f, 1438.2f),
	};
	for (int32 CoaxIndex = 0; CoaxIndex + 1 < UE_ARRAY_COUNT(CoaxRun); ++CoaxIndex)
	{
		const FVector Start = CoaxRun[CoaxIndex];
		const FVector End = CoaxRun[CoaxIndex + 1];
		const FVector Delta = End - Start;
		const float Length = Delta.Size();
		if (Length <= KINDA_SMALL_NUMBER)
		{
			continue;
		}
		if (UStaticMeshComponent* Coax = CreateBlock(
				(Start + End) * 0.5f,
				FVector(Length, 1.1f, 1.1f),
				PlasticDarkMaterial,
				false,
				nullptr,
				Delta.Rotation()))
		{
			Coax->SetCastShadow(false);
			Coax->SetCanEverAffectNavigation(false);
		}
	}

	// ImageGen residue arrives as value masks, never as baked lighting. Thin
	// masked receivers sit on the real wall/floor planes so the flashlight,
	// contact angle and PBR surface underneath stay coherent. None collides.
	auto AddResidue = [this](
		const FVector& Center,
		const FVector& Size,
		const FName MaterialName,
		const FRotator& Rotation) -> UStaticMeshComponent*
	{
		if (UStaticMeshComponent* Residue = CreateBlock(
				Center,
				Size,
				TexMat(MaterialName, SignWhiteMaterial),
				false,
				nullptr,
				Rotation))
		{
			Residue->SetCastShadow(false);
			Residue->SetCanEverAffectNavigation(false);
			return Residue;
		}
		return nullptr;
	};
	MissingFloorCavityWallResidue = AddResidue(
		FVector(249.65f, 700.0f, 1318.0f),
		FVector(0.30f, 102.0f, 118.0f),
		TEXT("M_MissingFloorHandprints"),
		FRotator::ZeroRotator);
	AddResidue(
		FVector(20.0f, 718.0f, 1200.25f),
		FVector(250.0f, 112.0f, 0.30f),
		TEXT("M_MissingFloorDragTrails"),
		FRotator(0.0f, 13.0f, 0.0f));
	// 분진 이음은 열린 바닥 가운데가 아니라 벽과 바닥이 만나는 선에 쌓인다.
	// 북쪽 벽(Y=940) 아래로 붙여 길고 얇게 눕히면 이음으로 읽히고, 끌림 자국과
	// 나란해진다 — 앞서는 이 둘이 열린 바닥에서 서로 교차해, 카메라에서 보면
	// 칠해 놓은 X 한 개로 합쳐졌다. 마스크의 대각선이 가로세로 비에 따라 눕는
	// 각도가 달라지므로, 같은 계열의 잔흔을 다른 요각으로 겹쳐 두면 안 된다.
	AddResidue(
		FVector(150.0f, 906.0f, 1200.26f),
		FVector(268.0f, 42.0f, 0.32f),
		TEXT("M_MissingFloorDustJoint"),
		FRotator(0.0f, 0.0f, 0.0f));
	// This face is deliberately behind bay B. It becomes visible only after
	// the night-four wall panel is removed, so the reveal cannot leak early.
	AddResidue(
		FVector(266.0f, 700.0f, 1322.0f),
		FVector(0.28f, 104.0f, 150.0f),
		TEXT("M_MissingFloorCavityScratches"),
		FRotator::ZeroRotator);
	MissingFloorAnnexLights.Add(CreateLight(
		FVector(0, 700, 1420), 650.0f, 900.0f,
		FLinearColor(1.0f, 0.93f, 0.82f), false));
	// The upper flight is deliberately dim, but it must still read as fourteen
	// grounded treads rather than a black transition volume. A cold bulkhead
	// spill also silhouettes the first real roof door from the fourth floor.
	MissingFloorAnnexLights.Add(CreateLight(
		FVector(-277.5f, 92.0f, 1358.0f), 820.0f, 470.0f,
		FLinearColor(0.50f, 0.61f, 0.76f), true, 8.0f));
	MissingFloorAnnexLights.Add(CreateLight(
		FVector(-35.0f, 224.0f, 1450.0f), 760.0f, 760.0f,
		FLinearColor(0.48f, 0.58f, 0.72f), true, 12.0f));
	// A separate battery emergency practical is deliberately not inserted into
	// MissingFloorAnnexLights. When Mok cuts the annex breaker, this weak red
	// pool survives and blends with the player's warm flashlight instead of
	// reducing the finale to featureless black.
	CreateBlock(
		FVector(-389.0f, 700.0f, 1370.0f),
		FVector(2.0f, 18.0f, 34.0f),
		SnackRedMaterial,
		false);
	if (UPointLightComponent* EmergencyPractical = CreateLight(
		FVector(-356.0f, 700.0f, 1355.0f),
		165.0f,
		430.0f,
		FLinearColor(1.0f, 0.055f, 0.018f),
		false))
	{
		EmergencyPractical->SetSourceRadius(5.0f);
		EmergencyPractical->SetVolumetricScatteringIntensity(0.14f);
	}

	ActiveParent = nullptr;
}

void AIGPrologueWorldScene::SetUnit403AgeStage(const int32 Stage)
{
	const int32 Clamped = FMath::Clamp(Stage, 0, 2);
	if (Unit403AgeStage == Clamped)
	{
		return;
	}
	Unit403AgeStage = Clamped;
	ApplyUnit403AgeStage();
}

void AIGPrologueWorldScene::ApplyUnit403AgeStage()
{
	// Cumulative, not exclusive: stage two keeps stage one's cracks and adds to
	// them. Damage does not move house.
	for (const TObjectPtr<UStaticMeshComponent>& Plane : Unit403AgeStageOne)
	{
		if (Plane)
		{
			Plane->SetHiddenInGame(Unit403AgeStage < 1);
		}
	}
	for (const TObjectPtr<UStaticMeshComponent>& Plane : Unit403AgeStageTwo)
	{
		if (Plane)
		{
			Plane->SetHiddenInGame(Unit403AgeStage < 2);
		}
	}
}

int32 AIGPrologueWorldScene::GetUnit403AgingPlaneCount() const
{
	int32 Count = 0;
	if (Unit403AgeStage >= 1)
	{
		Count += Unit403AgeStageOne.Num();
	}
	if (Unit403AgeStage >= 2)
	{
		Count += Unit403AgeStageTwo.Num();
	}
	return Count;
}

void AIGPrologueWorldScene::SetTheHourSealed(const bool bSealed)
{
	bTheHourSealed = bSealed;
	if (PostProcess)
	{
		// Night never lifts the corridor into grey. Film grain is restrained at
		// rest, then the stress layer can take it to 0.08 during pursuit.
		PostProcess->Settings.FilmGrainIntensity = bSealed ? 0.04f : 0.02f;
		PostProcess->Settings.AutoExposureMaxBrightness = bSealed ? 1.30f : 5.0f;
		// §11 V1: 자동노출 하한 잠금. Capping the ceiling alone still let the
		// histogram adapt *down* into an unlit corridor and quietly hand the
		// player night vision — which is exactly the currency the torch is
		// supposed to be. Pinning the floor to the ceiling freezes exposure for
		// the whole hour, so the dark stays as dark as it was authored and the
		// beam is the only thing that reveals anything.
		PostProcess->Settings.AutoExposureMinBrightness = bSealed ? 1.30f : -0.5f;
	}

	// The 공동현관. Shut the leaf first: a swing door only consults its
	// requirements while closed, so sealing an open door is a no-op and the
	// player strolls out. ForceOpenState is instant and tick-free, which also
	// dodges the proximity auto-reopen a scripted swing would expose.
	if (BuildingDoor)
	{
		if (bSealed)
		{
			BuildingDoor->ForceOpenState(false);
			TArray<FIGDoorRequirement> SealRequirements;
			FIGDoorRequirement& Seal = SealRequirements.AddDefaulted_GetRef();
			// Deliberately a state that is never granted during the hour. The
			// release below drops the requirement instead of granting the tag,
			// so no 'sealed' fact can ever be written into a save.
			Seal.RequiredState = FGameplayTag::RequestGameplayTag(
				FName(TEXT("State.MissingFloor.Night.MorningCame")),
				false);
			Seal.LockedPrompt = NSLOCTEXT(
				"IGMissingFloor", "EntranceSealedPrompt", "공동현관");
			Seal.LockedThought = NSLOCTEXT(
				"IGMissingFloor",
				"EntranceSealedThought",
				"…안 열린다. 잠긴 것도 아닌데.");
			BuildingDoor->SetRequirements(MoveTemp(SealRequirements));
		}
		else
		{
			TArray<FIGDoorRequirement> NoRequirements;
			BuildingDoor->SetRequirements(MoveTemp(NoRequirements));
		}
	}

	// The lift is dead for the hour. Its hall button *is* its interaction, so
	// disabling that is a complete lockout while the cab shells stay solid.
	if (Elevator)
	{
		Elevator->SetInteractionEnabled(!bSealed);
		if (!bSealed)
		{
			Elevator->ResetForNewRide();
		}
	}

	// The second seal, without which the stairs deliver the player into the
	// open car park and out to the alley.
	if (StairCoreNightGate)
	{
		StairCoreNightGate->SetHiddenInGame(!bSealed);
		StairCoreNightGate->SetCollisionEnabled(
			bSealed
				? ECollisionEnabled::QueryAndPhysics
				: ECollisionEnabled::NoCollision);
		StairCoreNightGate->SetCollisionProfileName(
			bSealed
				? UCollisionProfile::BlockAll_ProfileName
				: UCollisionProfile::NoCollision_ProfileName);
	}
}

void AIGPrologueWorldScene::SetNightStairPocketEnabled(const bool bEnabled)
{
	if (!StairTransition)
	{
		return;
	}
	const auto ToWorld = [this](const FVector& Local)
	{
		return GetActorTransform().TransformPosition(Local);
	};
	// Configure only repositions the portal boxes, so re-calling it is the
	// supported way to slide the trigger. Exits stay untouched in both modes.
	//
	// The night position is measured against the flight, not guessed. The
	// portal box is 24 cm half-width and the pawn capsule is 34 cm, so the
	// furthest west a player can stand is 58 cm east of the portal's centre.
	// Treads run west from X -341 at 22 cm apart, so -452 puts that line at
	// -394 and leaves the third tread (centre -385, two risers down at local
	// Z -36) standing room; the fourth (-407) trips the portal. At -440 the
	// line was -382 and the pocket was a single step. The far shaft wall is
	// at X -445, which is where the sighting figure has its ear, so the
	// figure stays inside the portal box and unreachable in both cases.
	const FVector UpperTrigger = bEnabled
		? FVector(-452.0f, -305.0f, 880.0f)
		: FVector(-352.0f, -305.0f, 915.0f);
	StairTransition->Configure(
		ToWorld(UpperTrigger),
		ToWorld(FVector(-300.0f, -305.0f, 942.0f)),
		FRotator(0.0f, 0.0f, 0.0f),
		ToWorld(FVector(-214.0f, -305.0f, 193.0f)),
		ToWorld(FVector(-188.0f, -305.0f, 187.0f)),
		FRotator(0.0f, 0.0f, 0.0f));
}

bool AIGPrologueWorldScene::ValidateMissingFloorRooftopRoute(
	float& OutCenterlineLengthCentimeters,
	int32& OutUpperStepCount) const
{
	OutCenterlineLengthCentimeters =
		FVector::Dist2D(
			IGPrologueWorld::MissingFloorRouteStart,
			IGPrologueWorld::MissingFloorRouteCorner)
		+ FVector::Dist2D(
			IGPrologueWorld::MissingFloorRouteCorner,
			IGPrologueWorld::MissingFloorRouteEnd);
	OutUpperStepCount = MissingFloorUpperStairSteps.Num();

	if (!FMath::IsNearlyEqual(
			OutCenterlineLengthCentimeters,
			IGPrologueWorld::MissingFloorRouteLengthCentimeters,
			0.1f)
		|| OutUpperStepCount != IGPrologueWorld::MissingFloorUpperStepCount
		|| MissingFloorRooftopRouteFloors.Num() != 3)
	{
		return false;
	}

	auto HasWalkableCollision = [](const UStaticMeshComponent* Component)
	{
		return Component
			&& Component->GetCollisionEnabled() != ECollisionEnabled::NoCollision
			&& Component->GetCollisionResponseToChannel(ECC_Pawn) == ECR_Block;
	};
	for (const UStaticMeshComponent* Step : MissingFloorUpperStairSteps)
	{
		if (!HasWalkableCollision(Step))
		{
			return false;
		}
	}
	for (const UStaticMeshComponent* Floor : MissingFloorRooftopRouteFloors)
	{
		if (!HasWalkableCollision(Floor))
		{
			return false;
		}
	}
	return true;
}

FVector AIGPrologueWorldScene::GetMissingFloorCctvCameraLocation() const
{
	return IGPrologueWorld::CctvCameraLocation;
}

FRotator AIGPrologueWorldScene::GetMissingFloorCctvCameraRotation() const
{
	return IGPrologueWorld::CctvCameraRotation;
}

float AIGPrologueWorldScene::GetMissingFloorCctvFieldOfView() const
{
	return IGPrologueWorld::CctvCameraFieldOfView;
}

void AIGPrologueWorldScene::SetMissingFloorAnnexPower(const bool bPowered)
{
	for (UPointLightComponent* Light : MissingFloorAnnexLights)
	{
		if (Light)
		{
			Light->SetVisibility(bPowered, true);
		}
	}
}

bool AIGPrologueWorldScene::OpenMissingFloorCavity()
{
	if (!MissingFloorCavityWallPanel)
	{
		return false;
	}
	MissingFloorCavityWallPanel->SetVisibility(false, true);
	MissingFloorCavityWallPanel->SetHiddenInGame(true, true);
	MissingFloorCavityWallPanel->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	if (MissingFloorCavityWallResidue)
	{
		// The receiver belongs to the removable face. Leaving it visible after
		// the gypsum disappears would create a pair of handprints floating over
		// the cavity — exactly the kind of physical break this reveal cannot bear.
		MissingFloorCavityWallResidue->SetVisibility(false, true);
		MissingFloorCavityWallResidue->SetHiddenInGame(true, true);
	}
	bMissingFloorCavityOpen = true;
	return true;
}

bool AIGPrologueWorldScene::ResetMissingFloorCavity()
{
	if (!MissingFloorCavityWallPanel)
	{
		return false;
	}
	MissingFloorCavityWallPanel->SetVisibility(true, true);
	MissingFloorCavityWallPanel->SetHiddenInGame(false, true);
	MissingFloorCavityWallPanel->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	if (MissingFloorCavityWallResidue)
	{
		MissingFloorCavityWallResidue->SetVisibility(true, true);
		MissingFloorCavityWallResidue->SetHiddenInGame(false, true);
	}
	bMissingFloorCavityOpen = false;
	return true;
}

bool AIGPrologueWorldScene::DropCorridorExtinguisher()
{
	if (!CorridorExtinguisher || bCorridorExtinguisherDropped)
	{
		return false;
	}
	bCorridorExtinguisherDropped = true;

	CorridorExtinguisher->SetSimulatePhysics(true);
	// A shove at the neck, toward the walkway, so the cylinder tips into the
	// player's path instead of rolling into the wall.
	const FVector Neck =
		CorridorExtinguisher->GetComponentLocation() + FVector(0.0f, 0.0f, 20.0f);
	CorridorExtinguisher->AddImpulseAtLocation(
		FVector(0.0f, 260.0f, 40.0f) * CorridorExtinguisher->GetMass(),
		Neck);

	// Physics only runs for the fall itself: once the clatter has settled the
	// body freezes where it lies and costs nothing again.
	GetWorldTimerManager().SetTimer(
		ExtinguisherSettleTimer,
		FTimerDelegate::CreateWeakLambda(this, [this]()
		{
			if (CorridorExtinguisher)
			{
				CorridorExtinguisher->SetSimulatePhysics(false);
			}
		}),
		4.0f,
		false);
	return true;
}

FVector AIGPrologueWorldScene::GetCorridorExtinguisherLocation() const
{
	return CorridorExtinguisher
		? CorridorExtinguisher->GetComponentLocation()
		: GetActorTransform().TransformPosition(FVector(232.0f, -364.0f, 926.0f));
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
				AlarmWorldLocation,
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

	// 떨림 자체(0.86~1.06)는 어두운 쪽이 충분히 밝아 점멸로 치지 않는다.
	// 5% 확률로 0.12까지 떨어지는 드롭아웃은 점멸이다. 점멸 감소를 켠 사람은
	// 낡은 형광등 아래를 걷되 그 떨어짐은 겪지 않아야 한다.
	const bool bReducedFlicker = GetGameInstance()
		&& GetGameInstance()->GetSubsystem<UIGAccessibilitySubsystem>()
		&& GetGameInstance()->GetSubsystem<UIGAccessibilitySubsystem>()
			->IsReducedFlickerEnabled();
	if (bReducedFlicker)
	{
		DegradedCorridorLight->SetIntensity(DegradedLightBaseIntensity * 0.94f);
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
	// North wall of the connector, split around the 관리실 doorway (X 120..200)
	// instead of the old single slab. Coverage outside the opening is
	// unchanged, so legacy traversal never notices.
	CreateBlock(FVector(15.5f, -235, 120), FVector(209, 20, 240), LobbyWallX);
	CreateBlock(FVector(315, -235, 120), FVector(230, 20, 240), LobbyWallX);
	CreateBlock(FVector(160, -235, 225), FVector(80, 20, 30), LobbyWallX);

	// 없는 층: the management booth, tucked behind the connector's north wall
	// where Korean villas actually put it — beside the way in. Mok Hansu's
	// daytime post and the night's P2 stage: the complaint ledger, the carbon
	// ledger underneath it, the CCTV monitor with one channel too many, and
	// the inner room whose door edge shows the egg-crate foam
	// (STORY_BIBLE_MISSING_FLOOR.md §8 밤2).
	CreateBlock(FVector(170, -155, -10), FVector(240, 160, 20), LobbyFloor);
	CreateBlock(FVector(170, -155, 250), FVector(240, 160, 20), LobbyCeil);
	CreateBlock(FVector(170, -77.5f, 120), FVector(240, 15, 240), LobbyWallX);
	CreateBlock(FVector(52.5f, -155, 120), FVector(15, 140, 240), LobbyWallY);
	CreateBlock(FVector(287.5f, -155, 120), FVector(15, 140, 240), LobbyWallY);
	// Desk with the monitor shell; the interactables land on it later, from
	// the night-2 director, because BuildLobby runs before any actor exists.
	CreateBlock(FVector(160, -110, 38), FVector(110, 55, 76), ShelfSteel);
	// 케이스는 검은 플라스틱이고 빛나는 것은 화면뿐이다. 예전에는 발광 재질을
	// 40x10x28 상자 전체에 발라서, 옆면과 윗면까지 빛나는 하늘색 덩어리가
	// 책상에 놓여 있었다. 화면은 케이스 앞면(Y=-105)보다 살짝 앞에 세우되
	// AIGCctvChannelFive가 채널 5 동안 띄우는 렌더 면(Y=-105.45)과는 겹치지
	// 않게 그 사이에 둔다.
	CreateBlock(
		FVector(150, -100, 96), FVector(40, 10, 28),
		PlasticDarkMaterial, false);
	CreateBlock(
		FVector(150, -105.2f, 96), FVector(34, 0.3f, 25.5f),
		ScreenGlowMaterial, false);
	CreateBlock(FVector(150, -103, 80), FVector(12, 8, 8), PlasticDarkMaterial, false);
	// The inner room's door leaf, always shut: a dark slab with a hairline
	// gap the foam reveal peers through. It never opens — that is the point.
	CreateBlock(FVector(240, -84.5f, 105), FVector(70, 5, 210), PlasticDarkMaterial, false);
	CreateBlock(FVector(272, -84, 105), FVector(2.4f, 3, 200), ConcreteDarkMaterial, false);
	// Booth ceiling lamp block so the room is not a cave in the day section.
	CreateBlock(FVector(170, -150, 237), FVector(24, 24, 4), PlasticDarkMaterial, false);
	CreateLight(FVector(170, -150, 226), 900.0f, 420.0f,
		FLinearColor(1.0f, 0.95f, 0.85f), false);
	// §8 비트 2-5's 낙하물. Boards and a paint tin stored against the wall just
	// outside the booth door, where a caretaker who is quietly building an extra
	// floor would keep them. Permanent dressing: the crash has to have had a
	// source the player could have seen on the way in, and the same stack is
	// still standing in the mornings.
	CreateBlock(
		FVector(168, -262, 52), FVector(96, 14, 104),
		TexMat(TEXT("M_ShelfSteelUV"), PlasticDarkMaterial),
		true, nullptr, FRotator(0, 0, 6.0f));
	CreateBlock(
		FVector(196, -258, 34), FVector(52, 12, 68),
		TexMat(TEXT("M_ShelfSteelUV"), PlasticDarkMaterial),
		true, nullptr, FRotator(0, 0, -9.0f));
	CreateBlock(
		FVector(140, -256, 12), FVector(24, 24, 24),
		TexMat(TEXT("M_StainlessUV"), MetalFrameMaterial),
		true, CylinderMesh);

	// Granite skirting round the lobby, matching the landings upstairs.
	CreateBlock(FVector(580, -233.4f, 6), FVector(300, 3.5f, 12), Skirting, false);
	CreateBlock(FVector(448.4f, -305, 6), FVector(3.5f, 160, 12), Skirting, false);

	// 없는 층 night seal: a fire shutter at the stair-core end of the
	// connector. Locking the common entrance alone does not shut the building
	// in — the ground stair mouth opens into the open pilotis car park, so the
	// stairs are a way out to the alley. Built here so the geometry exists from
	// the first frame, but hidden and non-colliding until SetTheHourSealed
	// lowers it; the legacy chapters therefore never see it.
	StairCoreNightGate = CreateBlock(
		FVector(-83, -305, 120),
		FVector(10, 120, 240),
		Metal,
		false);
	if (StairCoreNightGate)
	{
		StairCoreNightGate->SetHiddenInGame(true);
	}

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

	// 없는 층 P1: the utility meter cabinet and the distribution panel, on the
	// one genuinely empty wall in the vestibule — the street wall segment
	// X 450..600, whose inner face is Y = -375. Props extend toward +Y so
	// nothing sinks into the 20 cm wall, and everything stays west of X 598 to
	// clear the common-entrance opening and the swept volume of its leaf.
	//
	// Three unit meters, one clearly labelled common meter and a fifth with no
	// nameplate. The dial that does not turn is the whole point, so its component
	// is kept: the other four are given a slow rotation and it is left motionless.
	// 계량기함은 분전반이 아니다. 여기 케이스가 「분전반」 도장면을 쓰고
	// 있어서 8 cm 옆면마다 그 글자가 눌려 찍혔다. 신원은 아래 401·402·403·
	// 공용 명판이 이미 말한다.
	CreateBlock(FVector(506, -371.0f, 150), FVector(96, 8, 62), Metal, false);
	CreateBlock(FVector(506, -366.4f, 150), FVector(98, 1.2f, 64), Metal, false);
	{
		const TCHAR* MeterPlateNames[] = {
			TEXT("M_Plate401"), TEXT("M_Plate402"),
			TEXT("M_Plate403"), TEXT("M_PlateCommon")};
		const float MeterXs[] = {470.0f, 488.0f, 506.0f, 524.0f, 542.0f};
		for (int32 MeterIndex = 0; MeterIndex < 5; ++MeterIndex)
		{
			const float MeterX = MeterXs[MeterIndex];
			CreateBlock(
				FVector(MeterX, -365.4f, 156), FVector(13, 13, 1.2f),
				GlassMaterial, false);
			// 문자판. 눈금과 붉은 호는 텍스처가, 지침과 다섯 번째가 돌지
			// 않는다는 사실은 코드가 소유한다. 회전 원판보다 뒤(Y가 작은
			// 쪽)에 두고 원판을 줄여, 눈금이 원판 둘레로 보이면서 회전도
			// 함께 읽히게 한다 — P1은 「다섯째만 안 돈다」가 전부이므로
			// 원판이 문자판에 가려지면 퍼즐 자체가 사라진다.
			//
			// 깊이는 두 면 사이 0.6cm가 전부다. 함체 강판 앞면이 -365.8,
			// 원판 앞면이 -365.2이고 문자판은 그 사이에 있어야 한다.
			// -366.6/두께 0.8은 -367.0~-366.2라 강판 안에 통째로 들어가
			// 있었다 — 재질이 무엇이든 보일 수가 없는 자리였다. 두께를
			// 0.4로 줄여 양쪽에 0.1cm씩 띄운다.
			CreateBlock(
				FVector(MeterX, -365.5f, 152), FVector(11, 0.4f, 11),
				TexMat(TEXT("M_UtilityMeterDial"), SignWhiteMaterial), false);
			// Roll 90 puts the cylinder axis on world Y, so the disc face is
			// what a reader standing in the lobby actually sees.
			UStaticMeshComponent* Disc = CreateBlock(
				FVector(MeterX, -366.0f, 152), FVector(5, 5, 1.6f),
				PlasticDarkMaterial, false, CylinderMesh, FRotator(0, 0, 90));
			if (MeterIndex == 4)
			{
				FifthMeterDisc = Disc;
				// No nameplate: a unit that is not on any list.
				CreateBlock(
					FVector(MeterX, -365.6f, 133), FVector(15, 1.0f, 6),
					SignWhiteMaterial, false);
			}
			else
			{
				CreateBlock(
					FVector(MeterX, -365.6f, 133), FVector(15, 1.0f, 6),
					TexMat(MeterPlateNames[MeterIndex], SignWhiteMaterial), false);
			}
		}
	}

	// The 두꺼비집, east of the cabinet and clear of the entrance opening.
	// The fifth toggle is the one that is off.
	CreateBlock(FVector(576, -372.0f, 152), FVector(36, 6, 54), Metal, false);
	CreateBlock(
		FVector(576, -368.4f, 152), FVector(37, 1.2f, 55),
		TexMat(TEXT("M_MeterBox"), Metal), false);
	// 스위치판은 두 인쇄 띠 사이에만 놓인다 — 위로 「분전반」(Z 168.5~173.1),
	// 아래로 「취급주의」(Z 130.8~133.8). 34 cm 판이 Z 169까지 올라와 위 글자의
	// 아랫부분을 5 mm 잘라 먹고 있었다. 32 cm로 줄여 두 띠에서 1.3 cm씩 뗀다.
	CreateBlock(
		FVector(576, -367.4f, 151.1f), FVector(30, 1.0f, 32),
		TexMat(TEXT("M_SwitchPlate"), SignWhiteMaterial), false);
	// 스위치판 그림은 **2열** 차단기함이고, 왼쪽 열 맨 아래 슬롯에 주황
	// 표시점이 찍혀 있다. 토글 다섯을 가운데 한 줄로 세워 두는 바람에 그림과
	// 어긋났고, 판에 다섯이 8 cm 간격으로 들어가지 못해 다섯째가 문짝으로
	// 흘러내려 「취급주의」 활자 위에 앉아 있었다. 텍스처에서 실측한 창
	// 자리에 맞춘다 — 왼쪽 창 X 564.3~574.6, 오른쪽 창 X 577.5~587.8,
	// 두 창 모두 Z 139.7~162.1.
	constexpr float BreakerLeftX = 569.4f;
	constexpr float BreakerRightX = 582.7f;
	for (const float BreakerX : {BreakerLeftX, BreakerRightX})
	{
		for (const float BreakerZ : {159.0f, 151.0f})
		{
			CreateBlock(
				FVector(BreakerX, -366.4f, BreakerZ), FVector(4, 2.4f, 5),
				PlasticDarkMaterial, false);
		}
	}
	// The unnamed circuit, visibly thrown down. Kept so P1 can raise it.
	// 자기 자리는 표시점이 찍힌 왼쪽 열 맨 아래 슬롯(Z 143)이고, 지금은
	// 거기서 3 cm 내려와 있다. P1이 올리는 3 cm가 정확히 그 슬롯이라
	// 확인이 「제자리로 돌아왔다」로 읽힌다. 오른쪽 열 맨 아래는 예비 회로다.
	UnnamedBreakerToggle = CreateBlock(
		FVector(BreakerLeftX, -366.4f, 140.0f), FVector(4, 2.4f, 5),
		Stainless, false);

	// The meter-reading clipboard's backing, on the free north-wall band east
	// of the notice board. The note actor itself is spawned later, because
	// BuildLobby runs before any interactable exists.
	// Flat on the wall at the same standoff as the notice board beside it; at
	// Y -238.5 it hung two centimetres off the plaster.
	CreateBlock(FVector(672, -237, 150), FVector(23, 3, 32), Metal, false);

	// Lobby fittings: the video intercom by the door, a notice board over the
	// mailboxes, and the umbrella stand nobody has emptied since the rains.
	CreatePrintedBlock(
		FVector(672, -381, 145), FVector(16, 5, 22),
		FridgeInteriorMaterial,
		TexMat(TEXT("M_Intercom"), SignWhiteMaterial),
		FVector(0, 1, 0));
	CreateBlock(FVector(672, -383.5f, 145), FVector(19, 3, 25), Stainless, false);
	CreatePrintedBlock(
		FVector(600, -239.5f, 196), FVector(84, 3, 44),
		FridgeInteriorMaterial,
		TexMat(TEXT("M_NoticeA4"), SignWhiteMaterial),
		FVector(0, -1, 0));
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
	// 계단 한 단은 바닥에서 자란 한 덩어리다. 디딤면은 화강석 타일을 XY로
	// 읽어 맞지만, 같은 상자의 챌면은 세로 면이라 그 재질이 높이를 따라
	// 변하지 않는다 — 타일 한 줄이 18 cm 높이로 늘어난 민무늬 띠가 된다.
	// 챌면에는 실제 건물이 쓰는 것과 같은 화강석 판재를 YZ로 읽어 3 mm 덧댄다.
	UMaterialInterface* LobbyRiser =
		TexMat(TEXT("M_GranitePanel_Y"), ConcreteMaterial);
	for (int32 LowerStepIndex = 0; LowerStepIndex < 5; ++LowerStepIndex)
	{
		const float StepX = -100.0f - LowerStepIndex * 22.0f;
		const float StepTop = 18.0f + LowerStepIndex * 18.0f;
		CreateBlock(
			FVector(StepX, -305, StepTop * 0.5f),
			FVector(22, 112, StepTop),
			LobbyFloor);
		CreateBlock(
			FVector(StepX + 11.3f, -305, StepTop * 0.5f),
			FVector(0.6f, 112, StepTop),
			LobbyRiser,
			false);
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
	UMaterialInterface* VillaStuccoX =
		TexMat(TEXT("M_VillaStucco_X"), ConcreteMaterial);
	UMaterialInterface* VillaStuccoY =
		TexMat(TEXT("M_VillaStucco_Y"), ConcreteMaterial);
	UMaterialInterface* DarkX = TexMat(TEXT("M_ConcreteDark_X"), ConcreteDarkMaterial);
	UMaterialInterface* DarkY = TexMat(TEXT("M_ConcreteDark_Y"), ConcreteDarkMaterial);
	// 위를 보는 면과 아래를 보는 면. XZ/YZ 변형은 높이를 따라 UV가 변하므로
	// 수평면에 붙이면 한 줄이 폭 방향으로 통째로 늘어난다.
	UMaterialInterface* DarkXY =
		TexMat(TEXT("M_ConcreteDark_XY"), ConcreteDarkMaterial);
	UMaterialInterface* Metal = TexMat(TEXT("M_MetalUV"), MetalFrameMaterial);

	// Asphalt strip from the west dead end to the store front.
	CreateBlock(FVector(1040, -537.5f, -10), FVector(2720, 305, 20), AsphaltWorld);

	// The shop is deeper than the alley is wide, so the ground in front of its
	// northern half was missing entirely — from inside, looking out through
	// the glass showed a hole. Pave that corner and close it with a wall.
	CreateBlock(FVector(2300, -425, -10), FVector(220, 550, 20), AsphaltWorld);
	// The corner belongs to a patched cement-render annex, not to the opposing
	// brick shop row. Its two axes use separate world projections so neither
	// face collapses into the vertical colour stripe seen in the old capture.
	CreateBlock(FVector(2190, -270, 230), FVector(20, 240, 460), VillaStuccoY);
	CreateBlock(FVector(2300, -152, 230), FVector(240, 20, 460), VillaStuccoX);

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
		// 7.8 x 1.7 m 필로티 천장. 바닥과 마주 보는 면이므로 바닥과 같은
		// 축으로 읽어야 한다 — XZ로 읽는 동안 이 면 전체가 콘크리트 한 줄을
		// 1.7 m 늘여 놓은 민무늬였다.
		CreateBlock(FVector(50, -310, 244), FVector(780, 170, 12), DarkXY, false);
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
		CreatePrintedBlock(
			FVector(210, -240, 130), FVector(34, 12, 40),
			SnackRedMaterial,
			TexMat(TEXT("M_FireBox"), SnackRedMaterial),
			FVector(0, -1, 0));
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
	// The lift projects 160 cm beyond the old villa edge. Give that shaft its
	// own thin granite enclosure, then start the neighbouring brick block after
	// it. The previous continuous brick facade intersected the lobby cab and
	// appeared literally inside its left wall.
	CreateBlock(FVector(800, -394, 620), FVector(160, 6, 1240), GranitePanelX);
	CreateBlock(FVector(1015, -385, 230), FVector(270, 20, 460), VillaStuccoX);
	CreateBlock(FVector(1845, -385, 230), FVector(1110, 20, 460), VillaStuccoX);

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
	// 캐노피도 눕힌 판이라 수평 축으로 읽는다.
	CreateBlock(FVector(643, -405, 240), FVector(104, 44, 6), DarkXY, false);
	CreatePrintedBlock(
		FVector(643, -396.5f, 258), FVector(80, 3, 24),
		PlasticDarkMaterial,
		TexMat(TEXT("M_SignVilla"), SignWhiteMaterial),
		FVector(0, -1, 0));
	CreateBlock(FVector(692, -394, 115), FVector(10, 4, 16), PlasticDarkMaterial, false);
	CreateBlock(FVector(695, -395.4f, 118), FVector(3, 1.2f, 3),
		ScreenGlowMaterial, false);
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

	// Dark upstairs windows on the neighbouring block; nobody is awake at this
	// hour. These belong to the street facade at Y -395, not to the pilotis
	// back wall at Y -232: authored on the back wall's plane, every one of
	// them sat 1.6 m inside the building, above a wall that stops at Z 240 and
	// behind granite that hides it. Each pane now beds 1 cm into the stucco it
	// is glazed into, and the bays that fell on the villa's own frontage are
	// gone — that facade already carries its full sash-frame-sill-railing grid
	// for floors two to four, so a bare pane there was a second window drawn
	// over the first.
	const float NorthWindowXs[] = {1000, 1700, 2100};
	for (int32 WindowIndex = 0; WindowIndex < static_cast<int32>(UE_ARRAY_COUNT(NorthWindowXs)); ++WindowIndex)
	{
		const float OffsetZ = (WindowIndex % 2 == 0) ? 300.0f : 310.0f;
		CreateBlock(
			FVector(NorthWindowXs[WindowIndex], -396, OffsetZ),
			FVector(90, 4, 110),
			WindowDarkMaterial,
			false);
	}

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
		// Shopfront clutter stands on the road at Z 0 and beside the kerb,
		// whose face is at Y -657. Authored one or two centimetres up and
		// seven to ten centimetres back, each of these hovered over the
		// asphalt with a corner inside the kerb stone.
		CreateBlock(FVector(1985, -642, 10), FVector(40, 30, 20), FridgeInteriorMaterial);
		CreateBlock(FVector(1985, -642, 29), FVector(38, 28, 18), FridgeInteriorMaterial);
		CreateBlock(FVector(330, -645, 13), FVector(24, 24, 26), CoolerBodyMaterial, true, CylinderMesh);
		CreateBlock(FVector(330, -645, 36), FVector(34, 34, 30), BottleGreenMaterial, false, SphereMesh);
		CreateBlock(FVector(362, -643, 13), FVector(24, 24, 26), CoolerBodyMaterial, true, CylinderMesh);
		CreateBlock(FVector(362, -643, 36), FVector(34, 34, 30), BottleGreenMaterial, false, SphereMesh);
		// Upstairs: PC-bang sign dark, noraebang sign still glowing pink. Both
		// boxes bolt back onto the brick at Y -680; at Y -672 they hung a
		// centimetre off it with nothing carrying the load.
		// 벽에 볼트로 붙은 광고판이라 인쇄는 골목 쪽 한 면뿐이다. 14 cm
		// 몸통에 직접 주면 위아래 마구리에도 같은 상호가 눌려 찍힌다.
		CreatePrintedBlock(
			FVector(900, -673, 330), FVector(170, 14, 40),
			PlasticDarkMaterial,
			TexMat(TEXT("M_SignPC"), PlasticDarkMaterial),
			FVector(0, 1, 0));
		CreatePrintedBlock(
			FVector(1500, -673, 330), FVector(190, 14, 40),
			PlasticDarkMaterial,
			TexMat(TEXT("M_SignKaraoke"), PlasticDarkMaterial),
			FVector(0, 1, 0));
	}
	const float SouthWindowXs[] = {250, 650, 1150, 1750, 2150};
	for (int32 WindowIndex = 0; WindowIndex < static_cast<int32>(UE_ARRAY_COUNT(SouthWindowXs)); ++WindowIndex)
	{
		const float OffsetZ = (WindowIndex % 2 == 0) ? 320.0f : 330.0f;
		// Beds 1 cm into the brick face at Y -680 instead of hanging 2 cm
		// clear of it.
		CreateBlock(
			FVector(SouthWindowXs[WindowIndex], -679, OffsetZ),
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
	// Beside the kerb and clear of the parking cone, not through either.
	// Turned 65 degrees the box reaches 21.2 cm in X and 22.7 cm in Y, so at
	// (2255, -640) it had a corner 5.7 cm inside the kerb stone and 11.2 cm
	// inside the cone -- both of which the solver ejects on the first tick.
	CreatePhysicsProp(
		CubeMesh, CardboardMaterial,
		FVector(0.36f, 0.30f, 0.24f), FVector(2240, -633, 13), FRotator(0, 65, 0), 0.7f);
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
	// 정면을 향한 벽면이므로 Y 변형으로 읽는다. StoreWallX는 X를 따라
	// UV가 변해서, 두께 10 cm짜리 이 기둥에서는 한 줄만 나왔다.
	CreateBlock(FVector(2405, -196, 130), FVector(10, 32, 260), StoreWallY);
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
	CreateBlock(FVector(2405, -531.5f, 245), FVector(10, 297, 36), StoreWallY);

	// Signage: the lettered fascia glows down the whole alley, plus a blade sign.
	// 3.2 m짜리 발광 파사드. 몸통까지 발광 인쇄로 두면 골목에서 올려다볼 때
	// 밑면 12 x 320 cm가 상호를 한 번 더 눌러 찍은 띠로 보인다. 실제 채널
	// 간판이 그렇듯 함체는 어둡고 앞면만 빛난다.
	CreatePrintedBlock(
		FVector(2399, -520, 262), FVector(12, 320, 72),
		PlasticDarkMaterial,
		TexMat(TEXT("M_SignMainLit"), SignMintMaterial),
		FVector(-1, 0, 0));
	// Blade signs hang off the front of a shop, not through it. Centred on
	// X 2402 the 28 cm root crossed the fascia and drove 26 cm of the panel
	// into the ceiling slab, whose west edge oversails the shopfront by
	// 10 cm. The band of sign inside that eave is hidden and the rest is
	// not, so from the alley the sign read as sawn through by the building.
	// It now hangs off the eave's edge at X 2390, above and below it.
	// 돌출 간판은 골목 양쪽에서 읽으므로 두 면 모두 인쇄한다.
	CreatePrintedBlock(
		FVector(2376, -404, 300), FVector(28, 10, 88),
		PlasticDarkMaterial,
		TexMat(TEXT("M_SignBladeLit"), SignWhiteMaterial),
		FVector(0, 1, 0),
		false,
		true);

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
			// Diffuser flush with the ceiling soffit at Z 260. At Z 264 the
			// whole panel sat inside the 20 cm slab and lit nothing.
			StoreLightDiscs.Add(CreateBlock(
				FVector(PanelX, PanelY, 257), FVector(120, 42, 6),
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
		ScreenGlowMaterial, false,
		nullptr, FRotator(-28, 0, 0));
	// Hot-snack warmer glowing at the counter's west end.
	CreateBlock(FVector(2452, -250, 116), FVector(36, 38, 36), PlasticDarkMaterial);
	CreateBlock(FVector(2433, -250, 116), FVector(2, 30, 28), GlassMaterial, false);
	CreateBlock(FVector(2445, -250, 130), FVector(20, 26, 2),
		StreetLampGlowMaterial, false);
	// Tobacco wall behind the counter. The cabinet backs onto the north wall
	// face at Y -180 and the packs stand on its shop-facing side. Authored the
	// other way round, every pack sat behind its own backing board with three
	// centimetres of itself inside the building's wall.
	CreateBlock(FVector(2560, -187, 150), FVector(220, 14, 140), PlasticDarkMaterial);
	int32 CigaretteIndex = 0;
	for (const float RackZ : {126.0f, 154.0f, 182.0f})
	{
		for (float RackX = 2470.0f; RackX <= 2650.0f; RackX += 24.0f)
		{
			UMaterialInterface* RackMaterial =
				(CigaretteIndex % 3 == 0) ? SnackRedMaterial :
				(CigaretteIndex % 3 == 1) ? SnackYellowMaterial : SnackBlueMaterial;
			AddStoreStockBlock(
				FVector(RackX, -198, RackZ),
				FVector(14, 8, 12),
				RackMaterial,
				false);
			++CigaretteIndex;
		}
	}

	// Two double-sided gondolas built like real shop fixtures: a central back
	// panel, a kick base, and cantilevered shelf tiers on each face. Product
	// stands *on* a tier inside the bay instead of perching on the top cap.
	// The 174 cm run is a common chest-height fixture: the 120 cm product tier
	// has a real 150 cm shelf above it, so cup ramyeon reads as shelved stock
	// from a standing player's approach instead of an item on a display table.
	for (const float GondolaY : {-365.0f, -555.0f})
	{
		// Spine and structure.
		CreateBlock(FVector(2640, GondolaY, 86), FVector(300, 8, 172), ShelfSteel);
		CreateBlock(FVector(2640, GondolaY, 8), FVector(292, 46, 16), PlasticDarkMaterial);
		CreateBlock(FVector(2488, GondolaY, 87), FVector(6, 50, 174), Metal);
		CreateBlock(FVector(2792, GondolaY, 87), FVector(6, 50, 174), Metal);
		CreateBlock(FVector(2640, GondolaY, 174), FVector(304, 50, 4), Metal, false);

		const float TierHeights[] = {30.0f, 60.0f, 90.0f, 120.0f, 150.0f};
		const TCHAR* SnackLabels[] = {
			TEXT("M_SnackShrimp"), TEXT("M_SnackPotato"),
			TEXT("M_SnackSquid"), TEXT("M_SnackCorn")};

		for (const float FaceSign : {-1.0f, 1.0f})
		{
			int32 SnackIndex = static_cast<int32>(GondolaY * 0.1f + FaceSign);
			for (int32 TierIndex = 0; TierIndex < UE_ARRAY_COUNT(TierHeights); ++TierIndex)
			{
				const float TierZ = TierHeights[TierIndex];
				const float ShelfY = GondolaY + FaceSign * 13.0f;
				// Shelf plate, its raised front lip and the price rail.
				CreateBlock(FVector(2640, ShelfY, TierZ), FVector(298, 22, 3), Metal);
				CreateBlock(FVector(2640, GondolaY + FaceSign * 23.5f, TierZ + 3.5f),
					FVector(298, 2, 5), Metal, false);
				CreateBlock(FVector(2640, GondolaY + FaceSign * 24.5f, TierZ + 2.0f),
					FVector(296, 2, 7), FridgeInteriorMaterial, false);

				// The CH01 approach side of the first gondola reserves the bay
				// between the 120 cm and 150 cm shelves for cup ramyeon. The upper
				// shelf remains visible in profile and makes the storage relationship
				// unambiguous from the aisle.
				const bool bRamyeonBay =
					FMath::IsNearlyEqual(GondolaY, -365.0f)
					&& FaceSign < 0.0f
					&& TierIndex == 3;

				// Bags stand on the tier, backs to the spine, faces to the
				// aisle — packed shoulder to shoulder the way a stocked shelf
				// actually looks, not spaced out like a museum case.
				for (float SnackX = 2500.0f;
					!bRamyeonBay && SnackX <= 2780.0f;
					SnackX += 14.0f)
				{
					UMaterialInterface* SnackMaterial = TexMat(
						SnackLabels[FMath::Abs(SnackIndex) % 4],
						(SnackIndex % 3 == 0) ? SnackRedMaterial :
						(SnackIndex % 3 == 1) ? SnackYellowMaterial : SnackBlueMaterial);
					if (!AddStoreStockProp(
						TEXT("SM_SnackBag"),
						// The bag mesh carries a crimped bottom seal, so its
						// pivot sits slightly below the body.
						FVector(SnackX, GondolaY + FaceSign * 11.0f, TierZ + 6.8f),
						SnackMaterial,
						FaceSign > 0.0f ? 90.0f : -90.0f,
						0.48f,
						true))
					{
						AddStoreStockBlock(
							FVector(SnackX, GondolaY + FaceSign * 11.0f, TierZ + 9.5f),
							FVector(15, 12, 16), SnackMaterial, true);
					}
					++SnackIndex;
				}
			}
		}
	}

	// Two dense rows of cups occupy the enclosed 120--150 cm bay on the
	// player-facing side. The 11 cm cups rest on the 121.5 cm shelf surface and
	// leave 17.4 cm below the next shelf. The staggered back row reads as stocked
	// depth without intersecting either the spine or price rail.
	constexpr float RamyeonShelfSurfaceZ = 121.5f;
	for (int32 RowIndex = 0; RowIndex < 2; ++RowIndex)
	{
		const float RowY = RowIndex == 0 ? -379.0f : -368.7f;
		const float StartX = RowIndex == 0 ? 2504.0f : 2512.0f;
		const float EndX = RowIndex == 0 ? 2776.0f : 2768.0f;
		for (float CupX = StartX; CupX <= EndX; CupX += 16.0f)
		{
			// A cup ramyeon is three materials, not one. The foam cup, the
			// printed band and foil lid remain aligned as one retail unit.
			AddStoreStockCup(
				FVector(CupX, RowY, RamyeonShelfSurfaceZ),
				-90.0f);
		}
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
				? AddStoreStockProp(
					TEXT("SM_KimbapPack"),
					FVector(ItemX, -655, TierZ),
					FridgeInteriorMaterial,
					90.0f)
				: AddStoreStockProp(
					TEXT("SM_SandwichPack"),
					FVector(ItemX, -655, TierZ),
					SnackYellowMaterial,
					90.0f);
			if (!bPlaced)
			{
				AddStoreStockBlock(
					FVector(ItemX, -655, TierZ + (bKimbap ? 4.0f : 3.0f)),
					bKimbap ? FVector(9, 8, 8) : FVector(13, 9, 6),
					bKimbap ? FridgeInteriorMaterial : SnackYellowMaterial,
					true);
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
	// Ramyeon corner rack by the window bar. A thin back, two uprights and five
	// shelf plates leave genuine open bays; the former solid cabinet with cups
	// perched at Z=167.5 was not a plausible convenience-store fixture.
	// The whole fixture stands against the wall face at Y -680 instead of
	// through it: the back panel was buried in the wall and the 32 cm tiers
	// ran six centimetres into it, so the rear of every cup was inside the
	// building's south wall.
	CreateBlock(FVector(2452, -678.5f, 86), FVector(70, 3, 160), ShelfSteel);
	CreateBlock(FVector(2418.5f, -664, 86), FVector(3, 32, 160), Metal);
	CreateBlock(FVector(2485.5f, -664, 86), FVector(3, 32, 160), Metal);
	for (const float TierZ : {16.0f, 46.0f, 76.0f, 106.0f, 136.0f, 166.0f})
	{
		CreateBlock(FVector(2452, -664, TierZ), FVector(70, 32, 3), Metal);
		if (TierZ >= 166.0f)
		{
			continue;
		}
		for (float CupX = 2427.0f; CupX <= 2477.0f; CupX += 12.5f)
		{
			AddStoreStockCup(
				FVector(CupX, -664, TierZ + 1.5f),
				90.0f);
		}
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
		// Bay light strip, screwed up under the header of the cooler bank
		// rather than hovering in the middle of the bay's air.
		CreateBlock(FVector(2905, BayY, 214.5f), FVector(3, 60, 3), ScreenGlowMaterial, false);
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
				// Korean cooler shelves mix PET, glass, slim cans and cartons. The
				// earlier three-bottle loop made every bay read like a liquor wall.
				const int32 ProductVariant = FMath::Abs(DrinkIndex) % 5;
				const bool bTallBottle = ProductVariant == 2;
				const bool bDrinkCan = ProductVariant == 3;
				const bool bDrinkCarton = ProductVariant == 4;
				UMaterialInterface* DrinkMaterial = ProductVariant == 0
					? BottleGreenMaterial
					: BottleBrownMaterial;
				UMaterialInterface* CapMaterial =
					(DrinkIndex % 2 == 0) ? SnackRedMaterial : FridgeInteriorMaterial;
				const FVector BottleBase(DrinkRowX, DrinkY, ShelfTopZ);
				// Bottles face the aisle, so every label reads from the front.
				const float BottleYaw = -90.0f + (DrinkIndex % 3 - 1) * 7.0f;
				if (bDrinkCan)
				{
					const bool bPlaced = AddStoreStockProp(
						TEXT("SM_DrinkCan"),
						BottleBase,
						TexMat(TEXT("M_LabelSoda"), SnackBlueMaterial),
						BottleYaw,
						1.0f,
						true);
					ensureMsgf(
						bPlaced,
						TEXT("SM_DrinkCan is required for Korean cooler silhouette variety."));
				}
				else if (bDrinkCarton)
				{
					const bool bPlaced = AddStoreStockProp(
						TEXT("SM_MilkCarton"),
						BottleBase,
						TexMat(TEXT("M_LabelBarley"), FridgeInteriorMaterial),
						BottleYaw,
						1.0f,
						true);
					ensureMsgf(
						bPlaced,
						TEXT("SM_MilkCarton is required for Korean cooler silhouette variety."));
				}
				else if (bTallBottle)
				{
					if (!AddStoreStockProp(
							TEXT("SM_SojuBottle"),
							BottleBase,
							BottleGreenMaterial,
							BottleYaw))
					{
						AddStoreStockBlock(
							BottleBase + FVector(0.0f, 0.0f, 10.6f),
							FVector(6.7f, 6.7f, 21.2f),
							BottleGreenMaterial,
							true);
					}
					if (!AddStoreStockProp(
							TEXT("SM_BottleCap"),
							BottleBase + FVector(0, 0, 21.0f),
							CapMaterial,
							0.0f,
							1.0f,
							false))
					{
						AddStoreStockBlock(
							BottleBase + FVector(0.0f, 0.0f, 21.8f),
							FVector(3.4f, 3.4f, 1.6f),
							CapMaterial,
							false);
					}
					AddStoreStockBottleLabel(BottleBase, 3.42f, 3.0f, 7.0f,
						TEXT("M_LabelSoju"), BottleYaw);
				}
				else
				{
					if (!AddStoreStockProp(
							TEXT("SM_DrinkBottle"),
							BottleBase,
							DrinkMaterial,
							BottleYaw))
					{
						AddStoreStockBlock(
							BottleBase + FVector(0.0f, 0.0f, 8.55f),
							FVector(7.2f, 7.2f, 17.1f),
							DrinkMaterial,
							true);
					}
					if (!AddStoreStockProp(
							TEXT("SM_BottleCap"),
							BottleBase + FVector(0, 0, 16.9f),
							CapMaterial,
							0.0f,
							1.0f,
							false))
					{
						AddStoreStockBlock(
							BottleBase + FVector(0.0f, 0.0f, 17.7f),
							FVector(3.4f, 3.4f, 1.6f),
							CapMaterial,
							false);
					}
					const TCHAR* LabelName = ProductVariant == 0
						? TEXT("M_LabelGreenTea")
						: TEXT("M_LabelSoda");
					AddStoreStockBottleLabel(
						BottleBase, 3.67f, 3.0f, 7.6f, LabelName, BottleYaw);
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
			// The handle is fixed to the door leaf; at X 2894 it floated a
			// centimetre and a half in front of the glass.
			CreateBlock(FVector(2895.5f, BayY + 28.0f, 110), FVector(3, 4, 44), Metal, false);
		}
	}
	StoreLights.Add(CreateLight(
		FVector(2890, -430, 190), 320.0f, 480.0f, FLinearColor(0.75f, 0.85f, 1.0f), false, 8.0f));

	// Ice-cream chest freezer against the west glass, south of the door.
	CreateBlock(FVector(2445, -545, 46), FVector(58, 110, 80), FridgeBodyMaterial);
	CreateBlock(FVector(2445, -545, 88), FVector(52, 104, 4), GlassMaterial, false);
	CreateBlock(FVector(2445, -545, 84), FVector(56, 108, 3), Metal, false);

	// Tobacco notice over the cigarette wall, entrance mat, CCTV eye. The
	// notice is stuck to the wall itself, so it has to reach Y -180.
	CreateBlock(
		FVector(2560, -181, 228), FVector(140, 2, 12),
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

	FinalizeStoreStockBatches();
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

		// A standard 76 x 76 mm adhesive memo. Its top strip stays flush and the
		// lower edge releases by only 1.8 mm, so the contact shadow reads as paper.
		UStaticMesh* StickyNoteMesh = PropMesh(TEXT("SM_StickyNote76mm"));
		const bool bHasAuthoredStickyNote = StickyNoteMesh != nullptr;
		UStaticMeshComponent* StickyNote = CreateDecoOnComponent(
			Fridge->GetDoorPivot(),
			bHasAuthoredStickyNote ? StickyNoteMesh : PlaneMesh.Get(),
			TexMat(TEXT("M_NoteFridge"), SignWhiteMaterial),
			FVector(-7.43f, 36.0f, 18.0f),
			bHasAuthoredStickyNote ? FRotator::ZeroRotator : FRotator(-90.0f, 0.0f, 0.0f),
			bHasAuthoredStickyNote ? FVector::OneVector : FVector(0.076f));
		if (StickyNote)
		{
			StickyNote->SetCullDistance(650.0f);
			StickyNote->SetAffectDistanceFieldLighting(false);
		}
	}

	// This morning's paper on the landing outside 403. The delivery-ad sticker
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
			TexMat(TEXT("M_UnitDoorPaintedSteel"), DoorMaterial),
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
				NSLOCTEXT("IGPrologue", "NoticeL5", "   대상 :  전 세대 (401호 ~ 403호)"),
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
		// Brushed, mid-roughness stainless retains panel direction and contact
		// shading without becoming a mirror of the exterior Lumen scene.
		CabVisuals.StainlessMaterial =
			TexMat(TEXT("M_StainlessUV"), FridgeBodyMaterial);
		// The apartment-door navy read as an open patch of sky at the end of the
		// corridor. Neutral lift enamel plus the physical 1.4 cm centre seam keeps
		// both landing leaves unmistakably closed before the call completes.
		CabVisuals.DoorMaterial =
			TexMat(TEXT("M_SteelDoorUV"), FridgeBodyMaterial);
		// The story specifies no readable mirror in the safe CH01 car. A
		// brushed rear panel also prevents the outdoor facade/sky reflection
		// from appearing inside a closed elevator during the hidden transfer.
		CabVisuals.MirrorMaterial =
			TexMat(TEXT("M_StainlessUV"), FridgeBodyMaterial);
		CabVisuals.FloorMaterial = TexMat(TEXT("M_MarbleFloor_XY"), StoreFloorMaterial);
		CabVisuals.InlayMaterial = PlasticDarkMaterial;
		CabVisuals.CopMaterial = TexMat(TEXT("M_LiftCOP"), ScreenGlowMaterial);
		CabVisuals.HallMaterial = TexMat(TEXT("M_LiftHall"), ScreenGlowMaterial);
		// The point light below the fixture is the physical emitter. Reusing the
		// store's emissive panel here double-lit the small cab and clipped both
		// diffusers to featureless white after exposure adapted to the dark hall.
		// A matte white lens keeps the panel shape readable without baked light.
		CabVisuals.DiffuserMaterial = SignWhiteMaterial;
		Elevator->ConfigurePrototypeVisuals(CabVisuals, 900.0f);
		Elevator->OnReturnedToFourthFloor.AddUniqueDynamic(
			this,
			&ThisClass::HandleElevatorReturnedToFourthFloor);

		// BuildCabInterior owns the sole rider COP. Adding a camera-facing copy
		// here used to bury buttons inside the opposite handrail.
	}

	// Wallet on the desk.
	Wallet = World->SpawnActor<AIGPickupItem>(
		AIGPickupItem::StaticClass(),
		FTransform(FRotator(0, 20, 0), WalletWorldLocation),
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
			// A 55 mm commercial wrap band sits on the bottle's straight waist.
			// Keeping it clear of the lower grip ribs prevents floating film and
			// the stretched, torn-looking print of the old 86 mm sleeve.
			CreateDecoOnComponent(
				WaterBottle->GetMeshComponent(),
				PropMesh(TEXT("SM_LabelSleeve"), CylinderMesh),
				TexMat(TEXT("M_LabelWater"), WaterBlueMaterial),
				FVector(0, 0, 7.2f), FRotator(0.0f, -90.0f, 0.0f),
				FVector(3.30f, 3.30f, 5.5f));
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
					FVector(0, 0, 7.2f),
					FRotator(0.0f, -90.0f, 0.0f),
					FVector(3.30f, 3.30f, 5.5f));
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
		StoreCashRegisterVisual = PlacePhotoProp(
			TEXT("CashRegister_01"),
			FVector(2620, -253, 99), FVector(48, 44, 40), 180.0f, false);
		if (StoreCashRegisterVisual)
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
		FVector(346, 44, IGPrologueWorld::FourthFloorZ + 61.0f),
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
			NSLOCTEXT("IGCH02", "MailboxL4", "403호 연락 불가 · 가족 연락처 확인 요청"),
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
		FTransform(FRotator(0, 20, 0), WalletWorldLocation),
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
			FVector(0, 0, 7.2f), FRotator(0.0f, -90.0f, 0.0f),
			FVector(3.30f, 3.30f, 5.5f));
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
				FVector(0, 0, 7.2f),
				FRotator(0.0f, -90.0f, 0.0f),
				FVector(3.30f, 3.30f, 5.5f));
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

void AIGPrologueWorldScene::RefreshChapterTwoCatWaterAftermath()
{
	if (!GetWorld())
	{
		return;
	}
	for (TActorIterator<AIGChapterOneIncidentAction> It(GetWorld()); It; ++It)
	{
		AIGChapterOneIncidentAction* Existing = *It;
		if (Existing
			&& Existing->Tags.Contains(IGPrologueWorld::CatWaterAftermathTag)
			&& !Existing->IsActorBeingDestroyed())
		{
			Existing->Destroy();
		}
	}
	ChapterTwoCatWaterAftermath = nullptr;

	const UIGRebirthNarrativeSubsystem* RebirthState = GetGameInstance()
		? GetGameInstance()->GetSubsystem<UIGRebirthNarrativeSubsystem>()
		: nullptr;
	if (!RebirthState)
	{
		return;
	}
	const FIGRebirthChoiceState Choices = RebirthState->GetChoices();
	FVector LocalLocation = FVector::ZeroVector;
	FVector Size = FVector::ZeroVector;
	UStaticMesh* Mesh = nullptr;
	UMaterialInterface* Material = nullptr;
	FName PresentationTag;
	if (Choices.CatWaterState == EIGRebirthCatWaterState::BottleCap)
	{
		if (Choices.bWaitedForCat)
		{
			LocalLocation = FVector(1198.0f, -438.0f, 8.0f);
			Size = FVector(18.0f, 18.0f, 0.8f);
			Mesh = CylinderMesh;
			Material = WaterBlueMaterial;
			PresentationTag = IGPrologueWorld::CatWaterWetRingTag;
		}
		else
		{
			LocalLocation = FVector(1230.0f, -438.0f, 9.0f);
			Size = FVector(8.0f, 8.0f, 3.0f);
			Mesh = CylinderMesh;
			Material = PlasticDarkMaterial;
			PresentationTag = IGPrologueWorld::CatWaterCapTag;
		}
	}
	else if (Choices.CatWaterState == EIGRebirthCatWaterState::PaperCup)
	{
		LocalLocation = FVector(1255.0f, -438.0f, 10.0f);
		Size = FVector(10.0f, 10.0f, 13.0f);
		Mesh = CylinderMesh;
		Material = SignWhiteMaterial;
		PresentationTag = IGPrologueWorld::CatWaterCupTag;
	}
	else
	{
		return;
	}

	const FTransform SpawnTransform(
		GetActorTransform().TransformRotation(FQuat::Identity),
		GetActorTransform().TransformPosition(LocalLocation));
	ChapterTwoCatWaterAftermath =
		GetWorld()->SpawnActorDeferred<AIGChapterOneIncidentAction>(
			AIGChapterOneIncidentAction::StaticClass(),
			SpawnTransform,
			this,
			nullptr,
			ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (!ChapterTwoCatWaterAftermath)
	{
		return;
	}
	ChapterTwoCatWaterAftermath->Configure(
		nullptr,
		EIGChapterOneIncidentAction::None,
		Mesh,
		Material,
		Size,
		FText::GetEmpty());
	ChapterTwoCatWaterAftermath->Tags.AddUnique(
		IGPrologueWorld::CatWaterAftermathTag);
	ChapterTwoCatWaterAftermath->Tags.AddUnique(PresentationTag);
	ChapterTwoCatWaterAftermath->FinishSpawning(SpawnTransform);
	ChapterTwoCatWaterAftermath->SetActorHiddenInGame(false);
	ChapterTwoCatWaterAftermath->SetActorEnableCollision(false);
	ChapterTwoCatWaterAftermath->SetInteractionEnabled(false);
}

bool AIGPrologueWorldScene::ValidateChapterTwoCatWaterAftermath() const
{
	const UIGRebirthNarrativeSubsystem* RebirthState = GetGameInstance()
		? GetGameInstance()->GetSubsystem<UIGRebirthNarrativeSubsystem>()
		: nullptr;
	if (!RebirthState || !GetWorld())
	{
		return false;
	}
	const FIGRebirthChoiceState Choices = RebirthState->GetChoices();
	FName ExpectedTag;
	FVector ExpectedLocalLocation = FVector::ZeroVector;
	if (Choices.CatWaterState == EIGRebirthCatWaterState::BottleCap)
	{
		ExpectedTag = Choices.bWaitedForCat
			? IGPrologueWorld::CatWaterWetRingTag
			: IGPrologueWorld::CatWaterCapTag;
		ExpectedLocalLocation = Choices.bWaitedForCat
			? FVector(1198.0f, -438.0f, 8.0f)
			: FVector(1230.0f, -438.0f, 9.0f);
	}
	else if (Choices.CatWaterState == EIGRebirthCatWaterState::PaperCup)
	{
		ExpectedTag = IGPrologueWorld::CatWaterCupTag;
		ExpectedLocalLocation = FVector(1255.0f, -438.0f, 10.0f);
	}

	int32 LiveAftermathCount = 0;
	for (TActorIterator<AIGChapterOneIncidentAction> It(GetWorld()); It; ++It)
	{
		const AIGChapterOneIncidentAction* Existing = *It;
		LiveAftermathCount += Existing
			&& Existing->Tags.Contains(IGPrologueWorld::CatWaterAftermathTag)
			&& !Existing->IsActorBeingDestroyed()
			? 1
			: 0;
	}
	if (ExpectedTag.IsNone())
	{
		return LiveAftermathCount == 0
			&& !IsValid(ChapterTwoCatWaterAftermath);
	}
	const FVector ExpectedWorldLocation =
		GetActorTransform().TransformPosition(ExpectedLocalLocation);
	return LiveAftermathCount == 1
		&& IsValid(ChapterTwoCatWaterAftermath)
		&& ChapterTwoCatWaterAftermath->Tags.Contains(ExpectedTag)
		&& !ChapterTwoCatWaterAftermath->IsHidden()
		&& !ChapterTwoCatWaterAftermath->GetActorEnableCollision()
		&& !ChapterTwoCatWaterAftermath->IsInteractionEnabled()
		&& FVector::DistSquared(
			ChapterTwoCatWaterAftermath->GetActorLocation(),
			ExpectedWorldLocation) <= 1.0f;
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

	const FTransform AlarmTransform(FRotator::ZeroRotator, AlarmWorldLocation);
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

	int32 StoreStockBatchCount = 0;
	int32 StoreStockInstanceCount = 0;
	if (!ValidateStoreStockBatches(
			StoreStockBatchCount,
			StoreStockInstanceCount))
	{
		UE_LOG(
			LogIndieGame,
			Error,
			TEXT(
				"REBIRTH_RELEASE FAIL store_instancing instances=%d expected=%d "
				"batches=%d maximum=%d"),
			StoreStockInstanceCount,
			IGPrologueWorld::ExpectedStoreStockInstances,
			StoreStockBatchCount,
			IGPrologueWorld::MaximumStoreStockBatches);
		FailRebirthEndToEndValidation(TEXT("store stock batching contract"));
		return;
	}
	UE_LOG(
		LogIndieGame,
		Display,
		TEXT(
			"REBIRTH_RELEASE PASS store_instancing instances=%d batches=%d "
			"cull_cm=%d-%d"),
		StoreStockInstanceCount,
		StoreStockBatchCount,
		IGPrologueWorld::StoreStockCullStartCentimeters,
		IGPrologueWorld::StoreStockCullEndCentimeters);

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
	FString EndingValue;
	const bool bEndingB =
		FParse::Value(
			FCommandLine::Get(),
			TEXT("IGRebirthEnding="),
			EndingValue)
		&& EndingValue.Equals(TEXT("B"), ESearchCase::IgnoreCase);
	FString RouteValue;
	if (!FParse::Value(
			FCommandLine::Get(),
			TEXT("IGRebirthCH02Route="),
			RouteValue))
	{
		RouteValue = bEndingB ? TEXT("P2ThenP1") : TEXT("P1ThenP2");
	}
	const bool bP2First =
		RouteValue.Equals(TEXT("P2ThenP1"), ESearchCase::IgnoreCase);
	const bool bSkipP1 =
		RouteValue.Equals(TEXT("SkipP1"), ESearchCase::IgnoreCase)
		|| RouteValue.Equals(TEXT("SkipBoth"), ESearchCase::IgnoreCase);
	const bool bSkipP2 =
		RouteValue.Equals(TEXT("SkipP2"), ESearchCase::IgnoreCase)
		|| RouteValue.Equals(TEXT("SkipBoth"), ESearchCase::IgnoreCase);
	const bool bKnownRoute = bP2First
		|| RouteValue.Equals(TEXT("P1ThenP2"), ESearchCase::IgnoreCase)
		|| bSkipP1
		|| bSkipP2;
	if (!bKnownRoute || (bP2First && (bSkipP1 || bSkipP2)))
	{
		FailRebirthEndToEndValidation(TEXT("unknown CH02 validation route"));
		return;
	}
	if (!bRebirthEndToEndValidation
		|| !SecondMorningDirector
		|| !SecondMorningDirector->RunRebirthEndToEndRoute(
			bP2First,
			bSkipP1,
			bSkipP2))
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
	const FIGRebirthNarrativeSnapshot Snapshot = RebirthState
		? RebirthState->BuildSnapshot()
		: FIGRebirthNarrativeSnapshot();
	const bool bP1Resolved =
		Snapshot.ResolvedPuzzles.Contains(FName(TEXT("P1")));
	const bool bP2Resolved =
		Snapshot.ResolvedPuzzles.Contains(FName(TEXT("P2")));
	const int32 TimeEntryPhysicalCount =
		SecondMorningDirector->GetTimeEntryPhysicalContractCount();
	const int32 TimeEntryMeshComponentCount =
		SecondMorningDirector->GetTimeEntryMeshComponentCount();
	const int32 AuthoredHousingCount =
		SecondMorningDirector->GetAuthoredTimeEntryHousingCount();
	const int32 LayeredDisplayCount =
		SecondMorningDirector->GetLayeredTimeEntryDisplayCount();
	int32 TruthCount = 0;
	for (const TCHAR* TruthName : {
		TEXT("Truth.Alarm0510"),
		TEXT("Truth.DeathOverlay"),
		TEXT("Truth.WasSearched")})
	{
		TruthCount += RebirthState
			&& RebirthState->HasTruth(FGameplayTag::RequestGameplayTag(
				FName(TruthName),
				false))
			? 1
			: 0;
	}
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
		&& ValidateChapterTwoCatWaterAftermath()
		&& TimeEntryPhysicalCount == 2
		&& TimeEntryMeshComponentCount == 23
		&& AuthoredHousingCount == 2
		&& LayeredDisplayCount == 2
		&& bP1Resolved == !bSkipP1
		&& bP2Resolved == !bSkipP2
		&& TruthCount == (bSkipP1 || bSkipP2 ? 2 : 3)
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
			"REBIRTH_E2E PASS ch02_router p1=%d p2=%d truths=%d searched=1 "
			"outfit_once=1 human_gate=1 c3=1 purchase_preserved=1 "
			"item_continuity=1 cat_aftermath=1 authored_housings=%d "
			"layered_displays=%d "
			"time_entry_physical=%d "
			"time_entry_mesh_components=%d "
			"pressure_caps=%d "
			"route_order=%s"),
		bP1Resolved ? 1 : 0,
		bP2Resolved ? 1 : 0,
		TruthCount,
		AuthoredHousingCount,
		LayeredDisplayCount,
		TimeEntryPhysicalCount,
		TimeEntryMeshComponentCount,
		(bSkipP1 ? 0 : 1) + (bSkipP2 ? 0 : 1),
		bSkipP1 && bSkipP2
			? TEXT("skip_both")
			: bSkipP1
				? TEXT("skip_p1")
				: bSkipP2
					? TEXT("skip_p2")
					: bP2First ? TEXT("p2_p1") : TEXT("p1_p2"));
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
	RefreshChapterTwoCatWaterAftermath();
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
				? FVector(2515.0f, -455.0f, 104.0f)
				: bResumeAtWoke
					? IGPrologueWorld::PlayerLocation
					: FVector(430.0f, -305.0f, 998.0f))
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
		// P2 owns the final register presentation. Retire the CH01 dressing scan
		// before spawning it so two copies cannot overlap and z-fight.
		if (StoreCashRegisterVisual)
		{
			StoreCashRegisterVisual->SetVisibility(false, true);
			StoreCashRegisterVisual->SetHiddenInGame(true, true);
		}
		UStaticMesh* PosHousingMesh = nullptr;
		if (StoreCashRegisterVisual)
		{
			PosHousingMesh = StoreCashRegisterVisual->GetStaticMesh();
		}
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
			ManagementNotice,
			CubeMesh,
			PropMesh(TEXT("SM_AlarmClock")),
			PosHousingMesh,
			PlasticDarkMaterial,
			PlasticDarkMaterial,
			GlassMaterial,
			AlarmMaterial,
			ScreenGlowMaterial,
			SnackRedMaterial);
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
			true);
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
	if (ChapterTwoCatWaterAftermath)
	{
		ChapterTwoCatWaterAftermath->Destroy();
		ChapterTwoCatWaterAftermath = nullptr;
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
		FParse::Param(FCommandLine::Get(), TEXT("IGCaptureCH03"))
		|| FParse::Param(
			FCommandLine::Get(),
			TEXT("IGCaptureCH03LensDroplet"));
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
		if (FParse::Param(
			FCommandLine::Get(),
			TEXT("IGRebirthCH02FreedomProbe")))
		{
			FString RouteValue;
			FParse::Value(
				FCommandLine::Get(),
				TEXT("IGRebirthCH02Route="),
				RouteValue);
			UE_LOG(
				LogIndieGame,
				Display,
				TEXT("REBIRTH_CH02_FREEDOM PASS route=%s handoff=1"),
				*RouteValue);
			FPlatformMisc::RequestExitWithStatus(
				false,
				0,
				TEXT("REBIRTH CH02 freedom probe passed"));
		}
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
			MirrorRoomLamp->SetIntensity(390.0f);
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
			OfferingLight->SetIntensity(175.0f);
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
		FPlatformMisc::RequestExitWithStatus(
			false,
			1,
			TEXT("CH02 capture validation could not start"));
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
			FPlatformMisc::RequestExitWithStatus(
				false,
				1,
				TEXT("CH02 capture elevator descent failed"));
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
			FPlatformMisc::RequestExitWithStatus(
				false,
				bReturned ? 0 : 1,
				bReturned
					? TEXT("CH02 capture validation completed")
					: TEXT("CH02 capture elevator return failed"));
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
