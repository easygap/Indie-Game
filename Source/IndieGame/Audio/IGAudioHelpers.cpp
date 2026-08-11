#include "Audio/IGAudioHelpers.h"

#include "Audio/IGMissingFloorAudioSubsystem.h"
#include "Components/AudioComponent.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundAttenuation.h"
#include "Sound/SoundBase.h"

namespace IGAudio
{
	USoundAttenuation* MakeAttenuation(
		UObject* Outer,
		const float InnerRadius,
		const float FalloffDistance)
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
		USoundAttenuation* Attenuation = MakeAttenuation(Sound, InnerRadius, FalloffDistance);
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
