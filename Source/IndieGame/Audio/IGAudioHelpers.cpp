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
		Settings.SpatializationAlgorithm = SPATIALIZATION_HRTF;
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
