#include "Entity/IGNightOneBeatDirector.h"

#include "Audio/IGAudioHelpers.h"
#include "Audio/IGToneSequenceSoundWave.h"
#include "Components/AudioComponent.h"
#include "Core/IGPrologueWorldScene.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Entity/IGListenerEntity.h"
#include "Entity/IGNoiseSubsystem.h"
#include "Interaction/IGStairTransition.h"
#include "Interaction/IGZoneTrigger.h"
#include "Narrative/IGMissingFloorNarrativeSubsystem.h"
#include "Player/IGPlayerCharacter.h"
#include "TimerManager.h"

namespace IGNightOne
{
	// Beat ids in the persistent night state: once played, never replayed.
	const FName SightingBeatId(TEXT("Night1.Sighting"));
	const FName ExtinguisherBeatId(TEXT("Night1.Extinguisher"));

	/**
	 * The stair-throat trigger, at the 4F mouth of the down flight. The
	 * half-landing itself is not visible from here — the shaft turn hides it —
	 * so staging on entry never moves anything inside the player's view.
	 */
	const FVector SightingZoneCenter(-300.0f, -305.0f, 1010.0f);
	const FVector SightingZoneExtent(45.0f, 62.0f, 110.0f);

	/**
	 * Where the figure stands: on the half-landing, past the night portal
	 * line, ear against the far shaft wall. Unreachable on foot — the portal
	 * teleports the player first — so it can never body-block the descent.
	 */
	const FVector SightingStagePoint(-445.0f, -305.0f, 888.0f);
	const FVector SightingShufflePoint(-445.0f, -255.0f, 888.0f);

	/** Give up on the cameo if the player retreats and never descends. */
	constexpr float SightingFallbackSeconds = 45.0f;

	/** In front of the fire cabinet, spanning the corridor walkway. */
	const FVector ExtinguisherZoneCenter(232.0f, -320.0f, 1010.0f);
	const FVector ExtinguisherZoneExtent(60.0f, 55.0f, 110.0f);

	/** The fall takes about this long to reach tile from the bracket. */
	constexpr float ImpactDelaySeconds = 0.45f;
	/** §5.1: a dropped prop is a 0.6 — the whole corridor hears it. */
	constexpr float ImpactLoudness = 0.6f;

	/**
	 * The distribution board's hum pocket, the escape the beat teaches.
	 * Placed at the panel's corridor face, low enough that a player standing
	 * in front of it is inside the radius.
	 */
	const FVector BreakerPanelHumLocation(-90.0f, -245.0f, 1000.0f);
	constexpr float BreakerPanelHumRadius = 200.0f;
	constexpr float BreakerPanelHumMasking = 0.2f;
}

AIGNightOneBeatDirector::AIGNightOneBeatDirector()
{
	PrimaryActorTick.bCanEverTick = false;
}

FVector AIGNightOneBeatDirector::GetSightingZoneCenter()
{
	return IGNightOne::SightingZoneCenter;
}

FVector AIGNightOneBeatDirector::GetSightingStagePoint()
{
	return IGNightOne::SightingStagePoint;
}

FVector AIGNightOneBeatDirector::GetSightingShufflePoint()
{
	return IGNightOne::SightingShufflePoint;
}

bool AIGNightOneBeatDirector::Configure(
	AIGPrologueWorldScene* InScene,
	AIGListenerEntity* InEntity,
	AIGPlayerCharacter* InPlayer,
	const TArray<FVector>& InCorridorPatrolPoints)
{
	UWorld* World = GetWorld();
	if (!World || !InScene || !InEntity || !InPlayer)
	{
		return false;
	}
	Scene = InScene;
	Entity = InEntity;
	Player = InPlayer;
	CorridorPatrolPoints = InCorridorPatrolPoints;

	// Second hum pocket after the fridge: the corridor breaker panel. Owned
	// here because it is this beat's escape hatch; released on teardown.
	if (UIGNoiseSubsystem* Noise = World->GetSubsystem<UIGNoiseSubsystem>())
	{
		BreakerPanelHumHandle = Noise->RegisterHumSource(
			IGNightOne::BreakerPanelHumLocation,
			IGNightOne::BreakerPanelHumRadius,
			IGNightOne::BreakerPanelHumMasking);
		// 이 비트의 탈출구는 귀로 찾는 것이다. 배전반이 조용하면 플레이어는
		// 여기 설 이유를 붙잡히고 나서야 안다.
		BreakerPanelHumLoop = IGAudio::SpawnHumLoopAt(
			this,
			TEXT("NightOneBreakerHum"),
			IGNightOne::BreakerPanelHumLocation,
			IGNightOne::BreakerPanelHumRadius);
	}

	// Both zones deliberately carry no story tag: the tag would self-consume
	// on a save load and silently disarm the beats. Once-per-run bookkeeping
	// lives in the narrative snapshot instead.
	FActorSpawnParameters SpawnParameters;
	SpawnParameters.SpawnCollisionHandlingOverride =
		ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	SpawnParameters.Name = TEXT("Night1SightingZone");
	SightingZone = World->SpawnActor<AIGZoneTrigger>(
		AIGZoneTrigger::StaticClass(),
		FTransform(FRotator::ZeroRotator, IGNightOne::SightingZoneCenter),
		SpawnParameters);
	if (!SightingZone)
	{
		return false;
	}
	SightingZone->SetZoneExtent(IGNightOne::SightingZoneExtent);
	SightingZone->OnZoneTriggered.AddDynamic(
		this, &AIGNightOneBeatDirector::HandleSightingZone);

	SpawnParameters.Name = TEXT("Night1ExtinguisherZone");
	ExtinguisherZone = World->SpawnActor<AIGZoneTrigger>(
		AIGZoneTrigger::StaticClass(),
		FTransform(FRotator::ZeroRotator, IGNightOne::ExtinguisherZoneCenter),
		SpawnParameters);
	if (!ExtinguisherZone)
	{
		return false;
	}
	ExtinguisherZone->SetZoneExtent(IGNightOne::ExtinguisherZoneExtent);
	ExtinguisherZone->OnZoneTriggered.AddDynamic(
		this, &AIGNightOneBeatDirector::HandleExtinguisherZone);

	// The cameo ends the moment the player actually descends: the stair
	// teleport completing is the one reliable "they walked past it" signal.
	for (TActorIterator<AIGStairTransition> It(World); It; ++It)
	{
		It->OnTransitionCompleted.AddDynamic(
			this, &AIGNightOneBeatDirector::HandleStairTransitionCompleted);
		break;
	}
	return true;
}

void AIGNightOneBeatDirector::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	GetWorldTimerManager().ClearTimer(SightingFallbackTimer);
	GetWorldTimerManager().ClearTimer(ImpactTimer);
	if (BreakerPanelHumHandle != 0)
	{
		if (UWorld* World = GetWorld())
		{
			if (UIGNoiseSubsystem* Noise = World->GetSubsystem<UIGNoiseSubsystem>())
			{
				Noise->UnregisterHumSource(BreakerPanelHumHandle);
			}
		}
		BreakerPanelHumHandle = 0;
	}
	if (BreakerPanelHumLoop)
	{
		BreakerPanelHumLoop->Stop();
		BreakerPanelHumLoop->DestroyComponent();
		BreakerPanelHumLoop = nullptr;
	}
	if (bSightingStaged && !bSightingCompleted)
	{
		RestoreSightingEntity();
	}
	Super::EndPlay(EndPlayReason);
}

// -- 1-4 first sighting ----------------------------------------------------

void AIGNightOneBeatDirector::HandleSightingZone(AIGZoneTrigger* Zone)
{
	UIGMissingFloorNarrativeSubsystem* Narrative = GetNarrative();
	if (!Narrative || !Narrative->MarkBeatPlayed(IGNightOne::SightingBeatId))
	{
		return;
	}
	StageSighting();
}

void AIGNightOneBeatDirector::StageSighting()
{
	AIGListenerEntity* Listener = Entity.Get();
	AIGPrologueWorldScene* WorldScene = Scene.Get();
	if (!Listener || !WorldScene)
	{
		return;
	}

	bSightingStaged = true;
	// The dying west fixture flickers right over the stair throat; a staged
	// reveal must not fight it.
	WorldScene->SuspendCorridorFlicker(true);

	// Face the far wall — ear pressed against it, back to the descending
	// player. The two patrol points keep it shuffling along the same wall on
	// the landing plane, which its crawl can actually traverse.
	Listener->TeleportTo(
		IGNightOne::SightingStagePoint,
		FRotator(0.0f, 180.0f, 0.0f),
		false,
		true);
	Listener->SetPatrolPoints({
		IGNightOne::SightingStagePoint,
		IGNightOne::SightingShufflePoint,
	});

	GetWorldTimerManager().SetTimer(
		SightingFallbackTimer,
		this,
		&AIGNightOneBeatDirector::RestoreSightingEntity,
		IGNightOne::SightingFallbackSeconds,
		false);
}

void AIGNightOneBeatDirector::HandleStairTransitionCompleted(const bool bGoingDown)
{
	// Only the descent past the figure ends the cameo; riding back up from
	// the lobby later must not resurrect it.
	if (bSightingStaged && !bSightingCompleted && bGoingDown)
	{
		RestoreSightingEntity();
	}
}

void AIGNightOneBeatDirector::RestoreSightingEntity()
{
	if (!bSightingStaged || bSightingCompleted)
	{
		return;
	}
	bSightingCompleted = true;
	GetWorldTimerManager().ClearTimer(SightingFallbackTimer);

	if (AIGListenerEntity* Listener = Entity.Get())
	{
		Listener->SetPatrolPoints(CorridorPatrolPoints);
		// Back to its corridor spawn without touching the aggression tier:
		// the cameo was theatre, not a failure.
		Listener->ResetToPatrolStart(/*bRaiseAggression=*/false);
	}
	if (AIGPrologueWorldScene* WorldScene = Scene.Get())
	{
		WorldScene->SuspendCorridorFlicker(false);
	}
}

// -- 1-5 forced encounter --------------------------------------------------

void AIGNightOneBeatDirector::HandleExtinguisherZone(AIGZoneTrigger* Zone)
{
	UIGMissingFloorNarrativeSubsystem* Narrative = GetNarrative();
	if (!Narrative
		|| !Narrative->MarkBeatPlayed(IGNightOne::ExtinguisherBeatId))
	{
		return;
	}

	AIGPrologueWorldScene* WorldScene = Scene.Get();
	if (!WorldScene || !WorldScene->DropCorridorExtinguisher())
	{
		return;
	}
	bExtinguisherBeatFired = true;

	// The clang and the noise report belong to the impact, not the release:
	// the bracket lets go silently and the floor answers half a second later.
	GetWorldTimerManager().SetTimer(
		ImpactTimer,
		this,
		&AIGNightOneBeatDirector::PlayExtinguisherImpact,
		IGNightOne::ImpactDelaySeconds,
		false);
}

void AIGNightOneBeatDirector::PlayExtinguisherImpact()
{
	UWorld* World = GetWorld();
	AIGPrologueWorldScene* WorldScene = Scene.Get();
	if (!World || !WorldScene)
	{
		return;
	}
	const FVector Impact = WorldScene->GetCorridorExtinguisherLocation();

	// A steel cylinder on granite tile: the low body thud and the thin
	// rattling ring, composed from the existing factories.
	IGAudio::SpawnOneShotAt(
		this,
		UIGToneSequenceSoundWave::CreateDoorThud(this),
		Impact,
		1.0f,
		0.82f);
	IGAudio::SpawnOneShotAt(
		this,
		UIGToneSequenceSoundWave::CreateLockedRattle(this),
		Impact,
		0.8f,
		0.68f);

	// No instigator: the building did this, and the ripple HUD must not tell
	// the player "you made that sound".
	if (UIGNoiseSubsystem* Noise = World->GetSubsystem<UIGNoiseSubsystem>())
	{
		Noise->ReportNoise(Impact, IGNightOne::ImpactLoudness, nullptr);
	}
}

UIGMissingFloorNarrativeSubsystem* AIGNightOneBeatDirector::GetNarrative() const
{
	const UWorld* World = GetWorld();
	UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
	return GameInstance
		? GameInstance->GetSubsystem<UIGMissingFloorNarrativeSubsystem>()
		: nullptr;
}
