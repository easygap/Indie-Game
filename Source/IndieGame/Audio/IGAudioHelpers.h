#pragma once

#include "CoreMinimal.h"
#include "Audio/IGMissingFloorAudioSubsystem.h"

class UAudioComponent;
class USoundAttenuation;
class USoundBase;

/** Small runtime audio utilities shared by prologue actors. */
namespace IGAudio
{
	/** Creates a transient natural-falloff attenuation object. */
	INDIEGAME_API USoundAttenuation* MakeAttenuation(
		UObject* Outer,
		float InnerRadius,
		float FalloffDistance);

	/**
	 * Fire-and-forget spatialized one-shot. Returns the auto-destroying
	 * component, or nullptr when the world or sound is unavailable.
	 */
	INDIEGAME_API UAudioComponent* SpawnOneShotAt(
		const UObject* WorldContext,
		USoundBase* Sound,
		const FVector& Location,
		float VolumeMultiplier = 1.0f,
		float PitchMultiplier = 1.0f,
		float InnerRadius = 160.0f,
		float FalloffDistance = 1400.0f,
		EIGAudioBus Bus = EIGAudioBus::World,
		bool bPlayWhenPaused = false);
}
