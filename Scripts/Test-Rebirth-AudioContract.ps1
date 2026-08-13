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
$nightThree = Read-Source 'Source/IndieGame/Entity/IGMissingFloorNightThreeDirector.cpp'
$nightFour = Read-Source 'Source/IndieGame/Entity/IGMissingFloorNightFourDirector.cpp'
$nightPhaseScene = Read-Source 'Source/IndieGame/Entity/IGNightPhaseDirector.cpp'
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
	'ExpectedTrackCount = 19',
	'M0.RoomTone',
	'M1.StoreJingle',
	'M1b.DegradedJingle',
	'M2.DoorBeyond',
	'M3.FloodedWater',
	'M4.WindRope',
	'M4.TankPressure',
	'V1.PlasterDustFall',
	'P3.WallCavityHollow',
	'P3.WallCavitySolid',
	'P3.PipeWaterNear',
	'P3.PipeWaterFar',
	'P3.ValveOpen',
	'P5.HammerBreakThrough',
	'P2.FrottageRub',
	'V1.AudibleHeartbeat',
	'Entity.DragConcrete',
	'Entity.DragVinyl',
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

# §21.3 시그니처 SFX: 배관 수류 원근 4단, 밸브 개방, 망치 임팩트. 표에 적힌
# 파라미터가 실제 합성값이어야 문서가 명세로 남는다.
Require-All $tone @(
	'CreatePipeWaterFlow(',
	'const float Bandwidths[] = {5000.0f, 2400.0f, 1100.0f, 480.0f};',
	'CreateValveOpen(',
	'const float RingHz[] = {1800.0f, 1520.0f, 2150.0f};',
	'CreateHammerImpact(',
	'0.000f, 0.180f, 90.0f'
) '§21.3 signature SFX'
# 원근 4단은 루프여야 벽에 붙어 비교할 시간이 생긴다.
$flowBody = [regex]::Match(
	$tone,
	'(?s)UIGToneSequenceSoundWave\* UIGToneSequenceSoundWave::CreatePipeWaterFlow\(.*?\n\}').Value
if ($flowBody -notmatch 'ConfigureNotes\(MoveTemp\(FlowNotes\), true') {
	throw '배관 수류는 루프여야 한다. 원샷이면 벽을 비교할 시간이 없다.'
}
$assertions++
# 망치는 §21.3의 1.4초 건물 반향과 §10.3의 3단 파쇄를 함께 가져야 한다.
$hammerBody = [regex]::Match(
	$tone,
	'(?s)UIGToneSequenceSoundWave\* UIGToneSequenceSoundWave::CreateHammerImpact\(.*?\n\}').Value
foreach ($needle in @('1.400f', 'const int32 FractureCount = 3 + Stage * 3;')) {
	if (-not $hammerBody.Contains($needle)) {
		throw "망치 임팩트에서 $needle 가 사라졌다."
	}
	$assertions++
}

# §7 P3: 판별은 독백이 아니라 벽이 한다. 빈 벽은 실측 mass-air-mass 공명
# 대역에서 길게 울고, 속이 찬 벽은 짧게 죽는다.
Require-All $tone @(
	'CreateWallCavityResponse(',
	'IGWallCavityHollow',
	'IGWallCavitySolid',
	'0.006f, 1.450f, 105.0f'
) '§7 P3 wall discrimination synthesis'
$cavityBody = [regex]::Match(
	$tone,
	'(?s)UIGToneSequenceSoundWave\* UIGToneSequenceSoundWave::CreateWallCavityResponse\(.*?\n\}').Value
if ([string]::IsNullOrWhiteSpace($cavityBody)) {
	throw 'The wall cavity response body could not be located.'
}
$cavityHalves = $cavityBody -split 'if \(bHollow\)', 2
if ($cavityHalves.Count -ne 2) { throw 'The hollow/solid split is missing.' }
$hollowHalf = ($cavityHalves[1] -split 'else', 2)[0]
$solidHalf = ($cavityHalves[1] -split 'else', 2)[1]
$hollowLongest = 0.0
foreach ($note in [regex]::Matches($hollowHalf, 'WallNotes\.Add\(\{\s*[0-9.]+f,\s*([0-9.]+)f,')) {
	$hollowLongest = [Math]::Max($hollowLongest, [double]$note.Groups[1].Value)
}
$solidLongest = 0.0
foreach ($note in [regex]::Matches($solidHalf, 'WallNotes\.Add\(\{\s*[0-9.]+f,\s*([0-9.]+)f,')) {
	$solidLongest = [Math]::Max($solidLongest, [double]$note.Groups[1].Value)
}
if ($hollowLongest -lt $solidLongest * 3.0) {
	throw ('빈 벽은 속이 찬 벽보다 최소 세 배 길게 울어야 한다: ' +
		"hollow=$hollowLongest solid=$solidLongest")
}
$assertions++
# 여기서 사인파를 빼면 공명이 사라지고 다시 노이즈 두 덩이가 된다.
if ($hollowHalf -notmatch 'EIGToneWaveform::Sine') {
	throw '공동 공명은 대역 노이즈가 아니라 실제 공진 모드여야 한다.'
}
$assertions++
Require-All $nightThree @(
	'void AIGMissingFloorNightThreeDirector::PlayWallListenResponse(',
	'UIGToneSequenceSoundWave::CreateWallCavityResponse(this, bHollow)',
	'UIGToneSequenceSoundWave::CreateValveOpen(',
	'constexpr int32 RiserBedDistanceStep = 2',
	'constexpr int32 CavityWallDistanceStep = 0',
	'constexpr int32 SolidWallDistanceStep = 3'
) '§7 P3 wall discrimination route'
# 세 경로(밸브 전, 공동 벽, 나머지 벽) 모두가 실제로 소리를 낸다.
$wallResponseCalls = ([regex]::Matches(
	$nightThree,
	'PlayWallListenResponse\(BayIndex, /\*bHollow=\*/')).Count
if ($wallResponseCalls -ne 3) {
	throw ('세 청음 경로가 모두 소리를 내야 한다. 하나라도 빠지면 그 벽은 ' +
		"독백으로만 대답한다. 현재 $wallResponseCalls 개.")
}
$assertions++
Require-All $nightFour @(
	'UIGToneSequenceSoundWave::CreateHammerImpact(this, StrikeCount - 1)',
	'UIGToneSequenceSoundWave::CreateValveOpen(',
	'UIGToneSequenceSoundWave::CreateVentDuctSpinUp(this)'
) '§21.3 night four impact and controls'

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

# §11 V2 오염 레이어. 수광 평면은 0.15cm만 띄우고 그림자·충돌을 끈다. 조명색과
# 접촉 그림자는 월드가 계산하므로 흔적만 주변 재질에 젖어든다.
$settledDust = Read-Source 'Source/IndieGame/Environment/IGSettledDustComponent.cpp'
$settledDustHeader = Read-Source 'Source/IndieGame/Environment/IGSettledDustComponent.h'
Require-All $settledDustHeader @(
	'static constexpr float SurfaceOffset = 0.15f;',
	'void ConfigureField('
) '§11 V2 settled dust contract'
Require-All $dust @(
	'enum class EIGDustPrintKind : uint8',
	'void ReportSettledPrint(',
	'void ClearSettledPrints();',
	'static constexpr float PrintMergeDistance = 26.0f;'
) '§11 V2 settled dust world model'
Require-All $settledDust @(
	'M_MissingFloorHandprints',
	'M_MissingFloorDragTrails',
	'Layer->SetCastShadow(false);',
	'Layer->SetReceivesDecals(false);',
	'Layer->SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);',
	'Layer->bAffectDistanceFieldLighting = false;',
	'Layer->SetUsingAbsoluteLocation(true);',
	'FieldFloorZ + SurfaceOffset'
) '§11 V2 settled dust blending'
# 발자국이 남는 마스크: 두 종류의 마크가 서로 다른 크기여야 신발과 끌림이
# 구분된다. 같으면 바닥에 같은 도형만 깔린다.
if ($settledDustHeader -notmatch 'FootfallLengthCentimeters = 27\.0f' -or
	$settledDustHeader -notmatch 'DragLengthCentimeters = 62\.0f') {
	throw '신발 자국과 끌림 자국은 크기가 달라야 구분된다.'
}
$assertions++
# 흰 사각 그림으로 붙이지 않는다: 발광이나 불투명 기본 재질이면 계약 위반이다.
if ($settledDust -match 'M_(SignWhite|LightPanel|ScreenGlow|PaperClean)') {
	throw '오염 마크는 저자 잔여물 마스크만 쓴다(§11 V2).'
}
$assertions++
# 리포터는 먼지가 어디 있는지 몰라도 된다. 필드가 자기 범위로 걸러낸다.
Require-All $listener @(
	'EIGDustPrintKind::Drag'
) '§11 V2 entity drag mark'
$playerPrints = Read-Source 'Source/IndieGame/Player/IGPlayerCharacter.cpp'
Require-All $playerPrints @(
	'EIGDustPrintKind::Footfall',
	'FootPrintDropCentimeters'
) '§11 V2 player footfall mark'
# 포획 리셋은 공기와 바닥을 함께 04:30으로 되돌린다.
if ($listener -notmatch 'Dust->ClearSettledPrints\(\);') {
	throw '포획 리셋은 바닥의 흔적도 지워야 한다(§5.4).'
}
$assertions++

# §11 V2 403호 3단계 노화: 프롤로그는 깨끗하고, 누적이며, 밤으로 구동된다.
Require-All $worldScene @(
	'void AIGPrologueWorldScene::SetUnit403AgeStage(',
	'void AIGPrologueWorldScene::ApplyUnit403AgeStage()',
	'Unit403AgeStageOne',
	'Unit403AgeStageTwo',
	'M_MissingFloorCavityScratches',
	'M_DecalDampWallpaper'
) '§11 V2 403 aging'
Require-All $nightPhaseScene @(
	'WorldScene->SetUnit403AgeStage('
) '§11 V2 aging wiring'
# 누적이 아니라 배타면 밤4에서 균열이 사라진다.
if ($worldScene -notmatch 'Plane->SetHiddenInGame\(Unit403AgeStage < 1\);' -or
	$worldScene -notmatch 'Plane->SetHiddenInGame\(Unit403AgeStage < 2\);') {
	throw '403호 노화는 누적이어야 한다. 손상은 이사 가지 않는다.'
}
$assertions++
# 4F 복도 러너와 계량기함 녹.
Require-All $worldScene @(
	'§11 V2 끌린 자국 (복도 러너)',
	'M_MissingFloorDragTrails',
	'M_DecalRustFasteners'
) '§11 V2 corridor runner and meter rust'

# §21.3 프로타주: 밴드 노이즈 900~4200Hz, 지속. 소음 0.25를 내는 유일한 지속
# 상호작용이므로 유일한 지속 큐를 갖는다. 들리지 않는 비용은 선택할 수 없다.
Require-All $tone @(
	'CreateFrottageRub(',
	'IGFrottageRub',
	'900.0f',
	'4200.0f'
) '§21.3 frottage synthesis'
$rubBody = [regex]::Match(
	$tone,
	'(?s)UIGToneSequenceSoundWave\* UIGToneSequenceSoundWave::CreateFrottageRub\(.*?\n\}').Value
if ($rubBody -notmatch 'ConfigureNotes\(MoveTemp\(RubNotes\), true') {
	throw '프로타주는 홀드 내내 이어져야 하므로 루프다.'
}
$assertions++
foreach ($band in [regex]::Matches($rubBody, 'RubNotes\.Add\(\{\s*[^,]+,\s*[0-9.]+f,\s*([0-9.]+)f,')) {
	$value = [double]$band.Groups[1].Value
	if ($value -lt 900.0 -or $value -gt 4200.0) {
		throw "프로타주 대역은 900~4200Hz 안에 있어야 한다: $value Hz"
	}
	$assertions++
}
if ($rubBody -match 'EIGToneWaveform::(Sine|Triangle|SoftSquare)') {
	throw '흑연이 종이를 긁는 소리에는 음정이 없다.'
}
$assertions++
$evidence = Read-Source 'Source/IndieGame/Entity/IGMissingFloorEvidence.cpp'
$puzzleTwo = Read-Source 'Source/IndieGame/Entity/IGMissingFloorPuzzleTwoDirector.cpp'
Require-All $evidence @(
	'void AIGMissingFloorEvidence::SetSustainedRubCue(',
	'UIGToneSequenceSoundWave::CreateFrottageRub(this)',
	'Context.HoldProgress',
	'RubCueComponent->FadeOut('
) '§21.3 frottage route'
Require-All $puzzleTwo @(
	'CarbonLedger->SetSustainedRubCue(true);'
) '§21.3 frottage wiring'
# 손을 떼면 연필도 멈춘다. 놓아도 계속 울리면 남은 비용을 피할 수 없다.
if ($evidence -notmatch '(?s)EndInteraction_Implementation\(.*?StopRubCue\(\);') {
	throw '홀드를 놓으면 문지름이 멈춰야 한다.'
}
$assertions++

# §21.3 심박 소음화: 기존 심박 + 220Hz 로우패스, −6dB, 박동 동기.
Require-All $tone @(
	'CreateAudibleHeartbeat(',
	'IGAudibleHeartbeat'
) '§21.3 audible heartbeat synthesis'
$audibleBody = [regex]::Match(
	$tone,
	'(?s)UIGToneSequenceSoundWave\* UIGToneSequenceSoundWave::CreateAudibleHeartbeat\(.*?\n\}').Value
if ($audibleBody -notmatch '\* 0\.5f') {
	throw '심박 소음화는 −6dB, 즉 진폭 절반이어야 한다.'
}
$assertions++
# 220Hz 로우패스는 가산 합성에서 상위 부분음 제거로 실현된다. 88Hz가 남아
# 있으면 또렷한 심박이 그대로 방에 나가 전이가 들리지 않는다.
foreach ($partial in [regex]::Matches($audibleBody, 'Beat\.Add\(\{\s*[0-9.]+f,\s*[0-9.]+f,\s*([0-9.]+)f,')) {
	if ([double]$partial.Groups[1].Value -gt 220.0) {
		throw ('심박 소음화는 220Hz 위를 남기지 않는다: ' +
			"$($partial.Groups[1].Value) Hz")
	}
	$assertions++
}
Require-All $stress @(
	'UIGToneSequenceSoundWave::CreateAudibleHeartbeat(',
	'Reported.Loudness > 0.0f',
	'EIGAudioBus::Player'
) '§21.3 audible heartbeat route'
# 험이 삼킨 심박이 들리면 엄폐가 통하지 않는다고 가르치게 된다.
if ($stress -notmatch '(?s)const FIGNoiseEvent Reported = Noise->ReportNoise\(.*?if \(Reported\.Loudness > 0\.0f\)') {
	throw '마스킹을 통과한 심박만 들려야 한다.'
}
$assertions++

# §10.3 끌림 2종: 콘크리트와 장판. 같은 소리면 어느 바닥인지 알 수 없다.
Require-All $tone @(
	'CreateEntityDragLoop(',
	'IGEntityDragLoopVinyl',
	'if (bVinyl)'
) '§10.3 drag variants'
$dragBody = [regex]::Match(
	$tone,
	'(?s)UIGToneSequenceSoundWave\* UIGToneSequenceSoundWave::CreateEntityDragLoop\(.*?\n\}').Value
$dragHalves = $dragBody -split 'if \(bVinyl\)', 2
if ($dragHalves.Count -ne 2) { throw '끌림 두 종의 분기를 찾을 수 없다.' }
$vinylHalf = ($dragHalves[1] -split '\telse\b', 2)[0]
$concreteHalf = ($dragHalves[1] -split '\telse\b', 2)[1]
$vinylLowest = 99999.0
foreach ($n in [regex]::Matches($vinylHalf, 'DragNotes\.Add\(\{\s*[0-9.]+f,\s*[0-9.]+f,\s*([0-9.]+)f,')) {
	$vinylLowest = [Math]::Min($vinylLowest, [double]$n.Groups[1].Value)
}
$concreteLowest = 99999.0
foreach ($n in [regex]::Matches($concreteHalf, 'DragNotes\.Add\(\{\s*[0-9.]+f,\s*[0-9.]+f,\s*([0-9.]+)f,')) {
	$concreteLowest = [Math]::Min($concreteLowest, [double]$n.Groups[1].Value)
}
# 장판은 슬래브 위 얇은 플라스틱이라 저역 럼블을 잃는다.
if ($vinylLowest -le $concreteLowest) {
	throw ('장판 끌림은 콘크리트보다 저역이 얕아야 한다: ' +
		"vinyl=$vinylLowest concrete=$concreteLowest")
}
$assertions++
Require-All $listener @(
	'void AIGListenerEntity::RefreshDragSurface()',
	'const FName VinylSurfaceTag(TEXT("Footstep.Vinyl"));',
	'UIGToneSequenceSoundWave::CreateEntityDragLoop(this, bVinyl)'
) '§10.3 drag surface route'
# 발소리 매트릭스와 같은 태그를 써야 두 시스템이 어긋나지 않는다.
$playerCharacter = Read-Source 'Source/IndieGame/Player/IGPlayerCharacter.cpp'
if ($playerCharacter -notmatch 'VinylSurfaceTag\(TEXT\("Footstep\.Vinyl"\)\)') {
	throw '끌림과 발소리가 같은 표면 태그를 공유해야 한다.'
}
$assertions++

Write-Host (
	"REBIRTH_AUDIO_CONTRACT PASS assertions=$assertions generators=19 tracks=6 " +
	'crossfade_seconds=1.6 tank_silence_seconds=6 m5_seconds=45 ' +
	'reverb_presets=2 dust_density_max=2.0') `
	-ForegroundColor Green
