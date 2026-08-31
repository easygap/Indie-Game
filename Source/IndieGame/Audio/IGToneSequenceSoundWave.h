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

/** Physical floor families used by the §21.2 stealth/noise matrix. */
UENUM(BlueprintType)
enum class EIGFootstepSurface : uint8
{
	Vinyl,
	Concrete,
	MetalStair,
	Rooftop,
	GypsumDebris,
	Water
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

	/** 문틈을 통과해 복도 타일에 안착하는 얇은 종이 소리. */
	static UIGToneSequenceSoundWave* CreatePaperDoorSlide(UObject* Outer);

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

	/** Surface-authored footfall; gameplay loudness is applied by the caller. */
	static UIGToneSequenceSoundWave* CreateSurfaceFootstep(
		UObject* Outer,
		EIGFootstepSurface Surface,
		float VariationPitch,
		float Amplitude = 1.0f);

	/** M-조율: 220 Hz triangle, -30 cents, rising two cents per strike. */
	static UIGToneSequenceSoundWave* CreateTuningMotif(
		UObject* Outer,
		bool bResolvedEndingA);

	/** One M-조율 strike for a newly crossed truth; index rises by two cents. */
	static UIGToneSequenceSoundWave* CreateTuningStrike(
		UObject* Outer,
		int32 ConfirmationIndex);

	/** M-공동: 44 Hz cavity mass, 180 Hz value noise and sparse water ticks. */
	static UIGToneSequenceSoundWave* CreateCavityDrone(UObject* Outer);

	/** 118 BPM pursuit loop: 52 Hz pulse plus a 220/223/227 Hz cluster. */
	static UIGToneSequenceSoundWave* CreateChaseScore(UObject* Outer);

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
	 *
	 * 끌림 2종 (§10.3): concrete is gritty and carries low. 장판 is thin vinyl
	 * over screed, so it loses the rumble, hisses higher, and squeaks where the
	 * cloth sticks and slips. Which floor he is on is a fact about *where* he
	 * is, and a player who has learned both hears him come inside.
	 */
	static UIGToneSequenceSoundWave* CreateEntityDragLoop(
		UObject* Outer,
		bool bVinyl = false);

	/**
	 * Hardened plaster shell settling: two or three dry hairline cracks.
	 * Played when the entity stops moving to listen.
	 */
	static UIGToneSequenceSoundWave* CreatePlasterSettle(UObject* Outer);

	/**
	 * 분진 낙하 (§10.3, parameters fixed by §21.3) — the fine powder his drag
	 * scrapes off a joint, sifting down onto tile over 0.90 s. Deliberately the
	 * thinnest cue in the game: broadband hiss high-passed at 6 kHz with an
	 * exponential decay, and **no low frequency at all**. Powder has no mass, so
	 * anything lower would turn it into the wall itself moving — that is
	 * CreatePlasterSettle — or into falling debris, which is 미장 갈라짐.
	 *
	 * Being pure high frequency is also why it works: the §10.2 listening window
	 * drops the low end by 6 dB, so this survives the duck that swallows
	 * everything else, and it is the one cue that says "he passed here" rather
	 * than "he is here". It rides BUS_ENTITY, so §10.4 reverb tells the player
	 * whether the sift is two floors up or in this corridor.
	 */
	static UIGToneSequenceSoundWave* CreatePlasterDustFall(UObject* Outer);

	/**
	 * 채널 전환 지직임 (§8 비트 2-2) — an analog tube losing and finding sync.
	 *
	 * bCollapse false is the 0.35 s acquire: hiss decaying as the picture locks.
	 * bCollapse true is the 0.90 s death: hiss swelling, two sync tears, then
	 * nothing. The channel dies once and cannot be pressed again, so the collapse
	 * has to sound terminal rather than like a dropout that might come back.
	 *
	 * The hum is 60 Hz because the building is on Korean mains, and the thin
	 * 15.734 kHz line whine is the NTSC horizontal rate this monitor was built
	 * for. Both are there for the players who can hear them.
	 */
	static UIGToneSequenceSoundWave* CreateCrtChannelSwitch(
		UObject* Outer,
		bool bCollapse);

	/**
	 * 모니터 험 (§8 비트 2-2) — what a live tube sounds like when nobody is
	 * speaking: 60 Hz mains and its harmonics, a breath of hiss, the line whine.
	 * Very quiet. Playing only while channel 5 is up is deliberate — the 4분할
	 * monitor has been silent all night, so the hum arriving *with* the picture is
	 * the ear's confirmation that this input was never on before.
	 *
	 * Sized to the beat rather than looped, because a note envelope in this synth
	 * always returns to zero at the loop point and a hum that pulses once a second
	 * is worse than no hum. TotalSeconds should span the live window *and* the
	 * collapse, so the tube's slow dim ends underneath the tearing noise.
	 */
	static UIGToneSequenceSoundWave* CreateCrtChannelBed(
		UObject* Outer,
		float TotalSeconds);

	/**
	 * 배관 수류, 원근 4단 (§21.3) — the riser running behind the finished wall.
	 *
	 * DistanceStep 0..3 is how much building the water had to come through:
	 * bandwidths 5000 / 2400 / 1100 / 480 Hz. Structure is a low-pass filter, so
	 * the step *is* the distance, and the player reads it without being told.
	 * Close water still has audible ticks; far water is only a hum. Looping.
	 */
	static UIGToneSequenceSoundWave* CreatePipeWaterFlow(
		UObject* Outer,
		int32 DistanceStep);

	/**
	 * 옥상 물탱크의 출렁임 — 강판 안에서 2톤이 아주 느리게 오간다.
	 *
	 * 배관 수류와 다른 소리다. 관 속의 물은 계속 흐르지만 탱크의 물은
	 * 밀렸다가 돌아오며, 돌아오는 끝에서 벽을 한 번 친다. 주기가 4초를
	 * 넘는 것이 요점이다 — 이 느림이 「가득 차 있다」는 뜻이고, 그것이
	 * §13의 「물 2톤 옆의 갈증」을 만든다. Looping.
	 */
	static UIGToneSequenceSoundWave* CreateRooftopTankSlosh(UObject* Outer);

	/**
	 * The answer P3 is actually asking for: 속이 찬 벽은 짧게 죽고, 빈 벽은
	 * 길게 운다.
	 *
	 * A cavity wall is two leaves with an air spring between them, and that
	 * mass-air-mass system resonates — measured near 100–110 Hz in real stud
	 * walls. Driven by the riser it rings on. A solid wall has no air spring, so
	 * the same excitation dies almost immediately. This one difference is the
	 * whole puzzle, and it has to be audible: the thought bubble must confirm
	 * what the player already heard, never replace it.
	 */
	static UIGToneSequenceSoundWave* CreateWallCavityResponse(
		UObject* Outer,
		bool bHollow);

	/**
	 * 밸브 개방 (§21.3) — 1.8 kHz metal ringing plus a 1.2 s ramp of water
	 * starting to move, over 2.00 s. ValveIndex 0..2 picks one of the three
	 * authored wheels (§10.3); larger wheels ring lower and fill slower.
	 */
	static UIGToneSequenceSoundWave* CreateValveOpen(
		UObject* Outer,
		int32 ValveIndex);

	/**
	 * 망치 임팩트 (§21.3) — a 90 Hz impulse, gypsum fracture, and 1.4 s of the
	 * building answering, over 1.60 s. StrikeIndex escalates the fracture
	 * through the §10.3 three stages: the first blows bruise the board, the
	 * later ones break through it, and the sound has to say which.
	 */
	static UIGToneSequenceSoundWave* CreateHammerImpact(
		UObject* Outer,
		int32 StrikeIndex);

	/**
	 * 풀이 묻은 벽지를 누르는 고무 이음 롤러. 긴 상하 왕복 두 번과 젖은
	 * 종이 표면, 방향 전환 때의 작은 축 소리를 합성하며 엔딩 C에서만 쓴다.
	 */
	static UIGToneSequenceSoundWave* CreateWallpaperSeamRoller(UObject* Outer);

	/**
	 * 프로타주 문지름 (§21.3) — graphite laid flat and dragged over the carbon
	 * ledger until the pressed letters come up. Band noise 900~4200 Hz, looping
	 * for as long as the hold lasts.
	 *
	 * §5.1 rates this a sustained 0.25, three times a footstep, and until now it
	 * made no sound at all: the player rubbed for 1.2 s in silence while the
	 * noise bus told the one upstairs exactly where they were. A cost the player
	 * cannot hear is not a cost they can choose.
	 *
	 * The design says 입력 속도 연동. The shipped interaction is a hold rather
	 * than a rubbing gesture, so the hold's own progress drives the intensity —
	 * the stroke gets more insistent as the date surfaces.
	 */
	static UIGToneSequenceSoundWave* CreateFrottageRub(UObject* Outer);

	/**
	 * 심박 소음화 (§21.3, §5.2) — 자기 몸이 배신하는 소리.
	 *
	 * Past stress 0.85 the pulse stops being something the player hears in their
	 * head and becomes a sound in the room, audible to him within three meters
	 * (§4.3-5). That transition already reported to the noise bus, but it sounded
	 * identical, so the most dangerous state in the game had no tell.
	 *
	 * This is the ordinary lub-dub on the same beat, 6 dB down with the crisp
	 * upper partial gone — the 220 Hz low pass of §21.3, realised by dropping the
	 * partial rather than filtering, because this synth is additive. Played
	 * spatially so §10.4 lets the corridor answer it.
	 */
	static UIGToneSequenceSoundWave* CreateAudibleHeartbeat(
		UObject* Outer,
		float Loudness);

	// --- 없는 층: 엔딩 에필로그 (§9) ---------------------------------------

	/**
	 * 폴리스라인 테이프가 롤에서 당겨져 풀리는 소리. 몽타주의 첫 소리이며,
	 * 이 장면에서 처음으로 유담이 아닌 사람들이 건물에 들어온다.
	 */
	static UIGToneSequenceSoundWave* CreatePoliceLineTapePull(UObject* Outer);

	/** 들것 바퀴가 복도 타일 이음매를 넘어간다. 네 번, 점점 멀어진다. */
	static UIGToneSequenceSoundWave* CreateGurneyWheels(UObject* Outer);

	/** 현장 사진 셔터 세 번. 미러 슬랩과 얇은 모터 감김. */
	static UIGToneSequenceSoundWave* CreateCameraShutterTriple(UObject* Outer);

	/** 빗자루가 석고 조각을 쓸어 모은다. 마른 알갱이가 앞으로 밀리는 소리. */
	static UIGToneSequenceSoundWave* CreateDebrisSweep(UObject* Outer);

	/**
	 * 에필로그 1의 스코어. M-조율이 이 작품에서 유일하게 끝까지 간다.
	 *
	 * -30센트에서 시작해 여덟 타건에 걸쳐 220 Hz 정음으로 올라오고,
	 * 마지막에 열린 5도(A-E)를 한 번 누른 뒤 놓는다. 게임 내내 닿지
	 * 못하던 음이 여기서만 닿는 것이 §10.1의 계약이다.
	 */
	static UIGToneSequenceSoundWave* CreateEpilogueWorkshopScore(UObject* Outer);

	/**
	 * 에필로그 2의 베드. 크레인 유압의 아주 먼 저역과, 401호 창턱
	 * 라디오에서 새어 나오는 대역 제한 신호. 말은 만들지 않는다.
	 */
	static UIGToneSequenceSoundWave* CreateEpilogueAutumnBed(UObject* Outer);

	/** 열쇠 두 개가 중개사 반납함 철판 바닥에 떨어진다. */
	static UIGToneSequenceSoundWave* CreateKeyDropMetalBox(UObject* Outer);

	/**
	 * 계단 난간을 두 번. 응답 노크와 같은 손이지만 벽이 아니라 강관이라
	 * 저역 대신 금속 배음이 남는다. 이 게임의 마지막 입력의 소리다.
	 */
	static UIGToneSequenceSoundWave* CreateRailingKnockTwo(UObject* Outer);

	/**
	 * §5.5 기록되지 않는 시간 — a phone take played back through its own speaker.
	 *
	 * Built from the recording log rather than captured audio (§14), which is
	 * what makes the silences exact: a suppressed event contributes no notes at
	 * all and its duration simply passes. **노크가 있던 자리에 정확히 그 길이만큼의
	 * 무음.** Nothing is faded or crossfaded over the gap; an edit would be a
	 * different and much weaker idea than an absence.
	 *
	 * Everything sits above 400 Hz. A phone speaker has no low end, so the take
	 * is audibly a recording — and the knock, which lives at 58~80 Hz, could not
	 * have survived it even if the rule had let it through.
	 */
	static UIGToneSequenceSoundWave* CreateRecordingPlayback(
		UObject* Outer,
		const TArray<struct FIGRecordedSound>& Sounds);

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
