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
#include "IndieGame.h"
#include "Interaction/IGReadableNote.h"
#include "EngineUtils.h"
#include "Entity/IGListenerEntity.h"
#include "Entity/IGMissingFloorEvidence.h"
#include "Entity/IGMissingFloorFifthDawnDirector.h"
#include "Entity/IGMissingFloorMercyDirector.h"
#include "Entity/IGNoiseSubsystem.h"
#include "Entity/IGNightLoopDirector.h"
#include "Entity/IGNightPhaseDirector.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Math/RotationMatrix.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Narrative/IGMissingFloorNarrativeSubsystem.h"
#include "Player/IGFlashlightComponent.h"
#include "Player/IGHorrorHUD.h"
#include "Save/IGSaveSubsystem.h"
#include "Kismet/GameplayStatics.h"
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
	/** §22.4: 선택 직전 한 번. 양쪽을 다 보는 값이 여기서 정해진다. */
	const FName ChoiceOfferedBeat(TEXT("Night4.ChoiceOffered"));
	/** §22.3. 세 줄이 상한이다. 그 이상은 목격이 아니라 목록 낭독이 된다. */
	constexpr int32 ConfrontationReplyLimit = 3;
	/** 한 줄이 화면에 머무는 최소 시간과, 그만큼 뒤로 밀리는 통과. */
	constexpr float ConfrontationReplySeconds = 2.8f;
	constexpr float EntityPassBaseSeconds = 3.35f;
	constexpr float FailureCaptureSeconds = 2.15f;
	constexpr float FailureListingDelaySeconds = 2.2f;
	constexpr float FailureRetryDelaySeconds = 8.2f;

	// Posted beside the existing fourth-floor lift notice, not on top of the
	// 403 shipping labels. Its back face shares the established paper plane.
	const FVector EvictionNoticeLocation(640.0f, -232.0f, 1042.0f);
	// Both roof controls face the 1.2 m maintenance lane beside the tank.
	const FVector CleaningDrainLocation(-62.0f, 166.0f, 1340.0f);
	const FVector FloatBypassLocation(72.0f, 166.0f, 1340.0f);
	// Ground-floor transfer-pump selector, inside the management booth.
	// A wall-mounted selector above a floor-seated pump assembly in the booth.
	const FVector TransferPumpLocation(64.0f, -170.0f, 112.0f);
	// 선택반 옆에 붙은 절차서. 같은 벽면, 같은 높이.
	const FVector ProcedureSheetLocation(60.08f, -206.0f, 112.0f);
	/** 순서를 틀리면 인터록이 이만큼 선다(§7 P5). */
	constexpr float ControlLockoutSeconds = 10.0f;
	const FVector WallBreakLocation(246.0f, 700.0f, 1300.0f);
	/** 401호 안. 망치 소리에 그 노인이 아래에서 답하는 자리. */
	const FVector Unit401ReplyLocation(-146.0f, -239.0f, 960.0f);
	const FVector EndingALocation(223.0f, 665.0f, 1220.0f);
	const FVector EndingBLocation(165.0f, 765.0f, 1220.0f);
	const FVector CavityVisualOrigin(327.0f, 700.0f, 1200.0f);
	const FVector MokStartLocation(130.0f, 470.0f, 1195.0f);
	const FVector MokExitLocation(130.0f, 865.0f, 1195.0f);
	const FRotator MokRotation(0.0f, -90.0f, 0.0f);
	const FVector EndingHammerStart(218.0f, 660.0f, 1205.0f);
	const FVector EndingHammerRest(286.0f, 708.0f, 1268.0f);
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
			CavityVisualOrigin + FVector(-20.0f, 0.0f, 130.0f),
			CavityVisualOrigin + FVector(-23.0f, 0.0f, 95.0f),
			CavityVisualOrigin + FVector(-43.0f, 22.0f, 15.0f),
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

FVector AIGMissingFloorNightFourDirector::GetWallBreakLocation()
{
	return IGNightFour::WallBreakLocation;
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
		const FRotator& Rotation = FRotator::ZeroRotator) -> UStaticMeshComponent*
	{
		if (!Mesh)
		{
			return nullptr;
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
		return Component;
	};

	EvictionNotice = SpawnEvidence(
		TEXT("MissingFloorEvictionNotice"),
		IGNightFour::EvictionNoticeLocation,
		FRotator::ZeroRotator);
	CleaningDrain = SpawnEvidence(
		TEXT("MissingFloorCleaningDrain"),
		IGNightFour::CleaningDrainLocation,
		FRotator(0.0f, 0.0f, 90.0f));
	FloatBypass = SpawnEvidence(
		TEXT("MissingFloorFloatBypass"),
		IGNightFour::FloatBypassLocation,
		FRotator(0.0f, 0.0f, 90.0f));
	TransferPump = SpawnEvidence(
		TEXT("MissingFloorTransferPump"),
		IGNightFour::TransferPumpLocation,
		FRotator(0, 90, 0));
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
			"내일 아침 일곱 시부터 공사라고? 아직 안쪽도 못 봤는데."),
		EIGMissingFloorTruth::StillCoveringIt,
		EIGMissingFloorSource::EvictionWarning,
		0.0f,
		0.02f);
	EvictionNotice->OnExamined.AddUObject(
		this, &AIGMissingFloorNightFourDirector::HandleEvictionNotice);

	CleaningDrain->Configure(
		LargeValveMesh ? LargeValveMesh : CylinderMesh,
		MetalMaterial,
		LargeValveMesh ? FVector::ZeroVector : FVector(18.0f, 18.0f, 5.0f),
		NSLOCTEXT("IGMissingFloor", "CleaningDrainPrompt", "세척 배수 밸브 열기"),
		FText::GetEmpty(),
		EIGMissingFloorTruth::None,
		EIGMissingFloorSource::None,
		0.8f,
		0.18f);
	CleaningDrain->OnExamined.AddUObject(
		this, &AIGMissingFloorNightFourDirector::HandleCleaningDrain);

	FloatBypass->Configure(
		SmallValveMesh ? SmallValveMesh : CylinderMesh,
		MetalMaterial,
		SmallValveMesh ? FVector::ZeroVector : FVector(12.0f, 12.0f, 4.0f),
		NSLOCTEXT("IGMissingFloor", "FloatBypassPrompt", "부자밸브 우회 열기"),
		FText::GetEmpty(),
		EIGMissingFloorTruth::None,
		EIGMissingFloorSource::None,
		0.8f,
		0.18f);
	FloatBypass->OnExamined.AddUObject(
		this, &AIGMissingFloorNightFourDirector::HandleFloatBypass);

	TransferPump->Configure(
		LoadObject<UStaticMesh>(nullptr, TEXT("/Game/Meshes/SM_PumpControlPanel.SM_PumpControlPanel")),
		nullptr,
		FVector::ZeroVector,
		NSLOCTEXT(
			"IGMissingFloor", "TransferPumpPrompt", "이송펌프 — 수동으로 돌리기"),
		FText::GetEmpty(),
		EIGMissingFloorTruth::None,
		EIGMissingFloorSource::None,
		1.0f,
		0.22f);
	TransferPump->OnExamined.AddUObject(
		this, &AIGMissingFloorNightFourDirector::HandleTransferPump);

	// 설비의 고장 기록으로 관로 순서를 짐작하게 한다.
	SpawnParameters.Name = TEXT("MissingFloorPumpProcedureSheet");
	ProcedureSheet = World->SpawnActor<AIGReadableNote>(
		AIGReadableNote::StaticClass(),
		FTransform(FRotator(0, 90, 0), IGNightFour::ProcedureSheetLocation),
		SpawnParameters);
	if (!ProcedureSheet)
	{
		return false;
	}
	ProcedureSheet->ConfigurePrototypeVisuals(
		CubeMesh, LoadObject<UMaterialInterface>(nullptr,
			TEXT("/Game/Prototype/Materials/M_PumpProcedure.M_PumpProcedure")), FVector(21.0f, 0.08f, 29.7f));
	ProcedureSheet->SetInteractionPrompt(
		NSLOCTEXT("IGMissingFloor", "ProcedureSheetPrompt", "저수조 점검 메모"));
	ProcedureSheet->SetNoteText(
		NSLOCTEXT("IGMissingFloor", "ProcedureSheetTitle", "저수조 점검 메모 (2019.03)"),
		{
			NSLOCTEXT("IGMissingFloor", "ProcedureSheet1", "물이 넘친 날: 배수는 잠겨 있었고, 우회만 열려 있었음."),
			NSLOCTEXT("IGMissingFloor", "ProcedureSheet2", "펌프가 멎은 날: 배수만 열고 돌림. 우회관에 물이 안 찼음."),
			NSLOCTEXT("IGMissingFloor", "ProcedureSheet3", "세척할 때는 자동 수위 조절을 쓰지 말 것."),
			FText::GetEmpty(),
			NSLOCTEXT("IGMissingFloor", "ProcedureSheet4", "고장등이 켜지면 손대지 말고 10초 정도 기다리세요."),
			NSLOCTEXT("IGMissingFloor", "ProcedureSheet5", "배관이 조용해지고 등이 꺼진 뒤 다시 돌리면 됩니다."),
		});

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
			FVector(6.0f, 6.0f, 40.0f));
		AddEquipment(
			CylinderMesh,
			MetalMaterial,
			FName(*(Branch.Key.ToString() + TEXT("Nipple"))),
			FVector(Branch.Value, 149.0f, 1340.0f),
			FVector(6.0f, 6.0f, 18.0f),
			FRotator(0.0f, 0.0f, 90.0f));
		AddEquipment(
			CylinderMesh,
			DarkMaterial,
			FName(*(Branch.Key.ToString() + TEXT("Body"))),
			FVector(Branch.Value, 154.0f, 1340.0f),
			FVector(9.0f, 9.0f, 7.0f),
			FRotator(0.0f, 0.0f, 90.0f));
		AddEquipment(CylinderMesh, MetalMaterial,
			FName(*(Branch.Key.ToString() + TEXT("Stem"))),
			FVector(Branch.Value, 162.0f, 1340.0f), FVector(2.0f, 2.0f, 10.0f),
			FRotator(0.0f, 0.0f, 90.0f));
	}

	// 실물 사진에서 확인한 방열판·전장함·케이싱을 가진 소형 펌프.
	// 흡입관은 벽으로, 토출관은 천장으로 이어지며 공중에서 끝나지 않는다.
	AddEquipment(
		CubeMesh, DarkMaterial, TEXT("NightFourPumpBase"),
		FVector(87.5f, -154.0f, 4.0f), FVector(44.0f, 38.0f, 8.0f));
	AddEquipment(LoadObject<UStaticMesh>(nullptr, TEXT("/Game/Meshes/SM_BoothPump.SM_BoothPump")),
		nullptr, TEXT("NightFourPumpBody"), FVector(82.5f, -150.0f, 8.0f), FVector(100));
	// 조작반 아래에서 꺾어 올린다. 배관과 지지대는 한 메시로 묶었다.
	AddEquipment(LoadObject<UStaticMesh>(nullptr, TEXT("/Game/Meshes/SM_BoothPumpPipework.SM_BoothPumpPipework")),
		nullptr, TEXT("NightFourPumpPipework"), FVector(75.0f, -170.0f, 0.0f), FVector(100));
	const FVector PanelOrigin = TransferPump->GetActorLocation();
	PumpSelector = AddEquipment(LoadObject<UStaticMesh>(nullptr, TEXT("/Game/Meshes/SM_PumpSelector.SM_PumpSelector")),
		nullptr, TEXT("NightFourPumpSelector"), PanelOrigin + FVector(5.4f, 0, -7.7f), FVector(100), FRotator(0, 90, 0));
	UMaterialInterface* IndicatorMaterial = LoadObject<UMaterialInterface>(nullptr,
		TEXT("/Game/Prototype/Materials/M_PumpIndicator.M_PumpIndicator"));
	for (int32 Index = 0; Index < 3; ++Index)
	{
		UStaticMeshComponent* Lens = AddEquipment(CylinderMesh, IndicatorMaterial,
			FName(*FString::Printf(TEXT("NightFourPumpLens%d"), Index)),
			PanelOrigin + FVector(5.3f, (1 - Index) * 8.2f, 7.1f), FVector(2.1f, 2.1f, .45f), FRotator(90, 0, 0));
		if (Lens)
		{
			UMaterialInstanceDynamic* Lamp = Lens->CreateAndSetMaterialInstanceDynamic(0);
			Lamp->SetVectorParameterValue(TEXT("Tint"), Index == 0 ? FLinearColor(.28f,.30f,.25f)
				: Index == 1 ? FLinearColor(.015f,.28f,.06f) : FLinearColor(.38f,.016f,.008f));
			PumpLamps.Add(Lamp);
		}
	}

	WallBreakTarget->Configure(
		CubeMesh,
		nullptr,
		FVector(3.0f, 92.0f, 168.0f),
		NSLOCTEXT("IGMissingFloor", "WallBreakPrompt", "공동 벽 — 망치질"),
		FText::GetEmpty(),
		EIGMissingFloorTruth::None,
		EIGMissingFloorSource::None,
		// §18.4 망치 스윙 차징 0.8초. 뒤의 1.0은 §5.1 소음 크기다 — 같은
		// 자리에 붙어 있어서 한 번 헷갈리면 조용히 어긋난다.
		0.8f,
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
	// 유해와 목한수는 베이크된 3D 메시를 쓴다. 방수포와 카트 바퀴는 별도 소품이다.
	// 인물 앞에 정면 카드를 겹치지 않는다. 원점은 바닥 중심, 정면은 -Y다.
	UStaticMesh* CavityFigureMesh = LoadMesh(TEXT("SM_FinalCavityRemains"));
	UStaticMesh* MokFigureMesh = LoadMesh(TEXT("SM_MokHansooFigure"));
	UStaticMesh* TuningHammerMesh = LoadMesh(TEXT("SM_TuningHammer"));
	UStaticMesh* PhoneMesh = LoadMesh(TEXT("SM_CrackedPhone"));
	UStaticMesh* PlaneMesh = LoadObject<UStaticMesh>(
		nullptr, TEXT("/Engine/BasicShapes/Plane.Plane"));

	UMaterialInterface* DryClothMaterial = LoadMaterial(TEXT("M_ConcreteDark"));
	UMaterialInterface* BoneMaterial = LoadMaterial(TEXT("M_PaperOld"));
	UMaterialInterface* TarpMaterial = LoadMaterial(TEXT("M_PlasticDark"));
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
		if (Material)
		{
			Visual->SetMaterial(0, Material);
		}
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

	bCavityFigureAuthored = CavityFigureMesh != nullptr;
	bMokFigureAuthored = MokFigureMesh != nullptr;

	CavityRevealVisuals.Reset();
	if (bCavityFigureAuthored)
	{
		// 절차 셸은 -X를 보고 앉아 있었다(디테일 법선 (-1,0,0)). 정면 -Y인
		// 생성 메시는 yaw -90으로 같은 쪽을 본다.
		CavityRevealVisuals.Add(AddVisual(
			TEXT("FinalCavityRemains"), CavityFigureMesh, nullptr,
			IGNightFour::CavityVisualOrigin, FRotator(0.0f, -90.0f, 0.0f)));
		// 접은 방수포는 발밑에 눕힌다. 두께는 2.7cm이며 공동의 뒷벽 안쪽에 맞춘다.
		UStaticMeshComponent* FoldedTarp = AddVisual(
			TEXT("FinalCavityTarp"), TarpMesh, TarpMaterial,
			FVector(390.0f, 690.0f, 1200.4f), FRotator(90.0f, 0.0f, 0.0f));
		if (FoldedTarp)
		{
			FoldedTarp->SetWorldScale3D(FVector(0.12f, 0.8f, 0.65f));
		}
		CavityRevealVisuals.Add(FoldedTarp);
		CavityRevealVisuals.Add(AddVisual(
			TEXT("FinalCavityCaster"), CasterMesh, MetalMaterial,
			IGNightFour::CavityVisualOrigin + FVector(0.0f, 0.0f, -4.35f), FRotator::ZeroRotator));
	}
	else
	{
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
	}

	MokVisuals.Reset();
	if (bMokFigureAuthored)
	{
		// 절차 작업복은 -X가 정면이라 MokRotation(yaw -90)으로 +Y를 봤다.
		// 정면 -Y인 생성 메시는 yaw 180으로 같은 +Y를 본다.
		MokVisuals.Add(AddVisual(
			TEXT("MokHansooFigure"), MokFigureMesh, nullptr,
			IGNightFour::MokStartLocation, GetMokRotation()));
	}
	else
	{
		MokVisuals.Add(AddVisual(
			TEXT("MokHansooWorkwear"), MokWorkwearMesh, WorkwearMaterial,
			IGNightFour::MokStartLocation, IGNightFour::MokRotation));
		MokVisuals.Add(AddVisual(
			TEXT("MokHansooHeadHands"), MokHeadHandsMesh, BoneMaterial,
			IGNightFour::MokStartLocation, IGNightFour::MokRotation));
		MokVisuals.Add(AddVisual(
			TEXT("MokHansooGypsumBoard"), MokBoardMesh, BoardMaterial,
			IGNightFour::MokStartLocation, IGNightFour::MokRotation));
	}

	CavityDetailCard = nullptr;
	MokDetailCard = nullptr;
	if (!bCavityFigureAuthored)
	{
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
	}
	if (!bMokFigureAuthored)
	{
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
	}

	EndingHammerVisual = AddVisual(
		TEXT("NightFourEndingHammer"), TuningHammerMesh, MetalMaterial,
		IGNightFour::EndingHammerStart, FRotator(0.0f, 18.0f, -76.0f));
	EndingPhoneVisual = AddVisual(
		TEXT("NightFourEndingPhone"), PhoneMesh, WorkwearMaterial,
		IGNightFour::EndingPhoneRest, FRotator(0.0f, -8.0f, 0.0f));

	return HasFinaleFigures()
		&& EndingHammerVisual
		&& EndingPhoneVisual;
}

bool AIGMissingFloorNightFourDirector::HasFinaleFigures() const
{
	// 유해·방수포·바퀴를 확인한다. 구형 인물의 경우에만 정면 보조 카드가 필요하다.
	const bool bCavityComplete = bCavityFigureAuthored
		? CavityRevealVisuals.Num() == 3
		: CavityRevealVisuals.Num() == 4 && CavityDetailCard != nullptr;
	const bool bMokComplete = bMokFigureAuthored
		? MokVisuals.Num() == 1
		: MokVisuals.Num() == 3 && MokDetailCard != nullptr;
	return bCavityComplete
		&& !CavityRevealVisuals.Contains(nullptr)
		&& bMokComplete
		&& !MokVisuals.Contains(nullptr);
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

FRotator AIGMissingFloorNightFourDirector::GetMokRotation() const
{
	// 생성 메시의 정면은 -Y다. 등장·캡처·퇴장에도 같은 기준을 쓴다.
	return bMokFigureAuthored ? FRotator(0.0f, 180.0f, 0.0f) : IGNightFour::MokRotation;
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
				GetMokRotation());
		}
	}
	SetMokVisible(bShowMok);
	UpdateFinaleDetailLayers();
}

void AIGMissingFloorNightFourDirector::AbortFailureBlackout(const TCHAR* Reason)
{
	UWorld* World = GetWorld();
	AIGPlayerCharacter* Character = FailurePlayer.Get();
	APlayerController* Controller = Character
		? Cast<APlayerController>(Character->GetController())
		: nullptr;
	if (!Controller && World)
	{
		Controller = World->GetFirstPlayerController();
	}
	if (Controller && Controller->PlayerCameraManager)
	{
		Controller->PlayerCameraManager->StopCameraFade();
	}
	if (Character)
	{
		if (UCharacterMovementComponent* Movement =
			Character->GetCharacterMovement())
		{
			Movement->SetMovementMode(MOVE_Walking);
		}
	}
	UE_LOG(
		LogIndieGame,
		Warning,
		TEXT("IG_ENDING_C aborted blackout: %s (controller=%s)"),
		Reason,
		Controller ? TEXT("yes") : TEXT("none"));
}

void AIGMissingFloorNightFourDirector::EndPlay(
	const EEndPlayReason::Type EndPlayReason)
{
	// 엔딩 C는 이동을 잠그고 bHoldWhenFinished로 암전을 건 뒤 타이머로 푼다.
	// ResetFinaleTimers()가 바로 그 타이머를 지우므로, 시퀀스 도중에 이
	// 디렉터가 사라지면 푸는 쪽이 사라진다.
	if (bFailureEndingActive && EndPlayReason != EEndPlayReason::LevelTransition
		&& EndPlayReason != EEndPlayReason::EndPlayInEditor
		&& EndPlayReason != EEndPlayReason::Quit)
	{
		AbortFailureBlackout(
			TEXT("director destroyed during the failure ending"));
	}
	bFailureEndingActive = false;
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
	const auto ValveFits = [](const AIGMissingFloorEvidence* Evidence, const float Diameter)
	{
		const UStaticMeshComponent* Mesh = Evidence ? Evidence->GetPresentationMesh() : nullptr;
		if (!Mesh || !Mesh->GetStaticMesh()) { return false; }
		const FVector Size = Mesh->GetStaticMesh()->GetBounds().BoxExtent * 2.0f * Mesh->GetComponentScale().GetAbs();
		const bool bFits = FMath::IsNearlyEqual(Size.GetMax(), Diameter, 1.0f) && Size.GetMin() < 6.0f;
		UE_LOG(LogIndieGame, Display, TEXT("SPATIAL_VALVE %s %s size=%s"), *Evidence->GetName(),
			bFits ? TEXT("PASS") : TEXT("FAIL"), *Size.ToCompactString());
		return bFits;
	};
	return Scene.IsValid()
		&& EvictionNotice
		&& CleaningDrain
		&& FloatBypass
		&& ValveFits(CleaningDrain, 18.0f)
		&& ValveFits(FloatBypass, 12.0f)
		&& TransferPump && PumpSelector && PumpLamps.Num() == 3
		&& WallBreakTarget
		&& EndingATarget
		&& EndingBTarget
		&& Listener.IsValid()
		&& HasFinaleFigures()
		&& EndingHammerVisual
		&& EndingPhoneVisual;
}

int32 AIGMissingFloorNightFourDirector::GetPumpLampMask() const
{
	int32 Mask = 0;
	for (int32 Index = 0; Index < PumpLamps.Num(); ++Index)
	{
		if (PumpLamps[Index] && PumpLamps[Index]->K2_GetScalarParameterValue(TEXT("Lit")) > 1.f)
			Mask |= 1 << Index;
	}
	return Mask;
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
			"위에서부터 천천히 비춘다."),
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
				"머리가 옷깃 안으로 기울어 있다."),
			3.0f);
		// 알아보는 순간 공동 안에서 석고가 갈라져 떨어진다. 글만 있던 단계에 소리.
		IGAudio::SpawnOneShotAt(
			this,
			UIGToneSequenceSoundWave::CreateSettlePlasterTick(this),
			IGNightFour::CavityDetailCenter,
			0.9f,
			0.9f,
			160.0f,
			1200.0f,
			EIGAudioBus::Puzzle);
		if (AIGPlayerCharacter* PlayerCharacter =
			Cast<AIGPlayerCharacter>(UGameplayStatics::GetPlayerPawn(this, 0)))
		{
			if (UIGStressComponent* Stress = PlayerCharacter->GetStress())
			{
				Stress->ApplyScare(0.3f);
			}
			PlayerCharacter->PlayScareKick(0.8f);
		}
	}
	else if (FinalRevealStage == 1)
	{
		AIGHorrorHUD::PushThought(
			this,
			NSLOCTEXT(
				"IGMissingFloor", "FinalRevealRibs",
				"갈비뼈 사이에 석고 가루가 쌓였다. 오래됐다."),
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
			"한쪽 발이 안으로 꺾였다. 방수포, 카트 바퀴. 오빠다."),
		5.0f);
	const UIGMissingFloorNarrativeSubsystem* NarrativeNow = GetNarrative();
	if (FifthDawn.IsValid() && NarrativeNow
		&& !NarrativeNow->WasFifthDawnInterludeCompleted())
	{
		// 오빠를 본 직후에 그 다섯 새벽을 산다. 독백 한 줄을 읽을 3초를 두고
		// 눈을 감긴다. 노크와 목한수는 눈을 뜬 뒤에 온다.
		GetWorldTimerManager().SetTimer(
			InterludeTimer,
			this,
			&AIGMissingFloorNightFourDirector::BeginInterludeInsideTheWall,
			3.0f,
			false);
		return;
	}
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

void AIGMissingFloorNightFourDirector::SetFifthDawn(
	AIGMissingFloorFifthDawnDirector* InFifthDawn)
{
	FifthDawn = InFifthDawn;
}

void AIGMissingFloorNightFourDirector::BeginInterludeInsideTheWall()
{
	AIGMissingFloorFifthDawnDirector* FifthDawnActor = FifthDawn.Get();
	AIGPlayerCharacter* PlayerCharacter =
		Cast<AIGPlayerCharacter>(UGameplayStatics::GetPlayerPawn(this, 0));
	UWorld* World = GetWorld();
	if (!FifthDawnActor || !PlayerCharacter || !World || bAwaitingInterlude)
	{
		ResumeAfterInterlude();
		return;
	}
	// 막간의 베드는 침묵 위에서 못 산다. 침묵은 막간이 걷고, 눈을 뜨면
	// 다시 건다. 시계와 안전망도 선다 — 벽 안의 2분 40초는 이 밤의 시간이
	// 아니다.
	if (UIGMissingFloorAudioSubsystem* AudioDirector =
		World->GetSubsystem<UIGMissingFloorAudioSubsystem>())
	{
		AudioDirector->SetAuthoredSilence(false);
	}
	for (TActorIterator<AIGNightPhaseDirector> It(World); It; ++It)
	{
		It->SetHourPaused(true);
		break;
	}
	for (TActorIterator<AIGMissingFloorMercyDirector> It(World); It; ++It)
	{
		It->SetHourActive(false);
		break;
	}
	if (!FifthDawnActor->StartInterlude(PlayerCharacter))
	{
		for (TActorIterator<AIGNightPhaseDirector> It(World); It; ++It)
		{
			It->SetHourPaused(false);
			break;
		}
		for (TActorIterator<AIGMissingFloorMercyDirector> It(World); It; ++It)
		{
			It->SetHourActive(true);
			break;
		}
		ResumeAfterInterlude();
		return;
	}
	bAwaitingInterlude = true;
}

void AIGMissingFloorNightFourDirector::HandleInterludeCompleted()
{
	if (!bAwaitingInterlude)
	{
		return;
	}
	bAwaitingInterlude = false;
	if (UWorld* World = GetWorld())
	{
		for (TActorIterator<AIGNightPhaseDirector> It(World); It; ++It)
		{
			It->SetHourPaused(false);
			break;
		}
		for (TActorIterator<AIGMissingFloorMercyDirector> It(World); It; ++It)
		{
			It->SetHourActive(true);
			break;
		}
		// 눈을 뜬 자리의 정적. 목한수가 나타나며 걷는다.
		if (UIGMissingFloorAudioSubsystem* AudioDirector =
			World->GetSubsystem<UIGMissingFloorAudioSubsystem>())
		{
			AudioDirector->SetAuthoredSilence(true);
		}
	}
	ResumeAfterInterlude();
}

void AIGMissingFloorNightFourDirector::ResumeAfterInterlude()
{
	// 공동 너머의 노크 둘, 그리고 목한수.
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
	AIGHorrorHUD::PushAudioCaptionAt(
		this,
		NSLOCTEXT("IGMissingFloor", "FinalRevealKnockCaption", "멀리서, 두 번의 노크"),
		1.8f,
		KnockLocation);
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
				IGNightFour::MokStartLocation, GetMokRotation());
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
			"그만해요. 거기 손대지 마. 내가… 내가 처리한다고 했잖아요."),
		EIGDialogueChannel::Conversation,
		3.0f,
		EIGDialoguePriority::Critical);

	// §22.3. 그의 말은 언제나 같다 — 애원은 본 것과 무관하다. 달라지는 것은
	// 유담이 그 앞에서 무엇을 댈 수 있느냐다. 아무것도 못 본 회차는 침묵이
	// 대답이고, 그것도 이 장면에서 성립한다.
	TArray<FText> ReplyLines;
	BuildConfrontationReplyLines(ReplyLines);
	for (const FText& ReplyLine : ReplyLines)
	{
		AIGHorrorHUD::PushDialogue(
			this,
			NSLOCTEXT("IGMissingFloor", "YudamName", "백유담"),
			ReplyLine,
			EIGDialogueChannel::Conversation,
			IGNightFour::ConfrontationReplySeconds,
			EIGDialoguePriority::Critical);
	}
	// 그가 지나가기 전에 댄 줄이 다 끝나야 한다. 존재가 먼저 들어오면
	// 대치가 대화가 아니라 배경이 된다. 줄마다 실제로 화면에 머무는 시간을
	// 잰다 — 한 줄에 3.05초를 곱하던 값은 마흔 글자짜리 줄이 5초를 쓰는
	// 것을 몰랐고, 세 줄이면 그가 대화 중간에 들어왔다.
	float ReplySeconds = 0.0f;
	for (const FText& ReplyLine : ReplyLines)
	{
		ReplySeconds += AIGHorrorHUD::EstimateDialogueSeconds(
			ReplyLine, IGNightFour::ConfrontationReplySeconds);
	}
	GetWorldTimerManager().SetTimer(
		EntityPassTimer,
		this,
		&AIGMissingFloorNightFourDirector::BeginEntityPass,
		IGNightFour::EntityPassBaseSeconds + ReplySeconds,
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
	AIGHorrorHUD::PushAudioCaptionAt(
		this,
		NSLOCTEXT(
			"IGMissingFloor", "FinaleDragCaption",
			"석고 가루를 긁는 무거운 끌림 소리"),
		2.3f,
		EntityStart);
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
			"나를 지나쳐 갔다."),
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
					? GetMokRotation() + FRotator(0.0f, 0.0f, Alpha * 9.0f)
					: GetMokRotation());
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
	if (bControlLockoutActive || Narrative->HasNightFourControl(ControlId))
	{
		return;
	}
	// 틀린 손잡이는 기록에 남지 않는다. 여섯 순서가 전부 같은 곳에 닿으면
	// 순서는 퍼즐이 아니라 요금이다 — 틀리면 소리가 나고, 인터록이 서고,
	// 다시 해야 한다.
	const bool bSafeStep = IGNightFour::SafeOrder().IsValidIndex(OrderIndex)
		&& IGNightFour::SafeOrder()[OrderIndex] == ControlId;
	if (!bSafeStep)
	{
		HandleControlMisorder(ControlId, Evidence);
		return;
	}
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

	if (Evidence)
	{
		Evidence->SetInteractionEnabled(false);
	}
	// 성공한 조작에만 결과를 말한다. 증거 액터가 판정 전에 대사를 띄우면
	// 역순으로 밸브를 돌려도 '물이 빠진다'와 경보가 동시에 나왔다.
	const FText Result = ControlId == IGNightFour::CleaningDrainId
		? NSLOCTEXT("IGMissingFloor", "CleaningDrainThought", "아래쪽 관으로 물이 빠진다.")
		: ControlId == IGNightFour::FloatBypassId
			? NSLOCTEXT("IGMissingFloor", "FloatBypassThought", "급수관에서도 물소리가 난다.")
			: NSLOCTEXT("IGMissingFloor", "TransferPumpThought", "펌프가 돌기 시작했다.");
	AIGHorrorHUD::PushThought(this, Result, 3.0f);
	StartWaterMaskIfReady();
	RefreshPresentation();
}

void AIGMissingFloorNightFourDirector::HandleControlMisorder(
	const FName ControlId,
	AIGMissingFloorEvidence* Evidence)
{
	bHydraulicAlarmTriggered = true;
	const FVector At = Evidence ? Evidence->GetActorLocation() : GetActorLocation();
	// 두 가지 잘못이 두 가지 소리를 낸다. 배수 전에 우회를 열면 탱크가
	// 넘치고, 관이 안 열린 채 펌프를 돌리면 역지변이 쾅 닫힌다. 둘 다
	// 0.70 — 그가 듣는다.
	const bool bOverflow = ControlId == IGNightFour::FloatBypassId;
	if (UWorld* World = GetWorld())
	{
		if (UIGNoiseSubsystem* Noise = World->GetSubsystem<UIGNoiseSubsystem>())
		{
			Noise->ReportNoise(At, 0.70f, this);
		}
	}
	IGAudio::SpawnOneShotAt(
		this,
		bOverflow
			? static_cast<USoundBase*>(
				UIGToneSequenceSoundWave::CreateRooftopTankSlosh(this))
			: static_cast<USoundBase*>(UIGToneSequenceSoundWave::CreateDoorThud(this)),
		At,
		0.9f,
		1.0f,
		180.0f,
		1800.0f,
		EIGAudioBus::Puzzle);
	AIGHorrorHUD::PushThought(
		this,
		bOverflow
			? NSLOCTEXT(
				"IGMissingFloor",
				"P5OverflowAlarm",
				"물이 넘친다. 배수를 안 열었다.")
			: NSLOCTEXT(
				"IGMissingFloor",
				"P5PressureAlarm",
				"펌프가 멎었다. 고장등이 꺼질 때까지 기다리자."),
		3.8f);
	// 인터록이 선다. 그동안은 어느 손잡이도 안 돈다.
	bControlLockoutActive = true;
	for (AIGMissingFloorEvidence* Control :
		{CleaningDrain.Get(), FloatBypass.Get(), TransferPump.Get()})
	{
		if (Control)
		{
			Control->SetInteractionEnabled(false);
		}
	}
	RefreshPresentation();
	GetWorldTimerManager().SetTimer(
		ControlLockoutTimer,
		this,
		&AIGMissingFloorNightFourDirector::ReleaseControlLockout,
		IGNightFour::ControlLockoutSeconds,
		false);
}

void AIGMissingFloorNightFourDirector::ReleaseControlLockout()
{
	bControlLockoutActive = false;
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
				Noise->ReportNoise(
					AIGPrologueWorldScene::GetSharedRiserLocation(),
					0.55f,
					this);
				WaterMaskHumHandle = Noise->RegisterHumSource(
					IGNightFour::WallBreakLocation,
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
	WaterMaskBed->SetWorldLocation(FVector(310.0f, 746.0f, 1300.0f));
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
			AudioDirector->RegisterPersistentBed(
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
				"물이 도는 동안은 벽 두드리는 소리가 묻힌다."),
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
		IGAudio::SampleVariantOr(
			TEXT("Hammer_Hit"), 2, static_cast<uint32>(StrikeCount) * 2654435761u,
			[this, StrikeCount]() -> USoundBase* { return UIGToneSequenceSoundWave::CreateHammerImpact(this, StrikeCount - 1); }),
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
				"IGMissingFloor", "NightFourPowerCut", "아래에서 차단기 내리는 소리가 났다."),
			3.5f);
		// 차단기는 내리는 소리가 있다. 남쪽 층계참에서 딸깍, 그리고 멀어지는
		// 발소리 넷 — 그가 왔다 갔다는 것을 화면 없이 안다. 밤4에서 그의
		// 유일한 개입이 독백 한 줄이었다.
		IGAudio::SpawnOneShotAt(
			this,
			UIGToneSequenceSoundWave::CreateRelayClick(this),
			IGNightFour::MokStartLocation + FVector(0.0f, 0.0f, 60.0f),
			0.9f,
			0.8f,
			260.0f,
			2600.0f,
			EIGAudioBus::World);
		AIGHorrorHUD::PushFearDirection(this, IGNightFour::MokStartLocation, 1.2f);
		AIGHorrorHUD::PushAudioCaptionAt(
			this,
			NSLOCTEXT("IGMissingFloor", "BreakerCutCaption", "차단기 내리는 소리"),
			2.0f,
			IGNightFour::MokStartLocation);
		for (int32 Step = 0; Step < 4; ++Step)
		{
			const FVector StepAt = FMath::Lerp(
				IGNightFour::MokStartLocation,
				IGNightFour::MokExitLocation,
				(Step + 1) / 4.0f) + FVector(0.0f, 0.0f, -30.0f * Step);
			FTimerHandle StepTimer;
			GetWorldTimerManager().SetTimer(
				StepTimer,
				FTimerDelegate::CreateWeakLambda(this, [this, StepAt, Step]()
				{
					IGAudio::SpawnOneShotAt(
						this,
						UIGToneSequenceSoundWave::CreateSurfaceFootstep(
							this,
							EIGFootstepSurface::Concrete,
							0.92f,
							0.6f - 0.1f * Step),
						StepAt,
						0.62f - 0.1f * Step,
						1.0f,
						200.0f,
						1800.0f,
						EIGAudioBus::World);
				}),
				0.9f + 0.55f * Step,
				false);
		}
	}
	else if (StrikeCount < 5)
	{
		// 몇 번째인지는 파쇄 소리가 말한다. 글은 손과 벽만 본다.
		AIGHorrorHUD::PushThought(
			this,
			StrikeCount == 1
				? NSLOCTEXT("IGMissingFloor", "NightFourStrikeFirst", "석고가 갈라졌다.")
				: StrikeCount == 2
					? NSLOCTEXT("IGMissingFloor", "NightFourStrikeSecond", "안쪽이 비었다. 소리가 다르다.")
					: NSLOCTEXT("IGMissingFloor", "NightFourStrikeFourth", "손이 저리다. 한 번만 더."),
			2.2f);
		if (StrikeCount == 4)
		{
			// 401호가 듣고 있다. 망치 소리에 그 노인이 아래에서 답한다 — 둘,
			// 쉬고, 하나. 7월 29일에 한 번 했던 것을 다시. 이 밤에 사람 쪽에서
			// 오는 유일한 소리다.
			IGAudio::SpawnOneShotAt(
				this,
				UIGToneSequenceSoundWave::CreateAnswerKnockPattern(this, 0.72f),
				IGNightFour::Unit401ReplyLocation,
				0.8f,
				1.0f,
				400.0f,
				4200.0f,
				EIGAudioBus::World);
			AIGHorrorHUD::PushAudioCaptionAt(
				this,
				NSLOCTEXT("IGMissingFloor", "Unit401ReplyCaption", "둘, 쉬고, 하나"),
				2.6f,
				IGNightFour::Unit401ReplyLocation);
		}
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

void AIGMissingFloorNightFourDirector::BuildConfrontationReplyLines(
	TArray<FText>& OutLines) const
{
	OutLines.Reset();
	const UIGMissingFloorNarrativeSubsystem* Narrative = GetNarrative();
	if (!Narrative)
	{
		return;
	}

	// 두 묶음으로 나눈다. 앞 묶음은 그가 한 일을 가리키고, 뒤 묶음은 그 일이
	// 누구에게 일어났는지를 가리킨다. 순서는 각 묶음 안에서 그를 겨냥하는
	// 정도이자, 사람 쪽에서는 도하와 가까운 정도다.
	struct FReplyEntry
	{
		EIGMissingFloorWitness Witness;
		FText Line;
	};

	const FReplyEntry Documents[] =
	{
		{
			EIGMissingFloorWitness::BoothSoundproofing,
			NSLOCTEXT(
				"IGMissingFloor",
				"ConfrontationReplyFoam",
				"관리실 안쪽 방이요. 문틈까지 계란판 붙여 놓으셨더라고요."),
		},
		{
			EIGMissingFloorWitness::BoothWallCalendar,
			NSLOCTEXT(
				"IGMissingFloor",
				"ConfrontationReplyCalendar",
				"달력이요. 26일에 동그라미 치고, 그 주는 안 넘기셨죠."),
		},
		{
			EIGMissingFloorWitness::AnnexWorkGlove,
			NSLOCTEXT(
				"IGMissingFloor",
				"ConfrontationReplyGlove",
				"5층에 장갑 한 짝 있던데요. 오빠 손엔 두 치수 커요."),
		},
		{
			EIGMissingFloorWitness::RecorderEmptyBay,
			NSLOCTEXT(
				"IGMissingFloor",
				"ConfrontationReplyRecorder",
				"녹화기 하드, 언제 빼셨어요."),
		},
		{
			EIGMissingFloorWitness::BoothInnerRoomHum,
			NSLOCTEXT(
				"IGMissingFloor",
				"ConfrontationReplyInnerRoom",
				"창고라면서요. 거기서 주무시잖아요."),
		},
		{
			EIGMissingFloorWitness::RoofDoorWind,
			NSLOCTEXT(
				"IGMissingFloor",
				"ConfrontationReplyWind",
				"옥상에서 바람 들어 봤어요. 바람이 둘, 쉬고, 하나로 불어요?"),
		},
	};

	const FReplyEntry Humans[] =
	{
		{
			EIGMissingFloorWitness::RooftopCigarettePack,
			NSLOCTEXT(
				"IGMissingFloor",
				"ConfrontationReplyPack",
				"탱크 옆에 담배 여섯 개비요. 거기 앉아서 쉬던 사람이에요."),
		},
		{
			EIGMissingFloorWitness::HwangWaterBowl,
			NSLOCTEXT(
				"IGMissingFloor",
				"ConfrontationReplyBowl",
				"401호 할머니는 아직도 물그릇을 갈아 놓으세요. 매일요."),
		},
		{
			EIGMissingFloorWitness::SeoSleepingPills,
			NSLOCTEXT(
				"IGMissingFloor",
				"ConfrontationReplyPills",
				"서일영 씨, 작년 팔월부터 약 드세요."),
		},
	};

	TArray<FText> HeldDocuments;
	for (const FReplyEntry& Entry : Documents)
	{
		if (Narrative->HasWitness(Entry.Witness))
		{
			HeldDocuments.Add(Entry.Line);
		}
	}
	TArray<FText> HeldHumans;
	for (const FReplyEntry& Entry : Humans)
	{
		if (Narrative->HasWitness(Entry.Witness))
		{
			HeldHumans.Add(Entry.Line);
		}
	}

	// 서류는 최대 둘까지만 댄다. 셋을 연달아 대면 사람이 아니라 조서가 된다.
	const int32 DocumentQuota = FMath::Min(HeldDocuments.Num(), 2);
	for (int32 Index = 0; Index < DocumentQuota; ++Index)
	{
		OutLines.Add(HeldDocuments[Index]);
	}
	if (HeldHumans.Num() > 0)
	{
		OutLines.Add(HeldHumans[0]);
	}
	// 셋을 봤으면 셋을 말한다. 남은 자리는 서류 먼저, 그다음 사람으로
	// 채운다 — 이 순서라야 마지막 줄이 계속 사람 쪽에 남는다.
	for (int32 Index = DocumentQuota;
		Index < HeldDocuments.Num()
			&& OutLines.Num() < IGNightFour::ConfrontationReplyLimit;
		++Index)
	{
		OutLines.Add(HeldDocuments[Index]);
	}
	for (int32 Index = 1;
		Index < HeldHumans.Num()
			&& OutLines.Num() < IGNightFour::ConfrontationReplyLimit;
		++Index)
	{
		OutLines.Add(HeldHumans[Index]);
	}
}

void AIGMissingFloorNightFourDirector::RequestEndingChoiceAutosave()
{
	UGameInstance* GameInstance = GetGameInstance();
	UWorld* World = GetWorld();
	UIGSaveSubsystem* SaveSubsystem = GameInstance
		? GameInstance->GetSubsystem<UIGSaveSubsystem>()
		: nullptr;
	if (!SaveSubsystem || !World)
	{
		return;
	}
	SaveSubsystem->RequestAutosave(
		FGameplayTag::RequestGameplayTag(FName(TEXT("Chapter.MissingFloor")), false),
		World->GetOutermost()->GetFName(),
		FGameplayTag::RequestGameplayTag(
			FName(TEXT("Checkpoint.MissingFloor.EndingChoice")), false));
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
				"여기 내려놓자. 밖에 나가서 사람을 불러야 해.")
			: NSLOCTEXT(
				"IGMissingFloor",
				"EndingBChoiceThought",
				"조금만 더 여기 있을게."),
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

	if (bRecordCapture)
	{
		if (AIGListenerEntity* ListenerActor = Listener.Get())
		{
			// 추적만 멈추고 이미 닿은 몸과 덮치는 동작은 암전까지 남긴다.
			ListenerActor->KeepCaptureVisible();
		}
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
	if (AIGListenerEntity* ListenerActor = Listener.Get())
	{
		ListenerActor->SetDormant(true);
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
	if (!Character)
	{
		// 복구가 폰에 묶여 있다. 폰이 사라진 채로 여기 오면 암전과 이동
		// 잠금이 그대로 남는다.
		AbortFailureBlackout(TEXT("failure ending finished without a pawn"));
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
	if (bShowEviction && !bEvictionAnnounced)
	{
		// 밤사이 벽에 붙은 종이. 붙이는 것은 못 봤어도 그 자리가 달라졌다는
		// 것은 들려야 한다.
		bEvictionAnnounced = true;
		IGAudio::SpawnOneShotAt(
			this,
			UIGToneSequenceSoundWave::CreatePickupRustle(this),
			IGNightFour::EvictionNoticeLocation,
			0.5f,
			1.0f,
			120.0f,
			1100.0f,
			EIGAudioBus::World);
		AIGHorrorHUD::PushAudioCaptionAt(
			this,
			NSLOCTEXT("IGMissingFloor", "EvictionPostedCaption", "4층 복도 — 종이 소리"),
			2.0f,
			IGNightFour::EvictionNoticeLocation);
	}
	EvictionNotice->SetInteractionEnabled(
		bShowEviction
		&& !Narrative->HasSource(
			EIGMissingFloorTruth::StillCoveringIt,
			EIGMissingFloorSource::EvictionWarning));

	auto RefreshControl = [bNightFour, Narrative, bLockout = bControlLockoutActive](
		AIGMissingFloorEvidence* Control,
		const FName ControlId)
	{
		if (!Control)
		{
			return;
		}
		const bool bActivated = Narrative->HasNightFourControl(ControlId);
		Control->SetActorHiddenInGame(false);
		Control->SetInteractionEnabled(bNightFour && !bActivated && !bLockout);
	};
	RefreshControl(CleaningDrain, IGNightFour::CleaningDrainId);
	RefreshControl(FloatBypass, IGNightFour::FloatBypassId);
	RefreshControl(TransferPump, IGNightFour::TransferPumpId);
	const bool bPumpRunning = bNightFour && Narrative->IsNightFourMaskRunning() && !bControlLockoutActive;
	if (PumpSelector)
	{
		PumpSelector->SetWorldRotation(FRotator(bPumpRunning ? -45.f : 0.f, 90.f, 0.f));
	}
	for (int32 Index = 0; Index < PumpLamps.Num(); ++Index)
	{
		const bool bLit = Index == 0 || (Index == 1 && bPumpRunning) || (Index == 2 && bControlLockoutActive);
		PumpLamps[Index]->SetScalarParameterValue(TEXT("Lit"), bLit ? 1.8f : 0.f);
	}


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
	if (bCanChoose && Narrative->MarkBeatPlayed(IGNightFour::ChoiceOfferedBeat))
	{
		// §22.4 리플레이 유인 2. 두 결말은 애도의 방식만 다르고 사실은
		// 같다(§9). 그 차이를 보려고 밤 4를 통째로 다시 하게 만들면
		// 「다시 오는 이유는 더 잘하기 위해서」라는 전제가 무너진다.
		RequestEndingChoiceAutosave();
	}
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
