#include "Audio/IGToneSequenceSoundWave.h"

namespace IGToneSequence
{
	constexpr int32 SampleRateHz = 48000;
	constexpr float TwoPi = 2.0f * UE_PI;

	// Equal-tempered pitches used by the composed cues.
	constexpr float NoteC3 = 130.81f;
	constexpr float NoteG2 = 98.00f;
	constexpr float NoteA2 = 110.00f;
	constexpr float NoteF3 = 174.61f;
	constexpr float NoteC4 = 261.63f;
	constexpr float NoteD4 = 293.66f;
	constexpr float NoteE4 = 329.63f;
	constexpr float NoteG4 = 392.00f;
	constexpr float NoteA4 = 440.00f;
	constexpr float NoteC5 = 523.25f;
	constexpr float NoteD5 = 587.33f;
	constexpr float NoteE5 = 659.26f;

	float HashToSigned(const uint32 Value)
	{
		uint32 Hash = Value * 2654435761u;
		Hash ^= Hash >> 16;
		Hash *= 2246822519u;
		Hash ^= Hash >> 13;
		Hash *= 3266489917u;
		Hash ^= Hash >> 16;
		return (static_cast<float>(Hash) / 2147483648.0f) - 1.0f;
	}

	UIGToneSequenceSoundWave* NewWave(UObject* Outer, const TCHAR* BaseName)
	{
		return NewObject<UIGToneSequenceSoundWave>(
			Outer,
			MakeUniqueObjectName(Outer, UIGToneSequenceSoundWave::StaticClass(), BaseName));
	}
}

UIGToneSequenceSoundWave::UIGToneSequenceSoundWave(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bProcedural = true;
	bCanProcessAsync = true;
	bLooping = false;
	NumChannels = 1;
	SetSampleRate(IGToneSequence::SampleRateHz);
	SampleByteSize = sizeof(int16);
	Volume = 1.0f;
}

void UIGToneSequenceSoundWave::ConfigureNotes(
	TArray<FIGToneNote>&& InNotes,
	const bool bInLooping,
	const float LoopSeconds)
{
	Notes = MoveTemp(InNotes);
	GeneratedSampleCount = 0;

	float LastNoteEndSeconds = 0.0f;
	for (const FIGToneNote& Note : Notes)
	{
		LastNoteEndSeconds = FMath::Max(
			LastNoteEndSeconds,
			Note.StartSeconds + Note.DurationSeconds);
	}

	bLooping = bInLooping;
	if (bInLooping)
	{
		const float SafeLoopSeconds = FMath::Max(LoopSeconds, LastNoteEndSeconds);
		LoopSampleCount = static_cast<int64>(SafeLoopSeconds * IGToneSequence::SampleRateHz);
		TotalSampleCount = 0;
		Duration = INDEFINITELY_LOOPING_DURATION;
		VirtualizationMode = EVirtualizationMode::PlayWhenSilent;
	}
	else
	{
		// A short silent tail protects the release envelope from hard cutoff.
		const float TotalSeconds = LastNoteEndSeconds + 0.05f;
		LoopSampleCount = 0;
		TotalSampleCount = static_cast<int64>(TotalSeconds * IGToneSequence::SampleRateHz);
		Duration = TotalSeconds;
	}
}

float UIGToneSequenceSoundWave::EvaluateWaveform(
	const EIGToneWaveform Waveform,
	const float FrequencyHz,
	const double NoteTimeSeconds)
{
	const double CyclesExact = NoteTimeSeconds * FrequencyHz;
	const float Phase01 = static_cast<float>(CyclesExact - FMath::FloorToDouble(CyclesExact));
	const float Radians = IGToneSequence::TwoPi * Phase01;

	switch (Waveform)
	{
	case EIGToneWaveform::Sine:
		return FMath::Sin(Radians);

	case EIGToneWaveform::SoftSquare:
		return 0.72f * FMath::Sin(Radians)
			+ 0.24f * FMath::Sin(3.0f * Radians)
			+ 0.10f * FMath::Sin(5.0f * Radians);

	case EIGToneWaveform::Triangle:
		return 2.0f * FMath::Abs(2.0f * (Phase01 - FMath::FloorToFloat(Phase01 + 0.5f))) - 1.0f;

	case EIGToneWaveform::ValueNoise:
	{
		// Interpolated hash noise; FrequencyHz sets the effective bandwidth.
		const double NoiseCursor = NoteTimeSeconds * FMath::Max(40.0f, FrequencyHz);
		const uint32 Cell = static_cast<uint32>(FMath::FloorToDouble(NoiseCursor));
		const float CellFraction = static_cast<float>(NoiseCursor - FMath::FloorToDouble(NoiseCursor));
		const float Smooth = CellFraction * CellFraction * (3.0f - 2.0f * CellFraction);
		return FMath::Lerp(
			IGToneSequence::HashToSigned(Cell),
			IGToneSequence::HashToSigned(Cell + 1u),
			Smooth);
	}

	default:
		return 0.0f;
	}
}

float UIGToneSequenceSoundWave::EvaluateEnvelope(const FIGToneNote& Note, const float NoteProgress01)
{
	const float AttackFraction = FMath::Clamp(Note.AttackFraction, 0.001f, 0.9f);
	if (NoteProgress01 <= AttackFraction)
	{
		const float Attack = NoteProgress01 / AttackFraction;
		return Attack * Attack * (3.0f - 2.0f * Attack);
	}

	const float ReleaseProgress =
		(NoteProgress01 - AttackFraction) / FMath::Max(1.0f - AttackFraction, 0.001f);
	return FMath::Pow(
		FMath::Max(0.0f, 1.0f - ReleaseProgress),
		FMath::Max(0.25f, Note.ReleasePower));
}

int32 UIGToneSequenceSoundWave::OnGeneratePCMAudio(TArray<uint8>& OutAudio, const int32 NumSamples)
{
	if (NumSamples <= 0)
	{
		OutAudio.Reset();
		return 0;
	}

	OutAudio.SetNumUninitialized(NumSamples * sizeof(int16), EAllowShrinking::No);
	int16* const OutputSamples = reinterpret_cast<int16*>(OutAudio.GetData());

	// The schedule stays tiny (a few dozen notes), so scanning every note per
	// sample is cheaper and simpler than maintaining sorted active cursors.
	for (int32 OutputIndex = 0; OutputIndex < NumSamples; ++OutputIndex)
	{
		const uint64 AbsoluteSample = GeneratedSampleCount + static_cast<uint64>(OutputIndex);

		double PatternSeconds;
		if (LoopSampleCount > 0)
		{
			PatternSeconds =
				static_cast<double>(AbsoluteSample % static_cast<uint64>(LoopSampleCount)) /
				IGToneSequence::SampleRateHz;
		}
		else
		{
			PatternSeconds = static_cast<double>(AbsoluteSample) / IGToneSequence::SampleRateHz;
			if (TotalSampleCount > 0 && AbsoluteSample >= static_cast<uint64>(TotalSampleCount))
			{
				OutputSamples[OutputIndex] = 0;
				continue;
			}
		}

		float Mixed = 0.0f;
		for (const FIGToneNote& Note : Notes)
		{
			const double NoteTime = PatternSeconds - Note.StartSeconds;
			if (NoteTime < 0.0 || NoteTime >= Note.DurationSeconds)
			{
				continue;
			}

			const float Progress = static_cast<float>(NoteTime / Note.DurationSeconds);
			Mixed += Note.Amplitude
				* EvaluateEnvelope(Note, Progress)
				* EvaluateWaveform(Note.Waveform, Note.FrequencyHz, NoteTime);
		}

		OutputSamples[OutputIndex] = static_cast<int16>(
			FMath::RoundToInt(FMath::Clamp(Mixed, -1.0f, 1.0f) * 32767.0f));
	}

	GeneratedSampleCount += static_cast<uint64>(NumSamples);
	return NumSamples;
}

UIGToneSequenceSoundWave* UIGToneSequenceSoundWave::CreateDoorChime(UObject* Outer)
{
	using namespace IGToneSequence;
	UIGToneSequenceSoundWave* Wave = NewWave(Outer, TEXT("IGDoorChime"));
	TArray<FIGToneNote> ChimeNotes;
	ChimeNotes.Add({0.00f, 0.55f, NoteE5, 0.230f, 0.008f, 2.6f, EIGToneWaveform::Sine});
	ChimeNotes.Add({0.00f, 0.45f, NoteE5 * 2.0f, 0.050f, 0.008f, 3.0f, EIGToneWaveform::Sine});
	ChimeNotes.Add({0.42f, 0.75f, NoteC5, 0.230f, 0.008f, 2.6f, EIGToneWaveform::Sine});
	ChimeNotes.Add({0.42f, 0.60f, NoteC5 * 2.0f, 0.050f, 0.008f, 3.0f, EIGToneWaveform::Sine});
	Wave->ConfigureNotes(MoveTemp(ChimeNotes), false);
	return Wave;
}

UIGToneSequenceSoundWave* UIGToneSequenceSoundWave::CreateDoorCreak(UObject* Outer)
{
	UIGToneSequenceSoundWave* Wave = IGToneSequence::NewWave(Outer, TEXT("IGDoorCreak"));
	TArray<FIGToneNote> CreakNotes;
	const float StepFrequencies[] = {338.0f, 296.0f, 318.0f, 262.0f, 228.0f, 189.0f, 161.0f};
	float StartSeconds = 0.0f;
	for (const float Frequency : StepFrequencies)
	{
		CreakNotes.Add({StartSeconds, 0.12f, Frequency, 0.048f, 0.25f, 1.2f, EIGToneWaveform::SoftSquare});
		CreakNotes.Add({StartSeconds, 0.12f, Frequency * 3.1f, 0.016f, 0.25f, 1.2f, EIGToneWaveform::ValueNoise});
		StartSeconds += 0.075f;
	}
	Wave->ConfigureNotes(MoveTemp(CreakNotes), false);
	return Wave;
}

UIGToneSequenceSoundWave* UIGToneSequenceSoundWave::CreateDoorThud(UObject* Outer)
{
	UIGToneSequenceSoundWave* Wave = IGToneSequence::NewWave(Outer, TEXT("IGDoorThud"));
	TArray<FIGToneNote> ThudNotes;
	ThudNotes.Add({0.00f, 0.13f, 74.0f, 0.320f, 0.010f, 3.0f, EIGToneWaveform::Sine});
	ThudNotes.Add({0.00f, 0.06f, 520.0f, 0.060f, 0.050f, 2.0f, EIGToneWaveform::ValueNoise});
	Wave->ConfigureNotes(MoveTemp(ThudNotes), false);
	return Wave;
}

UIGToneSequenceSoundWave* UIGToneSequenceSoundWave::CreateLockedRattle(UObject* Outer)
{
	UIGToneSequenceSoundWave* Wave = IGToneSequence::NewWave(Outer, TEXT("IGLockedRattle"));
	TArray<FIGToneNote> RattleNotes;
	RattleNotes.Add({0.00f, 0.05f, 950.0f, 0.150f, 0.05f, 1.0f, EIGToneWaveform::ValueNoise});
	RattleNotes.Add({0.09f, 0.05f, 900.0f, 0.170f, 0.05f, 1.0f, EIGToneWaveform::ValueNoise});
	RattleNotes.Add({0.18f, 0.07f, 870.0f, 0.150f, 0.05f, 1.0f, EIGToneWaveform::ValueNoise});
	RattleNotes.Add({0.18f, 0.09f, 92.0f, 0.180f, 0.02f, 2.5f, EIGToneWaveform::Sine});
	Wave->ConfigureNotes(MoveTemp(RattleNotes), false);
	return Wave;
}

UIGToneSequenceSoundWave* UIGToneSequenceSoundWave::CreateScannerBeep(UObject* Outer)
{
	UIGToneSequenceSoundWave* Wave = IGToneSequence::NewWave(Outer, TEXT("IGScannerBeep"));
	TArray<FIGToneNote> BeepNotes;
	BeepNotes.Add({0.00f, 0.085f, 2093.0f, 0.200f, 0.03f, 0.6f, EIGToneWaveform::SoftSquare});
	Wave->ConfigureNotes(MoveTemp(BeepNotes), false);
	return Wave;
}

UIGToneSequenceSoundWave* UIGToneSequenceSoundWave::CreateRegisterSound(UObject* Outer)
{
	UIGToneSequenceSoundWave* Wave = IGToneSequence::NewWave(Outer, TEXT("IGRegisterSound"));
	TArray<FIGToneNote> RegisterNotes;
	RegisterNotes.Add({0.00f, 0.07f, 1568.0f, 0.150f, 0.03f, 0.6f, EIGToneWaveform::SoftSquare});
	RegisterNotes.Add({0.12f, 0.07f, 1568.0f, 0.150f, 0.03f, 0.6f, EIGToneWaveform::SoftSquare});
	RegisterNotes.Add({0.30f, 0.22f, 680.0f, 0.110f, 0.10f, 1.0f, EIGToneWaveform::ValueNoise});
	RegisterNotes.Add({0.30f, 0.10f, 66.0f, 0.300f, 0.01f, 3.0f, EIGToneWaveform::Sine});
	RegisterNotes.Add({0.52f, 0.05f, 1400.0f, 0.100f, 0.05f, 1.0f, EIGToneWaveform::ValueNoise});
	Wave->ConfigureNotes(MoveTemp(RegisterNotes), false);
	return Wave;
}

UIGToneSequenceSoundWave* UIGToneSequenceSoundWave::CreateFluorescentBallastSnap(UObject* Outer)
{
	UIGToneSequenceSoundWave* Wave =
		IGToneSequence::NewWave(Outer, TEXT("IGFluorescentBallastSnap"));
	TArray<FIGToneNote> SnapNotes;

	// Contactor/lampholder crack: very short and spectrally broad.
	SnapNotes.Add({
		0.000f, 0.022f, 3900.0f, 0.220f,
		0.015f, 0.75f, EIGToneWaveform::ValueNoise});
	SnapNotes.Add({
		0.006f, 0.034f, 1260.0f, 0.105f,
		0.010f, 1.8f, EIGToneWaveform::SoftSquare});

	// The tube/reflector rings after power drops. The slight detune prevents
	// this from reading as a UI beep.
	SnapNotes.Add({
		0.013f, 0.190f, 119.6f, 0.105f,
		0.010f, 4.2f, EIGToneWaveform::Sine});
	SnapNotes.Add({
		0.013f, 0.145f, 241.3f, 0.042f,
		0.010f, 4.8f, EIGToneWaveform::Sine});
	SnapNotes.Add({
		0.020f, 0.090f, 720.0f, 0.026f,
		0.010f, 3.6f, EIGToneWaveform::ValueNoise});

	Wave->ConfigureNotes(MoveTemp(SnapNotes), false);
	return Wave;
}

UIGToneSequenceSoundWave* UIGToneSequenceSoundWave::CreateWaterDripMetalRing(UObject* Outer)
{
	UIGToneSequenceSoundWave* Wave =
		IGToneSequence::NewWave(Outer, TEXT("IGWaterDripMetalRing"));
	TArray<FIGToneNote> DripNotes;

	// Rounded water contact, then the lift sill answers a fraction later.
	DripNotes.Add({
		0.000f, 0.045f, 760.0f, 0.105f,
		0.015f, 2.8f, EIGToneWaveform::ValueNoise});
	DripNotes.Add({
		0.000f, 0.060f, 185.0f, 0.090f,
		0.020f, 3.2f, EIGToneWaveform::Sine});
	DripNotes.Add({
		0.020f, 0.720f, 1287.0f, 0.080f,
		0.006f, 3.9f, EIGToneWaveform::Sine});
	DripNotes.Add({
		0.020f, 0.560f, 2134.0f, 0.038f,
		0.006f, 4.6f, EIGToneWaveform::Sine});
	DripNotes.Add({
		0.028f, 0.310f, 643.0f, 0.025f,
		0.010f, 3.5f, EIGToneWaveform::Triangle});

	Wave->ConfigureNotes(MoveTemp(DripNotes), false);
	return Wave;
}

UIGToneSequenceSoundWave* UIGToneSequenceSoundWave::CreateMuffledPrayerRadio(UObject* Outer)
{
	UIGToneSequenceSoundWave* Wave =
		IGToneSequence::NewWave(Outer, TEXT("IGMuffledPrayerRadio"));
	constexpr float LoopLength = 13.6f;
	TArray<FIGToneNote> RadioNotes;

	// A tiny mains hum and cabinet noise identify a cheap radio. The pulse
	// groups below imitate sentence cadence only; no formants or intelligible
	// speech are generated.
	RadioNotes.Add({
		0.0f, LoopLength, 59.8f, 0.010f,
		0.25f, 0.7f, EIGToneWaveform::Sine});
	RadioNotes.Add({
		0.0f, LoopLength, 92.0f, 0.006f,
		0.25f, 0.7f, EIGToneWaveform::ValueNoise});

	struct FMurmurPulse
	{
		float Start;
		float Duration;
		float BaseHz;
		float Level;
	};
	const FMurmurPulse Pulses[] = {
		{0.45f, 0.82f, 151.0f, 0.034f},
		{1.38f, 0.55f, 172.0f, 0.030f},
		{2.10f, 1.08f, 143.0f, 0.036f},
		{3.72f, 0.68f, 166.0f, 0.029f},
		{4.55f, 1.32f, 148.0f, 0.035f},
		{6.95f, 0.72f, 158.0f, 0.031f},
		{7.82f, 0.46f, 181.0f, 0.027f},
		{8.48f, 1.18f, 146.0f, 0.035f},
		{10.20f, 0.62f, 169.0f, 0.030f},
		{10.98f, 1.50f, 142.0f, 0.034f},
	};
	for (int32 PulseIndex = 0;
		PulseIndex < static_cast<int32>(UE_ARRAY_COUNT(Pulses));
		++PulseIndex)
	{
		const FMurmurPulse& Pulse = Pulses[PulseIndex];
		RadioNotes.Add({
			Pulse.Start,
			Pulse.Duration,
			Pulse.BaseHz,
			Pulse.Level,
			0.18f,
			1.7f,
			EIGToneWaveform::SoftSquare});
		RadioNotes.Add({
			Pulse.Start + 0.025f,
			Pulse.Duration * 0.92f,
			Pulse.BaseHz * (1.43f + (PulseIndex % 3) * 0.035f),
			Pulse.Level * 0.33f,
			0.22f,
			1.9f,
			EIGToneWaveform::ValueNoise});
	}

	Wave->ConfigureNotes(MoveTemp(RadioNotes), true, LoopLength);
	return Wave;
}

UIGToneSequenceSoundWave* UIGToneSequenceSoundWave::CreateCardboardDrag(UObject* Outer)
{
	UIGToneSequenceSoundWave* Wave =
		IGToneSequence::NewWave(Outer, TEXT("IGCardboardDrag"));
	TArray<FIGToneNote> DragNotes;

	// Broad but low-energy friction with two changes of pressure.
	DragNotes.Add({
		0.000f, 0.92f, 310.0f, 0.110f,
		0.12f, 1.45f, EIGToneWaveform::ValueNoise});
	DragNotes.Add({
		0.130f, 0.54f, 118.0f, 0.052f,
		0.18f, 1.6f, EIGToneWaveform::Triangle});
	DragNotes.Add({
		0.460f, 0.38f, 520.0f, 0.048f,
		0.08f, 1.8f, EIGToneWaveform::ValueNoise});

	// The box is set down, not thrown.
	DragNotes.Add({
		0.940f, 0.105f, 67.0f, 0.190f,
		0.015f, 3.2f, EIGToneWaveform::Sine});
	DragNotes.Add({
		0.940f, 0.055f, 430.0f, 0.060f,
		0.025f, 2.2f, EIGToneWaveform::ValueNoise});

	Wave->ConfigureNotes(MoveTemp(DragNotes), false);
	return Wave;
}

UIGToneSequenceSoundWave* UIGToneSequenceSoundWave::CreateStoreJingle(
	UObject* Outer,
	const float PitchSemitones,
	const float TimeScale)
{
	using namespace IGToneSequence;
	UIGToneSequenceSoundWave* Wave = NewWave(Outer, TEXT("IGStoreJingle"));

	const float SafeTimeScale = FMath::Clamp(TimeScale, 0.5f, 2.0f);
	const float PitchRatio = FMath::Pow(2.0f, PitchSemitones / 12.0f);
	const float Beat = 0.54f * SafeTimeScale; // CH01 ~111 BPM; CH02 12% slower.
	TArray<FIGToneNote> JingleNotes;

	struct FMelodyStep
	{
		float Frequency;
		float StartBeat;
		float Beats;
	};

	// Original cheerful music-box melody: bright, simple and endlessly repeating.
	const FMelodyStep Melody[] = {
		{NoteE4, 0.0f, 1.0f}, {NoteG4, 1.0f, 1.0f}, {NoteA4, 2.0f, 1.0f}, {NoteC5, 3.0f, 1.0f},
		{NoteA4, 4.0f, 1.0f}, {NoteG4, 5.0f, 1.0f}, {NoteE4, 6.0f, 2.0f},
		{NoteG4, 8.0f, 1.0f}, {NoteA4, 9.0f, 1.0f}, {NoteC5, 10.0f, 1.0f}, {NoteD5, 11.0f, 1.0f},
		{NoteC5, 12.0f, 2.0f}, {NoteA4, 14.0f, 2.0f},
		{NoteE5, 16.0f, 1.0f}, {NoteD5, 17.0f, 1.0f}, {NoteC5, 18.0f, 1.0f}, {NoteA4, 19.0f, 1.0f},
		{NoteG4, 20.0f, 1.0f}, {NoteA4, 21.0f, 1.0f}, {NoteC5, 22.0f, 1.0f}, {NoteA4, 23.0f, 1.0f},
		{NoteG4, 24.0f, 1.0f}, {NoteE4, 25.0f, 1.0f}, {NoteD4, 26.0f, 1.0f}, {NoteE4, 27.0f, 1.0f},
		{NoteC4, 28.0f, 3.0f},
	};

	for (const FMelodyStep& Step : Melody)
	{
		const float Start = Step.StartBeat * Beat;
		const float NoteLength = Step.Beats * Beat * 0.85f;
		JingleNotes.Add({
			Start, NoteLength, Step.Frequency * PitchRatio,
			0.105f, 0.02f, 2.2f, EIGToneWaveform::Sine});
		JingleNotes.Add({
			Start, NoteLength * 0.8f, Step.Frequency * 2.0f * PitchRatio,
			0.028f, 0.02f, 2.6f, EIGToneWaveform::Sine});
	}

	// Root bass every two beats: C / Am / F / C, then Am / F / G / C.
	const float BassLine[] = {
		NoteC3, NoteC3, NoteA2, NoteA2, NoteF3, NoteF3, NoteC3, NoteC3,
		NoteA2, NoteA2, NoteF3, NoteF3, NoteG2, NoteG2, NoteC3, NoteC3,
	};
	for (int32 BassIndex = 0; BassIndex < static_cast<int32>(UE_ARRAY_COUNT(BassLine)); ++BassIndex)
	{
		JingleNotes.Add({
			BassIndex * 2.0f * Beat,
			2.0f * Beat * 0.9f,
			BassLine[BassIndex] * PitchRatio,
			0.050f,
			0.04f,
			1.4f,
			EIGToneWaveform::Triangle});
	}

	Wave->ConfigureNotes(MoveTemp(JingleNotes), true, 32.0f * Beat);
	return Wave;
}

UIGToneSequenceSoundWave* UIGToneSequenceSoundWave::CreateDreadDrone(UObject* Outer)
{
	using namespace IGToneSequence;
	UIGToneSequenceSoundWave* Wave = NewWave(Outer, TEXT("IGDreadDrone"));

	constexpr float LoopLength = 24.0f;
	TArray<FIGToneNote> DroneNotes;
	DroneNotes.Add({0.0f, LoopLength, 55.00f, 0.050f, 0.40f, 0.8f, EIGToneWaveform::Sine});
	DroneNotes.Add({0.0f, LoopLength, NoteA2, 0.040f, 0.30f, 0.8f, EIGToneWaveform::Sine});
	DroneNotes.Add({0.0f, LoopLength, NoteA2 + 0.7f, 0.034f, 0.34f, 0.8f, EIGToneWaveform::Sine});
	DroneNotes.Add({0.0f, LoopLength, NoteC3, 0.026f, 0.36f, 0.8f, EIGToneWaveform::Sine});
	DroneNotes.Add({0.0f, LoopLength, NoteC3 + 0.9f, 0.022f, 0.30f, 0.8f, EIGToneWaveform::Sine});
	DroneNotes.Add({0.0f, LoopLength, 163.50f, 0.015f, 0.42f, 0.8f, EIGToneWaveform::Sine});
	// A dissonant minor second slides in mid-loop, then recedes.
	DroneNotes.Add({10.0f, 12.0f, 116.54f, 0.013f, 0.45f, 1.2f, EIGToneWaveform::Sine});
	// A barely audible high whistle keeps the ear searching for a source.
	DroneNotes.Add({6.0f, 14.0f, NoteE5, 0.005f, 0.48f, 1.5f, EIGToneWaveform::Sine});
	Wave->ConfigureNotes(MoveTemp(DroneNotes), true, LoopLength);
	return Wave;
}

UIGToneSequenceSoundWave* UIGToneSequenceSoundWave::CreateFootstep(
	UObject* Outer,
	const float PitchScale,
	const float Amplitude)
{
	UIGToneSequenceSoundWave* Wave = IGToneSequence::NewWave(Outer, TEXT("IGFootstep"));
	const float SafePitch = FMath::Clamp(PitchScale, 0.5f, 2.0f);
	const float SafeAmplitude = FMath::Clamp(Amplitude, 0.0f, 1.0f);
	TArray<FIGToneNote> StepNotes;
	StepNotes.Add({0.00f, 0.085f, 58.0f * SafePitch, 0.50f * SafeAmplitude, 0.010f, 3.0f, EIGToneWaveform::Sine});
	StepNotes.Add({0.00f, 0.050f, 420.0f * SafePitch, 0.22f * SafeAmplitude, 0.060f, 2.0f, EIGToneWaveform::ValueNoise});
	Wave->ConfigureNotes(MoveTemp(StepNotes), false);
	return Wave;
}
