[CmdletBinding()]
param()

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

# STORY_BIBLE_MISSING_FLOOR.md §24 「즉시 차단 22개」 중 코드로 확인할 수 있는데
# 아무 계약도 잠그고 있지 않던 항목들.
#
# 이 파일이 생긴 이유는 21번이 실제로 깨져 있었기 때문이다. 진실 열 개 중
# 둘(T5·T8)이 규칙표에만 있고 세계에 프롭이 없어 도달 불가였는데, 문서에는
# 「설계 잠금」으로 적혀 있었고 어떤 스크립트도 그것을 보고 있지 않았다.
# 선언과 구현이 갈라져도 아무도 모르는 상태가 결함 자체보다 위험하다.
#
# 여기서 보는 것은 셋이다.
#
#   6번  P4 정답 입력을 배우기 전에 요구하거나, 단서 1개로 정답을 노출함
#   10번 자유 순서 한 갈래라도 진행 게이트에 합류하지 못함
#   14번 밤3 P4 이후 05:30 최초 신고와 주간 확인이 생략됨
#
# 나머지 §24 항목이 잠긴 자리(중복 작성 금지):
#
#   16번        Test-MissingFloor-M0InputContract.ps1 — 다섯 동사의 독립 바인딩과
#               「Q must never alias Interact」
#   17·19·22번  Test-Rebirth-AudioContract.ps1 / Test-Rebirth-AccessibilityContract.ps1
#   21번        Test-ArtAssetContract.ps1 — 출처·진실 열거형을 게임플레이 파일과 대조
#   18번        Test-MissingFloor-ReleaseEndingContract.ps1 — 엔딩 C 재도전 경로
#
# 12번(성능·가독성 실측)과 1~5·13번(아트·물리)은 정적 검사의 대상이 아니다.
# 전자는 실측 증거, 후자는 기하 감사와 사람 검수가 맡는다.

$projectRoot = Split-Path -Parent $PSScriptRoot
$assertionCount = 0

function Read-ProjectText {
	param([Parameter(Mandatory = $true)][string]$RelativePath)
	$path = Join-Path $projectRoot $RelativePath
	if (-not (Test-Path -LiteralPath $path -PathType Leaf)) {
		throw "Missing release-gate contract file: $RelativePath"
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

function Get-MethodBody {
	<#
	.SYNOPSIS
	한 메서드의 **실행문만** 돌려준다.

	.DESCRIPTION
	주석을 걷어 내는 것이 요점이다. 이 파일을 처음 쓸 때 「새벽 경계에서
	신고한다」 검사가 호출을 주석 처리해도 통과했다 — 문자열 포함으로 보면
	`// MakeNightThreeFirstReport();`도 그 이름을 담고 있기 때문이다.
	계약이 잡아야 할 첫 번째 회귀가 정확히 그 모양이라, 걷어 내는 자리를
	호출부마다 두지 않고 여기 한 곳에 둔다.
	#>
	param(
		[Parameter(Mandatory = $true)][string]$Text,
		[Parameter(Mandatory = $true)][string]$Signature,
		[Parameter(Mandatory = $true)][string]$Label
	)
	$escaped = [regex]::Escape($Signature)
	$match = [regex]::Match($Text, "$escaped(?<body>[\s\S]*?)\r?\n\}")
	if (-not $match.Success) {
		throw "$Label could not be isolated."
	}
	return [regex]::Replace($match.Groups['body'].Value, '//[^\r\n]*', '')
}

$narrativeSource = Read-ProjectText `
	'Source/IndieGame/Narrative/IGMissingFloorNarrativeSubsystem.cpp'
$nightThreeSource = Read-ProjectText `
	'Source/IndieGame/Entity/IGMissingFloorNightThreeDirector.cpp'
$nightFourSource = Read-ProjectText `
	'Source/IndieGame/Entity/IGMissingFloorNightFourDirector.cpp'
$greyboxSource = Read-ProjectText `
	'Source/IndieGame/Entity/IGListenerGreyboxDirector.cpp'
$controllerSource = Read-ProjectText 'Source/IndieGame/Player/IGPlayerController.cpp'
$controllerHeader = Read-ProjectText 'Source/IndieGame/Player/IGPlayerController.h'
$hudSource = Read-ProjectText 'Source/IndieGame/Player/IGHorrorHUD.cpp'

# --- §24 6번: P4는 배운 뒤에만 물어본다 ------------------------------------
#
# 응답 노크는 이 게임에서 유일하게 「정답이 있는 입력」이다. 그래서 두 가지가
# 동시에 참이어야 무장된다: 어느 벽인지 알 것(T6), 그리고 박자를 **두 곳에서**
# 봤을 것. 한 곳만 봐도 열리면 음성사서함 하나로 정답이 나오고, 그건 §7이
# 약속한 「출처 2개」를 P4에서만 깨는 셈이다.

$answerBody = Get-MethodBody $nightThreeSource `
	'void AIGMissingFloorNightThreeDirector::RefreshAnswerTargetAvailability()' `
	'P4 arming'

Assert-ContainsAll $answerBody @(
	'EIGMissingFloorSource::AnswerRhythmVoicemail',
	'EIGMissingFloorSource::AnswerRhythmNotebook',
	'EIGMissingFloorSource::AnswerRhythmJournal'
) 'P4 rhythm provenance'

Assert-ContainsAll $answerBody @(
	'RhythmClueCount >= 2',
	'HasTruth(EIGMissingFloorTruth::SomeoneInTheWall)',
	'AnswerTarget->SetInteractionEnabled(bReady)',
	'AnswerTarget->SetActorHiddenInGame(!bReady)'
) 'P4 arming condition'

$assertionCount++
if ($answerBody -match 'RhythmClueCount\s*>=\s*1\b') {
	throw 'P4 must not arm on a single rhythm clue (§24 즉시 차단 6).'
}

# 파생 기록도 같은 문턱 안에서만 남는다. 밖에서 남기면 T9가 단서 하나로
# 열리는 경로가 생긴다.
$assertionCount++
$derivedIndex = $answerBody.IndexOf('EIGMissingFloorSource::AnswerRhythmMaterials')
$thresholdIndex = $answerBody.IndexOf('RhythmClueCount >= 2')
if ($derivedIndex -lt 0 -or $thresholdIndex -lt 0 -or $derivedIndex -lt $thresholdIndex) {
	throw 'The derived T9 record must be written only past the two-clue threshold.'
}

# --- §24 10번: 어느 순서로 와도 같은 문에서 만난다 --------------------------
#
# 최종 선택은 위치가 아니라 진실로 잠근다. T6·T7·T9 셋이며 그 밖의 조건을
# 넣으면 특정 경로만 통과하는 문이 된다.

$finalGateBody = Get-MethodBody $narrativeSource `
	'bool UIGMissingFloorNarrativeSubsystem::IsFinalChoiceUnlocked() const' `
	'Final choice gate'

Assert-ContainsAll $finalGateBody @(
	'EIGMissingFloorTruth::SomeoneInTheWall',
	'EIGMissingFloorTruth::WasStillAlive',
	'EIGMissingFloorTruth::WaitingForAnAnswer'
) 'Final choice gate truths'

$assertionCount++
$gateTruthCount = ([regex]::Matches($finalGateBody, 'HasTruth\(')).Count
if ($gateTruthCount -ne 3) {
	throw "Final choice gate must test exactly three truths, found $gateTruthCount."
}

# T6은 이 게임에서 유일하게 대안이 둘인 범주를 갖는다 — 물로 참을성 있게
# 찾거나, 주먹으로 시끄럽게 찾거나. 둘이 **같은 범주** 안에 있어야 두 길이
# 같은 진실에서 만난다. 서로 다른 범주로 갈라놓으면 둘 다 해야 열린다.
$assertionCount++
if (-not [regex]::IsMatch(
	$narrativeSource,
	'EIGMissingFloorTruth::SomeoneInTheWall,\s*TEXT\("Truth\.MissingFloor\.SomeoneInTheWall"\)')) {
	throw 'T6 rule block could not be found.'
}
# 두 이름이 같은 `{{ }}` 안에 있는지만 본다. 범주 경계는 `}}`이므로 그
# 사이에 `}}`가 끼면 서로 다른 범주로 갈라진 것이고, 그러면 두 길을 **둘 다**
# 걸어야 열리는 문이 된다.
$assertionCount++
if (-not [regex]::IsMatch(
	$narrativeSource,
	'\{\{(?:(?!\}\})[\s\S])*?PipeWaterComparison(?:(?!\}\})[\s\S])*?WallEchoByHand')) {
	throw 'T6 must accept the patient and the reckless route in one category.'
}

# 문 앞까지 왔어도 벽이 열리고 대치가 끝나야 결말이 선택된다. 이 셋이
# 빠지면 자유 순서가 아니라 순서 없음이 된다.
$finishEndingBody = Get-MethodBody $nightFourSource `
	'void AIGMissingFloorNightFourDirector::FinishEnding(const FName EndingId)' `
	'Ending commit'
Assert-ContainsAll $finishEndingBody @(
	'Narrative->IsNightFourWallOpened()',
	'IGNightFour::FinalConfrontationBeat',
	'Narrative->SelectEnding(EndingId)'
) 'Ending commit gate'

# --- §24 14번: 신고는 새벽에 하고, 하루는 막간이 채운다 ---------------------
#
# 유담이 벽 안의 목소리를 듣고도 24시간 아무것도 안 한 사람으로 보이면
# 그 뒤의 모든 선택이 이상해진다. 두 가지로 막는다: 신고는 05:30 경계가
# 자동으로 하고, 밤4는 막간을 지나지 않으면 시작되지 않는다.

$hourChangeBody = Get-MethodBody $greyboxSource `
	'void AIGListenerGreyboxDirector::HandleHourActiveChanged(const bool bActive)' `
	'Dawn boundary'
$assertionCount++
$dawnBranch = [regex]::Match(
	$hourChangeBody,
	'if\s*\(!bActive\)\s*\{(?<body>[\s\S]*?)\n\t\}')
if (-not $dawnBranch.Success) {
	throw 'The dawn boundary branch could not be isolated.'
}
if (-not $dawnBranch.Groups['body'].Value.Contains('MakeNightThreeFirstReport();')) {
	throw 'The first report must be made on the dawn boundary (§24 즉시 차단 14).'
}

$reportBody = Get-MethodBody $greyboxSource `
	'void AIGListenerGreyboxDirector::MakeNightThreeFirstReport()' `
	'First report'
Assert-ContainsAll $reportBody @(
	'Narrative->GetNightIndex() != 3',
	'Narrative->WasFirstReportMade()',
	'Narrative->SetFirstReportMade(true);',
	'Night3.FirstReport'
) 'First report body'

# 신고 없이 도달하는 결말은 없다. 이것이 14번의 실질적 자물쇠다.
Assert-ContainsAll $finishEndingBody @(
	'Narrative->WasFirstReportMade()'
) 'Ending requires the first report'

# 막간은 밤4의 벽 안에서 돈다(2026-09-10). 벽이 열리고 오빠를 본 직후에
# 다섯 새벽을 살고, 눈을 뜨면 공동 너머의 노크와 목한수가 온다. 밤3의
# 잠자리는 보통 밤처럼 밤4로 간다. 막간이 도는 동안 05:30은 서 있는다.
$fifthDawnBody = Get-MethodBody $greyboxSource `
	'void AIGListenerGreyboxDirector::HandleFifthDawnCompleted()' `
	'Fifth-dawn completion'
Assert-ContainsAll $fifthDawnBody @(
	'NightFour->HandleInterludeCompleted();'
) 'Interlude hand-off'
Assert-ContainsAll $nightFourSource @(
	'WasFifthDawnInterludeCompleted()',
	'FifthDawnActor->StartInterlude(PlayerCharacter)',
	'It->SetHourPaused(true);',
	'void AIGMissingFloorNightFourDirector::HandleInterludeCompleted()'
) 'Interlude inside the wall'

$assertionCount++
if (-not $greyboxSource.Contains('FifthDawn->StartInterlude(Player.Get())')) {
	throw 'The probe must still exercise the interlude timeline.'
}


# --- 첫 실행 콘텐츠 고지 ----------------------------------------------------
#
# 2026 접근성 논의의 결론은 「일반 경고는 사용자가 아니라 책임을 보호한다」다.
# 그래서 이 화면은 점멸이 있다고만 말하지 않는다: 무엇이 나오는지, 3Hz 상한을
# 지켰다는 사실, 그리고 그것을 어느 설정으로 줄일 수 있는지를 같은 화면에서
# 말하고, 그 자리에서 F10으로 넘어갈 수 있다.
Assert-ContainsAll $controllerHeader @(
	'void ShowContentNoticeIfNeeded();',
	'void DismissContentNotice();',
	'ContentNotice'
) '콘텐츠 고지 진입점'

Assert-ContainsAll $controllerSource @(
	'ContentNoticeShown',
	'SetSystemMenuMode(EIGSystemMenuMode::ContentNotice)',
	'IGContentNoticePreview'
) '콘텐츠 고지 상태'

# 고지가 소리 맞추기보다 먼저다. 무엇이 나오는지 모른 채로 첫 화면을
# 지나가게 할 수 없다.
$noticeIndex = $controllerSource.IndexOf('ShowContentNoticeIfNeeded();')
$headphoneIndex = $controllerSource.IndexOf('StartHeadphoneRecommendationIfNeeded();')
if ($noticeIndex -lt 0 -or $headphoneIndex -lt 0 -or $noticeIndex -gt $headphoneIndex) {
	throw 'The content notice must come before the audio onboarding.'
}

# 화면이 실제로 세 가지를 말하는지. 하나라도 빠지면 일반 경고로 되돌아간다.
Assert-ContainsAll $hudSource @(
	'"NoticeSensory"',
	'"NoticeThemes"',
	'"NoticeControls"',
	'화면이나 소리가 불편하면 잠시 쉬거나 설정을 조절해 주세요.',
	'접근성 설정에서 카메라 흔들림과 빛 깜빡임을 줄일 수 있습니다.',
	'추격 없음'
) '콘텐츠 고지 본문'

# 그 자리에서 설정으로 갈 수 있어야 한다. 읽고 나서 찾아 헤매면 소용없다.
Assert-ContainsAll $hudSource @(
	'F10  접근성 설정 열기'
) '고지에서 설정으로'
$accessibilityToggle = Get-MethodBody $controllerSource `
	'void AIGPlayerController::ToggleAccessibilityMenu()' `
	'Accessibility toggle'
if (-not $accessibilityToggle.Contains('AccessibilityReturnMode = SystemMenuMode')) {
	throw 'Opening accessibility from the notice must return to the notice.'
}

Write-Host (
	'MISSING_FLOOR_RELEASE_GATE_CONTRACT PASS blockers=6,10,12,14 assertions={0}' -f `
		$assertionCount) -ForegroundColor Green
