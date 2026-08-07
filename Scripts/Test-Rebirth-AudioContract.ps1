[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot

function Read-Source([string]$RelativePath) {
	Get-Content -Raw -Encoding UTF8 -LiteralPath (
		Join-Path $projectRoot $RelativePath)
}

$ambience = Read-Source 'Source/IndieGame/Audio/IGAmbienceSoundWave.cpp'
$tone = Read-Source 'Source/IndieGame/Audio/IGToneSequenceSoundWave.cpp'
$secondMorning = Read-Source 'Source/IndieGame/Sequence/IGSecondMorningDirector.cpp'
$thirdMorning = Read-Source 'Source/IndieGame/Sequence/IGThirdMorningDirector.cpp'
$stress = Read-Source 'Source/IndieGame/Player/IGStressComponent.cpp'
$assertions = 0

function Require-All(
	[string]$Source,
	[string[]]$Needles,
	[string]$ContractName) {
	foreach ($needle in $Needles) {
		if (-not $Source.Contains($needle)) {
			throw "$ContractName audio invariant is missing: $needle"
		}
		$script:assertions++
	}
}

# M0: the learned apartment bed must retain all four authored machine bands.
Require-All $ambience @(
	'StableSine(31.0f, SampleIndex)',
	'StableSine(60.0f, SampleIndex)',
	'StableSine(120.0f, SampleIndex)',
	'StableSine(240.0f, SampleIndex)'
) 'M0'

# M1/M1b: 8 bars at 76 BPM, then the exact -18 cent / -8 percent degradation.
Require-All $tone @(
	'BaseBeatSeconds = 60.0f / 76.0f',
	'32.0f * Beat',
	'EIGToneWaveform::Triangle',
	'EIGToneWaveform::SoftSquare',
	'if (bDegraded && StepIndex == 18)',
	'ConfigurePitchWow(0.003f, 0.30f)'
) 'M1/M1b synthesis'
Require-All $secondMorning @(
	'-0.18f',
	'1.0f / 0.92f'
) 'M1b route'
if ($secondMorning.Contains('CreateStoreJingle(this, -1.0f, 1.12f)')) {
	throw 'M1b must not regress to the old -100 cent / -12 percent degradation.'
}
$assertions++

# M2: one spatial source follows P1 pressure, never the player, and retires
# when Alarm0510 is proven through any source.
Require-All $ambience @(
	'case EIGAmbienceMode::DoorBeyond:',
	'StableSine(38.0f, SampleIndex)',
	'StableSine(57.0f, SampleIndex)',
	'DoorNoiseLow180',
	'DoorNoiseLow320'
) 'M2 synthesis'
Require-All $secondMorning @(
	'void AIGSecondMorningDirector::RefreshDoorBeyondBed(',
	'CH02DoorBeyondWave',
	'PressureStage * 0.07f',
	'!HasNarrativeTruth(TEXT("Truth.Alarm0510"))',
	'DoorBeyondComponent->FadeIn(1.6f',
	'DoorBeyondComponent->AdjustVolume(1.6f',
	'StopDoorBeyondBed(bRestoreImmediately ? 0.0f : 1.6f)'
) 'M2 route/proven-elsewhere'
$alarmTruthRouteCount = ([regex]::Matches(
	$secondMorning,
	'HasNarrativeTruth\(TEXT\("Truth\.Alarm0510"\)\)')).Count
if ($alarmTruthRouteCount -lt 3) {
	throw "M2 pressure, hints and playback must all honor proven_elsewhere; found $alarmTruthRouteCount guards."
}
$assertions++

# M3/M4: walking cadence modulates the shared flooded-bed factory without
# per-step wave allocation. Roof life and tank pressure are separate so C5
# removes only the directional pressure, and transitions exceed 1.5 seconds.
Require-All $tone @(
	'CreateFloodedCorridorWaterBed(',
	'Notes.Add({0.0f, 9.0f, 52.0f'
) 'M3 synthesis'
Require-All $thirdMorning @(
	'NarrativeCrossfadeSeconds = 1.6f',
	'UIGToneSequenceSoundWave::CreateFloodedCorridorWaterBed(this)',
	'NextWaterBedPulseTime = Now + FMath::Clamp(',
	'WaterBedComponent->AdjustVolume(0.30f, 0.42f)'
) 'M3 route'
Require-All $ambience @(
	'case EIGAmbienceMode::RoofWindRope:',
	'case EIGAmbienceMode::RoofTankPressure:',
	'StableSine(164.0f, SampleIndex)',
	'StableSine(72.0f, SampleIndex)'
) 'M4 synthesis'
Require-All $thirdMorning @(
	'void AIGThirdMorningDirector::StartRoofBed(',
	'CH03RoofWindRopeWave',
	'CH03RoofTankPressureWave',
	'StopRoofBed(IGThirdMorning::NarrativeCrossfadeSeconds)',
	'TankPressureComponent->FadeOut(',
	'TankRevealSilenceSeconds = 6.0f',
	'&ThisClass::RestoreRoofBedAfterTankSilence',
	'IGThirdMorning::TankRevealSilenceSeconds',
	'void AIGThirdMorningDirector::StartTankRevealSilence()',
	'Stress->SuppressHeartbeat('
) 'M4 route/silence'
Require-All $stress @(
	'void UIGStressComponent::SuppressHeartbeat(',
	'HeartbeatSuppressionRemaining',
	'HeartbeatComponent->Stop();',
	'PlayHeartbeat(FMath::Max(Stress, 0.38f))'
) 'M4 heartbeat silence'
$tankSilenceCallCount = ([regex]::Matches(
	$thirdMorning,
	'StartTankRevealSilence\(\);')).Count
if ($tankSilenceCallCount -ne 2) {
	throw "Both tank reveals must enter complete silence; found $tankSilenceCallCount calls."
}
$assertions++
$flashlightReveal = [regex]::Match(
	$thirdMorning,
	'(?s)void AIGThirdMorningDirector::HandleFlashlightReturnReveal\(.*?(?=void AIGThirdMorningDirector::SetVisibleInteractive)').Value
if ([string]::IsNullOrWhiteSpace($flashlightReveal) -or
	$flashlightReveal.Contains('PlayMetalEcho(')) {
	throw 'The flashlight-return reveal must not emit a metal cue inside its six-second silence.'
}
$assertions++
if ($thirdMorning.Contains('WaterBedComponent->FadeOut(1.1f')) {
	throw 'M3 to M4 must not regress below the 1.5-second crossfade contract.'
}
$assertions++

# The packaged release probe must render every production generator, reject
# silent or clipped buffers, and confirm the finite M5 tail before queueing M3.
Require-All $thirdMorning @(
	'ExpectedTrackCount = 8',
	'M0.RoomTone',
	'M1.StoreJingle',
	'M1b.DegradedJingle',
	'M2.DoorBeyond',
	'M3.FloodedWater',
	'M4.WindRope',
	'M4.TankPressure',
	'M5.ReturnHome',
	'RequestedSamplesPerTrack = 4096',
	'TrackNonZeroSamples > 32',
	'TrackClippedSamples == 0',
	'ExpectedM5DurationSeconds = 45.05f',
	'REBIRTH_RELEASE PASS audio_synthesis'
) 'runtime PCM probe'

# M5: the acceptance ending holds its authored bed for 45 seconds, resolves
# with exactly one diegetic drop, then hands off to ordinary spring life.
Require-All $tone @(
	'CreateEndingBReturnHomeBed(',
	'DurationSeconds = 45.0f',
	'BeatSeconds = 60.0f / 52.0f',
	'RimFrequencies[]',
	'PhraseIndex == 3 ? 2 : 3'
) 'M5 synthesis'
Require-All $thirdMorning @(
	'EndingBMusicDurationSeconds = 45.0f',
	'EndingBDripDelaySeconds = 42.0f',
	'EndingBFinalCardDelaySeconds = 45.2f',
	'UIGToneSequenceSoundWave::CreateEndingBReturnHomeBed(this)',
	'Director->StartEndingBLifeBed();',
	'UIGToneSequenceSoundWave::CreateSpringMorningBed(this)'
) 'M5 route'
$dripCount = ([regex]::Matches(
	$thirdMorning,
	'UIGToneSequenceSoundWave::CreateGlassCupDrip\(this\)')).Count
if ($dripCount -ne 1) {
	throw "M5 must contain exactly one epilogue glass-cup drop; found $dripCount."
}
$assertions++

Write-Host (
	"REBIRTH_AUDIO_CONTRACT PASS assertions=$assertions generators=8 tracks=6 " +
	'crossfade_seconds=1.6 tank_silence_seconds=6 m5_seconds=45') `
	-ForegroundColor Green
