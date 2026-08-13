#include "Player/IGStressComponent.h"

#include "Audio/IGAudioHelpers.h"
#include "Audio/IGToneSequenceSoundWave.h"
#include "Audio/IGMissingFloorAudioSubsystem.h"
#include "Camera/CameraComponent.h"
#include "Components/AudioComponent.h"
#include "Components/PostProcessComponent.h"
#include "Engine/World.h"
#include "Entity/IGNoiseSubsystem.h"
#include "GameFramework/Actor.h"
#include "Kismet/GameplayStatics.h"

namespace IGStress
{
	/**
	 * §4.3-5 puts the audible pulse at three meters. The cue carries a little
	 * further than the noise report so the player hears their own heart leak
	 * before it has reached anyone — the warning has to arrive before the cost.
	 */
	constexpr float AudibleHeartbeatCarry = 420.0f;
}

UIGStressComponent::UIGStressComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	// Calm gameplay has no fear presentation to integrate. Input setters wake
	// the component when darkness, a threat, a scare or authored silence starts.
	PrimaryComponentTick.bStartWithTickEnabled = false;
}

void UIGStressComponent::BeginPlay()
{
	Super::BeginPlay();

	AActor* Owner = GetOwner();
	if (!Owner)
	{
		return;
	}

	// An unbounded post-process volume riding on the player. It sits at a
	// higher priority than the world volume so the fear ramp always wins.
	FearPostProcess = NewObject<UPostProcessComponent>(Owner, TEXT("StressPostProcess"));
	FearPostProcess->SetupAttachment(Owner->GetRootComponent());
	FearPostProcess->bUnbound = true;
	FearPostProcess->Priority = 5.0f;
	FearPostProcess->BlendWeight = 0.0f;
	FearPostProcess->RegisterComponent();

	UpdatePostProcess();
	RefreshTickState();
}

void UIGStressComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (IsValid(HeartbeatComponent))
	{
		HeartbeatComponent->Stop();
	}
	HeartbeatComponent = nullptr;
	Super::EndPlay(EndPlayReason);
}

void UIGStressComponent::ApplyScare(const float Amount)
{
	ScareCharge = FMath::Clamp(ScareCharge + FMath::Max(0.0f, Amount), 0.0f, 1.0f);
	RefreshTickState();
}

void UIGStressComponent::SetThreatPressure(const float Pressure)
{
	ThreatPressure = FMath::Clamp(Pressure, 0.0f, 1.0f);
	RefreshTickState();
}

void UIGStressComponent::SetDarkness(const float InDarkness)
{
	Darkness = FMath::Clamp(InDarkness, 0.0f, 1.0f);
	RefreshTickState();
}

void UIGStressComponent::SuppressHeartbeat(
	const float DurationSeconds,
	const bool bPlayOneBeatOnRelease)
{
	const float SafeDuration = FMath::Max(0.0f, DurationSeconds);
	if (SafeDuration <= KINDA_SMALL_NUMBER)
	{
		return;
	}
	HeartbeatSuppressionRemaining = FMath::Max(
		HeartbeatSuppressionRemaining,
		SafeDuration);
	bPlayHeartbeatOnSuppressionRelease =
		bPlayHeartbeatOnSuppressionRelease || bPlayOneBeatOnRelease;
	BeatPhase = 0.0f;
	if (IsValid(HeartbeatComponent))
	{
		HeartbeatComponent->Stop();
	}
	HeartbeatComponent = nullptr;
	RefreshTickState();
}

float UIGStressComponent::GetBreathsPerMinute() const
{
	// Resting 13, hyperventilating 34. Breathing leads the heart slightly:
	// the player hears themselves start to pant before the pulse catches up.
	const float Eased = FMath::Pow(Stress, 0.8f);
	return FMath::Lerp(13.0f, 34.0f, Eased);
}

void UIGStressComponent::TickComponent(
	const float DeltaSeconds,
	const ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaSeconds, TickType, ThisTickFunction);

	UpdateStress(DeltaSeconds);
	UpdateHeartbeat(DeltaSeconds);
	UpdateTremor(DeltaSeconds);
	UpdatePostProcess();
	RefreshTickState();
}

void UIGStressComponent::RefreshTickState()
{
	const bool bNeedsTick =
		Stress > KINDA_SMALL_NUMBER
		|| Darkness > KINDA_SMALL_NUMBER
		|| ThreatPressure > KINDA_SMALL_NUMBER
		|| ScareCharge > KINDA_SMALL_NUMBER
		|| HeartbeatSuppressionRemaining > KINDA_SMALL_NUMBER;
	if (IsComponentTickEnabled() != bNeedsTick)
	{
		SetComponentTickEnabled(bNeedsTick);
	}
}

void UIGStressComponent::UpdateStress(const float DeltaSeconds)
{
	const float Previous = Stress;

	// A scare is a spike, not a plateau: the charge dumps into stress fast
	// and then empties, so the level settles back to whatever the sustained
	// sources justify.
	const float ScareTransfer = FMath::Min(ScareCharge, DeltaSeconds * 4.0f);
	Stress += ScareTransfer;
	ScareCharge -= ScareTransfer;

	Stress += Darkness * DarknessRate * DeltaSeconds;
	Stress += ThreatPressure * ThreatRate * DeltaSeconds;

	// Recovery only applies when nothing is pressing. Standing in the dark
	// does not calm you down just because nothing is chasing you.
	const float Pressure = FMath::Max(Darkness * 0.5f, ThreatPressure);
	if (Pressure < 0.05f && ScareCharge <= 0.0f)
	{
		Stress -= RecoveryRate * DeltaSeconds;
	}

	Stress = FMath::Clamp(Stress, 0.0f, 1.0f);

	// Threat pressure has to be re-asserted every frame by whatever is
	// hunting; otherwise a despawned pursuer would pin the player forever.
	ThreatPressure = FMath::Max(0.0f, ThreatPressure - DeltaSeconds * 2.0f);

	if (!FMath::IsNearlyEqual(Previous, Stress, 0.002f))
	{
		OnStressChanged.Broadcast(Stress);
	}
}

void UIGStressComponent::UpdateHeartbeat(const float DeltaSeconds)
{
	if (HeartbeatSuppressionRemaining > 0.0f)
	{
		HeartbeatSuppressionRemaining = FMath::Max(
			0.0f,
			HeartbeatSuppressionRemaining - DeltaSeconds);
		BeatPhase = 0.0f;
		if (HeartbeatSuppressionRemaining > 0.0f)
		{
			return;
		}

		const bool bPlayReleaseBeat = bPlayHeartbeatOnSuppressionRelease;
		bPlayHeartbeatOnSuppressionRelease = false;
		if (bPlayReleaseBeat)
		{
			PlayHeartbeat(FMath::Max(Stress, 0.38f));
			return;
		}
	}

	// Below a threshold you simply do not hear your own pulse.
	if (Stress < 0.18f)
	{
		BeatPhase = 0.0f;
		return;
	}

	const float BeatsPerMinute = FMath::Lerp(RestingBPM, PanicBPM, FMath::Pow(Stress, 0.85f));
	const float SecondsPerBeat = 60.0f / FMath::Max(BeatsPerMinute, 1.0f);

	BeatPhase += DeltaSeconds;
	if (BeatPhase < SecondsPerBeat)
	{
		return;
	}
	BeatPhase -= SecondsPerBeat;
	PlayHeartbeat(Stress);
}

void UIGStressComponent::PlayHeartbeat(const float EffectiveStress)
{
	// One heartbeat is a lub-dub: a low thump, then a slightly higher,
	// quieter one about a fifth of a beat later. Both are short noise-shaped
	// sines so they read as a body sound rather than a drum.
	TArray<FIGToneNote> Beat;
	const float SafeStress = FMath::Clamp(EffectiveStress, 0.18f, 1.0f);
	const float Loudness = FMath::GetMappedRangeValueClamped(
		FVector2D(0.18f, 1.0f), FVector2D(0.06f, 0.30f), SafeStress);
	Beat.Add({0.0f, 0.16f, 44.0f, Loudness, 0.04f, 2.6f, EIGToneWaveform::Sine});
	Beat.Add({0.0f, 0.10f, 88.0f, Loudness * 0.35f, 0.05f, 3.0f, EIGToneWaveform::Sine});
	Beat.Add({0.20f, 0.13f, 38.0f, Loudness * 0.72f, 0.05f, 2.8f, EIGToneWaveform::Sine});

	UIGToneSequenceSoundWave* Heartbeat = NewObject<UIGToneSequenceSoundWave>(this);
	Heartbeat->ConfigureNotes(MoveTemp(Beat), false);
	// 2D: the player's own pulse is inside their head, not in the room.
	if (IsValid(HeartbeatComponent))
	{
		HeartbeatComponent->Stop();
	}
	HeartbeatComponent = nullptr;
	if (UIGMissingFloorAudioSubsystem* AudioDirector = GetWorld()
		? GetWorld()->GetSubsystem<UIGMissingFloorAudioSubsystem>()
		: nullptr)
	{
		AudioDirector->PrepareSound(Heartbeat, EIGAudioBus::Player);
	}
	HeartbeatComponent = UGameplayStatics::CreateSound2D(
		this,
		Heartbeat,
		1.0f,
		1.0f);
	if (HeartbeatComponent)
	{
		HeartbeatComponent->SetUISound(false);
		if (UIGMissingFloorAudioSubsystem* AudioDirector = GetWorld()
			? GetWorld()->GetSubsystem<UIGMissingFloorAudioSubsystem>()
			: nullptr)
		{
			AudioDirector->RegisterComponent(
				HeartbeatComponent,
				EIGAudioBus::Player);
		}
		HeartbeatComponent->Play();
	}

	// Panic betrays you: past 0.85 the pulse itself is a sound in the world,
	// carrying about three meters. Standing beside a humming machine still
	// swallows it — managing fear and finding cover are the same skill.
	if (EffectiveStress >= 0.85f)
	{
		if (AActor* Owner = GetOwner())
		{
			if (UWorld* World = GetWorld())
			{
				if (UIGNoiseSubsystem* Noise =
					World->GetSubsystem<UIGNoiseSubsystem>())
				{
					const FIGNoiseEvent Reported = Noise->ReportNoise(
						Owner->GetActorLocation(),
						0.115f,
						Owner);
					// §21.3 심박 소음화. Only when the report actually survived
					// masking: beside the fridge the pulse is swallowed, and
					// hearing it leak anyway would teach the player that cover
					// does not work. The mix tells the truth or it teaches a lie.
					if (Reported.Loudness > 0.0f)
					{
						IGAudio::SpawnOneShotAt(
							this,
							UIGToneSequenceSoundWave::CreateAudibleHeartbeat(
								this,
								Loudness),
							Owner->GetActorLocation(),
							1.0f,
							1.0f,
							60.0f,
							IGStress::AudibleHeartbeatCarry,
							EIGAudioBus::Player);
					}
				}
			}
		}
	}
}

void UIGStressComponent::UpdateTremor(const float DeltaSeconds)
{
	TremorTime += DeltaSeconds;

	// Nothing below half stress; past that a fine, fast shiver comes in.
	const float Amount = FMath::GetMappedRangeValueClamped(
		FVector2D(0.5f, 1.0f), FVector2D(0.0f, 0.42f), Stress);
	if (Amount <= 0.0f)
	{
		Tremor = FRotator::ZeroRotator;
		return;
	}

	// Two incommensurate frequencies so the shiver never visibly repeats.
	Tremor.Pitch = FMath::Sin(TremorTime * 23.7f) * Amount;
	Tremor.Yaw = FMath::Sin(TremorTime * 17.3f + 1.1f) * Amount;
	Tremor.Roll = FMath::Sin(TremorTime * 11.9f + 2.4f) * Amount * 0.6f;
}

void UIGStressComponent::UpdatePostProcess()
{
	if (!FearPostProcess)
	{
		return;
	}

	// The ramp is held back until stress is genuinely high; a permanent
	// vignette on a calm walk just looks like a broken camera.
	const float Ramp = FMath::GetMappedRangeValueClamped(
		FVector2D(0.25f, 1.0f), FVector2D(0.0f, 1.0f), Stress);
	FearPostProcess->BlendWeight = Ramp;
	if (Ramp <= 0.0f)
	{
		return;
	}

	FPostProcessSettings& Settings = FearPostProcess->Settings;

	Settings.bOverride_VignetteIntensity = true;
	Settings.VignetteIntensity = FMath::Lerp(0.28f, 0.72f, Ramp);

	// Colour drains toward grey as fear rises — tunnel vision is partly a
	// loss of colour discrimination, and it reads instantly on screen.
	Settings.bOverride_ColorSaturation = true;
	Settings.ColorSaturation = FVector4(
		FMath::Lerp(1.0f, 0.66f, Ramp),
		FMath::Lerp(1.0f, 0.66f, Ramp),
		FMath::Lerp(1.0f, 0.70f, Ramp),
		1.0f);

	// A cold cast at the edges of panic.
	Settings.bOverride_ColorGain = true;
	Settings.ColorGain = FVector4(
		FMath::Lerp(1.0f, 0.94f, Ramp),
		FMath::Lerp(1.0f, 0.97f, Ramp),
		FMath::Lerp(1.0f, 1.06f, Ramp),
		1.0f);

	Settings.bOverride_SceneFringeIntensity = true;
	Settings.SceneFringeIntensity = FMath::Lerp(0.0f, 0.8f, Ramp);

	Settings.bOverride_FilmGrainIntensity = true;
	Settings.FilmGrainIntensity = FMath::Lerp(0.04f, 0.08f, Ramp);

	// Focus pulls in: the far end of the corridor goes soft.
	Settings.bOverride_DepthOfFieldFocalDistance = true;
	Settings.DepthOfFieldFocalDistance = FMath::Lerp(2400.0f, 700.0f, Ramp);
	Settings.bOverride_DepthOfFieldFstop = true;
	Settings.DepthOfFieldFstop = FMath::Lerp(22.0f, 5.6f, Ramp);
}
