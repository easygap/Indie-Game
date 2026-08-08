#include "Entity/IGListenerEntity.h"

#include "Audio/IGAudioHelpers.h"
#include "Audio/IGToneSequenceSoundWave.h"
#include "Components/AudioComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Player/IGPlayerCharacter.h"
#include "Player/IGStressComponent.h"

namespace IGListener
{
	// Timing of the idle cycle. The triple knock is the player's masked
	// window to move; keep these in step with CreateWallKnockTriple.
	constexpr float BangSeconds = 2.1f;
	constexpr float BangMasking = 0.3f;
	constexpr float HoldSeconds = 6.0f;
	constexpr float SearchSeconds = 12.0f;
	constexpr float SearchRadius = 350.0f;
	constexpr float ChaseGiveUpSeconds = 8.0f;
	constexpr float CaptureHoldSeconds = 2.5f;
	/** Hearing bonus while deliberately listening. */
	constexpr float ListeningGain = 2.0f;
	constexpr float HoldingGain = 1.5f;
	/** A second sound within this window of the first confirms the prey. */
	constexpr float ReactionMemorySeconds = 10.0f;
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
		DragLoopComponent->AttenuationSettings =
			IGAudio::MakeAttenuation(this, 220.0f, 2400.0f);
		DragLoopComponent->bAllowSpatialization = true;
		DragLoopComponent->SetVolumeMultiplier(0.0f);
		DragLoopComponent->Play();
	}

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
	UpdateDragLoop(LastMoveSpeed);
	UpdateThreatPressure();
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

	State = NewState;
	StateSeconds = 0.0f;
	StuckSeconds = 0.0f;

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
			PatrolIndex = PatrolPoints.Num() > 0
				? (PatrolIndex + 1) % PatrolPoints.Num()
				: 0;
			EnterState(EIGListenerState::Patrolling);
		}
		break;

	case EIGListenerState::Investigating:
		if (CrawlTowards(LastHeardLocation, InvestigateSpeed, DeltaSeconds))
		{
			EnterState(EIGListenerState::Holding);
		}
		break;

	case EIGListenerState::Holding:
		if (StateSeconds >= IGListener::HoldSeconds)
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
	}

	// Touching the player ends the night from any moving pursuit state.
	if (State == EIGListenerState::Investigating
		|| State == EIGListenerState::Chasing
		|| State == EIGListenerState::Searching)
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
			if (Distance <= CaptureRadius)
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
		|| State == EIGListenerState::CaptureHold)
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

	if (bSecondSound)
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
	return Distance <= Event.Radius * HearingMultiplier();
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
	static constexpr float Seconds[4] = {8.0f, 6.0f, 5.0f, 4.0f};
	return Seconds[FMath::Clamp(AggressionTier, 0, 3)];
}

float AIGListenerEntity::WaitSecondsForTier() const
{
	// Hope wears out: each reset it waits less on an answer.
	static constexpr float Seconds[4] = {20.0f, 12.0f, 6.0f, 6.0f};
	return Seconds[FMath::Clamp(AggressionTier, 0, 3)];
}

// -- public controls -------------------------------------------------------

void AIGListenerEntity::SetAggressionTier(const int32 Tier)
{
	AggressionTier = FMath::Clamp(Tier, 0, 3);
}

void AIGListenerEntity::SetPatrolPoints(const TArray<FVector>& Points)
{
	PatrolPoints = Points;
	PatrolIndex = 0;
}

void AIGListenerEntity::NotifyAnswerKnock(const FVector& KnockLocation)
{
	// The answer only reaches it within ordinary hearing of a knock-loud
	// sound; whispering the code from another floor does nothing.
	FIGNoiseEvent Probe;
	Probe.Location = KnockLocation;
	Probe.Loudness = 0.35f;
	Probe.Radius = Probe.Loudness * UIGNoiseSubsystem::CarryPerLoudness;
	if (!CanHear(Probe))
	{
		return;
	}

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
		2600.0f);
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
		}
	}
	else
	{
		// Wake at the route start, impatience preserved.
		ResetToPatrolStart(/*bRaiseAggression=*/false);
	}
}

void AIGListenerEntity::ResetToPatrolStart(const bool bRaiseAggression)
{
	if (bRaiseAggression)
	{
		SetAggressionTier(AggressionTier + 1);
	}
	TeleportTo(SpawnLocation, GetActorRotation(), false, true);
	PatrolIndex = 0;
	bReactingToSound = false;
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

		// Plaster-white stand-in until the M6 material pass: bone-grey,
		// fully rough, so it reads matte and dusty under a flashlight.
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

void AIGListenerEntity::PlayKnockTriple()
{
	IGAudio::SpawnOneShotAt(
		this,
		UIGToneSequenceSoundWave::CreateWallKnockTriple(this, 0.0f),
		GetActorLocation() + FVector(0.0f, 0.0f, 40.0f),
		1.0f,
		1.0f,
		300.0f,
		3600.0f);
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
		1200.0f);
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
	default:
		break;
	}
	if (Pressure > 0.0f)
	{
		// The component decays pressure on its own; only pushes are sent.
		Stress->SetThreatPressure(Pressure);
	}
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

	// Not a roar: the calmed reply, from very close. Then the director's
	// blackout and the half-past-four bed.
	IGAudio::SpawnOneShotAt(
		this,
		UIGToneSequenceSoundWave::CreateWallKnockReply(this),
		Player ? Player->GetActorLocation() : GetActorLocation(),
		1.0f,
		1.0f,
		120.0f,
		600.0f);

	OnPlayerCaptured.Broadcast(Player);
}
