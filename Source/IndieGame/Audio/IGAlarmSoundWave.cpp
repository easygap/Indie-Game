#include "Audio/IGAlarmSoundWave.h"

namespace IGAlarmSoundWave
{
	constexpr int32 SampleRateHz = 48000;
	constexpr int32 PulseSlotSamples = SampleRateHz / 5; // 200 ms
	constexpr int32 PulseSamples = SampleRateHz * 14 / 100; // 140 ms
	constexpr int32 ActiveSlotCount = 4;
	constexpr int32 ActivePatternSamples = PulseSlotSamples * ActiveSlotCount;
	constexpr int32 PatternSamples = SampleRateHz * 8 / 5; // 1.6 seconds
	constexpr int32 EnvelopeSamples = SampleRateHz * 4 / 1000; // 4 ms
	constexpr float BaseAmplitude = 0.17f;
	constexpr float HarmonicAmplitude = 0.025f;

	static_assert(PulseSamples < PulseSlotSamples);
	static_assert(ActivePatternSamples < PatternSamples);

	float GetEnvelope(const int32 PulseSample)
	{
		const float Attack = FMath::Clamp(
			static_cast<float>(PulseSample + 1) / static_cast<float>(EnvelopeSamples),
			0.0f,
			1.0f);
		const float Release = FMath::Clamp(
			static_cast<float>(PulseSamples - PulseSample) / static_cast<float>(EnvelopeSamples),
			0.0f,
			1.0f);
		const float LinearEnvelope = FMath::Min(Attack, Release);

		// Smoothstep suppresses clicks without requiring state shared across callbacks.
		return LinearEnvelope * LinearEnvelope * (3.0f - 2.0f * LinearEnvelope);
	}
}

UIGAlarmSoundWave::UIGAlarmSoundWave(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bProcedural = true;
	bCanProcessAsync = true;
	bLooping = true;
	Duration = INDEFINITELY_LOOPING_DURATION;
	NumChannels = 1;
	SetSampleRate(IGAlarmSoundWave::SampleRateHz);
	SampleByteSize = sizeof(int16);
	VirtualizationMode = EVirtualizationMode::PlayWhenSilent;
	Volume = 0.7f;
}

int32 UIGAlarmSoundWave::OnGeneratePCMAudio(TArray<uint8>& OutAudio, const int32 NumSamples)
{
	if (NumSamples <= 0)
	{
		OutAudio.Reset();
		return 0;
	}

	// USoundWaveProcedural expects NumSamples signed-int16 samples here and the
	// return value in samples (not bytes). Keeping capacity avoids steady-state
	// allocations on the audio render thread.
	OutAudio.SetNumUninitialized(NumSamples * sizeof(int16), EAllowShrinking::No);
	int16* const OutputSamples = reinterpret_cast<int16*>(OutAudio.GetData());

	for (int32 OutputIndex = 0; OutputIndex < NumSamples; ++OutputIndex)
	{
		const uint64 PatternSample =
			(GeneratedSampleCount + static_cast<uint64>(OutputIndex)) %
			static_cast<uint64>(IGAlarmSoundWave::PatternSamples);

		float Output = 0.0f;
		if (PatternSample < static_cast<uint64>(IGAlarmSoundWave::ActivePatternSamples))
		{
			const int32 SlotIndex = static_cast<int32>(PatternSample) /
				IGAlarmSoundWave::PulseSlotSamples;
			const int32 PulseSample = static_cast<int32>(PatternSample) %
				IGAlarmSoundWave::PulseSlotSamples;

			if (PulseSample < IGAlarmSoundWave::PulseSamples)
			{
				const float FrequencyHz = (SlotIndex & 1) == 0 ? 880.0f : 1100.0f;
				const float Phase = 2.0f * UE_PI * FrequencyHz *
					static_cast<float>(PulseSample) /
					static_cast<float>(IGAlarmSoundWave::SampleRateHz);
				const float Envelope = IGAlarmSoundWave::GetEnvelope(PulseSample);
				Output = Envelope * (
					IGAlarmSoundWave::BaseAmplitude * FMath::Sin(Phase) +
					IGAlarmSoundWave::HarmonicAmplitude * FMath::Sin(Phase * 2.0f));
			}
		}

		OutputSamples[OutputIndex] = static_cast<int16>(
			FMath::RoundToInt(FMath::Clamp(Output, -1.0f, 1.0f) * 32767.0f));
	}

	GeneratedSampleCount += static_cast<uint64>(NumSamples);
	return NumSamples;
}
