#include "Entity/IGMissingFloorMercyDirector.h"

#include "Audio/IGAudioHelpers.h"
#include "Audio/IGToneSequenceSoundWave.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/CollisionProfile.h"
#include "Engine/GameInstance.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Entity/IGListenerEntity.h"
#include "Core/IGPrologueWorldScene.h"
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
	const FVector RiserLocation = AIGPrologueWorldScene::GetSharedRiserLocation();

	/**
	 * 401's door leaf spans about 92 cm around X = -150. The five-capture note
	 * comes out at X = -150 and rests at Y = -269.5, so this one leaves from a
	 * few centimetres over and travels further into the corridor. Both pieces of
	 * paper can be on the tile at once and neither lands on the other.
	 */
	const FVector NoteStartLocation(-146.0f, -239.0f, 900.12f);
	const FVector NoteRestLocation(-128.0f, -288.0f, 900.12f);
	constexpr float NoteStartYaw = 2.5f;
	constexpr float NoteRestYaw = 14.0f;
	/** Slower than the five-capture note's 0.82 s: nobody is in a hurry. */
	constexpr float NoteSlideDuration = 0.94f;
	/** Rubber weatherstrip, then tile, then the paper settling twice. */
	constexpr float NoteFrictionVolume = 0.34f;
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
		return;
	}
	// One note per night. She is a neighbour, not a hint dispenser, and a second
	// piece of paper on the same night would read as a system rather than a
	// person. The corridor is swept between nights, so the note goes with it.
	bNoteDelivered = false;
	bNoteSliding = false;
	NoteSlideSeconds = 0.0f;
	if (Note)
	{
		Note->SetWorldLocation(IGMercy::NoteStartLocation);
		Note->SetWorldRotation(FRotator(0.0f, IGMercy::NoteStartYaw, 0.0f));
		Note->SetVisibility(false, true);
		Note->SetHiddenInGame(true, true);
	}
}

bool AIGMissingFloorMercyDirector::InitializeNote()
{
	if (Note)
	{
		return true;
	}
	// The five-capture note's paper shape is the right object — an 18 x 11 cm
	// folded sheet with the front edge lifted — and its material is torn from
	// the same pad: same recycled fibre, same 0.92 roughness, same ballpoint.
	// Only the sentences differ, which is the whole point. 황순금 has one
	// notepad, so two notes from her should be indistinguishable as objects and
	// tell apart only by what she wrote.
	UStaticMesh* NoteMesh = LoadObject<UStaticMesh>(
		nullptr,
		TEXT("/Game/Meshes/SM_CaptureMercyNote.SM_CaptureMercyNote"));
	UMaterialInterface* NoteMaterial = LoadObject<UMaterialInterface>(
		nullptr,
		TEXT("/Game/Prototype/Materials/M_MercyNoteUnderDoor."
			"M_MercyNoteUnderDoor"));
	if (!NoteMesh || !NoteMaterial)
	{
		return false;
	}
	Note = NewObject<UStaticMeshComponent>(this, TEXT("MercyNoteUnderDoor"));
	if (!Note)
	{
		return false;
	}
	Note->SetMobility(EComponentMobility::Movable);
	Note->SetStaticMesh(NoteMesh);
	Note->SetMaterial(0, NoteMaterial);
	// Same blending discipline as the five-capture note: paper this thin casting
	// a moving shadow smears across the terrazzo, and albedo plus roughness are
	// already enough to sit it on the floor.
	Note->SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);
	Note->SetGenerateOverlapEvents(false);
	Note->SetCanEverAffectNavigation(false);
	Note->SetCastShadow(false);
	Note->SetReceivesDecals(false);
	Note->SetCullDistance(850.0f);
	Note->SetAffectDistanceFieldLighting(false);
	Note->ComponentTags.AddUnique(FName(TEXT("MissingFloor.MercyNoteUnderDoor")));
	AddInstanceComponent(Note);
	Note->RegisterComponent();
	Note->SetWorldLocation(IGMercy::NoteStartLocation);
	Note->SetWorldRotation(FRotator(0.0f, IGMercy::NoteStartYaw, 0.0f));
	Note->SetVisibility(false, true);
	Note->SetHiddenInGame(true, true);
	return true;
}

FVector AIGMissingFloorMercyDirector::GetNoteLocation() const
{
	return Note ? Note->GetComponentLocation() : FVector::ZeroVector;
}

void AIGMissingFloorMercyDirector::UpdateNoteSlide(const float DeltaSeconds)
{
	if (!bNoteSliding || !Note)
	{
		return;
	}
	NoteSlideSeconds += DeltaSeconds;
	const float Alpha = FMath::Clamp(
		NoteSlideSeconds / IGMercy::NoteSlideDuration,
		0.0f,
		1.0f);
	// Smoothstep: a hand pushes paper, and a hand starts and stops.
	const float Eased = FMath::SmoothStep(0.0f, 1.0f, Alpha);
	Note->SetWorldLocation(FMath::Lerp(
		IGMercy::NoteStartLocation,
		IGMercy::NoteRestLocation,
		Eased));
	Note->SetWorldRotation(FRotator(
		0.0f,
		FMath::Lerp(IGMercy::NoteStartYaw, IGMercy::NoteRestYaw, Eased),
		0.0f));
	if (Alpha < 1.0f)
	{
		return;
	}
	bNoteSliding = false;
	// Back to the coarse cadence: the clock is ninety seconds long and nothing
	// else here needs a frame.
	PrimaryActorTick.TickInterval = IGMercy::TickIntervalSeconds;
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
	// The paper keeps moving even while the clock stands down: it is already in
	// the world, and freezing it mid-slide would look like a bug.
	UpdateNoteSlide(DeltaSeconds);

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
	// Round robin starting just past whatever ran last, so the same nudge never
	// comes twice running. Two of the three are conditional — the ear to the
	// wall only exists while P3 is unsolved, and the note is once a night — so
	// the rotation falls through to the pipes, which the building always has.
	static const EIGMercyResponse Order[] =
	{
		EIGMercyResponse::EarToWall,
		EIGMercyResponse::NoteUnderDoor,
		EIGMercyResponse::PipeCry
	};
	constexpr int32 OrderCount = UE_ARRAY_COUNT(Order);

	int32 StartIndex = 0;
	for (int32 Index = 0; Index < OrderCount; ++Index)
	{
		if (Order[Index] == LastResponse)
		{
			StartIndex = (Index + 1) % OrderCount;
			break;
		}
	}

	EIGMercyResponse Fired = EIGMercyResponse::None;
	for (int32 Offset = 0; Offset < OrderCount && Fired == EIGMercyResponse::None; ++Offset)
	{
		switch (Order[(StartIndex + Offset) % OrderCount])
		{
		case EIGMercyResponse::EarToWall:
			if (TryEarToWall())
			{
				Fired = EIGMercyResponse::EarToWall;
			}
			break;
		case EIGMercyResponse::NoteUnderDoor:
			if (TryNoteUnderDoor())
			{
				Fired = EIGMercyResponse::NoteUnderDoor;
			}
			break;
		case EIGMercyResponse::PipeCry:
			if (TryPipeCry())
			{
				Fired = EIGMercyResponse::PipeCry;
			}
			break;
		default:
			break;
		}
	}
	if (Fired == EIGMercyResponse::None)
	{
		return Fired;
	}

	LastResponse = Fired;
	++ResponseCount;
	const TCHAR* FiredName = TEXT("pipe_cry");
	if (Fired == EIGMercyResponse::EarToWall)
	{
		FiredName = TEXT("ear_to_wall");
	}
	else if (Fired == EIGMercyResponse::NoteUnderDoor)
	{
		FiredName = TEXT("note_under_door");
	}
	UE_LOG(
		LogTemp,
		Display,
		TEXT("MISSINGFLOOR_MERCY response=%s count=%d resets=%d"),
		FiredName,
		ResponseCount,
		ResetHintCount);
	return Fired;
}

bool AIGMissingFloorMercyDirector::TryNoteUnderDoor()
{
	if (bNoteDelivered || bNoteSliding || !InitializeNote())
	{
		return false;
	}
	bNoteDelivered = true;
	bNoteSliding = true;
	NoteSlideSeconds = 0.0f;
	Note->SetWorldLocation(IGMercy::NoteStartLocation);
	Note->SetWorldRotation(FRotator(0.0f, IGMercy::NoteStartYaw, 0.0f));
	Note->SetVisibility(true, true);
	Note->SetHiddenInGame(false, true);
	// Every-frame while the paper is in motion, coarse again once it settles.
	PrimaryActorTick.TickInterval = 0.0f;

	// The sound is the whole message for a player facing the other way: rubber
	// weatherstrip, then paper on tile. It arrives on the WORLD bus, because a
	// neighbour pushing a note is the building being ordinary, not a threat.
	IGAudio::SpawnOneShotAt(
		this,
		UIGToneSequenceSoundWave::CreatePaperDoorSlide(this),
		IGMercy::NoteStartLocation,
		IGMercy::NoteFrictionVolume,
		1.0f,
		90.0f,
		760.0f,
		EIGAudioBus::World);
	return true;
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
