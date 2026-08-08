#include "Entity/IGMissingFloorPuzzleTwoDirector.h"

#include "Audio/IGAudioHelpers.h"
#include "Audio/IGToneSequenceSoundWave.h"
#include "Core/IGPrologueWorldScene.h"
#include "Engine/GameInstance.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Entity/IGMissingFloorEvidence.h"
#include "Interaction/IGReadableNote.h"
#include "Interaction/IGSwingDoor.h"
#include "Materials/MaterialInterface.h"
#include "Narrative/IGMissingFloorNarrativeSubsystem.h"
#include "Player/IGHorrorHUD.h"

namespace IGPuzzleTwo
{
	// Booth-interior coordinates, ground floor (Z 0). Matched to the booth
	// BuildLobby erects behind the connector's north wall; duplicated by the
	// same rule every night director follows, because the scene's coordinate
	// namespace is .cpp-local.
	const FVector BoothDoorHinge(120.0f, -235.0f, 0.0f);
	const FVector FairCopyLocation(120.0f, -103.0f, 80.0f);
	const FVector CarbonLocation(160.0f, -103.0f, 79.0f);
	const FVector AgentNoteLocation(205.0f, -103.0f, 80.0f);
	const FVector CctvLocation(150.0f, -94.0f, 96.0f);
	const FVector FoamGapLocation(272.0f, -90.0f, 105.0f);

	/** §5.1: frottage is a sustained 0.25 — three times, on purpose. */
	constexpr float FrottageLoudness = 0.25f;
	constexpr float FrottageHoldSeconds = 1.2f;

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
	BoothDoor->ConfigurePrototypeVisuals(
		CubeMesh,
		nullptr,
		nullptr,
		FVector(6.0f, 80.0f, 204.0f));
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
		CubeMesh, nullptr, FVector(21.0f, 1.2f, 29.7f));
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
				"* 볼펜 필체가 유난히 눌려 있다. 아래 장에 자국이 남을 만큼."),
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
		CubeMesh,
		nullptr,
		FVector(21.0f, 1.0f, 29.7f),
		NSLOCTEXT("IGMissingFloor", "P2CarbonPrompt", "먹지 — 문지른다"),
		NSLOCTEXT(
			"IGMissingFloor",
			"P2CarbonRestored",
			"「벽에서 쿵쿵. 사람 소리 같다」… 7월 27일. 퇴거했다는 날 다음이잖아."),
		EIGMissingFloorTruth::WasStillAlive,
		EIGMissingFloorSource::CarbonLedgerOriginal,
		IGPuzzleTwo::FrottageHoldSeconds,
		IGPuzzleTwo::FrottageLoudness);
	CarbonLedger->SetProgressiveStages({
		NSLOCTEXT(
			"IGMissingFloor",
			"P2CarbonStage1",
			"…글자가 비친다. 연필을 더 눕혀서."),
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
		FTransform(FRotator::ZeroRotator, IGPuzzleTwo::AgentNoteLocation),
		SpawnParameters);
	if (!AgentMessageNote)
	{
		return false;
	}
	AgentMessageNote->ConfigurePrototypeVisuals(
		CubeMesh, nullptr, FVector(18.0f, 1.2f, 24.0f));
	AgentMessageNote->SetInteractionPrompt(
		NSLOCTEXT("IGMissingFloor", "P2AgentPrompt", "출력된 문자 사본"));
	AgentMessageNote->SetNoteText(
		NSLOCTEXT("IGMissingFloor", "P2AgentTitle", "무영부동산 문자 사본"),
		{
			NSLOCTEXT("IGMissingFloor", "P2Agent1", "[7/26 14:02] 사장님, 5층 짐 뺐습니다."),
			NSLOCTEXT("IGMissingFloor", "P2Agent2", "[7/26 14:05] 네. 방은 창고로 되돌립니다."),
			FText::GetEmpty(),
			NSLOCTEXT(
				"IGMissingFloor",
				"P2Agent3",
				"* 퇴거는 7월 26일. 민원 대장의 쿵쿵 소리는 그 뒤 닷새다."),
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
		nullptr,
		FVector(10.0f, 4.0f, 6.0f),
		NSLOCTEXT("IGMissingFloor", "P2CctvPrompt", "채널 선택기 — 5"),
		FText::GetEmpty(),
		EIGMissingFloorTruth::None,
		EIGMissingFloorSource::None,
		0.0f,
		0.06f);
	CctvSelector->OnExamined.AddUObject(
		this, &AIGMissingFloorPuzzleTwoDirector::HandleCctvExamined);

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
		0.05f);
	FoamGap->OnExamined.AddUObject(
		this, &AIGMissingFloorPuzzleTwoDirector::HandleFoamExamined);

	// The night-2 goal is a truth, not a button: listen for T7.
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
	Super::EndPlay(EndPlayReason);
}

void AIGMissingFloorPuzzleTwoDirector::SetHourActive(const bool bHourActive)
{
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
			"…안에 있다. 낮엔 열어 주지 않는다.");
		BoothDoor->SetRequirements(MoveTemp(DayLock));
	}
}

void AIGMissingFloorPuzzleTwoDirector::HandleCarbonRestored(
	AIGMissingFloorEvidence* Evidence)
{
	if (UIGMissingFloorNarrativeSubsystem* Narrative = GetNarrative())
	{
		Narrative->MarkPuzzleSolved(IGPuzzleTwo::PuzzleId);
	}
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
	AIGHorrorHUD::PushThought(
		this,
		NSLOCTEXT(
			"IGMissingFloor",
			"P2CctvThought1",
			"5번. …복도가 하나 더 있다. 자재, 비닐 — 그리고 낮게 지나가는 것."),
		4.4f);
	AIGHorrorHUD::PushAudioCaption(
		this,
		NSLOCTEXT("IGMissingFloor", "P2CctvCaption", "화면 지직임"),
		1.6f);
}

void AIGMissingFloorPuzzleTwoDirector::HandleFoamExamined(
	AIGMissingFloorEvidence* Evidence)
{
	UIGMissingFloorNarrativeSubsystem* Narrative = GetNarrative();
	if (!Narrative || !Narrative->MarkBeatPlayed(IGPuzzleTwo::FoamBeatId))
	{
		return;
	}
	AIGHorrorHUD::PushThought(
		this,
		NSLOCTEXT(
			"IGMissingFloor",
			"P2FoamThought",
			"문틈까지 계란판이다. …못 들은 게 아니라, 안 들리게 만들었구나."),
		4.6f);
}

void AIGMissingFloorPuzzleTwoDirector::HandleTruthConfirmed(
	const EIGMissingFloorTruth Truth)
{
	if (bSolvedAnnounced || Truth != EIGMissingFloorTruth::WasStillAlive)
	{
		return;
	}
	bSolvedAnnounced = true;
	OnSolved.Broadcast();
}

bool AIGMissingFloorPuzzleTwoDirector::ValidateFixtures() const
{
	return BoothDoor != nullptr
		&& FairCopyLedger != nullptr
		&& CarbonLedger != nullptr
		&& AgentMessageNote != nullptr
		&& CctvSelector != nullptr
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
