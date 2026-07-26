#include "Audio/IGAudioHelpers.h"

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
		const float FalloffDistance)
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

		USoundAttenuation* Attenuation = MakeAttenuation(Sound, InnerRadius, FalloffDistance);
		return UGameplayStatics::SpawnSoundAtLocation(
			World,
			Sound,
			Location,
			FRotator::ZeroRotator,
			VolumeMultiplier,
			PitchMultiplier,
			0.0f,
			Attenuation);
	}
}
