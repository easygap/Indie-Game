[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'
$projectRoot = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path

function Read-ProjectText {
	param([Parameter(Mandatory)][string]$RelativePath)

	$path = Join-Path $projectRoot $RelativePath
	if (-not (Test-Path -LiteralPath $path -PathType Leaf)) {
		throw "Required CH02 time-entry file is missing: $RelativePath"
	}
	return Get-Content -Raw -Encoding UTF8 -LiteralPath $path
}

function Assert-Contract {
	param(
		[Parameter(Mandatory)][bool]$Condition,
		[Parameter(Mandatory)][string]$Message
	)

	if (-not $Condition) {
		throw "REBIRTH CH02 time-entry contract failed: $Message"
	}
}

function Get-BlockBetween {
	param(
		[Parameter(Mandatory)][string]$Text,
		[Parameter(Mandatory)][string]$StartMarker,
		[Parameter(Mandatory)][string]$EndMarker
	)

	$start = $Text.IndexOf($StartMarker)
	$end = $Text.IndexOf($EndMarker, $start + $StartMarker.Length)
	Assert-Contract ($start -ge 0 -and $end -gt $start) `
		"Could not isolate '$StartMarker'."
	return $Text.Substring($start, $end - $start)
}

$tags = Read-ProjectText 'Config/DefaultGameplayTags.ini'
$gameConfig = Read-ProjectText 'Config/DefaultGame.ini'
$puzzleHeader =
	Read-ProjectText 'Source/IndieGame/Interaction/IGTimeEntryPuzzle.h'
$puzzleSource =
	Read-ProjectText 'Source/IndieGame/Interaction/IGTimeEntryPuzzle.cpp'
$readableHeader =
	Read-ProjectText 'Source/IndieGame/Interaction/IGReadableNote.h'
$readableSource =
	Read-ProjectText 'Source/IndieGame/Interaction/IGReadableNote.cpp'
$hud =
	Read-ProjectText 'Source/IndieGame/Player/IGHorrorHUD.cpp'
$director =
	Read-ProjectText 'Source/IndieGame/Sequence/IGSecondMorningDirector.cpp'
$humanGate =
	Read-ProjectText 'Source/IndieGame/Sequence/IGChapterTwoHumanGateDirector.cpp'
$worldScene =
	Read-ProjectText 'Source/IndieGame/Core/IGPrologueWorldScene.cpp'
$evidence =
	Read-ProjectText 'Source/IndieGame/Validation/IGRebirthEvidenceSubsystem.cpp'
$persistenceProbe =
	Read-ProjectText 'Source/IndieGame/Sequence/IGRebirthPersistenceProbe.cpp'
$persistenceHarness =
	Read-ProjectText 'Scripts/run_rebirth_savegame_roundtrips.py'
$materialAuthoring =
	Read-ProjectText 'Scripts/create_prototype_materials.py'

# 선택값과 오답 단계는 기존 StoryState 스냅샷에 태그로 저장한다.
foreach ($tag in @(
	'State.CH02.P1.Hour05',
	'State.CH02.P1.Minute10',
	'State.CH02.P1.Minute31',
	'State.CH02.P1.Pressure1',
	'State.CH02.P1.Pressure2',
	'State.CH02.P1.Pressure3',
	'State.CH02.P2.Hour05',
	'State.CH02.P2.Minute10',
	'State.CH02.P2.Minute31',
	'State.CH02.P2.Pressure1',
	'State.CH02.P2.Pressure2',
	'State.CH02.P2.Pressure3',
	'State.CH02.P2.SourceApprovalRead'
)) {
	Assert-Contract ($tags.Contains("Tag=`"$tag`"")) `
		"Gameplay tag '$tag' is missing."
}

foreach ($invariant in @(
	'AIGTimeEntryButton',
	'EIGTimeEntryButtonAction::Hour',
	'EIGTimeEntryButtonAction::Minute',
	'EIGTimeEntryButtonAction::Confirm',
	'CompleteInteraction_Implementation',
	'RunAutomatedSolution',
	'HasPhysicalContract',
	'GetPhysicalMeshComponentCount'
)) {
	Assert-Contract (
		$puzzleHeader.Contains($invariant) -or
		$puzzleSource.Contains($invariant)
	) "Physical input contract is missing '$invariant'."
}
foreach ($invariant in @(
	'DisplaySegmentTransforms.Num() == 28',
	'DisplayOnInstances->GetInstanceCount()',
	'DisplayOffInstances->GetInstanceCount() == 30',
	'PosKeypadInstances->GetInstanceCount() == 12',
	'LiveButtons == 3',
	'LivePresentationParts >= RequiredPresentationParts',
	'bUsesAuthoredHousing',
	'UsesLayeredDisplay()',
	'HourCandidates = {4, 5}',
	'MinuteCandidates = {44, 10, 31}',
	'? 5 : 4',
	'? 10 : 31',
	'FMath::Min(WrongAttempts + 1, 3)'
)) {
	Assert-Contract ($puzzleSource.Contains($invariant)) `
		"Time-entry implementation is missing '$invariant'."
}
Assert-Contract (-not $puzzleSource.Contains('LoadObject<UStaticMesh>')) `
	'퍼즐 상호작용 중 정적 메시를 동기 로드하면 안 된다.'

# P1과 P2는 같은 입력 규칙을 공유하되 화면에서 즉시 다른 생활 물건으로
# 읽혀야 한다. P1은 직접 제작 알람 메시, P2는 원본 PBR 슬롯을 보존한
# Poly Haven POS 스캔을 쓰고 각각 고유 실루엣 부품을 더한다.
foreach ($asset in @(
	'Content/Meshes/SM_AlarmClock.uasset',
	'Content/Prototype/Materials/M_Glass.uasset',
	'Content/Prototype/Materials/M_Alarm.uasset',
	'Content/Prototype/Materials/M_ScreenGlow.uasset',
	'Content/Photo/Props/CashRegister_01/CashRegister_01_1k/StaticMeshes/CashRegister_01_body.uasset',
	'Content/Photo/Props/CashRegister_01/CashRegister_01_1k/Materials/CashRegister_01.uasset',
	'Content/Photo/Props/CashRegister_01/CashRegister_01_1k/Textures/CashRegister_01_diff.uasset',
	'Content/Photo/Props/CashRegister_01/CashRegister_01_1k/Textures/CashRegister_01_metallic-CashRegister_01_roughness.uasset',
	'Content/Photo/Props/CashRegister_01/CashRegister_01_1k/Textures/CashRegister_01_nor_gl.uasset'
)) {
	$assetPath = Join-Path $projectRoot $asset
	Assert-Contract (
		(Test-Path -LiteralPath $assetPath -PathType Leaf) -and
		(Get-Item -LiteralPath $assetPath).Length -gt 1024
	) "Authored terminal asset is missing or empty: $asset"
}
foreach ($cookRoot in @(
	'+DirectoriesToAlwaysCook=(Path="/Game/Photo/Props")',
	'+DirectoriesToAlwaysCook=(Path="/Game/Meshes")'
)) {
	Assert-Contract ($gameConfig.Contains($cookRoot)) `
		"Packaged builds do not retain terminal asset root '$cookRoot'."
}
foreach ($invariant in @(
	'BuildAlarmHousing',
	'BuildPosHousing',
	'AddAuthoredPresentationMesh',
	'AddInstancedPart',
	'DisplayOnSegments',
	'DisplayOffSegments',
	'PosKeypadKeys',
	'REBIRTH.CH02.P1.AlarmHousing',
	'REBIRTH.CH02.P2.PosHousing',
	'AlarmSnoozeBar',
	'AlarmLeftFoot',
	'AlarmRightFoot',
	'PosCashDrawer',
	'PosDrawerHandle',
	'PosPrinterSlot',
	'Part->SetCastShadow(true)',
	'if (MaterialOverride)',
	'알람 시 조정',
	'알람 분 조정',
	'알람 시각 확인',
	'원거래 시 조정',
	'원거래 분 조정',
	'원거래 시각 복원'
)) {
	Assert-Contract (
		$puzzleHeader.Contains($invariant) -or
		$puzzleSource.Contains($invariant)
	) "Distinct P1/P2 presentation is missing '$invariant'."
}
foreach ($invariant in @(
	'PropMesh(TEXT("SM_AlarmClock"))',
	'StoreCashRegisterVisual = Fixture(TEXT("SM_RetailPOS")',
	'StoreCashRegisterVisual->GetStaticMesh()',
	'StoreCashRegisterVisual->SetVisibility(false, true)',
	'StoreCashRegisterVisual->SetHiddenInGame(true, true)',
	'InAlarmHousingMesh',
	'InPosHousingMesh'
)) {
	Assert-Contract (
		$worldScene.Contains($invariant) -or
		$director.Contains($invariant)
	) "Preloaded terminal wiring is missing '$invariant'."
}
foreach ($invariant in @(
	'PreferredObjectPath',
	'/Game/Photo/Props/%s/%s_1k/StaticMeshes/%s.%s',
	'LoadObject<UStaticMesh>',
	'LOAD_NoWarn'
)) {
	Assert-Contract ($worldScene.Contains($invariant)) `
		"First-run photo-prop preload contract is missing '$invariant'."
}
$chapterTwoTerminalBlock = Get-BlockBetween $worldScene `
	'if (SecondMorningDirector)' `
	'if (PlayerController)'
Assert-Contract (-not $chapterTwoTerminalBlock.Contains(
	'FindPhotoPropMesh(TEXT("CashRegister_01"))')) `
	'CH02 전환 중 계산대 에셋 레지스트리를 다시 조회하면 안 된다.'
Assert-Contract ($puzzleSource.Contains("`t`tPosMesh,`r`n`t`tnullptr,") -or `
	$puzzleSource.Contains("`t`tPosMesh,`n`t`tnullptr,")) `
	'The POS scan no longer preserves its imported PBR material slots.'

$glassMaterialBlock = Get-BlockBetween $materialAuthoring `
	'"M_Glass": {' `
	'"M_MetalFrame": {'
Assert-Contract (
	$glassMaterialBlock.Contains('"roughness": 0.04') -and
	$glassMaterialBlock.Contains('"opacity": 0.10')
) 'The terminal lens no longer uses the low-roughness clear-glass contract.'
$alarmMaterialBlock = Get-BlockBetween $materialAuthoring `
	'"M_Alarm": {' `
	'"M_FridgeBody": {'
Assert-Contract ($alarmMaterialBlock.Contains(
	'"emissive": (0.35, 0.002, 0.001, 1.0)')) `
	'The P1 alarm display no longer has its red emissive response.'
$posGlowBlock = Get-BlockBetween $materialAuthoring `
	'"M_ScreenGlow": {' `
	'"M_WaterBlue": {'
Assert-Contract ($posGlowBlock.Contains(
	'"emissive": (0.15, 0.5, 0.7, 1.0)')) `
	'The P2 POS display no longer has its blue emissive response.'
foreach ($invariant in @(
	'FVector(0.015f, 0.07f, 0.035f)',
	'FVector(3.1f, -10.0f, -9.0f)',
	'FVector(3.1f, 0.0f, -9.0f)',
	'FVector(3.1f, 10.0f, -9.0f)',
	'constexpr float PosUniformScale = 0.96f',
	'PosMesh->GetBounds().BoxExtent * PosUniformScale',
	'2.0f - PosRotatedExtent.X',
	'19.0f + Column * 4.0f'
)) {
	Assert-Contract ($puzzleSource.Contains($invariant)) `
		"Terminal fit contract is missing '$invariant'."
}

foreach ($invariant in @(
	'DisplayBacking = AddPart(',
	'TEXT("DisplayBacking")',
	'FVector(2.12f, 0.0f, 2.0f)',
	'FVector(0.20f, 31.0f, 13.5f)',
	'DisplayLens = AddPart(',
	'TEXT("DisplayLens")',
	'FVector(2.45f, 0.0f, 2.0f)',
	'FVector(0.08f, 31.0f, 13.5f)',
	'DisplayLens->SetTranslucentSortPriority(2)',
	'? InAlarmDisplayOnMaterial',
	': InPosDisplayOnMaterial',
	'FVector(2.32f, -12.0f, 2.0f)',
	'FVector(0.001f, 0.04f, 0.0065f)',
	'FVector(0.001f, 0.0065f, 0.04f)',
	'DisplayOnInstances->ClearInstances()',
	'DisplayOffInstances->ClearInstances()',
	'IsValid(DisplayGlassMaterial)'
)) {
	Assert-Contract ($puzzleSource.Contains($invariant)) `
		"Layered terminal display is missing '$invariant'."
}
foreach ($invariant in @(
	'GlassMaterial,',
	'AlarmMaterial,',
	'ScreenGlowMaterial,',
	'GetLayeredTimeEntryDisplayCount()',
	'layered_displays=%d',
	'GetTimeEntryMeshComponentCount()',
	'TimeEntryMeshComponentCount == 23',
	'time_entry_mesh_components=%d'
)) {
	Assert-Contract (
		$worldScene.Contains($invariant) -or $director.Contains($invariant)
	) "P1/P2 display-material wiring is missing '$invariant'."
}

# Front-to-back order in actor-local X: backing, LED, clear lens.
$backingFrontX = 2.12 + (0.20 * 0.5)
$segmentBackX = 2.32 - (0.10 * 0.5)
$segmentFrontX = 2.32 + (0.10 * 0.5)
$lensBackX = 2.45 - (0.08 * 0.5)
$buttonBackX = 3.10 - (1.50 * 0.5)
Assert-Contract ($backingFrontX -lt $segmentBackX) `
	'The LED segments intersect the opaque display backing.'
Assert-Contract ($segmentFrontX -lt $lensBackX) `
	'The clear display lens intersects the LED segments.'
Assert-Contract ($buttonBackX -lt 2.45) `
	'The interaction button no longer sits close to the terminal face.'

# The source glTF remains the independent dimensional oracle for the imported
# body. UE converts its Y-up axes during import, but the width/depth/height
# magnitudes remain identical.
$posSourcePath = Join-Path $projectRoot `
	'Content/SourceArt/PhotoProps/CashRegister_01/CashRegister_01_1k.gltf'
Assert-Contract (Test-Path -LiteralPath $posSourcePath -PathType Leaf) `
	'The POS source glTF is missing.'
$posSource = Get-Content -Raw -Encoding UTF8 -LiteralPath $posSourcePath |
	ConvertFrom-Json
$bodyPrimitive = $posSource.meshes[0].primitives[0]
$bodyAccessor = $posSource.accessors[$bodyPrimitive.attributes.POSITION]
$sourceWidthCm =
	([double]$bodyAccessor.max[0] - [double]$bodyAccessor.min[0]) * 100.0
$sourceHeightCm =
	([double]$bodyAccessor.max[1] - [double]$bodyAccessor.min[1]) * 100.0
$sourceDepthCm =
	([double]$bodyAccessor.max[2] - [double]$bodyAccessor.min[2]) * 100.0
$scaledWidthCm = $sourceWidthCm * 0.96
$scaledHeightCm = $sourceHeightCm * 0.96
$scaledDepthCm = $sourceDepthCm * 0.96
$keypadOuterEdgeCm = 27.0 + 1.3
$keypadInnerEdgeCm = 19.0 - 1.3
$displayRightEdgeCm = 31.0 * 0.5
$drawerBottomCm = -20.0 - (11.0 * 0.5)
Assert-Contract (
	$sourceWidthCm -ge 59.0 -and $sourceWidthCm -le 61.0 -and
	$sourceHeightCm -ge 61.0 -and $sourceHeightCm -le 63.0 -and
	$sourceDepthCm -ge 43.0 -and $sourceDepthCm -le 45.0
) "Unexpected POS source bounds: ${sourceWidthCm}x${sourceDepthCm}x${sourceHeightCm} cm."
Assert-Contract ($keypadOuterEdgeCm -le ($scaledWidthCm * 0.5)) `
	'The decorative keypad extends beyond the scaled POS body.'
Assert-Contract (($keypadInnerEdgeCm - $displayRightEdgeCm) -ge 2.0) `
	'The decorative keypad is visually merged with the time display.'
Assert-Contract ($drawerBottomCm -ge (-$scaledHeightCm * 0.5)) `
	'The cash drawer extends below the scaled POS body.'
Assert-Contract ($scaledDepthCm -ge 41.0 -and $scaledDepthCm -le 43.0) `
	'The fitted POS depth is outside the intended counter footprint.'

$memoBlock = Get-BlockBetween $director `
	'if (Note == MirrorAlarmMemo)' `
	'if (Note == MailboxBills)'
Assert-Contract ($memoBlock.Contains('단말에 맞는 시각을 넣어 보자')) `
	'P1 memo no longer explains the physical input.'
Assert-Contract (-not $memoBlock.Contains('RegisterSecondMorningTruth')) `
	'Closing the P1 memo must not solve P1 automatically.'
Assert-Contract (-not $memoBlock.Contains('P1.AlarmArithmeticProxy')) `
	'The removed P1 proxy is still active in the memo handler.'

$employeeBlock = Get-BlockBetween $director `
	'else if (StateTag.MatchesTagExact(CalledEmployeeTag))' `
	'else if (StateTag.MatchesTagExact(ReadDuplicateReceiptTag))'
Assert-Contract ($employeeBlock.Contains('SetP2ShutterStage')) `
	'Employee call does not start the P2 pressure space.'
Assert-Contract (-not $employeeBlock.Contains('RevealSecondReceipt')) `
	'Employee call still reveals the duplicate receipt before P2 is solved.'

$confirmationBlock = Get-BlockBetween $director `
	'void AIGSecondMorningDirector::HandleTimeEntryConfirmed(' `
	'void AIGSecondMorningDirector::ApplyP1PressureStage('
foreach ($invariant in @(
	'TEXT("Truth.Alarm0510")',
	'FName(TEXT("P1"))',
	'MarkPuzzleResolved(FName(TEXT("P2")))',
	'Scene->RevealSecondReceipt()',
	'CreateClothSettle',
	'CreateShutterMotorStep',
	'CreateThermalPrinterFeed',
	'CreateJingleOpeningNotes'
)) {
	Assert-Contract ($confirmationBlock.Contains($invariant)) `
		"Solved/wrong response is missing '$invariant'."
}
foreach ($invariant in @(
	'RestoreTimeEntryPuzzles()',
	'PersistTimeEntryState',
	'Pressure%d',
	'RequestCheckpointAutosave',
	'P1.AlarmArithmeticProxy',
	'P2.ReceiptComparisonProxy'
)) {
	Assert-Contract ($director.Contains($invariant)) `
		"Save compatibility or interruption restore is missing '$invariant'."
}

# 04:31은 기억에 맡기지 않고 404호 폰의 실제 조사 표면에서 다시 읽는다.
foreach ($invariant in @(
	'EIGChapterTwoHumanCheckAction::PhoneApprovalRecord',
	'04:31 카드 승인 기록 확인',
	'ApprovalRecordTitle',
	'2024/07/26(금) 04:31',
	'PhoneApprovalRecord->CompleteInteraction_Implementation',
	'bApprovalRecordPresented',
	'PhoneApprovalAction->SetAvailable(bUnlocked)',
	'PhoneApprovalReadTag'
)) {
	Assert-Contract ($humanGate.Contains($invariant)) `
		"P2 fair-clue route is missing '$invariant'."
}
$phoneTextIndex = $humanGate.IndexOf('PhoneApprovalRecord->SetNoteText(')
$phonePresentationIndex =
	$humanGate.IndexOf('PhoneApprovalRecord->SetPhoneNotificationPresentation();')
Assert-Contract (
	$phoneTextIndex -ge 0 -and $phonePresentationIndex -gt $phoneTextIndex
) 'The phone presentation is selected before its approval record is authored.'
foreach ($invariant in @(
	'SetPhoneNotificationPresentation',
	'UsesPhoneNotificationPresentation',
	'bUsesPhoneNotificationPresentation = true',
	'bUsesThermalReceiptPresentation = false'
)) {
	Assert-Contract (
		$readableHeader.Contains($invariant) -or
		$readableSource.Contains($invariant)
	) "Phone readable-note presentation is missing '$invariant'."
}
foreach ($invariant in @(
	'void AIGHorrorHUD::DrawPhoneNotificationPanel',
	'Note->UsesPhoneNotificationPresentation()',
	'PhoneHeight * 0.58f',
	'PhoneMetaFontSize = 15',
	'KoreanFontMedium',
	'KoreanPhoneMetaFont',
	'NotificationHeight = FMath::Min(250.0f, InnerHeight * 0.47f)',
	'PhoneApprovalApp',
	'PhoneApprovalJustNow',
	'PhoneApprovalHistory',
	'FLinearColor(0.20f, 0.55f, 0.95f, 1.0f)',
	'PhoneCloseGamepad',
	'PhoneCloseKeyboard',
	'NoteCloseGamepad',
	'NoteCloseKeyboard',
	'ReceiptCloseGamepad',
	'ReceiptCloseKeyboard'
)) {
	Assert-Contract ($hud.Contains($invariant)) `
		"Phone HUD or device-specific document control is missing '$invariant'."
}
Assert-Contract (-not $hud.Contains('[ E / A ]')) `
	'Document panels still show keyboard and gamepad prompts at the same time.'

# Runtime screenshots remain a manual gate. This independent geometry oracle
# catches minimum-resolution clipping before the HUD is allowed into that gate.
foreach ($resolution in @(
	@{ Width = 1280.0; Height = 720.0 },
	@{ Width = 1600.0; Height = 900.0 },
	@{ Width = 1920.0; Height = 1080.0 },
	@{ Width = 2560.0; Height = 1440.0 }
)) {
	$width = $resolution.Width
	$height = $resolution.Height
	$phoneHeight = [Math]::Min(700.0, [Math]::Max(500.0, $height * 0.82))
	$phoneWidth = [Math]::Min(400.0, [Math]::Max(280.0, $phoneHeight * 0.58))
	$phoneX = ($width - $phoneWidth) * 0.5
	$phoneY = ($height - $phoneHeight) * 0.5
	$innerHeight = $phoneHeight - 16.0
	$notificationY = $phoneY + 8.0 + 48.0
	$notificationHeight = [Math]::Min(250.0, $innerHeight * 0.47)
	$lineHeight = [Math]::Min(30.0, [Math]::Max(24.0, $notificationHeight / 7.8))
	$lastBodyBaseline = $notificationY + 94.0 + (3.0 * $lineHeight)
	$closeHintY = [Math]::Min($height - 26.0, $phoneY + $phoneHeight + 16.0)
	Assert-Contract (
		$phoneX -ge 0.0 -and $phoneX + $phoneWidth -le $width
	) "Phone frame is horizontally clipped at ${width}x${height}."
	Assert-Contract (
		$phoneY -ge 0.0 -and $phoneY + $phoneHeight -le $height
	) "Phone frame is vertically clipped at ${width}x${height}."
	Assert-Contract (
		$lastBodyBaseline + 18.0 -lt $notificationY + $notificationHeight
	) "Approval body overflows its notification card at ${width}x${height}."
	Assert-Contract ($closeHintY + 18.0 -le $height) `
		"Phone close hint is clipped at ${width}x${height}."
}

Add-Type -AssemblyName System.Drawing
$measurementBitmap = New-Object System.Drawing.Bitmap(1, 1)
$measurementGraphics =
	[System.Drawing.Graphics]::FromImage($measurementBitmap)
$phoneBodyFont = New-Object System.Drawing.Font(
	'Malgun Gothic',
	19,
	[System.Drawing.FontStyle]::Regular,
	[System.Drawing.GraphicsUnit]::Pixel)
try {
	# 720p에서 실제 코드가 만드는 본문 폭: phone 342.432 - bezel 16 -
	# notification margins 22 - text margins 32 = 272.432 px.
	$minimumBodyWidth = (720.0 * 0.82 * 0.58) - 70.0
	foreach ($line in @(
		'카드 승인 알림',
		'해온카드 · 체크카드 원승인',
		'2024/07/26(금) 04:31',
		'새벽24 무영로점',
		'승인 완료'
	)) {
		$measured = $measurementGraphics.MeasureString(
			$line,
			$phoneBodyFont,
			[int]::MaxValue,
			[System.Drawing.StringFormat]::GenericTypographic).Width
		Assert-Contract ($measured -le $minimumBodyWidth) `
			"19 px phone text exceeds the 720p notification width: $line ($measured px)."
	}
} finally {
	$phoneBodyFont.Dispose()
	$measurementGraphics.Dispose()
	$measurementBitmap.Dispose()
}

foreach ($invariant in @(
	'P1TimeEntry->RunAutomatedSolution()',
	'P2TimeEntry->RunAutomatedSolution()',
	'P1TimeEntry->HandleButton(EIGTimeEntryButtonAction::Confirm)',
	'P2TimeEntry->HandleButton(EIGTimeEntryButtonAction::Confirm)',
	'RunP2EndToEndStep() && RunP1EndToEndStep()',
	'RunP1EndToEndStep() && RunP2EndToEndStep()',
	'P1PressureCap',
	'P2PressureCap',
	'FMath::IsNearlyEqual(ShutterBottom, 205.0f, 0.5f)',
	'SourceApprovalRead',
	'GetTimeEntryPhysicalContractCount()',
	'GetAuthoredTimeEntryHousingCount()',
	'GetLayeredTimeEntryDisplayCount()',
	'AuthoredHousingCount == 2',
	'LayeredDisplayCount == 2',
	'authored_housings=%d',
	'layered_displays=%d',
	'time_entry_physical=%d',
	'pressure_caps=%d'
)) {
	Assert-Contract (
		$director.Contains($invariant) -or
		$worldScene.Contains($invariant)
	) "End-to-end physical route is missing '$invariant'."
}
foreach ($legacyId in @(
	'P1.AlarmArithmeticProxy',
	'P2.ReceiptComparisonProxy'
)) {
	Assert-Contract ($evidence.Contains($legacyId)) `
		"Legacy v3 puzzle id '$legacyId' is no longer readable."
}

foreach ($invariant in @(
	'CH02TimeWrite',
	'CH02TimeRead',
	'State.CH02.P1.Minute31',
	'State.CH02.P1.Pressure2',
	'State.CH02.P2.Minute10',
	'State.CH02.P2.Pressure3',
	'State.CH02.P2.SourceApprovalRead',
	's6_ch02_time_resume checkpoint=%d exact=1'
)) {
	Assert-Contract ($persistenceProbe.Contains($invariant)) `
		"Process-boundary persistence is missing '$invariant'."
}
foreach ($invariant in @(
	'IGRebirthCH02TimeCheckpoint',
	'"ch02TimeProcessRestarts": 2',
	'complete boundary=2 cat_choices=5 ch02_time=2 p5=4 p3=7 endings=2'
)) {
	Assert-Contract ($persistenceHarness.Contains($invariant)) `
		"Persistence harness is missing '$invariant'."
}

Write-Host 'REBIRTH CH02 time-entry contract passed.'
