#include "Entity/IGListenerEntity.h"

#include "Audio/IGAudioHelpers.h"
#include "Audio/IGMissingFloorAudioSubsystem.h"
#include "Audio/IGToneSequenceSoundWave.h"
#include "Components/AudioComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Environment/IGDustSubsystem.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Narrative/IGMissingFloorNarrativeSubsystem.h"
#include "Player/IGPlayerCharacter.h"
#include "Kismet/GameplayStatics.h"
#include "Player/IGStressComponent.h"

namespace IGListener
{
	// Timing of the idle cycle. The triple knock is the player's masked
	// window to move; keep these in step with CreateWallKnockTriple.
	constexpr float BangSeconds = 2.1f;
	constexpr float BangMasking = 0.3f;
	/** 한 밤에 대답이 통하는 횟수. 그 뒤로는 벽의 잔향만 돌아온다. */
	constexpr int32 AnswersPerNight = 3;
	constexpr float SearchSeconds = 12.0f;
	constexpr float SearchRadius = 350.0f;
	constexpr float ChaseGiveUpSeconds = 8.0f;
	constexpr float CaptureHoldSeconds = 2.5f;
	/** Hearing bonus while deliberately listening. */
	constexpr float ListeningGain = 2.0f;
	constexpr float HoldingGain = 1.5f;
	/** A second sound within this window of the first confirms the prey. */
	constexpr float ReactionMemorySeconds = 10.0f;

	/**
	 * Drag distance between audible dust sifts (§10.3 분진 낙하). About two
	 * corridor bays: often enough that a moving entity keeps shedding evidence,
	 * rare enough that the sift stays an event.
	 */
	constexpr float DustSiftIntervalCentimeters = 260.0f;
	/** The thinnest cue in the game. It hints; it never announces. */
	constexpr float DustSiftVolume = 0.42f;

	/** How often the floor under him is re-traced while he moves, in seconds. */
	constexpr float DragSurfacePollInterval = 0.30f;
	/** Sheet vinyl, tagged by the world scene for the §21.2 footstep matrix. */
	const FName VinylSurfaceTag(TEXT("Footstep.Vinyl"));
}

AIGListenerEntity::AIGListenerEntity()
{
	PrimaryActorTick.bCanEverTick = true;

	Body = CreateDefaultSubobject<UCapsuleComponent>(TEXT("Body"));
	// Low and wide: an upper body on elbows with the legs trailing behind.
	Body->InitCapsuleSize(42.0f, 58.0f);
	Body->SetCollisionProfileName(TEXT("Pawn"));
	Body->SetCanEverAffectNavigation(false);
	SetRootComponent(Body);

	// The entity is never player-possessed and never uses a controller brain;
	// the state machine below is the whole mind.
	AutoPossessAI = EAutoPossessAI::Disabled;
}

void AIGListenerEntity::BeginPlay()
{
	Super::BeginPlay();

	SpawnLocation = GetActorLocation();
	SearchAnchor = SpawnLocation;
	BuildGreyboxBody();

	if (UWorld* World = GetWorld())
	{
		NoiseSubsystem = World->GetSubsystem<UIGNoiseSubsystem>();
		if (NoiseSubsystem)
		{
			NoiseHandle = NoiseSubsystem->OnNoiseReported.AddUObject(
				this, &AIGListenerEntity::HandleNoise);
		}

		// The crawl bed runs for the entity's whole life; movement only
		// changes its volume, so silence always means "it stopped".
		DragLoopComponent = NewObject<UAudioComponent>(this);
		DragLoopComponent->RegisterComponent();
		DragLoopComponent->AttachToComponent(
			Body, FAttachmentTransformRules::KeepRelativeTransform);
		DragLoopComponent->SetSound(
			UIGToneSequenceSoundWave::CreateEntityDragLoop(this));
		DragLoopComponent->AttenuationSettings = IGAudio::MakeAttenuation(
			this,
			220.0f,
			2400.0f,
			EIGAudioBus::Entity);
		DragLoopComponent->bAllowSpatialization = true;
		DragLoopComponent->SetVolumeMultiplier(0.0f);
		if (UIGMissingFloorAudioSubsystem* AudioDirector =
			World->GetSubsystem<UIGMissingFloorAudioSubsystem>())
		{
			AudioDirector->RegisterComponent(
				DragLoopComponent,
				EIGAudioBus::Entity);
		}
		DragLoopComponent->Play();

		// 숨은 늘 쉰다. 멈춰 있어도 가까우면 들린다 — 끌림이 0인 자리에서 그가
		// 거기 있다는 것을 알려 주는 유일한 소리다.
		BreathLoopComponent = NewObject<UAudioComponent>(this);
		BreathLoopComponent->RegisterComponent();
		BreathLoopComponent->AttachToComponent(
			Body, FAttachmentTransformRules::KeepRelativeTransform);
		BreathLoopComponent->SetRelativeLocation(FVector(30.0f, 0.0f, 20.0f));
		BreathLoopComponent->SetSound(
			UIGToneSequenceSoundWave::CreateEntityBreathLoop(this));
		BreathLoopComponent->AttenuationSettings = IGAudio::MakeAttenuation(
			this,
			160.0f,
			1500.0f,
			EIGAudioBus::Entity);
		BreathLoopComponent->bAllowSpatialization = true;
		BreathLoopComponent->SetVolumeMultiplier(0.0f);
		if (UIGMissingFloorAudioSubsystem* AudioDirector =
			World->GetSubsystem<UIGMissingFloorAudioSubsystem>())
		{
			AudioDirector->PrepareSound(BreathLoopComponent->Sound, EIGAudioBus::Entity);
			AudioDirector->RegisterPersistentBed(
				BreathLoopComponent,
				EIGAudioBus::Entity);
		}
		BreathLoopComponent->Play();
	}

	// §20.4: the mode is a user setting, read once when he wakes into the world.
	// A run started from the harness can override it without writing it back.
	Difficulty = IGListenerTuning::ResolveActiveDifficulty();
	RefreshNightTuning();

	EnterState(EIGListenerState::Patrolling);
}

void AIGListenerEntity::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (NoiseSubsystem)
	{
		NoiseSubsystem->OnNoiseReported.Remove(NoiseHandle);
		// Never leave the building masked by a dead entity's knock window.
		NoiseSubsystem->SetGlobalMasking(0.0f);
	}
	Super::EndPlay(EndPlayReason);
}

void AIGListenerEntity::Tick(const float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	StateSeconds += DeltaSeconds;
	TickState(DeltaSeconds);
	UpdatePresentationLayer();
	UpdatePresentationPose(LastMoveSpeed, DeltaSeconds);
	UpdateDragLoop(LastMoveSpeed);
	// Only while he is actually moving, and only a few times a second: a trace
	// per frame for a sound that changes at a doorway would be pure waste.
	if (LastMoveSpeed > 1.0f)
	{
		DragSurfacePollSeconds += DeltaSeconds;
		if (DragSurfacePollSeconds >= IGListener::DragSurfacePollInterval)
		{
			DragSurfacePollSeconds = 0.0f;
			RefreshDragSurface();
		}
	}
	UpdateThreatPressure();
	ReportDustTrail();
	LastMoveSpeed = 0.0f;
}

// -- state machine ---------------------------------------------------------

void AIGListenerEntity::EnterState(const EIGListenerState NewState)
{
	// Leaving the knock window always clears its masking, whatever comes next.
	if (State == EIGListenerState::Banging && NoiseSubsystem)
	{
		NoiseSubsystem->SetGlobalMasking(0.0f);
	}

	const EIGListenerState PreviousState = State;
	State = NewState;
	StateSeconds = 0.0f;
	StuckSeconds = 0.0f;

	// 잡는 순간부터는 1인칭 포옹이 이 존재를 대신 그린다. 몸을 그대로 세워
	// 두면 팔이 화면을 감싸는 동안 같은 것이 복도 바닥에 한 번 더 보인다 —
	// m1 포획 캡처에 실제로 둘이 같이 찍혀 있었다. 충돌과 소리는 그대로 두고
	// 그림만 내린다.
	if (NewState == EIGListenerState::CaptureHold)
	{
		SetActorHiddenInGame(true);
	}
	else if (PreviousState == EIGListenerState::CaptureHold)
	{
		// 낮에는 잠들어 있어야 하므로 휴면 상태를 그대로 따른다.
		SetActorHiddenInGame(bDormant);
	}

	if (UWorld* World = GetWorld())
	{
		if (UIGMissingFloorAudioSubsystem* AudioDirector =
			World->GetSubsystem<UIGMissingFloorAudioSubsystem>())
		{
			EIGAudioThreatState AudioState = EIGAudioThreatState::Calm;
			switch (NewState)
			{
			case EIGListenerState::Banging:
				AudioState = EIGAudioThreatState::Banging;
				break;
			case EIGListenerState::Listening:
				AudioState = EIGAudioThreatState::Listening;
				break;
			case EIGListenerState::Investigating:
			case EIGListenerState::Holding:
			case EIGListenerState::Searching:
				AudioState = EIGAudioThreatState::Investigating;
				break;
			case EIGListenerState::Chasing:
				AudioState = EIGAudioThreatState::Chasing;
				break;
			case EIGListenerState::CaptureHold:
			case EIGListenerState::FinaleLured:
				AudioState = EIGAudioThreatState::Finale;
				break;
			case EIGListenerState::Patrolling:
			case EIGListenerState::Waiting:
			default:
				break;
			}
			AudioDirector->SetThreatState(AudioState);
			// §18.6 CHASE 진입. 소리가 상태를 바꾸는 자리에서 손도 함께
			// 바꾼다 — 둘을 떼어 두면 추격이 끝났는데 패드만 계속 우는
			// 상태가 만들어진다.
			if (AIGPlayerCharacter* PlayerCharacter =
				Cast<AIGPlayerCharacter>(
					UGameplayStatics::GetPlayerPawn(this, 0)))
			{
				PlayerCharacter->SetChaseHaptic(
					AudioState == EIGAudioThreatState::Chasing);
			}
		}
	}

	// 상태가 바뀌는 소리. 예전엔 조사도 추격도 소리 없이 시작됐다 — 음악이
	// 바뀌는 것 말고는 그가 무엇을 들었는지 알 길이 없었다.
	if (!bDormant)
	{
		const bool bWasIdle =
			PreviousState == EIGListenerState::Patrolling
			|| PreviousState == EIGListenerState::Banging
			|| PreviousState == EIGListenerState::Listening
			|| PreviousState == EIGListenerState::Waiting;
		if (NewState == EIGListenerState::Investigating && bWasIdle)
		{
			IGAudio::SpawnOneShotAt(
				this,
				UIGToneSequenceSoundWave::CreateEntityAlertVocal(this),
				GetActorLocation() + FVector(30.0f, 0.0f, 30.0f),
				0.9f,
				1.0f,
				220.0f,
				2400.0f,
				EIGAudioBus::Entity);
		}
		else if (NewState == EIGListenerState::Chasing && PreviousState != EIGListenerState::Chasing)
		{
			if (UWorld* World = GetWorld())
			{
				if (UIGMissingFloorAudioSubsystem* AudioDirector =
					World->GetSubsystem<UIGMissingFloorAudioSubsystem>())
				{
					AudioDirector->PlayStinger(
						EIGStinger::ChaseStart, GetActorLocation() + FVector(30.0f, 0.0f, 30.0f));
				}
			}
			if (AIGPlayerCharacter* PlayerCharacter =
				Cast<AIGPlayerCharacter>(UGameplayStatics::GetPlayerPawn(this, 0)))
			{
				// 비명은 몸으로도 온다. 가까울수록 크게.
				const float Distance = FVector::Dist(
					PlayerCharacter->GetActorLocation(), GetActorLocation());
				const float Near = FMath::Clamp(1.0f - Distance / 1600.0f, 0.0f, 1.0f);
				if (UIGStressComponent* Stress = PlayerCharacter->GetStress())
				{
					Stress->ApplyScare(0.35f + 0.30f * Near);
				}
				PlayerCharacter->PlayScareKick(0.8f + 1.2f * Near);
			}
		}
	}

	switch (NewState)
	{
	case EIGListenerState::Banging:
		if (NoiseSubsystem)
		{
			NoiseSubsystem->SetGlobalMasking(IGListener::BangMasking);
		}
		PlayKnockTriple();
		break;

	case EIGListenerState::Listening:
	case EIGListenerState::Holding:
		// It plants itself; the shell settles.
		PlayPlasterSettle();
		break;

	case EIGListenerState::Searching:
		SearchAnchor = GetActorLocation();
		SearchTarget = SearchAnchor;
		SearchRetargetSeconds = 0.0f;
		break;

	case EIGListenerState::Patrolling:
		bReactingToSound = false;
		break;

	case EIGListenerState::Waiting:
		// The learned family answer does not stun a monster. It makes Doha
		// plant both elbows and listen like a person expecting the next knock.
		ListenerPhase = 1.0f;
		ListenerPhaseIndex = INDEX_NONE;
		break;

	default:
		break;
	}
}

void AIGListenerEntity::TickState(const float DeltaSeconds)
{
	switch (State)
	{
	case EIGListenerState::Patrolling:
	{
		const FVector* Target = CurrentPatrolTarget();
		if (!Target)
		{
			// No route: haunt the spawn point in place.
			EnterState(EIGListenerState::Banging);
			break;
		}
		if (CrawlTowards(*Target, CrawlSpeed, DeltaSeconds))
		{
			EnterState(EIGListenerState::Banging);
		}
		break;
	}

	case EIGListenerState::Banging:
		if (StateSeconds >= IGListener::BangSeconds)
		{
			EnterState(EIGListenerState::Listening);
		}
		break;

	case EIGListenerState::Listening:
		if (StateSeconds >= ListenSecondsForTier())
		{
			// Tier 3 stops walking the route and goes to sit on the player's
			// habit instead (§5.6). Everything else takes the next stop.
			if (!TryBeginAmbush())
			{
				AdvancePatrolIndex();
				EnterState(EIGListenerState::Patrolling);
			}
		}
		break;

	case EIGListenerState::Investigating:
		if (CrawlTowards(LastHeardLocation, InvestigateSpeed, DeltaSeconds))
		{
			EnterState(EIGListenerState::Holding);
		}
		break;

	case EIGListenerState::Holding:
		// §20.2 「INVESTIGATE 도착 청취」. 표는 밤마다 6·6·5·5초라고 적어 두었는데
		// 실제로는 상수 6초만 읽고 있었다.
		if (StateSeconds >= FMath::Max(Tuning.InvestigateHoldSeconds, 0.5f))
		{
			EnterState(EIGListenerState::Searching);
		}
		break;

	case EIGListenerState::Chasing:
	{
		CrawlTowards(LastHeardLocation, ChaseSpeed, DeltaSeconds);
		const double SilenceSeconds =
			GetWorld()->GetRealTimeSeconds() - LastHeardTime;
		if (SilenceSeconds >= IGListener::ChaseGiveUpSeconds)
		{
			EnterState(EIGListenerState::Searching);
		}
		break;
	}

	case EIGListenerState::Searching:
		SearchRetargetSeconds -= DeltaSeconds;
		if (SearchRetargetSeconds <= 0.0f)
		{
			// Short blind sweeps around where the trail went cold. The
			// wobble is deterministic so replays and captures line up.
			const uint32 Hash =
				static_cast<uint32>(StateSeconds * 977.0f) * 2654435761u;
			const float Angle =
				2.0f * PI * ((Hash >> 8) & 0xFFFF) / 65536.0f;
			SearchTarget = SearchAnchor
				+ FVector(FMath::Cos(Angle), FMath::Sin(Angle), 0.0f)
				* IGListener::SearchRadius;
			SearchRetargetSeconds = 3.0f;
		}
		CrawlTowards(SearchTarget, CrawlSpeed, DeltaSeconds);
		if (StateSeconds >= IGListener::SearchSeconds)
		{
			bReactingToSound = false;
			EnterState(EIGListenerState::Patrolling);
		}
		break;

	case EIGListenerState::Waiting:
		// Hope, then the spot the answer came from.
		if (StateSeconds >= WaitSecondsForTier())
		{
			LastHeardLocation = AnswerKnockLocation;
			bReactingToSound = true;
			EnterState(EIGListenerState::Investigating);
		}
		break;

	case EIGListenerState::CaptureHold:
		// With a director bound, the reset (and the aggression raise) is its
		// call; the self-reset only covers a directorless test arena.
		if (StateSeconds >= IGListener::CaptureHoldSeconds
			&& !OnPlayerCaptured.IsBound())
		{
			ResetToPatrolStart(true);
		}
		break;

	case EIGListenerState::FinaleLured:
		if (!FinaleRoutePoints.IsValidIndex(FinaleRouteIndex))
		{
			SetDormant(true);
			break;
		}
		if (CrawlTowards(
				FinaleRoutePoints[FinaleRouteIndex],
				ChaseSpeed * 0.82f,
				DeltaSeconds))
		{
			++FinaleRouteIndex;
		}
		break;
	}

	// Touching the player ends the night. 예전엔 조사·추격·수색 셋에서만 잡았다.
	// 두드리는 2.1초, 듣는 8초, 서서 기다리는 6초 동안 110cm 안에 서 있어도
	// 아무 일이 없었다 — 그의 팔이 닿는 자리는 상태를 가리지 않는다. 대답 뒤의
	// 기다림(Waiting)만 약속이라 예외다.
	if (!bDormant
		&& State != EIGListenerState::Waiting
		&& State != EIGListenerState::CaptureHold
		&& State != EIGListenerState::FinaleLured)
	{
		if (!CachedPlayer.IsValid())
		{
			for (TActorIterator<AIGPlayerCharacter> It(GetWorld()); It; ++It)
			{
				CachedPlayer = *It;
				break;
			}
		}
		if (APawn* Player = CachedPlayer.Get())
		{
			const float Distance =
				FVector::Dist(Player->GetActorLocation(), GetActorLocation());
			// 듣기만 하는 밤: he reaches the sound and holds there, and that is
			// where it ends. Touching costs nothing, so the night can never be
			// taken away — the story, the puzzles and all three endings stay
			// exactly the same (§20.4).
			if (Distance <= CaptureRadius && Tuning.bCaptureEnabled)
			{
				BeginCapture(Player);
			}
		}
	}
}

void AIGListenerEntity::HandleNoise(const FIGNoiseEvent& Event)
{
	// Its own presentation sounds never enter the bus, so no self-filter is
	// needed beyond ignoring events it instigated by design.
	if (Event.Instigator.Get() == this)
	{
		return;
	}
	if (State == EIGListenerState::Waiting
		|| State == EIGListenerState::CaptureHold
		|| State == EIGListenerState::FinaleLured)
	{
		return;
	}
	if (!CanHear(Event))
	{
		return;
	}

	const double Now = Event.TimeSeconds;
	const bool bSecondSound =
		bReactingToSound
		&& (Now - LastHeardTime) <= IGListener::ReactionMemorySeconds;

	LastHeardLocation = Event.Location;
	LastHeardTime = Now;

	if (Event.Instigator.IsValid()
		&& Event.Instigator->IsA<AIGPlayerCharacter>())
	{
		CachedPlayer = Cast<APawn>(Event.Instigator.Get());
	}

	if (State == EIGListenerState::Chasing)
	{
		return; // Already committed; the new location is enough.
	}

	if (bSecondSound && Tuning.bChaseEnabled)
	{
		// A sound that answers twice is a someone.
		EnterState(EIGListenerState::Chasing);
		return;
	}

	bReactingToSound = true;
	EnterState(EIGListenerState::Investigating);
}

bool AIGListenerEntity::CanHear(const FIGNoiseEvent& Event) const
{
	if (Event.Loudness <= 0.0f)
	{
		return false;
	}
	float Distance = FVector::Dist(Event.Location, GetActorLocation());
	const float HeightGap =
		FMath::Abs(Event.Location.Z - GetActorLocation().Z);
	if (HeightGap > FloorHeightThreshold)
	{
		// Another floor: the structure eats some of the sound.
		Distance *= CrossFloorDistancePenalty;
	}
	// §20.2 기본 청취 반경. The night sensitivity scales his ear, not the sound:
	// a hammer still carries as far as a hammer carries, and the same footstep
	// simply reaches him from farther away as the nights go on.
	return Distance
		<= Event.Radius * HearingMultiplier() * Tuning.HearingSensitivity;
}

float AIGListenerEntity::HearingMultiplier() const
{
	switch (State)
	{
	case EIGListenerState::Listening:
		return IGListener::ListeningGain;
	case EIGListenerState::Holding:
		return IGListener::HoldingGain;
	default:
		return 1.0f;
	}
}

float AIGListenerEntity::ListenSecondsForTier() const
{
	// §20.2 gives the night base and §4.3-7 the tier axis; the tuning table has
	// already multiplied them, so there is one number left to obey.
	return Tuning.ListenWindowSeconds;
}

float AIGListenerEntity::WaitSecondsForTier() const
{
	// Hope wears out: each reset it waits less on an answer. 조용한 밤 stretches
	// every one of those waits by half again — more time to answer, same fear.
	// 한 밤 안에서도 두 번째 대답은 0.6배, 세 번째는 0.35배만 기다린다.
	static constexpr float Seconds[4] = {20.0f, 12.0f, 6.0f, 6.0f};
	static constexpr float AnswerDecay[3] = {1.0f, 0.6f, 0.35f};
	const int32 AnswerIndex = FMath::Clamp(AnswersThisNight - 1, 0, 2);
	return Seconds[FMath::Clamp(AggressionTier, 0, 3)] * Tuning.WaitScale
		* AnswerDecay[AnswerIndex];
}

// -- public controls -------------------------------------------------------

void AIGListenerEntity::SetAggressionTier(const int32 Tier)
{
	AggressionTier = FMath::Clamp(Tier, 0, 3);
	// The tier is one of the three axes, so the resolved numbers move with it.
	RefreshNightTuning();
}

void AIGListenerEntity::BeginObservationHold(const FVector& Target)
{
	if (bDormant || State == EIGListenerState::CaptureHold
		|| State == EIGListenerState::FinaleLured)
	{
		// Never interrupt a capture or the authored finale pass to be helpful.
		return;
	}
	// Keep his own floor: he crawls, and the state machine cannot drag him
	// through a slab to reach a spot the mercy net picked.
	LastHeardLocation =
		FVector(Target.X, Target.Y, GetActorLocation().Z);
	bReactingToSound = false;
	EnterState(EIGListenerState::Investigating);
}

void AIGListenerEntity::RefreshNightTuning()
{
	int32 NightIndex = IGListenerTuning::FirstNight;
	if (const UWorld* World = GetWorld())
	{
		if (const UGameInstance* GameInstance = World->GetGameInstance())
		{
			if (const UIGMissingFloorNarrativeSubsystem* Narrative =
				GameInstance->GetSubsystem<UIGMissingFloorNarrativeSubsystem>())
			{
				NightIndex = Narrative->GetNightIndex();
			}
		}
	}
	Tuning = IGListenerTuning::Resolve(NightIndex, Difficulty, AggressionTier);
	ChaseSpeed = Tuning.ChaseSpeed;

	// 듣기만 하는 밤에서는 이미 시작된 추격과 포획도 성립하지 않는다. 모드를
	// 밤 중간에 바꿔도 다음 판정부터 즉시 지켜져야 한다.
	if (!Tuning.bChaseEnabled && State == EIGListenerState::Chasing)
	{
		EnterState(EIGListenerState::Investigating);
	}
	if (!Tuning.bTierThreeAmbushAllowed)
	{
		bAmbushArmed = false;
	}
}

void AIGListenerEntity::SetDifficultyForTesting(
	const EIGNightDifficulty NewDifficulty)
{
	Difficulty = NewDifficulty;
	RefreshNightTuning();
}

void AIGListenerEntity::SetPatrolPoints(const TArray<FVector>& Points)
{
	PatrolPoints = Points;
	PatrolIndex = 0;
}

bool AIGListenerEntity::TryAnswerKnock(const FVector& KnockLocation)
{
	const UWorld* World = GetWorld();
	if (!World || bDormant)
	{
		return false;
	}
	// Nothing to answer while it is holding her, and the night-four authored
	// pass must not be divertible by a knock.
	if (State == EIGListenerState::CaptureHold
		|| State == EIGListenerState::FinaleLured)
	{
		return false;
	}
	// Out of earshot the taps are just taps on a wall. Checked before the tap is
	// recorded so a sequence started two floors away cannot be completed here.
	if (!CanHearAnswerFrom(KnockLocation))
	{
		return false;
	}
	// 이미 기다리는 중이면 탭은 받되 기다림을 늘리지 않는다. 같은 박자를 6초마다
	// 두드리면 밤새 얼어 있던 구멍이 여기였다.
	if (State == EIGListenerState::Waiting)
	{
		return true;
	}

	const double Now = World->GetTimeSeconds();
	if (AnswerTapTimes.Num() > 0
		&& Now - AnswerTapTimes.Last() > AnswerSequenceResetSeconds)
	{
		// Too long a gap: this is the beginning of a new attempt, not the end of
		// the old one. The player owns the silence between taps (§7 P4).
		AnswerTapTimes.Reset();
	}
	AnswerTapTimes.Add(Now);
	while (AnswerTapTimes.Num() > 3)
	{
		AnswerTapTimes.RemoveAt(0);
	}
	if (AnswerTapTimes.Num() < 3)
	{
		return true;
	}

	const double PairInterval = AnswerTapTimes[1] - AnswerTapTimes[0];
	const double RestInterval = AnswerTapTimes[2] - AnswerTapTimes[1];
	const bool bCadenceMatches =
		PairInterval >= AnswerPairMinSeconds
		&& PairInterval <= AnswerPairMaxSeconds
		&& RestInterval >= AnswerRestMinSeconds
		&& RestInterval <= AnswerRestMaxSeconds;
	AnswerTapTimes.Reset();
	if (bCadenceMatches)
	{
		NotifyAnswerKnock(KnockLocation);
	}
	// Either way the tap was a knock aimed at him, so the caller keeps it.
	return true;
}

bool AIGListenerEntity::CanHearAnswerFrom(const FVector& KnockLocation) const
{
	// The answer only reaches it within ordinary hearing of a knock-loud
	// sound; whispering the code from another floor does nothing. 험도 대답을
	// 가린다 — 냉장고 옆에서 친 노크가 그에게는 닿는데 같은 노크의 소음은
	// 험에 삼켜지던 모순을 없앤다. 소음과 대답은 같은 귀로 듣는다.
	// 그의 노크 3연이 거는 전역 마스킹(-0.3)은 뺀다. 그 창에 친 대답도 대답이다 —
	// 기계 옆의 험만 대답을 삼킨다.
	const float Masking = NoiseSubsystem
		? FMath::Max(0.0f, NoiseSubsystem->GetMaskingAt(KnockLocation) - NoiseSubsystem->GetGlobalMasking())
		: 0.0f;
	FIGNoiseEvent Probe;
	Probe.Location = KnockLocation;
	Probe.Loudness = FMath::Max(0.0f, 0.35f - Masking);
	Probe.Radius = Probe.Loudness * UIGNoiseSubsystem::CarryPerLoudness;
	if (!CanHear(Probe))
	{
		return false;
	}
	return true;
}

void AIGListenerEntity::NotifyAnswerKnock(const FVector& KnockLocation)
{
	if (!CanHearAnswerFrom(KnockLocation))
	{
		return;
	}
	// 한 밤에 세 번. 네 번째부터 대답은 오지 않고 벽의 잔향만 남는다 — 그리고
	// 그는 그 자리를 들었다.
	if (AnswersThisNight >= IGListener::AnswersPerNight)
	{
		LastHeardLocation = KnockLocation;
		bReactingToSound = true;
		if (State != EIGListenerState::Chasing)
		{
			EnterState(EIGListenerState::Investigating);
		}
		return;
	}
	++AnswersThisNight;

	AnswerKnockLocation = KnockLocation;
	EnterState(EIGListenerState::Waiting);
	// It answers: two knocks. The reply every tester should get chills from.
	IGAudio::SpawnOneShotAt(
		this,
		UIGToneSequenceSoundWave::CreateWallKnockReply(this),
		GetActorLocation(),
		0.9f,
		1.0f,
		240.0f,
		2600.0f,
		EIGAudioBus::Entity);
}

void AIGListenerEntity::SetDormant(const bool bInDormant)
{
	if (bDormant == bInDormant)
	{
		return;
	}
	bDormant = bInDormant;

	SetActorHiddenInGame(bDormant);
	SetActorEnableCollision(!bDormant);
	// Tick off is the whole dormancy: no state machine, no knocking, no
	// hearing, no threat pressure. Cheapest possible daytime.
	SetActorTickEnabled(!bDormant);

	if (bDormant)
	{
		if (DragLoopComponent)
		{
			DragLoopComponent->SetVolumeMultiplier(0.0f);
		}
		// A knock window must never outlive the knocker into the day.
		if (NoiseSubsystem)
		{
			NoiseSubsystem->SetGlobalMasking(0.0f);
			// §5.6: 밤이 끝나면 히트맵은 절반으로 감쇠한다. 어제의 습관이 오늘
			// 완전히 사라지지는 않는다 — 조용히 걷기로 바꿨더라도 어제 시끄러웠던
			// 복도는 여전히 조금 더 위험하다.
			NoiseSubsystem->DecayHeatmapForNewNight();
		}
		bAmbushArmed = false;
		AnswersThisNight = 0;
		if (UWorld* World = GetWorld())
		{
			if (UIGMissingFloorAudioSubsystem* AudioDirector =
				World->GetSubsystem<UIGMissingFloorAudioSubsystem>())
			{
				AudioDirector->SetThreatState(EIGAudioThreatState::Calm);
				AudioDirector->SetEntityDistance(MAX_flt);
			}
		}
	}
	else
	{
		// Wake at the route start, impatience preserved.
		ResetToPatrolStart(/*bRaiseAggression=*/false);
	}
}

void AIGListenerEntity::BeginFinalePass(
	const FVector& StartLocation,
	const TArray<FVector>& RoutePoints)
{
	if (RoutePoints.Num() == 0)
	{
		return;
	}

	// Do not route through SetDormant(false): normal waking deliberately resets
	// to the patrol start, while this one authored beat begins behind Mok.
	bDormant = false;
	SetActorHiddenInGame(false);
	SetActorEnableCollision(false);
	SetActorTickEnabled(true);
	TeleportTo(StartLocation, GetActorRotation(), false, true);
	FinaleRoutePoints = RoutePoints;
	FinaleRouteIndex = 0;
	bReactingToSound = false;
	CachedPlayer.Reset();
	EnterState(EIGListenerState::FinaleLured);
}

void AIGListenerEntity::ParkForBeat(const FVector& Where, const float Yaw)
{
	TeleportTo(Where, FRotator(0.0f, Yaw, 0.0f), false, true);
	SetActorEnableCollision(true);
	PatrolIndex = 0;
	// 이 한 줄이 카메오를 성립시킨다. 남겨 두면 그는 조사 중인 상태로 서 있다가
	// 아까 들은 자리로 기어간다.
	bReactingToSound = false;
	bAmbushArmed = false;
	AnswerTapTimes.Reset();
	EnterState(EIGListenerState::Patrolling);
}

void AIGListenerEntity::ResetToPatrolStart(const bool bRaiseAggression)
{
	if (bRaiseAggression)
	{
		SetAggressionTier(AggressionTier + 1);
	}
	TeleportTo(SpawnLocation, GetActorRotation(), false, true);
	SetActorEnableCollision(true);
	PatrolIndex = 0;
	FinaleRoutePoints.Reset();
	FinaleRouteIndex = 0;
	bReactingToSound = false;
	bAmbushArmed = false;
	// The hour restarts at 04:30, so the air restarts with it (§5.4). Leaving
	// the lane behind would let a reset player read a path nobody walked.
	bDustTrailSeeded = false;
	if (UWorld* World = GetWorld())
	{
		if (UIGDustSubsystem* Dust = World->GetSubsystem<UIGDustSubsystem>())
		{
			Dust->ClearDisturbances();
			// The floor goes back to 04:30 too. A swept fifth floor is how the
			// player knows the hour really did restart, and leaving last loop's
			// tracks would have them searching where they have not been.
			Dust->ClearSettledPrints();
		}
	}
	// §5.6: the habit survives the reset. He does not forget where you have been
	// loud just because the clock went back to half past four — that memory is
	// the whole point of the heatmap, and it only halves when a night ends.
	RefreshNightTuning();
	EnterState(EIGListenerState::Patrolling);
}

// -- locomotion ------------------------------------------------------------

bool AIGListenerEntity::CrawlTowards(
	const FVector& Target,
	const float Speed,
	const float DeltaSeconds)
{
	FVector ToTarget = Target - GetActorLocation();
	// Crawling cannot climb: steer on the floor plane and let collisions
	// keep it honest about stairs it has not been routed through.
	ToTarget.Z = 0.0f;
	const float Distance = ToTarget.Size();
	if (Distance <= 24.0f)
	{
		return true;
	}

	const FVector Direction = ToTarget / Distance;
	const FVector Step =
		Direction * FMath::Min(Speed * DeltaSeconds, Distance);
	FaceDirection(Direction, DeltaSeconds);

	const FVector Before = GetActorLocation();
	AddActorWorldOffset(Step, true);
	const float Moved =
		FVector::Dist2D(Before, GetActorLocation());
	LastMoveSpeed = DeltaSeconds > 0.0f ? Moved / DeltaSeconds : 0.0f;

	// Wedged against geometry: treat the spot as reached instead of
	// grinding on a wall forever. The search sweep will wander it free.
	if (Moved < Speed * DeltaSeconds * 0.15f)
	{
		StuckSeconds += DeltaSeconds;
		if (StuckSeconds >= 1.5f)
		{
			StuckSeconds = 0.0f;
			return true;
		}
	}
	else
	{
		StuckSeconds = 0.0f;
	}
	return false;
}

void AIGListenerEntity::AdvancePatrolIndex()
{
	const int32 NodeCount = PatrolPoints.Num();
	if (NodeCount <= 0)
	{
		PatrolIndex = 0;
		return;
	}

	const int32 NextIndex = (PatrolIndex + 1) % NodeCount;
	const UWorld* World = GetWorld();
	UIGNoiseSubsystem* Noise = NoiseSubsystem;
	if (Tuning.HeatmapWeight <= 0.0f || !Noise || !World || NodeCount < 3)
	{
		// 밤1은 히트맵 가중이 0이다. 첫 밤의 순찰은 배울 수 있는 순서여야 한다.
		PatrolIndex = NextIndex;
		return;
	}

	// The route order is the baseline and the heatmap is a bias on top: the next
	// stop keeps a head start so patrols still read as a round, and a stop only
	// jumps the queue when the player has genuinely been loud near it.
	int32 BestIndex = NextIndex;
	float BestScore = 1.0f;
	for (int32 Offset = 0; Offset < NodeCount; ++Offset)
	{
		const int32 Candidate = (PatrolIndex + 1 + Offset) % NodeCount;
		if (Candidate == PatrolIndex)
		{
			// Standing still is not a patrol.
			continue;
		}
		const float Heat = Noise->GetHeatAt(PatrolPoints[Candidate]);
		// The in-order stop starts at 1.0; anything else has to out-argue it.
		const float Score = (Candidate == NextIndex ? 1.0f : 0.0f)
			+ Heat * Tuning.HeatmapWeight * 2.0f;
		if (Score > BestScore)
		{
			BestScore = Score;
			BestIndex = Candidate;
		}
	}
	PatrolIndex = BestIndex;
}

bool AIGListenerEntity::TryBeginAmbush()
{
	if (!Tuning.bTierThreeAmbushAllowed || AggressionTier < 3 || bAmbushArmed)
	{
		return false;
	}
	UIGNoiseSubsystem* Noise = NoiseSubsystem;
	if (!Noise)
	{
		return false;
	}
	FVector Hottest = FVector::ZeroVector;
	float Heat = 0.0f;
	if (!Noise->GetHottestZone(Hottest, Heat) || Heat < 0.5f)
	{
		// Without a habit there is nothing to lie in wait for, and guessing
		// would make the ambush feel arbitrary instead of earned.
		return false;
	}

	// Keep his own floor: the hottest zone is a statistic, and dragging himself
	// through a slab to reach it is not something the state machine can do.
	AmbushLocation = FVector(Hottest.X, Hottest.Y, GetActorLocation().Z);
	bAmbushArmed = true;
	// Investigating walks him there; arriving hands over to Holding, which is
	// silent. No knock cycle announces this one.
	LastHeardLocation = AmbushLocation;
	bReactingToSound = false;
	EnterState(EIGListenerState::Investigating);
	return true;
}

void AIGListenerEntity::FaceDirection(
	const FVector& Direction,
	const float DeltaSeconds)
{
	if (Direction.IsNearlyZero())
	{
		return;
	}
	const FRotator Current = GetActorRotation();
	const FRotator Desired = Direction.Rotation();
	const FRotator Next(
		0.0f,
		FMath::FixedTurn(Current.Yaw, Desired.Yaw, 160.0f * DeltaSeconds),
		0.0f);
	SetActorRotation(Next);
}

const FVector* AIGListenerEntity::CurrentPatrolTarget() const
{
	if (PatrolPoints.Num() == 0)
	{
		return nullptr;
	}
	return &PatrolPoints[PatrolIndex % PatrolPoints.Num()];
}

// -- presentation ----------------------------------------------------------

void AIGListenerEntity::BuildGreyboxBody()
{
	// The release path is one authored static crawl pose built from the
	// ImageGen anatomy sheet. It keeps contact shadow, flashlight parallax and
	// a continuous human silhouette; the primitive assembly below is a safe
	// editor/source-only fallback while generated assets are unavailable.
	if (UStaticMesh* ListenerMesh = LoadObject<UStaticMesh>(
			nullptr,
			TEXT("/Game/Meshes/SM_ListenerEntityCrawl.SM_ListenerEntityCrawl")))
	{
		UStaticMeshComponent* Component = NewObject<UStaticMeshComponent>(
			this, TEXT("ListenerBody"));
		Component->RegisterComponent();
		Component->AttachToComponent(
			Body, FAttachmentTransformRules::KeepRelativeTransform);
		Component->SetStaticMesh(ListenerMesh);
		// The capsule origin is 58 cm above the floor. 접지 오프셋은 메시의
		// 가장 낮은 점에서 계산한다 — 예전 절차 셸은 발끝이 Z -31이라 -27을
		// 박아 두었는데, TRELLIS.2에서 다듬은 셸은 원점이 바닥 중심이라 그
		// 값을 그대로 쓰면 58 cm 떠 버린다.
		constexpr float CapsuleOriginAboveFloor = 58.0f;
		const FBoxSphereBounds MeshBounds = ListenerMesh->GetBounds();
		const float LowestZ = MeshBounds.Origin.Z - MeshBounds.BoxExtent.Z;
		Component->SetRelativeLocation(
			FVector(0.0f, 0.0f, -CapsuleOriginAboveFloor - LowestZ));
		Component->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Component->SetCanEverAffectNavigation(false);
		Component->SetCastShadow(true);
		if (UMaterialInterface* PlasterMaterial = LoadObject<UMaterialInterface>(
				nullptr,
				TEXT("/Game/Prototype/Materials/"
					 "M_MissingFloorListenerPlasterUV."
					 "M_MissingFloorListenerPlasterUV")))
		{
			// 석고도 숨을 쉰다. WPO 진폭 파라미터(BreathAmplitude,
			// TremorAmplitude)를 상태 머신이 조종할 수 있게 셸만 MID로 감싼다.
			ListenerShellMid =
				UMaterialInstanceDynamic::Create(PlasterMaterial, Component);
			Component->SetMaterial(
				0,
				ListenerShellMid
					? static_cast<UMaterialInterface*>(ListenerShellMid)
					: PlasterMaterial);
		}
		ListenerShell = Component;
		BodyBlocks.Add(Component);

		// The chase is staged head-on in a long, narrow corridor. Preserve the
		// human anatomy from the approved ImageGen front view with one lit,
		// masked PBR layer at that authored angle. The continuous 3D shell stays
		// hidden-but-shadow-casting underneath, so the card never creates a flat
		// rectangular shadow and the floor contact remains physically grounded.
		UStaticMesh* PlaneMesh = LoadObject<UStaticMesh>(
			nullptr, TEXT("/Engine/BasicShapes/Plane.Plane"));
		UMaterialInterface* FrontMaterial = LoadObject<UMaterialInterface>(
			nullptr,
			TEXT("/Game/Prototype/Materials/M_SpriteListenerFront."
				 "M_SpriteListenerFront"));
		static const TCHAR* PhaseMaterialPaths[] = {
			TEXT("/Game/Prototype/Materials/M_SpriteListenerCrawl0."
				 "M_SpriteListenerCrawl0"),
			TEXT("/Game/Prototype/Materials/M_SpriteListenerCrawl1."
				 "M_SpriteListenerCrawl1"),
			TEXT("/Game/Prototype/Materials/M_SpriteListenerCrawl2."
				 "M_SpriteListenerCrawl2"),
			TEXT("/Game/Prototype/Materials/M_SpriteListenerCrawl3."
				 "M_SpriteListenerCrawl3"),
		};
		for (const TCHAR* PhasePath : PhaseMaterialPaths)
		{
			if (UMaterialInterface* PhaseMaterial =
				LoadObject<UMaterialInterface>(nullptr, PhasePath))
			{
				ListenerPhaseMaterials.Add(PhaseMaterial);
			}
		}
		if (ListenerPhaseMaterials.Num() != UE_ARRAY_COUNT(PhaseMaterialPaths))
		{
			// Never advance a partial sequence: one absent phase would make the
			// identity and lighting flash. The approved still is a safe fallback.
			ListenerPhaseMaterials.Reset();
		}
		UMaterialInterface* CardMaterial =
			ListenerPhaseMaterials.Num() > 0
				? ListenerPhaseMaterials[0].Get()
				: FrontMaterial;
		if (PlaneMesh && CardMaterial)
		{
			UStaticMeshComponent* FrontCard = NewObject<UStaticMeshComponent>(
				this, TEXT("ListenerFrontCard"));
			FrontCard->RegisterComponent();
			FrontCard->AttachToComponent(
				Body, FAttachmentTransformRules::KeepRelativeTransform);
			FrontCard->SetStaticMesh(PlaneMesh);
			FrontCard->SetMaterial(0, CardMaterial);
			// Roll makes texture V vertical; +90 yaw makes the plane normal
			// follow the pawn's +X forward axis. At 85 cm card height the crop's
			// lower 9% margin resolves to the floor at this exact center height.
			// A 128 cm width stays inside the 4F corridor instead of clipping
			// through a dwelling wall when the patrol line hugs one side.
			FrontCard->SetRelativeLocation(FVector(38.0f, 0.0f, -23.0f));
			FrontCard->SetRelativeRotation(FRotator(0.0f, 90.0f, 90.0f));
			FrontCard->SetRelativeScale3D(FVector(1.28f, 0.85f, 1.0f));
			FrontCard->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			FrontCard->SetCanEverAffectNavigation(false);
			FrontCard->SetCastShadow(false);
			FrontCard->SetHiddenInGame(true);
			ListenerFrontCard = FrontCard;
			BodyBlocks.Add(FrontCard);
		}
		return;
	}

	UStaticMesh* Cube =
		LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
	UStaticMesh* Sphere =
		LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	if (!Cube || !Sphere)
	{
		return;
	}

	struct FBlock
	{
		UStaticMesh* Mesh = nullptr;
		FVector Location = FVector::ZeroVector;
		FRotator Rotation = FRotator::ZeroRotator;
		FVector Scale = FVector::OneVector;
	};

	// Upper body raised on its elbows, faceless head, legs trailing flat.
	// Proportions matter more than detail: the silhouette must read as a
	// person the moment a flashlight edge clips it.
	const FBlock Blocks[] =
	{
		{Cube, FVector(6.0f, 0.0f, -8.0f), FRotator(-16.0f, 0.0f, 0.0f), FVector(0.52f, 0.40f, 0.30f)},
		{Sphere, FVector(34.0f, 0.0f, 14.0f), FRotator::ZeroRotator, FVector(0.20f, 0.22f, 0.26f)},
		{Cube, FVector(24.0f, -20.0f, -22.0f), FRotator(24.0f, 8.0f, 0.0f), FVector(0.42f, 0.10f, 0.10f)},
		{Cube, FVector(24.0f, 20.0f, -22.0f), FRotator(24.0f, -8.0f, 0.0f), FVector(0.42f, 0.10f, 0.10f)},
		{Cube, FVector(-38.0f, -9.0f, -34.0f), FRotator(-3.0f, 4.0f, 0.0f), FVector(0.58f, 0.11f, 0.09f)},
		{Cube, FVector(-38.0f, 9.0f, -34.0f), FRotator(-3.0f, -4.0f, 0.0f), FVector(0.58f, 0.11f, 0.09f)},
	};

	int32 BlockIndex = 0;
	for (const FBlock& Block : Blocks)
	{
		UStaticMeshComponent* Component = NewObject<UStaticMeshComponent>(
			this,
			*FString::Printf(TEXT("ListenerBlock%d"), BlockIndex++));
		Component->RegisterComponent();
		Component->AttachToComponent(
			Body, FAttachmentTransformRules::KeepRelativeTransform);
		Component->SetStaticMesh(Block.Mesh);
		Component->SetRelativeLocation(Block.Location);
		Component->SetRelativeRotation(Block.Rotation);
		Component->SetRelativeScale3D(Block.Scale);
		Component->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Component->SetCanEverAffectNavigation(false);

		// Source-only fallback: bone-grey and fully rough under a flashlight.
		if (UMaterialInterface* BaseMaterial = Component->GetMaterial(0))
		{
			UMaterialInstanceDynamic* Mid =
				UMaterialInstanceDynamic::Create(BaseMaterial, Component);
			Mid->SetVectorParameterValue(
				TEXT("Color"), FLinearColor(0.62f, 0.60f, 0.56f));
			Component->SetMaterial(0, Mid);
		}
		BodyBlocks.Add(Component);
	}
}

void AIGListenerEntity::UpdatePresentationLayer()
{
	if (!ListenerShell || !ListenerFrontCard)
	{
		return;
	}

	if (!CachedPlayer.IsValid())
	{
		for (TActorIterator<AIGPlayerCharacter> It(GetWorld()); It; ++It)
		{
			CachedPlayer = *It;
			break;
		}
	}

	const APawn* Player = CachedPlayer.Get();
	bool bUseFrontCard = false;
	if (Player)
	{
		FVector ToPlayer = Player->GetActorLocation() - GetActorLocation();
		ToPlayer.Z = 0.0f;
		const float Distance = ToPlayer.Size();
		const float Facing = Distance > KINDA_SMALL_NUMBER
			? FVector::DotProduct(GetActorForwardVector(), ToPlayer / Distance)
			: 1.0f;

		// Hysteresis prevents one-frame popping at either threshold. The PBR
		// layer remains over the grounded 3D shadow shell through the narrow
		// head-on chase, then yields before the 110 cm capture boundary or as
		// soon as the player gets a readable side angle.
		if (bFrontCardActive)
		{
			bUseFrontCard = Distance > 125.0f && Facing > 0.35f;
		}
		else
		{
			bUseFrontCard = Distance > 160.0f && Facing > 0.60f;
		}
	}

	if (bUseFrontCard == bFrontCardActive)
	{
		return;
	}
	bFrontCardActive = bUseFrontCard;
	ListenerFrontCard->SetHiddenInGame(!bFrontCardActive);
	ListenerShell->SetHiddenInGame(bFrontCardActive);
	ListenerShell->SetCastHiddenShadow(bFrontCardActive);
}

void AIGListenerEntity::UpdatePresentationPose(
	const float CurrentSpeed,
	const float DeltaSeconds)
{
	if (!ListenerShell || !ListenerFrontCard)
	{
		return;
	}

	// Smooth speed before it controls cadence. A single blocked sweep must not
	// snap a crawling shoulder from full extension straight into an idle pose.
	PresentationSpeed = State == EIGListenerState::Waiting
		? 0.0f
		: FMath::FInterpTo(
			PresentationSpeed, CurrentSpeed, DeltaSeconds, 7.5f);
	const float SpeedAlpha = FMath::Clamp(
		PresentationSpeed / FMath::Max(ChaseSpeed, 1.0f), 0.0f, 1.0f);
	if (SpeedAlpha > 0.01f)
	{
		const float FramesPerSecond = FMath::Lerp(1.6f, 6.0f, SpeedAlpha);
		ListenerPhase = FMath::Fmod(
			ListenerPhase + DeltaSeconds * FramesPerSecond, 4.0f);
		// 팔꿈치가 바닥을 치는 자세(0과 2)마다 한 걸음. 끌림 루프는 속도만 말하고
		// 걸음은 박자를 말한다. 변주는 걸음 번호 해시의 피치로.
		const int32 StepIndex = FMath::FloorToInt(ListenerPhase * 0.5f);
		if (StepIndex != LastCrawlStepIndex && SpeedAlpha > 0.08f && !bDormant)
		{
			LastCrawlStepIndex = StepIndex;
			const uint32 StepHash = static_cast<uint32>(StateSeconds * 37.0f + ListenerPhase * 1000.0f) * 2654435761u;
			const float Pitch = 0.94f + 0.12f * ((StepHash >> 8) & 0xFF) / 255.0f;
			IGAudio::SpawnOneShotAt(
				this,
				UIGToneSequenceSoundWave::CreateEntityCrawlStep(this, bDragSurfaceIsVinyl),
				GetActorLocation() + FVector(20.0f, 0.0f, -40.0f),
				0.35f + 0.55f * SpeedAlpha,
				Pitch,
				200.0f,
				2200.0f,
				EIGAudioBus::Entity);
		}
	}

	if (ListenerPhaseMaterials.Num() == 4)
	{
		const int32 PhaseIndex =
			FMath::Clamp(FMath::FloorToInt(ListenerPhase), 0, 3);
		if (PhaseIndex != ListenerPhaseIndex)
		{
			ListenerFrontCard->SetMaterial(
				0, ListenerPhaseMaterials[PhaseIndex]);
			ListenerPhaseIndex = PhaseIndex;
		}
	}

	// 생체 신호 진폭. 흉곽 팽창과 잔떨림 자체는 셸 재질의 WPO가 만들고,
	// 여기서는 상태에 맞는 진폭만 정한다. 들을 때는 저도 숨을 죽이고, 쫓을
	// 때는 거칠어지고, 대답 노크에 얼어붙은 Waiting은 완전한 정지다 — 배운
	// 답이 통했다는 확인을 몸으로 보여 준다.
	float TargetBreath = 0.45f;
	float TargetTremor = 0.1f;
	switch (State)
	{
	case EIGListenerState::Waiting:
		TargetBreath = 0.0f;
		TargetTremor = 0.0f;
		break;
	case EIGListenerState::Listening:
	case EIGListenerState::Holding:
		TargetBreath = 0.16f;
		TargetTremor = 0.05f;
		break;
	case EIGListenerState::Chasing:
		TargetBreath = 0.8f;
		TargetTremor = 0.3f;
		break;
	default:
		break;
	}
	// 얼어붙는 쪽은 사람이 숨을 삼키는 속도, 풀리는 쪽은 한 호흡.
	const float InterpSpeed =
		State == EIGListenerState::Waiting ? 9.0f : 3.0f;
	ShellBreathAmplitude = FMath::FInterpTo(
		ShellBreathAmplitude, TargetBreath, DeltaSeconds, InterpSpeed);
	ShellTremorAmplitude = FMath::FInterpTo(
		ShellTremorAmplitude, TargetTremor, DeltaSeconds, InterpSpeed);
	if (ListenerShellMid)
	{
		ListenerShellMid->SetScalarParameterValue(
			TEXT("BreathAmplitude"), ShellBreathAmplitude);
		ListenerShellMid->SetScalarParameterValue(
			TEXT("TremorAmplitude"), ShellTremorAmplitude);
	}

	// The source poses supply elbow/leg changes. These sub-centimetre motions
	// blend their weight across frames without lifting the crop from the floor.
	// 카드의 숨 바운스도 같은 진폭을 따른다: 셸이 멎는데 카드만 계속
	// 오르내리면 1.6m 경계에서 정지의 의미가 새어 버린다.
	const float TimeSeconds = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
	const float BreathScale = ShellBreathAmplitude / 0.45f;
	const float Breath = FMath::Sin(TimeSeconds * 1.35f) * BreathScale;
	const float Stride = FMath::Sin(ListenerPhase * HALF_PI);
	const float WeightShift = Stride * SpeedAlpha;
	ListenerFrontCard->SetRelativeLocation(FVector(
		38.0f,
		WeightShift * 1.6f,
		-23.0f + Breath * 0.18f));
	ListenerFrontCard->SetRelativeRotation(FRotator(
		0.0f,
		90.0f + WeightShift * 0.6f,
		90.0f + WeightShift * 0.9f));
	ListenerShell->SetRelativeLocation(FVector(
		0.0f, WeightShift * 0.55f, -27.0f));
	ListenerShell->SetRelativeRotation(FRotator(
		0.0f, WeightShift * 0.3f, 0.0f));
}

void AIGListenerEntity::PlayKnockTriple()
{
	IGAudio::SpawnOneShotAt(
		this,
		UIGToneSequenceSoundWave::CreateWallKnockTriple(this, 0.0f),
		GetActorLocation() + FVector(0.0f, 0.0f, 40.0f),
		1.0f,
		1.0f,
		300.0f,
		3600.0f,
		EIGAudioBus::Entity);
}

void AIGListenerEntity::PlayPlasterSettle()
{
	IGAudio::SpawnOneShotAt(
		this,
		UIGToneSequenceSoundWave::CreatePlasterSettle(this),
		GetActorLocation(),
		0.6f,
		1.0f,
		160.0f,
		1200.0f,
		EIGAudioBus::Entity);
}

void AIGListenerEntity::UpdateDragLoop(const float CurrentSpeed)
{
	if (!DragLoopComponent)
	{
		return;
	}
	// Louder and faster the harder it pulls itself. At rest: true silence,
	// which is the scariest volume it has.
	const float SpeedRatio =
		ChaseSpeed > 0.0f ? FMath::Clamp(CurrentSpeed / ChaseSpeed, 0.0f, 1.0f) : 0.0f;
	DragLoopComponent->SetVolumeMultiplier(SpeedRatio * 0.9f);
	DragLoopComponent->SetPitchMultiplier(0.85f + 0.45f * SpeedRatio);
}

void AIGListenerEntity::RefreshDragSurface()
{
	const UWorld* World = GetWorld();
	if (!World || !DragLoopComponent)
	{
		return;
	}

	const FVector Origin = GetActorLocation();
	FHitResult Hit;
	FCollisionQueryParams QueryParams(
		SCENE_QUERY_STAT(IGListenerDragSurface),
		false,
		this);
	bool bVinyl = false;
	if (World->LineTraceSingleByChannel(
		Hit,
		Origin + FVector(0.0f, 0.0f, 12.0f),
		Origin - FVector(0.0f, 0.0f, 120.0f),
		ECC_Visibility,
		QueryParams))
	{
		if (const UPrimitiveComponent* Component = Hit.GetComponent())
		{
			bVinyl = Component->ComponentHasTag(IGListener::VinylSurfaceTag);
		}
	}
	if (bVinyl == bDragSurfaceIsVinyl)
	{
		return;
	}
	bDragSurfaceIsVinyl = bVinyl;

	// Swapping the wave mid-loop restarts the crawl cycle, which is the right
	// seam: the palm plant that lands on the new floor is the one that sounds
	// different. Volume is left to UpdateDragLoop so a stopped entity stays
	// silent through the change.
	DragLoopComponent->SetSound(
		UIGToneSequenceSoundWave::CreateEntityDragLoop(this, bVinyl));
	DragLoopComponent->Play();
}

void AIGListenerEntity::ReportDustTrail()
{
	// Asleep in the day, and still while he knocks or listens: dust rises from
	// the drag, so a stationary body raises none.
	if (bDormant || LastMoveSpeed <= 1.0f)
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}
	if (!DustSubsystem)
	{
		DustSubsystem = World->GetSubsystem<UIGDustSubsystem>();
		if (!DustSubsystem)
		{
			return;
		}
	}

	// He crawls, so the plaster comes off low. 40 cm above the floor is where a
	// torch held at chest height cuts through it.
	const FVector DragHeight = GetActorLocation() - FVector(0.0f, 0.0f, 18.0f);
	const float MovedCentimeters = bDustTrailSeeded
		? static_cast<float>(FVector::Dist(LastDustReportLocation, DragHeight))
		: 0.0f;
	if (bDustTrailSeeded && MovedCentimeters < UIGDustSubsystem::MergeDistance)
	{
		return;
	}

	// A burst scrapes more wall than a patrol crawl does, so a chase leaves the
	// brightest lane — the one the player most needs to read afterwards.
	const float DragStrength = FMath::Clamp(LastMoveSpeed / 160.0f, 0.45f, 1.0f);
	DustSubsystem->ReportDisturbance(DragHeight, DragStrength);
	// §11 V2: the same pass also presses the settled dust. His mark is a smear
	// across the direction of travel, not a footprint — and it is the one thing
	// on the fifth floor that says something came through here on its elbows.
	DustSubsystem->ReportSettledPrint(
		FVector(DragHeight.X, DragHeight.Y, DragHeight.Z),
		GetActorRotation().Yaw,
		EIGDustPrintKind::Drag);

	// One audible sift every few meters, never every sample: a continuous hiss
	// would sit on top of the crawl bed and stop being information. The cue is
	// the invitation — you hear powder fall somewhere, you raise the torch, and
	// the lane tells you which way he went.
	DustSiftCentimeters += MovedCentimeters;
	if (DustSiftCentimeters >= IGListener::DustSiftIntervalCentimeters)
	{
		DustSiftCentimeters = 0.0f;
		const UIGMissingFloorAudioSubsystem* AudioDirector =
			World->GetSubsystem<UIGMissingFloorAudioSubsystem>();
		// The two authored silences are holes in the mix, not quiet passages.
		// Nothing crawls into them, and he is holding still through both anyway.
		if (!AudioDirector || !AudioDirector->IsAuthoredSilence())
		{
			IGAudio::SpawnOneShotAt(
				this,
				UIGToneSequenceSoundWave::CreatePlasterDustFall(this),
				DragHeight + FVector(0.0f, 0.0f, 24.0f),
				IGListener::DustSiftVolume,
				1.0f,
				190.0f,
				2200.0f,
				EIGAudioBus::Entity);
		}
	}

	LastDustReportLocation = DragHeight;
	bDustTrailSeeded = true;
}

void AIGListenerEntity::UpdateThreatPressure()
{
	const AIGPlayerCharacter* Player =
		Cast<AIGPlayerCharacter>(CachedPlayer.Get());
	if (!Player)
	{
		return;
	}
	UIGStressComponent* Stress = Player->GetStress();
	if (!Stress)
	{
		return;
	}

	float Pressure = 0.0f;
	const float Distance =
		FVector::Dist(Player->GetActorLocation(), GetActorLocation());
	if (UWorld* World = GetWorld())
	{
		if (UIGMissingFloorAudioSubsystem* AudioDirector =
			World->GetSubsystem<UIGMissingFloorAudioSubsystem>())
		{
			AudioDirector->SetEntityDistance(Distance);
		}
	}
	switch (State)
	{
	case EIGListenerState::Chasing:
		Pressure = 0.85f;
		break;
	case EIGListenerState::Investigating:
	case EIGListenerState::Holding:
		Pressure = 0.45f * FMath::Clamp(1.0f - Distance / 1200.0f, 0.0f, 1.0f);
		break;
	case EIGListenerState::Searching:
		Pressure = 0.25f * FMath::Clamp(1.0f - Distance / 900.0f, 0.0f, 1.0f);
		break;
	case EIGListenerState::Listening:
	case EIGListenerState::Banging:
		// 순찰 중이라도 6m 안에서 두드리고 듣는 그는 압박이다. 예전엔 코앞에서
		// 노크 3연을 쳐도 심장이 잠잠했다.
		Pressure = 0.35f * FMath::Clamp(1.0f - Distance / 600.0f, 0.0f, 1.0f);
		break;
	default:
		break;
	}
	if (Pressure > 0.0f)
	{
		// The component decays pressure on its own; only pushes are sent.
		Stress->SetThreatPressure(Pressure);
	}
	UpdateBreathLoop(Distance);
	TryCloseCallStinger(Player, Distance);
}

void AIGListenerEntity::UpdateBreathLoop(const float Distance)
{
	if (!BreathLoopComponent)
	{
		return;
	}
	float Target = 0.0f;
	if (!bDormant)
	{
		switch (State)
		{
		case EIGListenerState::Chasing: Target = 0.85f; break;
		case EIGListenerState::Investigating: Target = 0.45f; break;
		case EIGListenerState::Holding:
		case EIGListenerState::Searching: Target = 0.34f; break;
		case EIGListenerState::Listening: Target = 0.26f; break;
		case EIGListenerState::Banging:
		case EIGListenerState::Patrolling: Target = 0.20f; break;
		case EIGListenerState::Waiting: Target = 0.10f; break;
		default: Target = 0.0f; break;
		}
	}
	if (!FMath::IsNearlyEqual(Target, BreathVolumeTarget, 0.02f))
	{
		BreathVolumeTarget = Target;
		BreathLoopComponent->AdjustVolume(0.6f, Target);
	}
	// 추격 중엔 숨이 빠르다. 루프 속도는 못 바꾸니 피치로.
	BreathLoopComponent->SetPitchMultiplier(State == EIGListenerState::Chasing ? 1.22f : 1.0f);
}

void AIGListenerEntity::TryCloseCallStinger(const AIGPlayerCharacter* Player, const float Distance)
{
	// 코앞에서 마주쳤다: 3.2m 안, 시야 안, 그가 깨어서 움직이거나 듣는 중. 25초에
	// 한 번. 「이미 본 형상의 위치 변화」(STORY_DIRECTION §7)를 소리로 찍는다.
	UWorld* World = GetWorld();
	if (!World || !Player || bDormant || Distance > 320.0f)
	{
		return;
	}
	if (State == EIGListenerState::CaptureHold
		|| State == EIGListenerState::FinaleLured
		|| State == EIGListenerState::Waiting)
	{
		return;
	}
	const double Now = World->GetTimeSeconds();
	if (Now - LastCloseCallSeconds < 25.0)
	{
		return;
	}
	FVector ToHim = GetActorLocation() - Player->GetActorLocation();
	ToHim.Z = 0.0f;
	const FVector View = Player->GetControlRotation().Vector().GetSafeNormal2D();
	if (FVector::DotProduct(View, ToHim.GetSafeNormal()) < 0.55f)
	{
		return;
	}
	LastCloseCallSeconds = Now;
	if (UIGMissingFloorAudioSubsystem* AudioDirector =
		World->GetSubsystem<UIGMissingFloorAudioSubsystem>())
	{
		AudioDirector->PlayStinger(EIGStinger::CloseCall, GetActorLocation() + FVector(0.0f, 0.0f, 40.0f));
	}
	if (UIGStressComponent* Stress = Player->GetStress())
	{
		Stress->ApplyScare(0.45f);
	}
	const_cast<AIGPlayerCharacter*>(Player)->PlayScareKick(1.4f);
}

void AIGListenerEntity::BeginCapture(APawn* Player)
{
	if (State == EIGListenerState::CaptureHold)
	{
		return;
	}
	EnterState(EIGListenerState::CaptureHold);

	if (AIGPlayerCharacter* Character = Cast<AIGPlayerCharacter>(Player))
	{
		if (UIGStressComponent* Stress = Character->GetStress())
		{
			Stress->ApplyScare(1.0f);
		}
	}

	// 덮치는 순간의 저역과 천 스침. 그 뒤가 「노크 둘」이다 — 포효는 아니지만
	// 붙잡히는 것이 아무 소리 없이 지나가서도 안 된다.
	if (UWorld* World = GetWorld())
	{
		if (UIGMissingFloorAudioSubsystem* AudioDirector =
			World->GetSubsystem<UIGMissingFloorAudioSubsystem>())
		{
			AudioDirector->PlayStinger(
				EIGStinger::Capture,
				Player ? Player->GetActorLocation() : GetActorLocation());
		}
	}

	// Not a roar: the calmed reply, from very close. Then the director's
	// blackout and the half-past-four bed.
	//
	// The one dry cue in the building (§21.3). Every other knock all night has
	// arrived wearing the corridor or the stairwell, and by now the player reads
	// that wetness as distance without being told. Taking it to zero is the
	// sentence: he is not down the hall any more.
	IGAudio::SpawnDryOneShotAt(
		this,
		UIGToneSequenceSoundWave::CreateWallKnockReply(this),
		Player ? Player->GetActorLocation() : GetActorLocation(),
		1.0f,
		1.0f,
		120.0f,
		600.0f,
		EIGAudioBus::Entity);

	OnPlayerCaptured.Broadcast(Player);
}
