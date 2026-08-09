#include "Entity/IGMissingFloorNightThreeDirector.h"

#include "Audio/IGAudioHelpers.h"
#include "Audio/IGToneSequenceSoundWave.h"
#include "Components/AudioComponent.h"
#include "Core/IGPrologueWorldScene.h"
#include "Engine/GameInstance.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Entity/IGMissingFloorEvidence.h"
#include "Interaction/IGReadableNote.h"
#include "Interaction/IGStairTransition.h"
#include "Interaction/IGSwingDoor.h"
#include "Narrative/IGMissingFloorNarrativeSubsystem.h"
#include "Narrative/IGStoryHelpers.h"
#include "Player/IGHorrorHUD.h"
#include "TimerManager.h"

namespace IGNightThree
{
	// The stub-top gate and its threshold, corridor-local + 900 (world).
	const FVector StairGateHinge(-317.0f, -150.0f, 972.0f);
	const FVector StubPortal(-277.5f, -122.0f, 1040.0f);
	const FVector StubReturnExit(-277.5f, -180.0f, 1064.0f);

	// The annex, in its own detached world block north of the villa. The
	// arrival point stands clear of the portal volume, or landing would
	// immediately teleport the player straight back down.
	const FVector AnnexPortal(-370.0f, 700.0f, 1290.0f);
	const FVector AnnexArrival(-285.0f, 700.0f, 1292.0f);

	// Annex contents.
	const FVector NotebookLocation(-90.0f, 770.0f, 1246.0f);
	const FVector WandLocation(-40.0f, 755.0f, 1245.0f);
	const FVector ValveLocation(296.0f, 610.0f, 1266.0f);
	const FVector ImpactMarkLocation(-10.0f, 585.0f, 1264.0f);
	const float WallBayYs[3] = {560.0f, 700.0f, 840.0f};
	constexpr int32 CavityBayIndex = 1;

	// The booth keyring, on the desk's west end.
	const FVector KeyringLocation(118.0f, -96.0f, 80.0f);

	// Day papers: the mover's labels in 403, the forum printout by the
	// mailboxes, the tally journal at 401's threshold once it is earned.
	const FVector LabelsLocation(-95.0f, -185.0f, 978.0f);
	const FVector ForumLocation(560.0f, -249.0f, 143.0f);
	const FVector JournalLocation(-172.0f, -237.5f, 985.0f);

	/** §5.1: a fist on gypsum carries; leaning an ear does not. */
	constexpr float KnockLoudness = 0.35f;
	constexpr float ListenLoudness = 0.05f;
	constexpr float ValveLoudness = 0.55f;

	/** Eight seconds of nothing before the wall comes back. */
	constexpr float AnswerDelaySeconds = 8.0f;

	const FName PuzzleThreeId(TEXT("P3"));
	const FName PuzzleFourId(TEXT("P4"));
}

AIGMissingFloorNightThreeDirector::AIGMissingFloorNightThreeDirector()
{
	PrimaryActorTick.bCanEverTick = false;
}

bool AIGMissingFloorNightThreeDirector::Configure(AIGPrologueWorldScene* InScene)
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

	FActorSpawnParameters SpawnParameters;
	SpawnParameters.SpawnCollisionHandlingOverride =
		ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	// The fifth-floor gate. Locked on a real, persistent fact — the keyring
	// tag — because owning a key is exactly the kind of thing a save should
	// remember (unlike the hour's seal, which must never be written).
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
		CubeMesh, nullptr, nullptr, FVector(6.0f, 80.0f, 170.0f));
	StairGate->SetOpenYaw(-95.0f);
	{
		TArray<FIGDoorRequirement> GateRequirements;
		FIGDoorRequirement& KeyLock = GateRequirements.AddDefaulted_GetRef();
		KeyLock.RequiredState = FGameplayTag::RequestGameplayTag(
			FName(TEXT("State.MissingFloor.HasStairKey")), false);
		KeyLock.LockedPrompt =
			NSLOCTEXT("IGMissingFloor", "StairGatePrompt", "5층 철문");
		KeyLock.LockedThought = NSLOCTEXT(
			"IGMissingFloor",
			"StairGateThought",
			"잠겼다. …관리실 열쇠 걸이에 이런 게 있었지.");
		StairGate->SetRequirements(MoveTemp(GateRequirements));
	}

	// The teleport that swallows the unbuilt flights between the stub and
	// the annex — the same trick the lobby stair already plays. The actor is
	// spawned yawed 90°, which rotates its portal boxes with it: their long
	// axis then runs across the stair, so the lower volume stays strictly
	// north of the gate plane and cannot swallow a player who has not
	// opened the door yet.
	SpawnParameters.Name = TEXT("MissingFloorAnnexTransition");
	AnnexTransition = World->SpawnActor<AIGStairTransition>(
		AIGStairTransition::StaticClass(),
		FTransform(FRotator(0.0f, 90.0f, 0.0f), FVector::ZeroVector),
		SpawnParameters);
	if (!AnnexTransition)
	{
		return false;
	}
	AnnexTransition->Configure(
		IGNightThree::AnnexPortal,
		IGNightThree::AnnexArrival,
		FRotator(0.0f, 0.0f, 0.0f),
		IGNightThree::StubPortal,
		IGNightThree::StubReturnExit,
		FRotator(0.0f, 180.0f, 0.0f));

	// The keyring, hanging in the booth the hour leaves open.
	SpawnParameters.Name = TEXT("MissingFloorKeyring");
	Keyring = World->SpawnActor<AIGMissingFloorEvidence>(
		AIGMissingFloorEvidence::StaticClass(),
		FTransform(FRotator::ZeroRotator, IGNightThree::KeyringLocation),
		SpawnParameters);
	if (!Keyring)
	{
		return false;
	}
	Keyring->Configure(
		CubeMesh,
		nullptr,
		FVector(7.0f, 3.0f, 9.0f),
		NSLOCTEXT("IGMissingFloor", "KeyringPrompt", "열쇠뭉치"),
		NSLOCTEXT(
			"IGMissingFloor",
			"KeyringThought",
			"계단 철문 열쇠다. …왜 관리실에만 있을까."),
		EIGMissingFloorTruth::None,
		EIGMissingFloorSource::None,
		0.0f,
		0.1f);
	Keyring->OnExamined.AddUObject(
		this, &AIGMissingFloorNightThreeDirector::HandleKeyringTaken);

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
	TunerNotebook->ConfigurePrototypeVisuals(
		CubeMesh, nullptr, FVector(16.0f, 1.4f, 22.0f));
	TunerNotebook->SetInteractionPrompt(
		NSLOCTEXT("IGMissingFloor", "NotebookPrompt", "조율 수첩"));
	TunerNotebook->SetNoteText(
		NSLOCTEXT("IGMissingFloor", "NotebookTitle", "조율 수첩 — 백도하"),
		{
			NSLOCTEXT("IGMissingFloor", "Notebook1", "월 — 서초 라이브홀, 마감 02:30. 첫차 전 귀가."),
			NSLOCTEXT("IGMissingFloor", "Notebook2", "수 — 대치 학원 업라이트 4대. 밤 작업."),
			FText::GetEmpty(),
			NSLOCTEXT("IGMissingFloor", "Notebook3", "공명이 있는 벽은 비어 있는 벽이다."),
			NSLOCTEXT("IGMissingFloor", "Notebook4", "속이 찬 벽은 짧게 죽고, 빈 벽은 길게 운다."),
			FText::GetEmpty(),
			NSLOCTEXT("IGMissingFloor", "Notebook5", "여백마다 같은 낙서 — ●● ○ ●"),
			NSLOCTEXT("IGMissingFloor", "Notebook6", "…아빠 노크다. 유담이 방문에 하던 그거."),
			FText::GetEmpty(),
			NSLOCTEXT("IGMissingFloor", "Notebook7", "뒷장 적금 표: 「유담 피아노」 — 칸이 거의 다 지워져 있다."),
		});
	TunerNotebook->OnReadStateChanged.AddDynamic(
		this, &AIGMissingFloorNightThreeDirector::HandleNotebookRead);

	SpawnParameters.Name = TEXT("MissingFloorTunerWand");
	TunerWand = World->SpawnActor<AIGMissingFloorEvidence>(
		AIGMissingFloorEvidence::StaticClass(),
		FTransform(FRotator(0.0f, 0.0f, 90.0f), IGNightThree::WandLocation),
		SpawnParameters);
	if (!TunerWand)
	{
		return false;
	}
	TunerWand->Configure(
		CylinderMesh,
		nullptr,
		FVector(3.0f, 3.0f, 26.0f),
		NSLOCTEXT("IGMissingFloor", "WandPrompt", "청음봉"),
		NSLOCTEXT(
			"IGMissingFloor",
			"WandThought",
			"조율사의 것. …오른손잡이용인데, 왜 여기 떨어져 있지."),
		EIGMissingFloorTruth::None,
		EIGMissingFloorSource::None,
		0.0f,
		0.06f);

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
		nullptr,
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
			"모서리가 검게 물들어 있다. …누가 여기 머리부터 넘어졌다."),
		EIGMissingFloorTruth::LandingStruggle,
		EIGMissingFloorSource::LandingImpactMark,
		0.0f,
		0.05f);
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
			IGNightThree::ListenLoudness);
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
			IGNightThree::KnockLoudness);
		Knock->OnExamined.AddWeakLambda(
			this,
			[this, BayIndex](AIGMissingFloorEvidence*)
			{
				HandleWallKnocked(BayIndex);
			});
		WallKnocks[BayIndex] = Knock;
	}

	// P4's surface on the cavity bay: hidden until the wall is certain and
	// the rhythm is understood. No prompt ever explains the pattern — the
	// player brings it.
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
			"둘, 쉬고, 하나 — 두드린다"),
		FText::GetEmpty(),
		EIGMissingFloorTruth::None,
		EIGMissingFloorSource::None,
		0.8f,
		IGNightThree::KnockLoudness);
	AnswerTarget->SetInteractionEnabled(false);
	AnswerTarget->SetActorHiddenInGame(true);
	AnswerTarget->OnExamined.AddUObject(
		this, &AIGMissingFloorNightThreeDirector::HandleAnswerKnock);

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
		CubeMesh, nullptr, FVector(18.0f, 1.2f, 13.0f));
	LabelsNote->SetInteractionPrompt(
		NSLOCTEXT("IGMissingFloor", "LabelsPrompt", "배송 라벨 뭉치"));
	LabelsNote->SetNoteText(
		NSLOCTEXT("IGMissingFloor", "LabelsTitle", "공방 창고에서 온 상자"),
		{
			NSLOCTEXT("IGMissingFloor", "Labels1", "받는 사람: 백도하"),
			NSLOCTEXT("IGMissingFloor", "Labels2", "무영로 27-3 달빛빌라 5"),
			FText::GetEmpty(),
			NSLOCTEXT(
				"IGMissingFloor",
				"Labels3",
				"* 다섯 장 전부 같은 손글씨 주소. 호수는 끝내 없다."),
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
		CubeMesh, nullptr, FVector(19.0f, 1.2f, 26.0f));
	ForumNote->SetInteractionPrompt(
		NSLOCTEXT("IGMissingFloor", "ForumPrompt", "게시글 출력물"));
	ForumNote->SetNoteText(
		NSLOCTEXT("IGMissingFloor", "ForumTitle", "층간소음 카페 — 인쇄본"),
		{
			NSLOCTEXT("IGMissingFloor", "Forum1", "6/30 새벽마다 위에서 끌고 두드리는 소리. 미치겠다."),
			NSLOCTEXT("IGMissingFloor", "Forum2", "7/12 관리인은 위층이 없다고 한다. 없는 층이 뭘 끄나."),
			FText::GetEmpty(),
			NSLOCTEXT("IGMissingFloor", "Forum3", "7/26 03:12 오늘은 올라가 본다. 얼굴이나 보자."),
			NSLOCTEXT("IGMissingFloor", "Forum4", "└ 댓글 12 — 전부 「참지 마세요」."),
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
	JournalNote->ConfigurePrototypeVisuals(
		CubeMesh, nullptr, FVector(17.0f, 1.4f, 24.0f));
	JournalNote->SetInteractionPrompt(
		NSLOCTEXT("IGMissingFloor", "JournalPrompt", "달력 뒷장 소리 일지"));
	JournalNote->SetNoteText(
		NSLOCTEXT("IGMissingFloor", "JournalTitle", "황순금의 일지"),
		{
			NSLOCTEXT("IGMissingFloor", "Journal1", "7/27 쿵 다섯. 벽이 운다."),
			NSLOCTEXT("IGMissingFloor", "Journal2", "7/28 쿵 다섯. 어제보다 힘이 없다."),
			NSLOCTEXT("IGMissingFloor", "Journal3", "7/29 벽이 하도 울어서 나도 두드려줬다. 그랬더니 조용하데. 사람인가."),
			NSLOCTEXT("IGMissingFloor", "Journal4", "7/30 쿵 넷."),
			NSLOCTEXT("IGMissingFloor", "Journal5", "7/31 오늘은 세 번뿐."),
			FText::GetEmpty(),
			NSLOCTEXT("IGMissingFloor", "Journal6", "그 뒤로는 빈 칸이다."),
		});
	JournalNote->OnReadStateChanged.AddDynamic(
		this, &AIGMissingFloorNightThreeDirector::HandleJournalRead);
	// Earned, not found: she hands it out only after the truth of the
	// knocking is known, and only by daylight.
	RefreshJournalAvailability(/*bHourActive=*/true);

	if (UIGMissingFloorNarrativeSubsystem* Narrative = GetNarrative())
	{
		TruthHandle = Narrative->OnTruthConfirmed.AddUObject(
			this, &AIGMissingFloorNightThreeDirector::HandleTruthConfirmed);
	}
	return true;
}

void AIGMissingFloorNightThreeDirector::EndPlay(
	const EEndPlayReason::Type EndPlayReason)
{
	GetWorldTimerManager().ClearTimer(AnswerTimer);
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
	RefreshJournalAvailability(bHourActive);
}

AIGMissingFloorEvidence* AIGMissingFloorNightThreeDirector::GetWallListen(
	const int32 BayIndex) const
{
	return WallListens.IsValidIndex(BayIndex) ? WallListens[BayIndex] : nullptr;
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
		UIGToneSequenceSoundWave::CreateHatchOpenMetal(this),
		IGNightThree::ValveLocation,
		0.7f,
		0.8f);

	// Water starts moving behind exactly one of three identical walls.
	if (UWorld* World = GetWorld())
	{
		RiserFlow = NewObject<UAudioComponent>(this, TEXT("RiserFlowBed"));
		RiserFlow->RegisterComponent();
		RiserFlow->SetWorldLocation(FVector(
			310.0f,
			IGNightThree::WallBayYs[IGNightThree::CavityBayIndex],
			1300.0f));
		RiserFlow->SetSound(
			UIGToneSequenceSoundWave::CreateFloodedCorridorWaterBed(this));
		RiserFlow->AttenuationSettings = IGAudio::MakeAttenuation(this, 120.0f, 900.0f);
		RiserFlow->bAllowSpatialization = true;
		RiserFlow->SetVolumeMultiplier(0.5f);
		RiserFlow->Play();
	}

	if (UIGMissingFloorNarrativeSubsystem* Narrative = GetNarrative())
	{
		Narrative->MarkPuzzleSolved(IGNightThree::PuzzleThreeId);
	}
	AIGHorrorHUD::PushThought(
		this,
		NSLOCTEXT(
			"IGMissingFloor",
			"ValveThought",
			"물이 내려간다. …이제 벽들이 서로 달라졌다."),
		3.8f);
}

void AIGMissingFloorNightThreeDirector::HandleWallListened(const int32 BayIndex)
{
	UIGMissingFloorNarrativeSubsystem* Narrative = GetNarrative();
	if (!bValveOpen)
	{
		AIGHorrorHUD::PushThought(
			this,
			NSLOCTEXT(
				"IGMissingFloor",
				"ListenSilent",
				"…조용하다. 세 벽이 전부 똑같이 조용하다."),
			3.2f);
		return;
	}
	if (BayIndex == IGNightThree::CavityBayIndex)
	{
		AIGHorrorHUD::PushThought(
			this,
			NSLOCTEXT(
				"IGMissingFloor",
				"ListenCavity",
				"바로 뒤에서 흐른다. …이 벽만 속이 비었다."),
			4.0f);
		if (Narrative)
		{
			Narrative->RegisterTruthSource(
				EIGMissingFloorTruth::SomeoneInTheWall,
				EIGMissingFloorSource::PipeWaterComparison);
		}
		return;
	}
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
		0.8f);

	if (BayIndex == IGNightThree::CavityBayIndex)
	{
		AIGHorrorHUD::PushThought(
			this,
			NSLOCTEXT(
				"IGMissingFloor",
				"KnockCavity",
				"…길게 운다. 속이 빈 소리다."),
			3.8f);
		if (UIGMissingFloorNarrativeSubsystem* Narrative = GetNarrative())
		{
			Narrative->RegisterTruthSource(
				EIGMissingFloorTruth::SomeoneInTheWall,
				EIGMissingFloorSource::WallEchoByHand);
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
	bAnswerPending = true;

	// The player's own hand: two, a rest, one, at the wall.
	IGAudio::SpawnOneShotAt(
		this,
		UIGToneSequenceSoundWave::CreateAnswerKnockPattern(this, 0.0f),
		Evidence ? Evidence->GetActorLocation() : GetActorLocation(),
		0.9f);
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
		1200.0f);

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
		EIGMissingFloorSource::AnswerRhythmMaterials);
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

void AIGMissingFloorNightThreeDirector::RefreshAnswerTargetAvailability()
{
	if (!AnswerTarget || bAnswerDelivered)
	{
		return;
	}
	const UIGMissingFloorNarrativeSubsystem* Narrative = GetNarrative();
	const bool bReady =
		Narrative
		&& Narrative->HasTruth(EIGMissingFloorTruth::SomeoneInTheWall)
		&& Narrative->HasSource(
			EIGMissingFloorTruth::WaitingForAnAnswer,
			EIGMissingFloorSource::AnswerRhythmMaterials);
	AnswerTarget->SetActorHiddenInGame(!bReady);
	AnswerTarget->SetInteractionEnabled(bReady);
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
	return StairGate != nullptr
		&& AnnexTransition != nullptr
		&& Keyring != nullptr
		&& TunerNotebook != nullptr
		&& TunerWand != nullptr
		&& RiserValve != nullptr
		&& WallListens.Num() == 3
		&& WallKnocks.Num() == 3
		&& ImpactMark != nullptr
		&& AnswerTarget != nullptr
		&& LabelsNote != nullptr
		&& ForumNote != nullptr
		&& JournalNote != nullptr;
}
