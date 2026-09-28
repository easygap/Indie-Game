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
	/**
	 * Cap on the log. 256이던 시절에는 밤2 스무 분의 걸음과 심박이 새벽 전에
	 * 문 앞의 노크를 밀어내서, 아침에 트는 발췌가 엉뚱한 자리를 잡거나 아예
	 * 비었다. 폰은 제 층의 둘레만 들으므로 한 밤이 이 안에 넉넉히 들어간다.
	 */
	constexpr int32 MaximumEvents = 4096;

	/** 켜 둔 폰이 듣는 범위. 슬래브 하나를 넘으면 안 들린다. */
	constexpr float MicrophoneReach = 900.0f;
	constexpr float MicrophoneFloorSpan = 250.0f;
	/** 이보다 작게 남는 발소리는 테이프의 쉬 소리에 묻힌다. */
	constexpr float MicrophoneNoiseFloor = 0.01f;

	/** 숨 한 번이 테이프에서 차지하는 길이. */
	constexpr float BreathSeconds = 0.34f;

	/**
	 * How long a refused sound leaves behind, by loudness. A triple knock is
	 * about two seconds of the building being full of him (§4.3-3), and the gap
	 * has to be that long or the player hears an edit instead of an absence.
	 */
	constexpr float MinimumSuppressedSeconds = 0.45f;
	constexpr float MaximumSuppressedSeconds = 2.10f;

	/** Seconds of the take kept before the refusal the excerpt is built around. */
	constexpr float ExcerptLeadSeconds = 4.5f;
	/** 규칙이 풀린 뒤의 테이프는 처음 담긴 그의 소리 조금 앞에서 시작한다. */
	constexpr float KeptLeadSeconds = 1.0f;

	/**
	 * The phone's own speaker: quiet, and no low end at all. 0.58이던 때는 낮
	 * 베드 밑에서 공백과 룸톤이 구별되지 않아, 정확한 길이의 무음이 아무 일
	 * 없는 16초로 들렸다.
	 */
	constexpr float PlaybackVolume = 0.8f;
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
	// 폰이 어디 놓였는지는 새로 켤 때마다 다시 알려 준다. 지난 테이프의 자리를
	// 들고 있으면 다른 층에서 켠 폰이 제 발소리를 버린다.
	bHasMicrophoneLocation = false;
	bRecording = true;
}

void UIGRecordingSubsystem::SetMicrophoneLocation(const FVector& Location)
{
	MicrophoneLocation = Location;
	bHasMicrophoneLocation = true;
}

float UIGRecordingSubsystem::GetTakeOffsetNow() const
{
	const UWorld* World = GetWorld();
	return World
		? static_cast<float>(World->GetTimeSeconds() - RecordingStartSeconds)
		: 0.0f;
}

void UIGRecordingSubsystem::RecordPlayerBody(const float Loudness, const bool bBreath)
{
	if (!bRecording || Loudness <= 0.0f)
	{
		return;
	}
	FIGRecordedSound Sound;
	Sound.OffsetSeconds = GetTakeOffsetNow();
	Sound.Loudness = Loudness;
	Sound.DurationSeconds = bBreath
		? IGRecording::BreathSeconds
		: IGRecording::MinimumSuppressedSeconds;
	Sound.bBody = bBreath;
	AddToTake(Sound);
}

void UIGRecordingSubsystem::AddToTake(const FIGRecordedSound& Sound)
{
	if (Recorded.Num() >= IGRecording::MaximumEvents)
	{
		// Keep the newest. 한 밤이 넉넉히 들어가는 크기라 여기까지 오는 일은
		// 드물다 — 오더라도 버리는 것은 가장 먼 과거다.
		Recorded.RemoveAt(0, 1, EAllowShrinking::No);
	}
	Recorded.Add(Sound);
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

void UIGRecordingSubsystem::RecordEntitySound(
	const FVector& Location,
	const float Loudness,
	AActor* Instigator)
{
	FIGNoiseEvent Event;
	Event.Location = Location;
	Event.Loudness = Loudness;
	Event.Instigator = Instigator;
	if (const UWorld* World = GetWorld())
	{
		Event.TimeSeconds = World->GetTimeSeconds();
	}
	HandleNoiseReported(Event);
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
	const AActor* Instigator = Event.Instigator.Get();
	const bool bFromEntity = Instigator && Instigator->IsA<AIGListenerEntity>();
	float Loudness = Event.Loudness;
	if (!bFromEntity && bHasMicrophoneLocation)
	{
		// 현관 바닥의 폰은 9미터 아래 관리실의 발소리를 듣지 못한다. 같은 층에서도
		// 멀어질수록 작게 담긴다.
		if (FMath::Abs(Event.Location.Z - MicrophoneLocation.Z)
			> IGRecording::MicrophoneFloorSpan)
		{
			return;
		}
		const float Distance = FVector::Dist(Event.Location, MicrophoneLocation);
		Loudness *= FMath::Clamp(
			1.0f - Distance / IGRecording::MicrophoneReach, 0.0f, 1.0f);
		if (Loudness < IGRecording::MicrophoneNoiseFloor)
		{
			return;
		}
	}

	FIGRecordedSound Sound;
	Sound.OffsetSeconds = GetTakeOffsetNow();
	Sound.Loudness = Loudness;
	Sound.bSuppressed = ShouldSuppress(Event);
	Sound.bFromEntity = bFromEntity;
	// Louder events occupied the building for longer, and the gap has to match.
	// 폰과의 거리가 아니라 건물이 들은 크기다 — 공백은 소리가 아니라 그 시간의 길이다.
	Sound.DurationSeconds = FMath::Lerp(
		IGRecording::MinimumSuppressedSeconds,
		IGRecording::MaximumSuppressedSeconds,
		FMath::Clamp(Event.Loudness, 0.0f, 1.0f));
	AddToTake(Sound);
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
	Sound.bFromEntity = bFromEntity;
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
	LastExcerptFocusEndSeconds = -1.0f;
	LastPlaybackSeconds = 0.0f;
	if (!World || Recorded.Num() == 0)
	{
		return false;
	}

	// 발췌는 한 순간을 둘러싼다. 그녀가 듣고 싶은 것은 공백이므로 가장 긴
	// 거부를 가운데 둔다 — 문 너머의 3연이 2.10초로 가장 길다. 같은 길이면
	// 먼저 온 것. 규칙이 풀린 뒤의 테이프라면 처음 담긴 그의 소리를 조금
	// 앞에서부터 튼다. 둘 다 없으면 테이프의 끝 16초다. 예전처럼 앞 16초를
	// 틀면 그 구간에 아무것도 없는 테이프는 재생 자체가 비었다.
	int32 FocusIndex = INDEX_NONE;
	for (int32 Index = 0; Index < Recorded.Num(); ++Index)
	{
		if (Recorded[Index].bSuppressed
			&& (FocusIndex == INDEX_NONE
				|| Recorded[Index].DurationSeconds
					> Recorded[FocusIndex].DurationSeconds + KINDA_SMALL_NUMBER))
		{
			FocusIndex = Index;
		}
	}
	float WindowStart = 0.0f;
	if (FocusIndex != INDEX_NONE)
	{
		WindowStart = FMath::Max(
			0.0f,
			Recorded[FocusIndex].OffsetSeconds - IGRecording::ExcerptLeadSeconds);
	}
	else
	{
		for (int32 Index = 0; Index < Recorded.Num(); ++Index)
		{
			if (Recorded[Index].bFromEntity)
			{
				FocusIndex = Index;
				break;
			}
		}
		WindowStart = FocusIndex != INDEX_NONE
			? FMath::Max(
				0.0f,
				Recorded[FocusIndex].OffsetSeconds - IGRecording::KeptLeadSeconds)
			: FMath::Max(
				0.0f,
				Recorded.Last().OffsetSeconds + 1.0f - MaximumPlaybackSeconds);
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
	// 부르는 쪽이 결론을 증거 뒤에 두도록, 둘러싼 순간이 끝나는 재생 시각과
	// 발췌의 길이를 남긴다.
	if (FocusIndex != INDEX_NONE)
	{
		const FIGRecordedSound& Focus = Recorded[FocusIndex];
		LastExcerptFocusEndSeconds =
			Focus.OffsetSeconds - WindowStart + Focus.DurationSeconds;
	}
	for (const FIGRecordedSound& Sound : Excerpt)
	{
		LastPlaybackSeconds = FMath::Max(
			LastPlaybackSeconds,
			Sound.OffsetSeconds + Sound.DurationSeconds);
	}
	LastPlaybackSeconds = FMath::Max(LastPlaybackSeconds, 1.0f);

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
