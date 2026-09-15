[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

$projectRoot = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$assertionCount = 0

function Read-ProjectText {
	param([Parameter(Mandatory)][string]$RelativePath)

	$path = Join-Path $projectRoot $RelativePath
	if (-not (Test-Path -LiteralPath $path -PathType Leaf)) {
		throw "M1_WAKE_ECHO_CONTRACT FAIL: missing file: $RelativePath"
	}
	return Get-Content -Raw -Encoding UTF8 -LiteralPath $path
}

function Assert-True {
	param(
		[Parameter(Mandatory)][bool]$Condition,
		[Parameter(Mandatory)][string]$Message
	)

	if (-not $Condition) {
		throw "M1_WAKE_ECHO_CONTRACT FAIL: $Message"
	}
	$script:assertionCount++
}

function Assert-ContainsAll {
	param(
		[Parameter(Mandatory)][string]$Text,
		[Parameter(Mandatory)][string[]]$Needles,
		[Parameter(Mandatory)][string]$Context
	)

	foreach ($needle in $Needles) {
		Assert-True $Text.Contains($needle) "$Context missing token: $needle"
	}
}

function Get-Block {
	param(
		[Parameter(Mandatory)][string]$Text,
		[Parameter(Mandatory)][string]$Start,
		[Parameter(Mandatory)][string]$End
	)

	$startIndex = $Text.IndexOf($Start)
	$endIndex = $Text.IndexOf($End, $startIndex + $Start.Length)
	Assert-True ($startIndex -ge 0 -and $endIndex -gt $startIndex) `
		"could not isolate block: $Start"
	return $Text.Substring($startIndex, $endIndex - $startIndex)
}

$nightHeader = Read-ProjectText 'Source/IndieGame/Entity/IGNightLoopDirector.h'
$night = Read-ProjectText 'Source/IndieGame/Entity/IGNightLoopDirector.cpp'
$hudHeader = Read-ProjectText 'Source/IndieGame/Player/IGHorrorHUD.h'
$hud = Read-ProjectText 'Source/IndieGame/Player/IGHorrorHUD.cpp'
$greybox = Read-ProjectText 'Source/IndieGame/Entity/IGListenerGreyboxDirector.cpp'
$story = Read-ProjectText 'Docs/STORY_BIBLE_MISSING_FLOOR.md'
$status = Read-ProjectText 'Docs/IMPLEMENTATION_STATUS.md'
$artMatrix = Read-ProjectText 'Docs/MISSING_FLOOR_ART_MATRIX.md'
$readme = Read-ProjectText 'README.md'
$captureRunner = Read-ProjectText 'Scripts/Run-MissingFloor-NightCapture.bat'

# 포획 리셋은 암전뿐 아니라 짧은 기상 잔향까지 하나의 원자적 상태다.
Assert-ContainsAll $nightHeader @(
	'bool IsCaptureResetInFlight() const { return bResetInFlight; }',
	'void FinishWakeRecovery();',
	'float GetWakeEchoSeconds() const;',
	'float GetWakeRecoverySeconds() const;',
	'FTimerHandle WakeRecoveryTimer;'
) 'wake recovery declarations'

Assert-ContainsAll $night @(
	'#include "Audio/IGAudioHelpers.h"',
	'#include "Audio/IGToneSequenceSoundWave.h"',
	'#include "Player/IGHorrorHUD.h"',
	'HorrorHUD->PlayCaptureWakeEcho(',
	'UIGToneSequenceSoundWave::CreateClothSettle(this)',
	'WakeTransform.GetLocation()',
	'&AIGNightLoopDirector::FinishWakeRecovery',
	'WakeEchoSeconds',
	'WakeRecoverySeconds',
	'FMath::Max(GetWakeFadeInSeconds(), GetWakeEchoSeconds())',
	'return 0.68f;',
	'return 0.48f;',
	'return 0.30f;',
	'return 0.16f;'
) 'wake recovery runtime'

$finishResetBlock = Get-Block $night `
	'void AIGNightLoopDirector::FinishReset()' `
	'void AIGNightLoopDirector::FinishWakeRecovery()'
Assert-True (-not $finishResetBlock.Contains('Character->EnableInput(Controller);')) `
	'input must stay locked while the wake echo owns the frame'
Assert-True (
	$finishResetBlock.IndexOf('Character->TeleportTo(') -lt
	$finishResetBlock.IndexOf('HorrorHUD->PlayCaptureWakeEcho(')) `
	'wake echo must begin after the player reaches the wake transform'
Assert-True (
	$finishResetBlock.IndexOf('HorrorHUD->PlayCaptureWakeEcho(') -lt
	$finishResetBlock.IndexOf('&AIGNightLoopDirector::FinishWakeRecovery')) `
	'wake echo must begin before the recovery timer is armed'

$finishRecoveryBlock = Get-Block $night `
	'void AIGNightLoopDirector::FinishWakeRecovery()' `
	'bool AIGNightLoopDirector::SpawnCaptureHandprint('
Assert-ContainsAll $finishRecoveryBlock @(
	'Character->EnableInput(Controller);',
	'CapturedPlayer = nullptr;',
	'bResetInFlight = false;'
) 'single input recovery exit'

# 기상 잔상은 기존 포옹 셀만 되감고 판정·충돌을 소유하지 않는다.
Assert-ContainsAll $hudHeader @(
	'void PlayCaptureWakeEcho(',
	'int32 CaptureCount,',
	'float VisualDurationSeconds = 0.68f,',
	'float OwnershipDurationSeconds = 0.68f);',
	'bool DrawCaptureWakeEcho(double CurrentTime);',
	'double CaptureWakeEchoStartTime = -1.0;',
	'double CaptureWakeEchoVisualEndTime = -1.0;',
	'double CaptureWakeEchoEndTime = -1.0;',
	'int32 CaptureWakeEchoCount = 0;',
	'bool bCaptureWakeEchoPreview = false;'
) 'wake echo HUD declarations'
Assert-ContainsAll $hud @(
	'TEXT("IGM1WakeEchoPreview")',
	'void AIGHorrorHUD::PlayCaptureWakeEcho(',
	'SafeOwnershipDuration',
	'if (DrawCaptureWakeEcho(CurrentTime))',
	'bool AIGHorrorHUD::DrawCaptureWakeEcho(',
	'CurrentTime >= CaptureWakeEchoVisualEndTime',
	'CaptureWakeEchoVisualEndTime - CaptureWakeEchoStartTime',
	'NormalizedAge >= 0.66f',
	'FrameIndex = 1;',
	'NormalizedAge >= 0.33f',
	'FrameIndex = 2;',
	'int32 FrameIndex = 3;',
	'Accessibility->IsReducedCameraMotionEnabled()',
	'0.34f * RepeatAttenuation',
	'FrameTile.BlendMode = SE_BLEND_Translucent;',
	'const float SpriteSize = FMath::Max(Canvas->ClipX, Canvas->ClipY);'
) 'wake echo HUD presentation'
$wakeDrawBlock = Get-Block $hud `
	'bool AIGHorrorHUD::DrawCaptureWakeEcho(' `
	'float AIGHorrorHUD::MeasureTextWidth('
Assert-True ($wakeDrawBlock.Contains('if (!bReducedMotion)')) `
	'reduced motion must keep the closed static pose'
Assert-True ($wakeDrawBlock.Contains('return true;')) `
	'missing art must not release the rest of the HUD during recovery'
Assert-True (-not $wakeDrawBlock.Contains('SE_BLEND_Additive')) `
	'wake echo must not use a flashing additive blend'

Assert-ContainsAll $greybox @(
	'!NightLoop->IsCaptureResetInFlight()',
	'bWakeRecoveryFinished',
	'PlayerCharacter->InputEnabled()',
	'bInputRestored',
	'recovery=%d input=%d'
) 'runtime recovery probe'

Assert-ContainsAll $story @(
	'2026-09-14 수정',
	'## 26. 2026-08-11 제품 감사',
	'### 26.7 2026-08-11 델타 감사',
	'## 29. v2.8 — 포획 뒤 기상 잔향',
	'게임 시작**은 저녁의 프롤로그',
	'포획 뒤 시작**은 새 게임이나 밤 처음부터가 아니다',
	# 잔향 길이는 회차와 짝지어야 뜻이 있다. 맨 숫자만 보면 표가
	# 뒤섞여도 통과한다.
	'| 1회 | 0.68초 | 3.0초 |',
	'| 2회 | 0.48초 | 2.2초 |',
	'| 3~4회 | 0.30초 | 1.4초 |',
	'| 5회 이상 | 0.16초 | 0.4초 |',
	'회차별 페이드 3.0/2.2/1.4/0.4초가 끝난 뒤',
	'1280×800',
	'https://gdconf.com/article/gdc-2026-state-of-the-game-industry-reveals-impact-of-layoffs-generative-ai-and-more/',
	'https://www.routinegame.com/',
	'https://blog.playstation.com/2026/07/13/coloratura-designing-a-world-where-sound-is-the-only-guide/'
) 'v2.8 design and trend audit'

Assert-ContainsAll $status @(
	'정사 v2.8 M1.1 포획 뒤 기상 잔향 구현',
	'게임 전체의 시작, 각 밤의 시작, 포획 뒤 재시작을 분리했다',
	'저녁 입주에서 시작하고',
	'같은 밤의 04:30 침대 위치로만',
	'잔상은 짧게 끝내되 일반 HUD와 입력은',
	'M1.1 기상 잔향 계약',
	'구현 완료·체감 승인 대기'
) 'implementation status'
Assert-ContainsAll $artMatrix @(
	'| 1인칭 기상 잔향 |',
	'포획 포옹 원본 재사용',
	'잔향 뒤 페이드 끝까지 HUD 점유',
	'1920×1080과 1280×800 D3D12 실렌더'
) 'art matrix wake reuse'
Assert-ContainsAll $readme @(
	# README는 플레이어 문서다. 기상 잔향의 HUD 점유 같은 구현 세부는
	# 이 계약의 소스·상태 문서 단언이 잡고, 문서에는 플레이어에게 한
	# 약속만 남는다: 입주 저녁에서 시작하고, 잡히면 같은 밤으로 되감기며,
	# 진행은 잃지 않는다.
	'입주 첫날 저녁부터 시작합니다',
	'붙잡히면 같은 밤 04시 30분의 침대로 돌아옵니다',
	'읽은 기록과 알아낸 것은 그대로 남고'
) 'player-facing readme'
Assert-ContainsAll $captureRunner @(
	'IG_NIGHT_CAPTURE_RES_X',
	'IG_NIGHT_CAPTURE_RES_Y',
	'-ResX=%IG_NIGHT_CAPTURE_RES_X%',
	'-ResY=%IG_NIGHT_CAPTURE_RES_Y%'
) 'resolution-variable night capture'

$wakeMediaPath = Join-Path $projectRoot 'Docs/Media/readme/m1-capture-wake-echo.gif'
Assert-True (Test-Path -LiteralPath $wakeMediaPath -PathType Leaf) `
	'wake echo README GIF must exist'
$wakeMedia = Get-Item -LiteralPath $wakeMediaPath
Assert-True ($wakeMedia.Length -gt 100KB -and $wakeMedia.Length -lt 5MB) `
	'wake echo README GIF must stay inside the review size budget'
$gifHeader = [System.Text.Encoding]::ASCII.GetString(
	[System.IO.File]::ReadAllBytes($wakeMediaPath), 0, 6)
Assert-True ($gifHeader -in @('GIF87a', 'GIF89a')) `
	'wake echo README media must be a real GIF'

Write-Host (
	'M1 wake echo contract passed ({0} assertions).' -f $assertionCount) `
	-ForegroundColor Green
