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
	BandLowA = BandBandA = BandLowB = BandBandB = BandLowC = BandBandC = 0.0f;
	GeneratedSampleCount = 0;
}

float UIGAmbienceSoundWave::BandPass(
	const float Input,
	float& Low,
	float& Band,
	const float CenterHz,
	const float Q) const
{
	// 체임벌린 상태변수 필터. 톤 합성기의 BandNoise와 같은 식이다.
	const float F = 2.0f * FMath::Sin(UE_PI * FMath::Clamp(CenterHz, 30.0f, 7000.0f) / IGAmbience::SampleRateHz);
	Low += F * Band;
	const float High = Input - Low - Band / FMath::Max(Q, 0.5f);
	Band += F * High;
	return FMath::Clamp(Band * 1.2f / FMath::Sqrt(FMath::Max(Q, 0.5f)), -1.0f, 1.0f);
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

		case EIGAmbienceMode::CorridorNight:
		{
			// 복도는 비어 있어도 조용하지 않다. 안정기가 떨고, 벽 사이 공기가 260Hz
			// 언저리에서 숨을 쉬고, 멀리 도로가 한 번씩 지나가고, 창틀이 아주 가끔 운다.
			const float Flutter = 1.0f + 0.22f * IGAmbience::StableSine(0.9f, SampleIndex);
			const float Ballast = Flutter * (
				0.0055f * IGAmbience::StableSine(120.0f, SampleIndex)
				+ 0.0018f * IGAmbience::StableSine(240.0f, SampleIndex)
				+ 0.0006f * IGAmbience::StableSine(360.0f, SampleIndex));
			const float AirSwell = 0.7f + 0.3f * IGAmbience::StableSine(0.031f, SampleIndex);
			const float Air = BandPass(White, BandLowA, BandBandA, 260.0f, 3.0f) * 0.045f * AirSwell;
			const float Traffic = FMath::Max(0.0f, IGAmbience::StableSine(0.019f, SampleIndex) - 0.55f) / 0.45f;
			const float Whistle = FMath::Square(FMath::Max(0.0f, IGAmbience::StableSine(0.037f, SampleIndex) - 0.78f) / 0.22f);
			Output = Ballast
				+ Air
				+ Brown * (0.018f + 0.030f * Traffic)
				+ BandPass(White, BandLowB, BandBandB, 2900.0f, 25.0f) * 0.006f * Whistle
				+ White * 0.0015f;
			break;
		}

		case EIGAmbienceMode::Stairwell:
		{
			// 마감 없는 콘크리트 통. 190Hz와 380Hz 언저리가 울리고, 샤프트를 타는 바람이
			// 한 번씩 올라오고, 발밑에 34Hz가 깔린다.
			const float Gust = FMath::Square(FMath::Clamp(
				0.45f + 0.35f * IGAmbience::StableSine(0.041f, SampleIndex)
					+ 0.20f * IGAmbience::StableSine(0.013f, SampleIndex),
				0.0f,
				1.0f));
			Output = BandPass(White, BandLowA, BandBandA, 190.0f, 6.0f) * 0.060f
				+ BandPass(White, BandLowB, BandBandB, 380.0f, 8.0f) * 0.028f
				+ BandPass(White, BandLowC, BandBandC, 1100.0f, 4.0f) * 0.020f * Gust
				+ Brown * 0.020f
				+ 0.010f * IGAmbience::StableSine(34.0f, SampleIndex);
			break;
		}

		case EIGAmbienceMode::UpperFloor:
		{
			// 불법 증축층. 벽이 다 없는 자리로 바람이 먼지를 밀고, 목재가 아주 가끔
			// 뒤틀리고, 40Hz가 느리게 맥박 친다.
			const float Gust = FMath::Square(FMath::Clamp(
				0.50f + 0.34f * IGAmbience::StableSine(0.047f, SampleIndex)
					+ 0.22f * IGAmbience::StableSine(0.011f, SampleIndex),
				0.0f,
				1.0f));
			const float CreakGate = FMath::Square(FMath::Clamp(
				(IGAmbience::StableSine(0.053f, SampleIndex) - 0.80f) / 0.20f, 0.0f, 1.0f));
			const float Pulse = 0.6f + 0.4f * IGAmbience::StableSine(0.09f, SampleIndex);
			Output = BandPass(White, BandLowA, BandBandA, 700.0f, 2.0f) * 0.030f * Gust
				+ Brown * (0.024f + 0.020f * Gust)
				+ BandPass(White, BandLowB, BandBandB, 1500.0f, 30.0f) * 0.018f * CreakGate
				+ 0.014f * Pulse * IGAmbience::StableSine(40.0f, SampleIndex);
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
