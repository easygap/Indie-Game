[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

$projectRoot = Split-Path -Parent $PSScriptRoot
$evidenceHeaderPath = Join-Path $projectRoot 'Source/IndieGame/Validation/IGRebirthEvidenceSubsystem.h'
$evidenceSourcePath = Join-Path $projectRoot 'Source/IndieGame/Validation/IGRebirthEvidenceSubsystem.cpp'
$directorPath = Join-Path $projectRoot 'Source/IndieGame/Sequence/IGThirdMorningDirector.cpp'
$buildRulesPath = Join-Path $projectRoot 'Source/IndieGame/IndieGame.Build.cs'
$manualEvidencePath = Join-Path $projectRoot 'Docs/MANUAL_EVIDENCE.md'

$evidenceHeader = Get-Content -Raw -Encoding UTF8 $evidenceHeaderPath
$evidenceSource = Get-Content -Raw -Encoding UTF8 $evidenceSourcePath
$director = Get-Content -Raw -Encoding UTF8 $directorPath
$buildRules = Get-Content -Raw -Encoding UTF8 $buildRulesPath
$manualEvidence = Get-Content -Raw -Encoding UTF8 $manualEvidencePath
$assertionCount = 0

function Assert-True {
	param(
		[Parameter(Mandatory = $true)]
		[bool]$Condition,
		[Parameter(Mandatory = $true)]
		[string]$Message
	)

	if (-not $Condition) {
		throw "REBIRTH_EVIDENCE_CONTRACT FAIL: $Message"
	}
	$script:assertionCount++
}

function Assert-ContainsAll {
	param(
		[Parameter(Mandatory = $true)]
		[string]$Text,
		[Parameter(Mandatory = $true)]
		[string[]]$Needles,
		[Parameter(Mandatory = $true)]
		[string]$Context
	)

	foreach ($needle in $Needles) {
		Assert-True ($Text.Contains($needle)) "$Context 누락: $needle"
	}
}

Assert-ContainsAll $evidenceHeader @(
	'UIGRebirthEvidenceSubsystem',
	'RecordEndingEvent',
	'RecordPuzzleFourObservation',
	'bCaptureEnabled',
	'bAutoDumpOnEvent'
) '전용 런타임 계측 서브시스템'

Assert-ContainsAll $evidenceSource @(
	'ig.Rebirth.DumpEvidence',
	'IGRebirthEvidenceCapture',
	'IGRebirthEvidenceAutoDump',
	'IGRebirthEvidencePath=',
	'FConsoleCommandWithArgsDelegate::CreateUObject'
) '명령행·콘솔 진입점'

Assert-ContainsAll $evidenceSource @(
	'FPaths::ProjectSavedDir()',
	'TEXT("Validation")',
	'TEXT("RebirthManual")',
	'FPaths::IsRelative',
	'FPaths::ConvertRelativePathToFull',
	'RequestedPath.StartsWith'
) '출력 경로 제한'

$saveIndex = $evidenceSource.IndexOf('FFileHelper::SaveStringToFile')
$moveIndex = $evidenceSource.IndexOf('IFileManager::Get().Move')
Assert-True ($evidenceSource.Contains('const FString TemporaryPath = OutputPath + TEXT(".tmp")')) '동일 경로 임시 파일이 없다'
Assert-True ($saveIndex -ge 0 -and $moveIndex -gt $saveIndex) '임시 파일 저장 뒤 원자 교체 순서가 아니다'
Assert-True ($evidenceSource.Contains('ForceUTF8WithoutBOM')) 'UTF-8 인코딩이 고정되지 않았다'

Assert-ContainsAll $evidenceSource @(
	'Values.Sort()',
	'SortedTruths.Sort',
	'SortNames(Snapshot.VisitedLocations)',
	'SortNames(Snapshot.ResolvedPuzzles)',
	'SortNames(Snapshot.SkippedPuzzles)',
	'SortNames(Snapshot.ChapterThree.ObservedP5Sources)',
	'SortTags(Snapshot.NarrativeDebt)',
	'SortTags(LegacyTags)'
) '결정적 배열 정렬'
Assert-True (-not $evidenceSource.Contains('FDateTime')) '결정적 JSON에 실행 시각 의존성이 들어갔다'

Assert-ContainsAll $evidenceSource @(
	'EIGRebirthConvergencePoint::C1StorePurchase',
	'EIGRebirthConvergencePoint::C2FirstReturn',
	'EIGRebirthConvergencePoint::C3SecondMorning',
	'EIGRebirthConvergencePoint::C4RoofReached',
	'EIGRebirthConvergencePoint::C5FinalChoice',
	'EIGRebirthConvergencePoint::C6AfterDiscovery',
	'TEXT("narrativeDebt")'
) 'C1~C6·NarrativeDebt 덤프'

Assert-ContainsAll $evidenceSource @(
	'TEXT("purchaseProfile")',
	'TEXT("paymentMethod")',
	'TEXT("catWaterState")',
	'TEXT("bottleClosureState")',
	'TEXT("hasPaperCup")',
	'TEXT("waitedForCat")',
	'TEXT("hasMemoryFlashlight")',
	'TEXT("tankOpenedEarly")'
) '선택·소지품 상태 덤프'

foreach ($puzzleId in 1..5) {
	Assert-True ($evidenceSource.Contains("TEXT(`"P$puzzleId`")")) "P$puzzleId 상태가 JSON에 없다"
}
Assert-ContainsAll $evidenceSource @(
	'const FName PuzzleP1(TEXT("P1"))',
	'P1.AlarmArithmeticProxy',
	'const FName PuzzleP2(TEXT("P2"))',
	'P2.ReceiptComparisonProxy',
	'TEXT("directInletClosed")',
	'TEXT("pressureKPa")',
	'TEXT("downwardLoopCount")',
	'TEXT("upwardRouteRevealed")',
	'TEXT("focusedEvidence")',
	'TEXT("observedSources")',
	'TEXT("observed")',
	'TEXT("confirmed")',
	'TEXT("explicitlySkipped")',
	'TEXT("routeSkipped")',
	'TEXT("catSafeConfirmed")',
	'TEXT("hoseCauseConfirmed")',
	'TEXT("fallConfirmed")',
	'TEXT("identityConfirmed")'
) 'P1~P5 관찰·확정 상태'

Assert-ContainsAll $evidenceSource @(
	'TEXT("truths")',
	'TEXT("tag")',
	'TEXT("confirmed")',
	'TEXT("sources")',
	'TEXT("visitedLocations")',
	'TEXT("resolvedPuzzles")',
	'TEXT("skippedPuzzles")',
	'TEXT("equippedOutfitChapters")',
	'TEXT("playedOneShotBeats")',
	'TEXT("legacyStoryTags")'
) '진실 출처·히스토리 상태'

$expectedEvents = @(
	@('EndingChoiceEvent', 'Choice'),
	@('ActualStateRestoredEvent', 'ActualStateRestored'),
	@('Found0731Event', 'Found0731'),
	@('CommonDiscoveryCardEvent', 'CommonDiscoveryCard'),
	@('BranchCodaEvent', 'BranchCoda')
)
$lastEventIndex = -1
foreach ($expectedEvent in $expectedEvents) {
	$constantName = $expectedEvent[0]
	$eventName = $expectedEvent[1]
	$eventIndex = $evidenceSource.IndexOf("const FName $constantName")
	Assert-True ($eventIndex -gt $lastEventIndex) "S4 예상 이벤트 선언 순서가 잘못됐다: $eventName"
	$lastEventIndex = $eventIndex
}
Assert-ContainsAll $evidenceSource @(
	'TEXT("expectedTimeline")',
	'TEXT("timeline")',
	'TEXT("sequence")',
	'TEXT("timelineCompleteInOrder")',
	'TEXT("timelineDuplicateFree")'
) '구조화 엔딩 타임라인'

$endingAStart = $director.IndexOf('void AIGThirdMorningDirector::FinishEndingA()')
$endingBStart = $director.IndexOf('void AIGThirdMorningDirector::BeginEndingB()')
$commonStart = $director.IndexOf('void AIGThirdMorningDirector::ShowCommonDiscoveryCard()')
$endingACodaStart = $director.IndexOf('void AIGThirdMorningDirector::FinishEndingAAfterDiscovery()')
$endingBCodaStart = $director.IndexOf('void AIGThirdMorningDirector::FinishEndingB()')
$endingBMontageStart = $director.IndexOf('void AIGThirdMorningDirector::StartEndingBMontage()')
$endingBEpilogueStart = $director.IndexOf('void AIGThirdMorningDirector::StartEndingBEpilogue()')
Assert-True ($endingAStart -ge 0 -and $endingBStart -gt $endingAStart) '엔딩 A/B 함수 경계를 찾지 못했다'
Assert-True (
	$commonStart -gt $endingBStart -and
	$endingACodaStart -gt $commonStart) '실제 상태·공통 발견 함수 경계를 찾지 못했다'
Assert-True (
	$endingBCodaStart -gt $endingACodaStart -and
	$endingBMontageStart -gt $endingBCodaStart -and
	$endingBEpilogueStart -gt $endingBMontageStart) '분기 코다 함수 경계를 찾지 못했다'

$endingASegment = $director.Substring($endingAStart, $endingBStart - $endingAStart)
$endingBSegment = $director.Substring($endingBStart, $commonStart - $endingBStart)
$commonSegment = $director.Substring($commonStart, $endingACodaStart - $commonStart)
$endingACodaSegment = $director.Substring($endingACodaStart, $endingBCodaStart - $endingACodaStart)
$endingBMontageSegment = $director.Substring(
	$endingBMontageStart,
	$endingBEpilogueStart - $endingBMontageStart)
Assert-True ($endingASegment.Contains('FName(TEXT("Choice"))')) '엔딩 A 선택 계측이 없다'
Assert-True ($endingBSegment.Contains('FName(TEXT("Choice"))')) '엔딩 B 선택 계측이 없다'
Assert-True (
	$endingASegment.Contains('ApplyCommonDiscoveryWorldState();') -and
	$endingASegment.Contains('FName(TEXT("ActualStateRestored"))') -and
	$endingBSegment.Contains('ApplyCommonDiscoveryWorldState();') -and
	$endingBSegment.Contains('FName(TEXT("ActualStateRestored"))')) '양 엔딩의 실제 상태 복원 계측이 없다'

$foundIndex = $commonSegment.IndexOf('FName(TEXT("Found0731"))')
$cardIndex = $commonSegment.IndexOf('FName(TEXT("CommonDiscoveryCard"))')
Assert-True ($foundIndex -ge 0 -and $cardIndex -gt $foundIndex) '발견·공통 카드 계측 순서가 S4 계약과 다르다'
Assert-True ($endingACodaSegment.Contains('FName(TEXT("BranchCoda"))')) '엔딩 A 분기 코다 계측이 없다'
Assert-True ($endingBMontageSegment.Contains('FName(TEXT("BranchCoda"))')) '엔딩 B 분기 코다 계측이 없다'

Assert-ContainsAll $director @(
	'RecordPuzzleFourObservation',
	'StairLoopCount >= 3',
	'HandleFifthFloorEntered'
) 'P4 런타임 관찰 계측'

Assert-True ($evidenceSource.Contains('if (!bCaptureEnabled || EventId.IsNone())')) '비활성 엔딩 계측 차단이 없다'
Assert-True ($evidenceSource.Contains('if (!bCaptureEnabled)')) '비활성 P4 계측 차단이 없다'
Assert-True ($evidenceSource.Contains('if (!bAutoDumpOnEvent)')) '자동 파일 쓰기 opt-in 차단이 없다'
$initializeStart = $evidenceSource.IndexOf('void UIGRebirthEvidenceSubsystem::Initialize(')
$deinitializeStart = $evidenceSource.IndexOf('void UIGRebirthEvidenceSubsystem::Deinitialize()')
$initializeSegment = $evidenceSource.Substring($initializeStart, $deinitializeStart - $initializeStart)
Assert-True (-not $initializeSegment.Contains('DumpEvidence(')) '초기화만으로 디스크 덤프가 실행된다'
Assert-True ($buildRules.Contains('"Json"')) 'Json 런타임 모듈 의존성이 없다'

Assert-ContainsAll $manualEvidence @(
	'IGRebirthEvidenceCapture',
	'IGRebirthEvidenceAutoDump',
	'ig.Rebirth.DumpEvidence',
	'Saved/Validation/RebirthManual',
	'Choice',
	'ActualStateRestored',
	'Found0731',
	'CommonDiscoveryCard',
	'BranchCoda',
	'Test-Rebirth-EvidenceContract.ps1'
) '수동 증거 사용 문서'

Write-Host "REBIRTH_EVIDENCE_CONTRACT PASS ($assertionCount assertions)"
