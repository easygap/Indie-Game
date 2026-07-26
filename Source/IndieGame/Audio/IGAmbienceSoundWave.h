#pragma once

#include "CoreMinimal.h"
#include "Sound/SoundWaveProcedural.h"
#include "IGAmbienceSoundWave.generated.h"

/** Which looping ambience layer this wave instance renders. */
UENUM()
enum class EIGAmbienceMode : uint8
{
	/** Quiet interior room tone with a refrigerator/appliance mains hum. */
	RoomTone,
	/** Pre-dawn alley wind with slow gusts and a faint distant rumble. */
	StreetWind,
	/** Convenience-store fluorescent ballast buzz with a thin high whine. */
	StoreBuzz
};

/**
 * Allocation-stable procedural PCM source for looping environmental beds.
 *
 * Configure() must be called on the game thread before the wave is handed to
 * an audio component. Afterwards the noise/integrator state is owned
 * exclusively by the audio render thread through OnGeneratePCMAudio.
 */
UCLASS()
class INDIEGAME_API UIGAmbienceSoundWave final : public USoundWaveProcedural
{
	GENERATED_BODY()

public:
	explicit UIGAmbienceSoundWave(const FObjectInitializer& ObjectInitializer);

	/** One-time setup before playback; the seed decorrelates simultaneous instances. */
	void Configure(EIGAmbienceMode InMode, uint32 InNoiseSeed = 0x1F123BB5u);

	virtual int32 OnGeneratePCMAudio(TArray<uint8>& OutAudio, int32 NumSamples) override;

private:
	float NextWhiteSample();

	UPROPERTY()
	EIGAmbienceMode Mode = EIGAmbienceMode::RoomTone;

	// Render-thread-owned generator state.
	uint64 GeneratedSampleCount = 0;
	uint32 NoiseState = 0x1F123BB5u;
	float BrownAccumulator = 0.0f;
};
