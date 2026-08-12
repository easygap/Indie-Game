[CmdletBinding()]
param()

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$projectRoot = Split-Path -Parent $PSScriptRoot

function Read-ProjectText {
	param([Parameter(Mandatory = $true)][string]$RelativePath)
	$path = Join-Path $projectRoot $RelativePath
	if (-not (Test-Path -LiteralPath $path -PathType Leaf)) {
		throw "Missing release-ending contract file: $RelativePath"
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
		if (-not $Text.Contains($token)) {
			throw "$Label contract is missing: $token"
		}
	}
}

$story = Read-ProjectText 'Docs/STORY_BIBLE_MISSING_FLOOR.md'
$fifthDawnHeader = Read-ProjectText `
	'Source/IndieGame/Entity/IGMissingFloorFifthDawnDirector.h'
$fifthDawnSource = Read-ProjectText `
	'Source/IndieGame/Entity/IGMissingFloorFifthDawnDirector.cpp'
$controllerSource = Read-ProjectText 'Source/IndieGame/Player/IGPlayerController.cpp'
$characterSource = Read-ProjectText 'Source/IndieGame/Player/IGPlayerCharacter.cpp'
$hudHeader = Read-ProjectText 'Source/IndieGame/Player/IGHorrorHUD.h'
$hudSource = Read-ProjectText 'Source/IndieGame/Player/IGHorrorHUD.cpp'
$nightFourHeader = Read-ProjectText `
	'Source/IndieGame/Entity/IGMissingFloorNightFourDirector.h'
$nightFourSource = Read-ProjectText `
	'Source/IndieGame/Entity/IGMissingFloorNightFourDirector.cpp'
$nightLoopSource = Read-ProjectText `
	'Source/IndieGame/Entity/IGNightLoopDirector.cpp'
$nightPhaseSource = Read-ProjectText `
	'Source/IndieGame/Entity/IGNightPhaseDirector.cpp'
$narrativeSource = Read-ProjectText `
	'Source/IndieGame/Narrative/IGMissingFloorNarrativeSubsystem.cpp'
$worldSource = Read-ProjectText 'Source/IndieGame/Core/IGPrologueWorldScene.cpp'
$toneSource = Read-ProjectText 'Source/IndieGame/Audio/IGToneSequenceSoundWave.cpp'
$greyboxSource = Read-ProjectText `
	'Source/IndieGame/Entity/IGListenerGreyboxDirector.cpp'

Assert-ContainsAll $story @(
	'재플레이에서는 2초 홀드로 건너뛸 수 있다.',
	'### 엔딩 C 「매물」 — 실패 엔딩 (밤4 한정)',
	'무영로 달빛빌라 403호',
	'같아요.**"',
	'밤 4를 다시 시작할 수 있다'
) 'Release-ending story'

Assert-ContainsAll $fifthDawnHeader @(
	'BeginReplaySkipInput()',
	'EndReplaySkipInput()',
	'IsReplaySkipAvailable()',
	'GetReplaySkipDurationSeconds()',
	'bReplaySkipRewinding'
) 'Fifth-dawn replay header'
Assert-ContainsAll $fifthDawnSource @(
	'ReplaySkipDurationSeconds = 2.0f',
	'FifthDawnExperienced',
	'GetHoldDurationScale()',
	'UsesToggleHoldInteractions()',
	'GConfig->Flush(false, GGameUserSettingsIni)',
	'FinishInterlude(/*bPersistExperience=*/false)',
	'SetActorTickEnabled(false)'
) 'Fifth-dawn replay runtime'
Assert-ContainsAll $controllerSource @(
	'It->BeginReplaySkipInput()',
	'It->EndReplaySkipInput()'
) 'Fifth-dawn input routing'
Assert-ContainsAll $hudHeader @(
	'SetSensoryInterludeSkipState(',
	'BeginMissingFloorFailureEnding(float InitialElapsedSeconds = 0.0f)',
	'SetMissingFloorFailureRetryEnabled'
) 'Release HUD header'
Assert-ContainsAll $hudSource @(
	'DrawSensoryInterludeSkip()',
	'DrawMissingFloorFailureEnding(CurrentTime)',
	'IsReducedCameraMotionEnabled()',
	'GetCaptionSizeScale()',
	'"무영로 달빛빌라 403호"',
	'"채광 좋은 남향, 즉시 입주 가능"',
	'"이 집 새벽에 노크 소리 나요."',
	'"두 명이서 하는 것 같아요."',
	'"E  밤 4를 다시 시작할 수 있다"',
	'"A  밤 4를 다시 시작할 수 있다"',
	'RecordLayoutValidationRect(PanelPosition, PanelPosition + PanelSize)'
) 'Ending C presentation'

Assert-ContainsAll $nightFourHeader @(
	'ResolveFailureEnding()',
	'RequestFailureRetry()',
	'CompleteFailurePresentationForProbe()',
	'IsFailureRetryEnabled()'
) 'Ending C director header'
Assert-ContainsAll $nightFourSource @(
	'FailureCaptureSeconds = 1.2f',
	'FailureListingDelaySeconds = 1.24f',
	'FailureRetryDelaySeconds = 7.2f',
	'AudioDirector->SetAuthoredSilence(false)',
	'AudioDirector->SetThreatState(EIGAudioThreatState::Calm)',
	'It->SuspendForFailureEnding()',
	'Noise->UnregisterHumSource(WaterMaskHumHandle)',
	'IGAudio::SpawnOneShotAt(',
	'CreateWallpaperSeamRoller(this)',
	'Narrative->ResetNightFourForRetry()',
	'SceneActor->ResetMissingFloorCavity()',
	'It->RestorePlayerAtWakePoint(Character)',
	'It->RestartTheHour(4)',
	'if (bFailureEndingActive)',
	'if (bNightFour && !bWallOpened)'
) 'Ending C director runtime'

$failureMethod = [regex]::Match(
	$nightFourSource,
	'bool AIGMissingFloorNightFourDirector::ResolveFailureEnding\(\)(?<body>[\s\S]*?)void AIGMissingFloorNightFourDirector::BeginFailureListing\(\)')
if (-not $failureMethod.Success) {
	throw 'Ending C failure method could not be isolated.'
}
if ($failureMethod.Groups['body'].Value.Contains('OnResolved.Broadcast()')) {
	throw 'Ending C must not release dawn through the successful-ending delegate.'
}

Assert-ContainsAll $nightLoopSource @(
	'Narrative->GetNightIndex() == 4',
	'Narrative->GetAggressionTier() >= 3',
	'Narrative->IsNightFourMaskRunning()',
	'RestorePlayerAtWakePoint('
) 'Capture ownership'
Assert-ContainsAll $nightPhaseSource @(
	'SuspendForFailureEnding()',
	'GetWorldTimerManager().ClearTimer(HourTimer)',
	'RestartTheHour(const int32 NightIndex)',
	'if (!bHourActive || bFailureEndingSuspended)'
) 'Hour suspension'
Assert-ContainsAll $narrativeSource @(
	'ResetNightFourForRetry()',
	'Snapshot.Night.NightFourControlOrder.Reset()',
	'Snapshot.Night.EndingChoice = NAME_None',
	'Snapshot.Night.SolvedPuzzles.Remove(FName(TEXT("P5")))',
	'Snapshot.Night.CompletedBeats.Remove(FName(TEXT("Night4.SecondReport")))'
) 'Scoped narrative rollback'
$rollbackMethod = [regex]::Match(
	$narrativeSource,
	'void UIGMissingFloorNarrativeSubsystem::ResetNightFourForRetry\(\)(?<body>[\s\S]*?)// -- persistence')
if (-not $rollbackMethod.Success) {
	throw 'Night-four rollback method could not be isolated.'
}
foreach ($durableToken in @('CaptureCount =', 'Truths.Reset', 'bFirstReportMade = false',
	'bFifthDawnInterludeCompleted = false')) {
	if ($rollbackMethod.Groups['body'].Value.Contains($durableToken)) {
		throw "Scoped retry incorrectly clears durable state: $durableToken"
	}
}
Assert-ContainsAll $worldSource @(
	'ResetMissingFloorCavity()',
	'SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics)',
	'bMissingFloorCavityOpen = false'
) 'Cavity rollback'
Assert-ContainsAll $toneSource @(
	'CreateWallpaperSeamRoller(',
	'기계나 프린터처럼 규칙적으로 들리지 않도록 두 번의 길이를 다르게 둔다.',
	'ConfigureNotes(MoveTemp(RollerNotes), false)'
) 'Wallpaper roller audio'
Assert-ContainsAll $characterSource @(
	'It->RequestFailureRetry()'
) 'Ending C retry input'
Assert-ContainsAll $greyboxSource @(
	'EProbeStep::NightFourFailureRetryContract',
	'NightFour->ResolveFailureEnding()',
	'NightPhase->IsFailureEndingSuspended()',
	'NightFour->CompleteFailurePresentationForProbe()',
	'Narrative->GetCaptureCount() == FailureRetryCaptureCountBefore + 1',
	'Narrative->WasFirstReportMade()',
	'Narrative->HasTruth(EIGMissingFloorTruth::WaitingForAnAnswer)',
	'!Scene->IsMissingFloorCavityOpen()'
) 'Ending C runtime probe'

Write-Host 'MISSING_FLOOR_RELEASE_ENDING_CONTRACT PASS replay_skip=1 ending_c=1 scoped_retry=1 audio=1 runtime_probe=1'
