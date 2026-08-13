[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot

function Read-Source([string]$RelativePath) {
	Get-Content -Raw -Encoding UTF8 -LiteralPath (
		Join-Path $projectRoot $RelativePath)
}

$tuningHeader = Read-Source 'Source/IndieGame/Entity/IGListenerTuning.h'
$tuning = Read-Source 'Source/IndieGame/Entity/IGListenerTuning.cpp'
$noiseHeader = Read-Source 'Source/IndieGame/Entity/IGNoiseSubsystem.h'
$noise = Read-Source 'Source/IndieGame/Entity/IGNoiseSubsystem.cpp'
$listener = Read-Source 'Source/IndieGame/Entity/IGListenerEntity.cpp'
$narrative = Read-Source 'Source/IndieGame/Narrative/IGMissingFloorNarrativeSubsystem.cpp'
$nightFour = Read-Source 'Source/IndieGame/Entity/IGMissingFloorNightFourDirector.cpp'
$nightPhase = Read-Source 'Source/IndieGame/Entity/IGNightPhaseDirector.cpp'
$greybox = Read-Source 'Source/IndieGame/Entity/IGListenerGreyboxDirector.cpp'
$assertions = 0

function Require-All(
	[string]$Source,
	[string[]]$Needles,
	[string]$ContractName) {
	foreach ($needle in $Needles) {
		if (-not $Source.Contains($needle)) {
			throw "$ContractName invariant is missing: $needle"
		}
		$script:assertions++
	}
}

# §20.2 튜닝 테이블. 네 밤의 일곱 파라미터가 문서의 초기값과 같아야 한다.
Require-All $tuning @(
	'constexpr float NightHearingRadius[] = {900.0f, 1100.0f, 1100.0f, 1300.0f};',
	'constexpr float NightListenWindow[] = {8.0f, 8.0f, 7.0f, 6.0f};',
	'constexpr int32 NightPatrolNodes[] = {6, 11, 14, 18};',
	'constexpr float NightChaseMultiplier[] = {2.4f, 3.0f, 3.0f, 3.4f};',
	'constexpr float NightInvestigateHold[] = {6.0f, 6.0f, 5.0f, 5.0f};',
	'constexpr float NightHeatmapWeight[] = {0.0f, 0.3f, 0.5f, 0.7f};',
	'constexpr bool NightAmbushAllowed[] = {false, false, true, true};'
) '§20.2 tuning table'

# 티어는 밤 값에 곱해지는 별도 축이다. 대체하면 밤별 곡선이 사라진다.
Require-All $tuning @(
	'constexpr float TierListenScale[] = {1.0f, 0.75f, 0.625f, 0.5f};',
	'NightListenWindow[Night] * TierListenScale[Tier]'
) '§20.2 tier axis'

# §20.4 네 모드. 이름도 세계의 언어여야 하므로 라벨까지 검사한다.
Require-All $tuningHeader @(
	'Quiet,',
	'Standard,',
	'Hasty,',
	'ListenOnly,'
) '§20.4 difficulty modes'
Require-All $tuning @(
	'"조용한 밤"',
	'"기본"',
	'"성급한 밤"',
	'"듣기만 하는 밤"'
) '§20.4 mode labels'
Require-All $tuning @(
	'Tuning.HearingSensitivity *= 0.75f;',
	'Tuning.ChaseSpeed *= 0.8f;',
	'Tuning.WaitScale = 1.5f;',
	'Tuning.HeatmapWeight = FMath::Min(1.0f, Tuning.HeatmapWeight + 0.2f);',
	'Tuning.bChaseEnabled = false;',
	'Tuning.bCaptureEnabled = false;'
) '§20.4 modifiers'
# 난이도는 저장본이 아니라 사용자 설정에 남는다. 저장본이 잠그면
# "언제든 변경할 수 있다"가 깨진다.
if ($tuning -notmatch 'GGameUserSettingsIni') {
	throw '난이도는 GameUserSettings.ini에 남아야 한다.'
}
$assertions++
if ($tuning -match 'Snapshot\.') {
	throw '난이도가 진행 세이브에 들어가면 모드 변경이 저장본을 잠근다.'
}
$assertions++
# 명령행 오버라이드는 저장값을 덮어쓰지 않는다.
$overrideBlock = [regex]::Match(
	$tuning,
	'(?s)EIGNightDifficulty ResolveActiveDifficulty\(\).*?\n\t\}').Value
if ($overrideBlock -match 'SavePersistedDifficulty') {
	throw '명령행 오버라이드가 저장값을 덮어써서는 안 된다.'
}
$assertions++

# 밸런스 기준은 항상 기본이며, 마이크 모드는 난이도가 아니다(§5.7).
if ($tuning -match 'Microphone|bMicrophone') {
	throw '마이크 모드는 난이도 축이 아니다(§20.4).'
}
$assertions++

# §5.6 적응 청각. 생성형이 아니라 순수 통계이며 재현 가능해야 한다.
Require-All $noiseHeader @(
	'float GetHeatAt(',
	'bool GetHottestZone(',
	'void DecayHeatmapForNewNight();',
	'void ResetHeatmap();',
	'static constexpr float HeatZoneSize = 400.0f;',
	'static constexpr float HeatNightDecay = 0.5f;'
) '§5.6 heatmap API'
# 배열이어야 순회 순서가 고정된다. TMap 해시 순서는 결정적이지 않으므로
# 같은 플레이가 다른 매복 지점을 만들 수 있다.
if ($noiseHeader -match 'TMap<FIntVector') {
	throw '히트맵은 해시 순서에 의존하면 재현 가능하지 않다.'
}
$assertions++
Require-All $noise @(
	'AccumulateHeat(Location, Effective);',
	'Zone.Heat = FMath::Min(Zone.Heat + Loudness, HeatSaturation);',
	'Zone.Heat *= HeatNightDecay;',
	'Zone.Heat > Best->Heat'
) '§5.6 heatmap statistics'
# 마스킹에 삼켜진 소리는 애초에 나지 않았으므로 열도 남기지 않는다.
$reportBody = [regex]::Match(
	$noise,
	'(?s)FIGNoiseEvent UIGNoiseSubsystem::ReportNoise\(.*?\n\}').Value
if ($reportBody.IndexOf('AccumulateHeat') -lt $reportBody.IndexOf('Event.Loudness = Effective;')) {
	throw '히트맵은 마스킹을 통과한 소리만 기억해야 한다.'
}
$assertions++

# 존재는 해결된 숫자만 읽는다.
Require-All $listener @(
	'void AIGListenerEntity::RefreshNightTuning()',
	'ChaseSpeed = Tuning.ChaseSpeed;',
	'Tuning.HearingSensitivity',
	'Tuning.WaitScale',
	'bSecondSound && Tuning.bChaseEnabled',
	'Distance <= CaptureRadius && Tuning.bCaptureEnabled',
	'void AIGListenerEntity::AdvancePatrolIndex()',
	'bool AIGListenerEntity::TryBeginAmbush()',
	'NoiseSubsystem->DecayHeatmapForNewNight();'
) '§20.2/§5.6 entity integration'
# 밤1의 히트맵 가중은 0이다. 첫 밤의 순찰은 배울 수 있는 순서여야 한다.
if ($listener -notmatch 'Tuning\.HeatmapWeight <= 0\.0f') {
	throw '히트맵 가중이 0인 밤에는 통계를 보지 않아야 한다.'
}
$assertions++
# 매복은 티어3에서만, 그리고 습관이 실제로 있을 때만 성립한다.
$ambushBody = [regex]::Match(
	$listener,
	'(?s)bool AIGListenerEntity::TryBeginAmbush\(\).*?\n\}').Value
foreach ($needle in @('AggressionTier < 3', 'Heat < 0.5f', 'EIGListenerState::Investigating')) {
	if (-not $ambushBody.Contains($needle)) {
		throw "티어3 매복에서 $needle 가 사라졌다."
	}
	$assertions++
}

# §20.2: 밤이 끝나면 티어는 1로 하강한다. 초기화가 아니라 하강이다.
Require-All $narrative @(
	'FMath::Min(Snapshot.Night.AggressionTier, 1)'
) '§20.2 tier descent'

# §20.4: 듣기만 하는 밤에도 엔딩 A·B·C가 모두 도달 가능해야 한다(§20.5).
Require-All $nightFour @(
	'bool AIGMissingFloorNightFourDirector::ResolveDawnFailureEnding()',
	'Narrative->IsNightFourWallOpened()',
	'CommitFailureEnding(/*bRecordCapture=*/false)',
	'CommitFailureEnding(/*bRecordCapture=*/true)'
) '§20.4 ending C substitution'
Require-All $nightPhase @(
	'ResolveDawnFailureEnding()'
) '§20.4 dawn route wiring'
# 새벽 경로는 포획이 아니다. 포획으로 기록하면 없는 포획이 티어를 올린다.
$dawnBody = [regex]::Match(
	$nightFour,
	'(?s)bool AIGMissingFloorNightFourDirector::ResolveDawnFailureEnding\(\).*?\n\}').Value
# 호출만 본다. 인자 이름 bRecordCapture는 여기서 걸려서는 안 된다.
if ($dawnBody -match '->RecordCapture\(') {
	throw '새벽 경로가 포획을 기록하면 포획 없는 모드의 티어가 올라간다.'
}
$assertions++

# 런타임 프로브가 표를 실제로 해결해 대조한다.
Require-All $greybox @(
	'case EProbeStep::DifficultyContract:',
	'MISSINGFLOOR_DIFFICULTY PASS',
	'IGListenerTuning::Resolve(',
	'EIGNightDifficulty::ListenOnly',
	'GetHottestZone(HottestCenter, HottestHeat)'
) 'runtime difficulty probe'

Write-Host (
	"MISSINGFLOOR_M8_DIFFICULTY_CONTRACT PASS assertions=$assertions " +
	'nights=4 modes=4 heatmap_decay=0.5 ending_c_routes=2') `
	-ForegroundColor Green
