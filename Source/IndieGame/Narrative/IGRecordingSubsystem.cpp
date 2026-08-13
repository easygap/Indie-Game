#include "Narrative/IGRecordingSubsystem.h"

#include "Audio/IGAudioHelpers.h"
#include "Audio/IGToneSequenceSoundWave.h"
#include "Components/AudioComponent.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Entity/IGListenerEntity.h"
#include "Entity/IGNoiseSubsystem.h"
#include "Narrative/IGMissingFloorNarrativeSubsystem.h"

namespace IGRecording
{
	/** Cap on the log. A whole night of footsteps does not need every step. */
	constexpr int32 MaximumEvents = 256;

	/**
	 * How long a refused sound leaves behind, by loudness. A triple knock is
	 * about two seconds of the building being full of him (§4.3-3), and the gap
	 * has to be that long or the player hears an edit instead of an absence.
	 */
	constexpr float MinimumSuppressedSeconds = 0.45f;
	constexpr float MaximumSuppressedSeconds = 2.10f;

	/** Seconds of the take kept either side of the first refusal. */
	constexpr float ExcerptLeadSeconds = 4.5f;

	/** The phone's own speaker: quiet, and no low end at all. */
	constexpr float PlaybackVolume = 0.58f;
	constexpr float PlaybackInnerRadius = 70.0f;
	constexpr float PlaybackFalloff = 620.0f;
}

void UIGRecordingSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	if (UWorld* World = GetWorld())
	{
		NoiseSubsystem = World->GetSubsystem<UIGNoiseSubsystem>();
		if (NoiseSubsystem)
		{
			NoiseHandle = NoiseSubsystem->OnNoiseReported.AddUObject(
				this,
				&UIGRecordingSubsystem::HandleNoiseReported);
		}
	}
}

void UIGRecordingSubsystem::Deinitialize()
{
	if (NoiseSubsystem)
	{
		NoiseSubsystem->OnNoiseReported.Remove(NoiseHandle);
	}
	NoiseSubsystem = nullptr;
	if (PlaybackComponent)
	{
		PlaybackComponent->Stop();
		PlaybackComponent = nullptr;
	}
	Recorded.Reset();
	Super::Deinitialize();
}

bool UIGRecordingSubsystem::IsRuleLifted() const
{
	// Derived from the save-backed wall state rather than stored beside it, so
	// a loaded game can never think the rule lifted on a night it did not.
	const UWorld* World = GetWorld();
	const UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
	const UIGMissingFloorNarrativeSubsystem* Narrative = GameInstance
		? GameInstance->GetSubsystem<UIGMissingFloorNarrativeSubsystem>()
		: nullptr;
	return Narrative && Narrative->IsNightFourWallOpened();
}

void UIGRecordingSubsystem::StartRecording()
{
	const UWorld* World = GetWorld();
	Recorded.Reset();
	TakeSeconds = 0.0f;
	RecordingStartSeconds = World ? World->GetTimeSeconds() : 0.0;
	bRecording = true;
}

void UIGRecordingSubsystem::StopRecording()
{
	if (!bRecording)
	{
		return;
	}
	bRecording = false;
	if (const UWorld* World = GetWorld())
	{
		TakeSeconds = static_cast<float>(
			World->GetTimeSeconds() - RecordingStartSeconds);
	}
}

void UIGRecordingSubsystem::ClearTake()
{
	Recorded.Reset();
	TakeSeconds = 0.0f;
}

bool UIGRecordingSubsystem::ShouldSuppress(const FIGNoiseEvent& Event) const
{
	if (IsRuleLifted())
	{
		// The one exception, and it is the climax: the hour is ending, so the
		// machine finally keeps what it hears (§5.5).
		return false;
	}
	// Her own footsteps and her own breathing are on the tape — beat 2-6 is
	// built on hearing yourself and nothing else. What the machines refuse is
	// what the hour brought, so the test is who made the sound, not when.
	const AActor* Instigator = Event.Instigator.Get();
	return Instigator && Instigator->IsA<AIGListenerEntity>();
}

void UIGRecordingSubsystem::HandleNoiseReported(const FIGNoiseEvent& Event)
{
	if (!bRecording || Event.Loudness <= 0.0f)
	{
		return;
	}
	if (Recorded.Num() >= IGRecording::MaximumEvents)
	{
		// Keep the newest: the tape she plays in the morning is the end of the
		// night, and the oldest steps are the least interesting thing on it.
		Recorded.RemoveAt(0, 1, EAllowShrinking::No);
	}

	const UWorld* World = GetWorld();
	FIGRecordedSound Sound;
	Sound.OffsetSeconds = World
		? static_cast<float>(World->GetTimeSeconds() - RecordingStartSeconds)
		: 0.0f;
	Sound.Loudness = Event.Loudness;
	Sound.bSuppressed = ShouldSuppress(Event);
	// Louder events occupied the building for longer, and the gap has to match.
	Sound.DurationSeconds = FMath::Lerp(
		IGRecording::MinimumSuppressedSeconds,
		IGRecording::MaximumSuppressedSeconds,
		FMath::Clamp(Event.Loudness, 0.0f, 1.0f));
	Recorded.Add(Sound);
}

void UIGRecordingSubsystem::RecordForTesting(
	const float OffsetSeconds,
	const float Loudness,
	const bool bFromEntity)
{
	FIGRecordedSound Sound;
	Sound.OffsetSeconds = OffsetSeconds;
	Sound.Loudness = Loudness;
	Sound.bSuppressed = bFromEntity && !IsRuleLifted();
	Sound.DurationSeconds = FMath::Lerp(
		IGRecording::MinimumSuppressedSeconds,
		IGRecording::MaximumSuppressedSeconds,
		FMath::Clamp(Loudness, 0.0f, 1.0f));
	Recorded.Add(Sound);
	TakeSeconds = FMath::Max(
		TakeSeconds,
		OffsetSeconds + Sound.DurationSeconds);
}

int32 UIGRecordingSubsystem::GetSuppressedCount() const
{
	int32 Count = 0;
	for (const FIGRecordedSound& Sound : Recorded)
	{
		Count += Sound.bSuppressed ? 1 : 0;
	}
	return Count;
}

int32 UIGRecordingSubsystem::GetSurvivingCount() const
{
	return Recorded.Num() - GetSuppressedCount();
}

float UIGRecordingSubsystem::GetSuppressedSeconds() const
{
	float Seconds = 0.0f;
	for (const FIGRecordedSound& Sound : Recorded)
	{
		if (Sound.bSuppressed)
		{
			Seconds += Sound.DurationSeconds;
		}
	}
	return Seconds;
}

bool UIGRecordingSubsystem::PlayBack(const FVector& Location)
{
	UWorld* World = GetWorld();
	if (!World || Recorded.Num() == 0)
	{
		return false;
	}

	// Excerpt around the first refusal, because the gap is the thing she is
	// listening for. With nothing refused the excerpt is simply the opening.
	float WindowStart = 0.0f;
	for (const FIGRecordedSound& Sound : Recorded)
	{
		if (Sound.bSuppressed)
		{
			WindowStart = FMath::Max(
				0.0f,
				Sound.OffsetSeconds - IGRecording::ExcerptLeadSeconds);
			break;
		}
	}
	const float WindowEnd = WindowStart + MaximumPlaybackSeconds;

	TArray<FIGRecordedSound> Excerpt;
	for (const FIGRecordedSound& Sound : Recorded)
	{
		if (Sound.OffsetSeconds < WindowStart || Sound.OffsetSeconds > WindowEnd)
		{
			continue;
		}
		FIGRecordedSound Shifted = Sound;
		Shifted.OffsetSeconds = Sound.OffsetSeconds - WindowStart;
		Excerpt.Add(Shifted);
	}
	if (Excerpt.Num() == 0)
	{
		return false;
	}

	UIGToneSequenceSoundWave* Take =
		UIGToneSequenceSoundWave::CreateRecordingPlayback(this, Excerpt);
	if (!Take)
	{
		return false;
	}
	if (PlaybackComponent)
	{
		PlaybackComponent->Stop();
		PlaybackComponent = nullptr;
	}
	// PLAYER bus: it is her phone in her hand, and §10.4 lets the room she is
	// standing in answer the little speaker.
	PlaybackComponent = IGAudio::SpawnOneShotAt(
		this,
		Take,
		Location,
		IGRecording::PlaybackVolume,
		1.0f,
		IGRecording::PlaybackInnerRadius,
		IGRecording::PlaybackFalloff,
		EIGAudioBus::Player);
	// Success is "the take was built and dispatched", not "a component came
	// back". Under -nosound there is no audio device to hand one over, and the
	// rule this function exists to enforce is still exactly as testable.
	++PlaybackCount;
	return true;
}
