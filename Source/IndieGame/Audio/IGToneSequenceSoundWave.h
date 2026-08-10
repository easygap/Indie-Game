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
	/** Applies a small integrated pitch drift without changing sequence timing. */
	void ConfigurePitchWow(float DepthRatio, float RateHz);
	/** Finite authored length, including the protected release tail. */
	float GetConfiguredDurationSeconds() const { return Duration; }
	/** True when ConfigureNotes installed an indefinitely repeating pattern. */
	bool IsConfiguredLooping() const { return bLooping; }

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
	 * A dry relay crack followed by the short 120 Hz ring of a failing
	 * fluorescent ballast. Kept separate from footsteps so a light going out
	 * reads as an electrical event even when it happens behind the player.
	 */
	static UIGToneSequenceSoundWave* CreateFluorescentBallastSnap(UObject* Outer);

	/**
	 * One water drop striking thin elevator metal: a soft liquid impact first,
	 * then a narrow, lingering metal resonance.
	 */
	static UIGToneSequenceSoundWave* CreateWaterDripMetalRing(UObject* Outer);

	/**
	 * Looping, unintelligible low-band radio cadence heard through unit 401's
	 * closed door. It suggests an early-morning prayer broadcast without
	 * synthesizing words or a recognizable human voice.
	 */
	static UIGToneSequenceSoundWave* CreateMuffledPrayerRadio(UObject* Outer);

	/** One rough cardboard scrape and a small box-settle thump. */
	static UIGToneSequenceSoundWave* CreateCardboardDrag(UObject* Outer);

	/** Dry paper lift and fingertip brush used by the daylight evidence journal. */
	static UIGToneSequenceSoundWave* CreateJournalPageTurn(UObject* Outer);

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

	/** Looping CH03 flooded-corridor bed with low water mass and sparse drops. */
	static UIGToneSequenceSoundWave* CreateFloodedCorridorWaterBed(UObject* Outer);

	/**
	 * Soft shoe-on-floor step. PitchScale shifts the surface character
	 * (lower = duller wood, higher = harder tile) and Amplitude scales loudness.
	 */
	static UIGToneSequenceSoundWave* CreateFootstep(UObject* Outer, float PitchScale, float Amplitude);

	/**
	 * The isolated first note of the 04:44 alarm pattern. CH02 plays it once
	 * in place of a call-connect tone; ending A replays it inside the blackout.
	 */
	static UIGToneSequenceSoundWave* CreateAlarmFirstNote(UObject* Outer);

	/** Two flat descending handset beeps that end a failed call attempt. */
	static UIGToneSequenceSoundWave* CreateCallFailTone(UObject* Outer);

	/** Rounded residential doorbell, softer and lower than the store chime. */
	static UIGToneSequenceSoundWave* CreateDoorbellChime(UObject* Outer);

	/** Tiny dry relay click for a small appliance switching off. */
	static UIGToneSequenceSoundWave* CreateRelayClick(UObject* Outer);

	// --- CH01 04:33 drink beat --------------------------------------------

	/** PET cap cracked open: security-ring snap and a short thread rasp. */
	static UIGToneSequenceSoundWave* CreateBottleCapOpen(UObject* Outer);

	/** A few unhurried swallows straight from the bottle. */
	static UIGToneSequenceSoundWave* CreateWaterSwallows(UObject* Outer);

	/** The cap ratcheted back shut in one motion. */
	static UIGToneSequenceSoundWave* CreateBottleReseal(UObject* Outer);

	/** Loaded carrier bag settling onto concrete: crinkle, then bottle knock. */
	static UIGToneSequenceSoundWave* CreatePlasticBagSetDown(UObject* Outer);

	/** The same bag gathered and lifted: stretch creak and a light clink. */
	static UIGToneSequenceSoundWave* CreatePlasticBagLift(UObject* Outer);

	// --- P1 / P2 pressure cues --------------------------------------------

	/** Duvet weight settling once: cloth friction with no breath rhythm. */
	static UIGToneSequenceSoundWave* CreateClothSettle(UObject* Outer);

	/** Short shutter-motor burst lowering the front grille one step. */
	static UIGToneSequenceSoundWave* CreateShutterMotorStep(UObject* Outer);

	/** Longer motor run with an end stop, used when the shutter fully rises. */
	static UIGToneSequenceSoundWave* CreateShutterMotorRise(UObject* Outer);

	/** Thermal printer feeding a short strip of blank paper. */
	static UIGToneSequenceSoundWave* CreateThermalPrinterFeed(UObject* Outer);

	/** Only the first two notes of the healthy CH01 jingle, played once. */
	static UIGToneSequenceSoundWave* CreateJingleOpeningNotes(UObject* Outer);

	// --- shared ending transition -----------------------------------------

	/** Multigas detector self-check: two clean passes and a confirm chirp. */
	static UIGToneSequenceSoundWave* CreateGasDetectorOk(UObject* Outer);

	/** Flexible duct unfolding, then a ventilation fan holding low RPM. */
	static UIGToneSequenceSoundWave* CreateVentDuctSpinUp(UObject* Outer);

	/** Safety-harness buckle, carabiner gate and webbing pulled tight. */
	static UIGToneSequenceSoundWave* CreateHarnessBuckle(UObject* Outer);

	/** Two people on an exterior ladder with distinctly different footfalls. */
	static UIGToneSequenceSoundWave* CreateLadderClimbTwoPeople(UObject* Outer);

	/** The real access hatch: latch turn, heavy hinge sweep, settle. */
	static UIGToneSequenceSoundWave* CreateHatchOpenMetal(UObject* Outer);

	// --- ending A ---------------------------------------------------------

	/**
	 * A phone vibrating far away on a desk. The pattern dies before its third
	 * bar completes; the screen never lights, so the sound is all there is.
	 */
	static UIGToneSequenceSoundWave* CreatePhoneVibrationUnfinished(UObject* Outer);

	/** Water lapped from a plastic cap; optionally cut off mid-lick. */
	static UIGToneSequenceSoundWave* CreateCatLickWaterPlastic(UObject* Outer, bool bCutMid);

	/** Wet paper cup and tongue, cut off mid-lick. */
	static UIGToneSequenceSoundWave* CreateCatLickWaterPaper(UObject* Outer);

	/**
	 * Small paws crossing hard ground. Steps counts the footfalls; bCutMid
	 * ends the run abruptly the way ending A's blackout cues stop.
	 */
	static UIGToneSequenceSoundWave* CreateCatPawTrot(UObject* Outer, int32 Steps, bool bCutMid);

	// --- ending B ---------------------------------------------------------

	/** The inspection rod sliding into its support groove and seating. */
	static UIGToneSequenceSoundWave* CreateRodWedgeSeat(UObject* Outer);

	/** A glasses temple touching a railing: one very small ring. */
	static UIGToneSequenceSoundWave* CreateGlassesTinyRing(UObject* Outer);

	/**
	 * The whole ending-B goodbye montage over black, in order: 404 door lock,
	 * a drawer, four books boxed, box tape torn in two pulls (0.6 s of air on
	 * both sides), a work-vest zip, one cracked-phone buzz, the roof door, a
	 * ceramic bowl set on concrete, water poured, and finally cloth folded
	 * with a single audible breath standing in for the unrecorded voice line.
	 */
	static UIGToneSequenceSoundWave* CreateEndingBMontage(UObject* Outer);

	/**
	 * M5 "Return Home": 45 seconds at 52 BPM, rebuilding the first three
	 * alarm intervals from glass-rim tones over refrigerator harmonics.
	 */
	static UIGToneSequenceSoundWave* CreateEndingBReturnHomeBed(UObject* Outer);

	/** Bright, ordinary spring morning bed: sparrows and a distant scooter. */
	static UIGToneSequenceSoundWave* CreateSpringMorningBed(UObject* Outer);

	/** One clear water drop landing in an empty glass cup. No second drop. */
	static UIGToneSequenceSoundWave* CreateGlassCupDrip(UObject* Outer);

	/** One short, unalarmed cat mewl from very close to the ground. */
	static UIGToneSequenceSoundWave* CreateCatShortMewl(UObject* Outer);

	// --- The Missing Floor: the one upstairs ------------------------------

	/** Close, exhausted inhale/exhale loop for the five-dawn black interlude. */
	static UIGToneSequenceSoundWave* CreateTrappedBreathBed(UObject* Outer);

	/**
	 * Three deliberate knuckle knocks on a stud wall, evenly spaced. The
	 * entity's idle cycle: it knocks, then listens. Muffle01 rolls off the
	 * contact click for playback through a closed wall (1 = fully entombed).
	 */
	static UIGToneSequenceSoundWave* CreateWallKnockTriple(UObject* Outer, float Muffle01 = 0.0f);

	/** One player-timed knuckle tap; P4 assembles three calls into its rhythm. */
	static UIGToneSequenceSoundWave* CreateWallKnockSingle(
		UObject* Outer,
		float Muffle01 = 0.0f);

	/**
	 * Two soft knocks, close together: the calmed reply it gives when an
	 * answer reaches it, and the last thing a captured player hears.
	 */
	static UIGToneSequenceSoundWave* CreateWallKnockReply(UObject* Outer);

	/**
	 * The family signal: two, a rest, one — "문 열어, 나야." The player's
	 * P4 answer and, muffled, the reply that comes back through the studs.
	 */
	static UIGToneSequenceSoundWave* CreateAnswerKnockPattern(
		UObject* Outer,
		float Muffle01 = 0.0f);

	/**
	 * Looping crawl bed for the entity: palm plant, a long dry drag of
	 * cloth-and-weight over concrete, and a plaster grit tail. Volume is
	 * driven by movement speed so silence means it is holding still.
	 */
	static UIGToneSequenceSoundWave* CreateEntityDragLoop(UObject* Outer);

	/**
	 * Hardened plaster shell settling: two or three dry hairline cracks.
	 * Played when the entity stops moving to listen.
	 */
	static UIGToneSequenceSoundWave* CreatePlasterSettle(UObject* Outer);

private:
	static float EvaluateWaveform(EIGToneWaveform Waveform, float FrequencyHz, double NoteTimeSeconds);
	static float EvaluateEnvelope(const FIGToneNote& Note, float NoteProgress01);

	// Immutable after ConfigureNotes; read from the audio render thread.
	TArray<FIGToneNote> Notes;
	int64 LoopSampleCount = 0;
	int64 TotalSampleCount = 0;
	float PitchWowDepthRatio = 0.0f;
	float PitchWowRateHz = 0.0f;

	// Render-thread-owned sample cursor.
	uint64 GeneratedSampleCount = 0;
};
