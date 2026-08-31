[CmdletBinding()]
param()

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

# STORY_BIBLE_MISSING_FLOOR.md의 숫자 표 둘을 코드의 배열과 **값으로** 대조한다.
#
#   §21.2 발소리 매트릭스   6표면 × 걷기/앉기/달리기
#   §20.2 튜닝 테이블       7파라미터 × 밤1~4
#
# 토큰이 있나 없나를 보는 검사와 다르다. 표를 파싱하고 배열을 파싱해서 숫자를
# 하나씩 맞춰 보므로, **어느 쪽이 움직여도** 잡힌다. 표만 고치고 코드를 안
# 고치는 것도, 코드만 조율하고 표를 안 고치는 것도 같은 결함이다.
#
# 이 두 표는 특히 그렇게 되기 쉽다. §21.2는 「가장 시끄러운 바닥이 가장 중요한
# 장소에 있다」는 설계를 숫자로 표현한 것이고, §20.2는 밤이 깊어지는 곡선
# 자체다. 한 칸이 조용히 달라지면 압박 곡선이 문서와 다른 게임이 된다.

$projectRoot = Split-Path -Parent $PSScriptRoot
$assertionCount = 0

function Read-ProjectText {
	param([Parameter(Mandatory = $true)][string]$RelativePath)
	$path = Join-Path $projectRoot $RelativePath
	if (-not (Test-Path -LiteralPath $path -PathType Leaf)) {
		throw "Missing tuning-table contract file: $RelativePath"
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
	<#
	마크다운 표의 **본문 행만** 돌려준다. 머리글과 `|---|---:|` 정렬 행은
	제외한다. 각 행은 앞뒤 파이프를 떼고 열 배열로 준다.
	#>
	param([Parameter(Mandatory = $true)][string]$Section)
	$rows = @()
	# 정렬 행(`|---|---:|`)을 지나기 전까지는 전부 머리글이다. 처음에는 정렬
	# 행만 걸렀는데 그러면 머리글이 표면 하나로 세어져 여섯이 일곱이 됐다.
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

function ConvertTo-TableNumber {
	<#
	표의 칸을 숫자로 바꾼다. 단위 접미사(m·s)를 떼고, 미터는 센티미터로
	올린다 — 코드가 cm를 쓰기 때문이다.
	#>
	param(
		[Parameter(Mandatory = $true)][string]$Cell,
		[Parameter(Mandatory = $true)][string]$Label
	)
	$clean = $Cell.Trim()
	if ($clean -match '^(?<value>[0-9]+(?:\.[0-9]+)?)\s*m$') {
		return [double]$Matches['value'] * 100.0
	}
	if ($clean -match '^(?<value>[0-9]+(?:\.[0-9]+)?)\s*s$') {
		return [double]$Matches['value']
	}
	if ($clean -match '^(?<value>[0-9]+(?:\.[0-9]+)?)$') {
		return [double]$Matches['value']
	}
	throw "$Label has a cell that is not a number: '$Cell'"
}

function Get-CodeNumbers {
	<#
	`constexpr ... Name[] = {a, b, c};` 형태에서 숫자만 뽑는다.
	#>
	param(
		[Parameter(Mandatory = $true)][string]$Text,
		[Parameter(Mandatory = $true)][string]$ArrayName
	)
	$escaped = [regex]::Escape($ArrayName)
	$match = [regex]::Match($Text, "$escaped\[\]\s*=\s*\{(?<body>[^}]*)\}")
	if (-not $match.Success) {
		throw "Array could not be found: $ArrayName"
	}
	return @([regex]::Matches($match.Groups['body'].Value, '-?[0-9]+(?:\.[0-9]+)?') |
		ForEach-Object { [double]$_.Value })
}

function Assert-NumbersEqual {
	param(
		[Parameter(Mandatory = $true)][AllowEmptyCollection()][double[]]$Expected,
		[Parameter(Mandatory = $true)][AllowEmptyCollection()][double[]]$Actual,
		[Parameter(Mandatory = $true)][string]$Label
	)
	if ($Expected.Count -ne $Actual.Count) {
		throw ("$Label length mismatch: doc has $($Expected.Count), " +
			"code has $($Actual.Count).")
	}
	for ($index = 0; $index -lt $Expected.Count; $index++) {
		$script:assertionCount++
		if ([Math]::Abs($Expected[$index] - $Actual[$index]) -gt 0.0005) {
			throw ("$Label differs at column $($index + 1): " +
				"doc $($Expected[$index]), code $($Actual[$index]).")
		}
	}
}

$story = Read-ProjectText 'Docs/STORY_BIBLE_MISSING_FLOOR.md'
$characterSource = Read-ProjectText 'Source/IndieGame/Player/IGPlayerCharacter.cpp'
$toneHeader = Read-ProjectText 'Source/IndieGame/Audio/IGToneSequenceSoundWave.h'
$tuningSource = Read-ProjectText 'Source/IndieGame/Entity/IGListenerTuning.cpp'

# --- §21.2 발소리 매트릭스 --------------------------------------------------

$footstepSection = Get-Section $story '### 21\.2 발소리 매트릭스' '### 21\.3' `
	'§21.2 발소리 매트릭스'
$footstepRows = Get-TableRows $footstepSection

$assertionCount++
if ($footstepRows.Count -ne 6) {
	throw "§21.2 must list six surfaces, found $($footstepRows.Count)."
}

# 표면 열거형이 표와 같은 수여야 한다. 행 순서가 곧 열거형 순서이므로
# 하나만 늘어도 그 뒤 전부가 다른 표면의 값을 읽는다.
$surfaceEnumBody = Get-Section $toneHeader 'enum class EIGFootstepSurface : uint8\s*\{' '\}' `
	'EIGFootstepSurface'
$surfaceNames = @([regex]::Matches($surfaceEnumBody, '(?m)^\s*(?<name>[A-Za-z][A-Za-z0-9]*)\s*,?\s*$') |
	ForEach-Object { $_.Groups['name'].Value })
$assertionCount++
if ($surfaceNames.Count -ne $footstepRows.Count) {
	throw ("EIGFootstepSurface has $($surfaceNames.Count) surfaces but §21.2 " +
		"lists $($footstepRows.Count).")
}

# 코드의 매트릭스는 `{walk, crouch, sprint}, // 이름` 여섯 줄이다.
$matrixBody = Get-Section $characterSource `
	'static constexpr FSurfaceLoudness Matrix\[\] =\s*\{' '\};' `
	'Footstep loudness matrix'
$matrixRows = @([regex]::Matches(
	$matrixBody,
	'\{\s*(?<walk>[0-9.]+)f\s*,\s*(?<crouch>[0-9.]+)f\s*,\s*(?<sprint>[0-9.]+)f\s*\}'))
$assertionCount++
if ($matrixRows.Count -ne $footstepRows.Count) {
	throw ("The loudness matrix has $($matrixRows.Count) rows but §21.2 lists " +
		"$($footstepRows.Count).")
}

for ($rowIndex = 0; $rowIndex -lt $footstepRows.Count; $rowIndex++) {
	$docRow = $footstepRows[$rowIndex]
	$surface = $docRow[0]
	if ($docRow.Count -lt 4) {
		throw "§21.2 row '$surface' is missing columns."
	}
	$expected = [double[]]@(
		(ConvertTo-TableNumber $docRow[1] "§21.2 '$surface' 걷기"),
		(ConvertTo-TableNumber $docRow[2] "§21.2 '$surface' 앉기"),
		(ConvertTo-TableNumber $docRow[3] "§21.2 '$surface' 달리기"))
	$codeRow = $matrixRows[$rowIndex]
	$actual = [double[]]@(
		[double]$codeRow.Groups['walk'].Value,
		[double]$codeRow.Groups['crouch'].Value,
		[double]$codeRow.Groups['sprint'].Value)
	Assert-NumbersEqual $expected $actual "§21.2 '$surface' ($($surfaceNames[$rowIndex]))"
}

# 설계의 요점은 순서다: 5층 파편과 밤4의 물이 가장 시끄러워야 지형이 난이도가
# 된다. 값이 전부 일치해도 이 관계가 깨지면 그 문장이 거짓이 된다.
$assertionCount++
$sprintValues = @($matrixRows | ForEach-Object { [double]$_.Groups['sprint'].Value })
$loudestIndex = 0
for ($index = 1; $index -lt $sprintValues.Count; $index++) {
	if ($sprintValues[$index] -gt $sprintValues[$loudestIndex]) {
		$loudestIndex = $index
	}
}
if ($surfaceNames[$loudestIndex] -ne 'Water') {
	throw ('The loudest floor must stay the night-four water (§21.2); ' +
		"found $($surfaceNames[$loudestIndex]).")
}
$assertionCount++
if ($sprintValues[4] -le $sprintValues[1]) {
	throw 'Gypsum debris must stay louder than the ordinary corridor (§21.2).'
}

# --- §20.2 튜닝 테이블 ------------------------------------------------------

$tuningSection = Get-Section $story '### 20\.2 튜닝 테이블 \(초기값\)' '### 20\.3' `
	'§20.2 튜닝 테이블'
$tuningRows = Get-TableRows $tuningSection

$assertionCount++
if ($tuningRows.Count -ne 7) {
	throw "§20.2 must list seven parameters, found $($tuningRows.Count)."
}

# 표의 행 이름 → 코드의 배열 이름. 이 사전이 표와 코드를 잇는 유일한 자리다.
$tuningArrays = [ordered]@{
	'기본 청취 반경'      = 'NightHearingRadius'
	'LISTENING 창'        = 'NightListenWindow'
	'순찰 노드 수'        = 'NightPatrolNodes'
	'CHASE 속도 배율'     = 'NightChaseMultiplier'
	'INVESTIGATE 도착 청취' = 'NightInvestigateHold'
	'소음 히트맵 가중(§5.6)' = 'NightHeatmapWeight'
}

foreach ($docRow in $tuningRows) {
	$parameter = $docRow[0]
	if ($docRow.Count -lt 5) {
		throw "§20.2 row '$parameter' must carry four nights."
	}

	if ($parameter -eq '티어3 매복') {
		# 유일한 비수치 행. 불가/가능을 false/true로 읽는다.
		$expectedFlags = @()
		for ($night = 1; $night -le 4; $night++) {
			switch ($docRow[$night]) {
				'불가' { $expectedFlags += 'false' }
				'가능' { $expectedFlags += 'true' }
				default { throw "§20.2 티어3 매복 has an unreadable cell: '$($docRow[$night])'" }
			}
		}
		$ambushMatch = [regex]::Match(
			$tuningSource, 'NightAmbushAllowed\[\]\s*=\s*\{(?<body>[^}]*)\}')
		if (-not $ambushMatch.Success) {
			throw 'NightAmbushAllowed could not be found.'
		}
		$actualFlags = @([regex]::Matches($ambushMatch.Groups['body'].Value, 'true|false') |
			ForEach-Object { $_.Value })
		$assertionCount++
		if (($expectedFlags -join ',') -ne ($actualFlags -join ',')) {
			throw ('§20.2 티어3 매복 differs: doc ' + ($expectedFlags -join '/') +
				', code ' + ($actualFlags -join '/') + '.')
		}
		continue
	}

	if (-not $tuningArrays.Contains($parameter)) {
		throw "§20.2 has a parameter this contract does not map: '$parameter'"
	}
	$expected = [double[]]@(
		for ($night = 1; $night -le 4; $night++) {
			ConvertTo-TableNumber $docRow[$night] "§20.2 '$parameter' 밤$night"
		})
	$actual = [double[]](Get-CodeNumbers $tuningSource $tuningArrays[$parameter])
	Assert-NumbersEqual $expected $actual "§20.2 '$parameter'"
}

# 공격성 티어는 별도 축이고, 표 아래 글이 그 값을 LISTENING 8→6→5→4초로
# 적어 두었다. 코드는 배율로 들고 있으므로 밤1 기준값 8초를 곱해서 맞춘다.
$assertionCount++
if (-not $story.Contains('LISTENING 8→6→5→4s')) {
	throw 'The §20.2 tier axis note lost its authored LISTENING seconds.'
}
$tierScales = Get-CodeNumbers $tuningSource 'TierListenScale'
$nightListen = Get-CodeNumbers $tuningSource 'NightListenWindow'
$expectedTierSeconds = [double[]]@(8.0, 6.0, 5.0, 4.0)
$actualTierSeconds = [double[]]@($tierScales | ForEach-Object { $_ * $nightListen[0] })
Assert-NumbersEqual $expectedTierSeconds $actualTierSeconds '§20.2 티어 축 LISTENING'

# CHASE 배율의 기준은 기는 속도 110이다. 걷기(300)로 바뀌면 밤1부터 720cm/s가
# 되어 §20.2의 「밤1은 잡히기 어렵게」가 성립하지 않는다.
$assertionCount++
if (-not $tuningSource.Contains('CrawlSpeedBase = 110.0f')) {
	throw 'The chase multiplier must stay anchored to the crawl speed (§20.2).'
}

Write-Host (
	'MISSING_FLOOR_TUNING_TABLE_CONTRACT PASS surfaces={0} parameters={1} assertions={2}' -f `
		$footstepRows.Count, $tuningRows.Count, $assertionCount) `
	-ForegroundColor Green
