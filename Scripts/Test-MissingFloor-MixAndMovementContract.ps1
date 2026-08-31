[CmdletBinding()]
param()

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

# STORY_BIBLE_MISSING_FLOOR.md의 남은 숫자 표 둘을 코드 상수와 값으로 대조한다.
#
#   §10.4 거리 리버브 프리셋   복도/계단실/옥상 × 감쇠·HF·확산·첫 반사
#   §18.2 이동감 수치 계약     걷기/앉기/달리기 × 속도·가속·제동·소음
#
# §21.2·§20.2를 잠근 계약과 같은 방식이다. 표를 파싱하고 상수를 파싱해 숫자를
# 맞추므로 어느 쪽이 움직여도 잡힌다.
#
# 두 표는 값만이 아니라 **값들 사이의 관계**가 설계다. 그래서 관계도 같이 본다.
#
#   - 계단실은 복도의 두 배 넘게 울려야 한다. 그 차이가 「계단에서 내 발소리가
#     훨씬 오래 남는다」는 §5.1의 학습을 만든다
#   - ENTITY의 젖음 하한은 0이 아니어야 한다. 완전히 마르는 것은 「같은 방」의
#     뜻으로 예약되어 있고, 그 예외는 포획 노크 하나뿐이다(§21.3)
#   - 달리기 제동만 셋 중 가장 낮아야 한다. 급정지가 안 되는 것이 달린 대가다

$projectRoot = Split-Path -Parent $PSScriptRoot
$assertionCount = 0

function Read-ProjectText {
	param([Parameter(Mandatory = $true)][string]$RelativePath)
	$path = Join-Path $projectRoot $RelativePath
	if (-not (Test-Path -LiteralPath $path -PathType Leaf)) {
		throw "Missing mix/movement contract file: $RelativePath"
	}
	return Get-Content -Raw -Encoding UTF8 -LiteralPath $path
}

function Get-Section {
	param(
		[Parameter(Mandatory = $true)][string]$Text,
		[Parameter(Mandatory = $true)][string]$StartPattern,
		[Parameter(Mandatory = $true)][string]$EndPattern,
		[Parameter(Mandatory = $true)][string]$Label
	)
	$match = [regex]::Match($Text, "$StartPattern(?<body>[\s\S]*?)$EndPattern")
	if (-not $match.Success) {
		throw "$Label section could not be isolated."
	}
	return $match.Groups['body'].Value
}

function Get-TableRows {
	# 정렬 행을 지나기 전은 전부 머리글이다(§21.2 계약과 같은 규칙).
	param([Parameter(Mandatory = $true)][string]$Section)
	$rows = @()
	$pastHeader = $false
	foreach ($line in ($Section -split "`r?`n")) {
		$trimmed = $line.Trim()
		if (-not $trimmed.StartsWith('|') -or -not $trimmed.EndsWith('|')) {
			continue
		}
		$columns = @($trimmed.Trim('|').Split('|') | ForEach-Object { $_.Trim() })
		if ($columns.Count -lt 2) {
			continue
		}
		if ($columns[0] -match '^:?-{2,}:?$') {
			$pastHeader = $true
			continue
		}
		if (-not $pastHeader) {
			continue
		}
		$rows += ,$columns
	}
	return $rows
}

function ConvertTo-Cell {
	<#
	표의 칸을 숫자로 바꾼다. 없는 값(—)은 $null이다.

	표는 사람이 읽는 표기를 쓴다: `300 (유지)`, `**900**`, `×0.86`, `+0.13`,
	`9ms`. 굵게와 괄호 주석을 떼고, 밀리초는 코드가 쓰는 초로 내린다.
	#>
	param([Parameter(Mandatory = $true)][string]$Cell)
	$clean = $Cell.Trim().Replace('**', '')
	$clean = [regex]::Replace($clean, '\([^)]*\)', '').Trim()
	if ($clean -eq '—' -or $clean -eq '-' -or $clean -eq '') {
		return $null
	}
	$clean = $clean.TrimStart('×', '+')
	if ($clean -match '^(?<value>[0-9]+(?:\.[0-9]+)?)\s*ms$') {
		return [double]$Matches['value'] / 1000.0
	}
	if ($clean -match '^(?<value>[0-9]+(?:\.[0-9]+)?)\s*s$') {
		return [double]$Matches['value']
	}
	if ($clean -match '^(?<value>[0-9]+(?:\.[0-9]+)?)$') {
		return [double]$Matches['value']
	}
	throw "A table cell is not a number: '$Cell'"
}

function Get-Constant {
	<#
	`constexpr ... Name = 1.25f;` 또는 `Object->Field = 0.80f;`의 값.
	#>
	param(
		[Parameter(Mandatory = $true)][string]$Text,
		[Parameter(Mandatory = $true)][string]$Expression
	)
	$escaped = [regex]::Escape($Expression)
	$match = [regex]::Match($Text, "$escaped\s*=\s*(?<value>-?[0-9]+(?:\.[0-9]+)?)f?\s*;")
	if (-not $match.Success) {
		throw "Constant could not be found: $Expression"
	}
	return [double]$match.Groups['value'].Value
}

function Assert-Number {
	param(
		[Parameter(Mandatory = $true)][double]$Expected,
		[Parameter(Mandatory = $true)][double]$Actual,
		[Parameter(Mandatory = $true)][string]$Label
	)
	$script:assertionCount++
	if ([Math]::Abs($Expected - $Actual) -gt 0.0005) {
		throw "$Label differs: doc $Expected, code $Actual."
	}
}

$story = Read-ProjectText 'Docs/STORY_BIBLE_MISSING_FLOOR.md'
$audioSource = Read-ProjectText 'Source/IndieGame/Audio/IGMissingFloorAudioSubsystem.cpp'
$characterSource = Read-ProjectText 'Source/IndieGame/Player/IGPlayerCharacter.cpp'

# --- §10.4 거리 리버브 프리셋 -----------------------------------------------

$reverbSection = Get-Section $story '\*\*확정 파라미터 \(2026-08-12 구현\):\*\*' `
	'복도를 실측보다 짧게 잡은 것은 의도다' '§10.4 리버브 프리셋'
$reverbRows = Get-TableRows $reverbSection

$assertionCount++
if ($reverbRows.Count -ne 3) {
	throw "§10.4 must list three spaces, found $($reverbRows.Count)."
}

# 감쇠와 HF는 namespace 상수 선언에서, 확산과 첫 반사는 프리셋 조립부의
# 대입문에서 읽는다. 호출부는 `IGMissingFloorMix::` 한정 이름을 쓰지만 값이
# 적힌 자리는 선언이다.
$reverbFields = [ordered]@{
	'복도'   = @{
		Decay        = 'constexpr float CorridorDecayTime'
		DecayHFRatio = 'constexpr float CorridorDecayHFRatio'
		Diffusion    = 'Corridor->Diffusion'
		Reflections  = 'Corridor->ReflectionsDelay'
	}
	'계단실' = @{
		Decay        = 'constexpr float StairwellDecayTime'
		DecayHFRatio = 'constexpr float StairwellDecayHFRatio'
		Diffusion    = 'Stairwell->Diffusion'
		Reflections  = 'Stairwell->ReflectionsDelay'
	}
}

foreach ($row in $reverbRows) {
	$space = $row[0]
	if ($row.Count -lt 5) {
		throw "§10.4 row '$space' is missing columns."
	}
	$decay = ConvertTo-Cell $row[1]

	if ($space -eq '옥상') {
		# 옥상은 프리셋이 없다 — 반사면이 바닥뿐이다. 네 칸이 전부 비어야 하고,
		# 코드에도 옥상 프리셋이 있으면 안 된다.
		for ($column = 1; $column -le 4; $column++) {
			$assertionCount++
			if ($null -ne (ConvertTo-Cell $row[$column])) {
				throw '§10.4 옥상 must stay without a reverb preset.'
			}
		}
		$assertionCount++
		if ([regex]::IsMatch($audioSource, 'Rooftop(Decay|Diffusion|Reflections)')) {
			throw 'A rooftop reverb preset appeared; §10.4 says the roof has none.'
		}
		continue
	}

	if (-not $reverbFields.Contains($space)) {
		throw "§10.4 has a space this contract does not map: '$space'"
	}
	$fields = $reverbFields[$space]
	Assert-Number $decay (Get-Constant $audioSource $fields.Decay) `
		"§10.4 '$space' 감쇠"
	Assert-Number (ConvertTo-Cell $row[2]) `
		(Get-Constant $audioSource $fields.DecayHFRatio) "§10.4 '$space' DecayHFRatio"
	Assert-Number (ConvertTo-Cell $row[3]) `
		(Get-Constant $audioSource $fields.Diffusion) "§10.4 '$space' 확산"
	Assert-Number (ConvertTo-Cell $row[4]) `
		(Get-Constant $audioSource $fields.Reflections) "§10.4 '$space' 첫 반사"
}

# 「두 프리셋의 감쇠 비가 2배 이상이라는 것이 계약이다」 — 문서가 명시한 관계다.
$corridorDecay = Get-Constant $audioSource 'constexpr float CorridorDecayTime'
$stairwellDecay = Get-Constant $audioSource 'constexpr float StairwellDecayTime'
$assertionCount++
if (($stairwellDecay / $corridorDecay) -lt 2.0) {
	throw ('The stairwell must ring at least twice as long as the corridor ' +
		"(§10.4): $stairwellDecay / $corridorDecay.")
}
$assertionCount++
if (-not $story.Contains('두 프리셋의 감쇠 비가 2배 이상이라는 것이 계약이다')) {
	throw 'The §10.4 decay-ratio contract sentence was removed.'
}

# 젖음 하한. ENTITY > PUZZLE > WORLD이고 ENTITY는 0이 아니다 — 드라이는
# 「같은 방」의 뜻이라 포획 노크 하나에 예약되어 있다.
$entityFloor = Get-Constant $audioSource 'constexpr float EntityReverbFloor'
$puzzleFloor = Get-Constant $audioSource 'constexpr float PuzzleReverbFloor'
$worldFloor = Get-Constant $audioSource 'constexpr float WorldReverbFloor'
$playerSend = Get-Constant $audioSource 'constexpr float PlayerManualReverbSend'
Assert-Number 0.22 $entityFloor '§10.4 ENTITY 젖음 하한'
Assert-Number 0.15 $puzzleFloor '§10.4 PUZZLE 젖음 하한'
Assert-Number 0.10 $worldFloor '§10.4 WORLD 젖음 하한'
Assert-Number 0.30 $playerSend '§10.4 PLAYER 고정 센드'
$assertionCount++
if ($entityFloor -le 0.0) {
	throw 'ENTITY must never go fully dry outside the capture knock (§10.4).'
}
$assertionCount++
if (-not ($entityFloor -gt $puzzleFloor -and $puzzleFloor -gt $worldFloor)) {
	throw 'The wetness floors must stay ordered ENTITY > PUZZLE > WORLD (§10.4).'
}

# --- §18.2 이동감 수치 계약 -------------------------------------------------

$movementSection = Get-Section $story '### 18\.2 이동감 수치 계약' '### 18\.3' `
	'§18.2 이동감 수치 계약'
$movementRows = Get-TableRows $movementSection

$assertionCount++
if ($movementRows.Count -ne 5) {
	throw "§18.2 must list five states, found $($movementRows.Count)."
}

$movementConstants = [ordered]@{
	'걷기'      = @{
		Speed        = 'constexpr float ReferenceWalkSpeed'
		Acceleration = 'constexpr float WalkAcceleration'
		Braking      = 'constexpr float WalkBraking'
	}
	'앉아 이동' = @{
		Speed        = 'constexpr float CrouchSpeed'
		Acceleration = 'constexpr float CrouchAcceleration'
		Braking      = 'constexpr float CrouchBraking'
	}
	'달리기'    = @{
		Speed        = 'constexpr float SprintSpeed'
		Acceleration = 'constexpr float SprintAcceleration'
		Braking      = 'constexpr float SprintBraking'
	}
}

# 지형 행은 속도 배율이고, 소음 열은 §21.2 장판 기준의 증가분이다.
$terrainScales = @{
	'자재·파편 지대(5층)' = 0.86
	'물 고인 바닥(밤4)'   = 0.78
}
$surfaceScaleBody = Get-Section $characterSource `
	'float AIGPlayerCharacter::GetSurfaceMovementScale\(' 'return 1\.0f;' `
	'Surface movement scale'

foreach ($row in $movementRows) {
	$state = $row[0]
	if ($row.Count -lt 5) {
		throw "§18.2 row '$state' is missing columns."
	}

	if ($terrainScales.ContainsKey($state)) {
		$assertionCount++
		$expectedScale = $terrainScales[$state]
		if (-not $surfaceScaleBody.Contains(("{0:0.00}f" -f $expectedScale))) {
			throw "§18.2 '$state' movement scale $expectedScale is not in the code."
		}
		Assert-Number $expectedScale (ConvertTo-Cell $row[1]) "§18.2 '$state' 배율"
		continue
	}

	if (-not $movementConstants.Contains($state)) {
		throw "§18.2 has a state this contract does not map: '$state'"
	}
	$constants = $movementConstants[$state]
	Assert-Number (ConvertTo-Cell $row[1]) `
		(Get-Constant $characterSource $constants.Speed) "§18.2 '$state' 속도"
	Assert-Number (ConvertTo-Cell $row[2]) `
		(Get-Constant $characterSource $constants.Acceleration) "§18.2 '$state' 가속"
	Assert-Number (ConvertTo-Cell $row[3]) `
		(Get-Constant $characterSource $constants.Braking) "§18.2 '$state' 제동"
}

# 「달리기 제동만 낮게 둔다」 — 급정지가 안 되므로 달린 대가가 코너에서
# 청구된다. 값이 다 맞아도 이 관계가 뒤집히면 그 문장이 거짓이 된다.
$walkBraking = Get-Constant $characterSource 'constexpr float WalkBraking'
$crouchBraking = Get-Constant $characterSource 'constexpr float CrouchBraking'
$sprintBraking = Get-Constant $characterSource 'constexpr float SprintBraking'
$assertionCount++
if (-not ($sprintBraking -lt $walkBraking -and $sprintBraking -lt $crouchBraking)) {
	throw 'Sprint braking must stay the lowest of the three (§18.2).'
}

# 앉기 전환은 즉시가 아니다. 은신 결정에도 비용이 있어야 그 결정이 플레이다.
Assert-Number 0.35 (Get-Constant $characterSource 'constexpr float CrouchTransitionSeconds') `
	'§18.2 앉기 전환 시간'
Assert-Number 0.5 (Get-Constant $characterSource 'constexpr float CrouchTransitionSpeedScale') `
	'§18.2 앉기 전환 속도 배율'
$assertionCount++
if (-not $story.Contains('앉기 전환은 0.35s')) {
	throw 'The §18.2 crouch-transition sentence lost its authored seconds.'
}

# 달리기 제동은 스태미나 게이지가 아니라 소음이 건다(§19의 게이지 금지).
Assert-Number 3.5 `
	(Get-Constant $characterSource 'constexpr float SprintBreathThresholdSeconds') `
	'§18.2 달리기 호흡 문턱'
$assertionCount++
if (-not $characterSource.Contains('BreathLoad * 0.20f')) {
	throw 'The sprint breath load must stay capped at +0.20 (§18.2).'
}
$assertionCount++
if (-not $story.Contains('3.5초 이후 초당')) {
	throw 'The §18.2 sprint-noise sentence lost its authored threshold.'
}

# §18.2의 소음 열은 §21.2 장판 행과 같은 값이어야 한다. 두 표가 같은 사실을
# 다르게 적으면 어느 쪽이 정사인지 알 수 없어진다.
$footstepSection = Get-Section $story '### 21\.2 발소리 매트릭스' '### 21\.3' `
	'§21.2 발소리 매트릭스'
$footstepRows = Get-TableRows $footstepSection
$vinylRow = @($footstepRows | Where-Object { $_[0] -like '장판*' })
$assertionCount++
if ($vinylRow.Count -ne 1) {
	throw 'The §21.2 vinyl row could not be found for the §18.2 cross-check.'
}
$walkRow = @($movementRows | Where-Object { $_[0] -eq '걷기' })[0]
$crouchRow = @($movementRows | Where-Object { $_[0] -eq '앉아 이동' })[0]
$sprintRow = @($movementRows | Where-Object { $_[0] -eq '달리기' })[0]
Assert-Number (ConvertTo-Cell $vinylRow[0][1]) (ConvertTo-Cell $walkRow[4]) `
	'§18.2 걷기 소음 vs §21.2 장판'
Assert-Number (ConvertTo-Cell $vinylRow[0][2]) (ConvertTo-Cell $crouchRow[4]) `
	'§18.2 앉기 소음 vs §21.2 장판'
Assert-Number (ConvertTo-Cell $vinylRow[0][3]) (ConvertTo-Cell $sprintRow[4]) `
	'§18.2 달리기 소음 vs §21.2 장판'

Write-Host (
	'MISSING_FLOOR_MIX_MOVEMENT_CONTRACT PASS spaces={0} states={1} assertions={2}' -f `
		$reverbRows.Count, $movementRows.Count, $assertionCount) `
	-ForegroundColor Green
