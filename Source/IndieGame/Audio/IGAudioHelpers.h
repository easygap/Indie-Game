#pragma once

#include "CoreMinimal.h"
#include "Audio/IGMissingFloorAudioSubsystem.h"
#include "Sound/SoundAttenuation.h"

class UAudioComponent;
class USoundAttenuation;
class USoundBase;

/** Small runtime audio utilities shared by prologue actors. */
namespace IGAudio
{
	/**
	 * §10.5. 이 게임의 훅은 「위에서 나는 소리」라서 기본이 바이노럴이다.
	 * 그런데 바이노럴을 스피커로 들으면 좌우가 서로 새어 상이 무너진다.
	 * 스피커도 막지 않기로 한 이상, 스피커로 듣는다고 말할 자리가 있어야
	 * 한다.
	 */
	enum class EIGOutputMode : uint8
	{
		Headphones,
		Speakers
	};

	INDIEGAME_API void SetOutputMode(EIGOutputMode Mode);
	INDIEGAME_API EIGOutputMode GetOutputMode();

	/** 지금 출력 방식에 맞는 정위 알고리즘. */
	INDIEGAME_API ESoundSpatializationAlgorithm GetSpatializationAlgorithm();

	/**
	 * Creates a transient natural-falloff attenuation object.
	 *
	 * The bus decides the §10.4 reverb-send curve: how wet this cue gets as it
	 * moves away from the listener. Every spatial cue in the building goes
	 * through here, so distance stays a single readable axis.
	 */
	INDIEGAME_API USoundAttenuation* MakeAttenuation(
		UObject* Outer,
		float InnerRadius,
		float FalloffDistance,
		EIGAudioBus Bus = EIGAudioBus::World,
		bool bForceDry = false);

	/**
	 * 험 존이 내는 소리를 그 자리에 건다.
	 *
	 * 마스킹 반경을 그대로 받아서 들리는 끝과 가려지는 끝을 한 값으로 맞춘다.
	 * 둘이 어긋나면 소리는 나는데 안 가려지는 띠가 생기고, 그 띠를 밟은
	 * 플레이어는 §5.1이 가르치려는 「기계 옆이 안전지대다」를 틀린 규칙으로
	 * 배운다.
	 */
	INDIEGAME_API UAudioComponent* SpawnHumLoopAt(
		AActor* Owner,
		FName ComponentName,
		const FVector& Location,
		float MaskingRadius);

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

	/**
	 * SpawnOneShotAt with the building reverb send switched off entirely.
	 *
	 * §21.3 reserves this for one cue: the two knocks at the moment of capture.
	 * The whole §10.4 grammar spends the game teaching the player to hear
	 * distance as wetness, so the single cue that arrives perfectly dry says
	 * **same space** more plainly than any volume could — he is holding you.
	 * Do not reach for this to make something merely clearer.
	 */
	INDIEGAME_API UAudioComponent* SpawnDryOneShotAt(
		const UObject* WorldContext,
		USoundBase* Sound,
		const FVector& Location,
		float VolumeMultiplier = 1.0f,
		float PitchMultiplier = 1.0f,
		float InnerRadius = 160.0f,
		float FalloffDistance = 1400.0f,
		EIGAudioBus Bus = EIGAudioBus::Entity);
}
