#include "Audio/IGMissingFloorAudioSubsystem.h"

#include "Audio/IGAudioHelpers.h"
#include "Audio/IGToneSequenceSoundWave.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/AudioComponent.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Player/IGHorrorHUD.h"
#include "Player/IGPlayerCharacter.h"
#include "Sound/ReverbEffect.h"
#include "Sound/SoundAttenuation.h"
#include "Sound/SoundBase.h"
#include "Sound/SoundClass.h"
#include "Sound/SoundMix.h"
#include "HAL/PlatformTime.h"

namespace IGMissingFloorMix
{
	constexpr float SilentDecibels = -96.0f;
	constexpr float EntityNearDistance = 600.0f;
	constexpr float EntityFarHysteresis = 640.0f;
	constexpr float EntityDistanceLeaseSeconds = 0.55f;
	constexpr float TitleKnockDelaySeconds = 4.0f;
	constexpr float TitleReplyDelaySeconds = 1.15f;
	constexpr float TitleCycleSeconds = 12.0f;

	/**
	 * §10.4 거리 리버브 문법. 두 프리셋은 같은 태그를 공유하므로 뒤에
	 * 올린 쪽이 앞의 것을 밀어내며, 엔진이 FadeTime으로 섞어 준다.
	 * 계단을 한 칸 오르는 시간보다 짧아야 공간 전환이 발소리와 맞는다.
	 */
	const FName AcousticSpaceReverbTag(TEXT("MissingFloor.AcousticSpace"));
	constexpr float AcousticSpaceReverbPriority = 1.0f;
	constexpr float AcousticCrossfadeSeconds = 0.90f;
	constexpr float AcousticPollIntervalSeconds = 0.20f;

	/**
	 * 복도 — 1.2m 폭, 2.4m 천장, 석고 마감에 세대문 세 짝. 맨 콘크리트
	 * 복도의 실측 잔향은 2초에 가깝지만, 이 게임의 시그니처 입력은
	 * 「둘, 쉬고, 하나」 리듬이다. 1.25초로 당겨야 세 번의 타격이 서로
	 * 뭉개지지 않고 플레이어가 되받아 칠 수 있다. 현실보다 서사가 먼저다.
	 */
	constexpr float CorridorDecayTime = 1.25f;
	constexpr float CorridorDecayHFRatio = 0.72f;

	/**
	 * 계단실 — 마감 없는 콘크리트 수직 통로. 흡음이 없어 고역이 거의
	 * 그대로 남고, 평행한 계단참 사이에서 플러터 에코가 생겨 후미가
	 * 거칠어진다(Diffusion 낮음). 복도의 두 배 넘게 울리는 이 차이가
	 * 「내 발소리가 계단에서 훨씬 오래 남는다」는 §5.1의 학습을 만든다.
	 */
	constexpr float StairwellDecayTime = 2.70f;
	constexpr float StairwellDecayHFRatio = 0.95f;

	/** 존재의 소리는 완전히 마르지 않는다. 마르면 같은 방이라는 뜻이다. */
	constexpr float EntityReverbFloor = 0.22f;
	constexpr float EntityReverbCeiling = 0.95f;
	constexpr float PuzzleReverbFloor = 0.15f;
	constexpr float PuzzleReverbCeiling = 0.80f;
	constexpr float WorldReverbFloor = 0.10f;
	constexpr float WorldReverbCeiling = 0.55f;
	/** 내 몸의 소리는 거리가 항상 0이므로 고정 센드로 공간을 태운다. */
	constexpr float PlayerManualReverbSend = 0.30f;

	constexpr float BaseDecibels[] =
	{
		0.0f,   // ENTITY
		-3.0f,  // PLAYER
		-1.0f,  // PUZZLE
		-8.0f,  // WORLD
		-10.0f, // UI
		-6.0f   // SCORE
	};

	constexpr int32 VoiceCaps[] =
	{
		4,  // ENTITY
		6,  // PLAYER
		6,  // PUZZLE
		12, // WORLD
		8,  // UI: navigation remains responsive under caption churn
		// SCORE: 압박 층이 상시 베드로 한 자리를 늘 차지한다. 그 위에 지금
		// 도는 루프와 놓아 주는 꼬리 — 둘이면 추격의 4초 테일이 다음 드론에
		// 밀려 0.12초에 잘렸다.
		3
	};

	/**
	 * 어떤 버스가 건물 반향을 타는가. UI와 스코어는 방 안에서 나는 소리가
	 * 아니므로 SoundClass 단계에서 끈다. FSoundClassProperties의 bReverb
	 * 기본값이 true라서 반드시 명시해야 한다.
	 */
	constexpr bool BusUsesBuildingReverb[] =
	{
		true,  // ENTITY
		true,  // PLAYER
		true,  // PUZZLE
		true,  // WORLD
		false, // UI
		false  // SCORE
	};

	const TCHAR* BusNames[] =
	{
		TEXT("BUS_ENTITY"),
		TEXT("BUS_PLAYER"),
		TEXT("BUS_PUZZLE"),
		TEXT("BUS_WORLD"),
		TEXT("BUS_UI"),
		TEXT("BUS_SCORE")
	};

	float DecibelsToLinear(const float Decibels)
	{
		return Decibels <= SilentDecibels
			? 0.0f
			: FMath::Pow(10.0f, Decibels / 20.0f);
	}

	int32 ToIndex(const EIGAudioBus Bus)
	{
		return FMath::Clamp(
			static_cast<int32>(Bus),
			0,
			static_cast<int32>(EIGAudioBus::Count) - 1);
	}
}

void UIGMissingFloorAudioSubsystem::Initialize(
	FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	BuildBusGraph();
	BuildAcousticPresets();
}

void UIGMissingFloorAudioSubsystem::Deinitialize()
{
	if (UWorld* World = GetWorld())
	{
		if (bMixPushed && RuntimeMix)
		{
			UGameplayStatics::PopSoundMixModifier(World, RuntimeMix);
		}
		if (bAcousticSpaceApplied)
		{
			UGameplayStatics::DeactivateReverbEffect(
				World,
				IGMissingFloorMix::AcousticSpaceReverbTag);
		}
	}
	StopScore(0.0f);
	bMixPushed = false;
	bAcousticSpaceApplied = false;
	bAcousticSpaceResolved = false;
	RuntimeMix = nullptr;
	AcousticPresets.Reset();
	BusSoundClasses.Reset();
	for (TArray<FTrackedVoice>& Voices : ActiveVoices)
	{
		Voices.Reset();
	}
	Super::Deinitialize();
}

void UIGMissingFloorAudioSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	if (RuntimeMix && !bMixPushed)
	{
		UGameplayStatics::PushSoundMixModifier(&InWorld, RuntimeMix);
		bMixPushed = true;
	}
	RefreshMix(0.0f);
	// 첫 프레임부터 복도가 울려 있어야 한다. 아직 한 걸음도 걷지 않은
	// 플레이어는 밟은 표면이 없으므로 폴링이 값을 줄 수 없다.
	SetAcousticSpace(AcousticSpace);
}

void UIGMissingFloorAudioSubsystem::Tick(const float DeltaTime)
{
	if (bTitleMode)
	{
		const double Now = FPlatformTime::Seconds();
		if (PendingTitleReplyRealTime > 0.0
			&& Now >= PendingTitleReplyRealTime)
		{
			PendingTitleReplyRealTime = -1.0;
			PlayTitleReply();
		}
		if (NextTitleKnockRealTime > 0.0 && Now >= NextTitleKnockRealTime)
		{
			PlayTitleKnockCycle();
		}
	}
	VoicePruneAccumulator += DeltaTime;
	if (VoicePruneAccumulator >= 0.25f)
	{
		VoicePruneAccumulator = 0.0f;
		PruneVoices();
	}

	AcousticPollAccumulator += DeltaTime;
	if (AcousticPollAccumulator >= IGMissingFloorMix::AcousticPollIntervalSeconds)
	{
		AcousticPollAccumulator = 0.0f;
		PollAcousticSpace();
	}

	const UWorld* World = GetWorld();
	if (bEntityNearPlayer && World
		&& World->GetTimeSeconds() - LastEntityDistanceUpdateSeconds
			> IGMissingFloorMix::EntityDistanceLeaseSeconds)
	{
		bEntityNearPlayer = false;
		RefreshMix();
	}
	// 거리 보고가 끊기면(잠들었거나 사라졌거나) 압박 층도 내려간다.
	if (PresenceAlpha > 0.0f && World
		&& World->GetTimeSeconds() - LastEntityDistanceUpdateSeconds > 1.5f)
	{
		FadePresenceLayer(0.0f, 1.2f);
	}
}

TStatId UIGMissingFloorAudioSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(
		UIGMissingFloorAudioSubsystem,
		STATGROUP_Tickables);
}

void UIGMissingFloorAudioSubsystem::PrepareSound(
	USoundBase* Sound,
	const EIGAudioBus Bus) const
{
	if (!Sound)
	{
		return;
	}
	if (USoundClass* SoundClass = GetBusSoundClass(Bus))
	{
		Sound->SoundClassObject = SoundClass;
	}
}

void UIGMissingFloorAudioSubsystem::SetHeadphoneOutput(const bool bHeadphones)
{
	bHeadphoneOutput = bHeadphones;
	IGAudio::SetOutputMode(
		bHeadphones
			? IGAudio::EIGOutputMode::Headphones
			: IGAudio::EIGOutputMode::Speakers);

	const ESoundSpatializationAlgorithm Algorithm =
		IGAudio::GetSpatializationAlgorithm();
	for (int32 BusIndex = 0; BusIndex < BusCount; ++BusIndex)
	{
		for (FTrackedVoice& Voice : ActiveVoices[BusIndex])
		{
			UAudioComponent* Component = Voice.Component.Get();
			if (!Component)
			{
				continue;
			}
			// 감쇠 개체는 소리마다 새로 만든 것이라 여기서 고쳐도 남의
			// 소리에 번지지 않는다.
			USoundAttenuation* Attenuation = Component->AttenuationSettings;
			if (!Attenuation)
			{
				continue;
			}
			Attenuation->Attenuation.SpatializationAlgorithm = Algorithm;
			Component->AdjustAttenuation(Attenuation->Attenuation);
		}
	}
}

void UIGMissingFloorAudioSubsystem::RegisterComponent(
	UAudioComponent* Component,
	const EIGAudioBus Bus)
{
	RegisterVoice(Component, Bus, /*bPersistent=*/false);
}

void UIGMissingFloorAudioSubsystem::RegisterVoice(
	UAudioComponent* Component,
	const EIGAudioBus Bus,
	const bool bPersistent)
{
	if (!Component)
	{
		return;
	}

	const int32 BusIndex = IGMissingFloorMix::ToIndex(Bus);
	if (BusSoundClasses.IsValidIndex(BusIndex))
	{
		Component->SoundClassOverride = BusSoundClasses[BusIndex];
		PrepareSound(Component->GetSound(), Bus);
	}

	TArray<FTrackedVoice>& Voices = ActiveVoices[BusIndex];
	Voices.RemoveAll([](const FTrackedVoice& Voice)
	{
		return !Voice.Component.IsValid();
	});

	const int32 VoiceCap = GetVoiceCap(Bus);
	while (Voices.Num() >= VoiceCap && Voices.Num() > 0)
	{
		int32 OldestIndex = INDEX_NONE;
		for (int32 Index = 0; Index < Voices.Num(); ++Index)
		{
			if (Voices[Index].bPersistent)
			{
				continue;
			}
			if (OldestIndex == INDEX_NONE
				|| Voices[Index].Serial < Voices[OldestIndex].Serial)
			{
				OldestIndex = Index;
			}
		}
		if (OldestIndex == INDEX_NONE)
		{
			// 남은 게 전부 늘 우는 소리다. 밀 수 있는 게 없으니 새 소리를
			// 얹지 않고 물러난다 — 여기서 계속 돌면 멈추지 않는다.
			return;
		}
		if (UAudioComponent* Oldest = Voices[OldestIndex].Component.Get())
		{
			Oldest->FadeOut(0.12f, 0.0f);
		}
		Voices.RemoveAt(OldestIndex);
	}

	Voices.Add({Component, NextVoiceSerial++, bPersistent});
}

void UIGMissingFloorAudioSubsystem::RegisterPersistentBed(
	UAudioComponent* Component,
	const EIGAudioBus Bus)
{
	RegisterVoice(Component, Bus, /*bPersistent=*/true);
}

void UIGMissingFloorAudioSubsystem::SetThreatState(
	const EIGAudioThreatState NewState)
{
	if (ThreatState == NewState)
	{
		return;
	}
	ThreatState = NewState;
	bEntityListening = NewState == EIGAudioThreatState::Listening;
	RefreshMix();
	if (!bTitleMode)
	{
		SwitchScore(NewState);
	}
}

void UIGMissingFloorAudioSubsystem::SetAcousticSpace(
	const EIGAcousticSpace NewSpace)
{
	const int32 SpaceIndex = FMath::Clamp(
		static_cast<int32>(NewSpace),
		0,
		AcousticSpaceCount - 1);
	const EIGAcousticSpace Clamped = static_cast<EIGAcousticSpace>(SpaceIndex);
	if (bAcousticSpaceResolved && AcousticSpace == Clamped)
	{
		return;
	}
	const bool bFirstResolve = !bAcousticSpaceResolved;
	AcousticSpace = Clamped;
	bAcousticSpaceResolved = true;
	// 첫 판정은 즉시 걸어야 시작 프레임이 드라이하게 새지 않는다. 이후
	// 공간 변화는 계단 한 칸을 오르는 시간 안에 섞인다.
	ApplyAcousticSpace(
		bFirstResolve ? 0.0f : IGMissingFloorMix::AcousticCrossfadeSeconds);
}

EIGAcousticSpace UIGMissingFloorAudioSubsystem::ClassifyAcousticSpace(
	const EIGFootstepSurface Surface)
{
	switch (Surface)
	{
	case EIGFootstepSurface::MetalStair:
		return EIGAcousticSpace::Stairwell;
	case EIGFootstepSurface::Rooftop:
		// 옥상은 반사면이 바닥 하나뿐이다. 건물 반향을 태우면 실내가 된다.
		return EIGAcousticSpace::Open;
	default:
		return EIGAcousticSpace::Corridor;
	}
}

UReverbEffect* UIGMissingFloorAudioSubsystem::GetAcousticPreset(
	const EIGAcousticSpace Space) const
{
	const int32 SpaceIndex = static_cast<int32>(Space);
	return AcousticPresets.IsValidIndex(SpaceIndex)
		? AcousticPresets[SpaceIndex]
		: nullptr;
}

void UIGMissingFloorAudioSubsystem::ConfigureReverbSend(
	FSoundAttenuationSettings& Settings,
	const EIGAudioBus Bus,
	const float InnerRadius,
	const float FalloffDistance)
{
	if (!IGMissingFloorMix::BusUsesBuildingReverb[IGMissingFloorMix::ToIndex(Bus)])
	{
		Settings.bEnableReverbSend = false;
		return;
	}

	Settings.bEnableReverbSend = true;

	if (Bus == EIGAudioBus::Player)
	{
		// 내 발소리와 호흡은 청취자와 같은 자리에서 난다. 거리로 젖음을
		// 구하면 언제나 최솟값이 되어 콘크리트 계단에서도 마른 소리가
		// 난다. 그래서 센드는 고정하고, 대신 프리셋이 길이를 바꾼다 —
		// 계단에서 내 소리가 오래 남는다는 사실이 §5.1의 압박이 된다.
		Settings.ReverbSendMethod = EReverbSendMethod::Manual;
		Settings.ManualReverbSendLevel =
			IGMissingFloorMix::PlayerManualReverbSend;
		return;
	}

	// 가까울수록 마르고 멀수록 젖는다. 플레이어는 이 한 축으로 거리를 읽고,
	// 드라이하게 들리는 순간을 「같은 방」으로 배운다.
	Settings.ReverbSendMethod = EReverbSendMethod::Linear;
	Settings.ReverbDistanceMin = FMath::Clamp(InnerRadius, 120.0f, 400.0f);
	Settings.ReverbDistanceMax = FMath::Max(
		Settings.ReverbDistanceMin + 200.0f,
		FMath::Max(1.0f, FalloffDistance) * 0.85f);
	switch (Bus)
	{
	case EIGAudioBus::Entity:
		Settings.ReverbWetLevelMin = IGMissingFloorMix::EntityReverbFloor;
		Settings.ReverbWetLevelMax = IGMissingFloorMix::EntityReverbCeiling;
		break;
	case EIGAudioBus::Puzzle:
		// 배관과 물은 구조체를 타고 온다. 소품보다 항상 더 젖어 있다.
		Settings.ReverbWetLevelMin = IGMissingFloorMix::PuzzleReverbFloor;
		Settings.ReverbWetLevelMax = IGMissingFloorMix::PuzzleReverbCeiling;
		break;
	case EIGAudioBus::World:
	default:
		Settings.ReverbWetLevelMin = IGMissingFloorMix::WorldReverbFloor;
		Settings.ReverbWetLevelMax = IGMissingFloorMix::WorldReverbCeiling;
		break;
	}
}

void UIGMissingFloorAudioSubsystem::SetPlayerListening(const bool bListening)
{
	if (bPlayerListening == bListening)
	{
		return;
	}
	bPlayerListening = bListening;
	RefreshMix();
}

void UIGMissingFloorAudioSubsystem::SetAuthoredSilence(const bool bSilent)
{
	if (bAuthoredSilence == bSilent)
	{
		return;
	}
	bAuthoredSilence = bSilent;
	RefreshMix(bSilent ? 0.08f : 0.45f);
}

void UIGMissingFloorAudioSubsystem::SetEntityDistance(
	const float DistanceCentimeters)
{
	if (const UWorld* World = GetWorld())
	{
		LastEntityDistanceUpdateSeconds = World->GetTimeSeconds();
	}
	const bool bWasNear = bEntityNearPlayer;
	if (bEntityNearPlayer)
	{
		bEntityNearPlayer = DistanceCentimeters
			<= IGMissingFloorMix::EntityFarHysteresis;
	}
	else
	{
		bEntityNearPlayer = DistanceCentimeters
			<= IGMissingFloorMix::EntityNearDistance;
	}
	if (bWasNear != bEntityNearPlayer)
	{
		RefreshMix();
	}
	UpdatePresenceLayer(DistanceCentimeters);
}

void UIGMissingFloorAudioSubsystem::UpdatePresenceLayer(const float DistanceCentimeters)
{
	UWorld* World = GetWorld();
	if (!World || bTitleMode)
	{
		return;
	}
	// 14m 밖에서 0, 3m 안에서 1. 1.6제곱이라 멀리서는 거의 없고 가까워질수록
	// 급하게 차오른다. 추격 중에는 거리와 무관하게 절반은 깔린다.
	float Target = FMath::Pow(
		FMath::Clamp((1400.0f - DistanceCentimeters) / 1100.0f, 0.0f, 1.0f), 1.6f);
	if (ThreatState == EIGAudioThreatState::Chasing)
	{
		Target = FMath::Max(Target, 0.5f);
	}
	if (Target <= 0.001f && !PresenceComponent)
	{
		return;
	}
	if (!PresenceComponent)
	{
		UIGToneSequenceSoundWave* Layer = UIGToneSequenceSoundWave::CreatePresenceLayer(this);
		PrepareSound(Layer, EIGAudioBus::Score);
		// 볼륨 배수는 1이다. 예전엔 0으로 만들고 AdjustVolume으로 올렸는데,
		// 엔진은 배수 × 페이더로 소리를 내므로 페이더가 어디로 가든 곱은 0이었다
		// — 이 층은 2026-09-09에 넣은 뒤로 한 번도 들린 적이 없다. 재생은
		// 아래 FadePresenceLayer가 페이더 0에서 올리며 시작한다. 자동 파괴를
		// 끄는 것은 0으로 내리는 페이드가 엔진에서 정지이기 때문이다 — 지워지면
		// 다음에 올릴 때 새 컴포넌트를 또 만들어야 한다.
		PresenceComponent = UGameplayStatics::CreateSound2D(
			World, Layer, 1.0f, 1.0f, 0.0f, nullptr, false, false);
		if (!PresenceComponent)
		{
			return;
		}
		PresenceComponent->SetUISound(false);
		RegisterPersistentBed(PresenceComponent, EIGAudioBus::Score);
	}
	FadePresenceLayer(Target, 0.35f);
}

void UIGMissingFloorAudioSubsystem::FadePresenceLayer(const float Target, const float Seconds)
{
	const float Clamped = FMath::Clamp(Target, 0.0f, 1.0f);
	if (FMath::IsNearlyEqual(PresenceAlpha, Clamped, 0.01f))
	{
		return;
	}
	PresenceAlpha = Clamped;
	if (!PresenceComponent)
	{
		return;
	}
	// 0으로 가는 페이드는 엔진이 끝에서 정지로 처리한다. 그래서 다시 올릴 때는
	// 멈춘 컴포넌트를 FadeIn으로 되살리고, 도는 중이면 페이더만 옮긴다 — 내려가는
	// 도중에 AdjustVolume을 받으면 엔진이 정지 예약을 풀고 다시 올라간다.
	const float Level = Clamped * 0.9f;
	if (Level <= 0.005f)
	{
		if (PresenceComponent->IsPlaying())
		{
			PresenceComponent->FadeOut(Seconds, 0.0f);
		}
		return;
	}
	if (!PresenceComponent->IsPlaying())
	{
		PresenceComponent->FadeIn(Seconds, Level);
		return;
	}
	PresenceComponent->AdjustVolume(Seconds, Level);
}

void UIGMissingFloorAudioSubsystem::PlayStinger(const EIGStinger Kind, const FVector& Location)
{
	if (!GetWorld() || bTitleMode)
	{
		return;
	}
	USoundBase* Wave = nullptr;
	float Volume = 1.0f;
	switch (Kind)
	{
	case EIGStinger::CloseCall:
		Wave = IGAudio::SampleOr(
			TEXT("Stinger_CloseCall"),
			[this]() -> USoundBase* { return UIGToneSequenceSoundWave::CreateCloseCallStinger(this); });
		Volume = 0.95f;
		break;
	case EIGStinger::ChaseStart:
		Wave = IGAudio::SampleOr(
			TEXT("Entity_Scream"),
			[this]() -> USoundBase* { return UIGToneSequenceSoundWave::CreateEntityChaseScream(this); });
		Volume = 1.0f;
		break;
	case EIGStinger::Capture:
		Wave = IGAudio::SampleOr(
			TEXT("Entity_Grab"),
			[this]() -> USoundBase* { return UIGToneSequenceSoundWave::CreateCaptureLunge(this); });
		Volume = 1.0f;
		break;
	}
	if (!Wave)
	{
		return;
	}
	IGAudio::SpawnOneShotAt(this, Wave, Location, Volume, 1.0f, 240.0f, 2600.0f, EIGAudioBus::Entity);
}

void UIGMissingFloorAudioSubsystem::SetTitleMode(const bool bEnabled)
{
	if (bTitleMode == bEnabled)
	{
		return;
	}
	bTitleMode = bEnabled;
	if (bTitleMode)
	{
		StartTitleSoundscape();
	}
	else
	{
		StopTitleSoundscape();
	}
}

void UIGMissingFloorAudioSubsystem::SetUserMasterVolume(
	const float Volume01,
	const float FadeSeconds)
{
	const float Clamped = FMath::Clamp(Volume01, 0.25f, 1.0f);
	if (FMath::IsNearlyEqual(UserMasterVolume, Clamped, 0.001f))
	{
		return;
	}
	UserMasterVolume = Clamped;
	RefreshMix(FadeSeconds);
}

float UIGMissingFloorAudioSubsystem::GetBusUserScale(const EIGAudioBus Bus) const
{
	// 이 switch에 ENTITY 분기가 없다는 사실이 §21.1 대원칙의 구현이다.
	switch (Bus)
	{
	case EIGAudioBus::Score:
		return ScoreUserVolume;
	case EIGAudioBus::World:
		return AmbienceUserVolume;
	default:
		return 1.0f;
	}
}

void UIGMissingFloorAudioSubsystem::SetScoreUserVolume(
	const float Volume01,
	const float FadeSeconds)
{
	const float Clamped = FMath::Clamp(Volume01, 0.0f, 1.0f);
	if (FMath::IsNearlyEqual(ScoreUserVolume, Clamped, 0.001f))
	{
		return;
	}
	ScoreUserVolume = Clamped;
	RefreshMix(FadeSeconds);
}

void UIGMissingFloorAudioSubsystem::SetAmbienceUserVolume(
	const float Volume01,
	const float FadeSeconds)
{
	const float Clamped = FMath::Clamp(Volume01, MinimumAmbienceVolume, 1.0f);
	if (FMath::IsNearlyEqual(AmbienceUserVolume, Clamped, 0.001f))
	{
		return;
	}
	AmbienceUserVolume = Clamped;
	RefreshMix(FadeSeconds);
}

void UIGMissingFloorAudioSubsystem::PlayCalibrationKnock()
{
	if (!GetWorld())
	{
		return;
	}
	++CalibrationKnockPlayCount;
	const APlayerController* Controller = GetWorld()->GetFirstPlayerController();
	const APlayerCameraManager* Camera = Controller
		? Controller->PlayerCameraManager
		: nullptr;
	const FVector Location = Camera
		? Camera->GetCameraLocation()
			+ Camera->GetActorForwardVector() * 145.0f
			+ Camera->GetActorRightVector() * 55.0f
			+ FVector::UpVector * 285.0f
		: FVector(145.0f, 55.0f, 285.0f);
	IGAudio::SpawnOneShotAt(
		this,
		IGAudio::SampleOr(
			TEXT("Entity_KnockTriple_Muffled"),
			[this]() -> USoundBase* { return UIGToneSequenceSoundWave::CreateWallKnockTriple(this, 0.78f); }),
		Location,
		0.62f,
		1.0f,
		110.0f,
		1650.0f,
		EIGAudioBus::Entity,
		true);
}

void UIGMissingFloorAudioSubsystem::PlayTruthConfirmation(
	const int32 ConfirmationIndex)
{
	UWorld* World = GetWorld();
	if (!World || ConfirmationIndex <= 0)
	{
		return;
	}
	UIGToneSequenceSoundWave* Strike =
		UIGToneSequenceSoundWave::CreateTuningStrike(
			this,
			ConfirmationIndex);
	PrepareSound(Strike, EIGAudioBus::Score);
	UAudioComponent* Component = UGameplayStatics::CreateSound2D(
		World,
		Strike,
		0.82f,
		1.0f,
		0.0f,
		nullptr,
		false,
		true);
	if (Component)
	{
		Component->SetUISound(false);
		RegisterComponent(Component, EIGAudioBus::Score);
		Component->Play();
	}
}

void UIGMissingFloorAudioSubsystem::PlayEndingATuningResolution()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}
	bAuthoredSilence = false;
	RefreshMix(0.45f);
	StopScore(0.45f);
	ActiveScoreState = EIGAudioThreatState::Finale;
	UIGToneSequenceSoundWave* Resolution =
		UIGToneSequenceSoundWave::CreateTuningMotif(this, true);
	PrepareSound(Resolution, EIGAudioBus::Score);
	ScoreComponent = UGameplayStatics::CreateSound2D(
		World,
		Resolution,
		0.78f,
		1.0f,
		0.0f,
		nullptr,
		false,
		true);
	if (ScoreComponent)
	{
		ScoreComponent->SetUISound(false);
		RegisterComponent(ScoreComponent, EIGAudioBus::Score);
		ScoreComponent->Play();
	}
}

float UIGMissingFloorAudioSubsystem::GetBusDuckingDecibels(
	const EIGAudioBus Bus) const
{
	// §21.1 더킹 열의 전부다. ENTITY는 여기에 분기가 없고, 그것이
	// 「존재의 소리는 절대 눌리지 않는다」의 구현이다(§24 즉시 차단 20).
	if (Bus == EIGAudioBus::Score && bAuthoredSilence)
	{
		return IGMissingFloorMix::SilentDecibels;
	}
	if (Bus == EIGAudioBus::World && bAuthoredSilence)
	{
		// WORLD starts at -8 dB; another -16 lands on the authored
		// -24 dB silence floor while player breath remains untouched.
		return -16.0f;
	}
	if (Bus == EIGAudioBus::Player && bEntityNearPlayer)
	{
		return -4.0f;
	}
	if (Bus == EIGAudioBus::World && (bPlayerListening || bEntityListening))
	{
		return -6.0f;
	}
	return 0.0f;
}

float UIGMissingFloorAudioSubsystem::GetEffectiveBusDecibels(
	const EIGAudioBus Bus) const
{
	const int32 BusIndex = IGMissingFloorMix::ToIndex(Bus);
	const float Ducking = GetBusDuckingDecibels(Bus);
	if (Ducking <= IGMissingFloorMix::SilentDecibels)
	{
		return IGMissingFloorMix::SilentDecibels;
	}
	return IGMissingFloorMix::BaseDecibels[BusIndex] + Ducking;
}

int32 UIGMissingFloorAudioSubsystem::GetVoiceCap(const EIGAudioBus Bus) const
{
	return IGMissingFloorMix::VoiceCaps[IGMissingFloorMix::ToIndex(Bus)];
}

USoundClass* UIGMissingFloorAudioSubsystem::GetBusSoundClass(
	const EIGAudioBus Bus) const
{
	const int32 BusIndex = IGMissingFloorMix::ToIndex(Bus);
	return BusSoundClasses.IsValidIndex(BusIndex)
		? BusSoundClasses[BusIndex]
		: nullptr;
}

bool UIGMissingFloorAudioSubsystem::ValidateContract(
	FString& OutFailure) const
{
	if (BusSoundClasses.Num() != BusCount || !RuntimeMix)
	{
		OutFailure = TEXT("six-bus graph was not constructed");
		return false;
	}
	for (int32 Index = 0; Index < BusCount; ++Index)
	{
		const USoundClass* SoundClass = BusSoundClasses[Index];
		if (!SoundClass)
		{
			OutFailure = FString::Printf(TEXT("bus %d has no SoundClass"), Index);
			return false;
		}
		const float Expected = IGMissingFloorMix::DecibelsToLinear(
			IGMissingFloorMix::BaseDecibels[Index]);
		if (!FMath::IsNearlyEqual(SoundClass->Properties.Volume, Expected, 0.001f))
		{
			OutFailure = FString::Printf(
				TEXT("bus %d base gain drifted"), Index);
			return false;
		}
		if (IGMissingFloorMix::VoiceCaps[Index] <= 0)
		{
			OutFailure = FString::Printf(TEXT("bus %d has no voice cap"), Index);
			return false;
		}
		if (SoundClass->Properties.bReverb
			!= IGMissingFloorMix::BusUsesBuildingReverb[Index])
		{
			OutFailure = FString::Printf(
				TEXT("bus %d building-reverb routing drifted"), Index);
			return false;
		}
	}

	// §10.4 거리 리버브 문법: 두 프리셋이 존재하고, 계단실이 복도보다
	// 두 배 이상 길게 울리며 고역을 더 오래 붙잡아야 공간이 구분된다.
	const UReverbEffect* Corridor =
		GetAcousticPreset(EIGAcousticSpace::Corridor);
	const UReverbEffect* Stairwell =
		GetAcousticPreset(EIGAcousticSpace::Stairwell);
	if (!Corridor || !Stairwell)
	{
		OutFailure = TEXT("acoustic space presets were not constructed");
		return false;
	}
	if (GetAcousticPreset(EIGAcousticSpace::Open))
	{
		OutFailure = TEXT("open air must not carry a building reverb preset");
		return false;
	}
	if (Stairwell->DecayTime < Corridor->DecayTime * 2.0f)
	{
		OutFailure = TEXT("stairwell does not ring twice the corridor");
		return false;
	}
	if (Stairwell->DecayHFRatio <= Corridor->DecayHFRatio)
	{
		OutFailure = TEXT("bare concrete must hold high frequencies longer");
		return false;
	}
	if (Stairwell->Diffusion >= Corridor->Diffusion)
	{
		OutFailure = TEXT("stairwell flutter echo must stay grainier");
		return false;
	}

	// 존재의 소리는 항상 건물을 태운다. 마르는 순간은 같은 방을 뜻하므로
	// 하한이 0이면 이 문법이 성립하지 않는다.
	if (IGMissingFloorMix::EntityReverbFloor <= 0.0f
		|| IGMissingFloorMix::EntityReverbCeiling
			<= IGMissingFloorMix::WorldReverbCeiling)
	{
		OutFailure = TEXT("entity cues do not ride the building reverb");
		return false;
	}
	if (IGMissingFloorMix::PlayerManualReverbSend <= 0.0f)
	{
		OutFailure = TEXT("player body cues must ring the space they stand in");
		return false;
	}
	if (!FMath::IsNearlyEqual(
		GetEffectiveBusDecibels(EIGAudioBus::Entity), 0.0f, 0.01f))
	{
		OutFailure = TEXT("entity bus is not invariant at 0 dB");
		return false;
	}
	if (UserMasterVolume < 0.25f || UserMasterVolume > 1.0f)
	{
		OutFailure = TEXT("user master volume escaped the calibrated range");
		return false;
	}
	OutFailure.Reset();
	return true;
}

bool UIGMissingFloorAudioSubsystem::IsTitleReplyTime(
	const FDateTime& LocalTime)
{
	const int32 MinuteOfDay = LocalTime.GetHour() * 60 + LocalTime.GetMinute();
	return MinuteOfDay >= 4 * 60 + 30 && MinuteOfDay <= 5 * 60 + 30;
}

void UIGMissingFloorAudioSubsystem::BuildBusGraph()
{
	BusSoundClasses.SetNum(BusCount);
	for (int32 Index = 0; Index < BusCount; ++Index)
	{
		USoundClass* SoundClass = NewObject<USoundClass>(
			this,
			IGMissingFloorMix::BusNames[Index]);
		SoundClass->Properties.Volume = IGMissingFloorMix::DecibelsToLinear(
			IGMissingFloorMix::BaseDecibels[Index]);
		SoundClass->Properties.Pitch = 1.0f;
		SoundClass->Properties.bIsUISound = Index == static_cast<int32>(EIGAudioBus::UI);
		SoundClass->Properties.bReverb =
			IGMissingFloorMix::BusUsesBuildingReverb[Index];
		// 2D 소리의 기본 센드는 0으로 남긴다. 젖음은 거리를 아는 감쇠
		// 설정에서만 결정되고, 화면에 붙은 소리는 방을 갖지 않는다.
		SoundClass->Properties.Default2DReverbSendAmount = 0.0f;
		BusSoundClasses[Index] = SoundClass;
	}
	RuntimeMix = NewObject<USoundMix>(this, TEXT("MIX_MISSING_FLOOR_RUNTIME"));
}

void UIGMissingFloorAudioSubsystem::BuildAcousticPresets()
{
	AcousticPresets.SetNum(AcousticSpaceCount);

	// 복도: 석고 마감과 세대문이 고역을 먼저 먹는다. 첫 반사가 9ms면
	// 1.5m 안쪽 벽까지의 왕복과 맞고, 확산이 높아 후미가 매끄럽다.
	UReverbEffect* Corridor = NewObject<UReverbEffect>(
		this,
		TEXT("REVERB_MISSING_FLOOR_CORRIDOR"));
	Corridor->bBypassEarlyReflections = false;
	Corridor->bBypassLateReflections = false;
	Corridor->ReflectionsDelay = 0.009f;
	Corridor->ReflectionsGain = 0.28f;
	Corridor->GainHF = 0.62f;
	Corridor->LateDelay = 0.015f;
	Corridor->LateGain = 1.05f;
	Corridor->DecayTime = IGMissingFloorMix::CorridorDecayTime;
	Corridor->DecayHFRatio = IGMissingFloorMix::CorridorDecayHFRatio;
	Corridor->Density = 0.86f;
	Corridor->Diffusion = 0.80f;
	Corridor->AirAbsorptionGainHF = 0.986f;
	Corridor->Gain = 0.32f;
	AcousticPresets[static_cast<int32>(EIGAcousticSpace::Corridor)] = Corridor;

	// 계단실: 마감이 없다. 고역이 거의 그대로 남고(DecayHFRatio 0.95),
	// 평행한 참 사이의 플러터 에코 때문에 확산이 낮아 후미가 거칠다.
	// 첫 반사가 16ms로 늦은 것이 「높은 통로」의 단서다.
	UReverbEffect* Stairwell = NewObject<UReverbEffect>(
		this,
		TEXT("REVERB_MISSING_FLOOR_STAIRWELL"));
	Stairwell->bBypassEarlyReflections = false;
	Stairwell->bBypassLateReflections = false;
	Stairwell->ReflectionsDelay = 0.016f;
	Stairwell->ReflectionsGain = 0.45f;
	Stairwell->GainHF = 0.80f;
	Stairwell->LateDelay = 0.024f;
	Stairwell->LateGain = 1.32f;
	Stairwell->DecayTime = IGMissingFloorMix::StairwellDecayTime;
	Stairwell->DecayHFRatio = IGMissingFloorMix::StairwellDecayHFRatio;
	Stairwell->Density = 0.95f;
	Stairwell->Diffusion = 0.58f;
	Stairwell->AirAbsorptionGainHF = 0.991f;
	Stairwell->Gain = 0.40f;
	AcousticPresets[static_cast<int32>(EIGAcousticSpace::Stairwell)] = Stairwell;

	// Open은 의도적으로 비어 있다. 건물 밖은 프리셋이 아니라 프리셋의
	// 부재로 표현한다.
	AcousticPresets[static_cast<int32>(EIGAcousticSpace::Open)] = nullptr;
}

void UIGMissingFloorAudioSubsystem::ApplyAcousticSpace(const float FadeSeconds)
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	UReverbEffect* Preset = GetAcousticPreset(AcousticSpace);
	if (!Preset)
	{
		if (bAcousticSpaceApplied)
		{
			UGameplayStatics::DeactivateReverbEffect(
				World,
				IGMissingFloorMix::AcousticSpaceReverbTag);
			bAcousticSpaceApplied = false;
		}
		return;
	}

	UGameplayStatics::ActivateReverbEffect(
		World,
		Preset,
		IGMissingFloorMix::AcousticSpaceReverbTag,
		IGMissingFloorMix::AcousticSpaceReverbPriority,
		1.0f,
		FMath::Max(0.0f, FadeSeconds));
	bAcousticSpaceApplied = true;
}

void UIGMissingFloorAudioSubsystem::PollAcousticSpace()
{
	const UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}
	const APlayerController* Controller = World->GetFirstPlayerController();
	const AIGPlayerCharacter* Character = Controller
		? Cast<AIGPlayerCharacter>(Controller->GetPawn())
		: nullptr;
	if (!Character)
	{
		return;
	}
	// 마지막으로 밟은 표면은 끈적하게 유지된다. 계단참에 멈춰 서 있어도
	// 계단실 반향이 풀리지 않는 것이 옳다 — 공간은 걷지 않아도 그대로다.
	SetAcousticSpace(
		ClassifyAcousticSpace(Character->GetLastFootstepSurface()));
}

void UIGMissingFloorAudioSubsystem::RefreshMix(const float FadeSeconds)
{
	UWorld* World = GetWorld();
	if (!World || !RuntimeMix)
	{
		return;
	}

	for (int32 Index = 0; Index < BusCount; ++Index)
	{
		const EIGAudioBus Bus = static_cast<EIGAudioBus>(Index);
		const float AdditionalDecibels = GetBusDuckingDecibels(Bus);
		UGameplayStatics::SetSoundMixClassOverride(
			World,
			RuntimeMix,
			BusSoundClasses[Index],
			IGMissingFloorMix::DecibelsToLinear(AdditionalDecibels)
				* UserMasterVolume
				* GetBusUserScale(Bus),
			1.0f,
			FMath::Max(0.0f, FadeSeconds),
			false);
	}
}

void UIGMissingFloorAudioSubsystem::PruneVoices()
{
	for (TArray<FTrackedVoice>& Voices : ActiveVoices)
	{
		Voices.RemoveAll([](const FTrackedVoice& Voice)
		{
			return !Voice.Component.IsValid()
				|| (!Voice.Component->IsPlaying()
					&& Voice.Component->bAutoDestroy);
		});
	}
}

void UIGMissingFloorAudioSubsystem::SwitchScore(
	const EIGAudioThreatState NewState,
	const bool bForce)
{
	if (!bForce && ActiveScoreState == NewState)
	{
		return;
	}

	const float ReleaseSeconds = ActiveScoreState == EIGAudioThreatState::Chasing
		? 4.0f
		: 0.30f;
	StopScore(ReleaseSeconds);
	ActiveScoreState = NewState;

	UIGToneSequenceSoundWave* Score = nullptr;
	float Volume = 1.0f;
	// §10.2: 조사 드론은 「페이드인」이다. 갑자기 켜지는 스코어는 드론이 아니라
	// 스팅이고, 스팅은 그의 것이지 음악의 것이 아니다. 추격만 빠르게 든다 —
	// 비명이 먼저 왔으니 음악이 그 뒤를 바로 받아야 한다.
	float FadeInSeconds = 0.30f;
	if (bTitleMode)
	{
		Score = UIGToneSequenceSoundWave::CreateTuningMotif(this, false);
		Volume = 0.78f;
		FadeInSeconds = 1.2f;
	}
	else
	{
		switch (NewState)
		{
		case EIGAudioThreatState::Investigating:
			Score = UIGToneSequenceSoundWave::CreateCavityDrone(this);
			Volume = 0.82f;
			FadeInSeconds = 1.8f;
			break;
		case EIGAudioThreatState::Chasing:
			Score = UIGToneSequenceSoundWave::CreateChaseScore(this);
			Volume = 1.0f;
			FadeInSeconds = 0.30f;
			break;
		case EIGAudioThreatState::Finale:
			Score = UIGToneSequenceSoundWave::CreateCavityDrone(this);
			Volume = 0.62f;
			FadeInSeconds = 1.6f;
			break;
		case EIGAudioThreatState::Calm:
		case EIGAudioThreatState::Banging:
		case EIGAudioThreatState::Listening:
		default:
			break;
		}
	}

	if (!Score || !GetWorld())
	{
		return;
	}
	PrepareSound(Score, EIGAudioBus::Score);
	ScoreComponent = UGameplayStatics::CreateSound2D(
		GetWorld(),
		Score,
		Volume,
		1.0f,
		0.0f,
		nullptr,
		false,
		true);
	if (ScoreComponent)
	{
		ScoreComponent->SetUISound(bTitleMode);
	}
	RegisterComponent(ScoreComponent, EIGAudioBus::Score);
	if (ScoreComponent)
	{
		ScoreComponent->FadeIn(FadeInSeconds, 1.0f);
	}
}

void UIGMissingFloorAudioSubsystem::StopScore(const float FadeSeconds)
{
	if (!ScoreComponent)
	{
		return;
	}
	if (FadeSeconds <= KINDA_SMALL_NUMBER)
	{
		ScoreComponent->Stop();
	}
	else
	{
		ScoreComponent->FadeOut(FadeSeconds, 0.0f);
	}
	ScoreComponent = nullptr;
}

void UIGMissingFloorAudioSubsystem::StartTitleSoundscape()
{
	bAuthoredSilence = false;
	RefreshMix(0.25f);
	SwitchScore(EIGAudioThreatState::Calm, true);
	NextTitleKnockRealTime =
		FPlatformTime::Seconds() + IGMissingFloorMix::TitleKnockDelaySeconds;
	PendingTitleReplyRealTime = -1.0;
}

void UIGMissingFloorAudioSubsystem::StopTitleSoundscape()
{
	NextTitleKnockRealTime = -1.0;
	PendingTitleReplyRealTime = -1.0;
	SwitchScore(ThreatState, true);
}

void UIGMissingFloorAudioSubsystem::PlayTitleKnockCycle()
{
	if (!bTitleMode || !GetWorld())
	{
		return;
	}
	IGAudio::SpawnOneShotAt(
		this,
		IGAudio::SampleOr(
			TEXT("Entity_KnockTriple_Muffled"),
			[this]() -> USoundBase* { return UIGToneSequenceSoundWave::CreateWallKnockTriple(this, 0.72f); }),
		ResolveTitleCueLocation(false),
		0.72f,
		1.0f,
		120.0f,
		1800.0f,
		EIGAudioBus::Entity,
		true);

	const double Now = FPlatformTime::Seconds();
	if (IsTitleReplyTime(FDateTime::Now()))
	{
		PendingTitleReplyRealTime =
			Now + IGMissingFloorMix::TitleReplyDelaySeconds;
	}
	NextTitleKnockRealTime = Now + IGMissingFloorMix::TitleCycleSeconds;
}

void UIGMissingFloorAudioSubsystem::PlayTitleReply()
{
	if (!bTitleMode)
	{
		return;
	}
	IGAudio::SpawnOneShotAt(
		this,
		UIGToneSequenceSoundWave::CreateWallKnockReply(this),
		ResolveTitleCueLocation(true),
		0.62f,
		0.94f,
		100.0f,
		1700.0f,
		EIGAudioBus::Entity,
		true);
	// 자막만 켜고 듣는 사람에게도 네시 반은 있어야 한다(§10.5).
	AIGHorrorHUD::PushAudioCaption(
		this,
		NSLOCTEXT("IGMissingFloor", "TitleReplyCaption", "[벽 너머 — 대답 둘]"),
		2.2f);
}

FVector UIGMissingFloorAudioSubsystem::ResolveTitleCueLocation(
	const bool bReply) const
{
	const APlayerController* Controller = GetWorld()
		? GetWorld()->GetFirstPlayerController()
		: nullptr;
	const APlayerCameraManager* Camera = Controller
		? Controller->PlayerCameraManager
		: nullptr;
	if (!Camera)
	{
		return FVector(0.0f, 0.0f, bReply ? 420.0f : 280.0f);
	}
	const FVector Forward = Camera->GetActorForwardVector();
	const FVector Right = Camera->GetActorRightVector();
	return Camera->GetCameraLocation()
		+ Forward * (bReply ? -110.0f : 180.0f)
		+ Right * (bReply ? -90.0f : 70.0f)
		+ FVector::UpVector * (bReply ? 430.0f : 260.0f);
}
