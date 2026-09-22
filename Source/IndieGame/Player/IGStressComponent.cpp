#include "Player/IGStressComponent.h"

#include "Accessibility/IGAccessibilitySubsystem.h"
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
#include "Player/IGPlayerCharacter.h"
#include "Sound/SoundBase.h"
#include "TimerManager.h"

namespace IGStress
{
	/**
	 * §4.3-5 puts the audible pulse at three meters. The cue carries a little
	 * further than the noise report so the player hears their own heart leak
	 * before it has reached anyone — the warning has to arrive before the cost.
	 */
	constexpr float AudibleHeartbeatCarry = 420.0f;
	// §18.3 멀미 완화 비네트의 최대 세기. 공포의 바닥값 0.28보다 살짝 위에
	// 두어 「끝까지 올렸는데 아무 차이가 없다」가 되지 않게 한다.
	constexpr float ComfortVignetteCeiling = 0.34f;
	// §18.6. 표의 「스트레스 0.85+」를 여기 한 번만 적는다.
	constexpr float HeartbeatHapticStressThreshold = 0.85f;
	// §19.8 심박 경고. 같은 임계에서 비네트가 이만큼까지 부풀었다 돌아온다.
	constexpr float HeartbeatWarningVignetteScale = 1.4f;
	// §5.2 숨 참기 반동. 놓으면 이만큼 동안 심박이 1.5배로 뛴다.
	constexpr float BreathReboundSeconds = 4.0f;
	constexpr float BreathReboundScale = 1.5f;
	// 맥동은 심박 속도를 따라간다. 경고가 제 박자로 뛰지 않으면 그건
	// 심박이 아니라 그냥 화면이 흔들리는 것이다.

	// 숨. 스트레스 0.32까지는 안 들리고 1.0에서 0.55다 — 심박이 0.18에서
	// 시작하니 숨이 먼저 들리면 안 된다. 심장이 뛰는 걸 먼저 알고, 그 다음에
	// 자기 숨을 듣는다.
	constexpr float BreathFearFloor = 0.32f;
	constexpr float BreathFearCeiling = 0.55f;
	// 달려서 찬 숨. 발소리를 키우는 §18.2의 같은 값이 숨소리도 키운다.
	constexpr float BreathExertionScale = 0.42f;
	// 참았던 숨을 놓은 반동. 심박 1.5배와 같은 4초 동안 숨이 얹힌다.
	constexpr float BreathReboundBoost = 0.30f;
	constexpr float BreathLevelCeiling = 0.75f;
	constexpr float BreathFadeSeconds = 0.45f;
	// 숨을 참는 순간은 빠르다. 0.45초 뒤에 조용해지는 건 참는 게 아니다.
	constexpr float BreathHoldFadeSeconds = 0.12f;
	// 멈춰 선 뒤 숨이 고르기까지. 6초.
	constexpr float ExertionDecayPerSecond = 1.0f / 6.0f;
	constexpr float GaspCooldownSeconds = 7.0f;
	// 녹음 gasp1은 5초에 숨을 세 번 들이켠다. 첫 번째만 쓴다.
	constexpr float GaspCutSeconds = 0.80f;
	constexpr float GaspVolume = 0.55f;
	constexpr float ReliefCooldownSeconds = 10.0f;
	constexpr float ReliefVolume = 0.50f;
	// 이보다 낮은 스트레스에서 내쉬는 숨은 안도가 아니라 그냥 숨이다.
	constexpr float ReliefStressFloor = 0.18f;
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
	if (IsValid(BreathComponent))
	{
		BreathComponent->Stop();
	}
	BreathComponent = nullptr;
	Super::EndPlay(EndPlayReason);
}

void UIGStressComponent::ApplyScare(const float Amount)
{
	ScareCharge = FMath::Clamp(ScareCharge + FMath::Max(0.0f, Amount), 0.0f, 1.0f);
	// 0.4 위의 놀람은 숨을 들이켠다. 1.0은 포획이고 포획은 제 숨(끊긴 숨과
	// 마찰)을 따로 가지므로 여기서 겹치지 않는다. 등 뒤의 등이 죽는 0.30,
	// 소화기의 0.35는 몸이 움찔하는 것으로 끝난다 — 전부 헐떡이면 아무것도
	// 헐떡이지 않는 것과 같다.
	if (Amount >= 0.4f && Amount < 0.99f)
	{
		PlayGasp();
	}
	RefreshTickState();
}

void UIGStressComponent::SetExertion(const float Exertion01)
{
	const float Clamped = FMath::Clamp(Exertion01, 0.0f, 1.0f);
	if (FMath::IsNearlyEqual(ExertionReported, Clamped, 0.005f))
	{
		return;
	}
	ExertionReported = Clamped;
	Exertion = FMath::Max(Exertion, ExertionReported);
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

void UIGStressComponent::BeginBreathHold(const float MaximumSeconds)
{
	bBreathHeld = true;
	HeartbeatReboundRemaining = 0.0f;
	// 한도보다 조금 길게 걸어 둔다. 놓는 쪽이 끊는다.
	SuppressHeartbeat(FMath::Max(MaximumSeconds, 0.5f) + 1.0f, false);
}

void UIGStressComponent::EndBreathHold(const bool bRebound)
{
	if (!bBreathHeld)
	{
		return;
	}
	bBreathHeld = false;
	HeartbeatSuppressionRemaining = 0.0f;
	bPlayHeartbeatOnSuppressionRelease = false;
	BeatPhase = 0.0f;
	if (bRebound)
	{
		HeartbeatReboundRemaining = IGStress::BreathReboundSeconds;
		// 참았던 만큼 들이켠다. 쿨다운을 무시하는 건 이게 놀람이 아니라
		// 몸의 당연한 순서라서다.
		PlayGasp(/*bIgnoreCooldown=*/true);
	}
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
	UpdateBreathLayer(DeltaSeconds);
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
		|| HeartbeatSuppressionRemaining > KINDA_SMALL_NUMBER
		|| HeartbeatReboundRemaining > KINDA_SMALL_NUMBER
		|| Exertion > KINDA_SMALL_NUMBER
		|| BreathLevelTarget > KINDA_SMALL_NUMBER;
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

	// 숨을 놓은 반동. 참았던 만큼 심장이 몰아친다 — 그동안은 낮은 스트레스에서도
	// 들리고, 0.85 위에서는 반동이 그대로 소음으로 샌다.
	float ReboundAlpha = 0.0f;
	if (HeartbeatReboundRemaining > 0.0f)
	{
		HeartbeatReboundRemaining = FMath::Max(0.0f, HeartbeatReboundRemaining - DeltaSeconds);
		ReboundAlpha = HeartbeatReboundRemaining / IGStress::BreathReboundSeconds;
	}

	// Below a threshold you simply do not hear your own pulse.
	if (Stress < 0.18f && ReboundAlpha <= 0.0f)
	{
		BeatPhase = 0.0f;
		return;
	}

	const float PulseStress = FMath::Max(Stress, 0.18f + 0.22f * ReboundAlpha);
	const float BeatsPerMinute = FMath::Lerp(RestingBPM, PanicBPM, FMath::Pow(PulseStress, 0.85f))
		* (1.0f + 0.22f * ReboundAlpha);
	const float SecondsPerBeat = 60.0f / FMath::Max(BeatsPerMinute, 1.0f);

	BeatPhase += DeltaSeconds;
	if (BeatPhase < SecondsPerBeat)
	{
		return;
	}
	BeatPhase -= SecondsPerBeat;
	HeartbeatReboundScaleNow = 1.0f + (IGStress::BreathReboundScale - 1.0f) * ReboundAlpha;
	PlayHeartbeat(PulseStress);
	HeartbeatReboundScaleNow = 1.0f;
}

void UIGStressComponent::PlayHeartbeat(const float EffectiveStress)
{
	const float ReboundScale = HeartbeatReboundScaleNow;
	// One heartbeat is a lub-dub: a low thump, then a slightly higher,
	// quieter one about a fifth of a beat later. Both are short noise-shaped
	// sines so they read as a body sound rather than a drum.
	TArray<FIGToneNote> Beat;
	const float SafeStress = FMath::Clamp(EffectiveStress, 0.18f, 1.0f);
	const float Loudness = FMath::GetMappedRangeValueClamped(
		FVector2D(0.18f, 1.0f), FVector2D(0.06f, 0.30f), SafeStress)
		* FMath::Clamp(ReboundScale, 1.0f, IGStress::BreathReboundScale);
	Beat.Add({0.0f, 0.16f, 44.0f, Loudness, 0.04f, 2.6f, EIGToneWaveform::Sine});
	Beat.Add({0.0f, 0.10f, 88.0f, Loudness * 0.35f, 0.05f, 3.0f, EIGToneWaveform::Sine});
	Beat.Add({0.20f, 0.13f, 38.0f, Loudness * 0.72f, 0.05f, 2.8f, EIGToneWaveform::Sine});

	// §18.6 심박 진동. 소리를 만드는 자리에서 함께 낸다 — 따로 두면 둘이
	// 어긋나도 아무도 모른다.
	if (SafeStress >= IGStress::HeartbeatHapticStressThreshold)
	{
		if (const AIGPlayerCharacter* PlayerCharacter =
			Cast<AIGPlayerCharacter>(GetOwner()))
		{
			PlayerCharacter->PlayHeartbeatHaptic();
		}
	}

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
					// 반동 중에는 새는 소음도 그만큼 크다. 숨을 참은 값을 치른다.
					const FIGNoiseEvent Reported = Noise->ReportNoise(
						Owner->GetActorLocation(),
						ReboundScale > 1.0f
							? 0.115f * FMath::Clamp(ReboundScale, 1.0f, IGStress::BreathReboundScale)
							: 0.115f,
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

void UIGStressComponent::UpdateBreathLayer(const float DeltaSeconds)
{
	// 폰이 넣는 값은 바닥이고, 거기서 위로는 스스로 가라앉는다.
	Exertion = FMath::Max(
		ExertionReported,
		Exertion - DeltaSeconds * IGStress::ExertionDecayPerSecond);

	const float ReboundAlpha = HeartbeatReboundRemaining > 0.0f
		? HeartbeatReboundRemaining / IGStress::BreathReboundSeconds
		: 0.0f;
	const float Fear = FMath::GetMappedRangeValueClamped(
		FVector2D(IGStress::BreathFearFloor, 1.0f),
		FVector2D(0.0f, IGStress::BreathFearCeiling),
		Stress);
	float Level = FMath::Max(Fear, Exertion * IGStress::BreathExertionScale)
		+ IGStress::BreathReboundBoost * ReboundAlpha;
	if (bBreathHeld)
	{
		Level = 0.0f;
	}
	Level = FMath::Clamp(Level, 0.0f, IGStress::BreathLevelCeiling);
	// 겁먹을수록, 숨이 찰수록 빠르고 높다. 루프 속도를 못 바꾸니 피치로 — 그의
	// 숨이 추격에서 쓰는 것과 같은 수다.
	const float Pitch = 1.0f + 0.10f * FMath::Max(Stress, Exertion);

	UWorld* World = GetWorld();
	if (!BreathComponent && Level > 0.02f && World)
	{
		USoundBase* Loop = IGAudio::SampleOr(
			TEXT("Player_Breath_Scared"),
			[this]() -> USoundBase* { return UIGToneSequenceSoundWave::CreatePlayerBreathLoop(this); });
		UIGMissingFloorAudioSubsystem* AudioDirector =
			World->GetSubsystem<UIGMissingFloorAudioSubsystem>();
		if (AudioDirector)
		{
			AudioDirector->PrepareSound(Loop, EIGAudioBus::Player);
		}
		// 2D: 심박과 같은 이유로 내 안에서 난다. 배수는 1, 볼륨은 페이더다.
		BreathComponent = UGameplayStatics::CreateSound2D(
			this, Loop, 1.0f, 1.0f, 0.0f, nullptr, false, false);
		if (BreathComponent)
		{
			BreathComponent->SetUISound(false);
			if (AudioDirector)
			{
				AudioDirector->RegisterPersistentBed(
					BreathComponent, EIGAudioBus::Player);
			}
		}
	}
	if (!BreathComponent)
	{
		return;
	}

	const float FadeSeconds = bBreathHeld
		? IGStress::BreathHoldFadeSeconds
		: IGStress::BreathFadeSeconds;
	const bool bLevelChanged =
		!FMath::IsNearlyEqual(Level, BreathLevelTarget, 0.02f);
	if (Level <= 0.02f)
	{
		if (bLevelChanged && BreathComponent->IsPlaying())
		{
			// 0으로 가는 페이드는 엔진이 정지로 처리한다. 그래서 되살릴 때는 FadeIn.
			BreathComponent->FadeOut(FadeSeconds, 0.0f);
		}
		BreathLevelTarget = 0.0f;
		return;
	}
	if (!BreathComponent->IsPlaying())
	{
		BreathComponent->FadeIn(FadeSeconds, Level);
		BreathLevelTarget = Level;
	}
	else if (bLevelChanged)
	{
		BreathComponent->AdjustVolume(FadeSeconds, Level);
		BreathLevelTarget = Level;
	}
	if (!FMath::IsNearlyEqual(Pitch, BreathPitchTarget, 0.01f))
	{
		BreathPitchTarget = Pitch;
		BreathComponent->SetPitchMultiplier(Pitch);
	}
}

void UIGStressComponent::PlayGasp(const bool bIgnoreCooldown)
{
	UWorld* World = GetWorld();
	if (!World || bBreathHeld)
	{
		return;
	}
	const double Now = World->GetTimeSeconds();
	if (!bIgnoreCooldown && Now - LastGaspSeconds < IGStress::GaspCooldownSeconds)
	{
		return;
	}
	LastGaspSeconds = Now;
	PlayBreathOneShot(
		IGAudio::SampleOr(
			TEXT("Player_Gasp"),
			[this]() -> USoundBase* { return UIGToneSequenceSoundWave::CreatePlayerGasp(this); }),
		IGStress::GaspVolume,
		IGStress::GaspCutSeconds);
}

void UIGStressComponent::PlayReliefExhale()
{
	UWorld* World = GetWorld();
	if (!World || bBreathHeld || Stress < IGStress::ReliefStressFloor)
	{
		return;
	}
	const double Now = World->GetTimeSeconds();
	if (Now - LastReliefSeconds < IGStress::ReliefCooldownSeconds)
	{
		return;
	}
	LastReliefSeconds = Now;
	PlayBreathOneShot(
		UIGToneSequenceSoundWave::CreatePlayerExhale(this),
		IGStress::ReliefVolume,
		0.0f);
}

void UIGStressComponent::PlayBreathOneShot(
	USoundBase* Sound,
	const float Volume,
	const float CutSeconds)
{
	UWorld* World = GetWorld();
	if (!World || !Sound)
	{
		return;
	}
	UIGMissingFloorAudioSubsystem* AudioDirector =
		World->GetSubsystem<UIGMissingFloorAudioSubsystem>();
	if (AudioDirector)
	{
		AudioDirector->PrepareSound(Sound, EIGAudioBus::Player);
	}
	UAudioComponent* Voice = UGameplayStatics::SpawnSound2D(this, Sound, Volume);
	if (!Voice)
	{
		return;
	}
	Voice->SetUISound(false);
	if (AudioDirector)
	{
		AudioDirector->RegisterComponent(Voice, EIGAudioBus::Player);
	}
	if (CutSeconds > 0.0f)
	{
		FTimerHandle CutHandle;
		TWeakObjectPtr<UAudioComponent> WeakVoice(Voice);
		World->GetTimerManager().SetTimer(
			CutHandle,
			FTimerDelegate::CreateWeakLambda(this, [WeakVoice]()
			{
				if (UAudioComponent* Cut = WeakVoice.Get())
				{
					Cut->FadeOut(0.25f, 0.0f);
				}
			}),
			CutSeconds,
			false);
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

float UIGStressComponent::GetHeartbeatWarningScale() const
{
	// §19.8. 켜져 있고 임계를 넘었을 때만 부푼다. 그 밖에서는 1이라서
	// 아래 계산이 예전과 한 글자도 다르게 돌지 않는다.
	const AActor* Owner = GetOwner();
	const UWorld* World = Owner ? Owner->GetWorld() : nullptr;
	const UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
	const UIGAccessibilitySubsystem* Accessibility = GameInstance
		? GameInstance->GetSubsystem<UIGAccessibilitySubsystem>()
		: nullptr;
	if (!Accessibility
		|| !Accessibility->UsesHeartbeatWarning()
		|| Stress < IGStress::HeartbeatHapticStressThreshold
		|| !World)
	{
		return 1.0f;
	}
	// 심박음이 쓰는 것과 같은 곡선이다. 따로 적으면 경고가 제 박자를
	// 벗어나고, 그러면 심박이 아니라 그냥 흔들리는 화면이 된다.
	const float WarningBeatsPerMinute =
		FMath::Lerp(RestingBPM, PanicBPM, FMath::Pow(Stress, 0.85f));
	const float BeatsPerSecond =
		FMath::Max(WarningBeatsPerMinute, 1.0f) / 60.0f;
	const float Phase = FMath::Frac(World->GetTimeSeconds() * BeatsPerSecond);
	// 한 박에 한 번 부풀었다 돌아온다.
	const float Pulse = 0.5f - 0.5f * FMath::Cos(Phase * 2.0f * UE_PI);
	return FMath::Lerp(1.0f, IGStress::HeartbeatWarningVignetteScale, Pulse);
}

float UIGStressComponent::GetComfortVignetteStrength() const
{
	const AActor* Owner = GetOwner();
	const UWorld* World = Owner ? Owner->GetWorld() : nullptr;
	const UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
	const UIGAccessibilitySubsystem* Accessibility = GameInstance
		? GameInstance->GetSubsystem<UIGAccessibilitySubsystem>()
		: nullptr;
	return Accessibility ? Accessibility->GetComfortVignetteStrength() : 0.0f;
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
	// §18.3 멀미 완화 비네트는 공포와 무관하게 늘 서 있다. 같은 후처리를
	// 쓰되 둘이 만나면 큰 쪽을 남긴다 — 편하라고 넣은 것이 공포의 터널
	// 시야를 지워서는 안 된다. 설정이 0이면 아래는 예전과 한 글자도 다르게
	// 돌지 않는다.
	const float Comfort = GetComfortVignetteStrength();
	const float Weight = FMath::Max(Ramp, Comfort);
	FearPostProcess->BlendWeight = Weight;
	if (Weight <= 0.0f)
	{
		return;
	}

	FPostProcessSettings& Settings = FearPostProcess->Settings;

	Settings.bOverride_VignetteIntensity = true;
	Settings.VignetteIntensity = FMath::Max(
		FMath::Lerp(0.28f, 0.72f, Ramp),
		Comfort * IGStress::ComfortVignetteCeiling)
		* GetHeartbeatWarningScale();

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
	// 밤 기본 0.16 위에 얹히는 값. 그레인 텍스처가 실제로 돌게 된 뒤의 눈금이다.
	Settings.FilmGrainIntensity = FMath::Lerp(0.16f, 0.30f, Ramp);

	// 불안이 높아져도 가까운 기록은 읽을 수 있어야 한다. 초점을 7~24m에
	// 고정하면 손에 든 단서까지 흐려진다. 압박은 주변 시야·색·심박으로 전한다.
	Settings.bOverride_DepthOfFieldFocalDistance = false;
	Settings.bOverride_DepthOfFieldFstop = false;
}
