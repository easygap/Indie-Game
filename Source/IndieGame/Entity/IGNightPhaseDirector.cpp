#include "Entity/IGNightPhaseDirector.h"

#include "Core/IGPrologueWorldScene.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Narrative/IGMissingFloorNarrativeSubsystem.h"
#include "Player/IGHorrorHUD.h"
#include "Player/IGPlayerCharacter.h"
#include "TimerManager.h"

namespace IGNightPhase
{
	/** How often the hour is sampled. Coarse on purpose: nothing needs frames. */
	constexpr float TickIntervalSeconds = 0.25f;
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
	HourElapsedSeconds = 0.0f;

	if (UIGMissingFloorNarrativeSubsystem* Narrative = GetNarrative())
	{
		Narrative->SetNightIndex(NightIndex);
		Narrative->SetHourSealed(true);
		Narrative->SetNightElapsedSeconds(0.0f);
	}

	if (AIGPrologueWorldScene* WorldScene = Scene.Get())
	{
		WorldScene->SetTheHourSealed(true);
		// The half-landing viewing pocket exists only while the hour does.
		WorldScene->SetNightStairPocketEnabled(true);
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
			NSLOCTEXT("IGMissingFloor", "NightCardEyebrow", "새벽 네시 반"),
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
}

void AIGNightPhaseDirector::CompleteNightGoal()
{
	if (!bHourActive || bGoalComplete)
	{
		return;
	}
	bGoalComplete = true;
	// The goal and the timeout share one exit so morning is always the same
	// world state, whichever way the player got there.
	ReleaseAtDawn();
}

void AIGNightPhaseDirector::TickHour()
{
	if (!bHourActive)
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

	// The release is announced by the world, not by a banner: the entrance
	// simply opens again. One inner-voice line is allowed (§7 forbids
	// confirmation UI, not thought).
	AIGHorrorHUD::PushThought(
		this,
		NSLOCTEXT("IGMissingFloor", "MorningCame", "…열린다. 아침이네."),
		3.4f);
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
