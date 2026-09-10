#include "Entity/IGNightPhaseDirector.h"

#include "Audio/IGAudioHelpers.h"
#include "Audio/IGToneSequenceSoundWave.h"
#include "Camera/PlayerCameraManager.h"
#include "Core/IGPrologueWorldScene.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Entity/IGMissingFloorNightFourDirector.h"
#include "GameFramework/PlayerController.h"
#include "Narrative/IGMissingFloorNarrativeSubsystem.h"
#include "Player/IGHorrorHUD.h"
#include "Player/IGPlayerCharacter.h"
#include "Save/IGSaveSubsystem.h"
#include "TimerManager.h"

namespace IGNightPhase
{
	/** How often the hour is sampled. Coarse on purpose: nothing needs frames. */
	constexpr float TickIntervalSeconds = 0.25f;
	/**
	 * 새벽은 한 번 눈을 감는다. 스무 분의 밤이 독백 한 줄로 끝나면 밤과 낮이
	 * 같은 화면이라 끝났다는 감각이 몸에 안 온다.
	 */
	constexpr float DawnFadeOutSeconds = 0.45f;
	constexpr float DawnBlackSeconds = 0.55f;
	constexpr float DawnFadeInSeconds = 0.9f;
	/** 공동현관 유리문의 전자 잠금. 문 자체는 (604, -385)에 서 있다. */
	const FVector EntranceLatchLocation(604.0f, -385.0f, 100.0f);
}

AIGNightPhaseDirector::AIGNightPhaseDirector()
{
	PrimaryActorTick.bCanEverTick = false;
}

void AIGNightPhaseDirector::BeginPlay()
{
	Super::BeginPlay();
}

void AIGNightPhaseDirector::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	GetWorldTimerManager().ClearTimer(HourTimer);
	// Never leave a torn-down director holding the building shut.
	if (bHourActive)
	{
		if (AIGPrologueWorldScene* WorldScene = Scene.Get())
		{
			WorldScene->SetTheHourSealed(false);
			WorldScene->SetNightStairPocketEnabled(false);
		}
		ApplySealedPresentation(false);
	}
	Super::EndPlay(EndPlayReason);
}

void AIGNightPhaseDirector::Configure(
	AIGPrologueWorldScene* InScene,
	AIGPlayerCharacter* InPlayer)
{
	Scene = InScene;
	Player = InPlayer;
}

void AIGNightPhaseDirector::BeginTheHour(const int32 NightIndex)
{
	if (bHourActive)
	{
		return;
	}
	bHourActive = true;
	bGoalComplete = false;
	bFailureEndingSuspended = false;
	bHourPaused = false;
	HourElapsedSeconds = 0.0f;

	// 같은 번호의 밤이 다시 오면 카드가 그것을 안다. 못 채운 밤의 되풀이와
	// 엔딩 C의 재시도가 여기 걸린다. 저장에서 이어 붙이는 것은 되풀이가 아니다.
	bool bRepeatedNight = false;
	if (UIGMissingFloorNarrativeSubsystem* Narrative = GetNarrative())
	{
		bRepeatedNight = !bRestoringHour
			&& NightIndex >= 1
			&& Narrative->GetNightIndex() == NightIndex;
		Narrative->SetNightIndex(NightIndex);
		Narrative->SetHourSealed(true);
		Narrative->SetNightElapsedSeconds(0.0f);
	}

	if (AIGPrologueWorldScene* WorldScene = Scene.Get())
	{
		WorldScene->SetTheHourSealed(true);
		// The half-landing viewing pocket exists only while the hour does.
		WorldScene->SetNightStairPocketEnabled(true);
		// §11 V2 403호 3단계 노화. The prologue and the first night are a flat
		// she lives in; nights two and three crack the ceiling corner; night
		// four has the damp down the wall. Driven off the night rather than any
		// story flag, because the building is not reacting to her — it is just
		// getting worse, and that is the point.
		WorldScene->SetUnit403AgeStage(
			NightIndex >= 4 ? 2 : (NightIndex >= 2 ? 1 : 0));
	}
	ApplySealedPresentation(true);

	// The one allowed piece of framing UI: the night card, mirroring the
	// legacy chapter cards. Everything after it is world and sound.
	FText NightTitle;
	switch (NightIndex)
	{
	case 1:
		NightTitle = NSLOCTEXT("IGMissingFloor", "Night1Title", "밤 1 — 소리");
		break;
	case 2:
		NightTitle = NSLOCTEXT("IGMissingFloor", "Night2Title", "밤 2 — 기록");
		break;
	case 3:
		NightTitle = NSLOCTEXT("IGMissingFloor", "Night3Title", "밤 3 — 조율");
		break;
	case 4:
		NightTitle = NSLOCTEXT("IGMissingFloor", "Night4Title", "밤 4 — 대답");
		break;
	default:
		break;
	}
	if (!NightTitle.IsEmpty())
	{
		AIGHorrorHUD::ShowChapterCard(
			this,
			bRepeatedNight
				? NSLOCTEXT("IGMissingFloor", "NightCardEyebrowAgain", "다시, 새벽 네시 반")
				: NSLOCTEXT("IGMissingFloor", "NightCardEyebrow", "새벽 네시 반"),
			NightTitle,
			FText::GetEmpty(),
			4.2f);
	}

	GetWorldTimerManager().SetTimer(
		HourTimer,
		this,
		&AIGNightPhaseDirector::TickHour,
		IGNightPhase::TickIntervalSeconds,
		true);

	OnHourActiveChanged.Broadcast(true);
	if (!bRestoringHour)
	{
		RequestMissingFloorAutosave(true);
	}
}

void AIGNightPhaseDirector::ResumeTheHour(
	const int32 NightIndex,
	const float ElapsedSeconds)
{
	if (bHourActive)
	{
		return;
	}
	bRestoringHour = true;
	BeginTheHour(NightIndex);
	bRestoringHour = false;
	HourElapsedSeconds = FMath::Clamp(
		ElapsedSeconds,
		0.0f,
		HourDurationSeconds - IGNightPhase::TickIntervalSeconds);
	if (UIGMissingFloorNarrativeSubsystem* Narrative = GetNarrative())
	{
		Narrative->SetNightElapsedSeconds(HourElapsedSeconds);
	}
}

void AIGNightPhaseDirector::SuspendForFailureEnding()
{
	if (!bHourActive || bFailureEndingSuspended)
	{
		return;
	}
	bFailureEndingSuspended = true;
	GetWorldTimerManager().ClearTimer(HourTimer);
}

void AIGNightPhaseDirector::RestartTheHour(const int32 NightIndex)
{
	GetWorldTimerManager().ClearTimer(HourTimer);
	bHourActive = false;
	bGoalComplete = false;
	bFailureEndingSuspended = false;
	BeginTheHour(NightIndex);
}

void AIGNightPhaseDirector::CompleteNightGoal()
{
	if (!bHourActive || bGoalComplete)
	{
		return;
	}
	bGoalComplete = true;
	// 채운 밤은 기록에 남는다. 못 채운 밤은 다음 저녁에 다시 온다(§5.4).
	if (UIGMissingFloorNarrativeSubsystem* Narrative = GetNarrative())
	{
		Narrative->MarkBeatPlayed(GoalBeatId(Narrative->GetNightIndex()));
	}
	// The goal and the timeout share one exit so morning is always the same
	// world state, whichever way the player got there.
	ReleaseAtDawn();
}

FName AIGNightPhaseDirector::GoalBeatId(const int32 NightIndex)
{
	return FName(*FString::Printf(TEXT("Night%d.Goal"), NightIndex));
}

void AIGNightPhaseDirector::TickHour()
{
	if (!bHourActive || bFailureEndingSuspended)
	{
		return;
	}
	if (bHourPaused)
	{
		return;
	}
	HourElapsedSeconds += IGNightPhase::TickIntervalSeconds;
	if (UIGMissingFloorNarrativeSubsystem* Narrative = GetNarrative())
	{
		Narrative->SetNightElapsedSeconds(HourElapsedSeconds);
	}
	if (HourElapsedSeconds >= HourDurationSeconds)
	{
		ReleaseAtDawn();
	}
}

void AIGNightPhaseDirector::ReleaseAtDawn()
{
	GetWorldTimerManager().ClearTimer(HourTimer);
	bHourActive = false;

	if (AIGPrologueWorldScene* WorldScene = Scene.Get())
	{
		WorldScene->SetTheHourSealed(false);
		WorldScene->SetNightStairPocketEnabled(false);
	}
	ApplySealedPresentation(false);

	if (UIGMissingFloorNarrativeSubsystem* Narrative = GetNarrative())
	{
		Narrative->SetHourSealed(false);
	}

	// §20.4: dawn on night four with the wall still closed is the substitute
	// route to ending C. It is the only way there for 듣기만 하는 밤, which has
	// no captures to raise the tier, and it is harmless in the other modes —
	// a player who reached 05:30 without opening the wall has failed either way.
	for (TActorIterator<AIGMissingFloorNightFourDirector> It(GetWorld()); It; ++It)
	{
		if (It->ResolveDawnFailureEnding())
		{
			// The failure ending owns the screen from here; the ordinary morning
			// line would talk over its card.
			OnHourActiveChanged.Broadcast(false);
			return;
		}
		break;
	}

	OnHourActiveChanged.Broadcast(false);
	RequestMissingFloorAutosave(false);

	if (bMorningPresentationSuppressed)
	{
		bMorningPresentationSuppressed = false;
		return;
	}
	// 세계가 먼저 바뀐다(위의 브로드캐스트). 눈은 그 뒤에 감긴다 — 검은
	// 화면 아래서 밤의 베드가 죽고 그가 잠들고, 눈을 뜨면 공동현관 잠금이
	// 풀리는 소리가 아래서 올라온다.
	AIGPlayerCharacter* PlayerCharacter = Player.Get();
	APlayerController* Controller = PlayerCharacter
		? Cast<APlayerController>(PlayerCharacter->GetController())
		: nullptr;
	if (Controller && Controller->PlayerCameraManager)
	{
		Controller->PlayerCameraManager->StartCameraFade(
			0.0f,
			1.0f,
			IGNightPhase::DawnFadeOutSeconds,
			FLinearColor::Black,
			/*bShouldFadeAudio=*/false,
			/*bHoldWhenFinished=*/true);
	}
	GetWorldTimerManager().SetTimer(
		DawnTimer,
		this,
		&AIGNightPhaseDirector::FinishDawnPresentation,
		IGNightPhase::DawnBlackSeconds,
		false);
}

void AIGNightPhaseDirector::FinishDawnPresentation()
{
	AIGPlayerCharacter* PlayerCharacter = Player.Get();
	APlayerController* Controller = PlayerCharacter
		? Cast<APlayerController>(PlayerCharacter->GetController())
		: nullptr;
	if (Controller && Controller->PlayerCameraManager)
	{
		Controller->PlayerCameraManager->StartCameraFade(
			1.0f,
			0.0f,
			IGNightPhase::DawnFadeInSeconds,
			FLinearColor::Black,
			/*bShouldFadeAudio=*/false,
			/*bHoldWhenFinished=*/false);
	}
	IGAudio::SpawnOneShotAt(
		this,
		UIGToneSequenceSoundWave::CreateRelayClick(this),
		IGNightPhase::EntranceLatchLocation,
		0.9f,
		0.7f,
		300.0f,
		3200.0f,
		EIGAudioBus::World);
	AIGHorrorHUD::PushAudioCaptionAt(
		this,
		NSLOCTEXT("IGMissingFloor", "DawnLatchCaption", "공동현관 잠금이 풀린다"),
		2.2f,
		IGNightPhase::EntranceLatchLocation);
	// The release is announced by the world, not by a banner: the entrance
	// simply opens again. One inner-voice line is allowed (§7 forbids
	// confirmation UI, not thought).
	AIGHorrorHUD::PushThought(
		this,
		NSLOCTEXT("IGMissingFloor", "MorningCame", "문이 열린다. 아침이다."),
		3.4f);
}

void AIGNightPhaseDirector::RequestMissingFloorAutosave(const bool bAtNight)
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
			FName(bAtNight
				? TEXT("Checkpoint.MissingFloor.Night")
				: TEXT("Checkpoint.MissingFloor.Day")),
			false));
}

void AIGNightPhaseDirector::ApplySealedPresentation(const bool bSealed)
{
	const UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}
	APlayerController* PlayerController = World->GetFirstPlayerController();
	if (!PlayerController)
	{
		return;
	}
	AIGHorrorHUD* Hud = Cast<AIGHorrorHUD>(PlayerController->GetHUD());
	if (!Hud)
	{
		return;
	}

	// Night austerity: no objective line while the hour holds. The objective
	// provider is still bound so the day sections have something to say.
	Hud->SetNightPresentation(bSealed);
	Hud->SetObjectiveProvider(this);
}

int32 AIGNightPhaseDirector::GetStoryMinutesRemaining() const
{
	const float Remaining =
		FMath::Max(HourDurationSeconds - HourElapsedSeconds, 0.0f);
	const float Ratio = Remaining / HourDurationSeconds;
	return FMath::CeilToInt(Ratio * (StoryEndMinutes - StoryStartMinutes));
}

FText AIGNightPhaseDirector::GetObjectiveText() const
{
	// During the hour the HUD suppresses the objective entirely, so this only
	// ever reads in the day sections between nights.
	if (bHourActive)
	{
		return FText::GetEmpty();
	}
	return NSLOCTEXT("IGMissingFloor", "DayObjective", "낮 — 물어볼 사람을 찾자");
}

FString AIGNightPhaseDirector::GetObjectiveTextAscii() const
{
	if (bHourActive)
	{
		return FString();
	}
	return TEXT("Daytime - find someone who will talk");
}

float AIGNightPhaseDirector::GetObjectiveProgress() const
{
	if (!bHourActive)
	{
		return 0.0f;
	}
	return FMath::Clamp(HourElapsedSeconds / HourDurationSeconds, 0.0f, 1.0f);
}

UIGMissingFloorNarrativeSubsystem* AIGNightPhaseDirector::GetNarrative() const
{
	const UWorld* World = GetWorld();
	UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
	return GameInstance
		? GameInstance->GetSubsystem<UIGMissingFloorNarrativeSubsystem>()
		: nullptr;
}
