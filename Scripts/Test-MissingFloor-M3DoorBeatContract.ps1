<#
.SYNOPSIS
	§8 밤2 비트 2-1 「문 하나를 사이에 둔 첫 대면」 계약 검사.

.DESCRIPTION
	이 비트는 두 가지를 동시에 진다. 하나는 연출이다 — 밤1이 거리를 두고
	가르친 규칙에서 거리를 없애고 강철 문 한 장만 남긴다. 다른 하나는 기능이다:
	§5.5의 녹음 규칙은 거부할 것이 테이프에 있어야 성립하고, 설계서가 지정한
	무장 계기가 바로 이 노크다.

	그래서 세 가지를 검사한다.

	하나, 순서와 값 — 한 번의 노크, 문구멍, 폰, 3연, 기다림, 끌려가는 소리.
	3연의 크기는 1.0이어야 한다. §5.5의 무음 길이 표가 그 1.0을 정확히
	2.10초로 옮기고, 아침에 그녀가 듣는 공백의 길이가 거기서 정해진다.

	둘, 막히지 않음 — 문구멍을 안 보거나 폰을 안 켜는 플레이어도 비트를
	끝까지 받는다. 다만 테이프에는 남지 않는다.

	셋, 형체는 빌린 것 — 복도의 그는 실제 존재이고, 비트가 놓아줄 때
	공격 티어를 건드리지 않고 순찰로 돌아가야 한다.
#>
[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot

function Read-Source([string]$RelativePath) {
	Get-Content -Raw -Encoding UTF8 -LiteralPath (
		Join-Path $projectRoot $RelativePath)
}

$beatHeader = Read-Source 'Source/IndieGame/Entity/IGMissingFloorNightTwoBeatDirector.h'
$beat = Read-Source 'Source/IndieGame/Entity/IGMissingFloorNightTwoBeatDirector.cpp'
$greybox = Read-Source 'Source/IndieGame/Entity/IGListenerGreyboxDirector.cpp'
$greyboxHeader = Read-Source 'Source/IndieGame/Entity/IGListenerGreyboxDirector.h'
$puzzleTwo = Read-Source 'Source/IndieGame/Entity/IGMissingFloorPuzzleTwoDirector.cpp'
$recording = Read-Source 'Source/IndieGame/Narrative/IGRecordingSubsystem.cpp'
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

# --- 좌표. 노크는 403호 현관문에서 난다 -------------------------------------
# 현관문은 (101, -225, 900)이고 남쪽 벽이 Y=-225이므로 복도는 그 반대쪽이다.
Require-All $beat @(
	'constexpr float FourthFloorZ = 900.0f;',
	'const FVector DoorLocation(101.0f, -225.0f, FourthFloorZ);',
	'const FVector KnockLocation(101.0f, -232.0f, FourthFloorZ + 112.0f);',
	'const FVector PeepholeLocation(101.0f, -220.0f, FourthFloorZ + 150.0f);',
	'const FVector FigureStagePoint(101.0f, -272.0f, FourthFloorZ);',
	'const FVector FigureShufflePoint(139.0f, -276.0f, FourthFloorZ);',
	'const FVector DragDepartPoint(-120.0f, -278.0f, FourthFloorZ)'
) '비트 2-1 coordinates'

# §5.5의 폰은 관리실이 아니라 403호 안에 있다. 처음 넣을 때 슬래브 높이를
# 빠뜨려서 9미터 아래 관리실 바닥에 놓여 있었다.
Require-All $puzzleTwo @(
	'constexpr float FourthFloorZ = 900.0f;',
	'const FVector PhoneAtDoorLocation(150.0f, -196.0f, FourthFloorZ + 4.0f);'
) '§5.5 phone placement'

# --- 순서. 한 번 → 문구멍 → 폰 → 3연 → 기다림 → 끌림 ----------------------
Require-All $beatHeader @(
	'Idle,',
	'Opening,',
	'AwaitingPeephole,',
	'AwaitingPhone,',
	'Answering,',
	'Spent'
) '비트 2-1 stage order'
Require-All $beat @(
	'void AIGMissingFloorNightTwoBeatDirector::PlayFirstKnock()',
	'void AIGMissingFloorNightTwoBeatDirector::PlayAnswer()',
	'void AIGMissingFloorNightTwoBeatDirector::PlayDragAway()',
	'CreateWallKnockSingle(',
	'CreateWallKnockTriple(',
	'CreateEntityDragLoop(this, /*bVinyl=*/false)',
	'KnockCount = 1;',
	'KnockCount = 2;',
	'KnockCount = 3;'
) '비트 2-1 cue order'

# --- 값. 3연이 1.0이어야 §5.5의 2.10초 무음이 나온다 -----------------------
Require-All $beat @(
	'constexpr float SingleKnockLoudness = 0.55f;',
	'constexpr float TripleKnockLoudness = 1.0f;',
	'constexpr float DragLoudness = 0.42f;',
	'constexpr float SingleKnockMuffle = 0.72f;',
	'constexpr float TripleKnockMuffle = 0.66f;'
) '비트 2-1 noise table'
Require-All $recording @(
	'constexpr float MaximumSuppressedSeconds = 2.10f;'
) '§5.5 gap length table'

# 【S】0.7 — §8 표의 값 그대로.
Require-All $beatHeader @(
	'static constexpr float ScareAmount = 0.7f;'
) '비트 2-1 scare value'
Require-All $beat @(
	'Stress->ApplyScare(ScareAmount);'
) '비트 2-1 scare wiring'

# --- 막히지 않음 -----------------------------------------------------------
# 문구멍도 폰도 인내 시간이 있다. 비트가 플레이어의 확인을 기다려 정지하면
# §20.3이 금지하는 그 상태가 된다.
Require-All $beat @(
	'constexpr float PeepholePatienceSeconds = 40.0f;',
	'constexpr float PhonePatienceSeconds = 55.0f;',
	'StageSeconds >= IGNightTwo::PeepholePatienceSeconds',
	'StageSeconds >= IGNightTwo::PhonePatienceSeconds',
	'constexpr float OpeningDelaySeconds = 6.0f;',
	'constexpr float WaitAfterTripleSeconds = 3.4f;'
) '비트 2-1 patience'

# --- 형체는 빌린 것 --------------------------------------------------------
Require-All $beat @(
	'Listener->TeleportTo(',
	'IGNightTwo::FigureStagePoint,',
	'Listener->SetPatrolPoints(CorridorPatrolPoints);',
	'Listener->ResetToPatrolStart(/*bRaiseAggression=*/false);'
) '비트 2-1 borrowed figure'
# 복도의 그가 소리의 발신자다. 그래야 §5.5가 그것을 거부한다.
Require-All $beat @(
	'Noise->ReportNoise(',
	'Entity.Get());'
) '비트 2-1 noise instigator'

# --- 1회 한정 -------------------------------------------------------------
Require-All $beat @(
	'const FName BeatId(TEXT("Night2.DoorKnock"));',
	'const FName PeepholeBeatId(TEXT("Night2.Peephole"));',
	'Narrative->MarkBeatPlayed(IGNightTwo::BeatId)',
	'Narrative->MarkBeatPlayed(IGNightTwo::PeepholeBeatId)'
) '비트 2-1 once per run'
# 밤2가 아니면 무장하지 않고, 문구멍은 노크 전에 열려 있지 않다.
Require-All $beat @(
	'Narrative->GetNightIndex() == 2',
	'Peephole->SetInteractionEnabled(false);',
	'Peephole->SetInteractionEnabled(true);'
) '비트 2-1 arming'

# --- 배선과 검증 ----------------------------------------------------------
Require-All $greyboxHeader @(
	'NightTwoDoorBeatContract,',
	'TObjectPtr<class AIGMissingFloorNightTwoBeatDirector> NightTwoBeats;'
) 'beat director wiring'
Require-All $greybox @(
	'NightTwoBeats = World->SpawnActor<AIGMissingFloorNightTwoBeatDirector>(',
	'NightTwoBeats->SetHourActive(bActive);',
	'case EProbeStep::NightTwoDoorBeatContract:',
	'MISSINGFLOOR_N2DOOR PASS',
	'NightTwoBeats->AdvanceForTesting();',
	'the figure stayed at the door after the beat',
	'the armed phone was not running for the answer',
	'the triple knock left only'
) 'runtime beat probe'

Write-Host (
	"MISSINGFLOOR_M3_DOOR_BEAT_CONTRACT PASS assertions=$assertions " +
	'cues=3 single=0.55 triple=1.00 drag=0.42 gap_seconds=2.10 scare=0.7 ' +
	'patience=40|55 plays=1') `
	-ForegroundColor Green
