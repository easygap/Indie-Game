#pragma once

#include "CoreMinimal.h"
#include "Sound/SoundWaveProcedural.h"
#include "IGToneSequenceSoundWave.generated.h"

/** Oscillator shape for one scheduled note. */
enum class EIGToneWaveform : uint8
{
	Sine,
	/** Odd-harmonic blend that reads as a soft electronic square. */
	SoftSquare,
	Triangle,
	/** Band-limited value noise; Frequency acts as the noise bandwidth in Hz. */
	ValueNoise
};

/**
 * One note in a tone sequence. All fields are immutable once playback starts.
 * The amplitude envelope is a smooth attack over AttackFraction of the note,
 * followed by a (1 - t)^ReleasePower decay to zero.
 */
struct FIGToneNote
{
	float StartSeconds = 0.0f;
	float DurationSeconds = 0.1f;
	float FrequencyHz = 440.0f;
	float Amplitude = 0.1f;
	float AttackFraction = 0.02f;
	float ReleasePower = 1.0f;
	EIGToneWaveform Waveform = EIGToneWaveform::Sine;
};

/**
 * Allocation-stable procedural PCM source that mixes a fixed schedule of notes.
 * It backs every melodic or percussive cue in the prologue: the store jingle,
 * the outdoor dread drone, the entrance chime, scanner/register cues, door
 * creaks and footsteps.
 *
 * Build the note list on the game thread via the static factories (or
 * ConfigureNotes) before handing the wave to an audio component; afterwards
 * the sample cursor is owned exclusively by the audio render thread.
 */
UCLASS()
class INDIEGAME_API UIGToneSequenceSoundWave final : public USoundWaveProcedural
{
	GENERATED_BODY()

public:
	explicit UIGToneSequenceSoundWave(const FObjectInitializer& ObjectInitializer);

	/**
	 * Installs the note schedule. When bInLooping is true the pattern repeats
	 * every LoopSeconds; otherwise the wave renders silence after the last
	 * note and stops at its finite Duration.
	 */
	void ConfigureNotes(TArray<FIGToneNote>&& InNotes, bool bInLooping, float LoopSeconds = 0.0f);

	virtual int32 OnGeneratePCMAudio(TArray<uint8>& OutAudio, int32 NumSamples) override;

	/** "Ding-dong" two-tone convenience-store entrance chime. */
	static UIGToneSequenceSoundWave* CreateDoorChime(UObject* Outer);

	/** Short descending squeak for a swinging hinge. */
	static UIGToneSequenceSoundWave* CreateDoorCreak(UObject* Outer);

	/** Low thud used when a door settles shut. */
	static UIGToneSequenceSoundWave* CreateDoorThud(UObject* Outer);

	/** Brief handle rattle for a locked door. */
	static UIGToneSequenceSoundWave* CreateLockedRattle(UObject* Outer);

	/** Single barcode-scanner beep. */
	static UIGToneSequenceSoundWave* CreateScannerBeep(UObject* Outer);

	/** Register confirmation beeps with a cash-drawer clunk. */
	static UIGToneSequenceSoundWave* CreateRegisterSound(UObject* Outer);

	/**
	 * Looping cheerful music-box store jingle.
	 * PitchSemitones and TimeScale author the degraded CH02 version without
	 * changing global audio-component pitch (which would couple both values).
	 */
	static UIGToneSequenceSoundWave* CreateStoreJingle(
		UObject* Outer,
		float PitchSemitones = 0.0f,
		float TimeScale = 1.0f);

	/** Looping low minor-cluster drone for the pre-dawn alley. */
	static UIGToneSequenceSoundWave* CreateDreadDrone(UObject* Outer);

	/**
	 * Soft shoe-on-floor step. PitchScale shifts the surface character
	 * (lower = duller wood, higher = harder tile) and Amplitude scales loudness.
	 */
	static UIGToneSequenceSoundWave* CreateFootstep(UObject* Outer, float PitchScale, float Amplitude);

private:
	static float EvaluateWaveform(EIGToneWaveform Waveform, float FrequencyHz, double NoteTimeSeconds);
	static float EvaluateEnvelope(const FIGToneNote& Note, float NoteProgress01);

	// Immutable after ConfigureNotes; read from the audio render thread.
	TArray<FIGToneNote> Notes;
	int64 LoopSampleCount = 0;
	int64 TotalSampleCount = 0;

	// Render-thread-owned sample cursor.
	uint64 GeneratedSampleCount = 0;
};
