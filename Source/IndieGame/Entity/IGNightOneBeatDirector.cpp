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
#include "Player/IGHorrorHUD.h"
#include "Player/IGPlayerCharacter.h"
#include "Player/IGStressComponent.h"
#include "TimerManager.h"

namespace IGNightOne
{
	// Beat ids in the persistent night state: once played, never replayed.
	const FName SightingBeatId(TEXT("Night1.Sighting"));
	const FName ExtinguisherBeatId(TEXT("Night1.Extinguisher"));
	const FName Unit402KnockBeatId(TEXT("Night1.Unit402Knock"));

	/** 402호 문 앞 복도. 문은 X -30, 복도 면 Y -235. */
	const FVector Unit402KnockZoneCenter(-30.0f, -300.0f, 1010.0f);
	const FVector Unit402KnockZoneExtent(70.0f, 62.0f, 110.0f);
	/** 노크는 문 안쪽 40 cm에서 난다. 문짝이 먹은 소리다. */
	const FVector Unit402KnockSource(-30.0f, -200.0f, 1000.0f);
	constexpr float Unit402SecondKnockSeconds = 0.74f;

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

	SpawnParameters.Name = TEXT("Night1Unit402KnockZone");
	Unit402KnockZone = World->SpawnActor<AIGZoneTrigger>(
		AIGZoneTrigger::StaticClass(),
		FTransform(FRotator::ZeroRotator, IGNightOne::Unit402KnockZoneCenter),
		SpawnParameters);
	if (Unit402KnockZone)
	{
		Unit402KnockZone->SetZoneExtent(IGNightOne::Unit402KnockZoneExtent);
		Unit402KnockZone->OnZoneTriggered.AddDynamic(
			this, &AIGNightOneBeatDirector::HandleUnit402KnockZone);
	}

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
	GetWorldTimerManager().ClearTimer(Unit402KnockTimer);
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

	// 형체가 있기 전에 소리가 있어야 한다. 계단참에서 기는 걸음 둘, 그리고
	// 계단 입구의 등이 죽는다 — 내려가면 통로 끝이 실루엣이 된다. 첫 목격이
	// 소리 없는 텔레포트였다.
	IGAudio::SpawnOneShotAt(
		this,
		IGAudio::SampleVariantOr(
			TEXT("Entity_CrawlStep"), 3, 0x51u,
			[this]() -> USoundBase* { return UIGToneSequenceSoundWave::CreateEntityCrawlStep(this, false); }),
		IGNightOne::SightingStagePoint + FVector(0.0f, 0.0f, 20.0f),
		0.7f,
		1.0f,
		200.0f,
		1600.0f,
		EIGAudioBus::Entity);
	AIGHorrorHUD::PushAudioCaptionAt(
		this,
		NSLOCTEXT("IGMissingFloor", "SightingCrawlCaption", "기는 소리"),
		2.0f,
		IGNightOne::SightingStagePoint);
	GetWorldTimerManager().SetTimer(
		SightingStepTimer,
		FTimerDelegate::CreateWeakLambda(this, [this]()
		{
			IGAudio::SpawnOneShotAt(
				this,
				IGAudio::SampleVariantOr(
					TEXT("Entity_CrawlStep"), 3, 0x9Bu,
					[this]() -> USoundBase* { return UIGToneSequenceSoundWave::CreateEntityCrawlStep(this, false); }),
				IGNightOne::SightingShufflePoint + FVector(0.0f, 0.0f, 20.0f),
				0.6f,
				1.0f,
				200.0f,
				1600.0f,
				EIGAudioBus::Entity);
		}),
		0.85f,
		false);
	SightingThroatFixture = INDEX_NONE;
	float NearestSquared = FMath::Square(260.0f);
	for (int32 Index = 0; Index < WorldScene->GetCorridorFixtureCount(); ++Index)
	{
		const float DistanceSquared = FVector::DistSquared2D(
			WorldScene->GetCorridorFixtureLocation(Index),
			IGNightOne::SightingZoneCenter);
		if (DistanceSquared < NearestSquared)
		{
			NearestSquared = DistanceSquared;
			SightingThroatFixture = Index;
		}
	}
	if (SightingThroatFixture != INDEX_NONE)
	{
		WorldScene->SetFixtureLive(SightingThroatFixture, false, true);
	}

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
		// 내려가며 그를 지나치는 순간 그가 고개를 든다. 이미 본 형상의 상태 변화
		// (STORY_DIRECTION §7) — 아무 소리 없이 지나가던 카메오에 놀람이 생긴다.
		if (AIGListenerEntity* Listener = Entity.Get())
		{
			IGAudio::SpawnOneShotAt(
				this,
				UIGToneSequenceSoundWave::CreateEntityAlertVocal(this),
				Listener->GetActorLocation() + FVector(0.0f, 0.0f, 30.0f),
				0.85f,
				0.96f,
				220.0f,
				2200.0f,
				EIGAudioBus::Entity);
			AIGHorrorHUD::PushFearDirection(this, Listener->GetActorLocation());
		}
		if (AIGPlayerCharacter* PlayerCharacter = Player.Get())
		{
			if (UIGStressComponent* Stress = PlayerCharacter->GetStress())
			{
				Stress->ApplyScare(0.5f);
			}
			PlayerCharacter->PlayScareKick(1.6f);
		}
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
		if (SightingThroatFixture != INDEX_NONE)
		{
			WorldScene->SetFixtureLive(SightingThroatFixture, true, true);
			SightingThroatFixture = INDEX_NONE;
		}
		WorldScene->SuspendCorridorFlicker(false);
	}
}

// -- 1-6 402호의 노크 ------------------------------------------------------

void AIGNightOneBeatDirector::HandleUnit402KnockZone(AIGZoneTrigger* Zone)
{
	// 소화기 비트를 겪은 뒤에만. 그 전에는 복도가 아직 규칙을 가르치는 중이고,
	// 규칙을 배우기 전의 놀람은 정보가 아니라 소음이다.
	if (!bExtinguisherBeatFired)
	{
		return;
	}
	UIGMissingFloorNarrativeSubsystem* Narrative = GetNarrative();
	if (!Narrative || !Narrative->MarkBeatPlayed(IGNightOne::Unit402KnockBeatId))
	{
		return;
	}
	const auto Knock = [this]()
	{
		IGAudio::SpawnOneShotAt(
			this,
			IGAudio::SampleVariantOr(
				TEXT("Knock_Plaster"), 3, static_cast<uint32>(GetWorld()->GetTimeSeconds() * 977.0f),
				[this]() -> USoundBase* { return UIGToneSequenceSoundWave::CreateWallKnockSingle(this, 0.6f); }),
			IGNightOne::Unit402KnockSource,
			0.7f,
			0.78f,
			140.0f,
			1100.0f,
			EIGAudioBus::World);
	};
	Knock();
	GetWorldTimerManager().SetTimer(
		Unit402KnockTimer,
		this,
		&AIGNightOneBeatDirector::PlayUnit402SecondKnock,
		IGNightOne::Unit402SecondKnockSeconds,
		false);
	AIGHorrorHUD::PushFearDirection(this, IGNightOne::Unit402KnockSource);
	AIGHorrorHUD::PushAudioCaptionAt(
		this,
		NSLOCTEXT("IGMissingFloor", "Unit402KnockCaption", "402호 안쪽 — 노크 둘"),
		2.2f,
		IGNightOne::Unit402KnockSource);
}

void AIGNightOneBeatDirector::PlayUnit402SecondKnock()
{
	IGAudio::SpawnOneShotAt(
		this,
		IGAudio::SampleVariantOr(
			TEXT("Knock_Plaster"), 3, 0x3Fu,
			[this]() -> USoundBase* { return UIGToneSequenceSoundWave::CreateWallKnockSingle(this, 0.6f); }),
		IGNightOne::Unit402KnockSource,
		0.62f,
		0.74f,
		140.0f,
		1100.0f,
		EIGAudioBus::World);
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
	if (USoundBase* Drop = IGAudio::Sample(TEXT("Extinguisher_Drop")))
	{
		IGAudio::SpawnOneShotAt(this, Drop, Impact, 1.0f, 1.0f, 200.0f, 2400.0f, EIGAudioBus::World);
	}
	else
	{
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
	}

	// No instigator: the building did this, and the ripple HUD must not tell
	// the player "you made that sound".
	if (UIGNoiseSubsystem* Noise = World->GetSubsystem<UIGNoiseSubsystem>())
	{
		Noise->ReportNoise(Impact, IGNightOne::ImpactLoudness, nullptr);
	}
	// 쇠통이 떨어지는데 몸이 가만히 있을 수는 없다. 그리고 2.2초 뒤, 등 뒤의
	// 등이 죽는다 — 시야 밖에서 꺼지는 조명(STORY_DIRECTION §7 허용).
	if (AIGPlayerCharacter* PlayerCharacter = Player.Get())
	{
		if (UIGStressComponent* Stress = PlayerCharacter->GetStress())
		{
			Stress->ApplyScare(0.35f);
		}
		PlayerCharacter->PlayScareKick(1.2f);
	}
	AIGHorrorHUD::PushFearDirection(this, Impact);
	if (!bFixtureDeathFired)
	{
		GetWorldTimerManager().SetTimer(
			FixtureDeathTimer,
			this,
			&AIGNightOneBeatDirector::KillFixtureBehindPlayer,
			2.2f,
			false);
	}
}

void AIGNightOneBeatDirector::KillFixtureBehindPlayer()
{
	if (bFixtureDeathFired)
	{
		return;
	}
	AIGPrologueWorldScene* WorldScene = Scene.Get();
	AIGPlayerCharacter* PlayerCharacter = Player.Get();
	if (!WorldScene || !PlayerCharacter)
	{
		return;
	}
	// 등 뒤, 가장 가까운 등. 시야 안의 등을 죽이면 「원인이 보이는 놀람」이 된다.
	const FVector PlayerLocation = PlayerCharacter->GetActorLocation();
	const FVector View = PlayerCharacter->GetControlRotation().Vector().GetSafeNormal2D();
	int32 Best = INDEX_NONE;
	float BestDistance = TNumericLimits<float>::Max();
	for (int32 Index = 0; Index < WorldScene->GetCorridorFixtureCount(); ++Index)
	{
		const FVector Fixture = WorldScene->GetCorridorFixtureLocation(Index);
		FVector ToFixture = Fixture - PlayerLocation;
		ToFixture.Z = 0.0f;
		const float Distance = ToFixture.Size();
		if (Distance < 60.0f || Distance > 900.0f
			|| FVector::DotProduct(View, ToFixture / Distance) > 0.15f)
		{
			continue;
		}
		if (Distance < BestDistance)
		{
			BestDistance = Distance;
			Best = Index;
		}
	}
	if (Best == INDEX_NONE)
	{
		return;
	}
	bFixtureDeathFired = true;
	const FVector FixtureLocation = WorldScene->GetCorridorFixtureLocation(Best);
	IGAudio::SpawnOneShotAt(
		this,
		IGAudio::SampleOr(
			TEXT("Ballast_Tick"),
			[this]() -> USoundBase* { return UIGToneSequenceSoundWave::CreateFluorescentBallastSnap(this); }),
		FixtureLocation,
		1.0f,
		1.0f,
		200.0f,
		1600.0f,
		EIGAudioBus::World);
	WorldScene->SetFixtureLive(Best, false, true);
	AIGHorrorHUD::PushFearDirection(this, FixtureLocation);
	if (UIGStressComponent* Stress = PlayerCharacter->GetStress())
	{
		Stress->ApplyScare(0.30f);
	}
	PlayerCharacter->PlayScareKick(0.7f);
}

UIGMissingFloorNarrativeSubsystem* AIGNightOneBeatDirector::GetNarrative() const
{
	const UWorld* World = GetWorld();
	UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
	return GameInstance
		? GameInstance->GetSubsystem<UIGMissingFloorNarrativeSubsystem>()
		: nullptr;
}
