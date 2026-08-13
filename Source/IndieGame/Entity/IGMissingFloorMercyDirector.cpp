#include "Entity/IGMissingFloorMercyDirector.h"

#include "Audio/IGAudioHelpers.h"
#include "Audio/IGToneSequenceSoundWave.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Entity/IGListenerEntity.h"
#include "Entity/IGMissingFloorNightThreeDirector.h"
#include "Narrative/IGMissingFloorNarrativeSubsystem.h"

namespace IGMercy
{
	/** Coarse on purpose: the clock is ninety seconds long. */
	constexpr float TickIntervalSeconds = 0.25f;

	/**
	 * The riser cry is heard through finished wall from wherever the player is,
	 * so it arrives at the muffled end of the §21.3 원근 4단. It is a sound that
	 * says "there is water in this building and it is moving", which is a place
	 * to walk toward — not an instruction.
	 */
	constexpr int32 PipeCryDistanceStep = 2;
	constexpr float PipeCryVolume = 0.66f;
	constexpr float PipeCryInnerRadius = 220.0f;
	constexpr float PipeCryFalloff = 2200.0f;
	/** The shared riser, above the fifth-floor bays. */
	const FVector RiserLocation(310.0f, 700.0f, 1300.0f);
}

AIGMissingFloorMercyDirector::AIGMissingFloorMercyDirector()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = IGMercy::TickIntervalSeconds;
	// Nothing to watch until the hour opens.
	PrimaryActorTick.bStartWithTickEnabled = false;
}

void AIGMissingFloorMercyDirector::Configure(
	AIGListenerEntity* InEntity,
	AIGMissingFloorNightThreeDirector* InNightThree)
{
	Entity = InEntity;
	NightThree = InNightThree;
}

UIGMissingFloorNarrativeSubsystem*
AIGMissingFloorMercyDirector::GetNarrative() const
{
	const UWorld* World = GetWorld();
	UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
	return GameInstance
		? GameInstance->GetSubsystem<UIGMissingFloorNarrativeSubsystem>()
		: nullptr;
}

void AIGMissingFloorMercyDirector::SetHourActive(const bool bActive)
{
	bHourActive = bActive;
	SetActorTickEnabled(bActive);
	if (!bActive)
	{
		// The day is not a place to be stuck in, and the clock should not carry
		// a night's frustration into the next one.
		StuckSeconds = 0.0f;
		ResetsSinceNewSource = 0;
		LastSourceCount = -1;
	}
}

void AIGMissingFloorMercyDirector::NotifyCaptureReset()
{
	if (!bHourActive)
	{
		return;
	}
	++ResetsSinceNewSource;
	// The clock starts again from the wake, not from where the capture found
	// them: a player who just lost the hour has not had ninety seconds yet.
	StuckSeconds = 0.0f;
	if (ResetsSinceNewSource < ResetsForEnvironmentHint)
	{
		return;
	}
	// §20.3-1. Two resets with nothing learned in between is the clearest signal
	// the game gets that the player cannot see what it is showing them.
	if (FireWorldResponse() != EIGMercyResponse::None)
	{
		++ResetHintCount;
		ResetsSinceNewSource = 0;
	}
}

void AIGMissingFloorMercyDirector::Tick(const float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	const UWorld* World = GetWorld();
	if (!bHourActive || !World || World->IsPaused())
	{
		// A note or a settings menu is not being stuck. The pressure clock stops
		// for them (§19.7) and so does this one.
		return;
	}

	const UIGMissingFloorNarrativeSubsystem* Narrative = GetNarrative();
	if (!Narrative)
	{
		return;
	}
	const int32 SourceCount = Narrative->GetTotalSourceCount();
	if (SourceCount != LastSourceCount)
	{
		// Something was learned. That is what progress means here, and both nets
		// stand down for it — including the reset counter, because a player who
		// found something after being caught twice is no longer stuck.
		LastSourceCount = SourceCount;
		StuckSeconds = 0.0f;
		ResetsSinceNewSource = 0;
		return;
	}

	StuckSeconds += DeltaSeconds;
	if (StuckSeconds < StuckResponseSeconds)
	{
		return;
	}
	// §20.3-2. Firing resets the clock, so the world speaks at most once every
	// ninety seconds and never becomes a metronome the player waits on.
	StuckSeconds = 0.0f;
	FireWorldResponse();
}

EIGMercyResponse AIGMissingFloorMercyDirector::FireWorldResponse()
{
	// Alternate so two consecutive nudges are never the same nudge. The ear to
	// the wall is the stronger of the two and only exists on night three, so the
	// pipes carry the rotation everywhere else.
	const bool bPreferEar = LastResponse != EIGMercyResponse::EarToWall;
	EIGMercyResponse Fired = EIGMercyResponse::None;
	if (bPreferEar && TryEarToWall())
	{
		Fired = EIGMercyResponse::EarToWall;
	}
	else if (TryPipeCry())
	{
		Fired = EIGMercyResponse::PipeCry;
	}
	else if (!bPreferEar && TryEarToWall())
	{
		Fired = EIGMercyResponse::EarToWall;
	}
	if (Fired == EIGMercyResponse::None)
	{
		return Fired;
	}

	LastResponse = Fired;
	++ResponseCount;
	UE_LOG(
		LogTemp,
		Display,
		TEXT("MISSINGFLOOR_MERCY response=%s count=%d resets=%d"),
		Fired == EIGMercyResponse::EarToWall ? TEXT("ear_to_wall") : TEXT("pipe_cry"),
		ResponseCount,
		ResetHintCount);
	return Fired;
}

bool AIGMissingFloorMercyDirector::TryEarToWall()
{
	AIGListenerEntity* EntityActor = Entity.Get();
	AIGMissingFloorNightThreeDirector* NightThreeActor = NightThree.Get();
	if (!EntityActor || !NightThreeActor || EntityActor->IsDormant())
	{
		return false;
	}
	FVector Observation = FVector::ZeroVector;
	if (!NightThreeActor->GetCavityWallObservationPoint(Observation))
	{
		return false;
	}
	// He is not being told to go there either. He goes because he is drawn to
	// the wall his brother is behind, which is what he has been doing all along
	// (§4.2 적의가 아니라 갈망) — the player simply happens to be watching.
	EntityActor->BeginObservationHold(Observation);
	return true;
}

bool AIGMissingFloorMercyDirector::TryPipeCry()
{
	if (!GetWorld())
	{
		return false;
	}
	IGAudio::SpawnOneShotAt(
		this,
		UIGToneSequenceSoundWave::CreatePipeWaterFlow(
			this,
			IGMercy::PipeCryDistanceStep),
		IGMercy::RiserLocation,
		IGMercy::PipeCryVolume,
		1.0f,
		IGMercy::PipeCryInnerRadius,
		IGMercy::PipeCryFalloff,
		EIGAudioBus::Puzzle);
	return true;
}

bool AIGMissingFloorMercyDirector::ForceWorldResponseForTesting()
{
	return FireWorldResponse() != EIGMercyResponse::None;
}
