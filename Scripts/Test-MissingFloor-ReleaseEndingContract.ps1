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

# -- v3.4 §35 엔딩 에필로그 -------------------------------------------------

$epilogueHeader = Read-ProjectText `
	'Source/IndieGame/Entity/IGMissingFloorEpilogueDirector.h'
$epilogueSource = Read-ProjectText `
	'Source/IndieGame/Entity/IGMissingFloorEpilogueDirector.cpp'
$narrativeTypes = Read-ProjectText `
	'Source/IndieGame/Narrative/IGMissingFloorNarrativeTypes.h'
$puzzleTwoSource = Read-ProjectText `
	'Source/IndieGame/Entity/IGMissingFloorPuzzleTwoDirector.cpp'

Assert-ContainsAll $story @(
	'## 35. v3.4 — 엔딩 에필로그와 선택적 목격',
	'몽타주 네 소리는 두 엔딩에서 같은 시각에 같은 순서로 난다',
	'마지막 카드는 최소 8초를 잡는다',
	'판은 원본 비례로 그린다',
	'나린의 제보는 목격과 무관하게 보도에 남는다'
) '에필로그 정사'

# 시각표는 소스가 정사다. 두 배열의 앞 다섯 값이 §9 「공통 사실」이며,
# ValidateTimelines가 단조 증가와 카드 체류를 실행 시각에 다시 확인한다.
Assert-ContainsAll $epilogueSource @(
	'constexpr float EndingATimes[]',
	'constexpr float EndingBTimes[]',
	'87.00f',
	'71.00f',
	'EIGMissingFloorEpilogueScene::Montage',
	'EIGMissingFloorEpilogueScene::Workshop',
	'EIGMissingFloorEpilogueScene::Autumn',
	'EIGMissingFloorEpilogueScene::ServiceBay',
	'EIGMissingFloorEpilogueScene::News',
	'EIGMissingFloorEpilogueScene::Card',
	'들어 주는 일에는 노크 두 번이면 충분했다.',
	'없는 층은 비었지만, 대답은 남았다.',
	'업라이트 1대 — 의뢰인: 백유담 (동생 집들이 선물)',
	'그 뒤로 이 건물의 새벽은 조용하다.',
	'인근 편의점 야간 근무자의 제보가 최초 시각 특정에 쓰였다'
) '에필로그 타임라인과 카피'

Assert-ContainsAll $epilogueHeader @(
	'static bool ValidateTimelines();',
	'bool CompleteImmediatelyForProbe();',
	'int32 GetPlayedSceneCount() const'
) '에필로그 검증 훅'

# 엔딩 C는 자기 화면과 재도전을 소유한다. 실패가 애도로 이어지면 §9의
# 세 결말이 섞이므로 StartEpilogue는 A/B만 받는다.
Assert-ContainsAll $epilogueSource @(
	'if (EndingId != IGEpilogue::EndingAId && EndingId != IGEpilogue::EndingBId)'
) '엔딩 C 분리'

Assert-ContainsAll $toneSource @(
	'CreatePoliceLineTapePull(',
	'CreateGurneyWheels(',
	'CreateCameraShutterTriple(',
	'CreateDebrisSweep(',
	'CreateEpilogueWorkshopScore(',
	'CreateEpilogueAutumnBed(',
	'CreateKeyDropMetalBox(',
	'CreateRailingKnockTwo('
) '에필로그 합성'

# §10.1: 게임 내내 -30센트에 머물던 모티프가 여기서만 0센트에 닿는다.
Assert-ContainsAll $toneSource @(
	'const float Cents = FMath::Lerp(-30.0f, 0.0f, Progress);'
) '정음 도달'

Assert-ContainsAll $hudHeader @(
	'enum class EIGMissingFloorEpilogueScene : uint8',
	'void BeginMissingFloorEpilogueScene(',
	'void EndMissingFloorEpilogue();',
	'bool IsMissingFloorEpilogueVisible() const'
) '에필로그 HUD 계층'

Assert-ContainsAll $hudSource @(
	'bool AIGHorrorHUD::DrawMissingFloorEpilogue(const double CurrentTime)',
	'const float SourceAspect = SourceWidth / SourceHeight;',
	'T_EpilogueWorkshop_D',
	'T_EpilogueAutumn_D',
	'T_EpilogueServiceBay_D'
) '에필로그 HUD 렌더'

# 텍스처 없이도 장면이 성립해야 한다. 에필로그의 뜻은 문장에 있다.
if (-not $hudSource.Contains('if (SceneTexture && SceneTexture->GetResource())')) {
	throw 'Epilogue still must degrade to text when the texture is absent.'
}

# -- v3.4 §22.3 선택적 목격 -------------------------------------------------

Assert-ContainsAll $narrativeTypes @(
	'enum class EIGMissingFloorWitness : uint8',
	'SeoSleepingPills = 1',
	'HwangWaterBowl = 2',
	'BoothSoundproofing = 3',
	'RooftopCigarettePack = 4',
	'TArray<FName> Witnesses;'
) '선택적 목격 정의'

Assert-ContainsAll $narrativeSource @(
	'Seen.SeoSleepingPills',
	'Seen.HwangWaterBowl',
	'Seen.BoothSoundproofing',
	'Seen.RooftopCigarettePack'
) '선택적 목격 직렬화'

# 목격은 어떤 교차에도 들어가지 않는다. RecordWitness가 진실을 다시
# 계산하면 「본 것이 진실을 열 수도 있다」가 코드에 남는다.
$recordWitness = [regex]::Match(
	$narrativeSource,
	'bool UIGMissingFloorNarrativeSubsystem::RecordWitness\((?<body>[\s\S]*?)\r?\n\}')
if (-not $recordWitness.Success) {
	throw 'RecordWitness implementation not found.'
}
# 주석은 그 금지를 설명하는 자리이므로 걷어 내고 실행문만 본다.
$recordWitnessCode = [regex]::Replace(
	$recordWitness.Groups['body'].Value, '//[^\r\n]*', '')
if ($recordWitnessCode.Contains('RecomputeConfirmations')) {
	throw 'RecordWitness must not touch truth confirmation.'
}

Assert-ContainsAll $epilogueSource @(
	'HasWitness(EIGMissingFloorWitness::HwangWaterBowl)',
	'HasWitness(EIGMissingFloorWitness::BoothSoundproofing)',
	'HasWitness(EIGMissingFloorWitness::SeoSleepingPills)',
	'HasWitness(EIGMissingFloorWitness::RooftopCigarettePack)'
) '목격에 따른 문장 분기'

Assert-ContainsAll $greyboxSource @(
	'SpawnOptionalWitnesses(CubeMesh);',
	'RecordWitness(EIGMissingFloorWitness::HwangWaterBowl)',
	'RecordWitness(EIGMissingFloorWitness::SeoSleepingPills)',
	'RecordWitness(EIGMissingFloorWitness::RooftopCigarettePack)',
	'Epilogue->StartEpilogue(Player.Get(), Narrative->GetEndingChoice());',
	'IGController->ShowTitleAfterEnding();'
) '목격 프롭과 에필로그 진입'

Assert-ContainsAll $puzzleTwoSource @(
	'RecordWitness(EIGMissingFloorWitness::BoothSoundproofing)'
) '문틈 목격'

# §13: 심는 자리가 없으면 엔딩 A의 제보 자막은 어디서 왔는지 알 수 없는
# 문장이 된다. 나린의 마지막 날 대사가 그 심기다.
Assert-ContainsAll $greyboxSource @(
	'FText AIGListenerGreyboxDirector::GetNarinCounterLine() const',
	'…안 물어볼게요. 대신 저 새벽에 여기 있어요. 뭐 들리면 적어 둘게요.',
	'ArrivalStoreBell->SetInteractionEnabled(!bActive);'
) '나린 포어섀도'

Assert-ContainsAll $greyboxSource @(
	'EProbeStep::EpilogueContract',
	'Epilogue->GetPlayedSceneCount() != 5',
	'Epilogue->CompleteImmediatelyForProbe()',
	'Hud->IsMissingFloorEpilogueVisible()',
	'Narrative->GetConfirmedTruthCount() != TruthsBefore',
	'Seen.NotAThingThisBuildKnows'
) '에필로그 런타임 프로브'

# 어느 목격도 진행을 잠그지 않는다. 스테이지 유효성 검사가 이것들을
# 묻지 않는 것이 「없어도 되는 것」이라는 설계의 표현이다.
foreach ($optional in @('WaterBowl', 'SleepingPills', 'CigarettePack')) {
	if ($greyboxSource -match "ValidateFixtures[\s\S]{0,4000}$optional") {
		throw "Optional witness must not gate stage validation: $optional"
	}
}

Write-Host 'MISSING_FLOOR_RELEASE_ENDING_CONTRACT PASS replay_skip=1 ending_c=1 scoped_retry=1 audio=1 runtime_probe=1 epilogue=2 witnesses=4'
