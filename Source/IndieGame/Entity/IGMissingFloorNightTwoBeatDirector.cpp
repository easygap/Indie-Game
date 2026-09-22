#include "Entity/IGMissingFloorNightTwoBeatDirector.h"

#include "Audio/IGAudioHelpers.h"
#include "Audio/IGToneSequenceSoundWave.h"
#include "Components/AudioComponent.h"
#include "Core/IGPrologueWorldScene.h"
#include "Engine/GameInstance.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Entity/IGListenerEntity.h"
#include "Entity/IGMissingFloorEvidence.h"
#include "Entity/IGNoiseSubsystem.h"
#include "Narrative/IGMissingFloorNarrativeSubsystem.h"
#include "Narrative/IGRecordingSubsystem.h"
#include "Player/IGHorrorHUD.h"
#include "Player/IGPlayerCharacter.h"
#include "Player/IGStressComponent.h"

namespace IGNightTwo
{
	/**
	 * 4층 세계 좌표. 남쪽 벽이 Y=-225이므로 집
	 * 안쪽은 Y가 0에 가까운 쪽, 복도는 그 반대쪽이다. 층 높이는 씬이 들고
	 * 있고, 문·노크·문구멍은 그 위에서 잰다.
	 */
	constexpr float FourthFloorZ = AIGPrologueWorldScene::FourthFloorZ;
	const FVector DoorLocation(
		AIGPrologueWorldScene::HomeDoorX + AIGPrologueWorldScene::WideDoorLeafWidth * 0.5f,
		AIGPrologueWorldScene::HomeDoorY,
		FourthFloorZ);
	// 아래 셋은 문에서 잰다. 예전에는 절대 좌표를 적고 관계는 주석에만
	// 두었는데, 그러면 문을 옮겼을 때 노크가 벽에서 나고 문구멍이
	// 복도를 본다. 복도는 Y가 작아지는 쪽이다.
	/** 노크는 복도 쪽 문짝에서 난다. 주먹 높이. */
	constexpr float KnockCorridorOffset = 7.0f;
	constexpr float KnockFistHeight = 112.0f;
	const FVector KnockLocation =
		DoorLocation + FVector(0.0f, -KnockCorridorOffset, KnockFistHeight);
	/** 문구멍은 문 안쪽, 눈높이. */
	constexpr float PeepholeInsideOffset = 5.0f;
	constexpr float PeepholeEyeHeight = 155.0f;
	const FVector PeepholeLocation =
		DoorLocation + FVector(0.0f, PeepholeInsideOffset, PeepholeEyeHeight);
	/** 그가 서는 자리 — 문에서 47 cm, 복도 안. */
	constexpr float FigureStandOffset = 47.0f;
	const FVector FigureStagePoint =
		DoorLocation + FVector(0.0f, -FigureStandOffset, 0.0f);
	/** 두 점 사이를 오가게 둔다. 서 있는 사람이 아니라 기다리는 사람이 된다. */
	const FVector FigureShufflePoint = DoorLocation + FVector(8.0f, -51.0f, 0.0f);
	/** 끌려가는 소리는 계단코어 쪽으로 멀어진다. */
	const FVector DragDepartPoint(-120.0f, -278.0f, FourthFloorZ);

	/**
	 * 04:30 직후가 아니라 6초 뒤다. 밤이 시작한 프레임에 노크가 나면 연출이
	 * 아니라 로딩의 일부로 읽힌다.
	 */
	constexpr float OpeningDelaySeconds = 6.0f;
	/**
	 * 인내 시간. 문구멍을 안 보는 플레이어도, 폰을 안 켜는 플레이어도 막히지
	 * 않는다 — 다만 테이프에는 남지 않는다. §20.3의 규칙은 퍼즐만이 아니라
	 * 비트에도 적용된다.
	 */
	constexpr float PeepholePatienceSeconds = 40.0f;
	constexpr float PhonePatienceSeconds = 55.0f;
	/** 폰이 놓인 뒤 3연까지의 숨. */
	constexpr float TripleDelaySeconds = 1.6f;
	/** 기다림. 이 침묵이 이 비트에서 가장 긴 시간이다. */
	constexpr float WaitAfterTripleSeconds = 3.4f;
	/** 끌려가는 소리가 복도를 빠져나가는 데 걸리는 시간. */
	constexpr float DragSeconds = 4.4f;
	constexpr float PollSeconds = 0.25f;

	/**
	 * §21.2 표의 값. 3연은 1.0 — 「건물이 그로 가득 차는」 2초이고, §5.5의
	 * 무음 길이 표가 그 1.0을 정확히 2.10초로 환산한다. 여는 노크 한 번은
	 * 그보다 작아야 한다: 그것은 부르는 소리이고 3연은 대답을 요구하는 소리다.
	 */
	constexpr float SingleKnockLoudness = 0.55f;
	constexpr float TripleKnockLoudness = 1.0f;
	constexpr float DragLoudness = 0.42f;

	/** 강철 현관문 한 장. 3연 때 그는 문에 더 붙어 있다. */
	constexpr float SingleKnockMuffle = 0.72f;
	constexpr float TripleKnockMuffle = 0.66f;

	constexpr float KnockVolume = 0.92f;
	constexpr float KnockInnerRadius = 220.0f;
	constexpr float KnockFalloff = 1500.0f;
	constexpr float DragVolume = 0.62f;

	const FName BeatId(TEXT("Night2.DoorKnock"));
	const FName PeepholeBeatId(TEXT("Night2.Peephole"));
	const FName ReturnBeatId(TEXT("Night2.ReturnChase"));

	// -- 비트 2-5 「귀환 추격」 ---------------------------------------------
	/**
	 * 관리실 오른쪽 벽. 관리실은 1층이라 Z가 작다. 쌓아 둔 자재가
	 * 무너지는 자리이며, 소리의 출처가 눈에 보이게 BuildLobby가 같은 좌표에
	 * 판재 더미를 세워 둔다.
	 */
	const FVector CollapseLocation(260.0f, -166.0f, 24.0f);
	/**
	 * 두 번. 이것이 이 비트의 전부다 — §5의 규칙이 「두 번 대답하는 소리는
	 * 누군가다」이므로, 무너지는 더미의 첫 조각과 나머지가 실제 AI를 추격으로
	 * 넘긴다. 전용 추격 코드는 한 줄도 없다. 0.55초는 반응 기억 10초 안이고,
	 * 한 번의 사고로 들릴 만큼 붙어 있다.
	 */
	constexpr float CollapseSecondImpactSeconds = 0.55f;
	/**
	 * 0.95는 §8 표의 【S】와 같은 값이다. 반경은 크기 × 2600 cm이므로 2470 cm까지
	 * 실리고, 층간 감쇠 1.4배를 물어도 4층 복도의 그에게 닿는다. 계산이 아니라
	 * 그렇게 되도록 고른 값이다 — 이 비트가 확실히 추격이 되어야 한다.
	 */
	constexpr float CollapseLoudness = 0.95f;
	constexpr float CollapseVolume = 1.0f;
	constexpr float CollapseInnerRadius = 260.0f;
	constexpr float CollapseFalloff = 2100.0f;

	/** 관리실은 Y -235..-75. 이 선을 넘으면 나선 것이다. */
	constexpr float BoothExitY = -242.0f;
	/** 403호 실내는 4층 X -190..190, Y -235..235. */
	const FBox Unit403Interior(
		FVector(-190.0f, -235.0f, FourthFloorZ - 20.0f),
		FVector(190.0f, 235.0f, FourthFloorZ + 230.0f));
	constexpr float ReturnPollSeconds = 0.25f;
}

AIGMissingFloorNightTwoBeatDirector::AIGMissingFloorNightTwoBeatDirector()
{
	PrimaryActorTick.bCanEverTick = false;
}

bool AIGMissingFloorNightTwoBeatDirector::Configure(
	AIGPrologueWorldScene* InScene,
	AIGListenerEntity* InEntity,
	AIGPlayerCharacter* InPlayer,
	const TArray<FVector>& InCorridorPatrolPoints)
{
	UWorld* World = GetWorld();
	if (!World || !InScene || !InEntity)
	{
		return false;
	}
	Scene = InScene;
	Entity = InEntity;
	Player = InPlayer;
	CorridorPatrolPoints = InCorridorPatrolPoints;

	UStaticMesh* CubeMesh =
		LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (!CubeMesh)
	{
		return false;
	}

	// 문구멍. 카메라를 자르지 않는다 — 설계서가 이 건물 안의 어떤 이동도 컷으로
	// 대체하지 못하게 해 둔 것과 같은 이유다. 프롭은 그녀가 본 것을 말하고,
	// 복도의 형체는 실제로 거기 서 있으므로 문을 열면 그가 있다.
	FActorSpawnParameters SpawnParameters;
	SpawnParameters.SpawnCollisionHandlingOverride =
		ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	SpawnParameters.Name = TEXT("MissingFloorNightTwoPeephole");
	Peephole = World->SpawnActor<AIGMissingFloorEvidence>(
		AIGMissingFloorEvidence::StaticClass(),
		FTransform(FRotator::ZeroRotator, IGNightTwo::PeepholeLocation),
		SpawnParameters);
	if (!Peephole)
	{
		return false;
	}
	Peephole->Configure(
		CubeMesh,
		nullptr,
		FVector(3.0f, 2.4f, 3.0f),
		NSLOCTEXT("IGMissingFloor", "N2PeepholePrompt", "문구멍"),
		FText::GetEmpty(),
		EIGMissingFloorTruth::None,
		EIGMissingFloorSource::None,
		0.0f,
		0.0f,
		/*bPresentationVisible=*/false);
	Peephole->OnExamined.AddUObject(
		this,
		&AIGMissingFloorNightTwoBeatDirector::HandlePeepholeExamined);
	// 노크가 나기 전에는 볼 이유가 없다. 밤새 문구멍이 켜져 있으면 비트가
	// 시작하기 전에 소진된다.
	Peephole->SetInteractionEnabled(false);
	return true;
}

void AIGMissingFloorNightTwoBeatDirector::EndPlay(
	const EEndPlayReason::Type EndPlayReason)
{
	GetWorldTimerManager().ClearTimer(StageTimer);
	GetWorldTimerManager().ClearTimer(ReturnTimer);
	GetWorldTimerManager().ClearTimer(CollapseTimer);
	GetWorldTimerManager().ClearTimer(DragFadeTimer);
	ReleaseFigure();
	Super::EndPlay(EndPlayReason);
}

UIGMissingFloorNarrativeSubsystem*
AIGMissingFloorNightTwoBeatDirector::GetNarrative() const
{
	const UWorld* World = GetWorld();
	UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
	return GameInstance
		? GameInstance->GetSubsystem<UIGMissingFloorNarrativeSubsystem>()
		: nullptr;
}

UIGNoiseSubsystem* AIGMissingFloorNightTwoBeatDirector::GetNoise() const
{
	UWorld* World = GetWorld();
	return World ? World->GetSubsystem<UIGNoiseSubsystem>() : nullptr;
}

UIGRecordingSubsystem*
AIGMissingFloorNightTwoBeatDirector::GetRecording() const
{
	UWorld* World = GetWorld();
	return World ? World->GetSubsystem<UIGRecordingSubsystem>() : nullptr;
}

void AIGMissingFloorNightTwoBeatDirector::SetHourActive(const bool bHourActive)
{
	const UIGMissingFloorNarrativeSubsystem* Narrative = GetNarrative();
	const bool bIsNightTwo = Narrative && Narrative->GetNightIndex() == 2;
	const bool bAlreadyPlayed =
		bPlayed || (Narrative && Narrative->HasBeatPlayed(IGNightTwo::BeatId));

	if (!bHourActive || !bIsNightTwo || bAlreadyPlayed)
	{
		// 새벽이 오거나 다른 밤이면 문구멍을 닫고 형체를 순찰로 돌려보낸다.
		// 귀환 추격도 함께 내린다 — 시간 초과로 새벽이 왔다면 목표는 이미
		// 끝났고, 낮에 폴링을 계속할 이유가 없다.
		GetWorldTimerManager().ClearTimer(StageTimer);
		GetWorldTimerManager().ClearTimer(ReturnTimer);
		if (ReturnStage != EIGNightTwoReturnStage::Home)
		{
			ReturnStage = EIGNightTwoReturnStage::Idle;
		}
		if (Peephole)
		{
			Peephole->SetInteractionEnabled(false);
		}
		ReleaseFigure();
		Stage = bAlreadyPlayed
			? EIGNightTwoBeatStage::Spent
			: EIGNightTwoBeatStage::Idle;
		return;
	}

	EnterStage(EIGNightTwoBeatStage::Opening);
	GetWorldTimerManager().SetTimer(
		StageTimer,
		this,
		&AIGMissingFloorNightTwoBeatDirector::AdvanceStage,
		IGNightTwo::PollSeconds,
		true);
}

void AIGMissingFloorNightTwoBeatDirector::EnterStage(
	const EIGNightTwoBeatStage NextStage)
{
	Stage = NextStage;
	StageSeconds = 0.0f;
}

void AIGMissingFloorNightTwoBeatDirector::AdvanceStage()
{
	StageSeconds += IGNightTwo::PollSeconds;

	switch (Stage)
	{
	case EIGNightTwoBeatStage::Opening:
		if (StageSeconds >= IGNightTwo::OpeningDelaySeconds)
		{
			PlayFirstKnock();
			EnterStage(EIGNightTwoBeatStage::AwaitingPeephole);
		}
		break;

	case EIGNightTwoBeatStage::AwaitingPeephole:
		// 봤거나, 안 봐도 시간이 지나면 넘어간다. 그는 그녀의 확인을 기다려
		// 주지 않는다.
		if (bPeepholeSeen
			|| StageSeconds >= IGNightTwo::PeepholePatienceSeconds)
		{
			EnterStage(EIGNightTwoBeatStage::AwaitingPhone);
		}
		break;

	case EIGNightTwoBeatStage::AwaitingPhone:
	{
		const UIGRecordingSubsystem* Recording = GetRecording();
		const bool bArmed = Recording && Recording->IsRecording();
		if (bArmed || StageSeconds >= IGNightTwo::PhonePatienceSeconds)
		{
			bRecordedAnswer = bArmed;
			EnterStage(EIGNightTwoBeatStage::Answering);
		}
		break;
	}

	case EIGNightTwoBeatStage::Answering:
	{
		// 3연 → 기다림 → 끌려가는 소리. 각 구간은 앞 구간이 끝난 시각으로만
		// 정해지므로 폴 간격이 바뀌어도 순서가 흐트러지지 않는다.
		const float TripleAt = IGNightTwo::TripleDelaySeconds;
		const float DragAt = TripleAt + IGNightTwo::WaitAfterTripleSeconds;
		const float DoneAt = DragAt + IGNightTwo::DragSeconds;
		if (KnockCount == 1 && StageSeconds >= TripleAt)
		{
			PlayAnswer();
		}
		else if (KnockCount == 2 && StageSeconds >= DragAt)
		{
			PlayDragAway();
		}
		else if (KnockCount >= 3 && StageSeconds >= DoneAt)
		{
			if (UIGMissingFloorNarrativeSubsystem* Narrative = GetNarrative())
			{
				Narrative->MarkBeatPlayed(IGNightTwo::BeatId);
			}
			bPlayed = true;
			GetWorldTimerManager().ClearTimer(StageTimer);
			if (Peephole)
			{
				Peephole->SetInteractionEnabled(false);
			}
			ReleaseFigure();
			EnterStage(EIGNightTwoBeatStage::Spent);
		}
		break;
	}

	default:
		GetWorldTimerManager().ClearTimer(StageTimer);
		break;
	}
}

void AIGMissingFloorNightTwoBeatDirector::PlayFirstKnock()
{
	// 한 번이다. 부르는 소리이고, 그녀를 문으로 데려오는 것이 전부다.
	IGAudio::SpawnOneShotAt(
		this,
		UIGToneSequenceSoundWave::CreateWallKnockSingle(
			this,
			IGNightTwo::SingleKnockMuffle),
		IGNightTwo::KnockLocation,
		IGNightTwo::KnockVolume,
		1.0f,
		IGNightTwo::KnockInnerRadius,
		IGNightTwo::KnockFalloff,
		EIGAudioBus::Entity);
	AIGHorrorHUD::PushAudioCaptionAt(
		this,
		NSLOCTEXT("IGMissingFloor", "N2DoorKnockCaption", "현관문 — 노크"),
		2.0f,
		IGNightTwo::KnockLocation);
	// 계기는 소리가 아니라 위치다. 사흘째 벽에서 들리던 것이 이번엔 문이다.
	AIGHorrorHUD::PushThought(
		this,
		NSLOCTEXT(
			"IGMissingFloor",
			"N2DoorKnockThought",
			"벽이 아니다. 우리 집 문이다."),
		4.2f);

	// §5.5가 거부할 것이 생기는 순간이다. 발신자가 존재여야 하므로 소음
	// 이벤트의 instigator를 그로 넘긴다 — 그것이 테이프의 공백을 만든다.
	if (UIGNoiseSubsystem* Noise = GetNoise())
	{
		Noise->ReportNoise(
			IGNightTwo::KnockLocation,
			IGNightTwo::SingleKnockLoudness,
			Entity.Get());
	}
	if (AIGPlayerCharacter* PlayerCharacter = Player.Get())
	{
		if (UIGStressComponent* Stress = PlayerCharacter->GetStress())
		{
			Stress->ApplyScare(ScareAmount);
		}
		// 우리 문이다. 스트레스만 오르고 화면은 가만히 있을 수 없다.
		PlayerCharacter->PlayScareKick(1.3f);
	}

	KnockCount = 1;
	StageFigure();
	if (Peephole)
	{
		Peephole->SetInteractionEnabled(true);
	}
}

void AIGMissingFloorNightTwoBeatDirector::PlayAnswer()
{
	IGAudio::SpawnOneShotAt(
		this,
		UIGToneSequenceSoundWave::CreateWallKnockTriple(
			this,
			IGNightTwo::TripleKnockMuffle),
		IGNightTwo::KnockLocation,
		IGNightTwo::KnockVolume,
		1.0f,
		IGNightTwo::KnockInnerRadius,
		IGNightTwo::KnockFalloff,
		EIGAudioBus::Entity);
	AIGHorrorHUD::PushAudioCaptionAt(
		this,
		NSLOCTEXT("IGMissingFloor", "N2TripleCaption", "문 너머 — 노크 3연"),
		2.4f,
		IGNightTwo::KnockLocation);
	// 1.0은 §21.2의 3연 값이고, §5.5의 표가 그것을 2.10초의 무음으로 옮긴다.
	// 아침에 그녀가 듣는 공백의 길이는 여기서 정해진다.
	if (UIGNoiseSubsystem* Noise = GetNoise())
	{
		Noise->ReportNoise(
			IGNightTwo::KnockLocation,
			IGNightTwo::TripleKnockLoudness,
			Entity.Get());
	}
	KnockCount = 2;
}

void AIGMissingFloorNightTwoBeatDirector::PlayDragAway()
{
	// 끌려가는 소리. 무엇이 끌려가는지는 밤4에 가서야 알게 되고, 지금은 복도가
	// 비어 가는 소리일 뿐이다. 4층 복도는 화강석 타일이라 비닐이 아니다.
	// 루프 파형이라 복도를 다 빠져나가는 시간에 맞춰 멎게 한다 — 예전엔 아무도
	// 안 끊어서 ENTITY 상한에 밀려날 때까지 문 앞에서 계속 긁었다.
	UAudioComponent* DragAway = IGAudio::SpawnOneShotAt(
		this,
		UIGToneSequenceSoundWave::CreateEntityDragLoop(this, /*bVinyl=*/false),
		IGNightTwo::FigureStagePoint,
		IGNightTwo::DragVolume,
		1.0f,
		IGNightTwo::KnockInnerRadius,
		IGNightTwo::KnockFalloff,
		EIGAudioBus::Entity);
	if (DragAway)
	{
		TWeakObjectPtr<UAudioComponent> WeakDrag(DragAway);
		GetWorldTimerManager().SetTimer(
			DragFadeTimer,
			FTimerDelegate::CreateWeakLambda(this, [WeakDrag]()
			{
				if (UAudioComponent* Loop = WeakDrag.Get())
				{
					Loop->FadeOut(1.2f, 0.0f);
				}
			}),
			FMath::Max(IGNightTwo::DragSeconds - 1.2f, 0.2f),
			false);
	}
	AIGHorrorHUD::PushAudioCaptionAt(
		this,
		NSLOCTEXT("IGMissingFloor", "N2DragCaption", "끌리는 소리 — 멀어짐"),
		2.6f,
		IGNightTwo::FigureStagePoint);
	if (UIGNoiseSubsystem* Noise = GetNoise())
	{
		Noise->ReportNoise(
			IGNightTwo::FigureStagePoint,
			IGNightTwo::DragLoudness,
			Entity.Get());
	}
	// 소리만 멀어지는 것이 아니라 그도 멀어진다. 문을 열어 확인하는 플레이어가
	// 빈 복도를 봐야 한다.
	if (AIGListenerEntity* Listener = Entity.Get())
	{
		Listener->SetPatrolPoints({
			IGNightTwo::FigureStagePoint,
			IGNightTwo::DragDepartPoint,
		});
	}
	KnockCount = 3;
}

void AIGMissingFloorNightTwoBeatDirector::StageFigure()
{
	AIGListenerEntity* Listener = Entity.Get();
	if (!Listener || bFigureStaged)
	{
		return;
	}
	bFigureStaged = true;
	// 문을 향해 선다. 1-4의 계단 카메오와 같은 장치이고, 같은 이유로 순찰
	// 두 점을 준다 — 서 있는 것이 아니라 기다리는 것으로 읽혀야 한다.
	Listener->SetPatrolPoints({
		IGNightTwo::FigureStagePoint,
		IGNightTwo::FigureShufflePoint,
	});
	// ParkForBeat clears his reaction to the last sound as well as moving him.
	// Without that he stands outside 403 for one frame and then crawls off toward
	// whatever he last heard, which for a beat that opens with a knock is common.
	Listener->ParkForBeat(IGNightTwo::FigureStagePoint, 90.0f);
}

void AIGMissingFloorNightTwoBeatDirector::ReleaseFigure()
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

// -- 비트 2-5 「귀환 추격」 -------------------------------------------------

void AIGMissingFloorNightTwoBeatDirector::ArmReturnChase()
{
	const UIGMissingFloorNarrativeSubsystem* Narrative = GetNarrative();
	if (!Narrative || Narrative->GetNightIndex() != 2)
	{
		return;
	}
	if (ReturnStage != EIGNightTwoReturnStage::Idle)
	{
		return;
	}
	ReturnStage = EIGNightTwoReturnStage::AwaitingExit;
	// 종이는 손에 있고 밤은 끝나지 않았다. 목표가 바뀐 것을 한 줄로 말한다.
	AIGHorrorHUD::PushThought(
		this,
		NSLOCTEXT(
			"IGMissingFloor",
			"N2ReturnThought",
			"이거 들고 집까지 가야 한다."),
		4.0f);
	GetWorldTimerManager().SetTimer(
		ReturnTimer,
		this,
		&AIGMissingFloorNightTwoBeatDirector::AdvanceReturn,
		IGNightTwo::ReturnPollSeconds,
		true);
}

bool AIGMissingFloorNightTwoBeatDirector::IsPlayerOutsideBooth() const
{
	const AIGPlayerCharacter* PlayerCharacter = Player.Get();
	if (!PlayerCharacter)
	{
		return false;
	}
	const FVector Where = PlayerCharacter->GetActorLocation();
	// 1층에서 관리실 남쪽 선을 넘었을 때만. 4층에서의 Y는 아무 의미가 없다.
	return Where.Z < IGNightTwo::FourthFloorZ * 0.5f
		&& Where.Y < IGNightTwo::BoothExitY;
}

bool AIGMissingFloorNightTwoBeatDirector::IsPlayerInsideUnit403() const
{
	const AIGPlayerCharacter* PlayerCharacter = Player.Get();
	return PlayerCharacter
		&& IGNightTwo::Unit403Interior.IsInsideOrOn(
			PlayerCharacter->GetActorLocation());
}

void AIGMissingFloorNightTwoBeatDirector::AdvanceReturn()
{
	switch (ReturnStage)
	{
	case EIGNightTwoReturnStage::AwaitingExit:
		if (IsPlayerOutsideBooth())
		{
			PlayMaterialCollapse();
			ReturnStage = EIGNightTwoReturnStage::Chased;
		}
		break;

	case EIGNightTwoReturnStage::Chased:
		// 리셋으로 침대에 돌아온 것은 도착이 아니다. 한 번 밖으로 나가야
		// 그 빚이 청산된다.
		if (bMustLeaveHomeAgain)
		{
			if (!IsPlayerInsideUnit403())
			{
				bMustLeaveHomeAgain = false;
			}
			break;
		}
		if (IsPlayerInsideUnit403())
		{
			ReturnStage = EIGNightTwoReturnStage::Home;
			GetWorldTimerManager().ClearTimer(ReturnTimer);
			if (UIGMissingFloorNarrativeSubsystem* Narrative = GetNarrative())
			{
				Narrative->MarkBeatPlayed(IGNightTwo::ReturnBeatId);
			}
			OnReturnedHome.Broadcast();
		}
		break;

	default:
		GetWorldTimerManager().ClearTimer(ReturnTimer);
		break;
	}
}

void AIGMissingFloorNightTwoBeatDirector::PlayMaterialCollapse()
{
	if (bReturnChaseFired)
	{
		return;
	}
	bReturnChaseFired = true;

	auto Impact = [this](const bool bSecond)
	{
		IGAudio::SpawnOneShotAt(
			this,
			bSecond
				? UIGToneSequenceSoundWave::CreateLockedRattle(this)
				: UIGToneSequenceSoundWave::CreateDoorThud(this),
			IGNightTwo::CollapseLocation,
			IGNightTwo::CollapseVolume,
			bSecond ? 0.74f : 0.86f,
			IGNightTwo::CollapseInnerRadius,
			IGNightTwo::CollapseFalloff);
		// 발신자 없음. 건물이 한 일이며, 파문 HUD가 「네가 냈다」고 말해서는
		// 안 된다 — 1-5의 소화기와 같은 규칙이다.
		if (UIGNoiseSubsystem* Noise = GetNoise())
		{
			Noise->ReportNoise(
				IGNightTwo::CollapseLocation,
				IGNightTwo::CollapseLoudness,
				nullptr);
		}
	};

	Impact(/*bSecond=*/false);
	// 나머지가 무너지는 두 번째 소리. 이 한 번이 조사를 추격으로 바꾼다.
	FTimerDelegate SecondImpact;
	SecondImpact.BindLambda([this, Impact]() { Impact(true); });
	GetWorldTimerManager().SetTimer(
		CollapseTimer,
		SecondImpact,
		IGNightTwo::CollapseSecondImpactSeconds,
		false);

	AIGHorrorHUD::PushAudioCaptionAt(
		this,
		NSLOCTEXT("IGMissingFloor", "N2CollapseCaption", "자재 무너짐"),
		2.4f,
		IGNightTwo::CollapseLocation);
	// §18.6 낙하물·충돌. 건물이 무너뜨린 것이지 그녀가 낸 소리가 아니라서
	// 손에는 한 번만 온다.
	if (AIGPlayerCharacter* PlayerCharacter = Player.Get())
	{
		PlayerCharacter->PlayImpactHaptic();
	}
	if (AIGPlayerCharacter* PlayerCharacter = Player.Get())
	{
		if (UIGStressComponent* Stress = PlayerCharacter->GetStress())
		{
			Stress->ApplyScare(ChaseScareAmount);
		}
	}
}

void AIGMissingFloorNightTwoBeatDirector::NotifyCaptureReset()
{
	if (ReturnStage == EIGNightTwoReturnStage::Chased)
	{
		bMustLeaveHomeAgain = true;
	}
}

void AIGMissingFloorNightTwoBeatDirector::HandlePeepholeExamined(
	AIGMissingFloorEvidence* Evidence)
{
	UIGMissingFloorNarrativeSubsystem* Narrative = GetNarrative();
	if (!Narrative || !Narrative->MarkBeatPlayed(IGNightTwo::PeepholeBeatId))
	{
		return;
	}
	bPeepholeSeen = true;
	AIGHorrorHUD::PushThought(
		this,
		NSLOCTEXT(
			"IGMissingFloor",
			"N2PeepholeThought1",
			"복도에 있다. 이쪽을 보고 있는 것도 아니고, 그냥 서 있다."),
		4.4f);
	// 이 줄이 폰을 가리킨다. 설계서의 계기는 「증거를 만들 생각」이고,
	// 프롬프트를 새로 띄우는 대신 그녀가 스스로 그 생각에 도달하게 둔다.
	AIGHorrorHUD::PushThought(
		this,
		NSLOCTEXT(
			"IGMissingFloor",
			"N2PeepholeThought2",
			"폰으로 찍자. 문에서 조금 떨어져서."),
		4.6f);
}

void AIGMissingFloorNightTwoBeatDirector::AdvanceForTesting()
{
	// 프로브가 인내 시간을 기다리지 않게 해 준다. 단계를 건너뛰는 것이 아니라
	// 매번 현재 단계의 시계를 만료시키고 정상 경로를 호출하므로, 순서와 부수
	// 효과는 실제 재생과 동일하다. 상한은 단계 수보다 넉넉하게 잡되 무한이
	// 되지 않게 둔다 — 진행하지 못하는 상태를 조용히 도는 것보다 프로브가
	// 그것을 보고 실패하는 편이 낫다.
	constexpr int32 MaximumSteps = 12;
	const float FarPast =
		IGNightTwo::PeepholePatienceSeconds
		+ IGNightTwo::PhonePatienceSeconds
		+ IGNightTwo::TripleDelaySeconds
		+ IGNightTwo::WaitAfterTripleSeconds
		+ IGNightTwo::DragSeconds;
	for (int32 Step = 0; Step < MaximumSteps; ++Step)
	{
		if (Stage == EIGNightTwoBeatStage::Spent
			|| Stage == EIGNightTwoBeatStage::Idle)
		{
			return;
		}
		StageSeconds = FarPast;
		AdvanceStage();
	}
}
