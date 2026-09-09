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
	StoreBuzz,
	/** CH02 pressure bed: 38/57 Hz beating with narrow 240 Hz air noise. */
	DoorBeyond,
	/** CH03 roof-only wind and rope tension; no score. */
	RoofWindRope,
	/** Directional tank pressure removed only after C5 and the third scratch tail. */
	RoofTankPressure,
	/** 밤의 4층 복도. 안정기 120Hz, 벽 사이의 공기, 멀리 도로, 가끔 창틀 휘파람. */
	CorridorNight,
	/** 계단실. 속이 빈 콘크리트의 공명 잡음, 샤프트를 타는 바람, 아주 낮은 저역. */
	Stairwell,
	/** 불법 5층. 먼지 바람, 뒤틀리는 목재, 느린 저역 맥박. */
	UpperFloor
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
	float DoorNoiseLow180 = 0.0f;
	float DoorNoiseLow320 = 0.0f;
	/** 공진 대역통과 둘의 기억. 복도·계단실·5층 베드가 쓴다. */
	float BandLowA = 0.0f;
	float BandBandA = 0.0f;
	float BandLowB = 0.0f;
	float BandBandB = 0.0f;
	float BandLowC = 0.0f;
	float BandBandC = 0.0f;
	float BandPass(float Input, float& Low, float& Band, float CenterHz, float Q) const;
};
