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
$mixDirector = Read-Source 'Source/IndieGame/Audio/IGMissingFloorAudioSubsystem.cpp'
$attenuation = Read-Source 'Source/IndieGame/Audio/IGAudioHelpers.cpp'
$listener = Read-Source 'Source/IndieGame/Entity/IGListenerEntity.cpp'
$dust = Read-Source 'Source/IndieGame/Environment/IGDustSubsystem.h'
$beamDust = Read-Source 'Source/IndieGame/Player/IGBeamDustComponent.cpp'
$worldScene = Read-Source 'Source/IndieGame/Core/IGPrologueWorldScene.cpp'
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
	'ExpectedTrackCount = 9',
	'M0.RoomTone',
	'M1.StoreJingle',
	'M1b.DegradedJingle',
	'M2.DoorBeyond',
	'M3.FloodedWater',
	'M4.WindRope',
	'M4.TankPressure',
	'V1.PlasterDustFall',
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

# §10.4 거리 리버브 문법: 건물 반향이 거리의 두 번째 축이다. 복도와 계단실
# 두 프리셋만 존재하고, 옥상은 프리셋의 부재로 드라이하게 남는다.
Require-All $mixDirector @(
	'const FName AcousticSpaceReverbTag(TEXT("MissingFloor.AcousticSpace"));',
	'constexpr float CorridorDecayTime = 1.25f',
	'constexpr float StairwellDecayTime = 2.70f',
	'constexpr float CorridorDecayHFRatio = 0.72f',
	'constexpr float StairwellDecayHFRatio = 0.95f',
	'REVERB_MISSING_FLOOR_CORRIDOR',
	'REVERB_MISSING_FLOOR_STAIRWELL',
	'AcousticPresets[static_cast<int32>(EIGAcousticSpace::Open)] = nullptr;',
	'UGameplayStatics::ActivateReverbEffect(',
	'UGameplayStatics::DeactivateReverbEffect(',
	'constexpr float AcousticCrossfadeSeconds = 0.90f',
	'constexpr float AcousticPollIntervalSeconds = 0.20f',
	'case EIGFootstepSurface::MetalStair:',
	'return EIGAcousticSpace::Stairwell;',
	'case EIGFootstepSurface::Rooftop:',
	'return EIGAcousticSpace::Open;',
	'SoundClass->Properties.bReverb =',
	'SoundClass->Properties.Default2DReverbSendAmount = 0.0f;',
	'Settings.ReverbSendMethod = EReverbSendMethod::Manual;',
	'Settings.ReverbSendMethod = EReverbSendMethod::Linear;',
	'constexpr float EntityReverbFloor = 0.22f',
	'constexpr float EntityReverbCeiling = 0.95f',
	'constexpr float PlayerManualReverbSend = 0.30f',
	'stairwell does not ring twice the corridor',
	'bare concrete must hold high frequencies longer',
	'entity cues do not ride the building reverb'
) '§10.4 reverb grammar'
# UI와 스코어만 건물 반향에서 빠진다. 나머지 네 버스는 반드시 방을 탄다.
if ($mixDirector -notmatch '(?s)constexpr bool BusUsesBuildingReverb\[\]\s*=\s*\{\s*true,[^}]*true,[^}]*true,[^}]*true,[^}]*false,[^}]*false') {
	throw 'ENTITY/PLAYER/PUZZLE/WORLD must ride the building reverb while UI/SCORE stay dry.'
}
$assertions++
# 전 공간 원샷이 한 지점을 지나야 거리 축이 하나로 유지된다.
Require-All $attenuation @(
	'UIGMissingFloorAudioSubsystem::ConfigureReverbSend(',
	'if (bForceDry)',
	'Settings.bEnableReverbSend = false;',
	'UAudioComponent* SpawnDryOneShotAt('
) '§10.4 attenuation routing'
# §21.3: 포획 노크만 리버브 0이다. 그 순간 그는 나를 안고 있다.
if (-not $listener.Contains('IGAudio::SpawnDryOneShotAt(')) {
	throw 'The capture knock must arrive completely dry (§21.3).'
}
$assertions++
$dryCueCount = 0
foreach ($file in @(
	'Source/IndieGame/Entity/IGListenerEntity.cpp',
	'Source/IndieGame/Entity/IGListenerGreyboxDirector.cpp',
	'Source/IndieGame/Entity/IGMissingFloorFifthDawnDirector.cpp',
	'Source/IndieGame/Entity/IGMissingFloorNightThreeDirector.cpp',
	'Source/IndieGame/Entity/IGMissingFloorNightFourDirector.cpp',
	'Source/IndieGame/Player/IGPlayerCharacter.cpp')) {
	$dryCueCount += ([regex]::Matches(
		(Read-Source $file),
		'IGAudio::SpawnDryOneShotAt\(')).Count
}
if ($dryCueCount -ne 1) {
	throw ('Exactly one cue in the game may be dry, or the grammar stops ' +
		"meaning anything; found $dryCueCount.")
}
$assertions++
$busTaggedAttenuationCount = ([regex]::Matches(
	(($listener + (Read-Source 'Source/IndieGame/Entity/IGMissingFloorNightThreeDirector.cpp')) +
		(Read-Source 'Source/IndieGame/Entity/IGMissingFloorNightFourDirector.cpp')) +
		(Read-Source 'Source/IndieGame/Entity/IGMissingFloorPuzzleOneDirector.cpp'),
	'IGAudio::MakeAttenuation\((?s).{0,220}?EIGAudioBus::')).Count
if ($busTaggedAttenuationCount -lt 4) {
	throw ('Every persistent audio bed must declare its bus so the reverb ' +
		"curve matches; found $busTaggedAttenuationCount.")
}
$assertions++

# §10.3 분진 낙하, §21.3 파라미터: 6kHz 하이패스·지수 감쇠·0.90초. 그보다
# 낮은 성분이 생기면 가루가 아니라 벽이 움직이거나 파편이 떨어지는 소리다.
Require-All $tone @(
	'CreatePlasterDustFall(',
	'IGPlasterDustFall',
	'0.000f, 0.900f, 6200.0f'
) '§10.3 dust fall synthesis'
$dustFallBody = [regex]::Match(
	$tone,
	'(?s)UIGToneSequenceSoundWave\* UIGToneSequenceSoundWave::CreatePlasterDustFall\(.*?\n\}').Value
if ([string]::IsNullOrWhiteSpace($dustFallBody)) {
	throw 'The dust-fall generator body could not be located.'
}
$assertions++
$dustBands = @()
$dustEndSeconds = 0.0
# Inline notes: {start, duration, band, amplitude, attack, release, waveform}.
foreach ($note in [regex]::Matches(
	$dustFallBody,
	'DustNotes\.Add\(\{\s*([0-9.]+)f,\s*([0-9.]+)f,\s*([0-9.]+)f,')) {
	$dustBands += [double]$note.Groups[3].Value
	$dustEndSeconds = [Math]::Max(
		$dustEndSeconds,
		[double]$note.Groups[1].Value + [double]$note.Groups[2].Value)
}
# The individual grains carry their bands in a table instead of inline.
$grainTable = [regex]::Match($dustFallBody, 'GrainBands\[\]\s*=\s*\{([^}]*)\}')
if (-not $grainTable.Success) {
	throw 'The dust-fall grain band table could not be located.'
}
foreach ($band in [regex]::Matches($grainTable.Groups[1].Value, '([0-9.]+)f')) {
	$dustBands += [double]$band.Groups[1].Value
}
if ($dustBands.Count -lt 10) {
	throw "분진 낙하 lost its layers; only $($dustBands.Count) bands were found."
}
$assertions++
# §21.3 fixes the high pass at 6 kHz. Falling grains push the spectral centroid
# up as they get finer, and plaster powder is much finer than sand.
foreach ($band in $dustBands) {
	if ($band -lt 6000.0) {
		throw ('분진 낙하 is high-passed at 6 kHz per §21.3; a lower band is ' +
			"debris or the wall itself, not powder (found $band Hz).")
	}
	$assertions++
}
# §21.3 also fixes the length at 0.90 s.
if ([Math]::Abs($dustEndSeconds - 0.90) -gt 0.001) {
	throw "분진 낙하 must run 0.90 s per §21.3; found $dustEndSeconds s."
}
$assertions++
if ($dustFallBody -match 'EIGToneWaveform::(Sine|Triangle|SoftSquare)') {
	throw 'Falling powder is broadband granular noise, never a pitched oscillator.'
}
$assertions++
Require-All $listener @(
	'constexpr float DustSiftIntervalCentimeters = 260.0f',
	'constexpr float DustSiftVolume = 0.42f',
	'UIGToneSequenceSoundWave::CreatePlasterDustFall(this)',
	'!AudioDirector->IsAuthoredSilence()',
	'EIGAudioBus::Entity'
) '§10.3 dust fall route'

# §11 V1: 빛으로 그의 최근 경로를 읽는다. 밀도는 정확히 두 배까지만 오르고,
# 포획 리셋은 공기까지 04:30으로 되돌린다.
Require-All $dust @(
	'static constexpr float MaxDensityMultiplier = 2.0f;',
	'static constexpr float DisturbanceLifetimeSeconds = 52.0f;',
	'void ReportDisturbance(',
	'float GetDensityMultiplierAt(',
	'void ClearDisturbances();'
) '§11 V1 dust world model'
Require-All $listener @(
	'void AIGListenerEntity::ReportDustTrail()',
	'DustSubsystem->ReportDisturbance(DragHeight, DragStrength)',
	'Dust->ClearDisturbances();'
) '§11 V1 trail reporting'
Require-All $beamDust @(
	'BatchUpdateInstancesTransforms(',
	'Motes->SetUsingAbsoluteLocation(true);',
	'M_FridgeInterior',
	'constexpr float MinRevealingBeamStrength = 0.35f',
	'ActiveMoteCount = BaseMoteCount',
	'TrailAnchors[Index % TrailAnchors.Num()]'
) '§11 V1 beam motes'
# 발광 모트는 어둠 속에서도 보인다. 빔만 리빌한다는 계약이 깨진다.
if ($beamDust -match 'Emissive|SetEmissive|M_(SignWhite|LightPanel|ScreenGlow|WindowGlow)') {
	throw 'Motes must be lit by the beam, never emissive — darkness has to stay dark.'
}
$assertions++
$beamMoteCounts = [regex]::Match(
	(Read-Source 'Source/IndieGame/Player/IGBeamDustComponent.h'),
	'BaseMoteCount = (\d+);(?s).*?TrailMoteCount = (\d+);')
if (-not $beamMoteCounts.Success -or
	[int]$beamMoteCounts.Groups[1].Value -ne [int]$beamMoteCounts.Groups[2].Value) {
	throw '§11 V1 asks for exactly twice the density over his lane, no more.'
}
$assertions++
# 밤 구간 자동노출 하한 잠금: 상한만 묶으면 히스토그램이 암부를 끌어올려
# 손전등이 자원이 아니게 된다.
Require-All $worldScene @(
	'PostProcess->Settings.AutoExposureMinBrightness = bSealed ? 1.30f : -0.5f;',
	'PostProcess->Settings.AutoExposureMaxBrightness = bSealed ? 1.30f : 5.0f;'
) '§11 V1 night exposure lock'

Write-Host (
	"REBIRTH_AUDIO_CONTRACT PASS assertions=$assertions generators=9 tracks=6 " +
	'crossfade_seconds=1.6 tank_silence_seconds=6 m5_seconds=45 ' +
	'reverb_presets=2 dust_density_max=2.0') `
	-ForegroundColor Green
