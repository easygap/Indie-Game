#include "Audio/IGAudioHelpers.h"

#include "Audio/IGMissingFloorAudioSubsystem.h"
#include "Components/AudioComponent.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundAttenuation.h"
#include "Sound/SoundBase.h"

namespace IGAudio
{
	namespace
	{
		// 단일 플레이어 게임이라 전역 하나로 충분하다. 새로 만드는 감쇠는
		// 즉시 이 값을 읽고, 이미 울고 있는 소리는 오디오 감독이 훑어서
		// 다시 걸어 준다.
		EIGOutputMode ActiveOutputMode = EIGOutputMode::Headphones;

		UAudioComponent* SpawnOneShotInternal(
			const UObject* WorldContext,
			USoundBase* Sound,
			const FVector& Location,
			const float VolumeMultiplier,
			const float PitchMultiplier,
			const float InnerRadius,
			const float FalloffDistance,
			const EIGAudioBus Bus,
			const bool bPlayWhenPaused,
			const bool bForceDry)
		{
			if (!Sound || !WorldContext)
			{
				return nullptr;
			}

			UWorld* World = GEngine
				? GEngine->GetWorldFromContextObject(WorldContext, EGetWorldErrorMode::ReturnNull)
				: nullptr;
			if (!World)
			{
				return nullptr;
			}

			UIGMissingFloorAudioSubsystem* AudioDirector =
				World->GetSubsystem<UIGMissingFloorAudioSubsystem>();
			if (AudioDirector)
			{
				AudioDirector->PrepareSound(Sound, Bus);
			}
			USoundAttenuation* Attenuation = MakeAttenuation(
				Sound,
				InnerRadius,
				FalloffDistance,
				Bus,
				bForceDry);
			UAudioComponent* Component = UGameplayStatics::SpawnSoundAtLocation(
				World,
				Sound,
				Location,
				FRotator::ZeroRotator,
				VolumeMultiplier,
				PitchMultiplier,
				0.0f,
				Attenuation);
			if (Component)
			{
				Component->SetUISound(bPlayWhenPaused);
			}
			if (AudioDirector)
			{
				AudioDirector->RegisterComponent(Component, Bus);
			}
			return Component;
		}
	}

	void SetOutputMode(const EIGOutputMode Mode)
	{
		ActiveOutputMode = Mode;
	}

	EIGOutputMode GetOutputMode()
	{
		return ActiveOutputMode;
	}

	ESoundSpatializationAlgorithm GetSpatializationAlgorithm()
	{
		return ActiveOutputMode == EIGOutputMode::Headphones
			? SPATIALIZATION_HRTF
			: SPATIALIZATION_Default;
	}

	USoundAttenuation* MakeAttenuation(
		UObject* Outer,
		const float InnerRadius,
		const float FalloffDistance,
		const EIGAudioBus Bus,
		const bool bForceDry)
	{
		UObject* SafeOuter = Outer ? Outer : GetTransientPackage();
		USoundAttenuation* Attenuation = NewObject<USoundAttenuation>(SafeOuter);
		FSoundAttenuationSettings& Settings = Attenuation->Attenuation;
		Settings.bAttenuate = true;
		Settings.bSpatialize = true;
		// Resonance Audio is selected project-wide. Plugin spatialization gives
		// the knock/pipe/entity contract its required binaural elevation cues;
		// platforms without the plugin still fall back to UE's normal panning.
		// 스피커를 고르면 평범한 패닝으로 내린다 — 바이노럴을 스피커로 틀면
		// 좌우가 서로 새어 위아래가 오히려 뭉개진다.
		Settings.SpatializationAlgorithm = GetSpatializationAlgorithm();
		Settings.AttenuationShapeExtents = FVector(FMath::Max(1.0f, InnerRadius), 0.0f, 0.0f);
		Settings.FalloffDistance = FMath::Max(1.0f, FalloffDistance);
		Settings.DistanceAlgorithm = EAttenuationDistanceModel::NaturalSound;
		Settings.dBAttenuationAtMax = -60.0f;
		// §10.4: volume tells the player a sound is far. Reverb tells them it is
		// far *through the building* — and the moment it goes dry, it is in the
		// room with them.
		if (bForceDry)
		{
			Settings.bEnableReverbSend = false;
		}
		else
		{
			UIGMissingFloorAudioSubsystem::ConfigureReverbSend(
				Settings,
				Bus,
				InnerRadius,
				Settings.FalloffDistance);
		}
		return Attenuation;
	}

	UAudioComponent* SpawnOneShotAt(
		const UObject* WorldContext,
		USoundBase* Sound,
		const FVector& Location,
		const float VolumeMultiplier,
		const float PitchMultiplier,
		const float InnerRadius,
		const float FalloffDistance,
		const EIGAudioBus Bus,
		const bool bPlayWhenPaused)
	{
		return SpawnOneShotInternal(
			WorldContext,
			Sound,
			Location,
			VolumeMultiplier,
			PitchMultiplier,
			InnerRadius,
			FalloffDistance,
			Bus,
			bPlayWhenPaused,
			/*bForceDry=*/false);
	}

	UAudioComponent* SpawnDryOneShotAt(
		const UObject* WorldContext,
		USoundBase* Sound,
		const FVector& Location,
		const float VolumeMultiplier,
		const float PitchMultiplier,
		const float InnerRadius,
		const float FalloffDistance,
		const EIGAudioBus Bus)
	{
		return SpawnOneShotInternal(
			WorldContext,
			Sound,
			Location,
			VolumeMultiplier,
			PitchMultiplier,
			InnerRadius,
			FalloffDistance,
			Bus,
			/*bPlayWhenPaused=*/false,
			/*bForceDry=*/true);
	}
}
