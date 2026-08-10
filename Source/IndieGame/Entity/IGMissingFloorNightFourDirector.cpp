#include "Entity/IGMissingFloorNightFourDirector.h"

#include "Audio/IGAudioHelpers.h"
#include "Audio/IGToneSequenceSoundWave.h"
#include "Components/AudioComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Core/IGPrologueWorldScene.h"
#include "Engine/GameInstance.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Entity/IGMissingFloorEvidence.h"
#include "Entity/IGNoiseSubsystem.h"
#include "Materials/MaterialInterface.h"
#include "Narrative/IGMissingFloorNarrativeSubsystem.h"
#include "Player/IGHorrorHUD.h"

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
	PrimaryActorTick.bCanEverTick = false;
}

bool AIGMissingFloorNightFourDirector::Configure(AIGPrologueWorldScene* InScene)
{
	UWorld* World = GetWorld();
	if (!World || !InScene)
	{
		return false;
	}
	Scene = InScene;

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

	RefreshPresentation();
	return ValidateFixtures();
}

void AIGMissingFloorNightFourDirector::EndPlay(
	const EEndPlayReason::Type EndPlayReason)
{
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
	Super::EndPlay(EndPlayReason);
}

void AIGMissingFloorNightFourDirector::SetHourActive(const bool bHourActive)
{
	bHourCurrentlyActive = bHourActive;
	RefreshPresentation();
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
		&& EndingBTarget;
}

bool AIGMissingFloorNightFourDirector::IsWaterMaskPlaying() const
{
	// This is the gameplay mix state, not the platform audio-device state.
	// Headless contracts run with -nosound, where UAudioComponent::IsPlaying
	// is false even though the authored mask and hum source are both active.
	return bWaterMaskActive;
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
			0.9f);
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
	WaterMaskBed->AttenuationSettings = IGAudio::MakeAttenuation(this, 180.0f, 1500.0f);
	WaterMaskBed->bAllowSpatialization = true;
	WaterMaskBed->SetVolumeMultiplier(0.62f);
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
	IGAudio::SpawnOneShotAt(
		this,
		UIGToneSequenceSoundWave::CreateDoorThud(this),
		Evidence ? Evidence->GetActorLocation() : IGNightFour::WallBreakLocation,
		1.0f,
		0.78f + static_cast<float>(StrikeCount) * 0.025f);

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
		AIGHorrorHUD::PushThought(
			this,
			NSLOCTEXT(
				"IGMissingFloor",
				"NightFourWallOpened",
				"방수포, 부러진 바퀴, 내려앉은 옷. …오빠가 여기 있었다."),
			5.0f);
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

void AIGMissingFloorNightFourDirector::FinishEnding(const FName EndingId)
{
	UIGMissingFloorNarrativeSubsystem* Narrative = GetNarrative();
	if (!Narrative || !Narrative->WasFirstReportMade()
		|| !Narrative->IsNightFourWallOpened()
		|| !Narrative->SelectEnding(EndingId))
	{
		return;
	}

	const bool bEndingA = EndingId == IGNightFour::EndingAId;
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
	if (!Narrative || !bHourCurrentlyActive || Narrative->GetNightIndex() != 4
		|| Narrative->GetAggressionTier() < 3
		|| !Narrative->IsNightFourMaskRunning()
		|| !Narrative->SelectEnding(IGNightFour::EndingCId))
	{
		return false;
	}
	RefreshPresentation();
	if (!bResolvedBroadcast)
	{
		bResolvedBroadcast = true;
		OnResolved.Broadcast();
	}
	return true;
}

void AIGMissingFloorNightFourDirector::RefreshPresentation()
{
	UIGMissingFloorNarrativeSubsystem* Narrative = GetNarrative();
	if (!Narrative)
	{
		return;
	}
	const bool bNightFour = bHourCurrentlyActive && Narrative->GetNightIndex() == 4;
	const bool bHasEnding = !Narrative->GetEndingChoice().IsNone();

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
	if (Narrative->IsNightFourWallOpened())
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
		&& Narrative->IsNightFourWallOpened()
		&& Narrative->WasFirstReportMade()
		&& !bHasEnding;
	EndingATarget->SetActorHiddenInGame(!bCanChoose);
	EndingATarget->SetInteractionEnabled(bCanChoose);
	EndingBTarget->SetActorHiddenInGame(!bCanChoose);
	EndingBTarget->SetInteractionEnabled(bCanChoose);

	StartWaterMaskIfReady();
	if (WaterMaskBed)
	{
		if (bNightFour && Narrative->IsNightFourMaskRunning())
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
