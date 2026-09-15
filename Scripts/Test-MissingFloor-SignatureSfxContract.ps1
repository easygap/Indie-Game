[CmdletBinding()]
param()

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

# STORY_BIBLE_MISSING_FLOOR.md §21.3 시그니처 SFX 표를 실제 합성 함수와 대조한다.
#
# §12 출처 표에서 셋이 규칙표에만 있고 세계에 없었던 것과 같은 종류의 검사다.
# 표가 정사라고 적혀 있고 아무도 대조하지 않으면, 행이 늘어나도 큐가 안 생기고
# 큐가 사라져도 표는 그대로다.
#
# 두 층으로 본다.
#
#   1. 행 → 구현 대조. 표의 「구현」 열에 적힌 함수가 실제로 존재하는지.
#      표를 파일에서 읽으므로 행을 추가하면 검사가 따라온다.
#   2. 구조 불변식. 조율 값이 아니라 **설계가 말이 되게 하는 성질**만 잠근다.
#      배관 4단의 컷오프는 「단계가 곧 거리」라는 계약이라 값이 계약이고,
#      포획 노크의 리버브 0은 게임 전체에서 유일한 드라이 큐라는 뜻이다.
#
# 합성 열의 개별 주파수는 잠그지 않는다. 그건 귀로 고치는 값이고, 표는
# 2026-08-31에 이미 실제 소리에 맞춰 정정했다(§21.3.1).

$projectRoot = Split-Path -Parent $PSScriptRoot
$assertionCount = 0

function Read-ProjectText {
	param([Parameter(Mandatory = $true)][string]$RelativePath)
	$path = Join-Path $projectRoot $RelativePath
	if (-not (Test-Path -LiteralPath $path -PathType Leaf)) {
		throw "Missing signature-SFX contract file: $RelativePath"
	}
	return Get-Content -Raw -Encoding UTF8 -LiteralPath $path
}

function Assert-ContainsAll {
	param(
		[Parameter(Mandatory = $true)][string]$Text,
		[Parameter(Mandatory = $true)][string[]]$Tokens,
		[Parameter(Mandatory = $true)][string]$Label
	)
	foreach ($token in $Tokens) {
		$script:assertionCount++
		if (-not $Text.Contains($token)) {
			throw "$Label is missing: $token"
		}
	}
}

function Get-FunctionBody {
	<#
	주석을 걷어 낸 함수 본문. 주석 처리한 코드와 살아 있는 코드를 문자열
	포함으로 구분할 수 없어 계약이 통과해 버린 적이 있다(릴리스 게이트 계약의
	14번). 여기서도 같은 함정을 막는다.
	#>
	param(
		[Parameter(Mandatory = $true)][string]$Text,
		[Parameter(Mandatory = $true)][string]$Name
	)
	$signature = "UIGToneSequenceSoundWave* UIGToneSequenceSoundWave::$Name("
	$escaped = [regex]::Escape($signature)
	$match = [regex]::Match($Text, "$escaped(?<body>[\s\S]*?)\r?\n\}")
	if (-not $match.Success) {
		throw "Signature SFX factory could not be isolated: $Name"
	}
	return [regex]::Replace($match.Groups['body'].Value, '//[^\r\n]*', '')
}

$story = Read-ProjectText 'Docs/STORY_BIBLE_MISSING_FLOOR.md'
$toneHeader = Read-ProjectText 'Source/IndieGame/Audio/IGToneSequenceSoundWave.h'
$toneSource = Read-ProjectText 'Source/IndieGame/Audio/IGToneSequenceSoundWave.cpp'
$entitySource = Read-ProjectText 'Source/IndieGame/Entity/IGListenerEntity.cpp'
$audioHelpers = Read-ProjectText 'Source/IndieGame/Audio/IGAudioHelpers.h'

# --- 1층: 표의 행이 곧 검사 목록 --------------------------------------------

$sfxSection = [regex]::Match(
	$story,
	'### 21\.3 시그니처 SFX 파라미터(?<body>[\s\S]*?)### 21\.4')
if (-not $sfxSection.Success) {
	throw 'The §21.3 signature SFX section could not be isolated.'
}
$sectionBody = $sfxSection.Groups['body'].Value

# 표 본문 행만 고른다. 머리글과 정렬 행은 건너뛴다.
$rows = @([regex]::Matches($sectionBody, '(?m)^\|(?!\s*(?:큐|-))[^\r\n]*\|\s*$'))
$assertionCount++
if ($rows.Count -lt 11) {
	throw "The §21.3 table lost rows: found $($rows.Count), expected at least 11."
}

$mappedFactories = @{}
foreach ($row in $rows) {
	$columns = @($row.Value.Trim('|').Split('|') | ForEach-Object { $_.Trim() })
	if ($columns.Count -lt 4) {
		throw "A §21.3 row has no implementation column: $($row.Value.Trim())"
	}
	$cueName = $columns[0]
	$implementation = $columns[3]
	$factories = @([regex]::Matches($implementation, 'Create[A-Za-z0-9]+') |
		ForEach-Object { $_.Value })
	$assertionCount++
	if ($factories.Count -lt 1) {
		throw "A §21.3 row names no factory: $cueName"
	}
	foreach ($factory in $factories) {
		$assertionCount++
		if (-not $toneHeader.Contains("static UIGToneSequenceSoundWave* $factory(")) {
			throw "§21.3 row '$cueName' names a factory that does not exist: $factory"
		}
		$mappedFactories[$factory] = $true
	}
}

# 표가 실제로 열한 큐를 덮는지. 행 수만 세면 같은 함수를 열한 번 적어도
# 통과하므로, 서로 다른 함수의 수도 본다.
$assertionCount++
if ($mappedFactories.Keys.Count -lt 8) {
	throw ("§21.3 must map to at least eight distinct factories, found " +
		"$($mappedFactories.Keys.Count).")
}

# --- 2층: 값이 곧 계약인 자리 -----------------------------------------------

# 배관 4단. §21.3과 §10.4가 「단계가 곧 거리」로 쓰므로 네 컷오프는 조율
# 값이 아니라 계약이다. 하나라도 움직이면 플레이어가 배운 거리 감각이 깨진다.
$pipeBody = Get-FunctionBody $toneSource 'CreatePipeWaterFlow'
Assert-ContainsAll $pipeBody @(
	'{5000.0f, 2400.0f, 1100.0f, 480.0f}'
) '배관 수류 원근 4단 컷오프'
$assertionCount++
if (-not $pipeBody.Contains('FMath::Clamp(DistanceStep, 0, 3)')) {
	throw 'The pipe-flow distance step must stay clamped to the four authored bands.'
}

# 포획 노크 둘. 간격 0.42초, 두 번, 그리고 게임 전체에서 유일한 드라이 큐다.
$replyBody = Get-FunctionBody $toneSource 'CreateWallKnockReply'
Assert-ContainsAll $replyBody @(
	'KnockIndex < 2',
	'0.42f * KnockIndex'
) '포획 노크 둘'

$assertionCount++
$dryCallCount = ([regex]::Matches($entitySource, 'SpawnDryOneShotAt\(')).Count
if ($dryCallCount -ne 1) {
	throw ('The dry cue must stay unique: found ' + $dryCallCount +
		' SpawnDryOneShotAt calls in the entity.')
}
$assertionCount++
if (-not [regex]::IsMatch(
	$entitySource,
	'SpawnDryOneShotAt\([\s\S]{0,200}?CreateCaptureStruggle')) {
	throw '포획의 마찰·숨·노크는 공간 잔향 없이 바로 들려야 한다.'
}
Assert-ContainsAll $audioHelpers @(
	'SpawnDryOneShotAt('
) '드라이 원샷 헬퍼'

# 노크 먹먹함은 같은 함수의 인자다. 두 벌로 나뉘면 「위 +」라는 표의 구조가
# 깨지고, 언젠가 한쪽만 조율된다.
$knockBody = Get-FunctionBody $toneSource 'CreateWallKnockTriple'
Assert-ContainsAll $knockBody @(
	'FMath::Clamp(Muffle01, 0.0f, 1.0f)'
) '안벽 노크 파라미터화'

# 분진은 전대역이 6 kHz 위다. 낮은 성분이 하나라도 들어오면 가루가 아니라
# 벽이 움직이는 소리가 된다 — 그건 다른 큐다.
$dustBody = Get-FunctionBody $toneSource 'CreatePlasterDustFall'
# 이 함수의 100 이상 리터럴은 전부 주파수다 — 시각·진폭·엔벨로프 지수는
# 모두 100 미만이라 대역만 걸러 낼 수 있다. 처음에는 노트 튜플의 세 번째
# 자리를 정규식으로 집었는데, 알갱이 대역이 배열 변수라 잡히지 않고 대신
# 시작 시각이 잡혔다.
$assertionCount++
$dustBands = @([regex]::Matches($dustBody, '(?<value>[0-9]+(?:\.[0-9]+)?)f\b') |
	ForEach-Object { [double]$_.Groups['value'].Value } |
	Where-Object { $_ -ge 100.0 })
if ($dustBands.Count -lt 8) {
	throw "The dust cue could not be read for its bands (found $($dustBands.Count))."
}
foreach ($band in $dustBands) {
	$assertionCount++
	if ($band -lt 6000.0) {
		throw "분진 낙하 must stay above 6 kHz; found $band Hz."
	}
}

# 석고보드 파쇄 3단. 미장 갈라짐은 독립 큐가 아니라 이 층이므로, 단계가
# 사라지면 표의 한 행이 통째로 사라진다.
$hammerBody = Get-FunctionBody $toneSource 'CreateHammerImpact'
Assert-ContainsAll $hammerBody @(
	'const int32 FractureCount = 3 + Stage * 3;',
	'StageGain[] = {0.62f, 0.84f, 1.00f}'
) '석고 파쇄 3단'

# 길이 열의 루프/유한 구분. 끌림과 프로타주는 계속 나야 하고, 밸브와 망치는
# 끝나야 한다. 뒤집히면 소리가 남거나 끊긴다.
# 첫 인자가 MoveTemp(...)라 괄호가 하나 끼어 있다. 「닫는 괄호 전까지」로
# 읽으면 그 괄호에서 멈춰 두 번째 인자를 못 본다.
$loopFlagPattern = 'ConfigureNotes\(\s*MoveTemp\([A-Za-z0-9_]+\)\s*,\s*{0}\b'
foreach ($loopingCue in @('CreateEntityDragLoop', 'CreateFrottageRub')) {
	$body = Get-FunctionBody $toneSource $loopingCue
	$assertionCount++
	if (-not [regex]::IsMatch($body, ($loopFlagPattern -f 'true'))) {
		throw "$loopingCue must loop (§21.3 길이 열)."
	}
}
foreach ($finiteCue in @('CreateValveOpen', 'CreateHammerImpact', 'CreateWallKnockReply')) {
	$body = Get-FunctionBody $toneSource $finiteCue
	$assertionCount++
	if (-not [regex]::IsMatch($body, ($loopFlagPattern -f 'false'))) {
		throw "$finiteCue must be finite (§21.3 길이 열)."
	}
}

# §21.3.1 — 표를 구현에 맞춘 기록. 이 절이 사라지면 다음 사람이 옛 숫자를
# 보고 「구현이 틀렸다」고 판단한다.
Assert-ContainsAll $story @(
	'### 21.3.1 표를 구현에 맞춰 고친 자리',
	'**구현 열이 이 표의 계약이다.**',
	'미장 갈라짐은 **독립 큐가 아니다.**'
) '§21.3.1 정정 기록'

Write-Host (
	'MISSING_FLOOR_SIGNATURE_SFX_CONTRACT PASS rows={0} factories={1} assertions={2}' -f `
		$rows.Count, $mappedFactories.Keys.Count, $assertionCount) `
	-ForegroundColor Green
