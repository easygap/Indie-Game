#include "Entity/IGMissingFloorNightThreeDirector.h"

#include "Audio/IGAudioHelpers.h"
#include "Audio/IGMissingFloorAudioSubsystem.h"
#include "Audio/IGToneSequenceSoundWave.h"
#include "Components/AudioComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Core/IGPrologueWorldScene.h"
#include "Engine/GameInstance.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Entity/IGListenerEntity.h"
#include "Entity/IGMissingFloorEvidence.h"
#include "Entity/IGNoiseSubsystem.h"
#include "GameFramework/PlayerController.h"
#include "Interaction/IGReadableNote.h"
#include "Interaction/IGSwingDoor.h"
#include "Interaction/IGZoneTrigger.h"
#include "Materials/MaterialInterface.h"
#include "Narrative/IGMissingFloorNarrativeSubsystem.h"
#include "Narrative/IGStoryHelpers.h"
#include "Player/IGHorrorHUD.h"
#include "Player/IGPlayerCharacter.h"
#include "Player/IGStressComponent.h"
#include "TimerManager.h"

namespace IGNightThree
{
	// Two real leaves on the same roof slab. The route between their threshold
	// centers is 407.5 + 232.5 = 640 cm and is built by the world scene.
	const FVector StairGateHinge(-320.0f, 220.0f, 1200.0f);
	const FVector AnnexGateHinge(85.0f, 452.5f, 1200.0f);

	// Annex contents.
	// 자재 더미 위에 놓인 수첩이다. 세워 놓고 중심을 Z=1246에 두면 22cm
	// 높이의 절반이 더미 상판(Z=1242) 아래로 들어가 박힌다. 눕혀서 얹는다.
	const FVector NotebookLocation(-90.0f, 770.0f, 1242.7f);
	const FVector TuningHammerLocation(-40.0f, 755.0f, 1245.0f);
	/**
	 * §22.3 작업 장갑 한 짝. 자재 더미는 두 단이다 — 보드 단이 X -150..-10,
	 * 윗면 Z=1230이고 그 위에 스터드 단(X -120..-60, 윗면 Z=1242)이 얹혀
	 * 있다. 수첩과 렌치는 스터드 단 위에 있으므로, 장갑은 보드 단이 드러난
	 * 서쪽 턱에 둔다. 스터드 단과 3 cm 떨어진다.
	 */
	const FVector WorkGloveLocation(-135.0f, 768.0f, 1231.2f);
	const FVector TunerToolCartLocation(-280.0f, 865.0f, 1201.0f);
	const FVector ValveLocation(296.0f, 610.0f, 1266.0f);
	const FVector ImpactMarkLocation(-10.0f, 585.0f, 1264.0f);
	/**
	 * T5 「새 벽 미장 시기」의 높이. 귀·주먹 판정이 Z 1277..1303을 쓰므로
	 * 바닥 쪽 이음선으로 내린다. 칸 좌표는 WallBayYs를 그대로 쓴다.
	 */
	constexpr float PlasterDatingZ = 1220.0f;
	/**
	 * T8 「탱크 물소리 청음」. 탱크 몸통은 Y -5..125이고 점검 통로는 그
	 * 북쪽이다. 씬에 붙인 24×14cm 표찰과 같은 위치에 읽기 판정을 둔다.
	 */
	const FVector TankAuditionLocation(0.0f, 125.7f, 1340.0f);
	/**
	 * §13 12행. 채널 5는 「도면에 없는 복도, 천장 전구 하나, 바닥을 지나가는
	 * 낮은 형체」를 보여 준다. 그 화각에 실제로 서는 자리는 별관 철문을
	 * 지나 복도가 시작되는 지점이다. 부피만 있고 그리는 것이 없다.
	 */
	const FVector AnnexRecognitionCenter(120.0f, 520.0f, 1290.0f);
	const FVector AnnexRecognitionExtent(150.0f, 60.0f, 100.0f);
	const float WallBayYs[3] = {560.0f, 700.0f, 840.0f};
	constexpr int32 CavityBayIndex = 1;

	// The booth keyring, on the desk's west end.
	// 관리실 책상 위. (118, -96, 80)은 P2 민원 대장 정서본(X 108.6~130.8,
	// Y -118.3~-87.7) 한가운데였고 상판(Z=76)에도 1cm 박혀 있었다. 서로
	// 다른 디렉터가 같은 책상에 놓으면서 부딪혔다. 대장 앞쪽 빈자리로 뺀다.
	const FVector KeyringLocation(118.0f, -126.0f, 76.16f);

	// Day papers: the mover's labels in 403, the forum printout by the
	// mailboxes, the tally journal at 401's threshold once it is earned.
	// 같은 이유로 눕힌다. 13cm 높이를 세워 Z=978에 두면 가구 상판(Z=974)
	// 아래로 들어갔다.
	const FVector LabelsLocation(-95.0f, -185.0f, 974.6f);
	// 관리인에게 남긴 인쇄본은 게시판에 꽂는다. 우편 투입구는 비워 둔다.
	const FVector ForumLocation(643.0f, -148.65f, 166.0f);
	const FVector JournalLocation(-172.0f, -237.5f, 985.0f);

	// 주먹이 석고보드를 때리는 값은 플레이어 쪽이 든다 —
	// AIGPlayerCharacter::KnockLoudness. 귀를 대는 건 소리를 안 낸다.
	constexpr float ListenLoudness = 0.05f;
	constexpr float ValveLoudness = 0.55f;
	/**
	 * 주먹으로 벽을 읽는 값. 밸브보다 조용하면 급한 길이 조용한 길이 된다
	 * (§7 신중한 자에게는 물이, 급한 자에게는 추격이). 대답 노크는 손가락
	 * 마디 값 그대로다.
	 */
	constexpr float WallEchoKnockLoudness = 0.48f;

	/**
	 * The three authored wheels ring differently (§10.3 밸브 3종): 0 is the 5F
	 * shaft inspection valve here, 1 the large rooftop cleaning drain, 2 the
	 * small float bypass. A player who opened this one in night 3 recognises it
	 * is *not* what they are turning on the roof in night 4.
	 */
	constexpr int32 ShaftValveIndex = 0;
	/**
	 * §21.3 원근 4단. Out in the annex the riser comes through finished wall and
	 * arrives muffled; against the cavity wall there is only air between the
	 * player's ear and the pipe; against a solid wall the water took the long
	 * way through mass. The step is the whole distance cue.
	 */
	constexpr int32 RiserBedDistanceStep = 2;
	constexpr int32 CavityWallDistanceStep = 0;
	constexpr int32 SolidWallDistanceStep = 3;
	/** Ear against board: close enough that the wall itself dominates. */
	constexpr float WallListenInnerRadius = 90.0f;
	constexpr float WallListenFalloff = 620.0f;

	/** Eight seconds of nothing before the wall comes back. */
	constexpr float AnswerDelaySeconds = 8.0f;
	// P4의 벽과 복도의 맨손 노크는 같은 박자를 받아야 한다. 정의는 존재가
	// 가지고 있고 여기서는 참조만 한다 — 두 벌로 두면 언젠가 어긋난다.
	constexpr double AnswerPairMinSeconds =
		AIGListenerEntity::AnswerPairMinSeconds;
	constexpr double AnswerPairMaxSeconds =
		AIGListenerEntity::AnswerPairMaxSeconds;
	constexpr double AnswerRestMinSeconds =
		AIGListenerEntity::AnswerRestMinSeconds;
	constexpr double AnswerRestMaxSeconds =
		AIGListenerEntity::AnswerRestMaxSeconds;

	const FName PuzzleThreeId(TEXT("P3"));
	const FName PuzzleFourId(TEXT("P4"));

	// -- 비트 3-7 「귀환길」 ------------------------------------------------
	constexpr float FourthFloorZ = AIGPrologueWorldScene::FourthFloorZ;
	/**
	 * 4층 복도, 계단코어와 403호 문 사이. 복도는 Y -385..-225로 깊이가 160cm뿐이라
	 * 사람 하나를 지나치는 일이 실제로 좁다 — 그것이 이 비트의 전부다.
	 * 그녀는 서쪽(계단코어, X=-277.5)에서 내려와 동쪽 403호 문(X=101)으로 간다.
	 */
	const FVector ReturnPassPoint(-95.0f, -300.0f, FourthFloorZ);
	/** 서 있는 것이 아니라 기다리는 것으로 읽히도록 복도를 가로질러 조금 움직인다. */
	const FVector ReturnShufflePoint(-95.0f, -334.0f, FourthFloorZ);
	/** 그가 향한 쪽. 남쪽 벽에 귀를 대고 있다 — 1-4의 자세 그대로. */
	constexpr float ReturnPassYaw = -90.0f;
	/** 지나쳤다고 인정하는 여유. 몸 하나 폭보다 넉넉하게. */
	constexpr float PassClearanceCentimeters = 70.0f;
	/** 403호 실내는 4층 X -190..190, Y -235..235. */
	const FBox Unit403Interior(
		FVector(-190.0f, -235.0f, FourthFloorZ - 20.0f),
		FVector(190.0f, 235.0f, FourthFloorZ + 230.0f));
	constexpr float ReturnPollSeconds = 0.25f;

	const FName PassByBeatId(TEXT("Night3.PassBy"));
}

AIGMissingFloorNightThreeDirector::AIGMissingFloorNightThreeDirector()
{
	PrimaryActorTick.bCanEverTick = false;
}

bool AIGMissingFloorNightThreeDirector::Configure(
	AIGPrologueWorldScene* InScene,
	AIGListenerEntity* InEntity,
	AIGPlayerCharacter* InPlayer,
	const TArray<FVector>& InCorridorPatrolPoints)
{
	UWorld* World = GetWorld();
	if (!World || !InScene)
	{
		return false;
	}
	Scene = InScene;
	Entity = InEntity;
	Player = InPlayer;
	CorridorPatrolPoints = InCorridorPatrolPoints;

	UStaticMesh* CubeMesh =
		LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
	UStaticMesh* CylinderMesh =
		LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	if (!CubeMesh || !CylinderMesh)
	{
		return false;
	}

	// Seo appears once, across the alley, only after T7 and only by day. A
	// masked fixed-camera sprite is appropriate here because the player cannot
	// approach or circle him; all near people and all interactable evidence stay
	// 3D. Feet sit exactly on Z=0 and the masked card keeps a real cast shadow.
	if (UStaticMesh* PlaneMesh = LoadObject<UStaticMesh>(
			nullptr, TEXT("/Engine/BasicShapes/Plane.Plane")))
	{
		if (UMaterialInterface* SeoMaterial = LoadObject<UMaterialInterface>(
				nullptr,
				TEXT("/Game/Prototype/Materials/M_SpriteSeo.M_SpriteSeo")))
		{
			DistantSeo = NewObject<UStaticMeshComponent>(this, TEXT("DistantSeo"));
			DistantSeo->RegisterComponent();
			DistantSeo->SetStaticMesh(PlaneMesh);
			DistantSeo->SetMaterial(0, SeoMaterial);
			DistantSeo->SetWorldLocation(FVector(-40.0f, -835.0f, 90.0f));
			DistantSeo->SetWorldRotation(FRotator(0.0f, 0.0f, 90.0f));
			DistantSeo->SetWorldScale3D(FVector(0.86f, 1.80f, 1.0f));
			DistantSeo->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			DistantSeo->SetCanEverAffectNavigation(false);
			DistantSeo->SetCastShadow(true);
			DistantSeo->SetVisibility(false, true);
		}
	}

	FActorSpawnParameters SpawnParameters;
	SpawnParameters.SpawnCollisionHandlingOverride =
		ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	UMaterialInterface* DoorMaterial = LoadObject<UMaterialInterface>(
		nullptr,
		TEXT("/Game/Prototype/Materials/M_SteelDoorUV.M_SteelDoorUV"));
	UMaterialInterface* HandleMaterial = LoadObject<UMaterialInterface>(
		nullptr,
		TEXT("/Game/Prototype/Materials/M_StainlessUV.M_StainlessUV"));

	// The roof and annex leaves are separate physical locks. The state is the
	// persisted *keyring* possession; the prop and text make its two labelled
	// keys explicit instead of implying one magical master key.
	SpawnParameters.Name = TEXT("MissingFloorStairGate");
	StairGate = World->SpawnActor<AIGSwingDoor>(
		AIGSwingDoor::StaticClass(),
		FTransform(FRotator(0.0f, -90.0f, 0.0f), IGNightThree::StairGateHinge),
		SpawnParameters);
	if (!StairGate)
	{
		return false;
	}
	StairGate->ConfigurePrototypeVisuals(
		CubeMesh, DoorMaterial, HandleMaterial, FVector(6.0f, 85.0f, 205.0f));
	StairGate->SetOpenYaw(-95.0f);
	{
		TArray<FIGDoorRequirement> GateRequirements;
		FIGDoorRequirement& KeyLock = GateRequirements.AddDefaulted_GetRef();
		KeyLock.RequiredState = FGameplayTag::RequestGameplayTag(
			FName(TEXT("State.MissingFloor.HasStairKey")), false);
		KeyLock.LockedPrompt =
			NSLOCTEXT("IGMissingFloor", "StairGatePrompt", "옥상 철문 — 잠겨 있다");
		KeyLock.LockedThought = NSLOCTEXT(
			"IGMissingFloor",
			"StairGateThought",
			"`옥상` 표찰 열쇠가 필요하다.");
		StairGate->SetRequirements(MoveTemp(GateRequirements));
	}

	SpawnParameters.Name = TEXT("MissingFloorAnnexGate");
	AnnexGate = World->SpawnActor<AIGSwingDoor>(
		AIGSwingDoor::StaticClass(),
		FTransform(FRotator(0.0f, -90.0f, 0.0f), IGNightThree::AnnexGateHinge),
		SpawnParameters);
	if (!AnnexGate)
	{
		return false;
	}
	AnnexGate->ConfigurePrototypeVisuals(
		CubeMesh, DoorMaterial, HandleMaterial, FVector(6.0f, 90.0f, 205.0f));
	AnnexGate->SetOpenYaw(95.0f);
	{
		TArray<FIGDoorRequirement> GateRequirements;
		FIGDoorRequirement& KeyLock = GateRequirements.AddDefaulted_GetRef();
		KeyLock.RequiredState = FGameplayTag::RequestGameplayTag(
			FName(TEXT("State.MissingFloor.HasStairKey")), false);
		KeyLock.LockedPrompt = NSLOCTEXT(
			"IGMissingFloor", "AnnexGatePrompt", "5층 철문 — 잠겨 있다");
		KeyLock.LockedThought = NSLOCTEXT(
			"IGMissingFloor", "AnnexGateThought", "`창고` 표찰 열쇠가 필요하다.");
		AnnexGate->SetRequirements(MoveTemp(GateRequirements));
	}
	float RouteLengthCentimeters = 0.0f;
	int32 UpperStepCount = 0;
	if (!Scene->ValidateMissingFloorRooftopRoute(
			RouteLengthCentimeters,
			UpperStepCount))
	{
		UE_LOG(
			LogTemp,
			Error,
			TEXT(
				"Missing-floor roof route invalid: length=%.1f steps=%d "
				"(expected 640.0/14)."),
			RouteLengthCentimeters,
			UpperStepCount);
		return false;
	}

	// 관리실 상판에 내려 둔 금속 열쇠 두 개. 이름표까지 같은 메시로 반입한다.
	SpawnParameters.Name = TEXT("MissingFloorKeyring");
	Keyring = World->SpawnActor<AIGMissingFloorEvidence>(
		AIGMissingFloorEvidence::StaticClass(),
		FTransform(FRotator::ZeroRotator, IGNightThree::KeyringLocation),
		SpawnParameters);
	if (!Keyring)
	{
		return false;
	}
	UStaticMesh* KeyringMesh = LoadObject<UStaticMesh>(
		nullptr, TEXT("/Game/Meshes/SM_BoothKeyring.SM_BoothKeyring"));
	Keyring->Configure(
		KeyringMesh,
		LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Prototype/Materials/MI_BoothKeyring.MI_BoothKeyring")),
		FVector::ZeroVector,
		NSLOCTEXT("IGMissingFloor", "KeyringPrompt", "옥상·창고 열쇠뭉치"),
		NSLOCTEXT(
			"IGMissingFloor",
			"KeyringThought",
			"옥상, 창고. 열쇠를 챙겼다."),
		EIGMissingFloorTruth::None,
		EIGMissingFloorSource::None,
		0.0f,
		0.1f);
	Keyring->OnExamined.AddUObject(
		this, &AIGMissingFloorNightThreeDirector::HandleKeyringTaken);
	RefreshKeyringAvailability();

	// -- annex contents ----------------------------------------------------

	SpawnParameters.Name = TEXT("MissingFloorTunerNotebook");
	TunerNotebook = World->SpawnActor<AIGReadableNote>(
		AIGReadableNote::StaticClass(),
		FTransform(FRotator::ZeroRotator, IGNightThree::NotebookLocation),
		SpawnParameters);
	if (!TunerNotebook)
	{
		return false;
	}
	// 5층 별관에 놓이는 종이와 금속. 비워 두면 엔진 기본 격자가 그대로 보인다.
	UMaterialInterface* AgedPaperMaterial = LoadObject<UMaterialInterface>(
		nullptr, TEXT("/Game/Prototype/Materials/M_PaperOld.M_PaperOld"));
	UMaterialInterface* FreshPaperMaterial = LoadObject<UMaterialInterface>(
		nullptr, TEXT("/Game/Prototype/Materials/M_PaperClean.M_PaperClean"));
	UMaterialInterface* AnnexMetalMaterial = LoadObject<UMaterialInterface>(
		nullptr, TEXT("/Game/Prototype/Materials/M_MetalFrame.M_MetalFrame"));

	TunerNotebook->ConfigurePrototypeVisuals(
		CubeMesh, AgedPaperMaterial, FVector(16.0f, 22.0f, 1.4f));
	TunerNotebook->SetInteractionPrompt(
		NSLOCTEXT("IGMissingFloor", "NotebookPrompt", "조율 수첩"));
	TunerNotebook->SetNoteText(
		NSLOCTEXT("IGMissingFloor", "NotebookTitle", "조율 수첩 — 백도하"),
		{
			NSLOCTEXT("IGMissingFloor", "Notebook1", "월  서초 공연장  ·  공연 끝나고 조율 02:30"),
			NSLOCTEXT("IGMissingFloor", "Notebook2", "수  대치동 학원  ·  업라이트 네 대  ·  22시 이후 출입"),
			FText::GetEmpty(),
			NSLOCTEXT("IGMissingFloor", "Notebook3", "옥탑 벽 확인: 빈 곳은 낮게 울리고 소리가 오래 감."),
			NSLOCTEXT("IGMissingFloor", "Notebook4", "기둥 있는 쪽은 짧고 둔함. 배관 멈추면 다시 확인."),
			FText::GetEmpty(),
			NSLOCTEXT("IGMissingFloor", "Notebook5", "●●  —  ●"),
			NSLOCTEXT("IGMissingFloor", "Notebook6", "토요일 11시  ·  유담 연습실 예약"),
			FText::GetEmpty(),
			NSLOCTEXT("IGMissingFloor", "Notebook7", "유담 피아노 적금  180만 / 200만"),
		});
	TunerNotebook->OnReadStateChanged.AddDynamic(
		this, &AIGMissingFloorNightThreeDirector::HandleNotebookRead);

	SpawnParameters.Name = TEXT("MissingFloorTuningHammer");
	TuningHammer = World->SpawnActor<AIGMissingFloorEvidence>(
		AIGMissingFloorEvidence::StaticClass(),
		FTransform(
			FRotator(0.0f, 0.0f, 90.0f),
			IGNightThree::TuningHammerLocation),
		SpawnParameters);
	if (!TuningHammer)
	{
		return false;
	}
	UStaticMesh* TuningHammerMesh = LoadObject<UStaticMesh>(
		nullptr, TEXT("/Game/Meshes/SM_TuningHammer.SM_TuningHammer"));
	UMaterialInterface* TuningHammerMaterial = LoadObject<UMaterialInterface>(
		nullptr, TEXT("/Game/Prototype/Materials/M_MetalFrame.M_MetalFrame"));
	TuningHammer->Configure(
		TuningHammerMesh ? TuningHammerMesh : CylinderMesh,
		TuningHammerMesh ? TuningHammerMaterial : nullptr,
		// 같은 이유로 0이다. 저작된 조율 렌치는 3 x 3 x 26cm다.
		TuningHammerMesh
			? FVector::ZeroVector
			: FVector(3.0f, 3.0f, 26.0f),
		NSLOCTEXT("IGMissingFloor", "TuningHammerPrompt", "조율 렌치"),
		NSLOCTEXT(
			"IGMissingFloor",
			"TuningHammerThought",
			"오빠 조율 렌치다. 왜 여기 떨어져 있어."),
		EIGMissingFloorTruth::None,
		EIGMissingFloorSource::None,
		0.0f,
		0.06f);
	TuningHammer->OnExamined.AddUObject(
		this, &AIGMissingFloorNightThreeDirector::HandleTuningHammerExamined);

	// §22.3. 도하의 손은 이 크기가 아니다. 진실을 열지는 않는다 — 누구
	// 것인지는 이미 T5가 말하고, 이건 그 사람이 여기 있었다는 감각이다.
	SpawnParameters.Name = TEXT("MissingFloorWorkGlove");
	WorkGlove = World->SpawnActor<AIGMissingFloorEvidence>(
		AIGMissingFloorEvidence::StaticClass(),
		FTransform(
			FRotator(0.0f, 24.0f, 0.0f),
			IGNightThree::WorkGloveLocation),
		SpawnParameters);
	if (WorkGlove)
	{
		WorkGlove->Configure(
			CubeMesh,
			AgedPaperMaterial,
			FVector(24.0f, 11.0f, 2.4f),
			NSLOCTEXT("IGMissingFloor", "WorkGlovePrompt", "작업 장갑 한 짝"),
			NSLOCTEXT(
				"IGMissingFloor",
				"WorkGloveThought",
				"장갑에 석고가 굳어 있다. 오빠가 쓰던 건 이것보다 작았는데."),
			EIGMissingFloorTruth::None,
			EIGMissingFloorSource::None,
			0.9f,
			0.05f);
		WorkGlove->OnExamined.AddUObject(
			this, &AIGMissingFloorNightThreeDirector::HandleWorkGloveExamined);
	}

	// A close prop must preserve parallax, contact shadow and the gap beneath
	// its shelves.  The ImageGen sheet is only the shape reference; the runtime
	// object is a real 45 x 34 x 78 cm mesh resting on the annex slab.
	if (UStaticMesh* CartMesh = LoadObject<UStaticMesh>(
			nullptr, TEXT("/Game/Meshes/SM_TunerToolCart.SM_TunerToolCart")))
	{
		TunerToolCart = NewObject<UStaticMeshComponent>(
			this, TEXT("TunerToolCart"));
		TunerToolCart->RegisterComponent();
		TunerToolCart->SetStaticMesh(CartMesh);
		TunerToolCart->SetMaterial(0, TuningHammerMaterial);
		TunerToolCart->SetWorldLocation(IGNightThree::TunerToolCartLocation);
		TunerToolCart->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		TunerToolCart->SetCollisionResponseToAllChannels(ECR_Block);
		TunerToolCart->SetCanEverAffectNavigation(false);
		TunerToolCart->SetCastShadow(true);
	}

	SpawnParameters.Name = TEXT("MissingFloorRiserValve");
	RiserValve = World->SpawnActor<AIGMissingFloorEvidence>(
		AIGMissingFloorEvidence::StaticClass(),
		FTransform(FRotator(90.0f, 0.0f, 0.0f), IGNightThree::ValveLocation),
		SpawnParameters);
	if (!RiserValve)
	{
		return false;
	}
	RiserValve->Configure(
		CylinderMesh,
		AnnexMetalMaterial,
		FVector(16.0f, 16.0f, 5.0f),
		NSLOCTEXT("IGMissingFloor", "ValvePrompt", "배관 밸브 — 연다"),
		FText::GetEmpty(),
		EIGMissingFloorTruth::None,
		EIGMissingFloorSource::None,
		0.8f,
		IGNightThree::ValveLoudness);
	RiserValve->OnExamined.AddUObject(
		this, &AIGMissingFloorNightThreeDirector::HandleValveOpened);

	SpawnParameters.Name = TEXT("MissingFloorImpactMark");
	ImpactMark = World->SpawnActor<AIGMissingFloorEvidence>(
		AIGMissingFloorEvidence::StaticClass(),
		FTransform(FRotator::ZeroRotator, IGNightThree::ImpactMarkLocation),
		SpawnParameters);
	if (!ImpactMark)
	{
		return false;
	}
	ImpactMark->Configure(
		CubeMesh,
		nullptr,
		FVector(22.0f, 10.0f, 6.0f),
		NSLOCTEXT("IGMissingFloor", "ImpactPrompt", "자재 모서리 — 얼룩"),
		NSLOCTEXT(
			"IGMissingFloor",
			"ImpactThought",
			"모서리가 검게 물들었다. 바닥에도 같은 얼룩이 있다."),
		EIGMissingFloorTruth::LandingStruggle,
		EIGMissingFloorSource::LandingImpactMark,
		0.0f,
		0.05f,
		/*bPresentationVisible=*/false);
	ImpactMark->OnExamined.AddUObject(
		this, &AIGMissingFloorNightThreeDirector::HandleImpactMarkExamined);

	// Per-bay verbs: an ear (quiet, needs the water) and a fist (loud,
	// needs nothing but nerve). Same answer, two prices — §7's promise.
	WallListens.SetNum(3);
	WallKnocks.SetNum(3);
	for (int32 BayIndex = 0; BayIndex < 3; ++BayIndex)
	{
		const float BayY = IGNightThree::WallBayYs[BayIndex];

		SpawnParameters.Name = *FString::Printf(
			TEXT("MissingFloorWallListen%d"), BayIndex);
		AIGMissingFloorEvidence* Listen =
			World->SpawnActor<AIGMissingFloorEvidence>(
				AIGMissingFloorEvidence::StaticClass(),
				FTransform(
					FRotator::ZeroRotator,
					FVector(247.0f, BayY - 26.0f, 1290.0f)),
				SpawnParameters);
		if (!Listen)
		{
			return false;
		}
		Listen->Configure(
			CubeMesh,
			nullptr,
			FVector(3.0f, 26.0f, 26.0f),
			NSLOCTEXT("IGMissingFloor", "WallListenPrompt", "벽 — 귀를 댄다"),
			FText::GetEmpty(),
			EIGMissingFloorTruth::None,
			EIGMissingFloorSource::None,
			0.8f,
			IGNightThree::ListenLoudness,
			/*bPresentationVisible=*/false);
		Listen->Tags.AddUnique(FName(TEXT("MissingFloor.Verb.Listen")));
		Listen->OnExamined.AddWeakLambda(
			this,
			[this, BayIndex](AIGMissingFloorEvidence*)
			{
				HandleWallListened(BayIndex);
			});
		WallListens[BayIndex] = Listen;

		SpawnParameters.Name = *FString::Printf(
			TEXT("MissingFloorWallKnock%d"), BayIndex);
		AIGMissingFloorEvidence* Knock =
			World->SpawnActor<AIGMissingFloorEvidence>(
				AIGMissingFloorEvidence::StaticClass(),
				FTransform(
					FRotator::ZeroRotator,
					FVector(247.0f, BayY + 26.0f, 1290.0f)),
				SpawnParameters);
		if (!Knock)
		{
			return false;
		}
		Knock->Configure(
			CubeMesh,
			nullptr,
			FVector(3.0f, 26.0f, 26.0f),
			NSLOCTEXT("IGMissingFloor", "WallKnockPrompt", "벽 — 두드린다"),
			FText::GetEmpty(),
			EIGMissingFloorTruth::None,
			EIGMissingFloorSource::None,
			0.0f,
			IGNightThree::WallEchoKnockLoudness,
			/*bPresentationVisible=*/false);
		Knock->Tags.AddUnique(FName(TEXT("MissingFloor.Verb.Knock")));
		WallKnocks[BayIndex] = Knock;
	}

	// P4's surface on the cavity bay: hidden until the wall is certain and two
	// independent rhythm clues are known.  The prompt exposes only the verb;
	// three player-timed taps, not a hold that auto-solves the pattern, are the
	// actual answer.
	SpawnParameters.Name = TEXT("MissingFloorAnswerTarget");
	AnswerTarget = World->SpawnActor<AIGMissingFloorEvidence>(
		AIGMissingFloorEvidence::StaticClass(),
		FTransform(
			FRotator::ZeroRotator,
			FVector(247.0f, IGNightThree::WallBayYs[IGNightThree::CavityBayIndex], 1266.0f)),
		SpawnParameters);
	if (!AnswerTarget)
	{
		return false;
	}
	AnswerTarget->Configure(
		CubeMesh,
		nullptr,
		FVector(3.0f, 40.0f, 20.0f),
		NSLOCTEXT(
			"IGMissingFloor",
			"AnswerPrompt",
			"벽 — 신호를 보낸다"),
		FText::GetEmpty(),
		EIGMissingFloorTruth::None,
		EIGMissingFloorSource::None,
		0.0f,
		AIGPlayerCharacter::KnockLoudness,
		/*bPresentationVisible=*/false);
	AnswerTarget->Tags.AddUnique(FName(TEXT("MissingFloor.Verb.Knock")));

	// T5의 두 번째 출처. 영수증이 「언제 실어 왔는가」라면 이쪽은 「언제
	// 발랐는가」다. 둘이 같은 주를 가리켜야 은폐가 확정된다(§12).
	PlasterDatings.SetNum(3);
	for (int32 BayIndex = 0; BayIndex < 3; ++BayIndex)
	{
		SpawnParameters.Name = *FString::Printf(
			TEXT("MissingFloorPlasterDating%d"), BayIndex);
		AIGMissingFloorEvidence* Dating =
			World->SpawnActor<AIGMissingFloorEvidence>(
				AIGMissingFloorEvidence::StaticClass(),
				FTransform(
					FRotator::ZeroRotator,
					FVector(
						247.0f,
						IGNightThree::WallBayYs[BayIndex],
						IGNightThree::PlasterDatingZ)),
				SpawnParameters);
		if (!Dating)
		{
			return false;
		}
		Dating->Configure(
			CubeMesh,
			nullptr,
			FVector(3.0f, 20.0f, 26.0f),
			NSLOCTEXT("IGMissingFloor", "PlasterDatingPrompt", "새 벽 — 이음선"),
			FText::GetEmpty(),
			EIGMissingFloorTruth::None,
			EIGMissingFloorSource::None,
			1.0f,
			IGNightThree::ListenLoudness,
			/*bPresentationVisible=*/false);
		Dating->OnExamined.AddUObject(
			this, &AIGMissingFloorNightThreeDirector::HandlePlasterDatingExamined);
		PlasterDatings[BayIndex] = Dating;
	}

	// §13 12행의 회수. 밤2에 모니터로 본 화각에 직접 서는 순간이다.
	SpawnParameters.Name = TEXT("MissingFloorAnnexRecognitionZone");
	AnnexRecognitionZone = World->SpawnActor<AIGZoneTrigger>(
		AIGZoneTrigger::StaticClass(),
		FTransform(FRotator::ZeroRotator, IGNightThree::AnnexRecognitionCenter),
		SpawnParameters);
	if (!AnnexRecognitionZone)
	{
		return false;
	}
	AnnexRecognitionZone->SetZoneExtent(IGNightThree::AnnexRecognitionExtent);
	AnnexRecognitionZone->OnZoneTriggered.AddDynamic(
		this, &AIGMissingFloorNightThreeDirector::HandleAnnexRecognitionZone);

	// T8의 두 번째 출처. 일지가 두드린 횟수를 세었다면 이쪽은 그 옆에
	// 무엇이 있었는지를 말한다.
	SpawnParameters.Name = TEXT("MissingFloorTankAudition");
	TankAudition = World->SpawnActor<AIGMissingFloorEvidence>(
		AIGMissingFloorEvidence::StaticClass(),
		FTransform(FRotator::ZeroRotator, IGNightThree::TankAuditionLocation),
		SpawnParameters);
	if (!TankAudition)
	{
		return false;
	}
	TankAudition->Configure(
		CubeMesh,
		nullptr,
		FVector(24.0f, 1.0f, 14.0f),
		NSLOCTEXT("IGMissingFloor", "TankAuditionPrompt", "저수조 살펴보기"),
		FText::GetEmpty(),
		EIGMissingFloorTruth::None,
		EIGMissingFloorSource::None,
		1.0f,
		IGNightThree::ListenLoudness,
		/*bPresentationVisible=*/false);
	TankAudition->Tags.AddUnique(FName(TEXT("MissingFloor.Verb.Listen")));
	TankAudition->OnExamined.AddUObject(
		this, &AIGMissingFloorNightThreeDirector::HandleTankAuditionExamined);
	AnswerTarget->SetInteractionEnabled(false);
	AnswerTarget->SetActorHiddenInGame(true);

	// -- day papers --------------------------------------------------------

	SpawnParameters.Name = TEXT("MissingFloorLabelsNote");
	LabelsNote = World->SpawnActor<AIGReadableNote>(
		AIGReadableNote::StaticClass(),
		FTransform(FRotator::ZeroRotator, IGNightThree::LabelsLocation),
		SpawnParameters);
	if (!LabelsNote)
	{
		return false;
	}
	LabelsNote->ConfigurePrototypeVisuals(
		CubeMesh, FreshPaperMaterial, FVector(18.0f, 13.0f, 1.2f));
	LabelsNote->SetInteractionPrompt(
		NSLOCTEXT("IGMissingFloor", "LabelsPrompt", "배송 라벨 뭉치"));
	LabelsNote->SetNoteText(
		NSLOCTEXT("IGMissingFloor", "LabelsTitle", "공방 창고에서 온 상자"),
		{
			NSLOCTEXT("IGMissingFloor", "Labels1", "받는 사람: 백도하"),
			NSLOCTEXT("IGMissingFloor", "Labels2", "무영로 27-3  달빛빌라 옥탑"),
			FText::GetEmpty(),
			NSLOCTEXT(
				"IGMissingFloor",
				"Labels3",
				"배송 요청: 부재 시 앞쪽 편의점 보관"),
		});
	LabelsNote->OnReadStateChanged.AddDynamic(
		this, &AIGMissingFloorNightThreeDirector::HandleLabelsRead);

	SpawnParameters.Name = TEXT("MissingFloorForumNote");
	ForumNote = World->SpawnActor<AIGReadableNote>(
		AIGReadableNote::StaticClass(),
		FTransform(FRotator::ZeroRotator, IGNightThree::ForumLocation),
		SpawnParameters);
	if (!ForumNote)
	{
		return false;
	}
	ForumNote->ConfigurePrototypeVisuals(
		CubeMesh, LoadObject<UMaterialInterface>(nullptr,
			TEXT("/Game/Prototype/Materials/M_LobbyForumPrint.M_LobbyForumPrint")), FVector(21.0f, 0.08f, 29.7f));
	ForumNote->SetInteractionPrompt(
		NSLOCTEXT("IGMissingFloor", "ForumPrompt", "소음 민원 자료"));
	ForumNote->SetNoteText(
		NSLOCTEXT("IGMissingFloor", "ForumTitle", "관리인에게 남긴 인쇄본"),
		{
			NSLOCTEXT("IGMissingFloor", "Forum1", "6/30  새벽 네 시만 되면 위에서 뭘 질질 끕니다. 자다가 매번 깨요."),
			NSLOCTEXT("IGMissingFloor", "Forum2", "7/12  관리인은 창고라 사람이 없대요. 그럼 이 소리는 어디서 나는 건가요?"),
			FText::GetEmpty(),
			NSLOCTEXT("IGMissingFloor", "Forum3", "7/26  03:12  또 시작됐네요. 오늘은 직접 올라가 보려고요."),
			NSLOCTEXT("IGMissingFloor", "Forum4", "댓글  ·  혼자 가서 싸우진 마시고, 일단 녹음해 두세요."),
		});
	ForumNote->OnReadStateChanged.AddDynamic(
		this, &AIGMissingFloorNightThreeDirector::HandleForumRead);

	SpawnParameters.Name = TEXT("MissingFloorJournalNote");
	JournalNote = World->SpawnActor<AIGReadableNote>(
		AIGReadableNote::StaticClass(),
		FTransform(FRotator::ZeroRotator, IGNightThree::JournalLocation),
		SpawnParameters);
	if (!JournalNote)
	{
		return false;
	}
	UStaticMesh* CalendarJournalMesh = LoadObject<UStaticMesh>(
		nullptr,
		TEXT("/Game/Meshes/SM_CalendarJournal.SM_CalendarJournal"));
	UMaterialInterface* JournalMaterial = LoadObject<UMaterialInterface>(
		nullptr, TEXT("/Game/Prototype/Materials/M_PaperOld.M_PaperOld"));
	JournalNote->ConfigurePrototypeVisuals(
		CalendarJournalMesh ? CalendarJournalMesh : CubeMesh,
		CalendarJournalMesh ? JournalMaterial : nullptr,
		CalendarJournalMesh
			? FVector(100.0f, 100.0f, 100.0f)
			: FVector(17.0f, 1.4f, 24.0f),
		CalendarJournalMesh != nullptr);
	JournalNote->SetInteractionPrompt(
		NSLOCTEXT("IGMissingFloor", "JournalPrompt", "달력 뒷장 소리 일지"));
	JournalNote->SetNoteText(
		NSLOCTEXT("IGMissingFloor", "JournalTitle", "황순금의 일지"),
		{
			NSLOCTEXT("IGMissingFloor", "Journal1", "7/27  네 시 반쯤 또 깸. 위에서 다섯 번. 관리실 전화 안 받음."),
			NSLOCTEXT("IGMissingFloor", "Journal2", "7/28  어제랑 같은 소리. 오늘은 좀 작았음."),
			NSLOCTEXT("IGMissingFloor", "Journal3", "7/29  벽을 두드려 봄. 저쪽에서도 한 번. 사람 있는 것 아닌지 다시 물어볼 것."),
			NSLOCTEXT("IGMissingFloor", "Journal4", "7/30  네 번 들음. 수도 틀자 안 들림."),
			NSLOCTEXT("IGMissingFloor", "Journal5", "7/31  오늘은 세 번. 귀가 먹은 건지."),
			FText::GetEmpty(),
			NSLOCTEXT("IGMissingFloor", "Journal6", "8/1"),
		});
	JournalNote->OnReadStateChanged.AddDynamic(
		this, &AIGMissingFloorNightThreeDirector::HandleJournalRead);
	// Earned, not found: she hands it out only after the truth of the
	// knocking is known, and only by daylight.
	RefreshJournalAvailability(/*bHourActive=*/true);

	if (UIGMissingFloorNarrativeSubsystem* Narrative = GetNarrative())
	{
		// The opening phone transcript is guaranteed starting knowledge. It is
		// still recorded as provenance so P4 needs one more independent clue,
		// instead of treating the protagonist's memory as invisible permission.
		Narrative->RegisterTruthSource(
			EIGMissingFloorTruth::WaitingForAnAnswer,
			EIGMissingFloorSource::AnswerRhythmVoicemail);
		TruthHandle = Narrative->OnTruthConfirmed.AddUObject(
			this, &AIGMissingFloorNightThreeDirector::HandleTruthConfirmed);
	}
	return true;
}

bool AIGMissingFloorNightThreeDirector::IsPlayerKnockTarget(
	const AActor* FocusedActor) const
{
	if (!IsValid(FocusedActor))
	{
		return false;
	}
	if (FocusedActor == AnswerTarget
		&& AnswerTarget
		&& AnswerTarget->IsInteractionEnabled()
		&& !AnswerTarget->IsHidden())
	{
		return true;
	}
	for (const AIGMissingFloorEvidence* KnockTarget : WallKnocks)
	{
		if (FocusedActor == KnockTarget
			&& IsValid(KnockTarget)
			&& KnockTarget->IsInteractionEnabled()
			&& !KnockTarget->IsHidden())
		{
			return true;
		}
	}
	return false;
}

bool AIGMissingFloorNightThreeDirector::IsPlayerListenTarget(
	const AActor* FocusedActor) const
{
	if (!IsValid(FocusedActor))
	{
		return false;
	}
	for (const AIGMissingFloorEvidence* ListenTarget : WallListens)
	{
		if (FocusedActor == ListenTarget
			&& IsValid(ListenTarget)
			&& ListenTarget->IsInteractionEnabled()
			&& !ListenTarget->IsHidden())
		{
			return true;
		}
	}
	return false;
}

bool AIGMissingFloorNightThreeDirector::TryPlayerKnock(
	AActor* FocusedActor,
	AActor* NoiseInstigator)
{
	if (!IsPlayerKnockTarget(FocusedActor))
	{
		return false;
	}

	if (UWorld* World = GetWorld())
	{
		if (UIGNoiseSubsystem* Noise = World->GetSubsystem<UIGNoiseSubsystem>())
		{
			// 대답 노크는 손가락 마디, 벽을 읽는 주먹은 그보다 크다.
			Noise->ReportNoise(
				FocusedActor->GetActorLocation(),
				FocusedActor == AnswerTarget
					? AIGPlayerCharacter::KnockLoudness
					: IGNightThree::WallEchoKnockLoudness,
				NoiseInstigator);
		}
	}

	if (FocusedActor == AnswerTarget)
	{
		HandleAnswerKnock(AnswerTarget);
		return true;
	}
	for (int32 BayIndex = 0; BayIndex < WallKnocks.Num(); ++BayIndex)
	{
		if (FocusedActor == WallKnocks[BayIndex])
		{
			HandleWallKnocked(BayIndex);
			return true;
		}
	}
	return false;
}

bool AIGMissingFloorNightThreeDirector::TryPlayerListen(
	AActor* FocusedActor,
	AActor* NoiseInstigator)
{
	if (!IsPlayerListenTarget(FocusedActor))
	{
		return false;
	}

	if (UWorld* World = GetWorld())
	{
		if (UIGNoiseSubsystem* Noise = World->GetSubsystem<UIGNoiseSubsystem>())
		{
			Noise->ReportNoise(
				FocusedActor->GetActorLocation(),
				IGNightThree::ListenLoudness,
				NoiseInstigator);
		}
	}
	for (int32 BayIndex = 0; BayIndex < WallListens.Num(); ++BayIndex)
	{
		if (FocusedActor == WallListens[BayIndex])
		{
			HandleWallListened(BayIndex);
			return true;
		}
	}
	return false;
}

void AIGMissingFloorNightThreeDirector::EndPlay(
	const EEndPlayReason::Type EndPlayReason)
{
	GetWorldTimerManager().ClearTimer(AnswerTimer);
	GetWorldTimerManager().ClearTimer(AnswerSilenceReleaseTimer);
	GetWorldTimerManager().ClearTimer(ReturnTimer);
	ReleaseReturnFigure();
	if (UWorld* World = GetWorld())
	{
		if (UIGMissingFloorAudioSubsystem* AudioDirector =
			World->GetSubsystem<UIGMissingFloorAudioSubsystem>())
		{
			AudioDirector->SetAuthoredSilence(false);
		}
	}
	if (UIGMissingFloorNarrativeSubsystem* Narrative = GetNarrative())
	{
		Narrative->OnTruthConfirmed.Remove(TruthHandle);
	}
	if (RiserFlow)
	{
		RiserFlow->Stop();
	}
	Super::EndPlay(EndPlayReason);
}

void AIGMissingFloorNightThreeDirector::SetHourActive(const bool bHourActive)
{
	RefreshKeyringAvailability();
	bHourCurrentlyActive = bHourActive;
	if (!bHourActive)
	{
		GetWorldTimerManager().ClearTimer(AnswerSilenceReleaseTimer);
		EndAnswerSilence();
		// 새벽은 어떻게 왔든 복도를 비운다. 시간 초과로 왔다면 그를 낮의
		// 복도에 세워 둔 채로 남기면 안 된다.
		GetWorldTimerManager().ClearTimer(ReturnTimer);
		ReleaseReturnFigure();
		if (ReturnStage != EIGNightThreeReturnStage::Home)
		{
			ReturnStage = EIGNightThreeReturnStage::Idle;
		}
	}
	RefreshJournalAvailability(bHourActive);
	RefreshDistantSeoVisibility();
}

AIGMissingFloorEvidence* AIGMissingFloorNightThreeDirector::GetWallListen(
	const int32 BayIndex) const
{
	return WallListens.IsValidIndex(BayIndex) ? WallListens[BayIndex] : nullptr;
}

bool AIGMissingFloorNightThreeDirector::GetCavityWallObservationPoint(
	FVector& OutLocation) const
{
	const AIGMissingFloorEvidence* CavityListen =
		GetWallListen(IGNightThree::CavityBayIndex);
	if (!CavityListen)
	{
		return false;
	}
	if (const UIGMissingFloorNarrativeSubsystem* Narrative = GetNarrative())
	{
		if (Narrative->IsPuzzleSolved(IGNightThree::PuzzleThreeId))
		{
			return false;
		}
	}
	// A little out from the face, so he ends up beside the wall rather than
	// inside it, and the player sees a body against a surface.
	OutLocation = CavityListen->GetActorLocation() - FVector(70.0f, 0.0f, 0.0f);
	return true;
}

void AIGMissingFloorNightThreeDirector::RefreshKeyringAvailability()
{
	if (!Keyring) { return; }
	const bool bTaken = IGStory::HasState(this, FGameplayTag::RequestGameplayTag(
		FName(TEXT("State.MissingFloor.HasStairKey")), false));
	Keyring->SetActorHiddenInGame(bTaken);
	Keyring->SetActorEnableCollision(!bTaken);
	Keyring->SetInteractionEnabled(!bTaken);
}

void AIGMissingFloorNightThreeDirector::HandleKeyringTaken(
	AIGMissingFloorEvidence* Evidence)
{
	// A key is a persistent fact: the tag is registered, granted once, and
	// saved — the gate's requirement reads it from then on.
	IGStory::AddState(
		this,
		FGameplayTag::RequestGameplayTag(
			FName(TEXT("State.MissingFloor.HasStairKey")), false));
	RefreshKeyringAvailability();
	// 아직 읽지 않았다면 옆에 놓인 문자 사본을 짚어 준다. 밤2부터 있는 종이다.
	if (const UIGMissingFloorNarrativeSubsystem* Narrative = GetNarrative())
	{
		if (!Narrative->HasSource(
			EIGMissingFloorTruth::WasStillAlive,
			EIGMissingFloorSource::AgentMoveOutMessage))
		{
			AIGHorrorHUD::PushThought(
				this,
				NSLOCTEXT(
					"IGMissingFloor",
					"KeyringPaperThought",
					"열쇠 옆에 부동산에서 보낸 문자가 있다."),
				3.8f);
		}
	}
}

void AIGMissingFloorNightThreeDirector::HandleValveOpened(
	AIGMissingFloorEvidence* Evidence)
{
	if (bValveOpen)
	{
		return;
	}
	bValveOpen = true;

	if (Evidence)
	{
		Evidence->SetInteractionPrompt(
			NSLOCTEXT("IGMissingFloor", "ValveOpenPrompt", "배관 밸브 — 열려 있다"));
	}
	IGAudio::SpawnOneShotAt(
		this,
		UIGToneSequenceSoundWave::CreateValveOpen(
			this,
			IGNightThree::ShaftValveIndex),
		IGNightThree::ValveLocation,
		0.7f,
		1.0f,
		160.0f,
		1400.0f,
		EIGAudioBus::Puzzle);

	// Water starts moving behind exactly one of three identical walls.
	if (UWorld* World = GetWorld())
	{
		RiserFlow = NewObject<UAudioComponent>(this, TEXT("RiserFlowBed"));
		RiserFlow->RegisterComponent();
		RiserFlow->SetWorldLocation(FVector(
			310.0f,
			IGNightThree::WallBayYs[IGNightThree::CavityBayIndex],
			1300.0f));
		// From out in the annex the riser is heard through finished wall, so it
		// arrives at the third band — present, but not yet locatable. Standing
		// at a wall and listening is what opens the band up (§21.3 원근 4단).
		RiserFlow->SetSound(
			UIGToneSequenceSoundWave::CreatePipeWaterFlow(
				this,
				IGNightThree::RiserBedDistanceStep));
		RiserFlow->AttenuationSettings = IGAudio::MakeAttenuation(
			this,
			120.0f,
			900.0f,
			EIGAudioBus::Puzzle);
		RiserFlow->bAllowSpatialization = true;
		RiserFlow->SetVolumeMultiplier(0.5f);
		if (UIGMissingFloorAudioSubsystem* AudioDirector =
			World->GetSubsystem<UIGMissingFloorAudioSubsystem>())
		{
			AudioDirector->RegisterComponent(RiserFlow, EIGAudioBus::Puzzle);
		}
		RiserFlow->Play();
	}

	AIGHorrorHUD::PushThought(
		this,
		NSLOCTEXT(
			"IGMissingFloor",
			"ValveThought",
			"물이 내려간다. 이제 벽에 귀를 대 보자."),
		3.8f);
}

void AIGMissingFloorNightThreeDirector::PlayWallListenResponse(
	const int32 BayIndex,
	const bool bHollow)
{
	const FVector WallLocation =
		WallListens.IsValidIndex(BayIndex) && WallListens[BayIndex]
			? WallListens[BayIndex]->GetActorLocation()
			: GetActorLocation();

	// The wall's own answer. This is the puzzle: a cavity rings on its
	// mass-air-mass note, a solid wall dies in a fifth of a second. Doha's memo
	// says 속이 찬 벽은 짧게 죽고 빈 벽은 길게 운다, and now that is literally
	// what the two walls do.
	IGAudio::SpawnOneShotAt(
		this,
		UIGToneSequenceSoundWave::CreateWallCavityResponse(this, bHollow),
		WallLocation,
		0.92f,
		1.0f,
		IGNightThree::WallListenInnerRadius,
		IGNightThree::WallListenFalloff,
		EIGAudioBus::Puzzle);

	// And the water, at the band the structure left it in.
	if (bValveOpen)
	{
		IGAudio::SpawnOneShotAt(
			this,
			UIGToneSequenceSoundWave::CreatePipeWaterFlow(
				this,
				bHollow
					? IGNightThree::CavityWallDistanceStep
					: IGNightThree::SolidWallDistanceStep),
			WallLocation,
			bHollow ? 0.80f : 0.58f,
			1.0f,
			IGNightThree::WallListenInnerRadius,
			IGNightThree::WallListenFalloff,
			EIGAudioBus::Puzzle);
	}
}

void AIGMissingFloorNightThreeDirector::HandleWorkGloveExamined(
	AIGMissingFloorEvidence* Evidence)
{
	if (UIGMissingFloorNarrativeSubsystem* Narrative = GetNarrative())
	{
		Narrative->RecordWitness(EIGMissingFloorWitness::AnnexWorkGlove);
	}
}

void AIGMissingFloorNightThreeDirector::HandleTuningHammerExamined(
	AIGMissingFloorEvidence* Evidence)
{
	UIGMissingFloorNarrativeSubsystem* Narrative = GetNarrative();
	// §13 13행의 회수. 중고 거래 글을 본 회차에만 이 한 줄이 붙는다 —
	// 못 본 사람에게는 그냥 오빠가 떨어뜨린 렌치이고, 그것도 맞는 말이다.
	if (!Narrative
		|| !Narrative->HasBeatPlayed(FName(TEXT("Day.UsedListing")))
		|| !Narrative->MarkBeatPlayed(FName(TEXT("Night3.HammerListing"))))
	{
		return;
	}
	AIGHorrorHUD::PushThought(
		this,
		NSLOCTEXT(
			"IGMissingFloor",
			"TuningHammerListingThought",
			"목록에 있던 공구다. 이것까지 팔지는 못했네."),
		5.0f);
}

void AIGMissingFloorNightThreeDirector::HandleAnnexRecognitionZone(
	AIGZoneTrigger* Zone)
{
	UIGMissingFloorNarrativeSubsystem* Narrative = GetNarrative();
	if (!Narrative)
	{
		return;
	}
	// 채널 5를 안 눌렀으면 회수할 것이 없다. 못 본 사람에게 「그때 그
	// 화면」이라고 말하면 있지도 않은 기억을 지어내는 셈이다.
	if (!Narrative->HasBeatPlayed(FName(TEXT("Night2.CCTV"))))
	{
		return;
	}
	if (!Narrative->MarkBeatPlayed(FName(TEXT("Night3.AnnexRecognition"))))
	{
		return;
	}
	AIGHorrorHUD::PushThought(
		this,
		NSLOCTEXT(
			"IGMissingFloor",
			"AnnexRecognitionThought",
			"화면에서 본 복도다. 전구 하나, 왼쪽으로 꺾인다."),
		4.8f);
}

void AIGMissingFloorNightThreeDirector::HandlePlasterDatingExamined(
	AIGMissingFloorEvidence* Evidence)
{
	UIGMissingFloorNarrativeSubsystem* Narrative = GetNarrative();
	if (!Narrative)
	{
		return;
	}
	Narrative->RegisterTruthSource(
		EIGMissingFloorTruth::WallSealedThatDay,
		EIGMissingFloorSource::FreshPlasterDating);
	// 기록 화면의 「두 겹의 마감」 카드와 같은 사실이어야 한다. 카드는
	// 「안쪽 보드와 바깥 실란트의 굳은 정도가 다르다」로 적힌다.
	AIGHorrorHUD::PushThought(
		this,
		NSLOCTEXT(
			"IGMissingFloor",
			"PlasterDatingThought",
			"안쪽 보드는 낡았는데, 바깥 실란트엔 손톱 자국이 난다. 최근에 다시 막았나?"),
		4.6f);
}

void AIGMissingFloorNightThreeDirector::HandleTankAuditionExamined(
	AIGMissingFloorEvidence* Evidence)
{
	UIGMissingFloorNarrativeSubsystem* Narrative = GetNarrative();
	UWorld* World = GetWorld();
	if (!Narrative || !World)
	{
		return;
	}
	Narrative->RegisterTruthSource(
		EIGMissingFloorTruth::FiveNightsOfThirst,
		EIGMissingFloorSource::TankWaterAudition);

	// 수위는 눈으로, 물의 움직임은 소리로 확인한다.
	IGAudio::SpawnOneShotAt(
		this,
		UIGToneSequenceSoundWave::CreateRooftopTankSlosh(this),
		IGNightThree::TankAuditionLocation,
		0.62f,
		1.0f,
		120.0f,
		900.0f,
		EIGAudioBus::Puzzle);
	AIGHorrorHUD::PushAudioCaption(
		this,
		NSLOCTEXT(
			"IGMissingFloor",
			"TankAuditionCaption",
			"[안쪽] 물이 아주 느리게 오간다"),
		3.0f);

	// 일지를 이미 읽었다면 두 사실이 여기서 맞물린다. 안 읽었으면 그냥
	// 가득 찬 탱크다 — 순서를 강제하지 않는 것이 §7의 자유 경로다.
	const bool bKnowsTally = Narrative->HasSource(
		EIGMissingFloorTruth::FiveNightsOfThirst,
		EIGMissingFloorSource::KnockTallyJournal);
	// 용량 표찰과 수위계는 같은 탱크에 붙어 있다. 기록에도 관찰한 수위를 남긴다.
	AIGHorrorHUD::PushThought(
		this,
		bKnowsTally
			? NSLOCTEXT(
				"IGMissingFloor",
				"TankAuditionThoughtCrossed",
				"탱크가 가득 찼다. 벽 바로 뒤로 관이 지나간다.")
			: NSLOCTEXT(
				"IGMissingFloor",
				"TankAuditionThought",
				"이 밑으로 급수관이 내려간다."),
		bKnowsTally ? 5.0f : 4.6f);
}

void AIGMissingFloorNightThreeDirector::HandleWallListened(const int32 BayIndex)
{
	UIGMissingFloorNarrativeSubsystem* Narrative = GetNarrative();
	if (!bValveOpen)
	{
		// Before the valve there is nothing driving the wall, so all three
		// answer alike — and they answer, rather than being described as
		// answering. The dead board is why the player goes looking for water.
		PlayWallListenResponse(BayIndex, /*bHollow=*/false);
		AIGHorrorHUD::PushThought(
			this,
			NSLOCTEXT(
				"IGMissingFloor",
				"ListenSilent",
				"조용하다. 세 벽 다 똑같다."),
			3.2f);
		return;
	}
	if (BayIndex == IGNightThree::CavityBayIndex)
	{
		PlayWallListenResponse(BayIndex, /*bHollow=*/true);
		// 수첩을 안 읽었으면 소리가 다르다는 것까지만 안다. 그게 무슨 뜻인지는
		// 오빠가 적어 놓은 줄이 말해 준다 — 빠진 조각이 있다는 것은 알려야 한다.
		const bool bKnowsCriterion = Narrative
			&& Narrative->HasSource(
				EIGMissingFloorTruth::SomeoneInTheWall,
				EIGMissingFloorSource::PipeAuditionCriterion);
		AIGHorrorHUD::PushThought(
			this,
			bKnowsCriterion
				? NSLOCTEXT(
					"IGMissingFloor",
					"ListenCavity",
					"바로 뒤에서 흐른다. 이 벽만 속이 비었다.")
				: NSLOCTEXT(
					"IGMissingFloor",
					"ListenCavityUnread",
					"바로 뒤에서 흐른다. 이 벽만 소리가 다르다. 왜 다른 거지."),
			4.0f);
		if (Narrative)
		{
			Narrative->RegisterTruthSource(
				EIGMissingFloorTruth::SomeoneInTheWall,
				EIGMissingFloorSource::PipeWaterComparison);
			if (Narrative->HasTruth(EIGMissingFloorTruth::SomeoneInTheWall))
			{
				Narrative->MarkPuzzleSolved(IGNightThree::PuzzleThreeId);
			}
		}
		return;
	}
	PlayWallListenResponse(BayIndex, /*bHollow=*/false);
	AIGHorrorHUD::PushThought(
		this,
		NSLOCTEXT(
			"IGMissingFloor",
			"ListenFar",
			"물소리가 멀리서 웅웅거린다. 이 벽은 아니다."),
		3.2f);
}

void AIGMissingFloorNightThreeDirector::HandleWallKnocked(const int32 BayIndex)
{
	// The reckless route: the fist reads the wall instantly, and the sound
	// it makes is real — reported by the evidence actor itself.
	IGAudio::SpawnOneShotAt(
		this,
		UIGToneSequenceSoundWave::CreateWallKnockTriple(this, 0.2f),
		WallKnocks.IsValidIndex(BayIndex) && WallKnocks[BayIndex]
			? WallKnocks[BayIndex]->GetActorLocation()
			: GetActorLocation(),
		0.8f,
		1.0f,
		160.0f,
		1400.0f,
		EIGAudioBus::Player);

	if (BayIndex == IGNightThree::CavityBayIndex)
	{
		UIGMissingFloorNarrativeSubsystem* Narrative = GetNarrative();
		const bool bKnowsCriterion = Narrative
			&& Narrative->HasSource(
				EIGMissingFloorTruth::SomeoneInTheWall,
				EIGMissingFloorSource::PipeAuditionCriterion);
		AIGHorrorHUD::PushThought(
			this,
			bKnowsCriterion
				? NSLOCTEXT(
					"IGMissingFloor",
					"KnockCavity",
					"길게 운다. 속이 비었다.")
				: NSLOCTEXT(
					"IGMissingFloor",
					"KnockCavityUnread",
					"길게 운다. 다른 두 벽하고 다르다. 오빠라면 이게 뭔지 알았을 텐데."),
			3.8f);
		if (Narrative)
		{
			Narrative->RegisterTruthSource(
				EIGMissingFloorTruth::SomeoneInTheWall,
				EIGMissingFloorSource::WallEchoByHand);
			if (Narrative->HasTruth(EIGMissingFloorTruth::SomeoneInTheWall))
			{
				Narrative->MarkPuzzleSolved(IGNightThree::PuzzleThreeId);
			}
		}
		return;
	}
	AIGHorrorHUD::PushThought(
		this,
		NSLOCTEXT(
			"IGMissingFloor",
			"KnockSolid",
			"짧게 죽는 소리. 속이 찼다."),
		3.0f);
}

void AIGMissingFloorNightThreeDirector::HandleImpactMarkExamined(
	AIGMissingFloorEvidence* Evidence)
{
	// The evidence actor already filed LandingImpactMark; nothing extra.
}

void AIGMissingFloorNightThreeDirector::HandleAnswerKnock(
	AIGMissingFloorEvidence* Evidence)
{
	if (bAnswerPending || bAnswerDelivered)
	{
		return;
	}

	// Every interaction is one physical tap. The player owns the silence
	// between taps; no progress bar or prompt leaks the accepted cadence.
	IGAudio::SpawnOneShotAt(
		this,
		UIGToneSequenceSoundWave::CreateWallKnockSingle(this, 0.0f),
		Evidence ? Evidence->GetActorLocation() : GetActorLocation(),
		0.9f,
		1.0f,
		160.0f,
		1400.0f,
		EIGAudioBus::Player);
	const UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}
	const double Now = World->GetTimeSeconds();
	AnswerTapTimes.Add(Now);
	while (AnswerTapTimes.Num() > 3)
	{
		AnswerTapTimes.RemoveAt(0);
	}
	if (AnswerTapTimes.Num() < 3)
	{
		return;
	}

	const double PairInterval = AnswerTapTimes[1] - AnswerTapTimes[0];
	const double RestInterval = AnswerTapTimes[2] - AnswerTapTimes[1];
	const bool bPairAccepted =
		PairInterval >= IGNightThree::AnswerPairMinSeconds
		&& PairInterval <= IGNightThree::AnswerPairMaxSeconds;
	const bool bRestAccepted =
		RestInterval >= IGNightThree::AnswerRestMinSeconds
		&& RestInterval <= IGNightThree::AnswerRestMaxSeconds;
	if (!bPairAccepted || !bRestAccepted)
	{
		// Keep a plausible new pair, otherwise make this tap the next attempt's
		// first beat. Failure feedback is only the ordinary wall resonance.
		if (RestInterval >= IGNightThree::AnswerPairMinSeconds
			&& RestInterval <= IGNightThree::AnswerPairMaxSeconds)
		{
			AnswerTapTimes.RemoveAt(0);
		}
		else
		{
			AnswerTapTimes.Reset();
			AnswerTapTimes.Add(Now);
		}
		return;
	}

	bAnswerPending = true;
	if (UIGMissingFloorAudioSubsystem* AudioDirector =
		GetWorld()->GetSubsystem<UIGMissingFloorAudioSubsystem>())
	{
		AudioDirector->SetAuthoredSilence(true);
	}
	if (APlayerController* Controller = GetWorld()->GetFirstPlayerController())
	{
		if (AIGPlayerCharacter* Pawn = Cast<AIGPlayerCharacter>(Controller->GetPawn()))
		{
			if (UIGStressComponent* Stress = Pawn->GetStress())
			{
				Stress->SuppressHeartbeat(
					IGNightThree::AnswerDelaySeconds + 1.65f,
					true);
			}
		}
	}
	if (Evidence)
	{
		// No repeat while the silence holds — the wait is the scene.
		Evidence->SetInteractionEnabled(false);
	}

	GetWorldTimerManager().SetTimer(
		AnswerTimer,
		this,
		&AIGMissingFloorNightThreeDirector::DeliverWallAnswer,
		IGNightThree::AnswerDelaySeconds,
		false);
}

void AIGMissingFloorNightThreeDirector::DeliverWallAnswer()
{
	bAnswerPending = false;
	bAnswerDelivered = true;

	// From inside the studs: the same rhythm, muffled by gypsum.
	const FVector InsideWall(
		278.0f,
		IGNightThree::WallBayYs[IGNightThree::CavityBayIndex],
		1290.0f);
	IGAudio::SpawnOneShotAt(
		this,
		UIGToneSequenceSoundWave::CreateAnswerKnockPattern(this, 1.0f),
		InsideWall,
		0.85f,
		0.92f,
		140.0f,
		1200.0f,
		EIGAudioBus::Entity);

	AIGHorrorHUD::PushThought(
		this,
		NSLOCTEXT("IGMissingFloor", "AnswerThought", "…대답이다."),
		4.2f);

	if (UIGMissingFloorNarrativeSubsystem* Narrative = GetNarrative())
	{
		Narrative->RegisterTruthSource(
			EIGMissingFloorTruth::WaitingForAnAnswer,
			EIGMissingFloorSource::AnswerReturned);
		Narrative->MarkPuzzleSolved(IGNightThree::PuzzleFourId);
	}
	GetWorldTimerManager().SetTimer(
		AnswerSilenceReleaseTimer,
		this,
		&AIGMissingFloorNightThreeDirector::EndAnswerSilence,
		1.65f,
		false);
}

void AIGMissingFloorNightThreeDirector::EndAnswerSilence()
{
	if (UWorld* World = GetWorld())
	{
		if (UIGMissingFloorAudioSubsystem* AudioDirector =
			World->GetSubsystem<UIGMissingFloorAudioSubsystem>())
		{
			AudioDirector->SetAuthoredSilence(false);
		}
	}
}

void AIGMissingFloorNightThreeDirector::HandleNotebookRead(
	AIGReadableNote* Note,
	const bool bOpened)
{
	if (!bOpened)
	{
		return;
	}
	UIGMissingFloorNarrativeSubsystem* Narrative = GetNarrative();
	if (!Narrative)
	{
		return;
	}
	// The mother lode: the cover names him, the schedule explains the dawn
	// noise, the margin teaches the criterion, and the doodle carries the
	// rhythm she grew up with.
	Narrative->RegisterTruthSource(
		EIGMissingFloorTruth::TenantIdentity,
		EIGMissingFloorSource::TunerNotebookName);
	Narrative->RegisterTruthSource(
		EIGMissingFloorTruth::NoiseWasHomecoming,
		EIGMissingFloorSource::TunerWorkSchedule);
	Narrative->RegisterTruthSource(
		EIGMissingFloorTruth::SomeoneInTheWall,
		EIGMissingFloorSource::PipeAuditionCriterion);
	Narrative->RegisterTruthSource(
		EIGMissingFloorTruth::WaitingForAnAnswer,
		EIGMissingFloorSource::AnswerRhythmNotebook);
	RefreshAnswerTargetAvailability();
}

void AIGMissingFloorNightThreeDirector::HandleLabelsRead(
	AIGReadableNote* Note,
	const bool bOpened)
{
	if (!bOpened)
	{
		return;
	}
	if (UIGMissingFloorNarrativeSubsystem* Narrative = GetNarrative())
	{
		Narrative->RegisterTruthSource(
			EIGMissingFloorTruth::TenantIdentity,
			EIGMissingFloorSource::ShippingLabels);
	}
}

void AIGMissingFloorNightThreeDirector::HandleForumRead(
	AIGReadableNote* Note,
	const bool bOpened)
{
	if (!bOpened)
	{
		return;
	}
	if (UIGMissingFloorNarrativeSubsystem* Narrative = GetNarrative())
	{
		Narrative->RegisterTruthSource(
			EIGMissingFloorTruth::NoiseWasHomecoming,
			EIGMissingFloorSource::NoiseForumPosts);
		Narrative->RegisterTruthSource(
			EIGMissingFloorTruth::LandingStruggle,
			EIGMissingFloorSource::ForumFinalPost);
	}
}

void AIGMissingFloorNightThreeDirector::HandleJournalRead(
	AIGReadableNote* Note,
	const bool bOpened)
{
	if (!bOpened)
	{
		return;
	}
	if (UIGMissingFloorNarrativeSubsystem* Narrative = GetNarrative())
	{
		Narrative->RegisterTruthSource(
			EIGMissingFloorTruth::FiveNightsOfThirst,
			EIGMissingFloorSource::KnockTallyJournal);
		Narrative->RegisterTruthSource(
			EIGMissingFloorTruth::WaitingForAnAnswer,
			EIGMissingFloorSource::AnswerRhythmJournal);
		RefreshAnswerTargetAvailability();
	}
}

void AIGMissingFloorNightThreeDirector::HandleTruthConfirmed(
	const EIGMissingFloorTruth Truth)
{
	if (Truth == EIGMissingFloorTruth::SomeoneInTheWall)
	{
		RefreshAnswerTargetAvailability();
	}
	if (Truth == EIGMissingFloorTruth::WaitingForAnAnswer && !bSolvedAnnounced)
	{
		bSolvedAnnounced = true;
		OnSolved.Broadcast();
	}
}

// -- 비트 3-7 「귀환길」 ---------------------------------------------------

FVector AIGMissingFloorNightThreeDirector::GetReturnPassPoint() const
{
	return IGNightThree::ReturnPassPoint;
}

void AIGMissingFloorNightThreeDirector::ArmReturnPass()
{
	const UIGMissingFloorNarrativeSubsystem* Narrative = GetNarrative();
	if (!Narrative || Narrative->GetNightIndex() != 3)
	{
		return;
	}
	if (ReturnStage != EIGNightThreeReturnStage::Idle)
	{
		return;
	}
	ReturnStage = EIGNightThreeReturnStage::Passing;
	bWasWestOfHim = false;
	StageReturnFigure();
	// 목표가 바뀐 것을 한 줄로만 말한다. 무엇을 해야 하는지는 방금 배웠다.
	AIGHorrorHUD::PushThought(
		this,
		NSLOCTEXT(
			"IGMissingFloor",
			"N3ReturnThought",
			"대답이 왔다. 내려가자."),
		4.0f);
	GetWorldTimerManager().SetTimer(
		ReturnTimer,
		this,
		&AIGMissingFloorNightThreeDirector::AdvanceReturn,
		IGNightThree::ReturnPollSeconds,
		true);
}

void AIGMissingFloorNightThreeDirector::StageReturnFigure()
{
	AIGListenerEntity* Listener = Entity.Get();
	if (!Listener || bFigureStaged)
	{
		return;
	}
	bFigureStaged = true;
	// 1-4의 계단 카메오와 같은 장치다. 두 점을 주어 복도를 가로질러 조금씩
	// 움직이게 하면, 세워 둔 프롭이 아니라 자리를 지키는 사람으로 읽힌다.
	Listener->SetPatrolPoints({
		IGNightThree::ReturnPassPoint,
		IGNightThree::ReturnShufflePoint,
	});
	// ParkForBeat, not TeleportTo: P4's own three taps just left him
	// investigating a spot up in the annex, and a teleported-but-still-reacting
	// pursuer crawls out of the corridor before she ever gets down the stairs.
	Listener->ParkForBeat(
		IGNightThree::ReturnPassPoint,
		IGNightThree::ReturnPassYaw);
}

void AIGMissingFloorNightThreeDirector::ReleaseReturnFigure()
{
	if (!bFigureStaged)
	{
		return;
	}
	bFigureStaged = false;
	if (AIGListenerEntity* Listener = Entity.Get())
	{
		Listener->SetPatrolPoints(CorridorPatrolPoints);
		// 연출이었지 실패가 아니다. 공격 티어는 건드리지 않는다.
		Listener->ResetToPatrolStart(/*bRaiseAggression=*/false);
	}
}

bool AIGMissingFloorNightThreeDirector::IsPlayerInsideUnit403() const
{
	const AIGPlayerCharacter* PlayerCharacter = Player.Get();
	return PlayerCharacter
		&& IGNightThree::Unit403Interior.IsInsideOrOn(
			PlayerCharacter->GetActorLocation());
}

void AIGMissingFloorNightThreeDirector::AdvanceReturn()
{
	if (ReturnStage != EIGNightThreeReturnStage::Passing)
	{
		GetWorldTimerManager().ClearTimer(ReturnTimer);
		return;
	}
	const AIGPlayerCharacter* PlayerCharacter = Player.Get();
	const AIGListenerEntity* Listener = Entity.Get();
	if (!PlayerCharacter || !Listener)
	{
		return;
	}

	// 「그가 멈춰 기다리는 옆을 걸어 지나가는」. 서쪽에 있었다가 그가 기다리는
	// 동안 동쪽으로 넘어가면 그것이 이 비트다. 대답하지 않고 몰래 지나가는
	// 것도 정당한 해법이고 언제나 그랬다 — 다만 이 비트는 아니다.
	const float PlayerX = PlayerCharacter->GetActorLocation().X;
	const float HisX = Listener->GetActorLocation().X;
	if (PlayerX < HisX - IGNightThree::PassClearanceCentimeters)
	{
		bWasWestOfHim = true;
	}
	if (!bPassedWhileWaiting
		&& bWasWestOfHim
		&& PlayerX > HisX + IGNightThree::PassClearanceCentimeters
		&& Listener->GetListenerState() == EIGListenerState::Waiting)
	{
		bPassedWhileWaiting = true;
		if (UIGMissingFloorNarrativeSubsystem* Narrative = GetNarrative())
		{
			Narrative->MarkBeatPlayed(IGNightThree::PassByBeatId);
		}
		// 스치는 순간 그가 숨을 들이쉰다. 움직이지는 않는다 — 기다리는 중이다.
		// 글 한 줄뿐이던 비트에 몸이 생긴다.
		IGAudio::SpawnOneShotAt(
			this,
			UIGToneSequenceSoundWave::CreateEntityAlertVocal(this),
			Listener->GetActorLocation() + FVector(0.0f, 0.0f, 30.0f),
			0.6f,
			0.92f,
			200.0f,
			1800.0f,
			EIGAudioBus::Entity);
		if (AIGPlayerCharacter* MutablePlayer = Player.Get())
		{
			if (UIGStressComponent* Stress = MutablePlayer->GetStress())
			{
				Stress->ApplyScare(0.4f);
			}
			MutablePlayer->PlayScareKick(1.0f);
		}
		// 회피 대상이 애도 대상으로. 이 게임에서 가장 조용한 한 줄이어야 하므로
		// 설명하지 않는다 — 그가 무엇을 하고 있는지만 말한다.
		AIGHorrorHUD::PushThought(
			this,
			NSLOCTEXT(
				"IGMissingFloor",
				"N3PassByThought",
				"날 지나쳤다. 지금은 소리 내지 말자."),
			4.6f);
	}

	if (bMustLeaveHomeAgain)
	{
		if (!IsPlayerInsideUnit403())
		{
			bMustLeaveHomeAgain = false;
		}
		return;
	}
	if (IsPlayerInsideUnit403())
	{
		ReturnStage = EIGNightThreeReturnStage::Home;
		GetWorldTimerManager().ClearTimer(ReturnTimer);
		ReleaseReturnFigure();
		OnReturnedHome.Broadcast();
	}
}

void AIGMissingFloorNightThreeDirector::NotifyCaptureReset()
{
	if (ReturnStage == EIGNightThreeReturnStage::Passing)
	{
		// 리셋은 침대로 되돌린다. 그것을 도착으로 세면 잡히는 것이 목표 달성이
		// 되므로, 한 번 밖으로 나갔다 와야 한다. 형체는 다시 복도에 세운다.
		bMustLeaveHomeAgain = true;
		bFigureStaged = false;
		StageReturnFigure();
	}
}

void AIGMissingFloorNightThreeDirector::RefreshAnswerTargetAvailability()
{
	if (!AnswerTarget || bAnswerDelivered)
	{
		return;
	}
	UIGMissingFloorNarrativeSubsystem* Narrative = GetNarrative();
	int32 RhythmClueCount = 0;
	for (const EIGMissingFloorSource Clue : {
		EIGMissingFloorSource::AnswerRhythmVoicemail,
		EIGMissingFloorSource::AnswerRhythmNotebook,
		EIGMissingFloorSource::AnswerRhythmJournal})
	{
		RhythmClueCount += Narrative
			&& Narrative->HasSource(EIGMissingFloorTruth::WaitingForAnAnswer, Clue)
				? 1
				: 0;
	}
	if (Narrative && RhythmClueCount >= 2)
	{
		// Store the derived compatibility record used by the existing T9 rule.
		// Individual clue records remain in the save for fairness audits.
		Narrative->RegisterTruthSource(
			EIGMissingFloorTruth::WaitingForAnAnswer,
			EIGMissingFloorSource::AnswerRhythmMaterials);
	}
	const bool bReady =
		Narrative
		&& Narrative->HasTruth(EIGMissingFloorTruth::SomeoneInTheWall)
		&& RhythmClueCount >= 2;
	AnswerTarget->SetActorHiddenInGame(!bReady);
	AnswerTarget->SetInteractionEnabled(bReady);
	if (bReady && !bAnswerTargetAnnounced && bHourCurrentlyActive)
	{
		// 새 동사가 생겼다는 것은 알려야 한다. 같은 벽에 「두드린다」가 둘이면
		// 지금이 리듬인지 잔향 시험인지 알 길이 없다.
		bAnswerTargetAnnounced = true;
		AIGHorrorHUD::PushThought(
			this,
			NSLOCTEXT(
				"IGMissingFloor",
				"AnswerTargetReady",
				"두 군데서 같은 박자다. 둘, 쉬고, 하나. 이 벽에."),
			4.6f);
	}
}

void AIGMissingFloorNightThreeDirector::RefreshJournalAvailability(
	const bool bHourActive)
{
	if (!JournalNote)
	{
		return;
	}
	const UIGMissingFloorNarrativeSubsystem* Narrative = GetNarrative();
	const bool bEarned =
		!bHourActive
		&& Narrative
		&& Narrative->HasTruth(EIGMissingFloorTruth::WasStillAlive);
	JournalNote->SetActorHiddenInGame(!bEarned);
	JournalNote->SetActorEnableCollision(bEarned);
	JournalNote->SetInteractionEnabled(bEarned);
	if (bEarned && !bJournalAnnounced)
	{
		// 그냥 나타나는 물건은 없다. 401호 문 밑으로 밀려 나오는 소리와 함께 —
		// 자비 쪽지와 같은 손, 같은 종이. 그 노인이 건네는 것이다.
		bJournalAnnounced = true;
		IGAudio::SpawnOneShotAt(
			this,
			UIGToneSequenceSoundWave::CreatePaperDoorSlide(this),
			IGNightThree::JournalLocation,
			0.4f,
			1.0f,
			90.0f,
			900.0f,
			EIGAudioBus::World);
		AIGHorrorHUD::PushAudioCaptionAt(
			this,
			NSLOCTEXT("IGMissingFloor", "JournalSlideCaption", "401호 문 밑 — 종이가 밀려 나온다"),
			2.4f,
			IGNightThree::JournalLocation);
	}
}

void AIGMissingFloorNightThreeDirector::RefreshDistantSeoVisibility()
{
	if (!DistantSeo)
	{
		return;
	}
	const UIGMissingFloorNarrativeSubsystem* Narrative = GetNarrative();
	const bool bShow =
		!bHourCurrentlyActive
		&& Narrative
		&& Narrative->IsPuzzleSolved(FName(TEXT("P2")))
		&& !Narrative->HasTruth(EIGMissingFloorTruth::WaitingForAnAnswer);
	DistantSeo->SetVisibility(bShow, true);
	if (bShow && !bSeoAnnounced)
	{
		// 골목 건너에 사람이 서 있다. 눈이 먼저 가게 발소리 하나.
		bSeoAnnounced = true;
		const FVector At = DistantSeo->GetComponentLocation();
		IGAudio::SpawnOneShotAt(
			this,
			UIGToneSequenceSoundWave::CreateSurfaceFootstep(
				this, EIGFootstepSurface::Concrete, 0.9f, 0.7f),
			At,
			0.55f,
			1.0f,
			300.0f,
			2600.0f,
			EIGAudioBus::World);
		AIGHorrorHUD::PushAudioCaptionAt(
			this,
			NSLOCTEXT("IGMissingFloor", "DistantSeoCaption", "골목 건너 — 발소리"),
			2.0f,
			At);
	}
}

UIGMissingFloorNarrativeSubsystem*
AIGMissingFloorNightThreeDirector::GetNarrative() const
{
	const UWorld* World = GetWorld();
	UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
	return GameInstance
		? GameInstance->GetSubsystem<UIGMissingFloorNarrativeSubsystem>()
		: nullptr;
}

bool AIGMissingFloorNightThreeDirector::ValidateFixtures() const
{
	float RouteLengthCentimeters = 0.0f;
	int32 UpperStepCount = 0;
	const bool bPhysicalRouteValid =
		Scene.IsValid()
		&& Scene->ValidateMissingFloorRooftopRoute(
			RouteLengthCentimeters,
			UpperStepCount);
	return StairGate != nullptr
		&& AnnexGate != nullptr
		&& bPhysicalRouteValid
		&& FMath::IsNearlyEqual(RouteLengthCentimeters, 640.0f, 0.1f)
		&& UpperStepCount == 14
		&& Keyring != nullptr
		&& TunerNotebook != nullptr
		&& TuningHammer != nullptr
		&& RiserValve != nullptr
		&& WallListens.Num() == 3
		&& WallKnocks.Num() == 3
		&& ImpactMark != nullptr
		&& AnswerTarget != nullptr
		&& PlasterDatings.Num() == 3
		&& TankAudition != nullptr
		&& LabelsNote != nullptr
		&& ForumNote != nullptr
		&& JournalNote != nullptr;
}
