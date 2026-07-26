#include "Player/IGStressComponent.h"

#include "Audio/IGToneSequenceSoundWave.h"
#include "Camera/CameraComponent.h"
#include "Components/PostProcessComponent.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Kismet/GameplayStatics.h"

UIGStressComponent::UIGStressComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = true;
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
}

void UIGStressComponent::ApplyScare(const float Amount)
{
	ScareCharge = FMath::Clamp(ScareCharge + FMath::Max(0.0f, Amount), 0.0f, 1.0f);
}

void UIGStressComponent::SetThreatPressure(const float Pressure)
{
	ThreatPressure = FMath::Clamp(Pressure, 0.0f, 1.0f);
}

void UIGStressComponent::SetDarkness(const float InDarkness)
{
	Darkness = FMath::Clamp(InDarkness, 0.0f, 1.0f);
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

	// One heartbeat is a lub-dub: a low thump, then a slightly higher,
	// quieter one about a fifth of a beat later. Both are short noise-shaped
	// sines so they read as a body sound rather than a drum.
	TArray<FIGToneNote> Beat;
	const float Loudness = FMath::GetMappedRangeValueClamped(
		FVector2D(0.18f, 1.0f), FVector2D(0.06f, 0.30f), Stress);
	Beat.Add({0.0f, 0.16f, 44.0f, Loudness, 0.04f, 2.6f, EIGToneWaveform::Sine});
	Beat.Add({0.0f, 0.10f, 88.0f, Loudness * 0.35f, 0.05f, 3.0f, EIGToneWaveform::Sine});
	Beat.Add({0.20f, 0.13f, 38.0f, Loudness * 0.72f, 0.05f, 2.8f, EIGToneWaveform::Sine});

	UIGToneSequenceSoundWave* Heartbeat = NewObject<UIGToneSequenceSoundWave>(this);
	Heartbeat->ConfigureNotes(MoveTemp(Beat), false);
	// 2D: the player's own pulse is inside their head, not in the room.
	UGameplayStatics::PlaySound2D(this, Heartbeat, 1.0f, 1.0f);
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
	Settings.VignetteIntensity = FMath::Lerp(0.40f, 1.15f, Ramp);

	// Colour drains toward grey as fear rises — tunnel vision is partly a
	// loss of colour discrimination, and it reads instantly on screen.
	Settings.bOverride_ColorSaturation = true;
	Settings.ColorSaturation = FVector4(
		FMath::Lerp(1.0f, 0.42f, Ramp),
		FMath::Lerp(1.0f, 0.42f, Ramp),
		FMath::Lerp(1.0f, 0.48f, Ramp),
		1.0f);

	// A cold cast at the edges of panic.
	Settings.bOverride_ColorGain = true;
	Settings.ColorGain = FVector4(
		FMath::Lerp(1.0f, 0.94f, Ramp),
		FMath::Lerp(1.0f, 0.97f, Ramp),
		FMath::Lerp(1.0f, 1.06f, Ramp),
		1.0f);

	Settings.bOverride_SceneFringeIntensity = true;
	Settings.SceneFringeIntensity = FMath::Lerp(0.0f, 2.6f, Ramp);

	// Focus pulls in: the far end of the corridor goes soft.
	Settings.bOverride_DepthOfFieldFocalDistance = true;
	Settings.DepthOfFieldFocalDistance = FMath::Lerp(2400.0f, 340.0f, Ramp);
	Settings.bOverride_DepthOfFieldFstop = true;
	Settings.DepthOfFieldFstop = FMath::Lerp(22.0f, 3.4f, Ramp);
}
