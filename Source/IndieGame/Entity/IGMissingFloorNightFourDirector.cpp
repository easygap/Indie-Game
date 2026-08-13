#include "Entity/IGMissingFloorNightFourDirector.h"

#include "Audio/IGAudioHelpers.h"
#include "Audio/IGMissingFloorAudioSubsystem.h"
#include "Audio/IGToneSequenceSoundWave.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/AudioComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Core/IGPrologueWorldScene.h"
#include "Engine/GameInstance.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Entity/IGListenerEntity.h"
#include "Entity/IGMissingFloorEvidence.h"
#include "Entity/IGNoiseSubsystem.h"
#include "Entity/IGNightLoopDirector.h"
#include "Entity/IGNightPhaseDirector.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Math/RotationMatrix.h"
#include "Materials/MaterialInterface.h"
#include "Narrative/IGMissingFloorNarrativeSubsystem.h"
#include "Player/IGFlashlightComponent.h"
#include "Player/IGHorrorHUD.h"
#include "Player/IGPlayerCharacter.h"
#include "Player/IGStressComponent.h"

namespace IGNightFour
{
	const FName CleaningDrainId(TEXT("P5.RoofCleaningDrain"));
	const FName FloatBypassId(TEXT("P5.RoofFloatBypass"));
	const FName TransferPumpId(TEXT("P5.TransferPump"));
	const FName PuzzleId(TEXT("P5"));
	const FName EndingAId(TEXT("Ending.A"));
	const FName EndingBId(TEXT("Ending.B"));
	const FName EndingCId(TEXT("Ending.C"));
	const FName PowerCutBeat(TEXT("Night4.PowerCut"));
	const FName FinalRevealBeat(TEXT("Night4.FinalReveal"));
	const FName FinalConfrontationBeat(TEXT("Night4.FinalConfrontation"));
	constexpr float FailureCaptureSeconds = 1.2f;
	constexpr float FailureListingDelaySeconds = 1.24f;
	constexpr float FailureRetryDelaySeconds = 7.2f;

	// Posted beside the existing fourth-floor lift notice, not on top of the
	// 403 shipping labels. Its back face shares the established paper plane.
	const FVector EvictionNoticeLocation(640.0f, -232.0f, 1042.0f);
	// Both roof controls face the 1.2 m maintenance lane beside the tank.
	const FVector CleaningDrainLocation(-62.0f, 158.0f, 1340.0f);
	const FVector FloatBypassLocation(72.0f, 158.0f, 1340.0f);
	// Ground-floor transfer-pump selector, inside the management booth.
	// A wall-mounted selector above a floor-seated pump assembly in the booth.
	const FVector TransferPumpLocation(63.0f, -170.0f, 112.0f);
	const FVector WallBreakLocation(246.0f, 700.0f, 1300.0f);
	const FVector EndingALocation(223.0f, 665.0f, 1220.0f);
	const FVector EndingBLocation(165.0f, 765.0f, 1220.0f);
	const FVector CavityVisualOrigin(276.0f, 700.0f, 1200.0f);
	const FVector MokStartLocation(130.0f, 470.0f, 1195.0f);
	const FVector MokExitLocation(130.0f, 865.0f, 1195.0f);
	const FRotator MokRotation(0.0f, -90.0f, 0.0f);
	const FVector EndingHammerStart(218.0f, 660.0f, 1205.0f);
	const FVector EndingHammerRest(263.0f, 686.0f, 1276.0f);
	const FVector EndingPhoneRest(205.0f, 753.0f, 1204.0f);
	const FVector CavityDetailCenter(244.0f, 700.0f, 1290.0f);
	const FVector MokDetailOffset(0.0f, 16.0f, 113.0f);
	const FVector CavityDetailNormal(-1.0f, 0.0f, 0.0f);
	const FVector MokDetailNormal(0.0f, 1.0f, 0.0f);

	static FRotator DetailCardRotation(
		const FVector& ScreenRight,
		const FVector& SurfaceNormal)
	{
		// Engine Plane uses local X/Y as image axes and local Z as its normal.
		// Supplying screen-right as X makes the computed Y point downward, so
		// the source bitmap remains upright without a negative component scale.
		return FRotationMatrix::MakeFromXZ(
			ScreenRight,
			SurfaceNormal).Rotator();
	}

	static const FVector& RevealTarget(const int32 Stage)
	{
		static const FVector Targets[] = {
			CavityVisualOrigin + FVector(-13.0f, 0.0f, 161.0f),
			CavityVisualOrigin + FVector(-14.0f, 0.0f, 118.0f),
			CavityVisualOrigin + FVector(-10.0f, 35.0f, 18.0f),
		};
		return Targets[FMath::Clamp(Stage, 0, 2)];
	}

	static float RequiredAttentionSeconds(const int32 Stage)
	{
		static constexpr float Seconds[] = {1.8f, 2.0f, 2.2f};
		return Seconds[FMath::Clamp(Stage, 0, 2)];
	}

	static const TArray<FName>& SafeOrder()
	{
		static const TArray<FName> Order = {
			CleaningDrainId,
			FloatBypassId,
			TransferPumpId,
		};
		return Order;
	}
}

AIGMissingFloorNightFourDirector::AIGMissingFloorNightFourDirector()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;
}

void AIGMissingFloorNightFourDirector::Tick(const float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (bFinalRevealActive)
	{
		UpdateCavityReveal(DeltaSeconds);
	}
	if (bMokRetreatActive)
	{
		UpdateMokRetreat(DeltaSeconds);
	}
	if (bEndingHammerMoving)
	{
		UpdateEndingHammer(DeltaSeconds);
	}
	UpdateFinaleDetailLayers();
	if (!bFinalRevealActive && !bMokRetreatActive && !bEndingHammerMoving)
	{
		SetActorTickEnabled(
			bCavityPresentationVisible || bMokPresentationVisible);
	}
}

bool AIGMissingFloorNightFourDirector::Configure(AIGPrologueWorldScene* InScene)
{
	UWorld* World = GetWorld();
	if (!World || !InScene)
	{
		return false;
	}
	Scene = InScene;
	for (TActorIterator<AIGListenerEntity> It(World); It; ++It)
	{
		Listener = *It;
		Listener->OnPlayerCaptured.AddUObject(
			this, &AIGMissingFloorNightFourDirector::HandleNightFourCapture);
		break;
	}

	UStaticMesh* CubeMesh =
		LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
	UStaticMesh* CylinderMesh =
		LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	if (!CubeMesh || !CylinderMesh)
	{
		return false;
	}

	UStaticMesh* LargeValveMesh = LoadObject<UStaticMesh>(
		nullptr, TEXT("/Game/Meshes/SM_P3ValveWheelLarge.SM_P3ValveWheelLarge"));
	UStaticMesh* SmallValveMesh = LoadObject<UStaticMesh>(
		nullptr, TEXT("/Game/Meshes/SM_P3ValveWheelSmall.SM_P3ValveWheelSmall"));
	UMaterialInterface* MetalMaterial = LoadObject<UMaterialInterface>(
		nullptr,
		TEXT("/Game/Prototype/Materials/M_P3CabinetMetalUV.M_P3CabinetMetalUV"));
	UMaterialInterface* PaperMaterial = LoadObject<UMaterialInterface>(
		nullptr, TEXT("/Game/Prototype/Materials/M_PaperClean.M_PaperClean"));
	UMaterialInterface* DarkMaterial = LoadObject<UMaterialInterface>(
		nullptr, TEXT("/Game/Prototype/Materials/M_PlasticDark.M_PlasticDark"));
	UMaterialInterface* RedMaterial = LoadObject<UMaterialInterface>(
		nullptr, TEXT("/Game/Prototype/Materials/M_SnackRed.M_SnackRed"));
	UMaterialInterface* ScreenMaterial = LoadObject<UMaterialInterface>(
		nullptr, TEXT("/Game/Prototype/Materials/M_ScreenGlow.M_ScreenGlow"));

	FActorSpawnParameters SpawnParameters;
	SpawnParameters.SpawnCollisionHandlingOverride =
		ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	auto SpawnEvidence = [World, &SpawnParameters](
		const TCHAR* Name,
		const FVector& Location,
		const FRotator& Rotation) -> AIGMissingFloorEvidence*
	{
		SpawnParameters.Name = FName(Name);
		return World->SpawnActor<AIGMissingFloorEvidence>(
			AIGMissingFloorEvidence::StaticClass(),
			FTransform(Rotation, Location),
			SpawnParameters);
	};
	EquipmentVisuals.Reset();
	auto AddEquipment = [this, World](
		UStaticMesh* Mesh,
		UMaterialInterface* Material,
		const FName Name,
		const FVector& Location,
		const FVector& LocalSize,
		const FRotator& Rotation = FRotator::ZeroRotator)
	{
		if (!Mesh)
		{
			return;
		}
		UStaticMeshComponent* Component =
			NewObject<UStaticMeshComponent>(this, Name);
		Component->SetStaticMesh(Mesh);
		if (Material)
		{
			Component->SetMaterial(0, Material);
		}
		Component->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Component->SetGenerateOverlapEvents(false);
		Component->SetCanEverAffectNavigation(false);
		Component->SetCastShadow(true);
		Component->SetWorldTransform(FTransform(
			Rotation,
			Location,
			LocalSize / 100.0f));
		AddInstanceComponent(Component);
		Component->RegisterComponentWithWorld(World);
		EquipmentVisuals.Add(Component);
	};

	EvictionNotice = SpawnEvidence(
		TEXT("MissingFloorEvictionNotice"),
		IGNightFour::EvictionNoticeLocation,
		FRotator::ZeroRotator);
	CleaningDrain = SpawnEvidence(
		TEXT("MissingFloorCleaningDrain"),
		IGNightFour::CleaningDrainLocation,
		FRotator(90.0f, 0.0f, 0.0f));
	FloatBypass = SpawnEvidence(
		TEXT("MissingFloorFloatBypass"),
		IGNightFour::FloatBypassLocation,
		FRotator(90.0f, 0.0f, 0.0f));
	TransferPump = SpawnEvidence(
		TEXT("MissingFloorTransferPump"),
		IGNightFour::TransferPumpLocation,
		FRotator::ZeroRotator);
	WallBreakTarget = SpawnEvidence(
		TEXT("MissingFloorWallBreakTarget"),
		IGNightFour::WallBreakLocation,
		FRotator::ZeroRotator);
	EndingATarget = SpawnEvidence(
		TEXT("MissingFloorEndingATarget"),
		IGNightFour::EndingALocation,
		FRotator::ZeroRotator);
	EndingBTarget = SpawnEvidence(
		TEXT("MissingFloorEndingBTarget"),
		IGNightFour::EndingBLocation,
		FRotator::ZeroRotator);
	if (!EvictionNotice || !CleaningDrain || !FloatBypass || !TransferPump
		|| !WallBreakTarget || !EndingATarget || !EndingBTarget)
	{
		return false;
	}

	EvictionNotice->Configure(
		CubeMesh,
		PaperMaterial,
		FVector(21.0f, 1.0f, 29.7f),
		NSLOCTEXT("IGMissingFloor", "EvictionNoticePrompt", "퇴거 요구서"),
		NSLOCTEXT(
			"IGMissingFloor",
			"EvictionNoticeThought",
			"내일 오전 7시 누수 보수. …그 전에 벽을 또 덮겠다는 거야."),
		EIGMissingFloorTruth::StillCoveringIt,
		EIGMissingFloorSource::EvictionWarning,
		0.0f,
		0.02f);
	EvictionNotice->OnExamined.AddUObject(
		this, &AIGMissingFloorNightFourDirector::HandleEvictionNotice);

	CleaningDrain->Configure(
		LargeValveMesh ? LargeValveMesh : CylinderMesh,
		MetalMaterial,
		LargeValveMesh ? FVector(100.0f) : FVector(18.0f, 18.0f, 5.0f),
		NSLOCTEXT("IGMissingFloor", "CleaningDrainPrompt", "세척 배수 — OPEN"),
		NSLOCTEXT(
			"IGMissingFloor",
			"CleaningDrainThought",
			"배출 길이 먼저 열렸다. 탱크 물이 세척관으로 내려간다."),
		EIGMissingFloorTruth::None,
		EIGMissingFloorSource::None,
		0.8f,
		0.18f);
	CleaningDrain->OnExamined.AddUObject(
		this, &AIGMissingFloorNightFourDirector::HandleCleaningDrain);

	FloatBypass->Configure(
		SmallValveMesh ? SmallValveMesh : CylinderMesh,
		MetalMaterial,
		SmallValveMesh ? FVector(100.0f) : FVector(14.0f, 14.0f, 4.0f),
		NSLOCTEXT("IGMissingFloor", "FloatBypassPrompt", "부자밸브 우회 — OPEN"),
		NSLOCTEXT(
			"IGMissingFloor",
			"FloatBypassThought",
			"수위 부자가 막던 급수관을 우회했다."),
		EIGMissingFloorTruth::None,
		EIGMissingFloorSource::None,
		0.8f,
		0.18f);
	FloatBypass->OnExamined.AddUObject(
		this, &AIGMissingFloorNightFourDirector::HandleFloatBypass);

	TransferPump->Configure(
		CubeMesh,
		MetalMaterial,
		FVector(6.0f, 34.0f, 52.0f),
		NSLOCTEXT(
			"IGMissingFloor", "TransferPumpPrompt", "이송펌프 선택반 — MANUAL"),
		NSLOCTEXT(
			"IGMissingFloor",
			"TransferPumpThought",
			"1층 저수조에서 옥상으로 물이 올라간다."),
		EIGMissingFloorTruth::None,
		EIGMissingFloorSource::None,
		1.0f,
		0.22f);
	TransferPump->OnExamined.AddUObject(
		this, &AIGMissingFloorNightFourDirector::HandleTransferPump);

	// P5 is a real 2019 cleaning circuit. These non-interactive pieces make the
	// two wheels read as valves attached to a tank manifold, rather than sprites
	// hovering in front of the shell. Basic shapes are deliberately used here:
	// they are the replaceable greybox contract for the final authored asset.
	AddEquipment(
		CylinderMesh, MetalMaterial, TEXT("NightFourRoofManifold"),
		FVector(5.0f, 140.0f, 1300.0f), FVector(7.0f, 7.0f, 155.0f),
		FRotator(90.0f, 0.0f, 0.0f));
	AddEquipment(
		CylinderMesh, MetalMaterial, TEXT("NightFourTankFeed"),
		FVector(5.0f, 133.0f, 1300.0f), FVector(9.0f, 9.0f, 22.0f),
		FRotator(0.0f, 0.0f, 90.0f));
	for (const TPair<FName, float>& Branch : {
		TPair<FName, float>(TEXT("NightFourDrain"), -62.0f),
		TPair<FName, float>(TEXT("NightFourBypass"), 72.0f)})
	{
		AddEquipment(
			CylinderMesh,
			MetalMaterial,
			FName(*(Branch.Key.ToString() + TEXT("Riser"))),
			FVector(Branch.Value, 140.0f, 1320.0f),
			FVector(8.0f, 8.0f, 40.0f));
		AddEquipment(
			CylinderMesh,
			MetalMaterial,
			FName(*(Branch.Key.ToString() + TEXT("Nipple"))),
			FVector(Branch.Value, 149.0f, 1340.0f),
			FVector(10.0f, 10.0f, 18.0f),
			FRotator(0.0f, 0.0f, 90.0f));
		AddEquipment(
			CylinderMesh,
			DarkMaterial,
			FName(*(Branch.Key.ToString() + TEXT("Body"))),
			FVector(Branch.Value, 154.0f, 1340.0f),
			FVector(16.0f, 16.0f, 10.0f),
			FRotator(0.0f, 0.0f, 90.0f));
	}

	// A recognizable close-coupled pump: bolted base, motor, volute, suction
	// and discharge pipes, plus the wall selector that owns the interaction.
	// Every bottom face is on Z=0 or on the base above it; nothing is suspended.
	AddEquipment(
		CubeMesh, DarkMaterial, TEXT("NightFourPumpBase"),
		FVector(93.0f, -170.0f, 4.0f), FVector(64.0f, 42.0f, 8.0f));
	AddEquipment(
		CylinderMesh, MetalMaterial, TEXT("NightFourPumpMotor"),
		FVector(101.0f, -170.0f, 25.0f), FVector(22.0f, 22.0f, 36.0f),
		FRotator(90.0f, 0.0f, 0.0f));
	AddEquipment(
		CylinderMesh, DarkMaterial, TEXT("NightFourPumpMotorCap"),
		FVector(120.0f, -170.0f, 25.0f), FVector(18.0f, 18.0f, 4.0f),
		FRotator(90.0f, 0.0f, 0.0f));
	AddEquipment(
		CylinderMesh, MetalMaterial, TEXT("NightFourPumpVolute"),
		FVector(75.0f, -170.0f, 25.0f), FVector(30.0f, 30.0f, 16.0f),
		FRotator(90.0f, 0.0f, 0.0f));
	AddEquipment(
		CylinderMesh, DarkMaterial, TEXT("NightFourPumpCoupling"),
		FVector(86.0f, -170.0f, 25.0f), FVector(8.0f, 8.0f, 10.0f),
		FRotator(90.0f, 0.0f, 0.0f));
	AddEquipment(
		CylinderMesh, MetalMaterial, TEXT("NightFourPumpDischarge"),
		FVector(75.0f, -170.0f, 60.0f), FVector(8.0f, 8.0f, 50.0f));
	AddEquipment(
		CylinderMesh, MetalMaterial, TEXT("NightFourPumpSuction"),
		FVector(75.0f, -147.0f, 25.0f), FVector(8.0f, 8.0f, 34.0f),
		FRotator(0.0f, 0.0f, 90.0f));
	AddEquipment(
		CylinderMesh, DarkMaterial, TEXT("NightFourPumpSelector"),
		FVector(67.5f, -170.0f, 110.0f), FVector(10.0f, 10.0f, 4.0f),
		FRotator(90.0f, 0.0f, 0.0f));
	AddEquipment(
		CubeMesh, RedMaterial, TEXT("NightFourPumpAlarmLamp"),
		FVector(66.5f, -178.0f, 126.0f), FVector(2.0f, 5.0f, 5.0f));
	AddEquipment(
		CubeMesh, ScreenMaterial, TEXT("NightFourPumpRunLamp"),
		FVector(66.5f, -169.0f, 126.0f), FVector(2.0f, 5.0f, 5.0f));

	WallBreakTarget->Configure(
		CubeMesh,
		nullptr,
		FVector(3.0f, 92.0f, 168.0f),
		NSLOCTEXT("IGMissingFloor", "WallBreakPrompt", "공동 벽 — 망치질"),
		FText::GetEmpty(),
		EIGMissingFloorTruth::None,
		EIGMissingFloorSource::None,
		1.0f,
		1.0f);
	WallBreakTarget->OnExamined.AddUObject(
		this, &AIGMissingFloorNightFourDirector::HandleWallStrike);
	// The real gypsum panel remains the visible surface. This trace receiver is
	// invisible so it cannot z-fight or read as a second wall laid over it.
	WallBreakTarget->GetPresentationMesh()->SetVisibility(false, true);

	EndingATarget->Configure(
		CubeMesh,
		nullptr,
		FVector(18.0f, 24.0f, 5.0f),
		NSLOCTEXT(
			"IGMissingFloor", "EndingAPrompt", "조율 렌치를 돌려놓고 물러난다"),
		FText::GetEmpty(),
		EIGMissingFloorTruth::None,
		EIGMissingFloorSource::None,
		1.2f,
		0.03f);
	EndingATarget->OnExamined.AddUObject(
		this, &AIGMissingFloorNightFourDirector::HandleEndingA);

	EndingBTarget->Configure(
		CubeMesh,
		nullptr,
		FVector(42.0f, 42.0f, 4.0f),
		NSLOCTEXT(
			"IGMissingFloor", "EndingBPrompt", "곁에 앉아 대답한다"),
		FText::GetEmpty(),
		EIGMissingFloorTruth::None,
		EIGMissingFloorSource::None,
		1.2f,
		0.03f);
	EndingBTarget->OnExamined.AddUObject(
		this, &AIGMissingFloorNightFourDirector::HandleEndingB);

	if (!BuildFinaleVisuals())
	{
		return false;
	}

	RefreshPresentation();
	return ValidateFixtures();
}

bool AIGMissingFloorNightFourDirector::BuildFinaleVisuals()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}

	auto LoadMesh = [](const TCHAR* Name) -> UStaticMesh*
	{
		return LoadObject<UStaticMesh>(
			nullptr,
			*FString::Printf(TEXT("/Game/Meshes/%s.%s"), Name, Name));
	};
	auto LoadMaterial = [](const TCHAR* Name) -> UMaterialInterface*
	{
		return LoadObject<UMaterialInterface>(
			nullptr,
			*FString::Printf(
				TEXT("/Game/Prototype/Materials/%s.%s"), Name, Name));
	};

	UStaticMesh* ClothingMesh = LoadMesh(TEXT("SM_FinalCavityClothingShell"));
	UStaticMesh* BoneMesh = LoadMesh(TEXT("SM_FinalCavityBoneInsert"));
	UStaticMesh* TarpMesh = LoadMesh(TEXT("SM_FinalCavityTarp"));
	UStaticMesh* CasterMesh = LoadMesh(TEXT("SM_FinalCavityBrokenCaster"));
	UStaticMesh* MokWorkwearMesh = LoadMesh(TEXT("SM_MokHansooWorkwear"));
	UStaticMesh* MokHeadHandsMesh = LoadMesh(TEXT("SM_MokHansooHeadHands"));
	UStaticMesh* MokBoardMesh = LoadMesh(TEXT("SM_MokHansooGypsumBoard"));
	UStaticMesh* TuningHammerMesh = LoadMesh(TEXT("SM_TuningHammer"));
	UStaticMesh* PhoneMesh = LoadMesh(TEXT("SM_CrackedPhone"));
	UStaticMesh* PlaneMesh = LoadObject<UStaticMesh>(
		nullptr, TEXT("/Engine/BasicShapes/Plane.Plane"));

	UMaterialInterface* DryClothMaterial = LoadMaterial(TEXT("M_ConcreteDark"));
	UMaterialInterface* BoneMaterial = LoadMaterial(TEXT("M_PaperOld"));
	UMaterialInterface* TarpMaterial = LoadMaterial(TEXT("M_WindowDark"));
	UMaterialInterface* MetalMaterial = LoadMaterial(TEXT("M_P3CabinetMetalUV"));
	UMaterialInterface* WorkwearMaterial = LoadMaterial(TEXT("M_PlasticDark"));
	UMaterialInterface* BoardMaterial = LoadMaterial(TEXT("M_MissingFloorPlaster_XY"));
	UMaterialInterface* CavityDetailMaterial = LoadMaterial(
		TEXT("M_SpriteFinalCavity"));
	UMaterialInterface* MokDetailMaterial = LoadMaterial(
		TEXT("M_SpriteMokFinalUpper"));

	if (!ClothingMesh || !BoneMesh || !TarpMesh || !CasterMesh
		|| !MokWorkwearMesh || !MokHeadHandsMesh || !MokBoardMesh
		|| !TuningHammerMesh || !PhoneMesh || !DryClothMaterial
		|| !BoneMaterial || !TarpMaterial || !MetalMaterial
		|| !WorkwearMaterial || !BoardMaterial || !PlaneMesh
		|| !CavityDetailMaterial || !MokDetailMaterial)
	{
		return false;
	}

	auto AddVisual = [this, World](
		const FName Name,
		UStaticMesh* Mesh,
		UMaterialInterface* Material,
		const FVector& Location,
		const FRotator& Rotation) -> UStaticMeshComponent*
	{
		UStaticMeshComponent* Visual = NewObject<UStaticMeshComponent>(this, Name);
		if (!Visual)
		{
			return nullptr;
		}
		Visual->SetStaticMesh(Mesh);
		Visual->SetMaterial(0, Material);
		Visual->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Visual->SetGenerateOverlapEvents(false);
		Visual->SetCanEverAffectNavigation(false);
		Visual->SetCastShadow(true);
		Visual->SetWorldTransform(FTransform(Rotation, Location));
		Visual->SetVisibility(false, true);
		Visual->SetHiddenInGame(true, true);
		AddInstanceComponent(Visual);
		Visual->RegisterComponentWithWorld(World);
		return Visual;
	};

	CavityRevealVisuals.Reset();
	CavityRevealVisuals.Add(AddVisual(
		TEXT("FinalCavityClothing"), ClothingMesh, DryClothMaterial,
		IGNightFour::CavityVisualOrigin, FRotator::ZeroRotator));
	CavityRevealVisuals.Add(AddVisual(
		TEXT("FinalCavityBoneInsert"), BoneMesh, BoneMaterial,
		IGNightFour::CavityVisualOrigin, FRotator::ZeroRotator));
	CavityRevealVisuals.Add(AddVisual(
		TEXT("FinalCavityTarp"), TarpMesh, TarpMaterial,
		IGNightFour::CavityVisualOrigin, FRotator::ZeroRotator));
	CavityRevealVisuals.Add(AddVisual(
		TEXT("FinalCavityCaster"), CasterMesh, MetalMaterial,
		IGNightFour::CavityVisualOrigin, FRotator::ZeroRotator));

	MokVisuals.Reset();
	MokVisuals.Add(AddVisual(
		TEXT("MokHansooWorkwear"), MokWorkwearMesh, WorkwearMaterial,
		IGNightFour::MokStartLocation, IGNightFour::MokRotation));
	MokVisuals.Add(AddVisual(
		TEXT("MokHansooHeadHands"), MokHeadHandsMesh, BoneMaterial,
		IGNightFour::MokStartLocation, IGNightFour::MokRotation));
	MokVisuals.Add(AddVisual(
		TEXT("MokHansooGypsumBoard"), MokBoardMesh, BoardMaterial,
		IGNightFour::MokStartLocation, IGNightFour::MokRotation));

	CavityDetailCard = AddVisual(
		TEXT("FinalCavityDetailCard"), PlaneMesh, CavityDetailMaterial,
		IGNightFour::CavityDetailCenter,
		IGNightFour::DetailCardRotation(
			FVector(0.0f, 1.0f, 0.0f),
			IGNightFour::CavityDetailNormal));
	if (CavityDetailCard)
	{
		CavityDetailCard->SetWorldScale3D(FVector(1.19f, 1.78f, 1.0f));
		CavityDetailCard->SetCastShadow(false);
	}
	MokDetailCard = AddVisual(
		TEXT("MokHansooDetailCard"), PlaneMesh, MokDetailMaterial,
		IGNightFour::MokStartLocation + IGNightFour::MokDetailOffset,
		IGNightFour::DetailCardRotation(
			FVector(1.0f, 0.0f, 0.0f),
			IGNightFour::MokDetailNormal));
	if (MokDetailCard)
	{
		MokDetailCard->SetWorldScale3D(FVector(1.21f, 1.82f, 1.0f));
		MokDetailCard->SetCastShadow(false);
	}

	EndingHammerVisual = AddVisual(
		TEXT("NightFourEndingHammer"), TuningHammerMesh, MetalMaterial,
		IGNightFour::EndingHammerStart, FRotator(0.0f, 18.0f, -76.0f));
	EndingPhoneVisual = AddVisual(
		TEXT("NightFourEndingPhone"), PhoneMesh, WorkwearMaterial,
		IGNightFour::EndingPhoneRest, FRotator(0.0f, -8.0f, 0.0f));

	return CavityRevealVisuals.Num() == 4
		&& !CavityRevealVisuals.Contains(nullptr)
		&& MokVisuals.Num() == 3
		&& !MokVisuals.Contains(nullptr)
		&& CavityDetailCard
		&& MokDetailCard
		&& EndingHammerVisual
		&& EndingPhoneVisual;
}

void AIGMissingFloorNightFourDirector::SetCavityRevealVisible(
	const bool bVisible)
{
	bCavityPresentationVisible = bVisible;
	for (UStaticMeshComponent* Visual : CavityRevealVisuals)
	{
		if (Visual)
		{
			Visual->SetHiddenInGame(!bVisible, true);
			Visual->SetVisibility(bVisible, true);
		}
	}
	UpdateFinaleDetailLayers();
}

void AIGMissingFloorNightFourDirector::SetMokVisible(const bool bVisible)
{
	bMokPresentationVisible = bVisible;
	for (UStaticMeshComponent* Visual : MokVisuals)
	{
		if (Visual)
		{
			Visual->SetHiddenInGame(!bVisible, true);
			Visual->SetVisibility(bVisible, true);
		}
	}
	UpdateFinaleDetailLayers();
}

void AIGMissingFloorNightFourDirector::UpdateFinaleDetailLayers()
{
	FVector ViewLocation = FVector::ZeroVector;
	bool bHasView = false;
	if (APlayerController* Controller = GetWorld()
		? GetWorld()->GetFirstPlayerController()
		: nullptr)
	{
		FRotator ViewRotation;
		Controller->GetPlayerViewPoint(ViewLocation, ViewRotation);
		bHasView = true;
	}

	auto ShouldUseLayer = [bHasView, &ViewLocation](
		const FVector& Origin,
		const FVector& Normal,
		const bool bWorldVisible) -> bool
	{
		if (!bWorldVisible || !bHasView)
		{
			return false;
		}
		const FVector ToView = ViewLocation - Origin;
		const float Distance = ToView.Size();
		return Distance > 105.0f
			&& Distance < 360.0f
			&& FVector::DotProduct(
				Normal, ToView.GetSafeNormal()) > 0.68f;
	};

	if (CavityDetailCard)
	{
		const bool bShowDetail = ShouldUseLayer(
			IGNightFour::CavityDetailCenter,
			IGNightFour::CavityDetailNormal,
			bCavityPresentationVisible);
		CavityDetailCard->SetHiddenInGame(!bShowDetail, true);
		CavityDetailCard->SetVisibility(bShowDetail, true);
		for (UStaticMeshComponent* Visual : CavityRevealVisuals)
		{
			if (Visual)
			{
				const bool bShowShell =
					bCavityPresentationVisible && !bShowDetail;
				Visual->SetHiddenInGame(!bShowShell, true);
				Visual->SetVisibility(bShowShell, true);
				Visual->SetCastHiddenShadow(
					bCavityPresentationVisible && bShowDetail);
			}
		}
	}
	if (MokDetailCard)
	{
		const FVector MokLocation = MokVisuals.Num() > 0 && MokVisuals[0]
			? MokVisuals[0]->GetComponentLocation()
			: IGNightFour::MokStartLocation;
		const FVector CardCenter = MokLocation + IGNightFour::MokDetailOffset;
		MokDetailCard->SetWorldLocation(CardCenter);
		const bool bShowDetail = ShouldUseLayer(
			CardCenter,
			IGNightFour::MokDetailNormal,
			bMokPresentationVisible);
		MokDetailCard->SetHiddenInGame(!bShowDetail, true);
		MokDetailCard->SetVisibility(bShowDetail, true);
	}

	if (bCavityPresentationVisible || bMokPresentationVisible)
	{
		SetActorTickEnabled(true);
	}
}

void AIGMissingFloorNightFourDirector::SetFinaleCapturePreview(
	const bool bShowCavity,
	const bool bShowMok)
{
	if (bShowCavity || bShowMok)
	{
		for (TActorIterator<AIGPlayerCharacter> It(GetWorld()); It; ++It)
		{
			if (UIGFlashlightComponent* Flashlight = It->GetFlashlight())
			{
				Flashlight->SetOn(true);
			}
			break;
		}
	}
	SetCavityRevealVisible(bShowCavity);
	for (UStaticMeshComponent* Visual : MokVisuals)
	{
		if (Visual)
		{
			Visual->SetWorldLocationAndRotation(
				IGNightFour::MokStartLocation,
				IGNightFour::MokRotation);
		}
	}
	SetMokVisible(bShowMok);
	UpdateFinaleDetailLayers();
}

void AIGMissingFloorNightFourDirector::EndPlay(
	const EEndPlayReason::Type EndPlayReason)
{
	ResetFinaleTimers();
	if (AIGListenerEntity* ListenerActor = Listener.Get())
	{
		ListenerActor->OnPlayerCaptured.RemoveAll(this);
	}
	if (UWorld* World = GetWorld())
	{
		if (WaterMaskHumHandle != INDEX_NONE)
		{
			if (UIGNoiseSubsystem* Noise = World->GetSubsystem<UIGNoiseSubsystem>())
			{
				Noise->UnregisterHumSource(WaterMaskHumHandle);
			}
			WaterMaskHumHandle = INDEX_NONE;
		}
	}
	if (WaterMaskBed)
	{
		WaterMaskBed->Stop();
	}
	if (APlayerController* Controller = GetWorld()
		? GetWorld()->GetFirstPlayerController()
		: nullptr)
	{
		if (AIGHorrorHUD* Hud = Cast<AIGHorrorHUD>(Controller->GetHUD()))
		{
			Hud->EndMissingFloorFailureEnding();
		}
	}
	if (UWorld* World = GetWorld())
	{
		if (UIGMissingFloorAudioSubsystem* AudioDirector =
			World->GetSubsystem<UIGMissingFloorAudioSubsystem>())
		{
			AudioDirector->SetAuthoredSilence(false);
		}
	}
	Super::EndPlay(EndPlayReason);
}

void AIGMissingFloorNightFourDirector::SetHourActive(const bool bHourActive)
{
	bHourCurrentlyActive = bHourActive;
	if (!bHourCurrentlyActive)
	{
		if (UWorld* World = GetWorld())
		{
			if (UIGMissingFloorAudioSubsystem* AudioDirector =
				World->GetSubsystem<UIGMissingFloorAudioSubsystem>())
			{
				AudioDirector->SetAuthoredSilence(false);
			}
		}
		ResetFinaleTimers();
		bFinalRevealActive = false;
		bMokRetreatActive = false;
		SetMokVisible(false);
		if (AIGListenerEntity* ListenerActor = Listener.Get())
		{
			ListenerActor->SetDormant(true);
		}
		if (APlayerController* Controller = GetWorld()
			? GetWorld()->GetFirstPlayerController()
			: nullptr)
		{
			if (Controller->PlayerCameraManager)
			{
				Controller->PlayerCameraManager->StopCameraFade();
			}
		}
	}
	RefreshPresentation();
	if (bHourCurrentlyActive)
	{
		if (UIGMissingFloorNarrativeSubsystem* Narrative = GetNarrative())
		{
			if (Narrative->GetNightIndex() == 4
				&& Narrative->IsNightFourWallOpened()
				&& !Narrative->HasBeatPlayed(
					IGNightFour::FinalConfrontationBeat))
			{
				BeginCavityReveal();
			}
		}
	}
}

bool AIGMissingFloorNightFourDirector::ValidateFixtures() const
{
	return Scene.IsValid()
		&& EvictionNotice
		&& CleaningDrain
		&& FloatBypass
		&& TransferPump
		&& WallBreakTarget
		&& EndingATarget
		&& EndingBTarget
		&& Listener.IsValid()
		&& CavityRevealVisuals.Num() == 4
		&& MokVisuals.Num() == 3
		&& CavityDetailCard
		&& MokDetailCard
		&& EndingHammerVisual
		&& EndingPhoneVisual;
}

bool AIGMissingFloorNightFourDirector::IsWaterMaskPlaying() const
{
	// This is the gameplay mix state, not the platform audio-device state.
	// Headless contracts run with -nosound, where UAudioComponent::IsPlaying
	// is false even though the authored mask and hum source are both active.
	return bWaterMaskActive;
}

void AIGMissingFloorNightFourDirector::ResetFinaleTimers()
{
	GetWorldTimerManager().ClearTimer(DistantReplyTimer);
	GetWorldTimerManager().ClearTimer(MokRevealTimer);
	GetWorldTimerManager().ClearTimer(EntityPassTimer);
	GetWorldTimerManager().ClearTimer(BlackoutTimer);
	GetWorldTimerManager().ClearTimer(BlackoutRestoreTimer);
	GetWorldTimerManager().ClearTimer(FailureListingTimer);
	GetWorldTimerManager().ClearTimer(FailureRetryTimer);
}

void AIGMissingFloorNightFourDirector::BeginCavityReveal()
{
	UIGMissingFloorNarrativeSubsystem* Narrative = GetNarrative();
	if (!Narrative || !bHourCurrentlyActive || Narrative->GetNightIndex() != 4
		|| !Narrative->IsNightFourWallOpened())
	{
		return;
	}

	SetCavityRevealVisible(true);
	bFinalConfrontationComplete = Narrative->HasBeatPlayed(
		IGNightFour::FinalConfrontationBeat);
	if (bFinalConfrontationComplete || bFinalRevealActive
		|| bMokRetreatActive)
	{
		return;
	}

	if (Narrative->HasBeatPlayed(IGNightFour::FinalRevealBeat))
	{
		const bool bMokAlreadyVisible = MokVisuals.Num() > 0
			&& MokVisuals[0]
			&& MokVisuals[0]->IsVisible();
		if (!bMokAlreadyVisible
			&& !GetWorldTimerManager().IsTimerActive(MokRevealTimer)
			&& !GetWorldTimerManager().IsTimerActive(EntityPassTimer))
		{
			GetWorldTimerManager().SetTimer(
				MokRevealTimer,
				this,
				&AIGMissingFloorNightFourDirector::PresentMokHansoo,
				0.25f,
				false);
		}
		return;
	}

	ResetFinaleTimers();
	FinalRevealStage = 0;
	RevealAttentionSeconds = 0.0f;
	RevealStageElapsedSeconds = 0.0f;
	bFinalRevealActive = true;

	// The hydraulic bed has served its mechanical purpose. Discovery begins
	// with one real second where the player only hears their own room tone.
	if (WaterMaskBed)
	{
		WaterMaskBed->FadeOut(0.14f, 0.0f);
	}
	bWaterMaskActive = false;
	if (UWorld* World = GetWorld())
	{
		if (WaterMaskHumHandle != INDEX_NONE)
		{
			if (UIGNoiseSubsystem* Noise = World->GetSubsystem<UIGNoiseSubsystem>())
			{
				Noise->UnregisterHumSource(WaterMaskHumHandle);
			}
			WaterMaskHumHandle = INDEX_NONE;
		}
	}
	if (AIGListenerEntity* ListenerActor = Listener.Get())
	{
		ListenerActor->SetDormant(true);
	}

	for (TActorIterator<AIGPlayerCharacter> It(GetWorld()); It; ++It)
	{
		if (UIGFlashlightComponent* Flashlight = It->GetFlashlight())
		{
			Flashlight->SetOn(true);
			Flashlight->TriggerBrownOut(0.08f);
		}
		break;
	}

	AIGHorrorHUD::PushThought(
		this,
		NSLOCTEXT(
			"IGMissingFloor",
			"FinalRevealBegin",
			"빛을 위에서부터 천천히 내린다. 눈을 피하면 아무것도 확정되지 않는다."),
		4.2f);
	SetActorTickEnabled(true);
}

void AIGMissingFloorNightFourDirector::UpdateCavityReveal(
	const float DeltaSeconds)
{
	if (!bFinalRevealActive || FinalRevealStage < 0 || FinalRevealStage > 2)
	{
		return;
	}

	RevealStageElapsedSeconds += DeltaSeconds;
	APlayerController* Controller = GetWorld()
		? GetWorld()->GetFirstPlayerController()
		: nullptr;
	if (Controller)
	{
		FVector ViewLocation;
		FRotator ViewRotation;
		Controller->GetPlayerViewPoint(ViewLocation, ViewRotation);
		const FVector ToTarget =
			(IGNightFour::RevealTarget(FinalRevealStage) - ViewLocation)
			.GetSafeNormal();
		const float Facing = FVector::DotProduct(
			ViewRotation.Vector(), ToTarget);
		if (Facing >= 0.72f)
		{
			RevealAttentionSeconds += DeltaSeconds;
		}
		else
		{
			RevealAttentionSeconds = FMath::Max(
				0.0f, RevealAttentionSeconds - DeltaSeconds * 0.20f);
		}
	}

	// The generous fallback prevents a lost flashlight or unusual FOV from
	// turning a presentation beat into a progression lock.
	if (RevealAttentionSeconds
			>= IGNightFour::RequiredAttentionSeconds(FinalRevealStage)
		|| RevealStageElapsedSeconds >= 5.5f)
	{
		AdvanceCavityReveal();
	}
}

void AIGMissingFloorNightFourDirector::AdvanceCavityReveal()
{
	if (FinalRevealStage == 0)
	{
		AIGHorrorHUD::PushThought(
			this,
			NSLOCTEXT(
				"IGMissingFloor", "FinalRevealSkull",
				"머리뼈가 옷깃 안으로 기울어 있다. 세워 둔 모형이 아니다."),
			3.0f);
	}
	else if (FinalRevealStage == 1)
	{
		AIGHorrorHUD::PushThought(
			this,
			NSLOCTEXT(
				"IGMissingFloor", "FinalRevealRibs",
				"갈비뼈 사이로 마른 석고 가루가 쌓였다. 오래됐다."),
			3.0f);
	}
	else
	{
		BeginSilenceBeat();
		return;
	}

	++FinalRevealStage;
	RevealAttentionSeconds = 0.0f;
	RevealStageElapsedSeconds = 0.0f;
}

void AIGMissingFloorNightFourDirector::BeginSilenceBeat()
{
	bFinalRevealActive = false;
	FinalRevealStage = 3;
	if (UIGMissingFloorNarrativeSubsystem* Narrative = GetNarrative())
	{
		Narrative->MarkBeatPlayed(IGNightFour::FinalRevealBeat);
	}
	if (UWorld* World = GetWorld())
	{
		if (UIGMissingFloorAudioSubsystem* AudioDirector =
			World->GetSubsystem<UIGMissingFloorAudioSubsystem>())
		{
			AudioDirector->SetAuthoredSilence(true);
		}
		if (APlayerController* Controller = World->GetFirstPlayerController())
		{
			if (AIGPlayerCharacter* Player =
				Cast<AIGPlayerCharacter>(Controller->GetPawn()))
			{
				if (UIGStressComponent* Stress = Player->GetStress())
				{
					Stress->SuppressHeartbeat(2.35f, true);
				}
			}
		}
	}

	AIGHorrorHUD::PushThought(
		this,
		NSLOCTEXT(
			"IGMissingFloor",
			"FinalRevealShoes",
			"한쪽 신발이 안으로 꺾였다. 방수포와 카트 바퀴까지… 오빠가 여기 있었다."),
		5.0f);
	GetWorldTimerManager().SetTimer(
		DistantReplyTimer,
		this,
		&AIGMissingFloorNightFourDirector::PlayDistantReply,
		1.05f,
		false);
	GetWorldTimerManager().SetTimer(
		MokRevealTimer,
		this,
		&AIGMissingFloorNightFourDirector::PresentMokHansoo,
		2.35f,
		false);
}

void AIGMissingFloorNightFourDirector::PlayDistantReply()
{
	const FVector KnockLocation(360.0f, 930.0f, 1390.0f);
	IGAudio::SpawnOneShotAt(
		this,
		UIGToneSequenceSoundWave::CreateWallKnockReply(this),
		KnockLocation,
		0.72f,
		0.84f,
		220.0f,
		2400.0f,
		EIGAudioBus::Entity);
	AIGHorrorHUD::PushAudioCaption(
		this,
		NSLOCTEXT("IGMissingFloor", "FinalRevealKnockCaption", "멀리서, 두 번의 노크"),
		1.8f);
}

void AIGMissingFloorNightFourDirector::PresentMokHansoo()
{
	if (bFinalConfrontationComplete)
	{
		return;
	}
	if (UWorld* World = GetWorld())
	{
		if (UIGMissingFloorAudioSubsystem* AudioDirector =
			World->GetSubsystem<UIGMissingFloorAudioSubsystem>())
		{
			AudioDirector->SetAuthoredSilence(false);
			AudioDirector->SetThreatState(EIGAudioThreatState::Finale);
		}
	}
	for (UStaticMeshComponent* Visual : MokVisuals)
	{
		if (Visual)
		{
			Visual->SetWorldLocationAndRotation(
				IGNightFour::MokStartLocation, IGNightFour::MokRotation);
		}
	}
	SetMokVisible(true);
	IGAudio::SpawnOneShotAt(
		this,
		UIGToneSequenceSoundWave::CreateCardboardDrag(this),
		IGNightFour::MokStartLocation,
		0.52f,
		0.88f,
		160.0f,
		1200.0f,
		EIGAudioBus::World);
	AIGHorrorHUD::PushFearDirection(
		this, IGNightFour::MokStartLocation, 1.0f);
	AIGHorrorHUD::PushDialogue(
		this,
		NSLOCTEXT("IGMissingFloor", "MokHansooName", "목한수"),
		NSLOCTEXT(
			"IGMissingFloor",
			"MokHansooFinalLine",
			"…다시 덮어야 해요. 아무도 안 믿어요."),
		EIGDialogueChannel::Conversation,
		3.0f,
		EIGDialoguePriority::Critical);
	GetWorldTimerManager().SetTimer(
		EntityPassTimer,
		this,
		&AIGMissingFloorNightFourDirector::BeginEntityPass,
		3.35f,
		false);
}

void AIGMissingFloorNightFourDirector::BeginEntityPass()
{
	bMokRetreatActive = true;
	MokRetreatSeconds = 0.0f;
	SetActorTickEnabled(true);

	const FVector EntityStart(130.0f, 425.0f, 1253.0f);
	if (AIGListenerEntity* ListenerActor = Listener.Get())
	{
		ListenerActor->BeginFinalePass(
			EntityStart,
			{
				FVector(130.0f, 520.0f, 1253.0f),
				FVector(145.0f, 650.0f, 1253.0f),
				FVector(135.0f, 790.0f, 1253.0f),
				FVector(70.0f, 910.0f, 1253.0f),
			});
	}
	AIGHorrorHUD::PushFearDirection(this, EntityStart, 1.5f);
	AIGHorrorHUD::PushAudioCaption(
		this,
		NSLOCTEXT(
			"IGMissingFloor", "FinaleDragCaption",
			"석고 가루를 긁는 무거운 끌림 소리"),
		2.3f);
	GetWorldTimerManager().SetTimer(
		BlackoutTimer,
		this,
		&AIGMissingFloorNightFourDirector::TriggerBlackout,
		2.25f,
		false);
}

void AIGMissingFloorNightFourDirector::TriggerBlackout()
{
	if (APlayerController* Controller = GetWorld()
		? GetWorld()->GetFirstPlayerController()
		: nullptr)
	{
		if (Controller->PlayerCameraManager)
		{
			Controller->PlayerCameraManager->StartCameraFade(
				0.0f,
				1.0f,
				0.18f,
				FLinearColor::Black,
				/*bShouldFadeAudio=*/true,
				/*bHoldWhenFinished=*/true);
		}
	}
	GetWorldTimerManager().SetTimer(
		BlackoutRestoreTimer,
		this,
		&AIGMissingFloorNightFourDirector::CompleteConfrontation,
		0.55f,
		false);
}

void AIGMissingFloorNightFourDirector::CompleteConfrontation()
{
	bMokRetreatActive = false;
	SetMokVisible(false);
	if (AIGListenerEntity* ListenerActor = Listener.Get())
	{
		ListenerActor->SetDormant(true);
	}
	if (UIGMissingFloorNarrativeSubsystem* Narrative = GetNarrative())
	{
		Narrative->MarkBeatPlayed(IGNightFour::FinalConfrontationBeat);
	}
	bFinalConfrontationComplete = true;

	if (APlayerController* Controller = GetWorld()
		? GetWorld()->GetFirstPlayerController()
		: nullptr)
	{
		if (Controller->PlayerCameraManager)
		{
			Controller->PlayerCameraManager->StartCameraFade(
				1.0f,
				0.0f,
				0.45f,
				FLinearColor::Black,
				/*bShouldFadeAudio=*/true,
				/*bHoldWhenFinished=*/false);
		}
	}
	AIGHorrorHUD::PushThought(
		this,
		NSLOCTEXT(
			"IGMissingFloor", "FinalConfrontationAfter",
			"그것은 내 앞에서 멈추지 않았다. 목한수의 발소리만 따라갔다."),
		4.5f);
	RefreshPresentation();
}

void AIGMissingFloorNightFourDirector::UpdateMokRetreat(
	const float DeltaSeconds)
{
	MokRetreatSeconds += DeltaSeconds;
	const float Alpha = FMath::InterpEaseInOut(
		0.0f, 1.0f, FMath::Clamp(MokRetreatSeconds / 2.8f, 0.0f, 1.0f), 2.0f);
	const FVector Location = FMath::Lerp(
		IGNightFour::MokStartLocation, IGNightFour::MokExitLocation, Alpha);
	for (int32 Index = 0; Index < MokVisuals.Num(); ++Index)
	{
		if (UStaticMeshComponent* Visual = MokVisuals[Index])
		{
			Visual->SetWorldLocation(Location);
			Visual->SetWorldRotation(
				Index == 2
					? IGNightFour::MokRotation + FRotator(0.0f, 0.0f, Alpha * 9.0f)
					: IGNightFour::MokRotation);
		}
	}
}

void AIGMissingFloorNightFourDirector::UpdateEndingHammer(
	const float DeltaSeconds)
{
	if (!EndingHammerVisual)
	{
		bEndingHammerMoving = false;
		return;
	}
	EndingHammerSeconds += DeltaSeconds;
	const float Alpha = FMath::InterpEaseInOut(
		0.0f, 1.0f, FMath::Clamp(EndingHammerSeconds / 0.85f, 0.0f, 1.0f), 2.0f);
	EndingHammerVisual->SetWorldLocation(FMath::Lerp(
		IGNightFour::EndingHammerStart, IGNightFour::EndingHammerRest, Alpha));
	EndingHammerVisual->SetWorldRotation(FQuat::Slerp(
		FRotator(0.0f, 18.0f, -76.0f).Quaternion(),
		FRotator(-12.0f, 72.0f, -88.0f).Quaternion(),
		Alpha));
	if (Alpha >= 1.0f)
	{
		bEndingHammerMoving = false;
	}
}

void AIGMissingFloorNightFourDirector::HandleEvictionNotice(
	AIGMissingFloorEvidence* Evidence)
{
	RefreshPresentation();
}

void AIGMissingFloorNightFourDirector::HandleCleaningDrain(
	AIGMissingFloorEvidence* Evidence)
{
	ActivateControl(IGNightFour::CleaningDrainId, Evidence);
}

void AIGMissingFloorNightFourDirector::HandleFloatBypass(
	AIGMissingFloorEvidence* Evidence)
{
	ActivateControl(IGNightFour::FloatBypassId, Evidence);
}

void AIGMissingFloorNightFourDirector::HandleTransferPump(
	AIGMissingFloorEvidence* Evidence)
{
	ActivateControl(IGNightFour::TransferPumpId, Evidence);
}

void AIGMissingFloorNightFourDirector::ActivateControl(
	const FName ControlId,
	AIGMissingFloorEvidence* Evidence)
{
	UIGMissingFloorNarrativeSubsystem* Narrative = GetNarrative();
	if (!Narrative || !bHourCurrentlyActive || Narrative->GetNightIndex() != 4)
	{
		return;
	}

	const int32 OrderIndex = Narrative->GetNightFourControlOrder().Num();
	if (!Narrative->ActivateNightFourControl(ControlId))
	{
		return;
	}

	// Every control was silent until now, which made the water mask a number
	// the HUD knew about rather than something the player built. The large
	// cleaning drain, the small float bypass and the transfer pump motor are
	// each their own sound (§10.3 밸브 3종), so the roof is assembled by ear.
	const FVector ControlLocation =
		Evidence ? Evidence->GetActorLocation() : GetActorLocation();
	USoundBase* ControlCue = ControlId == IGNightFour::TransferPumpId
		? static_cast<USoundBase*>(
			UIGToneSequenceSoundWave::CreateVentDuctSpinUp(this))
		: static_cast<USoundBase*>(UIGToneSequenceSoundWave::CreateValveOpen(
			this,
			ControlId == IGNightFour::CleaningDrainId ? 1 : 2));
	IGAudio::SpawnOneShotAt(
		this,
		ControlCue,
		ControlLocation,
		0.78f,
		1.0f,
		170.0f,
		1500.0f,
		EIGAudioBus::Puzzle);
	const bool bSafeStep = IGNightFour::SafeOrder().IsValidIndex(OrderIndex)
		&& IGNightFour::SafeOrder()[OrderIndex] == ControlId;
	if (!bSafeStep)
	{
		bHydraulicAlarmTriggered = true;
		if (UWorld* World = GetWorld())
		{
			if (UIGNoiseSubsystem* Noise = World->GetSubsystem<UIGNoiseSubsystem>())
			{
				Noise->ReportNoise(
					Evidence ? Evidence->GetActorLocation() : GetActorLocation(),
					0.70f,
					this);
			}
		}
		IGAudio::SpawnOneShotAt(
			this,
			UIGToneSequenceSoundWave::CreateDoorThud(this),
			Evidence ? Evidence->GetActorLocation() : GetActorLocation(),
			0.9f,
			1.0f,
			180.0f,
			1800.0f,
			EIGAudioBus::Puzzle);
		AIGHorrorHUD::PushThought(
			this,
			NSLOCTEXT(
				"IGMissingFloor",
				"P5PressureAlarm",
				"닫힌 쪽에 압력이 걸렸다. 인터록은 멈췄지만… 들었을 거야."),
			3.8f);
	}

	if (Evidence)
	{
		Evidence->SetInteractionEnabled(false);
	}
	StartWaterMaskIfReady();
	RefreshPresentation();
}

void AIGMissingFloorNightFourDirector::StartWaterMaskIfReady()
{
	// 엔딩 C는 매물 화면 전에 기계음 마스킹을 제거한다. 시간이 봉인된 동안에도
	// RefreshPresentation이 호출될 수 있으므로 호출부와 이 함수에서 함께 막는다.
	if (bFailureEndingActive)
	{
		return;
	}
	UIGMissingFloorNarrativeSubsystem* Narrative = GetNarrative();
	if (!Narrative || !Narrative->IsNightFourMaskRunning())
	{
		return;
	}
	Narrative->MarkPuzzleSolved(IGNightFour::PuzzleId);
	bWaterMaskActive = bHourCurrentlyActive;
	if (bHourCurrentlyActive && WaterMaskHumHandle == INDEX_NONE)
	{
		if (UWorld* World = GetWorld())
		{
			if (UIGNoiseSubsystem* Noise = World->GetSubsystem<UIGNoiseSubsystem>())
			{
				// The moving riser calls the listener to itself first. The local
				// 0.40 mask then turns each 1.0 hammer report into the authored
				// 0.60 effective loudness without deleting the sound.
				Noise->ReportNoise(FVector(310.0f, 700.0f, 1300.0f), 0.55f, this);
				WaterMaskHumHandle = Noise->RegisterHumSource(
					FVector(246.0f, 700.0f, 1300.0f),
					720.0f,
					0.40f);
			}
		}
	}
	if (WaterMaskBed)
	{
		if (bHourCurrentlyActive && !WaterMaskBed->IsPlaying())
		{
			WaterMaskBed->Play();
		}
		return;
	}

	WaterMaskBed = NewObject<UAudioComponent>(this, TEXT("NightFourWaterMask"));
	WaterMaskBed->RegisterComponent();
	WaterMaskBed->SetWorldLocation(FVector(305.0f, 700.0f, 1300.0f));
	WaterMaskBed->SetSound(
		UIGToneSequenceSoundWave::CreateFloodedCorridorWaterBed(this));
	WaterMaskBed->AttenuationSettings = IGAudio::MakeAttenuation(
		this,
		180.0f,
		1500.0f,
		EIGAudioBus::Puzzle);
	WaterMaskBed->bAllowSpatialization = true;
	WaterMaskBed->SetVolumeMultiplier(0.62f);
	if (UWorld* World = GetWorld())
	{
		if (UIGMissingFloorAudioSubsystem* AudioDirector =
			World->GetSubsystem<UIGMissingFloorAudioSubsystem>())
		{
			AudioDirector->RegisterComponent(
				WaterMaskBed,
				EIGAudioBus::Puzzle);
		}
	}
	if (bHourCurrentlyActive)
	{
		WaterMaskBed->Play();
	}
	if (bHourCurrentlyActive)
	{
		AIGHorrorHUD::PushThought(
			this,
			NSLOCTEXT(
				"IGMissingFloor",
				"P5MaskReady",
				"올라가는 물과 내려가는 물. 두 관이 벽 양쪽에서 운다."),
			4.0f);
	}
}

void AIGMissingFloorNightFourDirector::HandleWallStrike(
	AIGMissingFloorEvidence* Evidence)
{
	UIGMissingFloorNarrativeSubsystem* Narrative = GetNarrative();
	if (!Narrative || !Narrative->IsNightFourMaskRunning()
		|| Narrative->IsNightFourWallOpened())
	{
		return;
	}

	const int32 StrikeCount = Narrative->RecordNightFourWallStrike();
	// §21.3 망치 임팩트. The strike index escalates the §10.3 three-stage
	// fracture, so the wall audibly goes from bruised to broken through and the
	// player never needs the counter to know where they are.
	IGAudio::SpawnOneShotAt(
		this,
		UIGToneSequenceSoundWave::CreateHammerImpact(this, StrikeCount - 1),
		Evidence ? Evidence->GetActorLocation() : IGNightFour::WallBreakLocation,
		1.0f,
		1.0f,
		160.0f,
		1400.0f,
		EIGAudioBus::Puzzle);

	if (StrikeCount == 3)
	{
		Narrative->RegisterTruthSource(
			EIGMissingFloorTruth::StillCoveringIt,
			EIGMissingFloorSource::BreakerCutIntervention);
		Narrative->MarkBeatPlayed(IGNightFour::PowerCutBeat);
		if (AIGPrologueWorldScene* SceneActor = Scene.Get())
		{
			SceneActor->SetMissingFloorAnnexPower(false);
		}
		AIGHorrorHUD::PushThought(
			this,
			NSLOCTEXT(
				"IGMissingFloor", "NightFourPowerCut", "…목한수가 전기를 내렸다."),
			3.5f);
	}
	else if (StrikeCount < 5)
	{
		AIGHorrorHUD::PushThought(
			this,
			FText::Format(
				NSLOCTEXT(
					"IGMissingFloor",
					"NightFourStrikeCount",
					"석고가 갈라졌다. {0} / 5"),
				FText::AsNumber(StrikeCount)),
			2.2f);
	}

	if (StrikeCount >= 5)
	{
		AIGPrologueWorldScene* SceneActor = Scene.Get();
		if (!SceneActor || !SceneActor->OpenMissingFloorCavity())
		{
			return;
		}
		Narrative->SetNightFourWallOpened(true);
		if (Evidence)
		{
			Evidence->SetInteractionEnabled(false);
			Evidence->SetActorHiddenInGame(true);
		}
		BeginCavityReveal();
	}
	RefreshPresentation();
}

void AIGMissingFloorNightFourDirector::HandleEndingA(
	AIGMissingFloorEvidence* Evidence)
{
	FinishEnding(IGNightFour::EndingAId);
}

void AIGMissingFloorNightFourDirector::HandleEndingB(
	AIGMissingFloorEvidence* Evidence)
{
	FinishEnding(IGNightFour::EndingBId);
}

void AIGMissingFloorNightFourDirector::HandleNightFourCapture(APawn* Player)
{
	FailurePlayer = Cast<AIGPlayerCharacter>(Player);
	ResolveFailureEnding();
}

void AIGMissingFloorNightFourDirector::FinishEnding(const FName EndingId)
{
	UIGMissingFloorNarrativeSubsystem* Narrative = GetNarrative();
	if (!Narrative || !Narrative->WasFirstReportMade()
		|| !Narrative->IsNightFourWallOpened()
		|| !Narrative->HasBeatPlayed(IGNightFour::FinalConfrontationBeat)
		|| !Narrative->SelectEnding(EndingId))
	{
		return;
	}

	const bool bEndingA = EndingId == IGNightFour::EndingAId;
	if (bEndingA)
	{
		if (UIGMissingFloorAudioSubsystem* AudioDirector =
			GetWorld()->GetSubsystem<UIGMissingFloorAudioSubsystem>())
		{
			AudioDirector->PlayEndingATuningResolution();
		}
		bEndingHammerMoving = EndingHammerVisual != nullptr;
		EndingHammerSeconds = 0.0f;
		if (EndingHammerVisual)
		{
			EndingHammerVisual->SetHiddenInGame(false, true);
			EndingHammerVisual->SetVisibility(true, true);
		}
		SetActorTickEnabled(true);
	}
	else
	{
		if (EndingPhoneVisual)
		{
			EndingPhoneVisual->SetHiddenInGame(false, true);
			EndingPhoneVisual->SetVisibility(true, true);
		}
		IGAudio::SpawnOneShotAt(
			this,
			UIGToneSequenceSoundWave::CreateRelayClick(this),
			IGNightFour::EndingPhoneRest,
			0.35f,
			0.82f,
			80.0f,
			520.0f,
			EIGAudioBus::Puzzle);
	}
	AIGHorrorHUD::PushThought(
		this,
		bEndingA
			? NSLOCTEXT(
				"IGMissingFloor",
				"EndingAChoiceThought",
				"오빠의 렌치를 오른손 곁에 놓는다. 이제 기록이 남을 차례다.")
			: NSLOCTEXT(
				"IGMissingFloor",
				"EndingBChoiceThought",
				"폰을 끈다. 신호가 돌아올 때까지, 이번에는 내가 대답한다."),
		5.0f);
	RefreshPresentation();
	if (!bResolvedBroadcast)
	{
		bResolvedBroadcast = true;
		OnResolved.Broadcast();
	}
}

bool AIGMissingFloorNightFourDirector::ResolveFailureEnding()
{
	UIGMissingFloorNarrativeSubsystem* Narrative = GetNarrative();
	if (bFailureEndingActive || !Narrative || !bHourCurrentlyActive
		|| Narrative->GetNightIndex() != 4
		|| Narrative->GetAggressionTier() < 3
		|| !Narrative->IsNightFourMaskRunning()
		|| !Narrative->SelectEnding(IGNightFour::EndingCId))
	{
		return false;
	}
	return CommitFailureEnding(/*bRecordCapture=*/true);
}

bool AIGMissingFloorNightFourDirector::ResolveDawnFailureEnding()
{
	// §20.4: 듣기만 하는 밤에는 포획이 없으므로 티어가 3에 닿지 않고, 위 경로로는
	// 엔딩 C에 영원히 도달할 수 없다. 그래서 조건을 밤4의 05:30 벽 미개방으로
	// 대체한다. 실패의 문이 완전히 닫히면 성공의 무게도 사라진다.
	UIGMissingFloorNarrativeSubsystem* Narrative = GetNarrative();
	if (bFailureEndingActive || !Narrative
		|| Narrative->GetNightIndex() != 4
		|| Narrative->IsNightFourWallOpened()
		|| !Narrative->GetEndingChoice().IsNone())
	{
		return false;
	}
	if (!Narrative->SelectEnding(IGNightFour::EndingCId))
	{
		return false;
	}
	return CommitFailureEnding(/*bRecordCapture=*/false);
}

bool AIGMissingFloorNightFourDirector::CommitFailureEnding(
	const bool bRecordCapture)
{
	UIGMissingFloorNarrativeSubsystem* Narrative = GetNarrative();
	if (!Narrative)
	{
		return false;
	}
	bFailureEndingActive = true;
	bFailureRetryEnabled = false;
	// The dawn route is not a catch. Recording one there would raise the tier
	// and inflate the capture count in a mode that has no captures at all.
	if (bRecordCapture)
	{
		Narrative->RecordCapture();
	}
	if (UWorld* World = GetWorld())
	{
		if (UIGMissingFloorAudioSubsystem* AudioDirector =
			World->GetSubsystem<UIGMissingFloorAudioSubsystem>())
		{
			// 최종 리빌의 강제 무음은 공동 대치까지만 유효하다. 엔딩 C에서는
			// 가까운 롤러 소리를 내기 전에 WORLD 버스를 Calm 상태로 복구한다.
			AudioDirector->SetAuthoredSilence(false);
			AudioDirector->SetThreatState(EIGAudioThreatState::Calm);
		}
	}

	for (TActorIterator<AIGNightPhaseDirector> It(GetWorld()); It; ++It)
	{
		It->SuspendForFailureEnding();
		break;
	}
	if (WaterMaskBed)
	{
		WaterMaskBed->FadeOut(0.16f, 0.0f);
	}
	bWaterMaskActive = false;
	if (WaterMaskHumHandle != INDEX_NONE)
	{
		if (UIGNoiseSubsystem* Noise = GetWorld()->GetSubsystem<UIGNoiseSubsystem>())
		{
			Noise->UnregisterHumSource(WaterMaskHumHandle);
		}
		WaterMaskHumHandle = INDEX_NONE;
	}
	if (AIGListenerEntity* ListenerActor = Listener.Get())
	{
		ListenerActor->SetDormant(true);
	}

	if (AIGPlayerCharacter* Character = FailurePlayer.Get())
	{
		Character->PlayCaptureFeedback(IGNightFour::FailureCaptureSeconds);
		if (UCharacterMovementComponent* Movement = Character->GetCharacterMovement())
		{
			Movement->StopMovementImmediately();
			Movement->DisableMovement();
		}
		if (APlayerController* Controller =
			Cast<APlayerController>(Character->GetController()))
		{
			if (Controller->PlayerCameraManager)
			{
				Controller->PlayerCameraManager->StartCameraFade(
					0.0f,
					1.0f,
					IGNightFour::FailureCaptureSeconds,
					FLinearColor::Black,
					false,
					true);
			}
		}
	}
	AIGHorrorHUD::PushAudioCaption(
		this,
		NSLOCTEXT("IGMissingFloor", "EndingCCaption", "아주 가까이서, 같은 두 번의 노크"),
		2.0f);
	GetWorldTimerManager().SetTimer(
		FailureListingTimer,
		this,
		&AIGMissingFloorNightFourDirector::BeginFailureListing,
		IGNightFour::FailureListingDelaySeconds,
		false);
	GetWorldTimerManager().SetTimer(
		FailureRetryTimer,
		this,
		&AIGMissingFloorNightFourDirector::EnableFailureRetry,
		IGNightFour::FailureRetryDelaySeconds,
		false);
	RefreshPresentation();
	return true;
}

void AIGMissingFloorNightFourDirector::BeginFailureListing()
{
	if (!bFailureEndingActive)
	{
		return;
	}
	AIGPlayerCharacter* Character = FailurePlayer.Get();
	APlayerController* Controller = Character
		? Cast<APlayerController>(Character->GetController())
		: nullptr;
	if (AIGHorrorHUD* Hud = Controller
		? Cast<AIGHorrorHUD>(Controller->GetHUD())
		: nullptr)
	{
		Hud->BeginMissingFloorFailureEnding();
	}
	IGAudio::SpawnOneShotAt(
		this,
		UIGToneSequenceSoundWave::CreateWallpaperSeamRoller(this),
		Character ? Character->GetActorLocation() : GetActorLocation(),
		0.78f,
		0.96f,
		120.0f,
		520.0f,
		EIGAudioBus::World);
}

void AIGMissingFloorNightFourDirector::EnableFailureRetry()
{
	if (!bFailureEndingActive)
	{
		return;
	}
	bFailureRetryEnabled = true;
	AIGPlayerCharacter* Character = FailurePlayer.Get();
	APlayerController* Controller = Character
		? Cast<APlayerController>(Character->GetController())
		: nullptr;
	if (AIGHorrorHUD* Hud = Controller
		? Cast<AIGHorrorHUD>(Controller->GetHUD())
		: nullptr)
	{
		Hud->SetMissingFloorFailureRetryEnabled(true);
	}
}

bool AIGMissingFloorNightFourDirector::CompleteFailurePresentationForProbe()
{
	if (!bFailureEndingActive)
	{
		return false;
	}
	GetWorldTimerManager().ClearTimer(FailureListingTimer);
	GetWorldTimerManager().ClearTimer(FailureRetryTimer);
	BeginFailureListing();
	EnableFailureRetry();
	return bFailureRetryEnabled;
}

bool AIGMissingFloorNightFourDirector::RequestFailureRetry()
{
	if (!bFailureEndingActive)
	{
		return false;
	}
	if (bFailureRetryEnabled)
	{
		ResetAfterFailureEnding();
	}
	// 카드가 펼쳐지는 동안 상호작용을 소비해 전면 연출 뒤의 월드 대상까지
	// 입력이 새어 나가지 않게 한다.
	return true;
}

void AIGMissingFloorNightFourDirector::ResetAfterFailureEnding()
{
	if (!bFailureEndingActive || !bFailureRetryEnabled)
	{
		return;
	}
	ResetFinaleTimers();
	bFailureEndingActive = false;
	bFailureRetryEnabled = false;
	bResolvedBroadcast = false;
	bHydraulicAlarmTriggered = false;
	bFinalRevealActive = false;
	bFinalConfrontationComplete = false;
	bMokRetreatActive = false;
	bEndingHammerMoving = false;
	FinalRevealStage = INDEX_NONE;
	RevealAttentionSeconds = 0.0f;
	RevealStageElapsedSeconds = 0.0f;
	MokRetreatSeconds = 0.0f;
	EndingHammerSeconds = 0.0f;
	SetCavityRevealVisible(false);
	SetMokVisible(false);

	UIGMissingFloorNarrativeSubsystem* Narrative = GetNarrative();
	if (Narrative)
	{
		Narrative->ResetNightFourForRetry();
	}
	if (AIGPrologueWorldScene* SceneActor = Scene.Get())
	{
		SceneActor->ResetMissingFloorCavity();
		SceneActor->SetMissingFloorAnnexPower(true);
	}
	if (WaterMaskBed)
	{
		WaterMaskBed->Stop();
	}
	bWaterMaskActive = false;

	AIGPlayerCharacter* Character = FailurePlayer.Get();
	APlayerController* Controller = Character
		? Cast<APlayerController>(Character->GetController())
		: nullptr;
	if (AIGHorrorHUD* Hud = Controller
		? Cast<AIGHorrorHUD>(Controller->GetHUD())
		: nullptr)
	{
		Hud->EndMissingFloorFailureEnding();
	}
	if (Character)
	{
		for (TActorIterator<AIGNightLoopDirector> It(GetWorld()); It; ++It)
		{
			It->RestorePlayerAtWakePoint(Character);
			break;
		}
		if (UCharacterMovementComponent* Movement = Character->GetCharacterMovement())
		{
			Movement->StopMovementImmediately();
			Movement->SetMovementMode(MOVE_Walking);
		}
		if (Controller && Controller->PlayerCameraManager)
		{
			Controller->PlayerCameraManager->StartCameraFade(
				1.0f,
				0.0f,
				0.85f,
				FLinearColor::Black,
				false,
				false);
		}
	}
	if (AIGListenerEntity* ListenerActor = Listener.Get())
	{
		ListenerActor->SetAggressionTier(1);
		if (ListenerActor->IsDormant())
		{
			ListenerActor->SetDormant(false);
		}
		else
		{
			ListenerActor->ResetToPatrolStart(false);
		}
	}
	RefreshPresentation();
	for (TActorIterator<AIGNightPhaseDirector> It(GetWorld()); It; ++It)
	{
		It->RestartTheHour(4);
		break;
	}
	FailurePlayer.Reset();
}

void AIGMissingFloorNightFourDirector::RefreshPresentation()
{
	UIGMissingFloorNarrativeSubsystem* Narrative = GetNarrative();
	if (!Narrative)
	{
		return;
	}
	const bool bNightFour = bHourCurrentlyActive
		&& Narrative->GetNightIndex() == 4
		&& !bFailureEndingActive;
	const bool bHasEnding = !Narrative->GetEndingChoice().IsNone();
	const bool bWallOpened = Narrative->IsNightFourWallOpened();
	bFinalConfrontationComplete = Narrative->HasBeatPlayed(
		IGNightFour::FinalConfrontationBeat);
	SetCavityRevealVisible(bWallOpened);

	const bool bShowEviction = !bHourCurrentlyActive
		&& Narrative->HasTruth(EIGMissingFloorTruth::WaitingForAnAnswer);
	EvictionNotice->SetActorHiddenInGame(!bShowEviction);
	EvictionNotice->SetInteractionEnabled(
		bShowEviction
		&& !Narrative->HasSource(
			EIGMissingFloorTruth::StillCoveringIt,
			EIGMissingFloorSource::EvictionWarning));

	auto RefreshControl = [bNightFour, Narrative](
		AIGMissingFloorEvidence* Control,
		const FName ControlId)
	{
		if (!Control)
		{
			return;
		}
		const bool bActivated = Narrative->HasNightFourControl(ControlId);
		Control->SetActorHiddenInGame(false);
		Control->SetInteractionEnabled(bNightFour && !bActivated);
	};
	RefreshControl(CleaningDrain, IGNightFour::CleaningDrainId);
	RefreshControl(FloatBypass, IGNightFour::FloatBypassId);
	RefreshControl(TransferPump, IGNightFour::TransferPumpId);

	if (Narrative->GetNightFourWallStrikeCount() >= 3)
	{
		if (AIGPrologueWorldScene* SceneActor = Scene.Get())
		{
			SceneActor->SetMissingFloorAnnexPower(!bNightFour);
		}
	}
	if (bWallOpened)
	{
		if (AIGPrologueWorldScene* SceneActor = Scene.Get())
		{
			SceneActor->OpenMissingFloorCavity();
		}
	}

	const bool bCanBreak = bNightFour
		&& Narrative->IsFinalChoiceUnlocked()
		&& Narrative->IsNightFourMaskRunning()
		&& !Narrative->IsNightFourWallOpened();
	WallBreakTarget->SetActorHiddenInGame(!bCanBreak);
	WallBreakTarget->SetInteractionEnabled(bCanBreak);

	const bool bCanChoose = bNightFour
		&& bWallOpened
		&& bFinalConfrontationComplete
		&& Narrative->WasFirstReportMade()
		&& !bHasEnding;
	EndingATarget->SetActorHiddenInGame(!bCanChoose);
	EndingATarget->SetInteractionEnabled(bCanChoose);
	EndingBTarget->SetActorHiddenInGame(!bCanChoose);
	EndingBTarget->SetInteractionEnabled(bCanChoose);
	if (EndingHammerVisual)
	{
		const bool bShowHammer = bCanChoose
			|| Narrative->GetEndingChoice() == IGNightFour::EndingAId;
		EndingHammerVisual->SetHiddenInGame(!bShowHammer, true);
		EndingHammerVisual->SetVisibility(bShowHammer, true);
		if (bCanChoose)
		{
			EndingHammerVisual->SetWorldLocationAndRotation(
				IGNightFour::EndingHammerStart,
				FRotator(0.0f, 18.0f, -76.0f));
		}
	}
	if (EndingPhoneVisual)
	{
		const bool bShowPhone =
			Narrative->GetEndingChoice() == IGNightFour::EndingBId;
		EndingPhoneVisual->SetHiddenInGame(!bShowPhone, true);
		EndingPhoneVisual->SetVisibility(bShowPhone, true);
	}

	if (bNightFour && !bWallOpened)
	{
		StartWaterMaskIfReady();
	}
	if (WaterMaskBed)
	{
		if (bNightFour && Narrative->IsNightFourMaskRunning() && !bWallOpened)
		{
			bWaterMaskActive = true;
			if (!WaterMaskBed->IsPlaying())
			{
				WaterMaskBed->Play();
			}
		}
		else
		{
			bWaterMaskActive = false;
			WaterMaskBed->Stop();
		}
	}
}

UIGMissingFloorNarrativeSubsystem*
AIGMissingFloorNightFourDirector::GetNarrative() const
{
	const UWorld* World = GetWorld();
	UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
	return GameInstance
		? GameInstance->GetSubsystem<UIGMissingFloorNarrativeSubsystem>()
		: nullptr;
}
