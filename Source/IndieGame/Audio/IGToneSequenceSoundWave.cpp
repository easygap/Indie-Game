#include "Audio/IGToneSequenceSoundWave.h"

#include "Narrative/IGRecordingSubsystem.h"

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

void UIGToneSequenceSoundWave::ConfigurePitchWow(
	const float DepthRatio,
	const float RateHz)
{
	PitchWowDepthRatio = FMath::Clamp(DepthRatio, 0.0f, 0.02f);
	PitchWowRateHz = FMath::Clamp(RateHz, 0.0f, 4.0f);
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
			double WaveTime = NoteTime;
			if (PitchWowDepthRatio > 0.0f && PitchWowRateHz > 0.0f)
			{
				// Integrate 1 + depth*sin(wt) so wow changes instantaneous pitch
				// without the discontinuities caused by multiplying phase directly.
				const double AngularRate =
					IGToneSequence::TwoPi * PitchWowRateHz;
				const double PatternOffset =
					PitchWowDepthRatio / AngularRate
					* (1.0 - FMath::Cos(AngularRate * PatternSeconds));
				const double NoteStartOffset =
					PitchWowDepthRatio / AngularRate
					* (1.0 - FMath::Cos(AngularRate * Note.StartSeconds));
				WaveTime += PatternOffset - NoteStartOffset;
			}
			Mixed += Note.Amplitude
				* EvaluateEnvelope(Note, Progress)
				* EvaluateWaveform(Note.Waveform, Note.FrequencyHz, WaveTime);
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

UIGToneSequenceSoundWave* UIGToneSequenceSoundWave::CreateJournalPageTurn(UObject* Outer)
{
	UIGToneSequenceSoundWave* Wave =
		IGToneSequence::NewWave(Outer, TEXT("IGJournalPageTurn"));
	TArray<FIGToneNote> PaperNotes;

	// A quiet, dry lift followed by two fingertip brushes. The cue deliberately
	// avoids a tonal UI click: the journal is a physical object in the room,
	// even though PlaySound2D marks this one-shot as pause-safe UI audio.
	PaperNotes.Add({
		0.000f, 0.170f, 3400.0f, 0.045f,
		0.08f, 1.8f, EIGToneWaveform::ValueNoise});
	PaperNotes.Add({
		0.025f, 0.110f, 620.0f, 0.028f,
		0.10f, 2.1f, EIGToneWaveform::ValueNoise});
	PaperNotes.Add({
		0.145f, 0.075f, 2100.0f, 0.038f,
		0.05f, 2.4f, EIGToneWaveform::ValueNoise});
	PaperNotes.Add({
		0.205f, 0.050f, 1250.0f, 0.024f,
		0.04f, 2.8f, EIGToneWaveform::ValueNoise});

	Wave->ConfigureNotes(MoveTemp(PaperNotes), false);
	return Wave;
}

UIGToneSequenceSoundWave* UIGToneSequenceSoundWave::CreatePaperDoorSlide(
	UObject* Outer)
{
	UIGToneSequenceSoundWave* Wave =
		IGToneSequence::NewWave(Outer, TEXT("IGPaperDoorSlide"));
	TArray<FIGToneNote> PaperNotes;

	// 첫 레이어는 문풍지에 스치는 종이, 두 번째는 문틈을 벗어난 뒤의
	// 넓은 타일 마찰음이다. 마지막 두 번의 짧은 충격음으로 아이템 획득이
	// 아니라 가벼운 종이 한 장이 바닥에 안착했다는 느낌을 준다.
	PaperNotes.Add({
		0.000f, 0.34f, 2850.0f, 0.036f,
		0.10f, 1.55f, EIGToneWaveform::ValueNoise});
	PaperNotes.Add({
		0.110f, 0.58f, 930.0f, 0.052f,
		0.14f, 1.72f, EIGToneWaveform::ValueNoise});
	PaperNotes.Add({
		0.290f, 0.42f, 310.0f, 0.023f,
		0.18f, 1.95f, EIGToneWaveform::ValueNoise});
	PaperNotes.Add({
		0.700f, 0.055f, 1800.0f, 0.026f,
		0.04f, 2.8f, EIGToneWaveform::ValueNoise});
	PaperNotes.Add({
		0.748f, 0.040f, 720.0f, 0.018f,
		0.03f, 3.1f, EIGToneWaveform::ValueNoise});

	Wave->ConfigureNotes(MoveTemp(PaperNotes), false);
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
	constexpr float BaseBeatSeconds = 60.0f / 76.0f;
	const float Beat = BaseBeatSeconds * SafeTimeScale;
	const bool bDegraded =
		!FMath::IsNearlyZero(PitchSemitones)
		|| !FMath::IsNearlyEqual(SafeTimeScale, 1.0f);
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

	for (int32 StepIndex = 0;
		StepIndex < static_cast<int32>(UE_ARRAY_COUNT(Melody));
		++StepIndex)
	{
		// M1b leaves one expected mallet strike empty; the accompaniment keeps
		// moving, making the absence register before the player names it.
		if (bDegraded && StepIndex == 18)
		{
			continue;
		}
		const FMelodyStep& Step = Melody[StepIndex];
		const float Start = Step.StartBeat * Beat;
		const float NoteLength = Step.Beats * Beat * 0.85f;
		JingleNotes.Add({
			Start, NoteLength, Step.Frequency * PitchRatio,
			0.082f, 0.02f, 2.2f, EIGToneWaveform::Triangle});
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
			EIGToneWaveform::SoftSquare});
	}

	Wave->ConfigureNotes(MoveTemp(JingleNotes), true, 32.0f * Beat);
	if (bDegraded)
	{
		Wave->ConfigurePitchWow(0.003f, 0.30f);
	}
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

UIGToneSequenceSoundWave*
UIGToneSequenceSoundWave::CreateFloodedCorridorWaterBed(UObject* Outer)
{
	UIGToneSequenceSoundWave* Wave =
		IGToneSequence::NewWave(Outer, TEXT("IGFloodedCorridorWaterBed"));
	TArray<FIGToneNote> Notes;
	Notes.Add({0.0f, 9.0f, 96.0f, 0.025f, 0.20f, 0.8f, EIGToneWaveform::ValueNoise});
	Notes.Add({0.0f, 9.0f, 52.0f, 0.018f, 0.25f, 0.8f, EIGToneWaveform::Sine});
	Notes.Add({1.2f, 0.24f, 820.0f, 0.065f, 0.02f, 2.8f, EIGToneWaveform::ValueNoise});
	Notes.Add({4.7f, 0.36f, 1280.0f, 0.050f, 0.02f, 3.2f, EIGToneWaveform::Sine});
	Notes.Add({7.4f, 0.20f, 610.0f, 0.052f, 0.02f, 2.6f, EIGToneWaveform::ValueNoise});
	Wave->ConfigureNotes(MoveTemp(Notes), true, 9.0f);
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

UIGToneSequenceSoundWave* UIGToneSequenceSoundWave::CreateAlarmFirstNote(UObject* Outer)
{
	UIGToneSequenceSoundWave* Wave =
		IGToneSequence::NewWave(Outer, TEXT("IGAlarmFirstNote"));
	TArray<FIGToneNote> AlarmNotes;

	// One 140 ms pulse matching UIGAlarmSoundWave's first slot (880 Hz plus
	// its quiet harmonic), so the intercepted call reads as the same clock.
	AlarmNotes.Add({0.000f, 0.140f, 880.0f, 0.170f, 0.030f, 1.1f, EIGToneWaveform::Sine});
	AlarmNotes.Add({0.000f, 0.140f, 2640.0f, 0.025f, 0.030f, 1.1f, EIGToneWaveform::Sine});

	Wave->ConfigureNotes(MoveTemp(AlarmNotes), false);
	return Wave;
}

UIGToneSequenceSoundWave* UIGToneSequenceSoundWave::CreateCallFailTone(UObject* Outer)
{
	UIGToneSequenceSoundWave* Wave =
		IGToneSequence::NewWave(Outer, TEXT("IGCallFailTone"));
	TArray<FIGToneNote> FailNotes;

	// Flat handset beeps stepping down: the network refusing, not a jump cue.
	FailNotes.Add({0.000f, 0.180f, 425.0f, 0.085f, 0.040f, 1.4f, EIGToneWaveform::SoftSquare});
	FailNotes.Add({0.260f, 0.240f, 355.0f, 0.075f, 0.040f, 1.8f, EIGToneWaveform::SoftSquare});

	Wave->ConfigureNotes(MoveTemp(FailNotes), false);
	return Wave;
}

UIGToneSequenceSoundWave* UIGToneSequenceSoundWave::CreateDoorbellChime(UObject* Outer)
{
	using namespace IGToneSequence;
	UIGToneSequenceSoundWave* Wave =
		IGToneSequence::NewWave(Outer, TEXT("IGDoorbellChime"));
	TArray<FIGToneNote> BellNotes;

	// Rounder and lower than the store chime; it must sound like it belongs
	// to somebody's hallway, heard from outside their door.
	BellNotes.Add({0.000f, 0.700f, NoteE4, 0.085f, 0.010f, 2.6f, EIGToneWaveform::Sine});
	BellNotes.Add({0.000f, 0.520f, NoteE4 * 2.0f, 0.022f, 0.010f, 3.0f, EIGToneWaveform::Sine});
	BellNotes.Add({0.420f, 0.980f, NoteC4, 0.080f, 0.012f, 2.2f, EIGToneWaveform::Sine});
	BellNotes.Add({0.420f, 0.700f, NoteC4 * 2.0f, 0.020f, 0.012f, 2.6f, EIGToneWaveform::Sine});

	Wave->ConfigureNotes(MoveTemp(BellNotes), false);
	return Wave;
}

UIGToneSequenceSoundWave* UIGToneSequenceSoundWave::CreateRelayClick(UObject* Outer)
{
	UIGToneSequenceSoundWave* Wave =
		IGToneSequence::NewWave(Outer, TEXT("IGRelayClick"));
	TArray<FIGToneNote> ClickNotes;

	ClickNotes.Add({0.000f, 0.022f, 2400.0f, 0.100f, 0.005f, 4.0f, EIGToneWaveform::ValueNoise});
	ClickNotes.Add({0.000f, 0.045f, 300.0f, 0.055f, 0.005f, 3.6f, EIGToneWaveform::Sine});

	Wave->ConfigureNotes(MoveTemp(ClickNotes), false);
	return Wave;
}

UIGToneSequenceSoundWave* UIGToneSequenceSoundWave::CreateBottleCapOpen(UObject* Outer)
{
	UIGToneSequenceSoundWave* Wave =
		IGToneSequence::NewWave(Outer, TEXT("IGBottleCapOpen"));
	TArray<FIGToneNote> CapNotes;

	// Security-ring bridges snap in a fast cluster, then the threads rasp.
	CapNotes.Add({0.000f, 0.030f, 3100.0f, 0.085f, 0.005f, 4.0f, EIGToneWaveform::ValueNoise});
	CapNotes.Add({0.045f, 0.026f, 3400.0f, 0.075f, 0.005f, 4.0f, EIGToneWaveform::ValueNoise});
	CapNotes.Add({0.082f, 0.024f, 2900.0f, 0.065f, 0.005f, 4.0f, EIGToneWaveform::ValueNoise});
	CapNotes.Add({0.130f, 0.180f, 1600.0f, 0.038f, 0.060f, 2.4f, EIGToneWaveform::ValueNoise});
	// Air equalizing through the fresh opening.
	CapNotes.Add({0.320f, 0.090f, 900.0f, 0.020f, 0.200f, 2.0f, EIGToneWaveform::ValueNoise});

	Wave->ConfigureNotes(MoveTemp(CapNotes), false);
	return Wave;
}

UIGToneSequenceSoundWave* UIGToneSequenceSoundWave::CreateWaterSwallows(UObject* Outer)
{
	UIGToneSequenceSoundWave* Wave =
		IGToneSequence::NewWave(Outer, TEXT("IGWaterSwallows"));
	TArray<FIGToneNote> DrinkNotes;

	// Bottle glug and throat answer, three times, slowing slightly. Kept low
	// and short: this is a screen-bottom action, not a close-mic ASMR cue.
	const float SwallowStarts[] = {0.00f, 0.62f, 1.30f};
	for (int32 SwallowIndex = 0; SwallowIndex < 3; ++SwallowIndex)
	{
		const float Start = SwallowStarts[SwallowIndex];
		DrinkNotes.Add({
			Start, 0.110f, 240.0f - SwallowIndex * 18.0f, 0.055f,
			0.100f, 2.2f, EIGToneWaveform::Sine});
		DrinkNotes.Add({
			Start + 0.020f, 0.080f, 620.0f, 0.030f,
			0.080f, 2.6f, EIGToneWaveform::ValueNoise});
		DrinkNotes.Add({
			Start + 0.150f, 0.070f, 150.0f, 0.040f,
			0.060f, 2.8f, EIGToneWaveform::Sine});
	}

	Wave->ConfigureNotes(MoveTemp(DrinkNotes), false);
	return Wave;
}

UIGToneSequenceSoundWave* UIGToneSequenceSoundWave::CreateBottleReseal(UObject* Outer)
{
	UIGToneSequenceSoundWave* Wave =
		IGToneSequence::NewWave(Outer, TEXT("IGBottleReseal"));
	TArray<FIGToneNote> ResealNotes;

	// One continuous ratchet down the threads, ending in a firm stop.
	for (int32 ClickIndex = 0; ClickIndex < 5; ++ClickIndex)
	{
		ResealNotes.Add({
			ClickIndex * 0.055f, 0.020f, 2500.0f + ClickIndex * 120.0f,
			0.045f, 0.005f, 3.6f, EIGToneWaveform::ValueNoise});
	}
	ResealNotes.Add({0.300f, 0.060f, 800.0f, 0.045f, 0.010f, 3.2f, EIGToneWaveform::ValueNoise});

	Wave->ConfigureNotes(MoveTemp(ResealNotes), false);
	return Wave;
}

UIGToneSequenceSoundWave* UIGToneSequenceSoundWave::CreatePlasticBagSetDown(UObject* Outer)
{
	UIGToneSequenceSoundWave* Wave =
		IGToneSequence::NewWave(Outer, TEXT("IGPlasticBagSetDown"));
	TArray<FIGToneNote> BagNotes;

	// Film crinkle spreading as the load transfers, then bottles knock once.
	BagNotes.Add({0.000f, 0.240f, 3800.0f, 0.050f, 0.100f, 2.0f, EIGToneWaveform::ValueNoise});
	BagNotes.Add({0.060f, 0.180f, 2500.0f, 0.040f, 0.120f, 2.2f, EIGToneWaveform::ValueNoise});
	BagNotes.Add({0.190f, 0.070f, 210.0f, 0.060f, 0.010f, 3.0f, EIGToneWaveform::Sine});
	BagNotes.Add({0.240f, 0.055f, 260.0f, 0.038f, 0.010f, 3.2f, EIGToneWaveform::Sine});

	Wave->ConfigureNotes(MoveTemp(BagNotes), false);
	return Wave;
}

UIGToneSequenceSoundWave* UIGToneSequenceSoundWave::CreatePlasticBagLift(UObject* Outer)
{
	UIGToneSequenceSoundWave* Wave =
		IGToneSequence::NewWave(Outer, TEXT("IGPlasticBagLift"));
	TArray<FIGToneNote> BagNotes;

	// Handles stretching taut first, then the shorter gather crinkle.
	BagNotes.Add({0.000f, 0.140f, 1400.0f, 0.030f, 0.300f, 1.8f, EIGToneWaveform::ValueNoise});
	BagNotes.Add({0.090f, 0.190f, 3300.0f, 0.045f, 0.120f, 2.2f, EIGToneWaveform::ValueNoise});
	BagNotes.Add({0.210f, 0.045f, 290.0f, 0.030f, 0.010f, 3.2f, EIGToneWaveform::Sine});

	Wave->ConfigureNotes(MoveTemp(BagNotes), false);
	return Wave;
}

UIGToneSequenceSoundWave* UIGToneSequenceSoundWave::CreateClothSettle(UObject* Outer)
{
	UIGToneSequenceSoundWave* Wave =
		IGToneSequence::NewWave(Outer, TEXT("IGClothSettle"));
	TArray<FIGToneNote> ClothNotes;

	// A single sinking weight: broadband cloth friction that decays without
	// any rhythm. Deliberately nothing like breathing.
	ClothNotes.Add({0.000f, 0.460f, 1200.0f, 0.038f, 0.180f, 2.6f, EIGToneWaveform::ValueNoise});
	ClothNotes.Add({0.050f, 0.360f, 500.0f, 0.030f, 0.220f, 2.8f, EIGToneWaveform::ValueNoise});
	ClothNotes.Add({0.260f, 0.180f, 90.0f, 0.026f, 0.150f, 3.0f, EIGToneWaveform::Sine});

	Wave->ConfigureNotes(MoveTemp(ClothNotes), false);
	return Wave;
}

UIGToneSequenceSoundWave* UIGToneSequenceSoundWave::CreateShutterMotorStep(UObject* Outer)
{
	UIGToneSequenceSoundWave* Wave =
		IGToneSequence::NewWave(Outer, TEXT("IGShutterMotorStep"));
	TArray<FIGToneNote> MotorNotes;

	// Gear motor under load with slat chatter, one short descent step.
	MotorNotes.Add({0.000f, 0.850f, 95.0f, 0.070f, 0.060f, 1.6f, EIGToneWaveform::SoftSquare});
	MotorNotes.Add({0.000f, 0.850f, 190.0f, 0.030f, 0.060f, 1.6f, EIGToneWaveform::SoftSquare});
	MotorNotes.Add({0.080f, 0.700f, 1500.0f, 0.022f, 0.100f, 2.0f, EIGToneWaveform::ValueNoise});
	MotorNotes.Add({0.870f, 0.090f, 320.0f, 0.055f, 0.010f, 3.0f, EIGToneWaveform::Sine});

	Wave->ConfigureNotes(MoveTemp(MotorNotes), false);
	return Wave;
}

UIGToneSequenceSoundWave* UIGToneSequenceSoundWave::CreateShutterMotorRise(UObject* Outer)
{
	UIGToneSequenceSoundWave* Wave =
		IGToneSequence::NewWave(Outer, TEXT("IGShutterMotorRise"));
	TArray<FIGToneNote> MotorNotes;

	// Longer unloaded run, pitch easing up, ending on the top end stop.
	MotorNotes.Add({0.000f, 2.300f, 105.0f, 0.060f, 0.100f, 1.2f, EIGToneWaveform::SoftSquare});
	MotorNotes.Add({0.000f, 2.300f, 212.0f, 0.024f, 0.100f, 1.2f, EIGToneWaveform::SoftSquare});
	MotorNotes.Add({0.150f, 2.000f, 1400.0f, 0.016f, 0.150f, 1.6f, EIGToneWaveform::ValueNoise});
	MotorNotes.Add({2.320f, 0.120f, 480.0f, 0.050f, 0.010f, 3.0f, EIGToneWaveform::Sine});

	Wave->ConfigureNotes(MoveTemp(MotorNotes), false);
	return Wave;
}

UIGToneSequenceSoundWave* UIGToneSequenceSoundWave::CreateThermalPrinterFeed(UObject* Outer)
{
	UIGToneSequenceSoundWave* Wave =
		IGToneSequence::NewWave(Outer, TEXT("IGThermalPrinterFeed"));
	TArray<FIGToneNote> FeedNotes;

	// Stepper whirr plus paper hiss; ends with the cutter's soft chop.
	FeedNotes.Add({0.000f, 0.520f, 780.0f, 0.045f, 0.030f, 1.6f, EIGToneWaveform::SoftSquare});
	FeedNotes.Add({0.000f, 0.520f, 3100.0f, 0.018f, 0.080f, 1.8f, EIGToneWaveform::ValueNoise});
	FeedNotes.Add({0.560f, 0.070f, 1100.0f, 0.050f, 0.008f, 3.4f, EIGToneWaveform::ValueNoise});

	Wave->ConfigureNotes(MoveTemp(FeedNotes), false);
	return Wave;
}

UIGToneSequenceSoundWave* UIGToneSequenceSoundWave::CreateJingleOpeningNotes(UObject* Outer)
{
	using namespace IGToneSequence;
	UIGToneSequenceSoundWave* Wave =
		IGToneSequence::NewWave(Outer, TEXT("IGJingleOpeningNotes"));
	TArray<FIGToneNote> JingleNotes;

	// Exactly the healthy CH01 jingle's first two melody notes (E4 then G4 at
	// the original 0.54 s beat), including their music-box partials. Nothing
	// follows: the store answers the correct time with one healthy breath.
	constexpr float Beat = 0.54f;
	const float Melody[2] = {NoteE4, NoteG4};
	for (int32 NoteIndex = 0; NoteIndex < 2; ++NoteIndex)
	{
		const float Start = NoteIndex * Beat;
		const float NoteLength = Beat * 0.85f;
		JingleNotes.Add({
			Start, NoteLength, Melody[NoteIndex],
			0.105f, 0.02f, 2.2f, EIGToneWaveform::Sine});
		JingleNotes.Add({
			Start, NoteLength * 0.8f, Melody[NoteIndex] * 2.0f,
			0.030f, 0.02f, 2.8f, EIGToneWaveform::Sine});
	}

	Wave->ConfigureNotes(MoveTemp(JingleNotes), false);
	return Wave;
}

UIGToneSequenceSoundWave* UIGToneSequenceSoundWave::CreateGasDetectorOk(UObject* Outer)
{
	UIGToneSequenceSoundWave* Wave =
		IGToneSequence::NewWave(Outer, TEXT("IGGasDetectorOk"));
	TArray<FIGToneNote> DetectorNotes;

	// Two clean instrument passes and the shorter confirm chirp. Clinical,
	// quiet, and over quickly: machines are calm about this roof.
	DetectorNotes.Add({0.000f, 0.090f, 1900.0f, 0.060f, 0.020f, 2.0f, EIGToneWaveform::SoftSquare});
	DetectorNotes.Add({0.240f, 0.090f, 1900.0f, 0.060f, 0.020f, 2.0f, EIGToneWaveform::SoftSquare});
	DetectorNotes.Add({0.640f, 0.055f, 2533.0f, 0.050f, 0.020f, 2.4f, EIGToneWaveform::SoftSquare});

	Wave->ConfigureNotes(MoveTemp(DetectorNotes), false);
	return Wave;
}

UIGToneSequenceSoundWave* UIGToneSequenceSoundWave::CreateVentDuctSpinUp(UObject* Outer)
{
	UIGToneSequenceSoundWave* Wave =
		IGToneSequence::NewWave(Outer, TEXT("IGVentDuctSpinUp"));
	TArray<FIGToneNote> DuctNotes;

	// Flexible duct sections unfolding in three pulls...
	DuctNotes.Add({0.000f, 0.260f, 900.0f, 0.045f, 0.080f, 2.2f, EIGToneWaveform::ValueNoise});
	DuctNotes.Add({0.320f, 0.220f, 1150.0f, 0.040f, 0.080f, 2.2f, EIGToneWaveform::ValueNoise});
	DuctNotes.Add({0.600f, 0.240f, 800.0f, 0.038f, 0.080f, 2.4f, EIGToneWaveform::ValueNoise});
	// ...then the fan comes up and holds a steady low idle.
	DuctNotes.Add({0.900f, 1.700f, 68.0f, 0.055f, 0.350f, 0.9f, EIGToneWaveform::SoftSquare});
	DuctNotes.Add({0.900f, 1.700f, 136.0f, 0.022f, 0.350f, 0.9f, EIGToneWaveform::SoftSquare});
	DuctNotes.Add({1.100f, 1.500f, 400.0f, 0.014f, 0.300f, 1.0f, EIGToneWaveform::ValueNoise});

	Wave->ConfigureNotes(MoveTemp(DuctNotes), false);
	return Wave;
}

UIGToneSequenceSoundWave* UIGToneSequenceSoundWave::CreateHarnessBuckle(UObject* Outer)
{
	UIGToneSequenceSoundWave* Wave =
		IGToneSequence::NewWave(Outer, TEXT("IGHarnessBuckle"));
	TArray<FIGToneNote> BuckleNotes;

	// Buckle tongue seating, carabiner gate snapping, webbing pulled tight.
	BuckleNotes.Add({0.000f, 0.040f, 2100.0f, 0.080f, 0.005f, 3.6f, EIGToneWaveform::ValueNoise});
	BuckleNotes.Add({0.055f, 0.070f, 950.0f, 0.045f, 0.010f, 3.0f, EIGToneWaveform::Sine});
	BuckleNotes.Add({0.420f, 0.030f, 3200.0f, 0.075f, 0.005f, 4.0f, EIGToneWaveform::ValueNoise});
	BuckleNotes.Add({0.460f, 0.120f, 1450.0f, 0.030f, 0.010f, 3.4f, EIGToneWaveform::Sine});
	BuckleNotes.Add({0.700f, 0.300f, 700.0f, 0.028f, 0.150f, 2.2f, EIGToneWaveform::ValueNoise});

	Wave->ConfigureNotes(MoveTemp(BuckleNotes), false);
	return Wave;
}

UIGToneSequenceSoundWave* UIGToneSequenceSoundWave::CreateLadderClimbTwoPeople(UObject* Outer)
{
	UIGToneSequenceSoundWave* Wave =
		IGToneSequence::NewWave(Outer, TEXT("IGLadderClimbTwoPeople"));
	TArray<FIGToneNote> ClimbNotes;

	// Person one: heavier boots, slower cadence, deeper rung answer.
	for (int32 StepIndex = 0; StepIndex < 5; ++StepIndex)
	{
		const float Start = StepIndex * 0.560f;
		ClimbNotes.Add({
			Start, 0.070f, 180.0f, 0.070f, 0.008f, 3.0f, EIGToneWaveform::Sine});
		ClimbNotes.Add({
			Start + 0.010f, 0.240f, 620.0f, 0.026f, 0.010f, 3.6f, EIGToneWaveform::Sine});
	}
	// Person two: lighter, quicker, starting later and ringing higher.
	for (int32 StepIndex = 0; StepIndex < 6; ++StepIndex)
	{
		const float Start = 0.900f + StepIndex * 0.410f;
		ClimbNotes.Add({
			Start, 0.050f, 240.0f, 0.048f, 0.008f, 3.2f, EIGToneWaveform::Sine});
		ClimbNotes.Add({
			Start + 0.008f, 0.180f, 840.0f, 0.020f, 0.010f, 3.8f, EIGToneWaveform::Sine});
	}

	Wave->ConfigureNotes(MoveTemp(ClimbNotes), false);
	return Wave;
}

UIGToneSequenceSoundWave* UIGToneSequenceSoundWave::CreateHatchOpenMetal(UObject* Outer)
{
	UIGToneSequenceSoundWave* Wave =
		IGToneSequence::NewWave(Outer, TEXT("IGHatchOpenMetal"));
	TArray<FIGToneNote> HatchNotes;

	// Latch turned, the heavy lid swept up on real hinges, then set to rest.
	HatchNotes.Add({0.000f, 0.090f, 1300.0f, 0.070f, 0.010f, 3.2f, EIGToneWaveform::ValueNoise});
	HatchNotes.Add({0.180f, 0.850f, 140.0f, 0.060f, 0.200f, 1.4f, EIGToneWaveform::SoftSquare});
	HatchNotes.Add({0.180f, 0.850f, 340.0f, 0.026f, 0.200f, 1.4f, EIGToneWaveform::ValueNoise});
	HatchNotes.Add({1.120f, 0.140f, 210.0f, 0.075f, 0.008f, 2.8f, EIGToneWaveform::Sine});
	HatchNotes.Add({1.140f, 0.550f, 830.0f, 0.022f, 0.010f, 3.6f, EIGToneWaveform::Sine});

	Wave->ConfigureNotes(MoveTemp(HatchNotes), false);
	return Wave;
}

UIGToneSequenceSoundWave* UIGToneSequenceSoundWave::CreatePhoneVibrationUnfinished(UObject* Outer)
{
	UIGToneSequenceSoundWave* Wave =
		IGToneSequence::NewWave(Outer, TEXT("IGPhoneVibrationUnfinished"));
	TArray<FIGToneNote> BuzzNotes;

	// Motor buzz on a wooden desk: two full bars, then the third cut short.
	// Nothing answers it and the screen never lights.
	const float BarStarts[] = {0.000f, 0.560f, 1.120f};
	const float BarLengths[] = {0.360f, 0.360f, 0.120f};
	for (int32 BarIndex = 0; BarIndex < 3; ++BarIndex)
	{
		BuzzNotes.Add({
			BarStarts[BarIndex], BarLengths[BarIndex], 178.0f, 0.060f,
			0.040f, BarIndex == 2 ? 8.0f : 1.2f, EIGToneWaveform::SoftSquare});
		BuzzNotes.Add({
			BarStarts[BarIndex], BarLengths[BarIndex], 356.0f, 0.024f,
			0.040f, BarIndex == 2 ? 8.0f : 1.2f, EIGToneWaveform::SoftSquare});
		BuzzNotes.Add({
			BarStarts[BarIndex], BarLengths[BarIndex], 89.0f, 0.030f,
			0.040f, BarIndex == 2 ? 8.0f : 1.2f, EIGToneWaveform::Sine});
	}

	Wave->ConfigureNotes(MoveTemp(BuzzNotes), false);
	return Wave;
}

UIGToneSequenceSoundWave* UIGToneSequenceSoundWave::CreateCatLickWaterPlastic(
	UObject* Outer,
	const bool bCutMid)
{
	UIGToneSequenceSoundWave* Wave =
		IGToneSequence::NewWave(Outer, TEXT("IGCatLickWaterPlastic"));
	TArray<FIGToneNote> LickNotes;

	// Tiny tongue laps against a shallow plastic cap. Cut mid-lick when the
	// memory ends before the animal finishes.
	const int32 LickCount = bCutMid ? 3 : 4;
	for (int32 LickIndex = 0; LickIndex < LickCount; ++LickIndex)
	{
		const float Start = LickIndex * 0.240f;
		const bool bTruncated = bCutMid && LickIndex == LickCount - 1;
		LickNotes.Add({
			Start, bTruncated ? 0.030f : 0.075f, 1900.0f, 0.030f,
			0.100f, bTruncated ? 9.0f : 2.6f, EIGToneWaveform::ValueNoise});
		LickNotes.Add({
			Start + 0.012f, bTruncated ? 0.020f : 0.050f, 3600.0f, 0.016f,
			0.080f, bTruncated ? 9.0f : 3.0f, EIGToneWaveform::ValueNoise});
		if (!bTruncated)
		{
			// The cap itself answers with a faint plastic tick.
			LickNotes.Add({
				Start + 0.055f, 0.030f, 1150.0f, 0.012f,
				0.010f, 3.4f, EIGToneWaveform::Sine});
		}
	}

	Wave->ConfigureNotes(MoveTemp(LickNotes), false);
	return Wave;
}

UIGToneSequenceSoundWave* UIGToneSequenceSoundWave::CreateCatLickWaterPaper(UObject* Outer)
{
	UIGToneSequenceSoundWave* Wave =
		IGToneSequence::NewWave(Outer, TEXT("IGCatLickWaterPaper"));
	TArray<FIGToneNote> LickNotes;

	// Wet paper damps the tick into a softer patting texture; cut mid-lick.
	for (int32 LickIndex = 0; LickIndex < 3; ++LickIndex)
	{
		const float Start = LickIndex * 0.250f;
		const bool bTruncated = LickIndex == 2;
		LickNotes.Add({
			Start, bTruncated ? 0.030f : 0.085f, 1300.0f, 0.028f,
			0.140f, bTruncated ? 9.0f : 2.4f, EIGToneWaveform::ValueNoise});
		LickNotes.Add({
			Start + 0.018f, bTruncated ? 0.018f : 0.045f, 2400.0f, 0.013f,
			0.120f, bTruncated ? 9.0f : 2.8f, EIGToneWaveform::ValueNoise});
	}

	Wave->ConfigureNotes(MoveTemp(LickNotes), false);
	return Wave;
}

UIGToneSequenceSoundWave* UIGToneSequenceSoundWave::CreateCatPawTrot(
	UObject* Outer,
	const int32 Steps,
	const bool bCutMid)
{
	UIGToneSequenceSoundWave* Wave =
		IGToneSequence::NewWave(Outer, TEXT("IGCatPawTrot"));
	TArray<FIGToneNote> PawNotes;

	const int32 SafeSteps = FMath::Clamp(Steps, 1, 8);
	for (int32 StepIndex = 0; StepIndex < SafeSteps; ++StepIndex)
	{
		const float Start = StepIndex * 0.190f;
		const bool bTruncated = bCutMid && StepIndex == SafeSteps - 1;
		// Soft pads: almost no attack transient, just small weight.
		PawNotes.Add({
			Start, bTruncated ? 0.020f : 0.055f, 95.0f, 0.040f,
			0.100f, bTruncated ? 9.0f : 3.0f, EIGToneWaveform::Sine});
		PawNotes.Add({
			Start + 0.006f, bTruncated ? 0.015f : 0.035f, 900.0f, 0.012f,
			0.120f, bTruncated ? 9.0f : 3.2f, EIGToneWaveform::ValueNoise});
	}

	Wave->ConfigureNotes(MoveTemp(PawNotes), false);
	return Wave;
}

UIGToneSequenceSoundWave* UIGToneSequenceSoundWave::CreateRodWedgeSeat(UObject* Outer)
{
	UIGToneSequenceSoundWave* Wave =
		IGToneSequence::NewWave(Outer, TEXT("IGRodWedgeSeat"));
	TArray<FIGToneNote> RodNotes;

	// Steel rod sliding along the groove, then seating with one firm knock.
	RodNotes.Add({0.000f, 0.340f, 1050.0f, 0.035f, 0.120f, 2.0f, EIGToneWaveform::ValueNoise});
	RodNotes.Add({0.000f, 0.340f, 460.0f, 0.020f, 0.120f, 2.0f, EIGToneWaveform::Sine});
	RodNotes.Add({0.380f, 0.100f, 240.0f, 0.080f, 0.008f, 2.8f, EIGToneWaveform::Sine});
	RodNotes.Add({0.395f, 0.420f, 910.0f, 0.024f, 0.010f, 3.4f, EIGToneWaveform::Sine});

	Wave->ConfigureNotes(MoveTemp(RodNotes), false);
	return Wave;
}

UIGToneSequenceSoundWave* UIGToneSequenceSoundWave::CreateGlassesTinyRing(UObject* Outer)
{
	UIGToneSequenceSoundWave* Wave =
		IGToneSequence::NewWave(Outer, TEXT("IGGlassesTinyRing"));
	TArray<FIGToneNote> RingNotes;

	RingNotes.Add({0.000f, 0.020f, 3300.0f, 0.030f, 0.005f, 4.0f, EIGToneWaveform::ValueNoise});
	RingNotes.Add({0.004f, 0.480f, 3150.0f, 0.022f, 0.006f, 4.2f, EIGToneWaveform::Sine});
	RingNotes.Add({0.004f, 0.300f, 5210.0f, 0.009f, 0.006f, 4.6f, EIGToneWaveform::Sine});

	Wave->ConfigureNotes(MoveTemp(RingNotes), false);
	return Wave;
}

UIGToneSequenceSoundWave* UIGToneSequenceSoundWave::CreateEndingBMontage(UObject* Outer)
{
	UIGToneSequenceSoundWave* Wave =
		IGToneSequence::NewWave(Outer, TEXT("IGEndingBMontage"));
	TArray<FIGToneNote> MontageNotes;

	// 0.0 s — the 404 door lock: four familiar keypad notes and the bolt.
	for (int32 ToneIndex = 0; ToneIndex < 4; ++ToneIndex)
	{
		MontageNotes.Add({
			ToneIndex * 0.130f, 0.070f, 1568.0f - ToneIndex * 90.0f, 0.045f,
			0.020f, 2.4f, EIGToneWaveform::SoftSquare});
	}
	MontageNotes.Add({0.620f, 0.090f, 300.0f, 0.055f, 0.010f, 3.0f, EIGToneWaveform::Sine});

	// 1.5 s — a drawer pulled open on wooden runners and stopped by hand.
	MontageNotes.Add({1.500f, 0.420f, 480.0f, 0.038f, 0.150f, 2.0f, EIGToneWaveform::ValueNoise});
	MontageNotes.Add({1.500f, 0.420f, 130.0f, 0.028f, 0.150f, 2.0f, EIGToneWaveform::Sine});
	MontageNotes.Add({1.940f, 0.070f, 220.0f, 0.045f, 0.010f, 3.0f, EIGToneWaveform::Sine});

	// 2.8 s — four books set into a cardboard box, one unhurried hand.
	for (int32 BookIndex = 0; BookIndex < 4; ++BookIndex)
	{
		const float Start = 2.800f + BookIndex * 0.440f;
		MontageNotes.Add({
			Start, 0.075f, 150.0f - BookIndex * 8.0f, 0.055f,
			0.010f, 2.8f, EIGToneWaveform::Sine});
		MontageNotes.Add({
			Start + 0.008f, 0.060f, 700.0f, 0.020f,
			0.060f, 3.0f, EIGToneWaveform::ValueNoise});
	}

	// 5.2 s (after 0.6 s of held air) — the box tape from 7/22, torn in two
	// pulls: a short first pull, a hesitation, then the longer second pull.
	MontageNotes.Add({5.200f, 0.300f, 2200.0f, 0.070f, 0.030f, 1.6f, EIGToneWaveform::ValueNoise});
	MontageNotes.Add({5.200f, 0.300f, 950.0f, 0.030f, 0.030f, 1.6f, EIGToneWaveform::SoftSquare});
	MontageNotes.Add({5.780f, 0.520f, 2050.0f, 0.065f, 0.020f, 1.8f, EIGToneWaveform::ValueNoise});
	MontageNotes.Add({5.780f, 0.520f, 880.0f, 0.028f, 0.020f, 1.8f, EIGToneWaveform::SoftSquare});

	// 6.9 s (0.6 s of air again) — the work-vest zipper drawn up once.
	MontageNotes.Add({6.900f, 0.380f, 1700.0f, 0.040f, 0.050f, 1.8f, EIGToneWaveform::ValueNoise});
	MontageNotes.Add({6.900f, 0.380f, 620.0f, 0.018f, 0.050f, 1.8f, EIGToneWaveform::SoftSquare});

	// 7.9 s — the cracked phone buzzes exactly once, slightly rattly.
	MontageNotes.Add({7.900f, 0.340f, 172.0f, 0.050f, 0.040f, 1.4f, EIGToneWaveform::SoftSquare});
	MontageNotes.Add({7.900f, 0.340f, 344.0f, 0.022f, 0.040f, 1.4f, EIGToneWaveform::SoftSquare});
	MontageNotes.Add({7.900f, 0.340f, 2900.0f, 0.010f, 0.040f, 1.6f, EIGToneWaveform::ValueNoise});

	// 9.1 s — the roof door: latch, hinge sweep, frame contact.
	MontageNotes.Add({9.100f, 0.080f, 1300.0f, 0.055f, 0.010f, 3.2f, EIGToneWaveform::ValueNoise});
	MontageNotes.Add({9.220f, 0.700f, 150.0f, 0.048f, 0.200f, 1.5f, EIGToneWaveform::SoftSquare});
	MontageNotes.Add({9.980f, 0.110f, 230.0f, 0.060f, 0.008f, 2.8f, EIGToneWaveform::Sine});

	// 10.6 s — a ceramic bowl set on concrete: one careful contact.
	MontageNotes.Add({10.600f, 0.045f, 2600.0f, 0.055f, 0.006f, 3.6f, EIGToneWaveform::Sine});
	MontageNotes.Add({10.610f, 0.240f, 1320.0f, 0.028f, 0.010f, 3.2f, EIGToneWaveform::Sine});
	MontageNotes.Add({10.600f, 0.060f, 340.0f, 0.030f, 0.010f, 3.0f, EIGToneWaveform::Sine});

	// 11.5 s — water poured into the bowl, thinning as it fills.
	MontageNotes.Add({11.500f, 1.350f, 1450.0f, 0.038f, 0.150f, 1.6f, EIGToneWaveform::ValueNoise});
	MontageNotes.Add({11.500f, 1.350f, 620.0f, 0.024f, 0.150f, 1.6f, EIGToneWaveform::ValueNoise});
	MontageNotes.Add({12.500f, 0.500f, 2100.0f, 0.016f, 0.200f, 2.0f, EIGToneWaveform::ValueNoise});

	// 13.9 s — no recorded voice exists in this build, so the line the
	// player never hears becomes cloth folded once and a single breath.
	MontageNotes.Add({13.900f, 0.520f, 900.0f, 0.026f, 0.220f, 2.2f, EIGToneWaveform::ValueNoise});
	MontageNotes.Add({14.700f, 0.640f, 480.0f, 0.016f, 0.350f, 2.0f, EIGToneWaveform::ValueNoise});
	MontageNotes.Add({14.700f, 0.640f, 190.0f, 0.010f, 0.350f, 2.0f, EIGToneWaveform::Sine});

	Wave->ConfigureNotes(MoveTemp(MontageNotes), false);
	return Wave;
}

UIGToneSequenceSoundWave* UIGToneSequenceSoundWave::CreateEndingBReturnHomeBed(
	UObject* Outer)
{
	UIGToneSequenceSoundWave* Wave =
		IGToneSequence::NewWave(Outer, TEXT("IGEndingBReturnHomeBed"));
	constexpr float DurationSeconds = 45.0f;
	constexpr float BeatSeconds = 60.0f / 52.0f;
	TArray<FIGToneNote> Notes;

	// A refrigerator is the harmonic floor, not an orchestral pad. The shallow
	// release keeps it almost level until it naturally gives way at 45 seconds.
	Notes.Add({0.0f, DurationSeconds, 60.0f, 0.016f, 0.030f, 0.25f, EIGToneWaveform::Sine});
	Notes.Add({0.0f, DurationSeconds, 120.0f, 0.008f, 0.030f, 0.28f, EIGToneWaveform::Sine});
	Notes.Add({0.0f, DurationSeconds, 180.0f, 0.003f, 0.030f, 0.32f, EIGToneWaveform::Sine});

	// The 05:10 alarm's first three intervals are stretched to 52 BPM and
	// voiced as glass rims. The final statement intentionally omits its third
	// strike; the single diegetic cup drop resolves that space near the end.
	constexpr float PhraseStarts[] = {2.40f, 13.94f, 25.48f, 37.02f};
	constexpr float RimFrequencies[] = {1320.0f, 1650.0f, 1320.0f};
	for (int32 PhraseIndex = 0;
		PhraseIndex < static_cast<int32>(UE_ARRAY_COUNT(PhraseStarts));
		++PhraseIndex)
	{
		const int32 StrikeCount = PhraseIndex == 3 ? 2 : 3;
		const float PhraseGain = 0.030f - PhraseIndex * 0.004f;
		for (int32 StrikeIndex = 0; StrikeIndex < StrikeCount; ++StrikeIndex)
		{
			const float Start =
				PhraseStarts[PhraseIndex] + StrikeIndex * BeatSeconds;
			Notes.Add({
				Start,
				2.40f,
				RimFrequencies[StrikeIndex],
				PhraseGain,
				0.004f,
				3.4f,
				EIGToneWaveform::Sine});
			Notes.Add({
				Start,
				1.35f,
				RimFrequencies[StrikeIndex] * 2.0f,
				PhraseGain * 0.26f,
				0.004f,
				3.8f,
				EIGToneWaveform::Sine});
		}
	}

	Wave->ConfigureNotes(MoveTemp(Notes), false);
	return Wave;
}

UIGToneSequenceSoundWave* UIGToneSequenceSoundWave::CreateSpringMorningBed(UObject* Outer)
{
	UIGToneSequenceSoundWave* Wave =
		IGToneSequence::NewWave(Outer, TEXT("IGSpringMorningBed"));
	constexpr float LoopLength = 11.0f;
	TArray<FIGToneNote> SpringNotes;

	// Soft daylight room tone.
	SpringNotes.Add({0.0f, LoopLength, 210.0f, 0.006f, 0.30f, 0.8f, EIGToneWaveform::ValueNoise});

	// Sparrow chatter: quick high chirp clusters, unevenly spaced.
	struct FChirp
	{
		float Start;
		float BaseHz;
		int32 Count;
	};
	const FChirp Chirps[] = {
		{0.80f, 3900.0f, 3},
		{2.10f, 4300.0f, 2},
		{3.35f, 3700.0f, 4},
		{5.60f, 4100.0f, 3},
		{7.15f, 3800.0f, 2},
		{9.05f, 4400.0f, 3},
	};
	for (const FChirp& Chirp : Chirps)
	{
		for (int32 ChirpIndex = 0; ChirpIndex < Chirp.Count; ++ChirpIndex)
		{
			SpringNotes.Add({
				Chirp.Start + ChirpIndex * 0.085f, 0.045f,
				Chirp.BaseHz + ChirpIndex * 160.0f, 0.012f,
				0.100f, 2.6f, EIGToneWaveform::Triangle});
		}
	}

	// A delivery scooter passing two streets away, once per loop.
	SpringNotes.Add({4.20f, 2.60f, 95.0f, 0.010f, 0.45f, 1.1f, EIGToneWaveform::SoftSquare});
	SpringNotes.Add({4.20f, 2.60f, 190.0f, 0.006f, 0.45f, 1.1f, EIGToneWaveform::SoftSquare});

	Wave->ConfigureNotes(MoveTemp(SpringNotes), true, LoopLength);
	return Wave;
}

UIGToneSequenceSoundWave* UIGToneSequenceSoundWave::CreateGlassCupDrip(UObject* Outer)
{
	UIGToneSequenceSoundWave* Wave =
		IGToneSequence::NewWave(Outer, TEXT("IGGlassCupDrip"));
	TArray<FIGToneNote> DripNotes;

	// One drop against empty glass: liquid contact, then the cup's clear
	// ring. There is deliberately no second drop.
	DripNotes.Add({0.000f, 0.040f, 980.0f, 0.070f, 0.010f, 3.0f, EIGToneWaveform::ValueNoise});
	DripNotes.Add({0.012f, 0.900f, 1180.0f, 0.055f, 0.006f, 4.0f, EIGToneWaveform::Sine});
	DripNotes.Add({0.012f, 0.600f, 2360.0f, 0.020f, 0.006f, 4.4f, EIGToneWaveform::Sine});
	DripNotes.Add({0.020f, 0.260f, 560.0f, 0.018f, 0.010f, 3.4f, EIGToneWaveform::Sine});

	Wave->ConfigureNotes(MoveTemp(DripNotes), false);
	return Wave;
}

UIGToneSequenceSoundWave* UIGToneSequenceSoundWave::CreateCatShortMewl(UObject* Outer)
{
	UIGToneSequenceSoundWave* Wave =
		IGToneSequence::NewWave(Outer, TEXT("IGCatShortMewl"));
	TArray<FIGToneNote> MewlNotes;

	// A small falling two-segment cry, breathy rather than cartoonish.
	MewlNotes.Add({0.000f, 0.190f, 640.0f, 0.030f, 0.240f, 1.6f, EIGToneWaveform::Triangle});
	MewlNotes.Add({0.000f, 0.190f, 1280.0f, 0.010f, 0.240f, 1.6f, EIGToneWaveform::Triangle});
	MewlNotes.Add({0.170f, 0.230f, 512.0f, 0.026f, 0.120f, 2.2f, EIGToneWaveform::Triangle});
	MewlNotes.Add({0.170f, 0.230f, 1024.0f, 0.008f, 0.120f, 2.2f, EIGToneWaveform::Triangle});
	MewlNotes.Add({0.000f, 0.400f, 2100.0f, 0.005f, 0.300f, 2.0f, EIGToneWaveform::ValueNoise});

	Wave->ConfigureNotes(MoveTemp(MewlNotes), false);
	return Wave;
}

UIGToneSequenceSoundWave* UIGToneSequenceSoundWave::CreateTrappedBreathBed(
	UObject* Outer)
{
	UIGToneSequenceSoundWave* Wave =
		IGToneSequence::NewWave(Outer, TEXT("IGTrappedBreathBed"));
	constexpr float LoopLength = 5.2f;
	TArray<FIGToneNote> Notes;

	// Two asymmetric breaths. Value-noise is kept very low and banded by a
	// sine body so it reads as air against cloth, not a white-noise generator.
	Notes.Add({
		0.35f, 1.05f, 182.0f, 0.055f,
		0.36f, 1.25f, EIGToneWaveform::ValueNoise});
	Notes.Add({
		0.42f, 0.92f, 61.0f, 0.021f,
		0.42f, 1.10f, EIGToneWaveform::Sine});
	Notes.Add({
		2.45f, 1.38f, 154.0f, 0.048f,
		0.28f, 1.75f, EIGToneWaveform::ValueNoise});
	Notes.Add({
		2.55f, 1.14f, 54.0f, 0.018f,
		0.34f, 1.55f, EIGToneWaveform::Sine});
	Wave->ConfigureNotes(MoveTemp(Notes), true, LoopLength);
	return Wave;
}

UIGToneSequenceSoundWave* UIGToneSequenceSoundWave::CreateSurfaceFootstep(
	UObject* Outer,
	const EIGFootstepSurface Surface,
	const float VariationPitch,
	const float Amplitude)
{
	UIGToneSequenceSoundWave* Wave = IGToneSequence::NewWave(
		Outer,
		TEXT("IGSurfaceFootstep"));
	const float Pitch = FMath::Clamp(VariationPitch, 0.88f, 1.12f);
	const float Gain = FMath::Clamp(Amplitude, 0.0f, 1.0f);
	TArray<FIGToneNote> Notes;

	switch (Surface)
	{
	case EIGFootstepSurface::Vinyl:
		Notes.Add({0.000f, 0.080f, 62.0f * Pitch, 0.42f * Gain, 0.010f, 3.2f, EIGToneWaveform::Sine});
		Notes.Add({0.000f, 0.052f, 520.0f * Pitch, 0.20f * Gain, 0.040f, 2.4f, EIGToneWaveform::ValueNoise});
		break;

	case EIGFootstepSurface::Concrete:
		Notes.Add({0.000f, 0.095f, 57.0f * Pitch, 0.46f * Gain, 0.010f, 3.0f, EIGToneWaveform::Sine});
		Notes.Add({0.000f, 0.115f, 880.0f * Pitch, 0.25f * Gain, 0.025f, 2.0f, EIGToneWaveform::ValueNoise});
		Notes.Add({0.050f, 0.170f, 310.0f * Pitch, 0.075f * Gain, 0.080f, 2.2f, EIGToneWaveform::ValueNoise});
		break;

	case EIGFootstepSurface::MetalStair:
		Notes.Add({0.000f, 0.070f, 72.0f * Pitch, 0.44f * Gain, 0.010f, 3.0f, EIGToneWaveform::Sine});
		Notes.Add({0.000f, 0.055f, 1900.0f * Pitch, 0.26f * Gain, 0.010f, 2.2f, EIGToneWaveform::ValueNoise});
		Notes.Add({0.018f, 0.400f, 1480.0f * Pitch, 0.17f * Gain, 0.020f, 2.7f, EIGToneWaveform::Sine});
		Notes.Add({0.025f, 0.360f, 2780.0f * Pitch, 0.075f * Gain, 0.020f, 3.0f, EIGToneWaveform::Sine});
		break;

	case EIGFootstepSurface::Rooftop:
		Notes.Add({0.000f, 0.085f, 52.0f * Pitch, 0.50f * Gain, 0.010f, 3.4f, EIGToneWaveform::Sine});
		Notes.Add({0.000f, 0.060f, 300.0f * Pitch, 0.17f * Gain, 0.040f, 2.6f, EIGToneWaveform::ValueNoise});
		break;

	case EIGFootstepSurface::GypsumDebris:
		Notes.Add({0.000f, 0.075f, 64.0f * Pitch, 0.43f * Gain, 0.010f, 3.0f, EIGToneWaveform::Sine});
		// Three fixed micro-bursts become three audible variants through the
		// deterministic per-step pitch hash, without runtime random allocation.
		Notes.Add({0.000f, 0.045f, 1150.0f * Pitch, 0.25f * Gain, 0.010f, 2.0f, EIGToneWaveform::ValueNoise});
		Notes.Add({0.038f, 0.050f, 2450.0f / Pitch, 0.23f * Gain, 0.010f, 2.2f, EIGToneWaveform::ValueNoise});
		Notes.Add({0.078f, 0.060f, 760.0f * Pitch, 0.20f * Gain, 0.010f, 2.5f, EIGToneWaveform::ValueNoise});
		break;

	case EIGFootstepSurface::Water:
		Notes.Add({0.000f, 0.100f, 49.0f * Pitch, 0.38f * Gain, 0.010f, 2.8f, EIGToneWaveform::Sine});
		Notes.Add({0.000f, 0.150f, 560.0f * Pitch, 0.31f * Gain, 0.020f, 1.9f, EIGToneWaveform::ValueNoise});
		Notes.Add({0.025f, 0.180f, 1420.0f * Pitch, 0.13f * Gain, 0.020f, 2.7f, EIGToneWaveform::Sine});
		Notes.Add({0.090f, 0.100f, 2400.0f / Pitch, 0.065f * Gain, 0.020f, 3.0f, EIGToneWaveform::ValueNoise});
		break;
	}

	Wave->ConfigureNotes(MoveTemp(Notes), false);
	return Wave;
}

UIGToneSequenceSoundWave* UIGToneSequenceSoundWave::CreateTuningMotif(
	UObject* Outer,
	const bool bResolvedEndingA)
{
	UIGToneSequenceSoundWave* Wave = IGToneSequence::NewWave(
		Outer,
		TEXT("IGMissingFloorTuningMotif"));
	constexpr int32 StrikeCount = 8;
	constexpr float StrikeSpacing = 1.70f;
	constexpr float LoopLength = StrikeCount * StrikeSpacing;
	TArray<FIGToneNote> Notes;
	for (int32 Strike = 0; Strike < StrikeCount; ++Strike)
	{
		const float Cents = -30.0f + Strike * 2.0f;
		const float Frequency = 220.0f * FMath::Pow(2.0f, Cents / 1200.0f);
		const float Start = Strike * StrikeSpacing;
		Notes.Add({Start, 1.18f, Frequency, 0.075f, 0.025f, 2.0f, EIGToneWaveform::Triangle});
		Notes.Add({Start, 0.34f, Frequency * 2.0f, 0.016f, 0.030f, 2.8f, EIGToneWaveform::Triangle});
	}
	if (bResolvedEndingA)
	{
		// The only consonant answer in the score bible: a quiet open fifth
		// arrives after the eighth strike instead of resetting unresolved.
		Notes.Add({LoopLength - 1.10f, 1.05f, 329.63f, 0.038f, 0.10f, 2.0f, EIGToneWaveform::Triangle});
		Notes.Add({LoopLength - 1.10f, 1.05f, 440.00f, 0.030f, 0.10f, 2.0f, EIGToneWaveform::Triangle});
	}
	Wave->ConfigureNotes(MoveTemp(Notes), !bResolvedEndingA, LoopLength);
	return Wave;
}

UIGToneSequenceSoundWave* UIGToneSequenceSoundWave::CreateTuningStrike(
	UObject* Outer,
	const int32 ConfirmationIndex)
{
	UIGToneSequenceSoundWave* Wave = IGToneSequence::NewWave(
		Outer,
		TEXT("IGMissingFloorTruthTuningStrike"));
	// Ten truths stop twelve cents short of concert A. Only Ending A may add
	// the consonant resolution authored by CreateTuningMotif(true).
	const int32 Strike = FMath::Clamp(ConfirmationIndex - 1, 0, 9);
	const float Cents = -30.0f + Strike * 2.0f;
	const float Frequency = 220.0f * FMath::Pow(2.0f, Cents / 1200.0f);
	TArray<FIGToneNote> Notes;
	Notes.Add({0.0f, 1.18f, Frequency, 0.075f, 0.025f, 2.0f,
		EIGToneWaveform::Triangle});
	Notes.Add({0.0f, 0.34f, Frequency * 2.0f, 0.016f, 0.030f, 2.8f,
		EIGToneWaveform::Triangle});
	Wave->ConfigureNotes(MoveTemp(Notes), false);
	return Wave;
}

UIGToneSequenceSoundWave* UIGToneSequenceSoundWave::CreateCavityDrone(
	UObject* Outer)
{
	UIGToneSequenceSoundWave* Wave = IGToneSequence::NewWave(
		Outer,
		TEXT("IGMissingFloorCavityDrone"));
	constexpr float LoopLength = 8.0f;
	TArray<FIGToneNote> Notes;
	Notes.Add({0.0f, LoopLength, 44.0f, 0.095f, 0.35f, 0.8f, EIGToneWaveform::Sine});
	Notes.Add({0.0f, LoopLength, 180.0f, 0.027f, 0.30f, 0.9f, EIGToneWaveform::ValueNoise});
	for (const float Tick : {1.30f, 3.85f, 6.70f})
	{
		Notes.Add({Tick, 0.035f, 980.0f + Tick * 41.0f, 0.055f, 0.010f, 3.2f, EIGToneWaveform::Sine});
		Notes.Add({Tick, 0.090f, 420.0f + Tick * 13.0f, 0.026f, 0.020f, 2.8f, EIGToneWaveform::ValueNoise});
	}
	Wave->ConfigureNotes(MoveTemp(Notes), true, LoopLength);
	Wave->ConfigurePitchWow(0.0035f, 0.07f);
	return Wave;
}

UIGToneSequenceSoundWave* UIGToneSequenceSoundWave::CreateChaseScore(
	UObject* Outer)
{
	UIGToneSequenceSoundWave* Wave = IGToneSequence::NewWave(
		Outer,
		TEXT("IGMissingFloorChaseScore"));
	constexpr float Beat = 60.0f / 118.0f;
	constexpr int32 BeatCount = 8;
	constexpr float LoopLength = Beat * BeatCount;
	TArray<FIGToneNote> Notes;
	for (int32 Index = 0; Index < BeatCount; ++Index)
	{
		const float Start = Index * Beat;
		const float Accent = Index % 4 == 0 ? 1.0f : 0.72f;
		Notes.Add({Start, Beat * 0.54f, 52.0f, 0.15f * Accent, 0.025f, 2.4f, EIGToneWaveform::Sine});
		Notes.Add({Start, 0.040f, 520.0f, 0.055f * Accent, 0.010f, 3.2f, EIGToneWaveform::ValueNoise});
	}
	for (const float Frequency : {220.0f, 223.0f, 227.0f})
	{
		Notes.Add({Beat * 0.50f, LoopLength - Beat * 0.50f, Frequency, 0.035f, 0.12f, 1.1f, EIGToneWaveform::Triangle});
	}
	Wave->ConfigureNotes(MoveTemp(Notes), true, LoopLength);
	Wave->ConfigurePitchWow(0.0025f, 0.21f);
	return Wave;
}

UIGToneSequenceSoundWave* UIGToneSequenceSoundWave::CreateWallKnockTriple(
	UObject* Outer,
	const float Muffle01)
{
	UIGToneSequenceSoundWave* Wave =
		IGToneSequence::NewWave(Outer, TEXT("IGWallKnockTriple"));
	TArray<FIGToneNote> KnockNotes;

	// Each knock is a low structural thump plus a knuckle contact click.
	// Muffling (heard through a finished wall) keeps the thump and eats the
	// click, which is what real drywall does to a fist.
	const float Click = FMath::Lerp(0.085f, 0.012f, FMath::Clamp(Muffle01, 0.0f, 1.0f));
	const float Body = FMath::Lerp(0.360f, 0.300f, FMath::Clamp(Muffle01, 0.0f, 1.0f));
	for (int32 KnockIndex = 0; KnockIndex < 3; ++KnockIndex)
	{
		const float Start = 0.62f * KnockIndex;
		KnockNotes.Add({Start, 0.110f, 58.0f, Body, 0.004f, 2.6f, EIGToneWaveform::Sine});
		KnockNotes.Add({Start, 0.060f, 176.0f, 0.130f, 0.006f, 2.0f, EIGToneWaveform::Sine});
		KnockNotes.Add({Start, 0.030f, 1150.0f, Click, 0.020f, 1.2f, EIGToneWaveform::ValueNoise});
	}

	Wave->ConfigureNotes(MoveTemp(KnockNotes), false);
	return Wave;
}

UIGToneSequenceSoundWave* UIGToneSequenceSoundWave::CreateWallKnockSingle(
	UObject* Outer,
	const float Muffle01)
{
	UIGToneSequenceSoundWave* Wave =
		IGToneSequence::NewWave(Outer, TEXT("IGWallKnockSingle"));
	TArray<FIGToneNote> KnockNotes;
	const float Muffle = FMath::Clamp(Muffle01, 0.0f, 1.0f);
	const float Click = FMath::Lerp(0.085f, 0.012f, Muffle);
	const float Body = FMath::Lerp(0.360f, 0.300f, Muffle);
	KnockNotes.Add({0.0f, 0.110f, 58.0f, Body, 0.004f, 2.6f, EIGToneWaveform::Sine});
	KnockNotes.Add({0.0f, 0.060f, 176.0f, 0.130f, 0.006f, 2.0f, EIGToneWaveform::Sine});
	KnockNotes.Add({0.0f, 0.030f, 1150.0f, Click, 0.020f, 1.2f, EIGToneWaveform::ValueNoise});
	Wave->ConfigureNotes(MoveTemp(KnockNotes), false);
	return Wave;
}

UIGToneSequenceSoundWave* UIGToneSequenceSoundWave::CreateWallKnockReply(UObject* Outer)
{
	UIGToneSequenceSoundWave* Wave =
		IGToneSequence::NewWave(Outer, TEXT("IGWallKnockReply"));
	TArray<FIGToneNote> ReplyNotes;

	// Two knocks only, softer and closer together than the hunting triple:
	// an answer, not a search.
	for (int32 KnockIndex = 0; KnockIndex < 2; ++KnockIndex)
	{
		const float Start = 0.42f * KnockIndex;
		ReplyNotes.Add({Start, 0.100f, 58.0f, 0.250f, 0.005f, 2.8f, EIGToneWaveform::Sine});
		ReplyNotes.Add({Start, 0.050f, 176.0f, 0.080f, 0.008f, 2.0f, EIGToneWaveform::Sine});
		ReplyNotes.Add({Start, 0.024f, 1000.0f, 0.030f, 0.030f, 1.4f, EIGToneWaveform::ValueNoise});
	}

	Wave->ConfigureNotes(MoveTemp(ReplyNotes), false);
	return Wave;
}

UIGToneSequenceSoundWave* UIGToneSequenceSoundWave::CreateAnswerKnockPattern(
	UObject* Outer,
	const float Muffle01)
{
	UIGToneSequenceSoundWave* Wave =
		IGToneSequence::NewWave(Outer, TEXT("IGAnswerKnockPattern"));
	TArray<FIGToneNote> PatternNotes;

	// Two, a rest, one. The rest is the signature: 0.42 s inside the pair,
	// 0.73 s of silence, then the single settling knock.
	const float Click = FMath::Lerp(0.075f, 0.010f, FMath::Clamp(Muffle01, 0.0f, 1.0f));
	const float Body = FMath::Lerp(0.330f, 0.270f, FMath::Clamp(Muffle01, 0.0f, 1.0f));
	const float Starts[] = {0.0f, 0.42f, 1.15f};
	for (const float Start : Starts)
	{
		PatternNotes.Add({Start, 0.105f, 58.0f, Body, 0.004f, 2.6f, EIGToneWaveform::Sine});
		PatternNotes.Add({Start, 0.055f, 176.0f, 0.110f, 0.006f, 2.0f, EIGToneWaveform::Sine});
		PatternNotes.Add({Start, 0.028f, 1100.0f, Click, 0.020f, 1.2f, EIGToneWaveform::ValueNoise});
	}

	Wave->ConfigureNotes(MoveTemp(PatternNotes), false);
	return Wave;
}

UIGToneSequenceSoundWave* UIGToneSequenceSoundWave::CreateEntityDragLoop(
	UObject* Outer,
	const bool bVinyl)
{
	UIGToneSequenceSoundWave* Wave = IGToneSequence::NewWave(
		Outer,
		bVinyl ? TEXT("IGEntityDragLoopVinyl") : TEXT("IGEntityDragLoop"));
	TArray<FIGToneNote> DragNotes;

	if (bVinyl)
	{
		// 장판: sheet vinyl laid on screed. The slab is still there but a
		// millimetre of plastic sits over it, so the rumble goes and the drag
		// becomes a higher, tighter hiss. The palm plant lands softer.
		DragNotes.Add({0.000f, 0.060f, 78.0f, 0.130f, 0.010f, 2.8f, EIGToneWaveform::Sine});
		DragNotes.Add({0.000f, 0.045f, 1100.0f, 0.055f, 0.026f, 1.6f, EIGToneWaveform::ValueNoise});
		DragNotes.Add({0.180f, 0.520f, 900.0f, 0.078f, 0.180f, 1.2f, EIGToneWaveform::ValueNoise});
		DragNotes.Add({0.180f, 0.520f, 2200.0f, 0.038f, 0.200f, 1.3f, EIGToneWaveform::ValueNoise});
		// Stick-slip. Cloth on plastic catches and releases, and that squeak is
		// the single clearest sign underfoot that he has come in off the tile.
		DragNotes.Add({0.315f, 0.070f, 1750.0f, 0.030f, 0.060f, 2.2f, EIGToneWaveform::Triangle});
		DragNotes.Add({0.612f, 0.055f, 2050.0f, 0.022f, 0.070f, 2.4f, EIGToneWaveform::Triangle});
		DragNotes.Add({0.740f, 0.180f, 2600.0f, 0.020f, 0.200f, 1.8f, EIGToneWaveform::ValueNoise});
	}
	else
	{
		// One crawl cycle: palm plant, weight shift, the long drag of trailing
		// legs, then a grit tail. Loops at the crawl cadence.
		DragNotes.Add({0.000f, 0.070f, 66.0f, 0.240f, 0.008f, 2.4f, EIGToneWaveform::Sine});
		DragNotes.Add({0.000f, 0.050f, 700.0f, 0.050f, 0.030f, 1.5f, EIGToneWaveform::ValueNoise});
		DragNotes.Add({0.180f, 0.520f, 320.0f, 0.085f, 0.180f, 1.2f, EIGToneWaveform::ValueNoise});
		DragNotes.Add({0.180f, 0.520f, 92.0f, 0.070f, 0.180f, 1.4f, EIGToneWaveform::ValueNoise});
		DragNotes.Add({0.740f, 0.180f, 1400.0f, 0.024f, 0.200f, 1.8f, EIGToneWaveform::ValueNoise});
	}

	Wave->ConfigureNotes(MoveTemp(DragNotes), true, 1.05f);
	return Wave;
}

UIGToneSequenceSoundWave* UIGToneSequenceSoundWave::CreateFrottageRub(
	UObject* Outer)
{
	UIGToneSequenceSoundWave* Wave =
		IGToneSequence::NewWave(Outer, TEXT("IGFrottageRub"));
	TArray<FIGToneNote> RubNotes;

	// §21.3 fixes the band at 900~4200 Hz: graphite on paper has no low end and
	// no pitch, only the grain of the sheet. Three strokes to the loop at about
	// two and a half a second, which is the pace of a hand pressing hard enough
	// to lift letters rather than sketching.
	constexpr float LoopSeconds = 1.20f;
	const float StrokeStarts[] = {0.000f, 0.405f, 0.798f};
	for (int32 StrokeIndex = 0; StrokeIndex < UE_ARRAY_COUNT(StrokeStarts); ++StrokeIndex)
	{
		const float Start = StrokeStarts[StrokeIndex];
		// Each stroke swells and dies: the middle of a stroke presses hardest,
		// and the turn at either end is nearly silent. A flat band would read as
		// a machine, not a hand.
		RubNotes.Add({Start, 0.360f, 1200.0f, 0.062f, 0.300f, 1.5f, EIGToneWaveform::ValueNoise});
		RubNotes.Add({Start + 0.020f, 0.320f, 2600.0f, 0.044f, 0.320f, 1.6f, EIGToneWaveform::ValueNoise});
		RubNotes.Add({Start + 0.045f, 0.270f, 4200.0f, 0.026f, 0.340f, 1.8f, EIGToneWaveform::ValueNoise});
		// The sheet's own body under the pressure, at the bottom of the band.
		RubNotes.Add({Start, 0.300f, 900.0f, 0.034f, 0.280f, 1.4f, EIGToneWaveform::ValueNoise});
		// The graphite catching on the weave once per stroke, never on the beat.
		RubNotes.Add({
			Start + 0.150f + 0.030f * StrokeIndex,
			0.014f,
			3400.0f,
			0.020f,
			0.010f,
			1.4f,
			EIGToneWaveform::ValueNoise});
	}

	Wave->ConfigureNotes(MoveTemp(RubNotes), true, LoopSeconds);
	return Wave;
}

UIGToneSequenceSoundWave* UIGToneSequenceSoundWave::CreateAudibleHeartbeat(
	UObject* Outer,
	const float Loudness)
{
	UIGToneSequenceSoundWave* Wave =
		IGToneSequence::NewWave(Outer, TEXT("IGAudibleHeartbeat"));
	TArray<FIGToneNote> Beat;

	// §21.3: 기존 심박 + 220Hz 로우패스, −6dB, 박동 동기. The timing is exactly
	// the 2D beat's so the change is heard as the *same* heart, not a new sound.
	// −6 dB is half the amplitude; the low pass shows up as the 88 Hz partial
	// leaving, which is the part that made the pulse sound crisp and close.
	const float Safe = FMath::Clamp(Loudness, 0.0f, 1.0f) * 0.5f;
	Beat.Add({0.0f, 0.19f, 44.0f, Safe, 0.05f, 2.2f, EIGToneWaveform::Sine});
	Beat.Add({0.20f, 0.16f, 38.0f, Safe * 0.72f, 0.06f, 2.4f, EIGToneWaveform::Sine});
	// Conducted through a chest and a wall it loses the transient and gains a
	// little body: it rings slightly longer than the one inside your head.
	Beat.Add({0.0f, 0.24f, 62.0f, Safe * 0.26f, 0.09f, 1.9f, EIGToneWaveform::Sine});

	Wave->ConfigureNotes(MoveTemp(Beat), false);
	return Wave;
}

UIGToneSequenceSoundWave* UIGToneSequenceSoundWave::CreatePlasterSettle(UObject* Outer)
{
	UIGToneSequenceSoundWave* Wave =
		IGToneSequence::NewWave(Outer, TEXT("IGPlasterSettle"));
	TArray<FIGToneNote> SettleNotes;

	// Dry hairline cracks with no resonance: hardened plaster, not wood.
	SettleNotes.Add({0.000f, 0.030f, 2400.0f, 0.060f, 0.010f, 1.0f, EIGToneWaveform::ValueNoise});
	SettleNotes.Add({0.140f, 0.024f, 3100.0f, 0.045f, 0.010f, 1.0f, EIGToneWaveform::ValueNoise});
	SettleNotes.Add({0.330f, 0.040f, 1900.0f, 0.050f, 0.010f, 1.2f, EIGToneWaveform::ValueNoise});
	SettleNotes.Add({0.330f, 0.060f, 120.0f, 0.060f, 0.010f, 2.6f, EIGToneWaveform::Sine});

	Wave->ConfigureNotes(MoveTemp(SettleNotes), false);
	return Wave;
}

UIGToneSequenceSoundWave* UIGToneSequenceSoundWave::CreatePlasterDustFall(
	UObject* Outer)
{
	UIGToneSequenceSoundWave* Wave =
		IGToneSequence::NewWave(Outer, TEXT("IGPlasterDustFall"));
	TArray<FIGToneNote> DustNotes;

	// §21.3 fixes this cue at a 6 kHz high pass with an exponential decay over
	// 0.90 s, and that discipline is right: falling grains answer broadband with
	// a spectral centroid that climbs as they get finer, and plaster powder is
	// far finer than sand. Everything here therefore sits above 6 kHz. Anything
	// lower would be debris, and debris already has its own cue (미장 갈라짐).
	// Three overlapping bands with high release powers approximate the decay.
	DustNotes.Add({0.000f, 0.900f, 6200.0f, 0.030f, 0.020f, 3.2f, EIGToneWaveform::ValueNoise});
	DustNotes.Add({0.040f, 0.800f, 7100.0f, 0.024f, 0.060f, 2.8f, EIGToneWaveform::ValueNoise});
	DustNotes.Add({0.120f, 0.720f, 8400.0f, 0.016f, 0.100f, 3.0f, EIGToneWaveform::ValueNoise});

	// Individual grains riding the sift. Irregular on purpose — even spacing
	// would read as a machine ticking somewhere in the building.
	const float GrainStarts[] = {0.031f, 0.118f, 0.207f, 0.264f, 0.415f, 0.596f, 0.742f};
	const float GrainBands[] = {6900.0f, 8100.0f, 7300.0f, 9200.0f, 6400.0f, 8700.0f, 7600.0f};
	for (int32 GrainIndex = 0; GrainIndex < UE_ARRAY_COUNT(GrainStarts); ++GrainIndex)
	{
		// 8–20 ms each: one grain of granular synthesis, which is exactly what
		// a single falling speck is.
		const float GrainSeconds = 0.008f + 0.012f * ((GrainIndex % 3) * 0.5f);
		const float GrainAmplitude = 0.018f - 0.0011f * GrainIndex;
		DustNotes.Add({
			GrainStarts[GrainIndex],
			GrainSeconds,
			GrainBands[GrainIndex],
			GrainAmplitude,
			0.010f,
			1.3f,
			EIGToneWaveform::ValueNoise});
	}

	Wave->ConfigureNotes(MoveTemp(DustNotes), false);
	return Wave;
}

UIGToneSequenceSoundWave* UIGToneSequenceSoundWave::CreateRooftopTankSlosh(
	UObject* Outer)
{
	UIGToneSequenceSoundWave* Wave =
		IGToneSequence::NewWave(Outer, TEXT("IGRooftopTankSlosh"));
	// 4.4초에 한 번 왕복한다. 가득 찬 탱크일수록 느리게 오간다.
	constexpr float LoopLength = 8.8f;
	TArray<FIGToneNote> Notes;
	// 수면 아래에서 움직이는 덩어리. 대역이 아주 좁고 끊기지 않는다.
	Notes.Add({0.0f, LoopLength, 33.0f, 0.085f, 0.40f, 0.7f, EIGToneWaveform::Sine});
	Notes.Add({0.0f, LoopLength, 210.0f, 0.016f, 0.45f, 0.8f, EIGToneWaveform::ValueNoise});
	for (int32 Sway = 0; Sway < 2; ++Sway)
	{
		const float Start = Sway * 4.4f;
		// 밀려갔다 돌아오는 물. 올라갈 때가 길고 내려올 때가 짧다.
		Notes.Add({Start + 0.30f, 2.10f, 128.0f, 0.030f, 0.55f, 1.1f, EIGToneWaveform::ValueNoise});
		Notes.Add({Start + 2.20f, 1.40f, 96.0f, 0.024f, 0.35f, 1.4f, EIGToneWaveform::ValueNoise});
		// 돌아온 물이 강판을 친다. 판이 얇아 배음이 남는다.
		Notes.Add({Start + 3.55f, 0.075f, 340.0f, 0.052f, 0.010f, 2.6f, EIGToneWaveform::ValueNoise});
		Notes.Add({Start + 3.55f, 0.760f, 152.0f, 0.040f, 0.008f, 1.5f, EIGToneWaveform::Triangle});
		Notes.Add({Start + 3.55f, 0.540f, 421.0f, 0.017f, 0.008f, 1.7f, EIGToneWaveform::Triangle});
	}
	Wave->ConfigureNotes(MoveTemp(Notes), true, LoopLength);
	Wave->ConfigurePitchWow(0.0024f, 0.045f);
	return Wave;
}

UIGToneSequenceSoundWave* UIGToneSequenceSoundWave::CreatePoliceLineTapePull(
	UObject* Outer)
{
	UIGToneSequenceSoundWave* Wave =
		IGToneSequence::NewWave(Outer, TEXT("IGEpiloguePoliceLineTape"));
	TArray<FIGToneNote> Notes;
	// 롤이 도는 동안 접착면이 계속 뜯긴다 — 끊기지 않는 마찰이 먼저고,
	// 그 위에 롤 축의 얇은 떨림이 얹힌다.
	Notes.Add({0.000f, 1.35f, 3100.0f, 0.052f, 0.060f, 1.1f, EIGToneWaveform::ValueNoise});
	Notes.Add({0.040f, 1.24f, 1450.0f, 0.030f, 0.080f, 1.2f, EIGToneWaveform::ValueNoise});
	Notes.Add({0.060f, 1.18f, 214.0f, 0.014f, 0.120f, 1.4f, EIGToneWaveform::Triangle});
	// 끝에서 손으로 끊는다.
	Notes.Add({1.36f, 0.075f, 4200.0f, 0.085f, 0.008f, 2.9f, EIGToneWaveform::ValueNoise});
	Notes.Add({1.36f, 0.140f, 620.0f, 0.036f, 0.012f, 2.4f, EIGToneWaveform::ValueNoise});
	Wave->ConfigureNotes(MoveTemp(Notes), false);
	return Wave;
}

UIGToneSequenceSoundWave* UIGToneSequenceSoundWave::CreateGurneyWheels(
	UObject* Outer)
{
	UIGToneSequenceSoundWave* Wave =
		IGToneSequence::NewWave(Outer, TEXT("IGEpilogueGurneyWheels"));
	TArray<FIGToneNote> Notes;
	// 작은 캐스터의 연속 구름. 타일 이음매를 네 번 넘고, 넘을 때마다
	// 조금씩 작아진다 — 멀어지는 것은 속도가 아니라 거리다.
	Notes.Add({0.000f, 3.10f, 1750.0f, 0.030f, 0.140f, 1.0f, EIGToneWaveform::ValueNoise});
	Notes.Add({0.000f, 3.10f, 118.0f, 0.020f, 0.180f, 1.0f, EIGToneWaveform::Sine});
	const float SeamTimes[] = {0.42f, 1.16f, 1.94f, 2.71f};
	const float SeamGains[] = {1.00f, 0.82f, 0.64f, 0.47f};
	for (int32 Index = 0; Index < 4; ++Index)
	{
		const float Start = SeamTimes[Index];
		const float Gain = SeamGains[Index];
		Notes.Add({Start, 0.055f, 96.0f, 0.090f * Gain, 0.006f, 2.6f, EIGToneWaveform::Sine});
		Notes.Add({Start, 0.038f, 2600.0f, 0.048f * Gain, 0.006f, 3.1f, EIGToneWaveform::ValueNoise});
	}
	Wave->ConfigureNotes(MoveTemp(Notes), false);
	return Wave;
}

UIGToneSequenceSoundWave* UIGToneSequenceSoundWave::CreateCameraShutterTriple(
	UObject* Outer)
{
	UIGToneSequenceSoundWave* Wave =
		IGToneSequence::NewWave(Outer, TEXT("IGEpilogueCameraShutter"));
	TArray<FIGToneNote> Notes;
	for (const float Start : {0.00f, 0.86f, 1.79f})
	{
		// 미러 슬랩, 셔터막, 그리고 감기 모터의 짧은 회전.
		Notes.Add({Start, 0.030f, 1250.0f, 0.115f, 0.004f, 3.4f, EIGToneWaveform::ValueNoise});
		Notes.Add({Start + 0.012f, 0.026f, 3400.0f, 0.080f, 0.004f, 3.6f, EIGToneWaveform::ValueNoise});
		Notes.Add({Start + 0.044f, 0.150f, 320.0f, 0.036f, 0.010f, 2.2f, EIGToneWaveform::SoftSquare});
	}
	Wave->ConfigureNotes(MoveTemp(Notes), false);
	return Wave;
}

UIGToneSequenceSoundWave* UIGToneSequenceSoundWave::CreateDebrisSweep(
	UObject* Outer)
{
	UIGToneSequenceSoundWave* Wave =
		IGToneSequence::NewWave(Outer, TEXT("IGEpilogueDebrisSweep"));
	TArray<FIGToneNote> Notes;
	// 세 번 쓴다. 획마다 앞으로 밀리는 알갱이가 늘어 대역이 낮아진다.
	const float StrokeStarts[] = {0.00f, 1.05f, 2.02f};
	const float StrokeBands[] = {5200.0f, 4400.0f, 3700.0f};
	for (int32 Index = 0; Index < 3; ++Index)
	{
		const float Start = StrokeStarts[Index];
		Notes.Add({Start, 0.62f, StrokeBands[Index], 0.062f, 0.070f, 1.6f, EIGToneWaveform::ValueNoise});
		Notes.Add({Start + 0.05f, 0.50f, 880.0f, 0.024f, 0.090f, 1.8f, EIGToneWaveform::ValueNoise});
		// 획 끝에서 조각이 무더기에 부딪힌다.
		Notes.Add({Start + 0.58f, 0.070f, 1900.0f, 0.040f, 0.008f, 2.8f, EIGToneWaveform::ValueNoise});
	}
	Wave->ConfigureNotes(MoveTemp(Notes), false);
	return Wave;
}

UIGToneSequenceSoundWave* UIGToneSequenceSoundWave::CreateEpilogueWorkshopScore(
	UObject* Outer)
{
	UIGToneSequenceSoundWave* Wave =
		IGToneSequence::NewWave(Outer, TEXT("IGEpilogueWorkshopScore"));
	constexpr int32 StrikeCount = 8;
	constexpr float StrikeSpacing = 1.70f;
	TArray<FIGToneNote> Notes;
	// CreateTuningMotif과 같은 걸음으로 시작하되, 여기서는 마지막 타건이
	// 0센트에 닿는다. 게임 내내 -30에서 -16까지만 오던 그 음이다.
	for (int32 Strike = 0; Strike < StrikeCount; ++Strike)
	{
		const float Progress =
			static_cast<float>(Strike) / static_cast<float>(StrikeCount - 1);
		const float Cents = FMath::Lerp(-30.0f, 0.0f, Progress);
		const float Frequency = 220.0f * FMath::Pow(2.0f, Cents / 1200.0f);
		const float Start = Strike * StrikeSpacing;
		// 조율이 끝나 갈수록 세게 치지 않는다. 확인만 하면 되기 때문이다.
		const float Amplitude = FMath::Lerp(0.082f, 0.058f, Progress);
		Notes.Add({Start, 1.18f, Frequency, Amplitude, 0.025f, 2.0f, EIGToneWaveform::Triangle});
		Notes.Add({Start, 0.34f, Frequency * 2.0f, Amplitude * 0.21f, 0.030f, 2.8f, EIGToneWaveform::Triangle});
	}

	// 마지막 타건 뒤에 손을 떼고 한 번 눌러 본다 — 열린 5도, 길게.
	const float ChordStart = StrikeCount * StrikeSpacing + 0.65f;
	Notes.Add({ChordStart, 5.20f, 220.00f, 0.052f, 0.055f, 1.3f, EIGToneWaveform::Triangle});
	Notes.Add({ChordStart, 5.20f, 329.63f, 0.034f, 0.070f, 1.3f, EIGToneWaveform::Triangle});
	Notes.Add({ChordStart, 4.60f, 440.00f, 0.021f, 0.090f, 1.5f, EIGToneWaveform::Triangle});
	Notes.Add({ChordStart, 3.40f, 110.00f, 0.026f, 0.120f, 1.4f, EIGToneWaveform::Sine});
	Wave->ConfigureNotes(MoveTemp(Notes), false);
	// 나무 몸통과 사람 손이라 아주 작게 흔들린다. 기계로 들리면 안 된다.
	Wave->ConfigurePitchWow(0.0016f, 0.11f);
	return Wave;
}

UIGToneSequenceSoundWave* UIGToneSequenceSoundWave::CreateEpilogueAutumnBed(
	UObject* Outer)
{
	UIGToneSequenceSoundWave* Wave =
		IGToneSequence::NewWave(Outer, TEXT("IGEpilogueAutumnBed"));
	constexpr float LoopLength = 12.0f;
	TArray<FIGToneNote> Notes;
	// 크레인 유압. 골목 하나 건너에서 나는 소리라 저역만 남는다.
	Notes.Add({0.0f, LoopLength, 38.0f, 0.055f, 0.30f, 0.7f, EIGToneWaveform::Sine});
	Notes.Add({0.0f, LoopLength, 260.0f, 0.014f, 0.35f, 0.9f, EIGToneWaveform::ValueNoise});
	// 붐이 한 번 내려앉는다.
	Notes.Add({4.30f, 1.10f, 62.0f, 0.048f, 0.060f, 1.8f, EIGToneWaveform::Sine});
	Notes.Add({4.30f, 0.35f, 740.0f, 0.020f, 0.020f, 2.6f, EIGToneWaveform::ValueNoise});
	// 401호 창턱의 라디오. 대역을 좁혀 말이 되지 않게 둔다.
	for (int32 Phrase = 0; Phrase < 5; ++Phrase)
	{
		const float Start = 0.90f + Phrase * 2.15f;
		const float Frequency = 430.0f + static_cast<float>(Phrase % 3) * 55.0f;
		Notes.Add({Start, 1.15f, Frequency, 0.017f, 0.140f, 1.5f, EIGToneWaveform::ValueNoise});
		Notes.Add({Start + 0.28f, 0.62f, Frequency * 1.5f, 0.008f, 0.180f, 1.7f, EIGToneWaveform::ValueNoise});
	}
	Wave->ConfigureNotes(MoveTemp(Notes), true, LoopLength);
	Wave->ConfigurePitchWow(0.0030f, 0.06f);
	return Wave;
}

UIGToneSequenceSoundWave* UIGToneSequenceSoundWave::CreateKeyDropMetalBox(
	UObject* Outer)
{
	UIGToneSequenceSoundWave* Wave =
		IGToneSequence::NewWave(Outer, TEXT("IGEpilogueKeyDropBox"));
	TArray<FIGToneNote> Notes;
	// 투입구를 지나 얇은 철판 바닥에 닿고, 링이 한 번 더 튄다.
	Notes.Add({0.000f, 0.055f, 2900.0f, 0.070f, 0.004f, 3.2f, EIGToneWaveform::ValueNoise});
	Notes.Add({0.030f, 0.320f, 1180.0f, 0.062f, 0.006f, 2.0f, EIGToneWaveform::Triangle});
	Notes.Add({0.030f, 0.280f, 1770.0f, 0.034f, 0.006f, 2.2f, EIGToneWaveform::Triangle});
	Notes.Add({0.034f, 0.400f, 176.0f, 0.038f, 0.008f, 1.9f, EIGToneWaveform::Sine});
	Notes.Add({0.155f, 0.190f, 2340.0f, 0.026f, 0.005f, 2.8f, EIGToneWaveform::Triangle});
	Wave->ConfigureNotes(MoveTemp(Notes), false);
	return Wave;
}

UIGToneSequenceSoundWave* UIGToneSequenceSoundWave::CreateRailingKnockTwo(
	UObject* Outer)
{
	UIGToneSequenceSoundWave* Wave =
		IGToneSequence::NewWave(Outer, TEXT("IGEpilogueRailingKnockTwo"));
	TArray<FIGToneNote> Notes;
	// 「둘」의 간격은 §7 P4와 같은 0.34초다. 응답 노크를 배운 손이
	// 그대로 치는 것이므로 박자를 새로 만들지 않는다.
	for (const float Start : {0.00f, 0.34f})
	{
		Notes.Add({Start, 0.030f, 1900.0f, 0.075f, 0.004f, 3.0f, EIGToneWaveform::ValueNoise});
		// 강관은 벽과 달리 배음이 오래 남는다.
		Notes.Add({Start, 0.620f, 486.0f, 0.056f, 0.005f, 1.6f, EIGToneWaveform::Triangle});
		Notes.Add({Start, 0.520f, 1312.0f, 0.024f, 0.005f, 1.8f, EIGToneWaveform::Triangle});
		Notes.Add({Start, 0.240f, 92.0f, 0.030f, 0.006f, 2.4f, EIGToneWaveform::Sine});
	}
	Wave->ConfigureNotes(MoveTemp(Notes), false);
	return Wave;
}

UIGToneSequenceSoundWave* UIGToneSequenceSoundWave::CreateRecordingPlayback(
	UObject* Outer,
	const TArray<FIGRecordedSound>& Sounds)
{
	if (Sounds.Num() == 0)
	{
		return nullptr;
	}
	UIGToneSequenceSoundWave* Wave =
		IGToneSequence::NewWave(Outer, TEXT("IGRecordingPlayback"));
	TArray<FIGToneNote> TakeNotes;

	// Room tone of a cheap microphone left against a door: a thin hiss that runs
	// the whole take. It keeps playing through the gaps, which is exactly why the
	// gaps read as silence-on-a-recording rather than as the game stopping.
	float TakeEnd = 0.0f;
	for (const FIGRecordedSound& Sound : Sounds)
	{
		TakeEnd = FMath::Max(
			TakeEnd,
			Sound.OffsetSeconds + Sound.DurationSeconds);
	}
	TakeEnd = FMath::Max(TakeEnd, 1.0f);
	TakeNotes.Add({0.0f, TakeEnd, 5200.0f, 0.012f, 0.020f, 0.4f, EIGToneWaveform::ValueNoise});
	TakeNotes.Add({0.0f, TakeEnd, 1500.0f, 0.008f, 0.030f, 0.4f, EIGToneWaveform::ValueNoise});

	for (const FIGRecordedSound& Sound : Sounds)
	{
		if (Sound.bSuppressed)
		{
			// The whole rule, in one skipped iteration. Its duration is already
			// in TakeEnd, so the timeline keeps the room exactly this long.
			continue;
		}
		// Her own body, through a phone speaker. Band-limited on purpose: the
		// low thump of a real footfall is not what a small speaker gives back.
		const float Level = FMath::Clamp(Sound.Loudness, 0.0f, 1.0f);
		TakeNotes.Add({
			Sound.OffsetSeconds,
			0.055f,
			900.0f,
			0.045f + 0.075f * Level,
			0.010f,
			1.7f,
			EIGToneWaveform::ValueNoise});
		TakeNotes.Add({
			Sound.OffsetSeconds,
			0.038f,
			2400.0f,
			0.026f + 0.050f * Level,
			0.008f,
			2.0f,
			EIGToneWaveform::ValueNoise});
		// A trace of the body under it, still above the speaker's floor.
		TakeNotes.Add({
			Sound.OffsetSeconds,
			0.070f,
			430.0f,
			0.020f + 0.038f * Level,
			0.012f,
			2.4f,
			EIGToneWaveform::Sine});
	}

	Wave->ConfigureNotes(MoveTemp(TakeNotes), false);
	return Wave;
}

UIGToneSequenceSoundWave* UIGToneSequenceSoundWave::CreateCrtChannelSwitch(
	UObject* Outer,
	const bool bCollapse)
{
	UIGToneSequenceSoundWave* Wave = IGToneSequence::NewWave(
		Outer,
		bCollapse ? TEXT("IGCrtChannelCollapse") : TEXT("IGCrtChannelAcquire"));
	TArray<FIGToneNote> SwitchNotes;

	// Korean mains is 60 Hz, so the hum under every tube in this building is 60
	// and its harmonics — not the 50 a European monitor would give.
	constexpr float MainsHz = 60.0f;
	// NTSC horizontal line rate. Mostly felt rather than heard, and that is the
	// point: it is the difference between a screen and a picture of a screen.
	constexpr float LineWhineHz = 15734.0f;

	if (!bCollapse)
	{
		// Acquire. Snow first and loudest, because the tube shows noise before it
		// shows anything; the hum rises underneath as the input takes hold.
		SwitchNotes.Add({0.000f, 0.350f, 9000.0f, 0.185f, 0.004f, 2.6f, EIGToneWaveform::ValueNoise});
		SwitchNotes.Add({0.000f, 0.320f, 4200.0f, 0.120f, 0.004f, 2.4f, EIGToneWaveform::ValueNoise});
		SwitchNotes.Add({0.000f, 0.260f, 1500.0f, 0.062f, 0.006f, 2.8f, EIGToneWaveform::ValueNoise});
		// Two horizontal tears while the sync separator hunts.
		SwitchNotes.Add({0.058f, 0.020f, 2600.0f, 0.090f, 0.002f, 3.4f, EIGToneWaveform::ValueNoise});
		SwitchNotes.Add({0.148f, 0.016f, 3300.0f, 0.070f, 0.002f, 3.6f, EIGToneWaveform::ValueNoise});
		// The hum arrives and stays — it is handed over to the looping bed.
		SwitchNotes.Add({0.040f, 0.320f, MainsHz, 0.052f, 0.520f, 0.5f, EIGToneWaveform::Sine});
		SwitchNotes.Add({0.040f, 0.320f, MainsHz * 2.0f, 0.030f, 0.560f, 0.5f, EIGToneWaveform::Sine});
		SwitchNotes.Add({0.090f, 0.270f, LineWhineHz, 0.012f, 0.600f, 0.6f, EIGToneWaveform::Sine});
	}
	else
	{
		// Death. The picture is torn away rather than faded: the hiss swells into
		// the tear, and the hum is the last thing to go because the tube keeps its
		// charge for a moment after it loses the signal.
		SwitchNotes.Add({0.000f, 0.240f, 6200.0f, 0.090f, 0.340f, 1.2f, EIGToneWaveform::ValueNoise});
		SwitchNotes.Add({0.180f, 0.520f, 9600.0f, 0.205f, 0.030f, 1.9f, EIGToneWaveform::ValueNoise});
		SwitchNotes.Add({0.180f, 0.480f, 3800.0f, 0.135f, 0.030f, 1.8f, EIGToneWaveform::ValueNoise});
		SwitchNotes.Add({0.180f, 0.400f, 1200.0f, 0.058f, 0.040f, 2.0f, EIGToneWaveform::ValueNoise});
		// Three tears, closer together each time: the channel is not coming back.
		SwitchNotes.Add({0.196f, 0.026f, 2200.0f, 0.115f, 0.002f, 3.2f, EIGToneWaveform::ValueNoise});
		SwitchNotes.Add({0.352f, 0.022f, 2900.0f, 0.100f, 0.002f, 3.4f, EIGToneWaveform::ValueNoise});
		SwitchNotes.Add({0.470f, 0.018f, 3600.0f, 0.082f, 0.002f, 3.6f, EIGToneWaveform::ValueNoise});
		SwitchNotes.Add({0.000f, 0.760f, MainsHz, 0.048f, 0.060f, 1.1f, EIGToneWaveform::Sine});
		SwitchNotes.Add({0.000f, 0.700f, MainsHz * 2.0f, 0.026f, 0.060f, 1.3f, EIGToneWaveform::Sine});
		SwitchNotes.Add({0.000f, 0.560f, LineWhineHz, 0.011f, 0.040f, 1.6f, EIGToneWaveform::Sine});
		// The four-way split settling back in: one relay-quiet thump of the
		// deflection yoke, and then the booth is as silent as it was.
		SwitchNotes.Add({0.690f, 0.130f, 210.0f, 0.040f, 0.020f, 2.6f, EIGToneWaveform::Sine});
	}

	Wave->ConfigureNotes(MoveTemp(SwitchNotes), false);
	return Wave;
}

UIGToneSequenceSoundWave* UIGToneSequenceSoundWave::CreateCrtChannelBed(
	UObject* Outer,
	const float TotalSeconds)
{
	UIGToneSequenceSoundWave* Wave =
		IGToneSequence::NewWave(Outer, TEXT("IGCrtChannelBed"));
	TArray<FIGToneNote> HumNotes;

	const float Seconds = FMath::Clamp(TotalSeconds, 1.0f, 30.0f);
	constexpr float MainsHz = 60.0f;
	constexpr float LineWhineHz = 15734.0f;

	// One long note per partial. The 0.25 release power is the synth's flattest
	// decay, so the bed holds near full for most of the window and only gives up
	// the last of itself at the end — the tube dimming as the input goes, not a
	// fade someone applied. Quiet enough that the §10.2 listening duck still owns
	// the room: this must never compete with what she is trying to hear.
	HumNotes.Add({0.000f, Seconds, MainsHz, 0.034f, 0.045f, 0.25f, EIGToneWaveform::Sine});
	HumNotes.Add({0.000f, Seconds, MainsHz * 2.0f, 0.021f, 0.050f, 0.25f, EIGToneWaveform::Sine});
	HumNotes.Add({0.000f, Seconds, MainsHz * 3.0f, 0.010f, 0.055f, 0.25f, EIGToneWaveform::Sine});
	HumNotes.Add({0.000f, Seconds, 7400.0f, 0.014f, 0.040f, 0.25f, EIGToneWaveform::ValueNoise});
	HumNotes.Add({0.000f, Seconds, LineWhineHz, 0.009f, 0.060f, 0.25f, EIGToneWaveform::Sine});

	Wave->ConfigureNotes(MoveTemp(HumNotes), false);
	// Analog sync was never stable. A third of a percent at 0.6 Hz is under the
	// threshold of hearing it as vibrato and over the threshold of hearing the
	// difference between this and a synthesised tone.
	Wave->ConfigurePitchWow(0.003f, 0.6f);
	return Wave;
}

UIGToneSequenceSoundWave* UIGToneSequenceSoundWave::CreatePipeWaterFlow(
	UObject* Outer,
	const int32 DistanceStep)
{
	UIGToneSequenceSoundWave* Wave =
		IGToneSequence::NewWave(Outer, TEXT("IGPipeWaterFlow"));
	TArray<FIGToneNote> FlowNotes;

	// §21.3 원근 4단. Concrete and board are a low-pass filter, so the band the
	// water arrives in is the distance it travelled. Nothing else needs to say
	// how far away it is.
	const int32 Step = FMath::Clamp(DistanceStep, 0, 3);
	const float Bandwidths[] = {5000.0f, 2400.0f, 1100.0f, 480.0f};
	const float Levels[] = {0.085f, 0.062f, 0.042f, 0.026f};
	const float Bandwidth = Bandwidths[Step];
	const float Level = Levels[Step];
	constexpr float LoopSeconds = 1.60f;

	// Two overlapping halves so the loop seam never lands on a silence, and a
	// slow body under them: moving water is never a steady tone.
	FlowNotes.Add({0.000f, 0.96f, Bandwidth, Level, 0.180f, 0.9f, EIGToneWaveform::ValueNoise});
	FlowNotes.Add({0.780f, 0.96f, Bandwidth * 0.92f, Level * 0.94f, 0.200f, 0.9f, EIGToneWaveform::ValueNoise});
	FlowNotes.Add({0.000f, LoopSeconds, Bandwidth * 0.26f, Level * 0.55f, 0.250f, 0.8f, EIGToneWaveform::ValueNoise});

	// Ticks are the sound of water hitting the inside of a pipe, and only the
	// near steps keep them: through two walls the ticks are gone before the hum.
	if (Step <= 1)
	{
		const float TickStarts[] = {0.113f, 0.402f, 0.667f, 0.941f, 1.284f};
		for (int32 TickIndex = 0; TickIndex < UE_ARRAY_COUNT(TickStarts); ++TickIndex)
		{
			FlowNotes.Add({
				TickStarts[TickIndex],
				0.016f,
				Bandwidth * (0.70f + 0.06f * (TickIndex % 3)),
				Level * (Step == 0 ? 0.52f : 0.30f),
				0.010f,
				1.6f,
				EIGToneWaveform::ValueNoise});
		}
	}

	Wave->ConfigureNotes(MoveTemp(FlowNotes), true, LoopSeconds);
	return Wave;
}

UIGToneSequenceSoundWave* UIGToneSequenceSoundWave::CreateWallCavityResponse(
	UObject* Outer,
	const bool bHollow)
{
	UIGToneSequenceSoundWave* Wave = IGToneSequence::NewWave(
		Outer,
		bHollow ? TEXT("IGWallCavityHollow") : TEXT("IGWallCavitySolid"));
	TArray<FIGToneNote> WallNotes;

	// The same excitation both times: an ear settling against board while the
	// riser drives it from behind. If this differed, the player would be reading
	// the contact instead of the wall.
	WallNotes.Add({0.000f, 0.030f, 2600.0f, 0.038f, 0.010f, 1.4f, EIGToneWaveform::ValueNoise});

	if (bHollow)
	{
		// 105 Hz is the mass-air-mass resonance of a real cavity stud wall — two
		// leaves rocking on the air between them. Low release powers keep it
		// ringing for a second and a half: 빈 벽은 길게 운다.
		WallNotes.Add({0.006f, 1.450f, 105.0f, 0.130f, 0.014f, 0.95f, EIGToneWaveform::Sine});
		// The cavity's own axial modes: about 2.4 m tall gives ~71 Hz, and the
		// shaft-side gap of roughly 40 cm gives ~430 Hz.
		WallNotes.Add({0.010f, 1.320f, 71.0f, 0.072f, 0.020f, 1.05f, EIGToneWaveform::Sine});
		WallNotes.Add({0.004f, 0.880f, 215.0f, 0.048f, 0.012f, 1.15f, EIGToneWaveform::Triangle});
		WallNotes.Add({0.008f, 0.620f, 430.0f, 0.030f, 0.014f, 1.30f, EIGToneWaveform::Sine});
		// Water heard through nothing but air, arriving with the ring.
		WallNotes.Add({0.020f, 1.180f, 4200.0f, 0.026f, 0.120f, 1.10f, EIGToneWaveform::ValueNoise});
	}
	else
	{
		// No air spring, no resonance. One damped board note and it is over in
		// under a third of a second: 속이 찬 벽은 짧게 죽는다.
		WallNotes.Add({0.004f, 0.220f, 150.0f, 0.110f, 0.016f, 3.00f, EIGToneWaveform::Sine});
		WallNotes.Add({0.004f, 0.140f, 320.0f, 0.042f, 0.014f, 3.40f, EIGToneWaveform::Triangle});
		// Whatever water reaches here came the long way round, through mass.
		WallNotes.Add({0.018f, 0.300f, 520.0f, 0.020f, 0.140f, 2.40f, EIGToneWaveform::ValueNoise});
	}

	Wave->ConfigureNotes(MoveTemp(WallNotes), false);
	return Wave;
}

UIGToneSequenceSoundWave* UIGToneSequenceSoundWave::CreateValveOpen(
	UObject* Outer,
	const int32 ValveIndex)
{
	UIGToneSequenceSoundWave* Wave =
		IGToneSequence::NewWave(Outer, TEXT("IGValveOpen"));
	TArray<FIGToneNote> ValveNotes;

	// §21.3: 1.8 kHz metal ringing plus a 1.2 s ramp of water starting to move,
	// 2.00 s total. Bigger wheels ring lower and fill slower — the three
	// authored valves stay distinguishable by ear alone (§10.3 밸브 3종).
	const int32 Index = FMath::Clamp(ValveIndex, 0, 2);
	const float RingHz[] = {1800.0f, 1520.0f, 2150.0f};
	const float FillSeconds[] = {1.20f, 1.34f, 1.06f};
	const float Ring = RingHz[Index];
	const float Fill = FillSeconds[Index];

	// The stem breaking free, then the wheel turning in three dry clicks.
	ValveNotes.Add({0.000f, 0.048f, 1400.0f, 0.070f, 0.008f, 1.8f, EIGToneWaveform::ValueNoise});
	ValveNotes.Add({0.000f, 0.340f, Ring, 0.062f, 0.006f, 2.6f, EIGToneWaveform::Sine});
	ValveNotes.Add({0.000f, 0.520f, Ring * 0.5f, 0.034f, 0.010f, 2.2f, EIGToneWaveform::Triangle});
	const float ClickStarts[] = {0.185f, 0.372f, 0.548f};
	for (const float ClickStart : ClickStarts)
	{
		ValveNotes.Add({ClickStart, 0.026f, 1100.0f, 0.038f, 0.010f, 1.7f, EIGToneWaveform::ValueNoise});
		ValveNotes.Add({ClickStart, 0.150f, Ring * 0.92f, 0.026f, 0.008f, 2.8f, EIGToneWaveform::Sine});
	}

	// Water arriving: the band opens up over the fill as the pipe charges, so
	// the ramp is heard as pressure building rather than a fade-in.
	constexpr int32 RampSteps = 6;
	for (int32 RampIndex = 0; RampIndex < RampSteps; ++RampIndex)
	{
		const float Alpha = static_cast<float>(RampIndex) / (RampSteps - 1);
		const float Start = 0.62f + Fill * Alpha * 0.82f;
		ValveNotes.Add({
			Start,
			Fill * 0.42f,
			FMath::Lerp(620.0f, 3400.0f, Alpha),
			FMath::Lerp(0.020f, 0.058f, Alpha),
			0.220f,
			1.1f,
			EIGToneWaveform::ValueNoise});
	}
	// Settled flow holding the last of the two seconds.
	ValveNotes.Add({1.520f, 0.480f, 2600.0f, 0.044f, 0.180f, 1.2f, EIGToneWaveform::ValueNoise});
	ValveNotes.Add({1.520f, 0.480f, 700.0f, 0.026f, 0.200f, 1.1f, EIGToneWaveform::ValueNoise});

	Wave->ConfigureNotes(MoveTemp(ValveNotes), false);
	return Wave;
}

UIGToneSequenceSoundWave* UIGToneSequenceSoundWave::CreateHammerImpact(
	UObject* Outer,
	const int32 StrikeIndex)
{
	UIGToneSequenceSoundWave* Wave =
		IGToneSequence::NewWave(Outer, TEXT("IGHammerImpact"));
	TArray<FIGToneNote> HammerNotes;

	// §21.3: a 90 Hz impulse, gypsum fracture, and 1.4 s of the building
	// answering, over 1.60 s. §10.3 wants the fracture in three stages, and the
	// stage is the information: the player hears the wall going, not a counter.
	const int32 Strike = FMath::Max(0, StrikeIndex);
	const int32 Stage = Strike <= 1 ? 0 : (Strike <= 3 ? 1 : 2);
	const float StageGain[] = {0.62f, 0.84f, 1.00f};
	const float Gain = StageGain[Stage];

	// The head landing. Low, and the same every time — the arm does not change.
	HammerNotes.Add({0.000f, 0.180f, 90.0f, 0.300f, 0.003f, 2.4f, EIGToneWaveform::Sine});
	HammerNotes.Add({0.000f, 0.090f, 240.0f, 0.120f, 0.004f, 2.8f, EIGToneWaveform::Triangle});
	HammerNotes.Add({0.000f, 0.022f, 3200.0f, 0.090f * Gain, 0.006f, 1.5f, EIGToneWaveform::ValueNoise});

	// Stage 0: the board bruises and holds. A dull crush, nothing separating.
	// Stage 1: paper tears and the core starts letting go.
	// Stage 2: it breaks through — pieces, and the cavity behind them.
	const int32 FractureCount = 3 + Stage * 3;
	for (int32 FractureIndex = 0; FractureIndex < FractureCount; ++FractureIndex)
	{
		const float Spread = static_cast<float>(FractureIndex) / FractureCount;
		HammerNotes.Add({
			0.026f + Spread * (0.140f + 0.120f * Stage),
			0.020f + 0.014f * (FractureIndex % 3),
			FMath::Lerp(1400.0f, 4600.0f, FMath::Frac(Spread * 2.7f)),
			(0.034f + 0.016f * Stage) * (1.0f - Spread * 0.45f),
			0.008f,
			1.4f,
			EIGToneWaveform::ValueNoise});
	}
	if (Stage == 2)
	{
		// The cavity is open now, so its mass-air-mass note rings free instead
		// of being muffled by the board that used to close it.
		HammerNotes.Add({0.060f, 0.900f, 105.0f, 0.090f, 0.014f, 1.10f, EIGToneWaveform::Sine});
	}

	// 1.4 s of building. Concrete keeps the high end, which is why a hammer at
	// 04:30 is the loudest mistake available (§5.1 소음 1.0).
	HammerNotes.Add({0.040f, 1.400f, 170.0f, 0.070f * Gain, 0.030f, 1.20f, EIGToneWaveform::ValueNoise});
	HammerNotes.Add({0.055f, 1.320f, 900.0f, 0.040f * Gain, 0.060f, 1.35f, EIGToneWaveform::ValueNoise});
	HammerNotes.Add({0.070f, 1.180f, 2400.0f, 0.024f * Gain, 0.090f, 1.50f, EIGToneWaveform::ValueNoise});
	// Dust coming off the break, arriving last.
	HammerNotes.Add({0.320f, 1.280f, 6800.0f, 0.016f * Gain, 0.140f, 2.60f, EIGToneWaveform::ValueNoise});

	Wave->ConfigureNotes(MoveTemp(HammerNotes), false);
	return Wave;
}

UIGToneSequenceSoundWave* UIGToneSequenceSoundWave::CreateWallpaperSeamRoller(
	UObject* Outer)
{
	UIGToneSequenceSoundWave* Wave =
		IGToneSequence::NewWave(Outer, TEXT("IGWallpaperSeamRoller"));
	TArray<FIGToneNote> RollerNotes;

	// 이음 롤러는 페인트 롤러가 아니라 작은 고무 원통이다. 종이와 풀의
	// 넓은 마찰을 중심으로 두고, 방향이 바뀔 때만 작은 축이 반응한다.
	// 기계나 프린터처럼 규칙적으로 들리지 않도록 두 번의 길이를 다르게 둔다.
	RollerNotes.Add({0.000f, 1.48f, 1280.0f, 0.060f, 0.160f, 1.15f, EIGToneWaveform::ValueNoise});
	RollerNotes.Add({0.040f, 1.40f, 3600.0f, 0.025f, 0.220f, 1.30f, EIGToneWaveform::ValueNoise});
	RollerNotes.Add({0.080f, 1.26f, 185.0f, 0.022f, 0.180f, 1.20f, EIGToneWaveform::Triangle});
	RollerNotes.Add({1.510f, 0.055f, 780.0f, 0.048f, 0.006f, 2.6f, EIGToneWaveform::ValueNoise});
	RollerNotes.Add({1.510f, 0.120f, 142.0f, 0.034f, 0.008f, 2.8f, EIGToneWaveform::Sine});
	RollerNotes.Add({1.700f, 1.76f, 1120.0f, 0.057f, 0.180f, 1.10f, EIGToneWaveform::ValueNoise});
	RollerNotes.Add({1.760f, 1.66f, 3300.0f, 0.022f, 0.240f, 1.35f, EIGToneWaveform::ValueNoise});
	RollerNotes.Add({1.780f, 1.52f, 168.0f, 0.020f, 0.220f, 1.25f, EIGToneWaveform::Triangle});
	RollerNotes.Add({3.490f, 0.070f, 620.0f, 0.043f, 0.008f, 3.0f, EIGToneWaveform::ValueNoise});

	Wave->ConfigureNotes(MoveTemp(RollerNotes), false);
	return Wave;
}
