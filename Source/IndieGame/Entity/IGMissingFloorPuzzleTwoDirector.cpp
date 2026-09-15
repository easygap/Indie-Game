#include "Entity/IGMissingFloorPuzzleTwoDirector.h"

#include "Audio/IGAudioHelpers.h"
#include "Audio/IGToneSequenceSoundWave.h"
#include "Core/IGPrologueWorldScene.h"
#include "Engine/GameInstance.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Audio/IGMissingFloorAudioSubsystem.h"
#include "Components/AudioComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Entity/IGMissingFloorEvidence.h"
#include "Entity/IGNoiseSubsystem.h"
#include "Environment/IGCctvChannelFive.h"
#include "Interaction/IGReadableNote.h"
#include "Interaction/IGSwingDoor.h"
#include "Materials/MaterialInterface.h"
#include "Narrative/IGMissingFloorNarrativeSubsystem.h"
#include "Narrative/IGRecordingSubsystem.h"
#include "Player/IGHorrorHUD.h"

namespace IGPuzzleTwo
{
	// Booth-interior coordinates, ground floor (Z 0). Matched to the booth
	// BuildLobby erects behind the connector's north wall; duplicated by the
	// same rule every night director follows, because the scene's coordinate
	// namespace is .cpp-local.
	const FVector BoothDoorHinge(112.0f, -235.0f, 0.0f);
	// Desk top is Z=76.  The 2.8 cm ledger rests at Z=77.5, never at the old
	// upright-paper centre that made it intersect and appear to float.
	const FVector FairCopyLocation(120.0f, -103.0f, 77.5f);
	// 녹화기 오른쪽에 4.8cm를 띄워 먹지를 놓는다.
	const FVector CarbonLocation(179.0f, -103.0f, 77.5f);
	// 책상에 놓인 종이다. 세워 놓고 중심을 Z=80에 두면 24cm 높이의 절반이
	// 상판(Z=76) 아래로 들어가 책상에 박힌다 — 위 두 장에서 이미 한 번 고친
	// 실수다. 눕히고 상판 위에 올린다.
	const FVector AgentNoteLocation(205.0f, -103.0f, 76.08f);
	// 별도 상자 대신 모니터 하단 INPUT 버튼을 누른다.
	const FVector CctvLocation(134.8f, -105.6f, 91.3f);
	const FVector FoamGapLocation(272.0f, -90.0f, 105.0f);
	/**
	 * 자재 반입 영수증 두 장. 관리실 상판(X 105..215, Y -137.5..-82.5, Z=76)
	 * 앞쪽 오른편의 빈자리다 — 대리인 쪽지(X 196..214, Y -115..-91)와는
	 * Y로 갈라지고, 정서본·먹지·받침대는 전부 X 105..183 안쪽에 있다.
	 * 눕혀서 얹는다. 세우면 절반이 상판 아래로 들어간다.
	 */
	const FVector BoardReceiptsLocation(196.0f, -126.0f, 76.08f);
	/**
	 * §22.3 관리실 달력. 북벽 안쪽 면이 Y=-85이다. 동쪽은 벽이 아니라
	 * 안쪽 방 문짝(X 205..275)이라 처음에 거기 걸었다가 문을 뚫었다.
	 * 책상(X 105..215) 서쪽, 서벽(X 45..60)과도 떨어진 빈 벽에 건다.
	 */
	const FVector WallCalendarLocation(82.0f, -85.12f, 150.0f);
	// 녹화기 앞면의 표시등을 확인하는 범위다. 모델과 같은 위치를 쓴다.
	const FVector RecorderBayLocation(150.0f, -111.5f, 78.5f);
	/**
	 * 1층 배관 밸브. 펌프 토출관(X 75, Y -170) 앞, 책상 서쪽. 열면 관에 물이
	 * 흐르고 그 소리가 책상을 덮는다 — 밸브라는 동사를 여기서 처음 배운다.
	 * 밤3의 5층 밸브와 밤4의 세 손잡이가 같은 손이다(§7 P2, 2026-09-10).
	 */
	const FVector BoothValveLocation(75.0f, -184.2f, 52.0f);
	constexpr float BoothValveHoldSeconds = 0.8f;
	constexpr float BoothValveLoudness = 0.55f;
	/** 책상 둘레의 마스킹. 문지르기 0.25가 이 안에서 삼켜진다. */
	const FVector BoothMaskCenter(160.0f, -108.0f, 80.0f);
	constexpr float BoothMaskRadius = 420.0f;
	constexpr float BoothMaskAmount = 0.30f;
	/**
	 * §22.3 안쪽 방 청음. 문짝은 X 205..275, Y -87..-82이고 문틈 판정은
	 * X 270..274다. 그 문 앞에 서는 자리로, 문짝에서 1 cm 떨어뜨린다.
	 */
	const FVector InnerRoomListenLocation(248.0f, -93.0f, 140.0f);

	/** §5.1: frottage is a sustained 0.25 — three times, on purpose. */
	constexpr float FrottageLoudness = 0.25f;
	constexpr float FrottageHoldSeconds = 1.2f;

	/**
	 * §5.5. 비트 2-1에서 유담이 폰 녹음을 켜고 403호 현관문에 대어 둔다. 문
	 * 안쪽 바닥이므로 복도의 그가 아니라 그녀가 종일 지나는 자리다.
	 *
	 * 4층 세계 좌표다. 관리실 프롭들은 1층이라 Z가 작지만 이 폰만은 403호
	 * 안이므로 슬래브 높이를 더해야 한다 — 처음 넣을 때 그 900을 빠뜨려서
	 * 폰이 관리실 바닥에 놓여 있었다. 현관문은 (101, -225)에 있고 문 안쪽은
	 * Y가 0에 가까운 쪽이다.
	 */
	constexpr float FourthFloorZ = AIGPrologueWorldScene::FourthFloorZ;
	const FVector PhoneAtDoorLocation(150.0f, -196.0f, FourthFloorZ + 4.0f);
	/** 폰을 놓는 것은 소리를 내는 행동이다. 발소리보다 조용하지만 0은 아니다. */
	constexpr float PhonePlacementLoudness = 0.08f;

	const FName CctvBeatId(TEXT("Night2.CCTV"));
	const FName FoamBeatId(TEXT("Night2.Foam"));
	const FName PuzzleId(TEXT("P2"));
}

AIGMissingFloorPuzzleTwoDirector::AIGMissingFloorPuzzleTwoDirector()
{
	PrimaryActorTick.bCanEverTick = false;
}

bool AIGMissingFloorPuzzleTwoDirector::Configure(AIGPrologueWorldScene* InScene)
{
	UWorld* World = GetWorld();
	if (!World || !InScene)
	{
		return false;
	}
	Scene = InScene;

	UStaticMesh* CubeMesh =
		LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (!CubeMesh)
	{
		return false;
	}
	UStaticMesh* ComplaintLedgerMesh = LoadObject<UStaticMesh>(
		nullptr, TEXT("/Game/Meshes/SM_ComplaintLedger.SM_ComplaintLedger"));
	UMaterialInterface* LedgerMaterial = LoadObject<UMaterialInterface>(
		nullptr, TEXT("/Game/Prototype/Materials/M_PaperOld.M_PaperOld"));
	// 대리인 문자 사본은 어제 뽑은 출력물이다. 낡은 장부 종이가 아니라
	// 멀쩡한 종이를 쓴다. 재질을 비워 두면 엔진 기본 격자가 나온다.
	UMaterialInterface* SheetMaterial = LoadObject<UMaterialInterface>(
		nullptr, TEXT("/Game/Prototype/Materials/M_PaperClean.M_PaperClean"));
	// 관리실 문짝과 채널 선택기가 쓰는 무광 검은 플라스틱. 씬이 안쪽 방
	// 문짝에 이미 같은 재질을 쓴다. 둘 다 재질이 비어 있어서 엔진 기본
	// 격자로 그려지고 있었다 — 선택기는 모니터 뒤에 숨어 아무도 못 봤고,
	// 문은 플레이어가 지나다니는 6 x 80 x 204cm짜리였다.
	UMaterialInterface* DarkPlasticMaterial = LoadObject<UMaterialInterface>(
		nullptr, TEXT("/Game/Prototype/Materials/M_PlasticDark.M_PlasticDark"));
	UMaterialInterface* MetalMaterial = LoadObject<UMaterialInterface>(
		nullptr, TEXT("/Game/Prototype/Materials/M_MetalFrame.M_MetalFrame"));
	// 먹지는 낡은 종이가 아니다. 왁스 안료가 눌린 자리에서 얇아져 광택이
	// 달라지는 것이 이 물건의 전부이고, 플레이어가 문지르는 동안 보는 것도
	// 그것이다. 정서본과 같은 재질을 쓰면 「두 기록이 다르다」가 물건 단계에서
	// 이미 무너진다. 미베이크 환경에서는 기존 종이로 폴백해 진행을 막지 않는다.
	UMaterialInterface* CarbonMaterial = LoadObject<UMaterialInterface>(
		nullptr, TEXT("/Game/Prototype/Materials/M_CarbonPaper.M_CarbonPaper"));
	if (!CarbonMaterial)
	{
		CarbonMaterial = LedgerMaterial;
	}
	// Her own phone, the cracked one CH03 already models.
	UStaticMesh* PhoneMesh = LoadObject<UStaticMesh>(
		nullptr, TEXT("/Game/Meshes/SM_CrackedPhone.SM_CrackedPhone"));

	FActorSpawnParameters SpawnParameters;
	SpawnParameters.SpawnCollisionHandlingOverride =
		ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	// The booth door: same hinge grammar as the unit doors, panel spanning
	// the doorway BuildLobby left in the north wall. Starts day-locked;
	// SetHourActive is the only key it ever has.
	SpawnParameters.Name = TEXT("MissingFloorBoothDoor");
	BoothDoor = World->SpawnActor<AIGSwingDoor>(
		AIGSwingDoor::StaticClass(),
		FTransform(FRotator(0.0f, -90.0f, 0.0f), IGPuzzleTwo::BoothDoorHinge),
		SpawnParameters);
	if (!BoothDoor)
	{
		return false;
	}
	UStaticMesh* BoothLeaf = LoadObject<UStaticMesh>(nullptr,
		TEXT("/Game/Meshes/SM_UnitDoorLeafWideL.SM_UnitDoorLeafWideL"));
	if (BoothLeaf)
	{
		BoothDoor->ConfigureAuthoredLeaf(BoothLeaf,
			LoadObject<UStaticMesh>(nullptr, TEXT("/Game/Meshes/SM_UnitDoorHardwareWideL.SM_UnitDoorHardwareWideL")),
			FVector(5, AIGPrologueWorldScene::WideDoorLeafWidth, 200));
	}
	else
	{
		BoothDoor->ConfigurePrototypeVisuals(CubeMesh, DarkPlasticMaterial, MetalMaterial,
			FVector(6, AIGPrologueWorldScene::WideDoorLeafWidth, 204));
	}
	BoothDoor->SetOpenYaw(-95.0f);
	SetHourActive(false);

	// The fair copy: what the building admits happened.
	SpawnParameters.Name = TEXT("MissingFloorFairCopyLedger");
	FairCopyLedger = World->SpawnActor<AIGReadableNote>(
		AIGReadableNote::StaticClass(),
		FTransform(FRotator::ZeroRotator, IGPuzzleTwo::FairCopyLocation),
		SpawnParameters);
	if (!FairCopyLedger)
	{
		return false;
	}
	FairCopyLedger->ConfigurePrototypeVisuals(
		ComplaintLedgerMesh ? ComplaintLedgerMesh : CubeMesh,
		ComplaintLedgerMesh ? LedgerMaterial : nullptr,
		ComplaintLedgerMesh
			? FVector(100.0f, 100.0f, 100.0f)
			: FVector(21.0f, 1.2f, 29.7f),
		ComplaintLedgerMesh != nullptr);
	FairCopyLedger->SetInteractionPrompt(
		NSLOCTEXT("IGMissingFloor", "P2FairCopyPrompt", "민원 대장"));
	FairCopyLedger->SetNoteText(
		NSLOCTEXT("IGMissingFloor", "P2FairCopyTitle", "달빛빌라 민원 처리 대장"),
		{
			NSLOCTEXT("IGMissingFloor", "P2FairCopy1", "7/27  401호  물탱크 배관 소음.  조치 완료"),
			NSLOCTEXT("IGMissingFloor", "P2FairCopy2", "7/28  401호  배관 소음 재발.   조치 완료"),
			NSLOCTEXT("IGMissingFloor", "P2FairCopy3", "7/29  401호  옥상 바람 소리.   확인"),
			NSLOCTEXT("IGMissingFloor", "P2FairCopy4", "7/31  401호  소음 없음.       종결"),
			FText::GetEmpty(),
			NSLOCTEXT(
				"IGMissingFloor",
				"P2FairCopyNote",
				"담당  목한수  /  확인란 별도"),
		});

	// The carbon pad. Three passes of the pencil; the truth is the third.
	SpawnParameters.Name = TEXT("MissingFloorCarbonLedger");
	CarbonLedger = World->SpawnActor<AIGMissingFloorEvidence>(
		AIGMissingFloorEvidence::StaticClass(),
		FTransform(FRotator::ZeroRotator, IGPuzzleTwo::CarbonLocation),
		SpawnParameters);
	if (!CarbonLedger)
	{
		return false;
	}
	CarbonLedger->Configure(
		ComplaintLedgerMesh ? ComplaintLedgerMesh : CubeMesh,
		ComplaintLedgerMesh ? CarbonMaterial : nullptr,
		// 저작된 민원 대장은 22 x 30.7 x 2.8cm로 이미 실제 크기다. 예전에는
		// 여기에 (100,100,100)을 넘겨서 책이 1m 정육면체로 부풀었고, 관리실
		// 책상 캡처가 먹지 한 장으로 가득 찼다.
		ComplaintLedgerMesh
			? FVector::ZeroVector
			: FVector(21.0f, 1.0f, 29.7f),
		NSLOCTEXT("IGMissingFloor", "P2CarbonPrompt", "먹지 — 문지른다"),
		NSLOCTEXT(
			"IGMissingFloor",
			"P2CarbonRestored",
			"「벽에서 쿵쿵. 사람 소리 같다.」 7월 27일에 접수됐다고 적혀 있다."),
		EIGMissingFloorTruth::WasStillAlive,
		EIGMissingFloorSource::CarbonLedgerOriginal,
		IGPuzzleTwo::FrottageHoldSeconds,
		IGPuzzleTwo::FrottageLoudness);
	// §21.3 프로타주: the only sustained interaction in the game gets the only
	// sustained cue. Until now the player rubbed for 1.2 s in silence while the
	// noise bus reported 0.25 — the cost was real and inaudible.
	CarbonLedger->SetSustainedRubCue(true);
	CarbonLedger->SetProgressiveStages({
		NSLOCTEXT(
			"IGMissingFloor",
			"P2CarbonStage1",
			"글자가 비친다. 연필을 더 눕혀야겠다."),
		NSLOCTEXT(
			"IGMissingFloor",
			"P2CarbonStage2",
			"날짜가 나온다. 7월 27… 28… 조금만 더."),
	});
	CarbonLedger->OnExamined.AddUObject(
		this, &AIGMissingFloorPuzzleTwoDirector::HandleCarbonRestored);

	// The agent's message: when the building says he left.
	SpawnParameters.Name = TEXT("MissingFloorAgentNote");
	AgentMessageNote = World->SpawnActor<AIGReadableNote>(
		AIGReadableNote::StaticClass(),
		FTransform(FRotator(0, 180, 0), IGPuzzleTwo::AgentNoteLocation),
		SpawnParameters);
	if (!AgentMessageNote)
	{
		return false;
	}
	AgentMessageNote->ConfigurePrototypeVisuals(
		CubeMesh, LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Prototype/Materials/M_BoothAgentNote.M_BoothAgentNote")), FVector(18.0f, 24.0f, 0.08f));
	AgentMessageNote->SetInteractionPrompt(
		NSLOCTEXT("IGMissingFloor", "P2AgentPrompt", "출력된 문자 사본"));
	AgentMessageNote->SetNoteText(
		NSLOCTEXT("IGMissingFloor", "P2AgentTitle", "무영부동산 문자 사본"),
		{
			NSLOCTEXT("IGMissingFloor", "P2Agent1", "7/26 14:02  부동산: 사장님, 옥탑 짐은 다 뺐습니다."),
			NSLOCTEXT("IGMissingFloor", "P2Agent2", "7/26 14:05  목한수: 네. 이제 창고로 쓸 거예요."),
			FText::GetEmpty(),
			NSLOCTEXT(
				"IGMissingFloor",
				"P2Agent3",
				"7/26 14:11  부동산: 열쇠는 우편함에 넣고 갑니다."),
		});
	AgentMessageNote->OnReadStateChanged.AddDynamic(
		this, &AIGMissingFloorPuzzleTwoDirector::HandleAgentNoteRead);

	// The monitor's channel selector: a four-way split and a fifth button.
	SpawnParameters.Name = TEXT("MissingFloorCctvSelector");
	CctvSelector = World->SpawnActor<AIGMissingFloorEvidence>(
		AIGMissingFloorEvidence::StaticClass(),
		FTransform(FRotator::ZeroRotator, IGPuzzleTwo::CctvLocation),
		SpawnParameters);
	if (!CctvSelector)
	{
		return false;
	}
	CctvSelector->Configure(
		CubeMesh,
		DarkPlasticMaterial,
		FVector(5.0f, 2.0f, 4.0f),
		NSLOCTEXT("IGMissingFloor", "P2CctvPrompt", "모니터 — 외부 입력"),
		FText::GetEmpty(),
		EIGMissingFloorTruth::None,
		EIGMissingFloorSource::None,
		0.0f,
		0.06f, false);
	CctvSelector->OnExamined.AddUObject(
		this, &AIGMissingFloorPuzzleTwoDirector::HandleCctvExamined);

	// §14 CCTV 채널 5. Spawned with the booth so the monitor material and the AUX
	// label are resident before the press; the capture and its render target are
	// not created until the button is actually pushed (상시 렌더 금지).
	SpawnParameters.Name = TEXT("MissingFloorCctvChannelFive");
	CctvChannelFive = World->SpawnActor<AIGCctvChannelFive>(
		AIGCctvChannelFive::StaticClass(),
		FTransform::Identity,
		SpawnParameters);
	if (!CctvChannelFive || !CctvChannelFive->Configure(InScene))
	{
		// The beat degrades to its narration rather than failing the whole booth:
		// a missing baked material must not cost the player P2 and T7.
		if (CctvChannelFive)
		{
			CctvChannelFive->Destroy();
			CctvChannelFive = nullptr;
		}
	}

	// The inner room's door gap: the foam reveal.
	SpawnParameters.Name = TEXT("MissingFloorFoamGap");
	FoamGap = World->SpawnActor<AIGMissingFloorEvidence>(
		AIGMissingFloorEvidence::StaticClass(),
		FTransform(FRotator::ZeroRotator, IGPuzzleTwo::FoamGapLocation),
		SpawnParameters);
	if (!FoamGap)
	{
		return false;
	}
	FoamGap->Configure(
		CubeMesh,
		nullptr,
		FVector(4.0f, 4.0f, 60.0f),
		NSLOCTEXT("IGMissingFloor", "P2FoamPrompt", "문틈"),
		FText::GetEmpty(),
		EIGMissingFloorTruth::None,
		EIGMissingFloorSource::None,
		0.0f,
		0.05f,
		/*bPresentationVisible=*/false);
	FoamGap->OnExamined.AddUObject(
		this, &AIGMissingFloorPuzzleTwoDirector::HandleFoamExamined);

	// 1층 배관 밸브. 밸브라는 동사의 첫 수업이다 — 물이 흐르면 책상의 소리가
	// 묻힌다. 밤3의 5층 밸브, 밤4의 세 손잡이가 같은 손으로 이어진다.
	SpawnParameters.Name = TEXT("MissingFloorBoothRiserValve");
	BoothRiserValve = World->SpawnActor<AIGMissingFloorEvidence>(
		AIGMissingFloorEvidence::StaticClass(),
		FTransform(FRotator(0, 0, 90), IGPuzzleTwo::BoothValveLocation),
		SpawnParameters);
	if (!BoothRiserValve)
	{
		return false;
	}
	UStaticMesh* BoothValveMesh = LoadObject<UStaticMesh>(
		nullptr, TEXT("/Game/Meshes/SM_P3ValveWheelSmall.SM_P3ValveWheelSmall"));
	BoothRiserValve->Configure(
		BoothValveMesh,
		MetalMaterial,
		FVector::ZeroVector,
		NSLOCTEXT("IGMissingFloor", "BoothValvePrompt", "배관 밸브 — 연다"),
		FText::GetEmpty(),
		EIGMissingFloorTruth::None,
		EIGMissingFloorSource::None,
		IGPuzzleTwo::BoothValveHoldSeconds,
		IGPuzzleTwo::BoothValveLoudness);
	BoothRiserValve->GetPresentationMesh()->SetMobility(EComponentMobility::Movable);
	BoothRiserValve->OnExamined.AddUObject(
		this, &AIGMissingFloorPuzzleTwoDirector::HandleBoothValveOpened);

	// T5의 첫 번째 출처(§12). 목한수의 이중 서류 습관은 밤2 관리실에서
	// 심고 밤4·엔딩에서 그를 잡는 증거가 된다(§13).
	SpawnParameters.Name = TEXT("MissingFloorBoardReceipts");
	BoardReceipts = World->SpawnActor<AIGReadableNote>(
		AIGReadableNote::StaticClass(),
		FTransform(
			FRotator(0.0f, 173.0f, 0.0f),
			IGPuzzleTwo::BoardReceiptsLocation),
		SpawnParameters);
	if (!BoardReceipts)
	{
		return false;
	}
	BoardReceipts->ConfigurePrototypeVisuals(
		CubeMesh, LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Prototype/Materials/M_BoothReceipts.M_BoothReceipts")), FVector(16.0f, 9.0f, 0.08f));
	BoardReceipts->SetInteractionPrompt(
		NSLOCTEXT("IGMissingFloor", "P2ReceiptsPrompt", "자재 반입 영수증"));
	BoardReceipts->SetNoteText(
		NSLOCTEXT("IGMissingFloor", "P2ReceiptsTitle", "자재 반입 영수증 (2매)"),
		{
			NSLOCTEXT("IGMissingFloor", "P2Receipt1", "무영건재  ·  무영로 27-3 달빛빌라"),
			FText::GetEmpty(),
			NSLOCTEXT("IGMissingFloor", "P2Receipt2", "7/26   석고보드 9.5T   12장     현금"),
			NSLOCTEXT("IGMissingFloor", "P2Receipt3", "       경량스터드 3.6m  8본     현금"),
			FText::GetEmpty(),
			NSLOCTEXT("IGMissingFloor", "P2Receipt4", "7/27   석고보드 9.5T   12장     현금"),
			NSLOCTEXT("IGMissingFloor", "P2Receipt5", "       미장몰탈 20kg    4포     현금"),
			FText::GetEmpty(),
			NSLOCTEXT(
				"IGMissingFloor",
				"P2Receipt6",
				"기재일 7/26  ·  옥상 보수비"),
		});
	BoardReceipts->OnReadStateChanged.AddDynamic(
		this, &AIGMissingFloorPuzzleTwoDirector::HandleBoardReceiptsRead);

	// §22.3. 둘 다 진실 표에 들어가지 않는다 — 밤2를 통과하는 데는 필요
	// 없고, 본 회차에만 목한수 앞에서 댈 것이 늘어난다.
	SpawnParameters.Name = TEXT("MissingFloorWallCalendar");
	WallCalendar = World->SpawnActor<AIGMissingFloorEvidence>(
		AIGMissingFloorEvidence::StaticClass(),
		FTransform(FRotator::ZeroRotator, IGPuzzleTwo::WallCalendarLocation),
		SpawnParameters);
	if (WallCalendar)
	{
		WallCalendar->Configure(
			CubeMesh,
			LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Prototype/Materials/M_BoothCalendar.M_BoothCalendar")),
			FVector(30.0f, 0.16f, 42.0f),
			NSLOCTEXT("IGMissingFloor", "P2CalendarPrompt", "벽 달력"),
			NSLOCTEXT(
				"IGMissingFloor",
				"P2CalendarThought",
				"작년 7월 달력이다. 26일에만 표시가 돼 있다."),
			EIGMissingFloorTruth::None,
			EIGMissingFloorSource::None,
			0.9f,
			0.04f);
		WallCalendar->OnExamined.AddUObject(
			this, &AIGMissingFloorPuzzleTwoDirector::HandleWallCalendarExamined);
	}

	SpawnParameters.Name = TEXT("MissingFloorRecorderBay");
	RecorderBay = World->SpawnActor<AIGMissingFloorEvidence>(
		AIGMissingFloorEvidence::StaticClass(),
		FTransform(FRotator::ZeroRotator, IGPuzzleTwo::RecorderBayLocation),
		SpawnParameters);
	if (RecorderBay)
	{
		RecorderBay->Configure(
			CubeMesh,
			DarkPlasticMaterial,
			FVector(26.0f, 3.0f, 4.5f),
			NSLOCTEXT("IGMissingFloor", "P2RecorderPrompt", "녹화기"),
			NSLOCTEXT(
				"IGMissingFloor",
				"P2RecorderThought",
				"화면에 ‘HDD 없음’이라고 뜬다. 녹화 표시등도 꺼져 있다."),
			EIGMissingFloorTruth::None,
			EIGMissingFloorSource::None,
			0.9f,
			0.06f, false);
		RecorderBay->OnExamined.AddUObject(
			this, &AIGMissingFloorPuzzleTwoDirector::HandleRecorderBayExamined);
	}

	// 같은 문에서 눈과 귀가 다른 것을 준다. 문틈은 계란판을 보여 주고,
	// 여기서는 그 너머에서 뭔가 돌고 있다는 것만 들린다.
	SpawnParameters.Name = TEXT("MissingFloorInnerRoomListen");
	InnerRoomListen = World->SpawnActor<AIGMissingFloorEvidence>(
		AIGMissingFloorEvidence::StaticClass(),
		FTransform(FRotator::ZeroRotator, IGPuzzleTwo::InnerRoomListenLocation),
		SpawnParameters);
	if (InnerRoomListen)
	{
		InnerRoomListen->Configure(
			CubeMesh,
			nullptr,
			FVector(40.0f, 10.0f, 60.0f),
			NSLOCTEXT(
				"IGMissingFloor",
				"P2InnerRoomListenPrompt",
				"안쪽 방 문 — 귀를 댄다"),
			FText::GetEmpty(),
			EIGMissingFloorTruth::None,
			EIGMissingFloorSource::None,
			1.0f,
			0.03f,
			/*bPresentationVisible=*/false);
		InnerRoomListen->Tags.AddUnique(FName(TEXT("MissingFloor.Verb.Listen")));
		InnerRoomListen->OnExamined.AddUObject(
			this, &AIGMissingFloorPuzzleTwoDirector::HandleInnerRoomListenExamined);
	}

	// §5.5 비트 2-1과 2-6. 같은 물건이 밤에는 녹음을 켜고 아침에는 그것을
	// 재생한다. 두 번째 상호작용이 이 게임에서 가장 조용한 절망이다.
	SpawnParameters.Name = TEXT("MissingFloorPhoneRecorder");
	PhoneRecorder = World->SpawnActor<AIGMissingFloorEvidence>(
		AIGMissingFloorEvidence::StaticClass(),
		FTransform(FRotator(0.0f, 12.0f, 0.0f), IGPuzzleTwo::PhoneAtDoorLocation),
		SpawnParameters);
	if (!PhoneRecorder)
	{
		return false;
	}
	// 저작된 깨진 폰은 이미 실제 크기다. (100.0f)를 넘기면 1m 정육면체가
	// 되고, 재질을 비우면 엔진 기본 격자가 나온다. 밤4가 같은 메시에 쓰는
	// 무광 검은 플라스틱을 맞춰 준다.
	PhoneRecorder->Configure(
		PhoneMesh ? PhoneMesh : CubeMesh,
		DarkPlasticMaterial,
		PhoneMesh ? FVector::ZeroVector : FVector(7.0f, 14.5f, 1.6f),
		NSLOCTEXT("IGMissingFloor", "P2PhoneArmPrompt", "폰 — 녹음"),
		FText::GetEmpty(),
		EIGMissingFloorTruth::None,
		EIGMissingFloorSource::None,
		0.0f,
		IGPuzzleTwo::PhonePlacementLoudness);
	PhoneRecorder->OnExamined.AddUObject(
		this, &AIGMissingFloorPuzzleTwoDirector::HandlePhoneRecorder);
	RefreshPhonePrompt();

	// T7 lands on night 3, when the realtor's message is found beside the
	// keyring. The night-2 goal is the restored original, and the note stays
	// off the desk until then.
	RefreshAgentNoteAvailability();
	if (UIGMissingFloorNarrativeSubsystem* Narrative = GetNarrative())
	{
		TruthHandle = Narrative->OnTruthConfirmed.AddUObject(
			this, &AIGMissingFloorPuzzleTwoDirector::HandleTruthConfirmed);
	}
	return true;
}

void AIGMissingFloorPuzzleTwoDirector::EndPlay(
	const EEndPlayReason::Type EndPlayReason)
{
	if (UIGMissingFloorNarrativeSubsystem* Narrative = GetNarrative())
	{
		Narrative->OnTruthConfirmed.Remove(TruthHandle);
	}
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(CctvThoughtTimer);
	}
	CloseBoothValve();
	Super::EndPlay(EndPlayReason);
}

void AIGMissingFloorPuzzleTwoDirector::SetHourActive(const bool bHourActive)
{
	RefreshAgentNoteAvailability();
	// §5.5: dawn closes the take. She left it running all night and now there is
	// something to play — which is the only reason beat 2-6 can exist.
	if (UWorld* World = GetWorld())
	{
		if (UIGRecordingSubsystem* Recording =
			World->GetSubsystem<UIGRecordingSubsystem>())
		{
			if (!bHourActive && Recording->IsRecording())
			{
				Recording->StopRecording();
				bPhonePlayedBack = false;
			}
		}
	}
	RefreshPhonePrompt();
	if (!bHourActive)
	{
		// 낮에 그가 도로 잠근다.
		CloseBoothValve();
	}

	if (!BoothDoor)
	{
		return;
	}
	if (bHourActive)
	{
		// He is hiding in the soundproofed room; the booth stands open.
		TArray<FIGDoorRequirement> NoRequirements;
		BoothDoor->SetRequirements(MoveTemp(NoRequirements));
	}
	else
	{
		// Daytime post. The requirement tag is a registered sentinel that is
		// never granted — the door opens by clearing requirements, never by
		// a tag, so no lock state can leak into a save.
		BoothDoor->ForceOpenState(false);
		TArray<FIGDoorRequirement> DayLock;
		FIGDoorRequirement& Lock = DayLock.AddDefaulted_GetRef();
		Lock.RequiredState = FGameplayTag::RequestGameplayTag(
			FName(TEXT("State.MissingFloor.Office.Vacant")), false);
		Lock.LockedPrompt =
			NSLOCTEXT("IGMissingFloor", "BoothDayPrompt", "관리실");
		Lock.LockedThought = NSLOCTEXT(
			"IGMissingFloor",
			"BoothDayThought",
			"안에 있다. 낮에는 문을 안 열어 준다.");
		BoothDoor->SetRequirements(MoveTemp(DayLock));
	}
}

void AIGMissingFloorPuzzleTwoDirector::HandleCarbonRestored(
	AIGMissingFloorEvidence* Evidence)
{
	if (UIGMissingFloorNarrativeSubsystem* Narrative = GetNarrative())
	{
		if (!Narrative->HasTruth(EIGMissingFloorTruth::WasStillAlive)) { return; }
		Narrative->MarkPuzzleSolved(IGPuzzleTwo::PuzzleId);
	}
	// 원문을 읽는 데서 끝내지 않고, 퇴실 문자와 접수 날짜를 대조한다.
	if (!bSolvedAnnounced)
	{
		bSolvedAnnounced = true;
		OnSolved.Broadcast();
	}
}

void AIGMissingFloorPuzzleTwoDirector::HandleBoothValveOpened(
	AIGMissingFloorEvidence* Evidence)
{
	UWorld* World = GetWorld();
	if (bBoothValveOpen || !World)
	{
		return;
	}
	bBoothValveOpen = true;
	BoothRiserValve->GetPresentationMesh()->AddLocalRotation(FRotator(0, 90, 0));
	if (Evidence)
	{
		Evidence->SetInteractionPrompt(
			NSLOCTEXT("IGMissingFloor", "BoothValveOpenPrompt", "배관 밸브 — 열려 있다"));
		Evidence->SetInteractionEnabled(false);
	}
	IGAudio::SpawnOneShotAt(
		this,
		UIGToneSequenceSoundWave::CreateValveOpen(this, 0),
		IGPuzzleTwo::BoothValveLocation,
		0.7f,
		1.0f,
		160.0f,
		1400.0f,
		EIGAudioBus::Puzzle);

	// 관에 물이 흐른다. 흡입관 옆에서는 가까이, 책상에서는 한 단 멀리.
	BoothRiserFlow = NewObject<UAudioComponent>(this, TEXT("BoothRiserFlowBed"));
	if (BoothRiserFlow)
	{
		BoothRiserFlow->RegisterComponent();
		BoothRiserFlow->SetWorldLocation(
			IGPuzzleTwo::BoothValveLocation + FVector(0.0f, -15.0f, -20.0f));
		BoothRiserFlow->SetSound(
			UIGToneSequenceSoundWave::CreatePipeWaterFlow(this, 1));
		BoothRiserFlow->AttenuationSettings = IGAudio::MakeAttenuation(
			this,
			120.0f,
			900.0f,
			EIGAudioBus::Puzzle);
		BoothRiserFlow->bAllowSpatialization = true;
		BoothRiserFlow->SetVolumeMultiplier(0.5f);
		if (UIGMissingFloorAudioSubsystem* AudioDirector =
			World->GetSubsystem<UIGMissingFloorAudioSubsystem>())
		{
			AudioDirector->RegisterComponent(BoothRiserFlow, EIGAudioBus::Puzzle);
		}
		BoothRiserFlow->Play();
	}
	// 책상 둘레가 물소리 아래로 들어간다. 문지르기 0.25는 여기서 삼켜지고,
	// 소음 파문도 안 뜬다 — 그게 수업이다.
	if (UIGNoiseSubsystem* Noise = World->GetSubsystem<UIGNoiseSubsystem>())
	{
		BoothValveHumHandle = Noise->RegisterHumSource(
			IGPuzzleTwo::BoothMaskCenter,
			IGPuzzleTwo::BoothMaskRadius,
			IGPuzzleTwo::BoothMaskAmount);
	}
	AIGHorrorHUD::PushThought(
		this,
		NSLOCTEXT(
			"IGMissingFloor",
			"BoothValveThought",
			"관에서 물소리. 책상까지 울린다."),
		3.6f);
}

void AIGMissingFloorPuzzleTwoDirector::CloseBoothValve()
{
	if (!bBoothValveOpen)
	{
		return;
	}
	bBoothValveOpen = false;
	if (BoothRiserValve) { BoothRiserValve->GetPresentationMesh()->AddLocalRotation(FRotator(0, -90, 0)); }
	if (BoothRiserFlow)
	{
		BoothRiserFlow->Stop();
		BoothRiserFlow->DestroyComponent();
		BoothRiserFlow = nullptr;
	}
	if (UWorld* World = GetWorld())
	{
		if (UIGNoiseSubsystem* Noise = World->GetSubsystem<UIGNoiseSubsystem>())
		{
			if (BoothValveHumHandle != INDEX_NONE)
			{
				Noise->UnregisterHumSource(BoothValveHumHandle);
			}
		}
	}
	BoothValveHumHandle = INDEX_NONE;
	if (BoothRiserValve)
	{
		BoothRiserValve->SetInteractionPrompt(
			NSLOCTEXT("IGMissingFloor", "BoothValvePrompt", "배관 밸브 — 연다"));
		BoothRiserValve->SetInteractionEnabled(true);
	}
}

void AIGMissingFloorPuzzleTwoDirector::RefreshAgentNoteAvailability()
{
	if (!AgentMessageNote)
	{
		return;
	}
	const UIGMissingFloorNarrativeSubsystem* Narrative = GetNarrative();
	const bool bPresent = Narrative != nullptr;
	AgentMessageNote->SetActorHiddenInGame(!bPresent);
	AgentMessageNote->SetActorEnableCollision(bPresent);
	AgentMessageNote->SetInteractionEnabled(bPresent);
}

void AIGMissingFloorPuzzleTwoDirector::HandleAgentNoteRead(
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
			EIGMissingFloorTruth::WasStillAlive,
			EIGMissingFloorSource::AgentMoveOutMessage);
	}
}

void AIGMissingFloorPuzzleTwoDirector::RefreshPhonePrompt()
{
	if (!PhoneRecorder)
	{
		return;
	}
	const UWorld* World = GetWorld();
	const UIGRecordingSubsystem* Recording = World
		? World->GetSubsystem<UIGRecordingSubsystem>()
		: nullptr;
	if (!Recording)
	{
		return;
	}
	if (Recording->IsRecording())
	{
		PhoneRecorder->SetInteractionPrompt(
			NSLOCTEXT("IGMissingFloor", "P2PhoneRecording", "폰 — 녹음 중"));
		return;
	}
	if (Recording->HasTake() && !bPhonePlayedBack)
	{
		PhoneRecorder->SetInteractionPrompt(
			NSLOCTEXT("IGMissingFloor", "P2PhonePlayPrompt", "폰 — 재생"));
		return;
	}
	PhoneRecorder->SetInteractionPrompt(
		NSLOCTEXT("IGMissingFloor", "P2PhoneArmPrompt", "폰 — 녹음"));
}

void AIGMissingFloorPuzzleTwoDirector::HandlePhoneRecorder(
	AIGMissingFloorEvidence* Evidence)
{
	UWorld* World = GetWorld();
	UIGRecordingSubsystem* Recording = World
		? World->GetSubsystem<UIGRecordingSubsystem>()
		: nullptr;
	if (!Recording)
	{
		return;
	}

	// 비트 2-6. The take exists, so this press is the morning one.
	if (!Recording->IsRecording() && Recording->HasTake() && !bPhonePlayedBack)
	{
		const FVector At = Evidence
			? Evidence->GetActorLocation()
			: IGPuzzleTwo::PhoneAtDoorLocation;
		if (!Recording->PlayBack(At))
		{
			return;
		}
		bPhonePlayedBack = true;
		RefreshPhonePrompt();
		// 거부된 자리가 있으면 그 공백이 대사다. 거부된 게 없는 테이프는 둘로
		// 갈린다 — 규칙이 풀린 뒤라면 담긴 것이고, 아니면 그가 다녀간 뒤에
		// 켠 것이다. 후자에 「담겼다」를 주면 규칙이 거짓말이 된다.
		const bool bAnythingRefused = Recording->GetSuppressedCount() > 0;
		const bool bRuleLifted = Recording->IsRuleLifted();
		AIGHorrorHUD::PushThought(
			this,
			bAnythingRefused
				? NSLOCTEXT(
					"IGMissingFloor",
					"P2PhoneSilence",
					"내 발소리는 들리는데. 문에서 난 소리는 하나도 안 담겼다.")
				: bRuleLifted
					? NSLOCTEXT(
						"IGMissingFloor",
						"P2PhoneKept",
						"담겼다. 이번엔 담겼어.")
					: NSLOCTEXT(
						"IGMissingFloor",
						"P2PhoneNothing",
						"발소리하고 숨소리만 담겼다."),
			5.0f);
		return;
	}

	// 비트 2-1. She sets it against the door and lets it run.
	if (!Recording->IsRecording())
	{
		Recording->StartRecording();
		bPhonePlayedBack = false;
		RefreshPhonePrompt();
		AIGHorrorHUD::PushThought(
			this,
			NSLOCTEXT(
				"IGMissingFloor",
				"P2PhoneArmed",
				"여기 두면 들리겠지."),
			3.4f);
	}
}

void AIGMissingFloorPuzzleTwoDirector::HandleCctvExamined(
	AIGMissingFloorEvidence* Evidence)
{
	UIGMissingFloorNarrativeSubsystem* Narrative = GetNarrative();
	if (!Narrative || !Narrative->MarkBeatPlayed(IGPuzzleTwo::CctvBeatId))
	{
		return;
	}
	// One press, one look, and the channel dies back to the four-way split.
	// The fifth floor exists on a monitor before it exists underfoot — and
	// the low shape crossing the frame is why the player climbs in night 3.
	IGAudio::SpawnOneShotAt(
		this,
		UIGToneSequenceSoundWave::CreateRelayClick(this),
		Evidence ? Evidence->GetActorLocation() : GetActorLocation(),
		0.7f);
	// The picture comes up on the monitor itself. When the channel cannot be
	// built the thought still lands, because losing the reveal must never leave
	// the player without the reason to climb in 밤3.
	const bool bChannelLive = CctvChannelFive && CctvChannelFive->Play();
	AIGHorrorHUD::PushAudioCaption(
		this,
		NSLOCTEXT("IGMissingFloor", "P2CctvCaption", "화면 지직임"),
		1.6f);
	if (!bChannelLive)
	{
		AIGHorrorHUD::PushThought(
			this,
			NSLOCTEXT(
				"IGMissingFloor",
				"P2CctvThought1",
				"저 복도는 어디지? 여기 들어올 때는 없었는데."),
			4.4f);
		return;
	}
	// 화면이 살아 있는 동안은 글이 없다. 보는 동안 읽게 하면 둘 다 놓친다.
	// 스노우 0.32초, 화면 5.60초, 찢어짐 0.86초가 지나고 숨 한 번 뒤에
	// 남은 것만 말한다 — 낮게 지나간 것은 본 사람의 몫으로 남긴다.
	GetWorldTimerManager().SetTimer(
		CctvThoughtTimer,
		FTimerDelegate::CreateWeakLambda(this, [this]()
		{
			AIGHorrorHUD::PushThought(
				this,
				NSLOCTEXT(
					"IGMissingFloor",
					"P2CctvThought1",
					"저 복도는 어디지? 여기 들어올 때는 없었는데."),
				4.4f);
			// §5.5. The picture is on screen and already unrecoverable: nothing
			// about channel 5 reaches the recorder, and the label on the case
			// says so. This is the line that keeps 위험 8 — 화면과 규칙의 모순 —
			// from being possible.
			AIGHorrorHUD::PushThought(
				this,
				NSLOCTEXT(
					"IGMissingFloor",
					"P2CctvThought2",
					"저장은 안 되는 채널이다. 화면부터 기억해 두자."),
				4.2f);
		}),
		0.32f + AIGCctvChannelFive::LiveSeconds + 0.86f + 0.25f,
		false);
}

void AIGMissingFloorPuzzleTwoDirector::HandleBoardReceiptsRead(
	AIGReadableNote* Note,
	const bool bOpened)
{
	// 펼친 순간에 적는다. 두 날짜가 같은 종이에 있으므로 볼 것은 다 봤다.
	if (!bOpened)
	{
		return;
	}
	UIGMissingFloorNarrativeSubsystem* Narrative = GetNarrative();
	if (!Narrative)
	{
		return;
	}
	Narrative->RegisterTruthSource(
		EIGMissingFloorTruth::WallSealedThatDay,
		EIGMissingFloorSource::BoardDeliveryReceipt);
	if (Narrative->MarkBeatPlayed(FName(TEXT("Night2.BoardReceipts"))))
	{
		AIGHorrorHUD::PushThought(
			this,
			NSLOCTEXT(
				"IGMissingFloor",
				"P2ReceiptsThought",
				"같은 보드를 26일, 27일 이틀로 나눠 샀다. 대장에는 하루만."),
			4.6f);
	}
}

void AIGMissingFloorPuzzleTwoDirector::HandleWallCalendarExamined(
	AIGMissingFloorEvidence* Evidence)
{
	if (UIGMissingFloorNarrativeSubsystem* Narrative = GetNarrative())
	{
		Narrative->RecordWitness(EIGMissingFloorWitness::BoothWallCalendar);
	}
}

void AIGMissingFloorPuzzleTwoDirector::HandleInnerRoomListenExamined(
	AIGMissingFloorEvidence* Evidence)
{
	UIGMissingFloorNarrativeSubsystem* Narrative = GetNarrative();
	if (!Narrative)
	{
		return;
	}
	Narrative->RecordWitness(EIGMissingFloorWitness::BoothInnerRoomHum);
	IGAudio::SpawnOneShotAt(
		this,
		UIGToneSequenceSoundWave::CreateFoamedRoomHum(this),
		Evidence ? Evidence->GetActorLocation() : GetActorLocation(),
		0.60f,
		1.0f,
		80.0f,
		360.0f,
		EIGAudioBus::World);
	AIGHorrorHUD::PushAudioCaption(
		this,
		NSLOCTEXT(
			"IGMissingFloor",
			"P2InnerRoomCaption",
			"[문 너머, 낮게] 뭔가 계속 돌고 있다"),
		3.0f);
	AIGHorrorHUD::PushThought(
		this,
		NSLOCTEXT(
			"IGMissingFloor",
			"P2InnerRoomThought",
			"안에 누가 있다. 지금 문을 열면 들키겠어."),
		4.4f);
}

void AIGMissingFloorPuzzleTwoDirector::HandleRecorderBayExamined(
	AIGMissingFloorEvidence* Evidence)
{
	if (UIGMissingFloorNarrativeSubsystem* Narrative = GetNarrative())
	{
		Narrative->RecordWitness(EIGMissingFloorWitness::RecorderEmptyBay);
	}
}

void AIGMissingFloorPuzzleTwoDirector::HandleFoamExamined(
	AIGMissingFloorEvidence* Evidence)
{
	UIGMissingFloorNarrativeSubsystem* Narrative = GetNarrative();
	if (!Narrative || !Narrative->MarkBeatPlayed(IGPuzzleTwo::FoamBeatId))
	{
		return;
	}
	// §22.3. 이 문틈은 진실을 열지 않는다 — 열어야 할 진실이 없다. 대신
	// 본 사람의 엔딩 자막에서 「듣지 않은 사람」이 이름을 얻는다.
	Narrative->RecordWitness(EIGMissingFloorWitness::BoothSoundproofing);
	AIGHorrorHUD::PushThought(
		this,
		NSLOCTEXT(
			"IGMissingFloor",
			"P2FoamThought",
			"문틈까지 막아 놨다. 여기 안에서는 바깥 소리가 안 들리겠네."),
		4.6f);
}

void AIGMissingFloorPuzzleTwoDirector::HandleTruthConfirmed(
	const EIGMissingFloorTruth Truth)
{
	if (Truth != EIGMissingFloorTruth::WasStillAlive)
	{
		return;
	}
	// 문자부터 읽었든 먹지부터 복원했든, 둘을 모으면 같은 지점에 도착한다.
	HandleCarbonRestored(CarbonLedger);
}

bool AIGMissingFloorPuzzleTwoDirector::ValidateFixtures() const
{
	return BoothDoor != nullptr
		&& FairCopyLedger != nullptr
		&& CarbonLedger != nullptr
		&& AgentMessageNote != nullptr
		&& CctvSelector != nullptr
		&& BoardReceipts != nullptr
		&& FoamGap != nullptr;
}

UIGMissingFloorNarrativeSubsystem*
AIGMissingFloorPuzzleTwoDirector::GetNarrative() const
{
	const UWorld* World = GetWorld();
	UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
	return GameInstance
		? GameInstance->GetSubsystem<UIGMissingFloorNarrativeSubsystem>()
		: nullptr;
}
