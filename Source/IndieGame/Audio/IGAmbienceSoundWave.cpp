#include "Audio/IGAmbienceSoundWave.h"

namespace IGAmbience
{
	constexpr int32 SampleRateHz = 48000;
	constexpr float TwoPi = 2.0f * UE_PI;

	/** Phase-stable sine that avoids precision loss on long-running sample counters. */
	float StableSine(const float FrequencyHz, const uint64 SampleIndex)
	{
		const double CyclesExact =
			static_cast<double>(SampleIndex) * FrequencyHz / SampleRateHz;
		const double Phase01 = CyclesExact - FMath::FloorToDouble(CyclesExact);
		return FMath::Sin(TwoPi * static_cast<float>(Phase01));
	}
}

UIGAmbienceSoundWave::UIGAmbienceSoundWave(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bProcedural = true;
	bCanProcessAsync = true;
	bLooping = true;
	Duration = INDEFINITELY_LOOPING_DURATION;
	NumChannels = 1;
	SetSampleRate(IGAmbience::SampleRateHz);
	SampleByteSize = sizeof(int16);
	VirtualizationMode = EVirtualizationMode::PlayWhenSilent;
	Volume = 1.0f;
}

void UIGAmbienceSoundWave::Configure(const EIGAmbienceMode InMode, const uint32 InNoiseSeed)
{
	Mode = InMode;
	NoiseState = InNoiseSeed != 0 ? InNoiseSeed : 0x1F123BB5u;
	BrownAccumulator = 0.0f;
	DoorNoiseLow180 = 0.0f;
	DoorNoiseLow320 = 0.0f;
	GeneratedSampleCount = 0;
}

float UIGAmbienceSoundWave::NextWhiteSample()
{
	// xorshift32; cheap and allocation-free on the audio render thread.
	NoiseState ^= NoiseState << 13;
	NoiseState ^= NoiseState >> 17;
	NoiseState ^= NoiseState << 5;
	return (static_cast<float>(NoiseState) / 2147483648.0f) - 1.0f;
}

int32 UIGAmbienceSoundWave::OnGeneratePCMAudio(TArray<uint8>& OutAudio, const int32 NumSamples)
{
	if (NumSamples <= 0)
	{
		OutAudio.Reset();
		return 0;
	}

	OutAudio.SetNumUninitialized(NumSamples * sizeof(int16), EAllowShrinking::No);
	int16* const OutputSamples = reinterpret_cast<int16*>(OutAudio.GetData());

	for (int32 OutputIndex = 0; OutputIndex < NumSamples; ++OutputIndex)
	{
		const uint64 SampleIndex = GeneratedSampleCount + static_cast<uint64>(OutputIndex);
		const float White = NextWhiteSample();
		// Leaky integration turns white noise into a soft low rumble.
		BrownAccumulator = (BrownAccumulator + White * 0.02f) * 0.995f;
		const float Brown = FMath::Clamp(BrownAccumulator * 3.2f, -1.0f, 1.0f);

		float Output = 0.0f;
		switch (Mode)
		{
		case EIGAmbienceMode::RoomTone:
		{
			const float SlowSwell = 1.0f + 0.12f * IGAmbience::StableSine(0.05f, SampleIndex);
			const float Hum =
				0.004f * IGAmbience::StableSine(31.0f, SampleIndex) +
				0.007f * IGAmbience::StableSine(60.0f, SampleIndex) +
				0.010f * IGAmbience::StableSine(120.0f, SampleIndex) +
				0.003f * IGAmbience::StableSine(240.0f, SampleIndex);
			Output = SlowSwell * (Brown * 0.045f + Hum) + White * 0.002f;
			break;
		}

		case EIGAmbienceMode::StreetWind:
		{
			const float GustCurve =
				0.6f * IGAmbience::StableSine(0.06f, SampleIndex) +
				0.4f * IGAmbience::StableSine(0.017f, SampleIndex);
			const float Gust = FMath::Max(0.0f, GustCurve);
			const float RumbleGate = FMath::Max(
				0.0f,
				IGAmbience::StableSine(0.011f, SampleIndex) - 0.62f) / 0.38f;
			Output =
				Brown * (0.035f + 0.055f * Gust) +
				White * 0.005f +
				0.030f * RumbleGate * IGAmbience::StableSine(42.0f, SampleIndex);
			break;
		}

		case EIGAmbienceMode::StoreBuzz:
		{
			const float BallastFlutter = 1.0f + 0.18f * IGAmbience::StableSine(0.7f, SampleIndex);
			const float Buzz =
				IGAmbience::StableSine(120.0f, SampleIndex) +
				0.34f * IGAmbience::StableSine(360.0f, SampleIndex) +
				0.12f * IGAmbience::StableSine(600.0f, SampleIndex);
			Output =
				BallastFlutter * Buzz * 0.011f +
				White * 0.0035f +
				0.0022f * IGAmbience::StableSine(7902.0f, SampleIndex);
			break;
		}

		case EIGAmbienceMode::DoorBeyond:
		{
			// Two one-pole low passes form a cheap, stable 180-320 Hz band.
			// The band keeps the clue audible on small speakers while the 38/57 Hz
			// pair carries the physical pressure on full-range systems.
			DoorNoiseLow180 += (White - DoorNoiseLow180) * 0.0233f;
			DoorNoiseLow320 += (White - DoorNoiseLow320) * 0.0410f;
			const float LimitedNoise = FMath::Clamp(
				(DoorNoiseLow320 - DoorNoiseLow180) * 4.8f,
				-1.0f,
				1.0f);
			const float SlowBreath =
				0.82f + 0.18f * IGAmbience::StableSine(0.09f, SampleIndex);
			const float LowPair =
				0.014f * IGAmbience::StableSine(38.0f, SampleIndex)
				+ 0.012f * IGAmbience::StableSine(57.0f, SampleIndex);
			Output = SlowBreath * LowPair + LimitedNoise * 0.018f;
			break;
		}

		case EIGAmbienceMode::RoofWindRope:
		{
			const float Gust = FMath::Square(FMath::Clamp(
				0.55f
					+ 0.34f * IGAmbience::StableSine(0.043f, SampleIndex)
					+ 0.21f * IGAmbience::StableSine(0.017f, SampleIndex),
				0.0f,
				1.0f));
			const float RopeGate = FMath::Square(FMath::Clamp(
				(IGAmbience::StableSine(0.071f, SampleIndex) - 0.72f) / 0.28f,
				0.0f,
				1.0f));
			const float RopeTension = RopeGate * (
				0.009f * IGAmbience::StableSine(164.0f, SampleIndex)
				+ 0.004f * IGAmbience::StableSine(328.0f, SampleIndex));
			Output = Brown * (0.025f + 0.050f * Gust)
				+ White * (0.003f + 0.006f * Gust)
				+ RopeTension;
			break;
		}

		case EIGAmbienceMode::RoofTankPressure:
		{
			const float PressurePulse =
				0.58f + 0.42f * IGAmbience::StableSine(0.13f, SampleIndex);
			Output = Brown * 0.022f * PressurePulse
				+ 0.012f * PressurePulse
					* IGAmbience::StableSine(72.0f, SampleIndex)
				+ 0.004f * IGAmbience::StableSine(144.0f, SampleIndex);
			break;
		}

		default:
			break;
		}

		OutputSamples[OutputIndex] = static_cast<int16>(
			FMath::RoundToInt(FMath::Clamp(Output, -1.0f, 1.0f) * 32767.0f));
	}

	GeneratedSampleCount += static_cast<uint64>(NumSamples);
	return NumSamples;
}
