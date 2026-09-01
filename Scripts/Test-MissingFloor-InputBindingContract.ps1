[CmdletBinding()]
param()

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

# 런타임 키 재설정(§19.8)의 계약.
#
# 이 검사가 있는 이유는 만들면서 바로 깨졌기 때문이다. 재설정 표에
# `LoadAutosave`의 패드 기본값을 Y로 적었는데, Y는 기록 열람이 이미 쓰고
# 있었다. 한 장치에서 두 동사가 같은 버튼을 쓰는 것을 재설정 화면이 금지하는데
# **기본값이 그 규칙을 어기고 있었다.**
#
# 그래서 두 방향을 본다.
#
#   1. 표의 기본값 == `Config/DefaultInput.ini`. 한쪽만 고치면 처음 실행한
#      사람은 표에 없는 키를 쓰게 된다
#   2. 표 안에서 같은 장치에 중복 키가 없다. 기본값 자체가 규칙을 지켜야 한다

$projectRoot = Split-Path -Parent $PSScriptRoot
$assertionCount = 0

function Read-ProjectText {
	param([Parameter(Mandatory = $true)][string]$RelativePath)
	$path = Join-Path $projectRoot $RelativePath
	if (-not (Test-Path -LiteralPath $path -PathType Leaf)) {
		throw "Missing input-binding contract file: $RelativePath"
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

$bindingSource = Read-ProjectText 'Source/IndieGame/Player/IGInputBindingSubsystem.cpp'
$bindingHeader = Read-ProjectText 'Source/IndieGame/Player/IGInputBindingSubsystem.h'
$inputConfig = Read-ProjectText 'Config/DefaultInput.ini'
$controllerSource = Read-ProjectText 'Source/IndieGame/Player/IGPlayerController.cpp'
$hudSource = Read-ProjectText 'Source/IndieGame/Player/IGHorrorHUD.cpp'

# --- 표를 읽는다 ------------------------------------------------------------

$tableBody = [regex]::Match(
	$bindingSource,
	'static const TArray<FIGBindableActionInfo> Table = \{(?<body>[\s\S]*?)\n\t\t\};')
if (-not $tableBody.Success) {
	throw 'The bindable action table could not be isolated.'
}
$entries = @([regex]::Matches(
	$tableBody.Groups['body'].Value,
	'TEXT\("(?<action>[A-Za-z]+)"\)[\s\S]*?EKeys::(?<keyboard>[A-Za-z_0-9]+),[\s\S]*?EKeys::(?<gamepad>[A-Za-z_0-9]+),'))

$assertionCount++
if ($entries.Count -lt 10) {
	throw "The bindable action table lost entries: found $($entries.Count)."
}

# --- 1. 기본값이 DefaultInput.ini와 같은가 ---------------------------------

foreach ($entry in $entries) {
	$action = $entry.Groups['action'].Value
	$configured = @([regex]::Matches(
		$inputConfig,
		"ActionName=""$action"",[^)]*Key=(?<key>[A-Za-z_0-9]+)") |
		ForEach-Object { $_.Groups['key'].Value })
	$assertionCount++
	if ($configured.Count -lt 1) {
		throw "The action table names an action with no default mapping: $action"
	}
	foreach ($column in @('keyboard', 'gamepad')) {
		$tableKey = $entry.Groups[$column].Value
		$assertionCount++
		if ($tableKey -eq 'Invalid') {
			# 기본값이 없다고 적었으면 설정에도 그 장치의 매핑이 없어야 한다.
			$hasPad = @($configured | Where-Object { $_ -like 'Gamepad_*' }).Count -gt 0
			$hasKeyboard = @($configured | Where-Object { $_ -notlike 'Gamepad_*' }).Count -gt 0
			if (($column -eq 'gamepad' -and $hasPad) -or
				($column -eq 'keyboard' -and $hasKeyboard)) {
				throw ("$action declares no $column default but DefaultInput.ini " +
					'binds one.')
			}
			continue
		}
		if ($configured -notcontains $tableKey) {
			throw ("$action $column default is $tableKey, which is not in " +
				'DefaultInput.ini.')
		}
	}
}

# --- 2. 기본값 자체가 재설정 규칙을 지키는가 --------------------------------

foreach ($column in @('keyboard', 'gamepad')) {
	$seen = @{}
	foreach ($entry in $entries) {
		$key = $entry.Groups[$column].Value
		if ($key -eq 'Invalid') {
			continue
		}
		$assertionCount++
		if ($seen.ContainsKey($key)) {
			throw ("Two actions share the same $column default: " +
				"$($seen[$key]) and $($entry.Groups['action'].Value) both use $key.")
		}
		$seen[$key] = $entry.Groups['action'].Value
	}
}

# 두드리기와 조사가 같은 키를 쓰면 §24 즉시 차단 16이 깨진다. 위의 중복
# 검사가 이미 막지만, 그 규칙이 왜 있는지를 이름으로 남겨 둔다.
$assertionCount++
$knock = @($entries | Where-Object { $_.Groups['action'].Value -eq 'Knock' })
$interact = @($entries | Where-Object { $_.Groups['action'].Value -eq 'Interact' })
if ($knock.Count -ne 1 -or $interact.Count -ne 1) {
	throw 'Knock and Interact must both be rebindable actions.'
}
if ($knock[0].Groups['keyboard'].Value -eq $interact[0].Groups['keyboard'].Value) {
	throw '§24 즉시 차단 16: knock must never share a key with interact.'
}

# --- 3. 규칙이 코드에 있는가 ------------------------------------------------

Assert-ContainsAll $bindingHeader @(
	'bool TryRebind(',
	'void ResetToDefaults();',
	'static bool IsReservedKey(const FKey& Key);',
	'bool HasAnyOverride() const'
) '재설정 API'

# 거절할 때는 왜인지 말한다. 「안 됩니다」만 말하는 화면은 고장과 구분되지 않는다.
Assert-ContainsAll $bindingSource @(
	'OutFailureReason',
	'RebindReserved',
	'RebindConflict',
	'RebindNeedsPad',
	'RebindNeedsKeyboard'
) '거절 사유'

# 나가는 문은 잠글 수 없다.
Assert-ContainsAll $bindingSource @(
	'Key == EKeys::Escape',
	'Key == EKeys::F10'
) '고정 키'

# DefaultInput.ini는 패키지에서 읽기 전용이고 다음 사람의 기본값이다.
# 재설정은 사용자 설정에만 남아야 한다.
$assertionCount++
if ($bindingSource.Contains('SaveKeyMappings()')) {
	throw 'Rebinding must not write DefaultInput.ini; user overrides live in GameUserSettings.'
}
Assert-ContainsAll $bindingSource @(
	'GGameUserSettingsIni',
	'ForceRebuildKeymaps()'
) '재설정 영속화'

# 저장이 규칙을 어기고 있으면 버린다. 남겨 두면 영영 눌리지 않는 키를
# 화면이 「지금 이 키」라고 보여 준다.
$loadBody = [regex]::Match(
	$bindingSource,
	'void UIGInputBindingSubsystem::LoadOverrides\(\)(?<body>[\s\S]*?)\r?\n\}')
if (-not $loadBody.Success) {
	throw 'LoadOverrides could not be isolated.'
}
foreach ($guard in @('IsReservedKey(StoredKey)', 'StoredKey.IsGamepadKey()')) {
	$assertionCount++
	if (-not $loadBody.Groups['body'].Value.Contains($guard)) {
		throw "Stored overrides must be re-validated on load: $guard"
	}
}

# --- 4. 화면이 있는가 -------------------------------------------------------

Assert-ContainsAll $controllerSource @(
	'EIGSystemMenuMode::KeyBindings',
	'UIGInputBindingSubsystem'
) '재설정 화면 진입'
Assert-ContainsAll $hudSource @(
	'bSystemMenuIsKeyBindings'
) '재설정 화면 렌더'

# --- 5. 시점 감도와 상하 반전 -----------------------------------------------
#
# 1인칭에서 감도를 못 바꾸면 손이 맞지 않는 사람은 그 자리에서 끝난다.
# 마우스와 패드는 곡선이 달라 값이 둘이어야 한다 — 하나로 묶으면 한쪽을
# 맞추는 순간 다른 쪽이 어긋난다.

Assert-ContainsAll $bindingSource @(
	'AdjustMouseSensitivity',
	'AdjustGamepadSensitivity',
	'ToggleInvertLookY'
) '시점 조절 진입점'

foreach ($axis in @('Mouse', 'Gamepad')) {
	$adjustBody = [regex]::Match(
		$bindingSource,
		('void UIGInputBindingSubsystem::Adjust{0}Sensitivity\(' -f $axis) +
			'const int32 Direction\)(?<body>[\s\S]*?)\r?\n\}')
	$assertionCount++
	if (-not $adjustBody.Success) {
		throw "Adjust${axis}Sensitivity could not be isolated."
	}
	$assertionCount++
	if ($adjustBody.Groups['body'].Value -notmatch
		('{0}Sensitivity\s*=\s*FMath::Clamp' -f $axis)) {
		throw "Adjust${axis}Sensitivity must clamp to the declared range."
	}
	$assertionCount++
	if (-not $adjustBody.Groups['body'].Value.Contains('LookSensitivityStep')) {
		throw "Adjust${axis}Sensitivity must move by the declared step."
	}
}

# 저장한 값도 다시 조인다. 손으로 고친 ini가 화면을 못 쓰게 만들면 안 된다.
$lookLoadBody = [regex]::Match(
	$bindingSource,
	'void UIGInputBindingSubsystem::LoadLookSettings\(\)(?<body>[\s\S]*?)\r?\n\}')
$assertionCount++
if (-not $lookLoadBody.Success) {
	throw 'LoadLookSettings could not be isolated.'
}
$assertionCount++
if (([regex]::Matches(
		$lookLoadBody.Groups['body'].Value, 'FMath::Clamp')).Count -lt 2) {
	throw 'Stored look sensitivities must be re-clamped on load.'
}

# 되돌리기가 키만 되돌리고 감도를 남기면 「전부 기본값으로」가 거짓말이 된다.
$resetBody = [regex]::Match(
	$bindingSource,
	'void UIGInputBindingSubsystem::ResetToDefaults\(\)(?<body>[\s\S]*?)\r?\n\}')
$assertionCount++
if (-not $resetBody.Success) {
	throw 'ResetToDefaults could not be isolated.'
}
foreach ($restored in @(
	'MouseSensitivity = 1.0f',
	'GamepadSensitivity = 1.0f',
	'bInvertLookY = false')) {
	$assertionCount++
	if (-not $resetBody.Groups['body'].Value.Contains($restored)) {
		throw "Reset-all must also restore look settings: $restored"
	}
}

# 값이 실제로 시점에 걸리는가. 설정만 있고 카메라가 안 읽으면 아무 일도
# 일어나지 않는 화면이 된다.
$characterSource = Get-Content -Raw -Encoding UTF8 (
	Join-Path $projectRoot 'Source/IndieGame/Player/IGPlayerCharacter.cpp')
Assert-ContainsAll $characterSource @(
	'AddControllerYawInput(Value * GetLookSensitivity())',
	'IsLookInverted()'
) '시점 적용'

# Turn/LookUp은 마우스와 스틱을 같은 축으로 받는다. 장치를 안 가리면 한쪽
# 값이 다른 장치에도 걸린다.
$sensitivityBody = [regex]::Match(
	$characterSource,
	'float AIGPlayerCharacter::GetLookSensitivity\(\) const(?<body>[\s\S]*?)\r?\n\}')
$assertionCount++
if (-not $sensitivityBody.Success) {
	throw 'GetLookSensitivity could not be isolated.'
}
foreach ($branch in @(
	'IsUsingGamepadLook()',
	'GetGamepadSensitivity()',
	'GetMouseSensitivity()')) {
	$assertionCount++
	if (-not $sensitivityBody.Groups['body'].Value.Contains($branch)) {
		throw "Look sensitivity must pick per device: $branch"
	}
}
# 장치 판정 자체는 게임이 이미 들고 있는 값을 봐야 한다. 여기가 따로 놀면
# 화면은 패드 안내를 보여 주는데 시점은 마우스 감도로 도는 일이 생긴다.
$deviceBody = [regex]::Match(
	$characterSource,
	'bool AIGPlayerCharacter::IsUsingGamepadLook\(\) const(?<body>[\s\S]*?)\r?\n\}')
$assertionCount++
if (-not $deviceBody.Success) {
	throw 'IsUsingGamepadLook could not be isolated.'
}
$assertionCount++
if (-not $deviceBody.Groups['body'].Value.Contains('IsUsingGamepadForHud()')) {
	throw 'The look device must follow the HUD device judgement.'
}

# --- §18.3 패드 시점 문법 ---------------------------------------------------
#
# 데드존·응답 곡선·최대 회전은 문서가 숫자까지 적어 둔 것이고, 그동안
# 코드에는 한 줄도 없었다.

$story = Read-ProjectText 'Docs/STORY_BIBLE_MISSING_FLOOR.md'
$padRow = [regex]::Match(
	$story,
	'데드존 내측 (?<inner>[0-9.]+) 외측 (?<outer>[0-9.]+), 응답 곡선 지수 (?<curve>[0-9.]+), 최대 회전 (?<rate>[0-9]+)')
$assertionCount++
if (-not $padRow.Success) {
	throw 'The §18.3 gamepad look row could not be read.'
}
$padValues = @{
	'PadInnerDeadzone' = $padRow.Groups['inner'].Value
	'PadOuterDeadzone' = $padRow.Groups['outer'].Value
	'PadResponseExponent' = $padRow.Groups['curve'].Value
	'PadMaximumTurnRateDegrees' = $padRow.Groups['rate'].Value
}
foreach ($name in $padValues.Keys) {
	$declared = [regex]::Match(
		$characterSource,
		('constexpr float {0} = (?<value>[0-9.]+)f;' -f $name))
	$assertionCount++
	if (-not $declared.Success) {
		throw "The §18.3 gamepad constant is missing: $name"
	}
	$assertionCount++
	if ([double]$declared.Groups['value'].Value -ne [double]$padValues[$name]) {
		throw (
			'{0} is {1} but §18.3 says {2}.' -f
				$name, $declared.Groups['value'].Value, $padValues[$name])
	}
}

# 데드존은 원시 스틱에 걸어야 한다. 들어온 축 값에 걸면 마우스가 섞여 있어
# 값을 되돌릴 수 없다.
$shapeBody = [regex]::Match(
	$characterSource,
	'float AIGPlayerCharacter::ShapeGamepadLookAxis\(const float RawStick\)(?<body>[\s\S]*?)\r?\n\}')
$assertionCount++
if (-not $shapeBody.Success) {
	throw 'ShapeGamepadLookAxis could not be isolated.'
}
foreach ($piece in @(
	'IGPlayerNoise::PadInnerDeadzone',
	'IGPlayerNoise::PadOuterDeadzone',
	'FMath::Pow(Normalized, IGPlayerNoise::PadResponseExponent)')) {
	$assertionCount++
	if (-not $shapeBody.Groups['body'].Value.Contains($piece)) {
		throw "The stick curve must use the declared §18.3 shape: $piece"
	}
}
$applyBody = [regex]::Match(
	$characterSource,
	'bool AIGPlayerCharacter::ApplyGamepadLook\((?<body>[\s\S]*?)\r?\n\}')
$assertionCount++
if (-not $applyBody.Success) {
	throw 'ApplyGamepadLook could not be isolated.'
}
$assertionCount++
if (-not $applyBody.Groups['body'].Value.Contains('PlayerInput->GetKeyValue(StickAxis)')) {
	throw 'The deadzone must read the raw stick, not the blended axis (§18.3).'
}
# 초당 회전 상한. DeltaSeconds가 빠지면 프레임률이 곧 감도가 된다.
$assertionCount++
if (-not $applyBody.Groups['body'].Value.Contains('World->GetDeltaSeconds()')) {
	throw 'The pad turn rate must be measured per second (§18.3).'
}
$assertionCount++
if (-not $applyBody.Groups['body'].Value.Contains(
	'IGPlayerNoise::PadMaximumTurnRateDegrees')) {
	throw 'The pad turn rate cap is not applied (§18.3).'
}
# 각도 그대로 넣는다는 전제가 설정에 남아 있는가.
$assertionCount++
if ($inputConfig -notmatch 'bEnableLegacyInputScales=False') {
	throw 'Legacy input scales must stay off or the turn-rate cap breaks (§18.3).'
}

# 수직 별도 배율. 문서가 폭까지 적어 두었다.
$verticalRow = [regex]::Match(
	$story, '수직 별도 배율 (?<min>[0-9]+\.[0-9]+)~(?<max>[0-9]+\.[0-9]+)')
$assertionCount++
if (-not $verticalRow.Success) {
	throw 'The §18.3 vertical look scale row could not be read.'
}
foreach ($bound in @(
	@{ Name = 'MinimumVerticalLookScale'; Value = $verticalRow.Groups['min'].Value },
	@{ Name = 'MaximumVerticalLookScale'; Value = $verticalRow.Groups['max'].Value })) {
	$declared = [regex]::Match(
		$bindingHeader, ('{0} = (?<value>[0-9.]+)f;' -f $bound.Name))
	$assertionCount++
	if (-not $declared.Success) {
		throw ('The vertical look bound is missing: {0}' -f $bound.Name)
	}
	$assertionCount++
	if ([double]$declared.Groups['value'].Value -ne [double]$bound.Value) {
		throw (
			'{0} is {1} but §18.3 says {2}.' -f
				$bound.Name, $declared.Groups['value'].Value, $bound.Value)
	}
}
$assertionCount++
if (-not $characterSource.Contains('GetVerticalLookScale()')) {
	throw 'The vertical scale must reach the pitch input (§18.3).'
}

# 화면의 행 번호가 시점 셋만큼 밀려 있는가. 여기가 어긋나면 조사를 고르고
# 두드리기가 바뀐다.
$assertionCount++
if ($controllerSource -notmatch
	'TryRebind\(\s*\r?\n?\s*KeyBindingSelection - UIGInputBindingSubsystem::LookRowCount') {
	throw 'Rebind must offset the screen row by the look rows.'
}
$assertionCount++
if ($hudSource -notmatch
	'Row \+ UIGInputBindingSubsystem::LookRowCount\s*\r?\n?\s*== SystemMenuKeyBindingSelection') {
	throw 'Binding rows must be drawn with the look-row offset.'
}

# --- 헤드밥 (§18.3) ---------------------------------------------------------
#
# 문서가 자세별 진폭과 주기를 적어 두었다. 그동안 코드에는 진폭이 하나뿐이었고,
# 동작 감소에서는 아예 0이 됐다 — 문서가 「0으로 만들지 않는다」고 못 박은
# 바로 그 자리다.

$bobRow = [regex]::Match(
	$story,
	'헤드밥 진폭: 걷기 (?<walk>[0-9.]+)cm\(주기 (?<period>[0-9.]+)s\), 앉기 (?<crouch>[0-9.]+)cm, 달리기 (?<sprint>[0-9.]+)cm')
$assertionCount++
if (-not $bobRow.Success) {
	throw 'The §18.3 head bob row could not be read.'
}
$bobValues = @{
	'WalkBobAmplitude' = $bobRow.Groups['walk'].Value
	'CrouchBobAmplitude' = $bobRow.Groups['crouch'].Value
	'SprintBobAmplitude' = $bobRow.Groups['sprint'].Value
}
foreach ($name in $bobValues.Keys) {
	$declared = [regex]::Match(
		$characterSource,
		('constexpr float {0} = (?<value>[0-9.]+)f;' -f $name))
	$assertionCount++
	if (-not $declared.Success) {
		throw "The §18.3 head bob constant is missing: $name"
	}
	$assertionCount++
	if ([double]$declared.Groups['value'].Value -ne [double]$bobValues[$name]) {
		throw (
			'{0} is {1} but §18.3 says {2}.' -f
				$name, $declared.Groups['value'].Value, $bobValues[$name])
	}
}

# 자세를 실제로 가르는가. 상수만 세 개 두고 하나만 쓰면 표만 맞는다.
$amplitudeBody = [regex]::Match(
	$characterSource,
	'float AIGPlayerCharacter::GetHeadBobAmplitude\(\) const(?<body>[\s\S]*?)\r?\n\}')
$assertionCount++
if (-not $amplitudeBody.Success) {
	throw 'GetHeadBobAmplitude could not be isolated.'
}
foreach ($stance in @('bIsCrouched', 'bSprinting')) {
	$assertionCount++
	if (-not $amplitudeBody.Groups['body'].Value.Contains($stance)) {
		throw "The head bob must read the stance: $stance"
	}
}
foreach ($name in $bobValues.Keys) {
	$assertionCount++
	if (-not $amplitudeBody.Groups['body'].Value.Contains(
		('IGPlayerNoise::{0}' -f $name))) {
		throw "The head bob amplitude is declared but unused: $name"
	}
}

# 주기는 보폭에서 나온다. 헤드밥과 발소리가 같은 위상을 쓰기 때문이다.
$characterHeader = Read-ProjectText 'Source/IndieGame/Player/IGPlayerCharacter.h'
$strideMatch = [regex]::Match(
	$characterHeader, 'float StepDistance = (?<value>[0-9.]+)f;')
$assertionCount++
if (-not $strideMatch.Success) {
	throw 'StepDistance could not be read.'
}
$referenceMatch = [regex]::Match(
	$characterSource, 'constexpr float ReferenceWalkSpeed = (?<value>[0-9.]+)f;')
$assertionCount++
if (-not $referenceMatch.Success) {
	throw 'ReferenceWalkSpeed could not be read.'
}
$expectedStride =
	[double]$bobRow.Groups['period'].Value * [double]$referenceMatch.Groups['value'].Value / 2.0
$assertionCount++
if ([Math]::Abs([double]$strideMatch.Groups['value'].Value - $expectedStride) -gt 0.5) {
	throw (
		'StepDistance is {0} but §18.3 period {1}s at {2}cm/s wants {3}.' -f
			$strideMatch.Groups['value'].Value,
			$bobRow.Groups['period'].Value,
			$referenceMatch.Groups['value'].Value,
			$expectedStride)
}

# 동작 감소가 헤드밥을 0으로 만들면 안 된다. 다른 흔들림과 같은 가드 안에
# 들어 있으면 그렇게 된다.
$motionBody = [regex]::Match(
	$characterSource,
	'void AIGPlayerCharacter::UpdateCameraMotion\(const float DeltaSeconds\)(?<body>[\s\S]*?)\r?\n\}')
$assertionCount++
if (-not $motionBody.Success) {
	throw 'UpdateCameraMotion could not be isolated.'
}
$motionText = $motionBody.Groups['body'].Value
$assertionCount++
if ($motionText -notmatch
	'const float BobScale = bReducedMotion\s*\r?\n?\s*\?\s*IGPlayerNoise::ReducedMotionBobScale') {
	throw 'Reduced motion must scale the head bob, not remove it (§18.3).'
}
$assertionCount++
if ($motionText -notmatch
	'if \(bWalking\)\s*\r?\n\s*\{\s*\r?\n(?![\s\S]{0,200}?if \(!bReducedMotion\))') {
	throw 'The head bob must not sit inside a reduced-motion guard (§18.3).'
}
$scaleMatch = [regex]::Match(
	$characterSource, 'constexpr float ReducedMotionBobScale = (?<value>[0-9.]+)f;')
$assertionCount++
if (-not $scaleMatch.Success) {
	throw 'ReducedMotionBobScale is missing.'
}
$assertionCount++
if ([double]$scaleMatch.Groups['value'].Value -ne 0.25) {
	throw (
		'ReducedMotionBobScale is {0} but §18.3 says 0.25.' -f
			$scaleMatch.Groups['value'].Value)
}
$assertionCount++
if ($story -notmatch '\*\*0으로 만들지 않는다\*\*') {
	throw 'The §18.3 rule that the bob never reaches zero was removed.'
}

# --- §18.4 상호작용 손맛 -----------------------------------------------------
#
# 조준 반경부터 입력 버퍼까지 열한 줄이 숫자로 적혀 있다. 대부분은 코드와
# 맞았지만 망치 차징이 0.8이 아니라 1.0이었고, 서랍은 만들어진 적이 없었다.

$interactionHeader = Read-ProjectText 'Source/IndieGame/Player/IGInteractionComponent.h'
$hudSource = Read-ProjectText 'Source/IndieGame/Player/IGHorrorHUD.cpp'

# 조준: 스윕 반경과 최대 거리.
$reachRow = [regex]::Match(
	$story, '반경 (?<radius>[0-9]+)cm 스윕 보정[\s\S]{0,80}?최대 거리 (?<distance>[0-9]+)cm')
$assertionCount++
if (-not $reachRow.Success) {
	throw 'The §18.4 reach row could not be read.'
}
foreach ($field in @(
	@{ Name = 'FocusSweepRadius'; Value = $reachRow.Groups['radius'].Value },
	@{ Name = 'TraceDistance'; Value = $reachRow.Groups['distance'].Value })) {
	$declared = [regex]::Match(
		$interactionHeader, ('float {0} = (?<value>[0-9.]+)f;' -f $field.Name))
	$assertionCount++
	if (-not $declared.Success) {
		throw ('The §18.4 reach field is missing: {0}' -f $field.Name)
	}
	$assertionCount++
	if ([double]$declared.Groups['value'].Value -ne [double]$field.Value) {
		throw (
			'{0} is {1} but §18.4 says {2}.' -f
				$field.Name, $declared.Groups['value'].Value, $field.Value)
	}
}

# 브래킷: 지연과 형성 시간. 즉시 팝인은 금지다.
$bracketRow = [regex]::Match(
	$story, '대상 진입 후 (?<delay>[0-9]+)ms 지연 뒤 (?<reveal>[0-9]+)ms에 걸쳐 맺힌다')
$assertionCount++
if (-not $bracketRow.Success) {
	throw 'The §18.4 bracket row could not be read.'
}
foreach ($field in @(
	@{ Name = 'FocusAcquireDelaySeconds'; Ms = $bracketRow.Groups['delay'].Value },
	@{ Name = 'FocusAcquireRevealSeconds'; Ms = $bracketRow.Groups['reveal'].Value })) {
	$declared = [regex]::Match(
		$hudSource, ('constexpr float {0} = (?<value>[0-9.]+)f;' -f $field.Name))
	$assertionCount++
	if (-not $declared.Success) {
		throw ('The §18.4 bracket field is missing: {0}' -f $field.Name)
	}
	$expectedSeconds = [double]$field.Ms / 1000.0
	$assertionCount++
	if ([Math]::Abs([double]$declared.Groups['value'].Value - $expectedSeconds) -gt 0.0005) {
		throw (
			'{0} is {1}s but §18.4 says {2}ms.' -f
				$field.Name, $declared.Groups['value'].Value, $field.Ms)
	}
}
$assertionCount++
if ($story -notmatch '즉시 팝인 금지') {
	throw 'The §18.4 no-popin rule was removed.'
}

# 홀드 진행은 브래킷이 닫히는 것이다. 링·숫자·퍼센트는 금지다.
$assertionCount++
if ($hudSource -notmatch
	'DrawFocusBracket\(\s*\r?\n?\s*IGHorrorHUD::RedAccent,\s*\r?\n?\s*HoldProgress\)') {
	throw 'Hold progress must be drawn as the bracket closing (§18.4).'
}
foreach ($banned in @('DrawHoldRing', 'HoldPercentText', 'DrawHoldGauge')) {
	$assertionCount++
	if ($hudSource.Contains($banned)) {
		throw "Ring gauges, numbers and percentages are banned (§18.4): $banned"
	}
}
$assertionCount++
if ($story -notmatch '숫자·퍼센트·링 게이지 전부 금지') {
	throw 'The §18.4 gauge ban was removed.'
}

# 홀드 시간. 문서가 부르는 이름과 코드의 자리를 짝지어 둔다.
$holdRow = [regex]::Match(
	$story,
	'홀드 시간: 문 조용히 열기 (?<door>[0-9.]+)s[\s\S]{0,60}?망치 스윙\s*\r?\n?\s*차징 (?<hammer>[0-9.]+)s, 엔딩 A의 조율 렌치 되돌리기 (?<wrench>[0-9.]+)s')
$assertionCount++
if (-not $holdRow.Success) {
	throw 'The §18.4 hold-time row could not be read.'
}
$doorHold = [regex]::Match(
	(Read-ProjectText 'Source/IndieGame/Interaction/IGSwingDoor.h'),
	'float QuietOpenHoldSeconds = (?<value>[0-9.]+)f;')
$assertionCount++
if (-not $doorHold.Success) {
	throw 'QuietOpenHoldSeconds could not be read.'
}
$assertionCount++
if ([double]$doorHold.Groups['value'].Value -ne [double]$holdRow.Groups['door'].Value) {
	throw (
		'QuietOpenHoldSeconds is {0} but §18.4 says {1}.' -f
			$doorHold.Groups['value'].Value, $holdRow.Groups['door'].Value)
}

$nightFour = Read-ProjectText 'Source/IndieGame/Entity/IGMissingFloorNightFourDirector.cpp'
foreach ($target in @(
	@{ Prompt = 'WallBreakPrompt'; Value = $holdRow.Groups['hammer'].Value; Name = '망치 스윙 차징' },
	@{ Prompt = 'EndingAPrompt'; Value = $holdRow.Groups['wrench'].Value; Name = '조율 렌치 되돌리기' })) {
	# HoldSeconds는 NoiseLoudness 바로 앞에 온다. 두 숫자가 붙어 있어서
	# 자리를 세지 않고 읽으면 소음 크기를 홀드 시간으로 착각한다.
	$configured = [regex]::Match(
		$nightFour,
		[regex]::Escape($target.Prompt) +
			'[\s\S]{0,400}?EIGMissingFloorSource::None,\s*\r?\n(?:\s*//[^\r\n]*\r?\n)*\s*(?<hold>[0-9.]+)f,\s*\r?\n\s*(?<noise>[0-9.]+)f\);')
	$assertionCount++
	if (-not $configured.Success) {
		throw ('The hold time could not be read: {0}' -f $target.Prompt)
	}
	$assertionCount++
	if ([double]$configured.Groups['hold'].Value -ne [double]$target.Value) {
		throw (
			'{0} holds {1}s but §18.4 says {2}s.' -f
				$target.Name, $configured.Groups['hold'].Value, $target.Value)
	}
}

# 프로타주는 홀드 내내 소리가 난다. 들리지 않는 비용은 고를 수 없는 비용이다.
$assertionCount++
if (-not (Read-ProjectText 'Source/IndieGame/Entity/IGMissingFloorEvidence.h').Contains(
	'void SetSustainedRubCue(bool bEnabled);')) {
	throw 'Frottage must sound for the whole hold (§18.4).'
}

# 서랍은 만들어진 적이 없다. 표에 되살아나면 다음 사람이 찾다가 시간을 버린다.
$assertionCount++
if ($story -match '홀드 시간:[^\r\n]*서랍') {
	throw 'The §18.4 hold list names a drawer that no interaction implements.'
}

# 되감기와 입력 버퍼.
$rewindRow = [regex]::Match($story, '진행도는 (?<scale>[0-9.]+)배 속도로 되감긴다')
$bufferRow = [regex]::Match($story, '입력 버퍼 (?<ms>[0-9]+)ms')
$assertionCount++
if (-not $rewindRow.Success -or -not $bufferRow.Success) {
	throw 'The §18.4 rewind or buffer row could not be read.'
}
$rewindDeclared = [regex]::Match(
	$interactionHeader, 'float HoldRewindSpeedScale = (?<value>[0-9.]+)f;')
$assertionCount++
if (-not $rewindDeclared.Success) {
	throw 'HoldRewindSpeedScale could not be read.'
}
$assertionCount++
if ([double]$rewindDeclared.Groups['value'].Value -ne [double]$rewindRow.Groups['scale'].Value) {
	throw (
		'HoldRewindSpeedScale is {0} but §18.4 says {1}.' -f
			$rewindDeclared.Groups['value'].Value, $rewindRow.Groups['scale'].Value)
}
$bufferDeclared = [regex]::Match(
	$interactionHeader, 'float InputBufferSeconds = (?<value>[0-9.]+)f;')
$assertionCount++
if (-not $bufferDeclared.Success) {
	throw 'InputBufferSeconds could not be read.'
}
$bufferExpected = [double]$bufferRow.Groups['ms'].Value / 1000.0
$bufferActual = [double]$bufferDeclared.Groups['value'].Value
$assertionCount++
if ([Math]::Abs($bufferActual - $bufferExpected) -gt 0.0005) {
	throw (
		'InputBufferSeconds is {0}s but §18.4 says {1}ms.' -f
			$bufferDeclared.Groups['value'].Value, $bufferRow.Groups['ms'].Value)
}

# 소음은 접촉 프레임에 보고한다. 문이 걸쇠를 넘는 순간이지 문이 다 열린
# 뒤가 아니다.
$assertionCount++
if (-not (Read-ProjectText 'Source/IndieGame/Interaction/IGSwingDoor.cpp').Contains(
	'void AIGSwingDoor::ReportSwingNoise(const float Loudness) const')) {
	throw 'The door must report its noise from a contact-frame helper (§18.4).'
}
$assertionCount++
if ($story -notmatch '\*\*물리 접촉 프레임에\*\*') {
	throw 'The §18.4 contact-frame noise rule was removed.'
}

# --- §18.5 두드리기의 손맛 ---------------------------------------------------
#
# 이 게임의 시그니처 입력이다. 여섯 줄 전부 이미 맞았지만, 맞았다고 두면
# 다음에 누가 옮긴다. 특히 마지막 한 줄은 P4의 소름이 걸려 있다.

# 타격 프레임의 화면 흔들림.
$kickRow = [regex]::Match($story, '화면 미세\s*\r?\n?\s*흔들림 (?<degrees>[0-9.]+)도')
$assertionCount++
if (-not $kickRow.Success) {
	throw 'The §18.5 knock camera kick row could not be read.'
}
$kickDeclared = [regex]::Match(
	$characterSource, 'constexpr float KnockCameraKickDegrees = (?<value>[0-9.]+)f;')
$assertionCount++
if (-not $kickDeclared.Success) {
	throw 'KnockCameraKickDegrees could not be read.'
}
$assertionCount++
if ([double]$kickDeclared.Groups['value'].Value -ne [double]$kickRow.Groups['degrees'].Value) {
	throw (
		'KnockCameraKickDegrees is {0} but §18.5 says {1}.' -f
			$kickDeclared.Groups['value'].Value, $kickRow.Groups['degrees'].Value)
}

# 세 번째 탭 이후의 입력 잠금.
$lockRow = [regex]::Match($story, '세 번째 탭 이후 (?<seconds>[0-9.]+)초 입력 잠금')
$assertionCount++
if (-not $lockRow.Success) {
	throw 'The §18.5 input lock row could not be read.'
}
$lockDeclared = [regex]::Match(
	$characterSource, 'constexpr float KnockInputLockSeconds = (?<value>[0-9.]+)f;')
$assertionCount++
if (-not $lockDeclared.Success) {
	throw 'KnockInputLockSeconds could not be read.'
}
$assertionCount++
if ([double]$lockDeclared.Groups['value'].Value -ne [double]$lockRow.Groups['seconds'].Value) {
	throw (
		'KnockInputLockSeconds is {0} but §18.5 says {1}.' -f
			$lockDeclared.Groups['value'].Value, $lockRow.Groups['seconds'].Value)
}
# 세 번째다. 두 번이나 네 번이면 §7 P4의 「둘-쉬고-하나」와 어긋난다.
$tapBody = [regex]::Match(
	$characterSource,
	'void AIGPlayerCharacter::RegisterKnockSequenceTap\(\)(?<body>[\s\S]*?)\r?\n\}')
$assertionCount++
if (-not $tapBody.Success) {
	throw 'RegisterKnockSequenceTap could not be isolated.'
}
$assertionCount++
if ($tapBody.Groups['body'].Value -notmatch 'KnockSequenceTapCount >= 3') {
	throw 'The lock must arm on the third tap (§18.5).'
}
$assertionCount++
if (-not $tapBody.Groups['body'].Value.Contains(
	'KnockInputLockedUntil = Now + IGPlayerNoise::KnockInputLockSeconds')) {
	throw 'The third tap must actually arm the input lock (§18.5).'
}

# 진동: 내 노크는 0.06s/강도 0.35 단발.
$hapticRow = [regex]::Match(
	$story, '내 노크는 (?<seconds>[0-9.]+)s/강도 (?<intensity>[0-9.]+) 단발')
$assertionCount++
if (-not $hapticRow.Success) {
	throw 'The §18.5 haptic row could not be read.'
}
$feedbackBody = [regex]::Match(
	$characterSource,
	'void AIGPlayerCharacter::ApplyPlayerKnockFeedback\(\)(?<body>[\s\S]*?)\r?\n\}')
$assertionCount++
if (-not $feedbackBody.Success) {
	throw 'ApplyPlayerKnockFeedback could not be isolated.'
}
$expectedHaptic = 'PlayHapticFeedback({0}f, {1}f);' -f `
	$hapticRow.Groups['intensity'].Value, $hapticRow.Groups['seconds'].Value
$assertionCount++
if (-not $feedbackBody.Groups['body'].Value.Contains($expectedHaptic)) {
	throw ('The knock haptic must match §18.5: {0}' -f $expectedHaptic)
}

# **벽에서 돌아오는 응답 노크에는 진동이 없다.** 이 한 줄이 P4의 소름을
# 만든다 — 내 손이 아닌 것이 내 손처럼 느껴지면 그 장면은 끝이다.
foreach ($replyPath in @(
	'Source/IndieGame/Entity/IGListenerEntity.cpp',
	'Source/IndieGame/Audio/IGMissingFloorAudioSubsystem.cpp')) {
	$replySource = Read-ProjectText $replyPath
	foreach ($buzz in @(
		'PlayHapticFeedback',
		'PlayDynamicForceFeedback',
		'ClientPlayForceFeedback')) {
		$assertionCount++
		if ($replySource.Contains($buzz)) {
			throw (
				'The wall''s reply knock must never buzz the pad (§18.5): {0} in {1}' -f
					$buzz, $replyPath)
		}
	}
}
$assertionCount++
if ($story -notmatch '\*\*벽에서 돌아오는 응답 노크에는 진동이\s*\r?\n?\s*없다\*\*') {
	throw 'The §18.5 silent-reply rule was removed.'
}

# 입력→발음 지연 40ms 이하. 타이머를 끼우면 그 자리에서 깨진다.
$latencyRow = [regex]::Match($story, '입력→발음 지연 \*\*(?<ms>[0-9]+)ms 이하\*\*')
$assertionCount++
if (-not $latencyRow.Success) {
	throw 'The §18.5 latency row could not be read.'
}
$knockBody = [regex]::Match(
	$characterSource,
	'void AIGPlayerCharacter::Knock\(\)(?<body>[\s\S]*?)\r?\n\}\r?\n\r?\nbool AIGPlayerCharacter::OfferAnswerKnock')
$assertionCount++
if (-not $knockBody.Success) {
	throw 'Knock could not be isolated.'
}
$knockText = $knockBody.Groups['body'].Value
$assertionCount++
if (-not $knockText.Contains('IGAudio::SpawnOneShotAt(')) {
	throw 'The knock must make its sound in the same call as the input (§18.5).'
}
foreach ($delay in @('SetTimer', 'FTimerDelegate', 'Delay(')) {
	$assertionCount++
	if ($knockText.Contains($delay)) {
		throw "Nothing may sit between the tap and the sound (§18.5): $delay"
	}
}

# 단발 탭만. 자동 연타도, 정답을 대신 쳐 주는 것도 없다.
$assertionCount++
if ($story -notmatch '자동 연타·자동 정답 재생 금지') {
	throw 'The §18.5 no-autofire rule was removed.'
}
foreach ($auto in @('AutoKnock', 'RepeatKnock', 'PlayKnockPattern(')) {
	$assertionCount++
	if ($characterSource.Contains($auto)) {
		throw "The knock is a single tap; nothing may play it for the player (§18.5): $auto"
	}
}

# 실패해도 벽의 잔향만 돌아온다. 실패 문구도 붉은 표시도 없다.
$assertionCount++
if ($story -notmatch '실패 시 실패 문구·붉은 표시 없음') {
	throw 'The §18.5 no-failure-message rule was removed.'
}
$assertionCount++
if ($knockText -match 'PushThought|PushDialogue|PushAudioCaption') {
	throw 'A missed knock must say nothing (§18.5).'
}

# --- §18.6 햅틱 계약 ---------------------------------------------------------
#
# 여섯 줄짜리 표다. 셋은 이미 있었고 셋은 없었다 — CHASE 진입, 심박,
# 낙하물·충돌.

$hapticRows = @{}
foreach ($row in [regex]::Matches(
	$story,
	'(?m)^\| (?<event>[^|]+?) \| (?<intensity>[^|]+?) \| (?<length>[^|]+?) \|\s*$')) {
	$hapticRows[$row.Groups['event'].Value.Trim()] = @{
		Intensity = $row.Groups['intensity'].Value.Trim()
		Length = $row.Groups['length'].Value.Trim()
	}
}
foreach ($event in @(
	'내 노크', '발소리(달리기 한정)', '낙하물·충돌',
	'CHASE 진입', '포획', '심박(스트레스 0.85+)')) {
	$assertionCount++
	if (-not $hapticRows.ContainsKey($event)) {
		throw "The §18.6 haptic table lost a row: $event"
	}
}

# 표의 숫자를 코드 상수와 짝짓는다.
$hapticConstants = @(
	@{ Event = '내 노크'; Call = 'PlayHapticFeedback(0.35f, 0.06f);' },
	@{ Event = '발소리(달리기 한정)'; Call = 'PlayHapticFeedback(0.12f, 0.04f);' })
foreach ($pair in $hapticConstants) {
	$row = $hapticRows[$pair.Event]
	$expected = 'PlayHapticFeedback({0}f, {1}f);' -f `
		$row.Intensity, ($row.Length -replace 's$', '')
	$assertionCount++
	if ($expected -ne $pair.Call) {
		throw (
			'The §18.6 row moved without the code following: {0} wants {1}' -f
				$pair.Event, $expected)
	}
	$assertionCount++
	if (-not $characterSource.Contains($pair.Call)) {
		throw ('The §18.6 haptic is missing: {0}' -f $pair.Event)
	}
}

# 달리기 한정이라고 적혀 있다. 걸을 때도 울리면 표가 거짓말이 된다.
$assertionCount++
if ($characterSource -notmatch
	'if \(bSprinting && !bIsCrouched\)\s*\r?\n\s*\{\s*\r?\n\s*PlayHapticFeedback\(0\.12f, 0\.04f\);') {
	throw 'The footstep haptic is sprint-only (§18.6).'
}

# 나머지 넷은 이름 붙인 상수로 둔다. 표를 옮기면 여기서 잡힌다.
$namedHaptics = @(
	@{ Name = 'ImpactHapticIntensity'; Value = '0.55' },
	@{ Name = 'ImpactHapticSeconds'; Value = '0.20' },
	@{ Name = 'ChaseHapticIntensity'; Value = '0.25' },
	@{ Name = 'ChaseHapticFadeInSeconds'; Value = '0.40' },
	@{ Name = 'HeartbeatHapticIntensity'; Value = '0.10' },
	@{ Name = 'CaptureHapticIntensity'; Value = '0.70' })
foreach ($named in $namedHaptics) {
	$declared = [regex]::Match(
		$characterSource,
		('constexpr float {0} = (?<value>[0-9.]+)f;' -f $named.Name))
	$assertionCount++
	if (-not $declared.Success) {
		throw ('The §18.6 haptic constant is missing: {0}' -f $named.Name)
	}
	$assertionCount++
	if ([double]$declared.Groups['value'].Value -ne [double]$named.Value) {
		throw (
			'{0} is {1} but §18.6 says {2}.' -f
				$named.Name, $declared.Groups['value'].Value, $named.Value)
	}
}

# 표의 문자열과 상수를 맞대 본다. 표만 고치고 코드를 두면 조용히 어긋난다.
$assertionCount++
if ($hapticRows['낙하물·충돌'].Intensity -ne '0.55' -or
	$hapticRows['낙하물·충돌'].Length -ne '0.20s') {
	throw 'The §18.6 impact row moved; the code constants did not follow.'
}
$assertionCount++
if ($hapticRows['CHASE 진입'].Intensity -notmatch '^0\.25' -or
	$hapticRows['CHASE 진입'].Length -notmatch '0\.4s') {
	throw 'The §18.6 chase row moved; the code constants did not follow.'
}
$assertionCount++
if ($hapticRows['포획'].Intensity -notmatch '^0\.70') {
	throw 'The §18.6 capture row moved; the code constants did not follow.'
}
$assertionCount++
if ($hapticRows['심박(스트레스 0.85+)'].Intensity -notmatch '^0\.10') {
	throw 'The §18.6 heartbeat row moved; the code constants did not follow.'
}

# CHASE는 지속이다. 페이드인이 있고, 끝날 때 반드시 멈춘다 — 안 멈추면
# 추격이 끝났는데 패드만 계속 우는 상태가 남는다.
$chaseBody = [regex]::Match(
	$characterSource,
	'void AIGPlayerCharacter::UpdateChaseHaptic\(const float DeltaSeconds\)(?<body>[\s\S]*?)\r?\n\}')
$assertionCount++
if (-not $chaseBody.Success) {
	throw 'UpdateChaseHaptic could not be isolated.'
}
foreach ($piece in @(
	'IGPlayerNoise::ChaseHapticFadeInSeconds',
	'EDynamicForceFeedbackAction::Update',
	'EDynamicForceFeedbackAction::Start')) {
	$assertionCount++
	if (-not $chaseBody.Groups['body'].Value.Contains($piece)) {
		throw "The chase haptic must ramp and hold (§18.6): $piece"
	}
}
$stopBody = [regex]::Match(
	$characterSource,
	'void AIGPlayerCharacter::StopChaseHaptic\(\)(?<body>[\s\S]*?)\r?\n\}')
$assertionCount++
if (-not $stopBody.Success) {
	throw 'StopChaseHaptic could not be isolated.'
}
$assertionCount++
if (-not $stopBody.Groups['body'].Value.Contains(
	'EDynamicForceFeedbackAction::Stop')) {
	throw 'Leaving the chase must stop the sustained buzz (§18.6).'
}
# 진동 전체 끄기를 켜 두었는데 지속 진동만 살아남으면 안 된다.
$assertionCount++
if (-not $chaseBody.Groups['body'].Value.Contains('AreHapticsEnabled()')) {
	throw 'The sustained chase haptic must honour the haptics-off option (§18.6).'
}

# 심박은 소리를 만드는 자리에서 함께 낸다. 임계값도 한 번만 적는다.
$stressSourceForHaptics = Read-ProjectText 'Source/IndieGame/Player/IGStressComponent.cpp'
$beatBody = [regex]::Match(
	$stressSourceForHaptics,
	'void UIGStressComponent::PlayHeartbeat\(const float EffectiveStress\)(?<body>[\s\S]*?)\r?\n\}')
$assertionCount++
if (-not $beatBody.Success) {
	throw 'PlayHeartbeat could not be isolated.'
}
$assertionCount++
if (-not $beatBody.Groups['body'].Value.Contains('PlayHeartbeatHaptic()')) {
	throw 'The heartbeat haptic must fire where the beat is made (§18.6).'
}
$thresholdDeclared = [regex]::Match(
	$stressSourceForHaptics,
	'constexpr float HeartbeatHapticStressThreshold = (?<value>[0-9.]+)f;')
$assertionCount++
if (-not $thresholdDeclared.Success) {
	throw 'HeartbeatHapticStressThreshold could not be read.'
}
$assertionCount++
if ([double]$thresholdDeclared.Groups['value'].Value -ne 0.85) {
	throw (
		'HeartbeatHapticStressThreshold is {0} but §18.6 says 0.85.' -f
			$thresholdDeclared.Groups['value'].Value)
}

# 전체 OFF 옵션이 살아 있는가.
$assertionCount++
if ($story -notmatch '전체 OFF 옵션을 제공하되') {
	throw 'The §18.6 haptics-off promise was removed.'
}
$hapticGate = [regex]::Match(
	$characterSource,
	'void AIGPlayerCharacter::PlayHapticFeedback\((?<body>[\s\S]*?)\r?\n\}')
$assertionCount++
if (-not $hapticGate.Success) {
	throw 'PlayHapticFeedback could not be isolated.'
}
$assertionCount++
if (-not $hapticGate.Groups['body'].Value.Contains('AreHapticsEnabled()')) {
	throw 'Every haptic must pass the off switch (§18.6).'
}

# --- §18.7 조작감 합격식 ------------------------------------------------------
#
# 다섯 줄 중 하나는 사람을 앉혀 놓고 봐야 하는 것이고(초견 5명), 하나는
# 실측이다(입력→소리 지연). 나머지 셋은 여기서 본다. 홀드 편차는 숫자를
# 실제로 돌려 봐야 해서 audit_hold_timing.py가 따로 맡는다.

# 다섯 동사가 독립 액션으로 있는가. 조사의 별칭으로 살면 §24 즉시 차단 16이다.
$verbKeys = @{}
foreach ($mapping in [regex]::Matches(
	$inputConfig,
	'ActionMappings=\(ActionName="(?<action>[^"]+)"[^)]*?Key=(?<key>[A-Za-z_0-9]+)')) {
	$action = $mapping.Groups['action'].Value
	if (-not $verbKeys.ContainsKey($action)) {
		$verbKeys[$action] = @()
	}
	$verbKeys[$action] += $mapping.Groups['key'].Value
}
$acceptanceVerbs = @('Crouch', 'Sprint', 'Knock', 'Listen', 'HoldBreath')
foreach ($verb in $acceptanceVerbs) {
	$assertionCount++
	if (-not $verbKeys.ContainsKey($verb)) {
		throw "The §18.7 acceptance verb has no independent action: $verb"
	}
}
# 조사와 키를 나눠 쓰면 그건 별칭이다.
$interactKeys = $verbKeys['Interact']
foreach ($verb in $acceptanceVerbs) {
	foreach ($key in $verbKeys[$verb]) {
		$assertionCount++
		if ($interactKeys -contains $key) {
			throw "The §18.7 verb shares a key with Interact: $verb on $key"
		}
	}
}

# 패드만으로도, 키보드만으로도 완주할 수 있어야 한다. 장치별로 훑는다.
# 엿듣기는 키보드에서 벽 조준 중 조사 홀드로 대신하는 것이 설계이고,
# 그 예외는 재설정 표의 설명에 적혀 있어야 한다 — 말 없이 비어 있으면
# 빠뜨린 것과 구분되지 않는다.
foreach ($verb in $acceptanceVerbs) {
	$hasPad = @($verbKeys[$verb] | Where-Object { $_ -like 'Gamepad_*' }).Count -gt 0
	$hasKeyboard = @($verbKeys[$verb] | Where-Object { $_ -notlike 'Gamepad_*' }).Count -gt 0
	$assertionCount++
	if (-not $hasPad) {
		throw "The §18.7 pad-only playthrough is broken: $verb has no gamepad key"
	}
	$assertionCount++
	if (-not $hasKeyboard -and $verb -ne 'Listen') {
		throw "The §18.7 keyboard-only playthrough is broken: $verb has no key"
	}
}
$assertionCount++
if ($bindingSource -notmatch 'DescListen[^)]*패드 전용 독립 입력') {
	throw 'The keyboard route for 엿듣기 must stay written down (§18.7).'
}
$assertionCount++
if ($story -notmatch '패드만으로 프롤로그~밤1 완주, 마우스·키보드만으로 동일 완주') {
	throw 'The §18.7 both-devices criterion was removed.'
}
$assertionCount++
if ($story -notmatch '`Interact` 별칭이 남아 있지 않다') {
	throw 'The §18.7 no-alias criterion was removed.'
}

# 홀드 편차는 숫자를 돌려서 본다. 계약은 그 감사가 실제로 걸려 있는지만
# 확인한다 — 스크립트만 있고 아무도 안 부르면 없는 것과 같다.
$validateScript = Read-ProjectText 'Scripts/Validate-Project.ps1'
$assertionCount++
if (-not $validateScript.Contains('Scripts/audit_hold_timing.py')) {
	throw 'The §18.7 hold-timing audit is not wired into validation.'
}
$assertionCount++
if ($story -notmatch '60fps·30fps 양쪽에서 홀드 완료 시간 편차 ±3% 이내') {
	throw 'The §18.7 hold deviation window was removed.'
}

# 사람이 봐야 하는 줄은 자동으로 못 본다. 지워지지만 않게 지킨다.
$assertionCount++
if ($story -notmatch '초견 5명 중 4명 이상이 튜토리얼 텍스트 없이') {
	throw 'The §18.7 playtest criterion was removed.'
}

# --- §19 UI·UX 계약 ----------------------------------------------------------
#
# 화면 계층부터 합격식까지 열한 절이다. 여기서는 숫자와 「없어야 하는 것」을
# 본다. §19.6.1·19.6.2는 각자 자기 자리에서 이미 잠겨 있다.

$settingsLayoutSource = Read-ProjectText 'Source/IndieGame/Player/IGSettingsMenuLayout.h'
$hudSource = Read-ProjectText 'Source/IndieGame/Player/IGHorrorHUD.cpp'

# 19.1 — 밤에는 기록을 열 수 없다. 이 게임 UX의 중심 결정이다.
$assertionCount++
if ($story -notmatch '\*\*열 수 없음\*\*') {
	throw 'The §19.1 night journal lock was removed.'
}
$assertionCount++
if (-not $controllerSource.Contains('지금은 그럴 때가 아니다')) {
	throw 'The §19.1 refusal line is missing.'
}

# 19.2 — 기록은 Tab 홀드로 연다. 스치는 손에 전체 화면이 열리면 안 된다.
$journalRow = [regex]::Match($story, '열기 `Tab` 홀드 (?<seconds>[0-9.]+)s')
$assertionCount++
if (-not $journalRow.Success) {
	throw 'The §19.2 journal hold row could not be read.'
}
$journalDeclared = [regex]::Match(
	$controllerSource,
	'const double JournalHoldSeconds = (?<value>[0-9.]+) \*')
$assertionCount++
if (-not $journalDeclared.Success) {
	throw 'JournalHoldSeconds could not be read.'
}
$assertionCount++
if ([double]$journalDeclared.Groups['value'].Value -ne [double]$journalRow.Groups['seconds'].Value) {
	throw (
		'JournalHoldSeconds is {0} but §19.2 says {1}.' -f
			$journalDeclared.Groups['value'].Value, $journalRow.Groups['seconds'].Value)
}
# 필터·검색·정답 하이라이트는 없다. 빈칸을 보여 주면 세계가 체크리스트가 된다.
$assertionCount++
if ($story -notmatch '필터·검색·정답 하이라이트·미확인 항목 회색 슬롯 없음') {
	throw 'The §19.2 no-checklist rule was removed.'
}
foreach ($banned in @('JournalFilter', 'JournalSearch', 'DrawJournalHighlight')) {
	$assertionCount++
	if ($hudSource.Contains($banned)) {
		throw "The journal must not become a checklist (§19.2): $banned"
	}
}

# 19.4 — 낮의 힌트는 정답이 아니라 한 줄이다.
$assertionCount++
if (-not $controllerSource.Contains('401호에 물어볼 수 있다')) {
	throw 'The §19.4 daytime hint line is missing.'
}

# 19.5 — 기상 연출이 짧아지는 것 자체가 정보다.
$wakeRow = [regex]::Match(
	$story,
	'기상 연출 길이: 1회차 (?<first>[0-9.]+)s, 2회차 (?<second>[0-9.]+)s, 3~4회차 (?<third>[0-9.]+)s, 5회차부터 (?<fifth>[0-9.]+)s')
$assertionCount++
if (-not $wakeRow.Success) {
	throw 'The §19.5 wake ladder could not be read.'
}
$loopSource = Read-ProjectText 'Source/IndieGame/Entity/IGNightLoopDirector.cpp'
$wakeBody = [regex]::Match(
	$loopSource,
	'float AIGNightLoopDirector::GetWakeFadeInSeconds\(\) const(?<body>[\s\S]*?)\r?\n\}')
$assertionCount++
if (-not $wakeBody.Success) {
	throw 'GetWakeFadeInSeconds could not be isolated.'
}
$wakeSteps = @()
foreach ($step in [regex]::Matches(
	$wakeBody.Groups['body'].Value, 'return (?<value>[0-9.]+)f;')) {
	$wakeSteps += [double]$step.Groups['value'].Value
}
$expectedWake = @(
	[double]$wakeRow.Groups['first'].Value,
	[double]$wakeRow.Groups['second'].Value,
	[double]$wakeRow.Groups['third'].Value,
	[double]$wakeRow.Groups['fifth'].Value)
$assertionCount++
if ($wakeSteps.Count -ne $expectedWake.Count) {
	throw (
		'The wake ladder has {0} steps but §19.5 lists {1}.' -f
			$wakeSteps.Count, $expectedWake.Count)
}
for ($index = 0; $index -lt $expectedWake.Count; $index++) {
	$assertionCount++
	if ($wakeSteps[$index] -ne $expectedWake[$index]) {
		throw (
			'Wake step {0} is {1}s but §19.5 says {2}s.' -f
				$index, $wakeSteps[$index], $expectedWake[$index])
	}
}
# 짧아지기만 해야 한다. 중간이 길어지면 「그가 성급해졌다」가 뒤집힌다.
for ($index = 1; $index -lt $wakeSteps.Count; $index++) {
	$assertionCount++
	if ($wakeSteps[$index] -ge $wakeSteps[$index - 1]) {
		throw 'The wake ladder must only ever get shorter (§19.5).'
	}
}
# 다섯 번째 포획에만 문 아래 메모가 온다.
$mercyRow = [regex]::Match($story, '(?<count>[0-9]+)회 연속 포획에 한해')
$assertionCount++
if (-not $mercyRow.Success) {
	throw 'The §19.5 mercy-note threshold could not be read.'
}
$mercyDeclared = [regex]::Match(
	$loopSource, 'constexpr int32 MercyNoteCaptureThreshold = (?<value>[0-9]+);')
$assertionCount++
if (-not $mercyDeclared.Success) {
	throw 'MercyNoteCaptureThreshold could not be read.'
}
$assertionCount++
if ([int]$mercyDeclared.Groups['value'].Value -ne [int]$mercyRow.Groups['count'].Value) {
	throw (
		'MercyNoteCaptureThreshold is {0} but §19.5 says {1}.' -f
			$mercyDeclared.Groups['value'].Value, $mercyRow.Groups['count'].Value)
}
# 게임오버 화면·재시도 버튼·사망 카운터는 없다.
foreach ($banned in @('GameOver', 'RetryButton', 'DeathCount')) {
	$assertionCount++
	if ($hudSource.Contains($banned)) {
		throw "A reset is not a failure screen (§19.5): $banned"
	}
}

# 19.7 — 화면 설정은 명시적으로 적용한 뒤 10초 확인한다.
$confirmRow = [regex]::Match($story, '(?<seconds>[0-9]+)초\s*\r?\n?\s*확인하며, 응답이 없으면')
$assertionCount++
if (-not $confirmRow.Success) {
	throw 'The §19.7 confirmation window could not be read.'
}
$assertionCount++
if ($controllerSource -notmatch
	('DisplayConfirmationSecondsRemaining = {0};' -f $confirmRow.Groups['seconds'].Value)) {
	throw ('The display confirmation window must be {0}s (§19.7).' -f $confirmRow.Groups['seconds'].Value)
}
# 확인 창은 「이 설정 유지」에 커서를 둔다. 행을 하나 끼웠더니 조작 행을
# 가리키고 있었다 — 그래서 번호 대신 이름으로 적는다.
$assertionCount++
if (-not $controllerSource.Contains(
	'DisplaySettingsSelection = IGSettingsMenuLayout::ApplyOrKeep;')) {
	throw 'The confirmation window must land on the keep row (§19.7).'
}
$assertionCount++
if ($controllerSource -match 'DisplaySettingsSelection == [0-9]') {
	throw 'Display rows must be compared by name, not by number (§19.7).'
}
$assertionCount++
if ($settingsLayoutSource -notmatch 'enum EDisplayRow : int32') {
	throw 'The display row names are missing (§19.7).'
}
$assertionCount++
if ($settingsLayoutSource -notmatch 'BackOrRevert \+ 1 == DisplayRowCount') {
	throw 'The display row names must stay tied to the row count (§19.7).'
}
# 720p에서도 포인터 행이 44px 아래로 내려가지 않는다.
$rowHeightRow = [regex]::Match(
	$story, '포인터 행 높이는 (?<pixels>[0-9]+)px 미만으로 줄이지 않는다')
$assertionCount++
if (-not $rowHeightRow.Success) {
	throw 'The §19.7 minimum row height could not be read.'
}
foreach ($field in @('CategoryRowHeight', 'OptionRowHeight')) {
	$declared = [regex]::Match(
		$settingsLayoutSource,
		('Result.{0} = FMath::Max\((?<floor>[0-9.]+)f,' -f $field))
	$assertionCount++
	if (-not $declared.Success) {
		throw ('The settings row floor is missing: {0}' -f $field)
	}
	$assertionCount++
	if ([double]$declared.Groups['floor'].Value -lt [double]$rowHeightRow.Groups['pixels'].Value) {
		throw (
			'{0} can fall to {1}px but §19.7 says at least {2}px.' -f
				$field, $declared.Groups['floor'].Value, $rowHeightRow.Groups['pixels'].Value)
	}
}

# 19.8 — 파문 링은 동작 감소에서도 끄지 않는다. 연출이 아니라 정보다.
$assertionCount++
if ($story -notmatch '소음 파문 링은 동작 감소에서도 \*\*끄지 않는다\*\*') {
	throw 'The §19.8 ripple rule was removed.'
}

# --- §20 난이도 설계 ----------------------------------------------------------
#
# §20.2 튜닝 테이블은 자기 계약이 따로 본다. 여기서는 안전망 셋과 난이도
# 네 모드를 본다. 둘 다 문서가 숫자를 적어 둔 자리다.

$mercyHeader = Read-ProjectText 'Source/IndieGame/Entity/IGMissingFloorMercyDirector.h'
$mercySource = Read-ProjectText 'Source/IndieGame/Entity/IGMissingFloorMercyDirector.cpp'
$tuningSource = Read-ProjectText 'Source/IndieGame/Entity/IGListenerTuning.cpp'

# 20.3-1 — 2회 연속 리셋에 환경 힌트 하나.
$resetRow = [regex]::Match(
	$story, '\*\*관찰 재료 증가\*\*[^\r\n]*?(?<count>[0-9]+)회 연속 리셋 시 환경 힌트 (?<hints>[0-9]+)개')
$assertionCount++
if (-not $resetRow.Success) {
	throw 'The §20.3-1 reset-hint row could not be read.'
}
$resetDeclared = [regex]::Match(
	$mercyHeader,
	'static constexpr int32 ResetsForEnvironmentHint = (?<value>[0-9]+);')
$assertionCount++
if (-not $resetDeclared.Success) {
	throw 'ResetsForEnvironmentHint could not be read.'
}
$assertionCount++
if ([int]$resetDeclared.Groups['value'].Value -ne [int]$resetRow.Groups['count'].Value) {
	throw (
		'ResetsForEnvironmentHint is {0} but §20.3 says {1}.' -f
			$resetDeclared.Groups['value'].Value, $resetRow.Groups['count'].Value)
}
# 새 출처를 얻으면 세는 것이 처음으로 돌아가야 한다. 안 그러면 잘 하고 있는
# 플레이어에게도 언젠가 힌트가 켜진다. 같은 줄이 여러 곳에 있으므로 새 출처를
# 확인하는 자리에서 함께 도는지를 본다.
$assertionCount++
if ($mercySource -notmatch
	'SourceCount != LastSourceCount\)[\s\S]{0,400}?LastSourceCount = SourceCount;\s*\r?\n\s*StuckSeconds = 0\.0f;\s*\r?\n\s*ResetsSinceNewSource = 0;') {
	throw 'Learning something must stand both nets down together (§20.3-1).'
}

# 20.3-2 — 새 출처 없이 90초가 지나면 세계가 먼저 움직인다.
$stuckRow = [regex]::Match(
	$story, '새 출처 없이 (?<seconds>[0-9]+)초가 지나면')
$assertionCount++
if (-not $stuckRow.Success) {
	throw 'The §20.3-2 ninety-second row could not be read.'
}
$stuckDeclared = [regex]::Match(
	$mercyHeader,
	'static constexpr float StuckResponseSeconds = (?<value>[0-9.]+)f;')
$assertionCount++
if (-not $stuckDeclared.Success) {
	throw 'StuckResponseSeconds could not be read.'
}
$assertionCount++
if ([double]$stuckDeclared.Groups['value'].Value -ne [double]$stuckRow.Groups['seconds'].Value) {
	throw (
		'StuckResponseSeconds is {0} but §20.3 says {1}.' -f
			$stuckDeclared.Groups['value'].Value, $stuckRow.Groups['seconds'].Value)
}
# 메뉴를 열어 둔 것은 막힌 것이 아니다. 시계가 거기서 멈춰야 한다(§19.7).
$assertionCount++
if (-not $mercySource.Contains('World->IsPaused()')) {
	throw 'The stuck clock must stop while the game is paused (§20.3-2).'
}

# 안전망은 볼 곳을 줄 뿐 답을 말하지 않는다.
$assertionCount++
if ($story -notmatch '\*\*막힌 플레이어에게 주는 것은\s*\r?\n?\s*답이 아니라 볼 곳이다\.\*\*') {
	throw 'The §20.3 no-answers rule was removed.'
}

# --- §20.4 난이도 네 모드 -----------------------------------------------------
#
# 이름부터 세계의 언어다. 「쉬움/보통/어려움」으로 부르지 않는다.
$assertionCount++
if ($story -notmatch '"쉬움/보통/어려움"이라 부르지 않는다') {
	throw 'The §20.4 naming rule was removed.'
}
foreach ($mode in @('조용한 밤', '성급한 밤', '듣기만 하는 밤')) {
	$assertionCount++
	if (-not $tuningSource.Contains($mode) -and -not $mercyHeader.Contains($mode)) {
		$tuningHeader = Read-ProjectText 'Source/IndieGame/Entity/IGListenerTuning.h'
		if (-not $tuningHeader.Contains($mode)) {
			throw "The §20.4 difficulty mode is missing: $mode"
		}
	}
}

# 조용한 밤의 세 배율. 문서가 표에 적어 둔 값 그대로다.
$quietRow = [regex]::Match(
	$story,
	'청취 반경 ×(?<hearing>[0-9.]+), CHASE 속도 ×(?<chase>[0-9.]+), WAITING 시간 ×(?<wait>[0-9.]+)')
$assertionCount++
if (-not $quietRow.Success) {
	throw 'The §20.4 quiet-night row could not be read.'
}
$quietBody = [regex]::Match(
	$tuningSource,
	'case EIGNightDifficulty::Quiet:(?<body>[\s\S]*?)break;')
$assertionCount++
if (-not $quietBody.Success) {
	throw 'The quiet-night branch could not be isolated.'
}
foreach ($pair in @(
	@{ Line = ('Tuning.HearingSensitivity *= {0}f;' -f $quietRow.Groups['hearing'].Value); Name = '청취 반경' },
	@{ Line = ('Tuning.ChaseSpeed *= {0}f;' -f $quietRow.Groups['chase'].Value); Name = 'CHASE 속도' },
	@{ Line = ('Tuning.WaitScale = {0}f;' -f $quietRow.Groups['wait'].Value); Name = 'WAITING 시간' })) {
	$assertionCount++
	if (-not $quietBody.Groups['body'].Value.Contains($pair.Line)) {
		throw ('The quiet night must follow §20.4: {0} ({1})' -f $pair.Name, $pair.Line)
	}
}

# 성급한 밤의 세 축.
$hastyRow = [regex]::Match(
	$story,
	'티어 초기값 (?<tier>[0-9]+), 히트맵 가중 \+(?<heatmap>[0-9.]+), LISTENING −(?<listen>[0-9]+)s')
$assertionCount++
if (-not $hastyRow.Success) {
	throw 'The §20.4 hasty-night row could not be read.'
}
$hastyBody = [regex]::Match(
	$tuningSource,
	'case EIGNightDifficulty::Hasty:(?<body>[\s\S]*?)break;')
$assertionCount++
if (-not $hastyBody.Success) {
	throw 'The hasty-night branch could not be isolated.'
}
$assertionCount++
if (-not $hastyBody.Groups['body'].Value.Contains(
	('Tuning.HeatmapWeight + {0}f' -f $hastyRow.Groups['heatmap'].Value))) {
	throw 'The hasty night must raise the heatmap weight by the §20.4 amount.'
}
$assertionCount++
if (-not $hastyBody.Groups['body'].Value.Contains(
	('NightListenWindow[Night] - {0}.0f' -f $hastyRow.Groups['listen'].Value))) {
	throw 'The hasty night must shorten LISTENING by the §20.4 amount.'
}

# 듣기만 하는 밤: 포획도 추격도 없다. 매복도 함께 꺼진다 — 잡을 수 없는
# 매복은 압박이 아니라 연출이다.
$listenBody = [regex]::Match(
	$tuningSource,
	'case EIGNightDifficulty::ListenOnly:(?<body>[\s\S]*?)break;')
$assertionCount++
if (-not $listenBody.Success) {
	throw 'The listen-only branch could not be isolated.'
}
foreach ($off in @(
	'Tuning.bChaseEnabled = false;',
	'Tuning.bCaptureEnabled = false;')) {
	$assertionCount++
	if (-not $listenBody.Groups['body'].Value.Contains($off)) {
		throw "The listen-only night must not chase or capture (§20.4): $off"
	}
}
# 포획이 없으니 엔딩 C의 보통 경로가 영영 안 열린다. 대체 경로가 있어야
# §20.5의 「모든 모드에서 엔딩 셋 도달」이 성립한다.
$nightFourHeader = Read-ProjectText 'Source/IndieGame/Entity/IGMissingFloorNightFourDirector.h'
$assertionCount++
if (-not $nightFourHeader.Contains('bool ResolveDawnFailureEnding();')) {
	throw 'The listen-only night needs its substitute route to ending C (§20.4).'
}
$assertionCount++
if ($story -notmatch '엔딩 C의 도달 조건을 \*\*밤4의 05:30\s*\r?\n?\s*벽 미개방\*\*으로 대체한다') {
	throw 'The §20.4 substitute ending-C condition was removed.'
}
$assertionCount++
if ($story -notmatch '「듣기만 하는 밤」으로 진실 10개 전부 확정 가능, 엔딩 A·B·C 전부 도달 가능') {
	throw 'The §20.5 all-modes-reachable criterion was removed.'
}

# 마이크는 난이도가 아니다. 밸런스 기준은 항상 꺼진 상태다.
$assertionCount++
if ($story -notmatch '\*\*마이크 모드는 난이도가 아니다\*\*') {
	throw 'The §20.4 microphone rule was removed.'
}
$accessibilityHeaderForMic = Read-ProjectText 'Source/IndieGame/Accessibility/IGAccessibilitySubsystem.h'
$assertionCount++
if ($accessibilityHeaderForMic -notmatch 'bool bMicrophoneNoiseEnabled = false;') {
	throw 'The microphone must stay off by default; it is the balance baseline (§20.4).'
}

# --- 합격식 현황표 ------------------------------------------------------------
#
# 바이블의 네 합격식은 스무 줄이고, MISSING_FLOOR_ACCEPTANCE.md가 그 스무 줄을
# 표로 다시 센다. 두 자리가 어긋나면 「자동으로 못 보는 줄」이 조용히 사라진다 —
# 그게 이 문서가 막으려는 유일한 사고다.

$acceptanceDoc = Read-ProjectText 'Docs/MISSING_FLOOR_ACCEPTANCE.md'
$bibleLineCount = 0
foreach ($section in @('18.7', '19.9', '20.5', '21.5')) {
	$body = [regex]::Match(
		$story,
		'### ' + [regex]::Escape($section) +
			'[^\r\n]*\r?\n(?<body>[\s\S]*?)(?=\r?\n###|\r?\n---|\r?\n## )')
	$assertionCount++
	if (-not $body.Success) {
		throw "The §$section acceptance section could not be read."
	}
	# 최상위 항목만 센다. 들여쓴 줄은 같은 줄의 부연이다.
	$items = [regex]::Matches($body.Groups['body'].Value, '(?m)^- ')
	$assertionCount++
	if ($items.Count -lt 1) {
		throw "The §$section acceptance section lost every line."
	}
	$bibleLineCount += $items.Count
}

# 표의 행을 센다. 각 절의 표에서 머리글과 정렬 줄을 뺀 나머지다.
$docLineCount = 0
foreach ($section in @('18.7', '19.9', '20.5', '21.5')) {
	$table = [regex]::Match(
		$acceptanceDoc,
		'## §' + [regex]::Escape($section) +
			'[^\r\n]*\r?\n(?<body>[\s\S]*?)(?=\r?\n## |\r?\n---)')
	$assertionCount++
	if (-not $table.Success) {
		throw "The acceptance sheet is missing §$section."
	}
	$rows = [regex]::Matches(
		$table.Groups['body'].Value, '(?m)^\| (?!줄 \|)(?!---)[^|]+\|')
	$docLineCount += $rows.Count
}
$assertionCount++
if ($docLineCount -ne $bibleLineCount) {
	throw (
		'The acceptance sheet lists {0} lines but the bible has {1}.' -f
			$docLineCount, $bibleLineCount)
}

# 표가 「사람」을 지우고 전부 계약으로 바꿔 놓으면, 아직 못 본 것을 봤다고
# 적은 것이 된다. 사람이 필요한 줄이 남아 있는지 본다.
# 표에서 「사람」을 하나씩 계약으로 바꿔 놓으면, 아직 못 본 것을 봤다고
# 적은 것이 된다. 표가 스스로 밝힌 수와 실제 행 수를 맞대 본다.
$humanRows = ([regex]::Matches(
	$acceptanceDoc, '(?m)^\|[^|]+\|[^|]*\*\*사람\*\*[^|]*\|')).Count
$assertionCount++
if ($humanRows -lt 1) {
	throw 'The acceptance sheet must keep naming the lines a person has to check.'
}
$statedHuman = [regex]::Match($acceptanceDoc, '나머지 \*\*(?<count>[^*]+)\*\*은 사람이 필요하다')
$assertionCount++
if (-not $statedHuman.Success) {
	throw 'The acceptance sheet no longer says how many lines need a person.'
}
$humanWords = @{ '열둘' = 12; '열셋' = 13; '열넷' = 14; '열다섯' = 15 }
$assertionCount++
if (-not $humanWords.ContainsKey($statedHuman.Groups['count'].Value)) {
	throw 'The acceptance sheet states a count this check cannot read.'
}
$assertionCount++
if ($humanRows -ne $humanWords[$statedHuman.Groups['count'].Value]) {
	throw (
		'The sheet says {0} lines need a person but {1} rows are marked.' -f
			$statedHuman.Groups['count'].Value, $humanRows)
}
$assertionCount++
if ($acceptanceDoc -notmatch '자동으로 볼 수 없다는 것과\s*\r?\n?안 봐도 된다는 것은 다르다') {
	throw 'The acceptance sheet lost the rule that keeps its own rows honest.'
}

# 감사를 이름으로 걸어 두었으니 그 감사가 실제로 있어야 한다.
$assertionCount++
if (-not (Test-Path (Join-Path $projectRoot 'Scripts/audit_hold_timing.py'))) {
	throw 'The acceptance sheet names an audit that does not exist.'
}

# §19.9는 4K까지 이름을 댄다. 레이아웃 검증이 거기까지 가는지 본다.
$assertionCount++
if ($story -notmatch '720p·1080p·1440p·4K에서 파문 링과 브래킷이 안전 영역 안에 들어온다') {
	throw 'The §19.9 resolution criterion was removed.'
}
$accessibilityContract = Read-ProjectText 'Scripts/Test-Rebirth-AccessibilityContract.ps1'
$assertionCount++
if ($accessibilityContract -notmatch 'Width = 3840\.0; Height = 2160\.0') {
	throw 'The layout check must reach 4K, which §19.9 names.'
}

# --- §22 재미의 구조 ----------------------------------------------------------
#
# 이 절은 대부분 설계 철학이지만, 「만들지 않기로 한 것」이 구체적이다.
# 없는 것으로 지켜지는 규칙은 생기는 순간 조용히 깨진다.

$sceneSource = Read-ProjectText 'Source/IndieGame/Core/IGPrologueWorldScene.cpp'
$nightFourSource = Read-ProjectText 'Source/IndieGame/Entity/IGMissingFloorNightFourDirector.cpp'
$frontendLayout = Read-ProjectText 'Source/IndieGame/Player/IGFrontendMenuLayout.h'

# 22.3 — 수집률 UI·업적 팝업·완료 퍼센트 없음.
$assertionCount++
if ($story -notmatch '수집률 UI·업적 팝업·완료 퍼센트 없음') {
	throw 'The §22.3 no-collection-UI rule was removed.'
}
foreach ($banned in @(
	'CollectionRate', 'AchievementPopup', 'CompletionPercent',
	'DrawCollectionProgress')) {
	$assertionCount++
	if ($hudSource.Contains($banned)) {
		throw "Discoveries are witnessed, not collected (§22.3): $banned"
	}
}
# 무엇을 놓쳤는지 알려 주지 않는다. 회색 슬롯이 곧 체크리스트다.
$assertionCount++
if ($story -notmatch '무엇을 놓쳤는지 알려 주지 않는다') {
	throw 'The §22.3 no-missing-list rule was removed.'
}

# 발견의 보상은 문장이다. 문서가 인용한 줄이 실제로 나가는 줄이어야 한다.
$rewardRow = [regex]::Match(
	$story,
	'약봉투를 봤다면 밤4의\s*\r?\n?\s*대치에서 유담이 한 줄을 더 말한다: "(?<line>[^"]+)"')
$assertionCount++
if (-not $rewardRow.Success) {
	throw 'The §22.3 reward line could not be read.'
}
# 문서는 줄을 접어 적고 코드는 문자열을 이어 붙인다. 공백을 지우고 견준다.
$documentedLine = $rewardRow.Groups['line'].Value -replace '\s', ''
$pillsBody = [regex]::Match(
	$nightFourSource,
	'EIGMissingFloorWitness::SeoSleepingPills,(?<body>[\s\S]{0,600}?)\r?\n\t\t\},')
$assertionCount++
if (-not $pillsBody.Success) {
	throw 'The sleeping-pills reply could not be isolated.'
}
# 앞의 둘은 NSLOCTEXT의 네임스페이스와 키다. 대사는 그 뒤부터다.
$codeLine = (
	[regex]::Matches($pillsBody.Groups['body'].Value, '"(?<part>[^"]*)"') |
		Select-Object -Skip 2 |
		ForEach-Object { $_.Groups['part'].Value }) -join ''
$codeLine = $codeLine -replace '\s', ''
$assertionCount++
if ($codeLine -ne $documentedLine) {
	throw (
		'The §22.3 reward line differs from what night four says: doc "{0}" code "{1}"' -f
			$documentedLine, $codeLine)
}
# 그 줄이 목격에 걸려 있어야 보상이지, 늘 나오면 보상이 아니다.
$assertionCount++
if (-not $nightFourSource.Contains('BuildConfrontationReplyLines')) {
	throw 'The extra line must hang off the witness list (§22.3).'
}

# 403호 정사와 404 이스터에그. 문패 셋과 벽의 메모.
# 재질 목록에만 이름이 있는 것과 문에 실제로 붙는 것은 다르다. 이웃 둘은
# 배치 배열에서, 403은 자기 자리를 그리는 호출에서 확인한다.
$neighbourPlates = [regex]::Match(
	$sceneSource,
	'const TCHAR\* NeighborPlates\[\] = \{(?<body>[^}]*)\};')
$assertionCount++
if (-not $neighbourPlates.Success) {
	throw 'The §22.3 neighbour nameplates are not placed.'
}
foreach ($plate in @('M_Plate401', 'M_Plate402')) {
	$assertionCount++
	if (-not $neighbourPlates.Groups['body'].Value.Contains($plate)) {
		throw "The §22.3 neighbour nameplate is missing: $plate"
	}
}
$assertionCount++
if ($sceneSource -notmatch 'TexMat\(TEXT\("M_Plate403"\)') {
	throw 'The §22.3 403 nameplate is not drawn on its own door.'
}
$assertionCount++
if (-not $sceneSource.Contains('M_Note404NotFound')) {
	throw 'The §22.3 404 memo is missing.'
}
# 상호작용·윤곽선·자막·업적·기록 카드·독백·효과음·별도 광원 전부 없다.
# 그래서 이 소품이 §0의 숫자 절제를 깨는 공포 기호가 되지 않는다.
$eggBlock = [regex]::Match(
	$sceneSource,
	'M_Note404NotFound(?<body>[\s\S]{0,900}?)\r?\n\t\}')
$assertionCount++
if (-not $eggBlock.Success) {
	throw 'The 404 memo block could not be isolated.'
}
foreach ($banned in @(
	'SetCollisionProfileName(UCollisionProfile::BlockAll',
	'PushThought', 'PushAudioCaption', 'SpawnOneShotAt',
	'PointLightComponent', 'AIGInteractable')) {
	$assertionCount++
	if ($eggBlock.Groups['body'].Value.Contains($banned)) {
		throw "The 404 memo stays a background prop (§22.3): $banned"
	}
}
$assertionCount++
if ($story -notmatch '상호작용·윤곽선·자막·\s*\r?\n?\s*업적·기록 카드·독백·효과음·별도 광원은 없고') {
	throw 'The §22.3 easter-egg restraint list was removed.'
}

# 22.4 — 「밤 5」는 엔딩 B를 본 세이브에만, 타이틀에서만 보인다.
$assertionCount++
if ($story -notmatch '「밤 5」 슬롯\(엔딩 B 한정, §9\)') {
	throw 'The §22.4 night-five gate was removed.'
}
$assertionCount++
if ($frontendLayout -notmatch '밤 5는 타이틀에서만, 그리고 엔딩 B를 본 세이브가 있을 때만 보인다') {
	throw 'The night-five gate must stay written where the menu is laid out (§22.4).'
}
# 선택 직전 자동 저장이 있어야 양쪽을 보는 비용이 낮다.
$assertionCount++
if ($story -notmatch '선택 직전 자동 저장이 있어') {
	throw 'The §22.4 pre-choice autosave promise was removed.'
}
# 정의만 남고 호출이 사라지면 이름은 그대로인데 저장은 안 된다. 부르는
# 자리를 따로 센다 — 정의 한 번, 호출 한 번 이상.
$autosaveUses = ([regex]::Matches(
	$nightFourSource, 'RequestEndingChoiceAutosave\(\)')).Count
$assertionCount++
if ($autosaveUses -lt 2) {
	throw 'The ending choice must actually call its autosave, not just declare it (§22.4).'
}

# 2회차 전용 컷·숨겨진 층·진 엔딩은 만들지 않는다.
$assertionCount++
if ($story -notmatch '2회차 전용 컷·숨겨진 층·진 엔딩은 만들지 않는다') {
	throw 'The §22.4 no-new-game-plus rule was removed.'
}
foreach ($banned in @('NewGamePlus', 'TrueEnding', 'HiddenFloor')) {
	$assertionCount++
	if ($nightFourSource.Contains($banned) -or $hudSource.Contains($banned)) {
		throw "This story ends once (§22.4): $banned"
	}
}

# --- §23 몰입 계약 ------------------------------------------------------------
#
# 열 줄짜리 금지 표다. 「몰입을 만드는 것은 추가가 아니라 제거」이므로 여기
# 걸리는 것은 전부 **없어야** 지켜진다. 없는 것은 grep으로 지킬 수 없으니
# 생기면 거절하는 쪽으로 건다.

$immersionRows = @()
$immersionTable = [regex]::Match(
	$story,
	'## 23\. 몰입 계약[^\r\n]*\r?\n(?<body>[\s\S]*?)\r?\n\r?\n몰입을 만드는 것은')
$assertionCount++
if (-not $immersionTable.Success) {
	throw 'The §23 prohibition table could not be read.'
}
foreach ($row in [regex]::Matches(
	$immersionTable.Groups['body'].Value, '(?m)^\| (?<ban>[^|]+?) \| (?<why>[^|]+?) \|\s*$')) {
	$ban = $row.Groups['ban'].Value.Trim()
	if ($ban -eq '금지' -or $ban -match '^-+$') {
		continue
	}
	$immersionRows += $ban
}
$assertionCount++
if ($immersionRows.Count -ne 10) {
	throw (
		'The §23 table has {0} rows; it is supposed to have ten.' -f
			$immersionRows.Count)
}
# 표의 각 줄이 실제로 그 문장인지도 본다. 하나를 조용히 갈아 끼우면 나머지
# 검사들이 무엇을 지키는지 알 수 없게 된다.
foreach ($expected in @(
	'포획 리셋·낮밤 전환의 로딩 화면',
	'튜토리얼 팝업·키 안내 오버레이',
	'퍼센트·게이지·카운터',
	'업적 토스트',
	'사망 카운터·재시도 버튼',
	'자동 저장 아이콘 상시 표시',
	'밤 중 기록 열람',
	'밤 중 `F9` 즉시 로드',
	'존재를 설명하는 컷신',
	'상표·실존 인물·실존 사건')) {
	$assertionCount++
	if ($immersionRows -notcontains $expected) {
		throw "The §23 table lost a row: $expected"
	}
}
$assertionCount++
if ($story -notmatch '몰입을 만드는 것은 추가가 아니라 \*\*제거\*\*다') {
	throw 'The §23 first principle was removed.'
}

# 화면에 생기면 안 되는 것들. 이름이 하나라도 나타나면 표가 거짓이 된다.
foreach ($banned in @(
	@{ Symbol = 'DrawLoadingScreen'; Row = '로딩 화면' },
	@{ Symbol = 'ShowLoadingScreen'; Row = '로딩 화면' },
	@{ Symbol = 'DrawTutorialPopup'; Row = '튜토리얼 팝업' },
	@{ Symbol = 'DrawKeyPromptOverlay'; Row = '키 안내 오버레이' },
	@{ Symbol = 'DrawProgressGauge'; Row = '퍼센트·게이지·카운터' },
	@{ Symbol = 'DrawAchievementToast'; Row = '업적 토스트' },
	@{ Symbol = 'DrawDeathCounter'; Row = '사망 카운터' },
	@{ Symbol = 'DrawEntityCutscene'; Row = '존재를 설명하는 컷신' })) {
	$assertionCount++
	if ($hudSource.Contains($banned.Symbol)) {
		throw (
			'§23 forbids this and the HUD grew it: {0} ({1})' -f
				$banned.Symbol, $banned.Row)
	}
}

# 자동 저장 아이콘은 상시 표시가 금지된 것이지 표시 자체가 금지된 것은
# 아니다. §19.7이 점 하나 0.8초를 약속했고, 그게 없으면 수동 슬롯도 없는
# 게임에서 저장됐는지 물어볼 데가 없다.
$dotRow = [regex]::Match(
	$story, '저장\s*\r?\n?\s*표시는 화면 구석 점 하나 (?<seconds>[0-9.]+)초')
$assertionCount++
if (-not $dotRow.Success) {
	throw 'The §19.7 save dot could not be read.'
}
$dotDeclared = [regex]::Match(
	$hudSource, 'constexpr double SaveIndicatorSeconds = (?<value>[0-9.]+);')
$assertionCount++
if (-not $dotDeclared.Success) {
	throw 'SaveIndicatorSeconds could not be read.'
}
$assertionCount++
if ([double]$dotDeclared.Groups['value'].Value -ne [double]$dotRow.Groups['seconds'].Value) {
	throw (
		'The save dot lasts {0}s but §19.7 says {1}s.' -f
			$dotDeclared.Groups['value'].Value, $dotRow.Groups['seconds'].Value)
}
# 저장이 성공했을 때만 남는다. 실패는 이미 문장으로 말하고 있다.
$saveHandler = [regex]::Match(
	$controllerSource,
	'void AIGPlayerController::HandleSaveCompleted\((?<body>[\s\S]*?)\r?\n\}')
$assertionCount++
if (-not $saveHandler.Success) {
	throw 'HandleSaveCompleted could not be isolated.'
}
$assertionCount++
if (-not $saveHandler.Groups['body'].Value.Contains('ShowSaveIndicator()')) {
	throw 'A successful save must leave its mark (§19.7).'
}
# 사라져야 상시 표시가 아니다.
$dotDraw = [regex]::Match(
	$hudSource,
	'void AIGHorrorHUD::DrawSaveIndicator\(const double CurrentTime\)(?<body>[\s\S]*?)\r?\n\}')
$assertionCount++
if (-not $dotDraw.Success) {
	throw 'DrawSaveIndicator could not be isolated.'
}
$assertionCount++
if (-not $dotDraw.Groups['body'].Value.Contains('CurrentTime >= SaveIndicatorEndTime')) {
	throw 'The save dot must go away; §23 forbids an always-on icon.'
}

# --- §24 즉시 차단 22개 -------------------------------------------------------
#
# 이 절이 스스로 경고한 것이 있다. 「선언과 구현이 갈라져도 아무도 모르는
# 상태가 결함 자체보다 위험하다」. 실제로 잠금 표가 스물둘 중 열여섯만 세고
# 있었고, 빠진 여섯 중 넷은 이미 계약이 보고 있었는데 표만 몰랐다.

# 차단 항목이 스물둘인가.
$blockerList = [regex]::Match(
	$story,
	'### 즉시 차단 22개\r?\n(?<body>[\s\S]*?)\r?\n### 즉시 차단 항목이 잠긴 자리')
$assertionCount++
if (-not $blockerList.Success) {
	throw 'The §24 blocker list could not be read.'
}
$blockerNumbers = @()
foreach ($item in [regex]::Matches(
	$blockerList.Groups['body'].Value, '(?m)^(?<number>[0-9]+)\. ')) {
	$blockerNumbers += [int]$item.Groups['number'].Value
}
$assertionCount++
if ($blockerNumbers.Count -ne 22) {
	throw (
		'The §24 list has {0} blockers; the heading says 22.' -f
			$blockerNumbers.Count)
}
# 번호가 1부터 22까지 빠짐없이 이어지는가. 하나를 지우면 뒤가 당겨져
# 세는 것만으로는 못 잡는다.
for ($index = 0; $index -lt 22; $index++) {
	$assertionCount++
	if ($blockerNumbers[$index] -ne ($index + 1)) {
		throw (
			'The §24 blocker numbering breaks at {0}.' -f $blockerNumbers[$index])
	}
}

# 잠금 표가 스물둘을 빠짐없이 덮는가.
$lockTable = [regex]::Match(
	$story,
	'### 즉시 차단 항목이 잠긴 자리\r?\n(?<body>[\s\S]*?)\r?\n\r?\n스물둘이 전부')
$assertionCount++
if (-not $lockTable.Success) {
	throw 'The §24 lock table could not be read.'
}
$covered = @{}
$namedScripts = @{}
foreach ($row in [regex]::Matches(
	$lockTable.Groups['body'].Value, '(?m)^\| (?<items>[^|]+?) \| (?<where>[^|]+?) \|\s*$')) {
	$items = $row.Groups['items'].Value.Trim()
	if ($items -eq '항목' -or $items -match '^-+$') {
		continue
	}
	foreach ($number in [regex]::Matches($items, '[0-9]+')) {
		$covered[[int]$number.Value] = $true
	}
	foreach ($script in [regex]::Matches(
		$row.Groups['where'].Value, '`(?<name>Test-[A-Za-z0-9-]+\.ps1)`')) {
		$namedScripts[$script.Groups['name'].Value] = $true
	}
}
for ($number = 1; $number -le 22; $number++) {
	$assertionCount++
	if (-not $covered.ContainsKey($number)) {
		throw "The §24 lock table does not say what watches blocker $number."
	}
}

# 표가 이름을 댄 스크립트는 실제로 있어야 한다. 없는 이름을 적어 두면
# 「잠겼다」가 「잠긴 줄 알았다」가 된다.
foreach ($name in $namedScripts.Keys) {
	$assertionCount++
	if (-not (Test-Path (Join-Path $projectRoot (Join-Path 'Scripts' $name)))) {
		throw "The §24 lock table names a script that does not exist: $name"
	}
}
$assertionCount++
if ($namedScripts.Count -lt 8) {
	throw 'The §24 lock table stopped naming the scripts that watch it.'
}

# 20번은 이 세션에 잠근 자리다. 표가 그 사실을 잊지 않게 한다.
$assertionCount++
if ($story -notmatch '`Test-MissingFloor-MixAndMovementContract\.ps1` — 더킹에 ENTITY 분기가 없음') {
	throw 'Blocker 20 lost the contract that watches it.'
}
$assertionCount++
if ($story -notmatch '선언과 구현이 갈라져도 아무도 모르는 상태가 결함 자체보다\s*\r?\n?위험하다') {
	throw 'The §24 warning this table exists to answer was removed.'
}

# 「일부러 깨서 잡히는 것까지 확인한 뒤에 넣는다」가 이 저장소의 방식이다.
$assertionCount++
if ($story -notmatch '\*\*일부러 깨서 잡히는 것까지 확인한 뒤\*\* 넣는다') {
	throw 'The §24 break-it-first rule was removed.'
}

# --- §26 제품 감사 계약 -------------------------------------------------------
#
# 26.2 첫 12분, 26.3 오디오 제작, 26.4 UI·조작. 26.5 성능 예산은 실측이라
# 합격식 문서가 맡는다.

$audioHeaderFor26 = Read-ProjectText 'Source/IndieGame/Audio/IGMissingFloorAudioSubsystem.h'
$uprojectText = Read-ProjectText 'IndieGame.uproject'

# 26.3-2 — 버스 이름은 §21.1의 여섯이다. 두 절이 같은 것을 다르게 부르면
# 어느 쪽을 고쳐야 하는지 알 수 없다.
$busNameRow = [regex]::Match(
	$story,
	'`BUS_ENTITY / PLAYER / PUZZLE / WORLD / UI / SCORE`')
$assertionCount++
if (-not $busNameRow.Success) {
	throw 'The §26.3 bus names no longer match §21.1.'
}
$busEnum = [regex]::Match(
	$audioHeaderFor26,
	'enum class EIGAudioBus : uint8\s*\r?\n\{(?<body>[\s\S]*?)\}')
$assertionCount++
if (-not $busEnum.Success) {
	throw 'The audio bus enum could not be read.'
}
foreach ($bus in @('Entity', 'Player', 'Puzzle', 'World', 'UI', 'Score')) {
	$assertionCount++
	if ($busEnum.Groups['body'].Value -notmatch ('(?m)^\s*' + $bus + ',')) {
		throw "The §26.3 bus name is not in the enum: $bus"
	}
}
# 옛 이름이 되살아나면 두 절이 다시 갈라진다.
foreach ($stale in @('BUS_ROOM', 'BUS_VOICE', 'BUS_SILENCE')) {
	$assertionCount++
	if ($story -match [regex]::Escape($stale)) {
		throw "The §26.3 bus list drifted back to a pre-implementation name: $stale"
	}
}

# 26.3-6 — 베타 기능은 출시 필수 경로에 두지 않는다.
$assertionCount++
if ($story -notmatch '\*\*안정판 우선\.\*\* Audio Gameplay Volumes처럼 Beta인 기능은 출시 필수 경로에') {
	throw 'The §26.3 stable-first rule was removed.'
}
foreach ($beta in @('AudioGameplayVolume', 'AudioGameplayVolumes')) {
	$assertionCount++
	if ($uprojectText -match [regex]::Escape($beta)) {
		throw "A beta audio plugin reached the shipping path (§26.3-6): $beta"
	}
}

# 26.4 — 공통 시스템 UI를 새 프레임워크로 전면 이식하지 않는다.
$assertionCount++
if ($story -notmatch '공통 시스템 UI를 새 프레임워크로 전면 이식하지 않는다') {
	throw 'The §26.4 no-CommonUI-port rule was removed.'
}
$assertionCount++
if ($uprojectText -match '"CommonUI"') {
	throw 'The native HUD path must stay; CommonUI was enabled (§26.4).'
}

# 26.4 재페이지 — 글자를 키우면 카드가 줄지, 카드가 작아지지 않는다.
$pageRow = [regex]::Match(
	$story,
	'(?<small>[0-9]+)~(?<smallMax>[0-9]+)%는 레인당 (?<three>[0-9]+)장, (?<midMin>[0-9]+)~(?<midMax>[0-9]+)%는 (?<two>[0-9]+)장, (?<bigMin>[0-9]+)~(?<bigMax>[0-9]+)%는 (?<one>[0-9]+)장')
$assertionCount++
if (-not $pageRow.Success) {
	throw 'The §26.4 repagination row could not be read.'
}
# 문서의 경계는 퍼센트, 코드의 경계는 배율이다. 116%는 1.15 초과, 151%는
# 1.50 초과로 옮겨진다.
$midThreshold = ([double]$pageRow.Groups['smallMax'].Value) / 100.0
$bigThreshold = ([double]$pageRow.Groups['midMax'].Value) / 100.0
$ladderSites = [regex]::Matches(
	$hudSource,
	'const int32 CardsPerLanePerPage = UserTextScale > (?<big>[0-9.]+)f\s*\r?\n\s*\? (?<one>[0-9]+)\s*\r?\n\s*: UserTextScale > (?<mid>[0-9.]+)f \? (?<two>[0-9]+) : (?<three>[0-9]+);')
$assertionCount++
if ($ladderSites.Count -lt 2) {
	throw 'The repagination ladder must stay in both the page count and the layout.'
}
foreach ($site in $ladderSites) {
	$assertionCount++
	if ([double]$site.Groups['big'].Value -ne $bigThreshold) {
		throw (
			'Repagination drops to one card above {0} but §26.4 says {1}.' -f
				$site.Groups['big'].Value, $bigThreshold)
	}
	$assertionCount++
	if ([double]$site.Groups['mid'].Value -ne $midThreshold) {
		throw (
			'Repagination drops to two cards above {0} but §26.4 says {1}.' -f
				$site.Groups['mid'].Value, $midThreshold)
	}
	foreach ($pair in @(
		@{ Group = 'one'; Expected = $pageRow.Groups['one'].Value },
		@{ Group = 'two'; Expected = $pageRow.Groups['two'].Value },
		@{ Group = 'three'; Expected = $pageRow.Groups['three'].Value })) {
		$assertionCount++
		if ($site.Groups[$pair.Group].Value -ne $pair.Expected) {
			throw (
				'Repagination shows {0} cards where §26.4 says {1}.' -f
					$site.Groups[$pair.Group].Value, $pair.Expected)
		}
	}
}
$assertionCount++
if ($story -notmatch '확대를 카드 안 축소로 상쇄하지 않는다') {
	throw 'The §26.4 no-shrink-to-fit rule was removed.'
}

# 26.4 — 생성 이미지의 글자가 정보가 되는 경로는 0이다.
$assertionCount++
if ($story -notmatch '생성 이미지의 글자\s*\r?\n?\s*환각이 정보가 되는 경로는 0이다') {
	throw 'The §26.4 no-hallucinated-text rule was removed.'
}

# 26.2 — 첫 12분에는 튜토리얼 팝업이 없고 밤 HUD가 0이다. §23이 같은 것을
# 금지하고 있으므로 여기서는 표가 그 약속을 계속 말하는지만 본다.
$assertionCount++
if ($story -notmatch '튜토리얼 팝업 대신 서로 다른 물리 반응') {
	throw 'The §26.2 no-tutorial-popup promise was removed.'
}
$assertionCount++
if ($story -notmatch '정보 대사 2문장 상한') {
	throw 'The §26.2 two-sentence cap was removed.'
}
$assertionCount++
if ($story -notmatch '강제 컷신 대신 첫 자율 공포 판단, 밤 HUD 0') {
	throw 'The §26.2 night-HUD-zero promise was removed.'
}

# --- §27 M0 입력 계약 실행 보완 -----------------------------------------------
#
# 이 절은 스스로 「수치나 우선순위가 충돌하면 이 절을 M0의 최신 계약으로
# 사용한다」고 적었다. 그러니 §18과 겹치는 값은 서로 같아야 하고, §18이
# 다루지 않는 줄은 여기서만 지켜진다.

# 27.3 — 이동 네 상태. 걷기·앉기·달리기는 §18.2와 겹치고, 듣기는 여기에만
# 있다. 하필 그 행만 코드에 숫자가 박혀 있었다.
$movementSection27 = [regex]::Match(
	$story,
	'### 27\.3 이동 상태 계약\r?\n(?<body>[\s\S]*?)\r?\n### 27\.4')
$assertionCount++
if (-not $movementSection27.Success) {
	throw 'The §27.3 movement table could not be read.'
}
$stateConstants = @{
	'걷기' = @('WalkAcceleration', 'WalkBraking')
	'앉기' = @('CrouchAcceleration', 'CrouchBraking')
	'달리기' = @('SprintAcceleration', 'SprintBraking')
	'듣기' = @('ListenAcceleration', 'ListenBraking')
}
$stateSpeeds = @{
	'걷기' = 'ReferenceWalkSpeed'
	'앉기' = 'CrouchSpeed'
	'달리기' = 'SprintSpeed'
	'듣기' = 'ListenSpeed'
}
$seenStates = 0
foreach ($row in [regex]::Matches(
	$movementSection27.Groups['body'].Value,
	'(?m)^\| (?<state>[^|]+?) \| (?<speed>[0-9]+) \| (?<accel>[0-9]+) \| (?<brake>[0-9]+) \|')) {
	$state = $row.Groups['state'].Value.Trim()
	$assertionCount++
	if (-not $stateConstants.ContainsKey($state)) {
		throw "The §27.3 table grew a state this contract does not know: $state"
	}
	$seenStates++
	$fields = @(
		@{ Name = $stateSpeeds[$state]; Expected = $row.Groups['speed'].Value },
		@{ Name = $stateConstants[$state][0]; Expected = $row.Groups['accel'].Value },
		@{ Name = $stateConstants[$state][1]; Expected = $row.Groups['brake'].Value })
	foreach ($field in $fields) {
		$declared = [regex]::Match(
			$characterSource,
			('constexpr float {0} = (?<value>[0-9.]+)f;' -f $field.Name))
		$assertionCount++
		if (-not $declared.Success) {
			throw ('The §27.3 movement constant is missing: {0}' -f $field.Name)
		}
		$assertionCount++
		if ([double]$declared.Groups['value'].Value -ne [double]$field.Expected) {
			throw (
				'{0} is {1} but §27.3 says {2}.' -f
					$field.Name, $declared.Groups['value'].Value, $field.Expected)
		}
	}
}
$assertionCount++
if ($seenStates -ne 4) {
	throw "The §27.3 table lists $seenStates states; it is supposed to list four."
}
# 듣는 동안 제동이 가장 세다. 그게 「멈추는 것도 빨라야 소리를 놓치지 않는다」의
# 구현이고, 네 상태 중 유일하게 걷기보다 큰 값이다.
$listenBraking = [regex]::Match(
	$characterSource, 'constexpr float ListenBraking = (?<value>[0-9.]+)f;')
$walkBraking = [regex]::Match(
	$characterSource, 'constexpr float WalkBraking = (?<value>[0-9.]+)f;')
$assertionCount++
if (-not $listenBraking.Success -or -not $walkBraking.Success) {
	throw 'The listen and walk braking could not be compared.'
}
$assertionCount++
if ([double]$listenBraking.Groups['value'].Value -le [double]$walkBraking.Groups['value'].Value) {
	throw 'Listening must stop faster than walking (§27.3).'
}
# 듣기 상태가 그 상수를 실제로 쓴다. 숫자를 도로 박아 넣으면 표와 코드가
# 다시 갈라진다.
$assertionCount++
if ($characterSource -notmatch
	'MovementComponent->MaxAcceleration = IGPlayerNoise::ListenAcceleration;') {
	throw 'The listening state must use its named acceleration (§27.3).'
}
$assertionCount++
if ($characterSource -notmatch
	'MovementComponent->BrakingDecelerationWalking =\s*\r?\n?\s*IGPlayerNoise::ListenBraking;') {
	throw 'The listening state must use its named braking (§27.3).'
}

# 27.2 — 동사 태그가 조준 대상에서 입력을 가른다.
foreach ($verb in @('MissingFloor.Verb.Knock', 'MissingFloor.Verb.Listen')) {
	$assertionCount++
	if ($story -notmatch [regex]::Escape($verb)) {
		throw "The §27.2 verb tag was removed from the doc: $verb"
	}
}
$verbTagPlaced = @{ 'Knock' = 0; 'Listen' = 0 }
$verbTagRead = @{ 'Knock' = 0; 'Listen' = 0 }
foreach ($file in Get-ChildItem -Path (Join-Path $projectRoot 'Source/IndieGame') `
	-Filter '*.cpp' -Recurse) {
	$text = Get-Content -Raw -Encoding UTF8 -LiteralPath $file.FullName
	foreach ($placed in [regex]::Matches(
		$text,
		'Tags\.AddUnique\(FName\(TEXT\("MissingFloor\.Verb\.(?<verb>Knock|Listen)"\)\)\)')) {
		$verbTagPlaced[$placed.Groups['verb'].Value]++
	}
	foreach ($use in [regex]::Matches(
		$text, 'MissingFloor\.Verb\.(?<verb>Knock|Listen)"')) {
		$verbTagRead[$use.Groups['verb'].Value]++
	}
}
foreach ($verb in @('Knock', 'Listen')) {
	$assertionCount++
	if ($verbTagPlaced[$verb] -lt 1) {
		throw "The §27.2 verb tag is never placed on anything: $verb"
	}
	# 붙인 자리도 같은 문자열이라 전체에서 빼야 읽는 자리만 남는다.
	$actualReads = $verbTagRead[$verb] - $verbTagPlaced[$verb]
	$assertionCount++
	if ($actualReads -lt 1) {
		throw "The §27.2 verb tag is placed but nothing reads it: $verb"
	}
}
# 일반 노크는 문에만 허용한다. 병·종이·버튼을 두드리는 우발 입력을 막는 줄이다.
$assertionCount++
if ($story -notmatch '`Interaction\.Door` 물성을 가진 문에만 허용해') {
	throw 'The §27.2 door-only knock rule was removed.'
}
$assertionCount++
if ($characterSource -notmatch 'FName\(TEXT\("Interaction\.Door"\)\)') {
	throw 'The ordinary knock must still be limited to doors (§27.2).'
}
# Q는 어떤 경로에서도 Interact의 별칭이 아니다(§24 즉시 차단 16).
$assertionCount++
if ($story -notmatch 'Q는 어떤 경로에서도 `Interact`의 별칭이 아니다') {
	throw 'The §27.2 no-alias rule was removed.'
}

# 27.5 — 응답음은 플레이어 피드백 함수를 통하지 않는다. §18.5가 같은 것을
# 다른 말로 잠갔고, 이 절은 그 이유를 적어 둔 자리다.
$assertionCount++
if ($story -notmatch '응답음은 플레이어 피드백\s*\r?\n?함수를 통하지 않으므로 카메라 킥과 진동이 없다') {
	throw 'The §27.5 silent-reply rule was removed.'
}
# 잠금은 노크만 잠근다. 이동·시점·숨 참기를 빼앗으면 그건 벌이다.
$assertionCount++
if ($story -notmatch '노크 액션만 0\.9초 잠근다\. 이동·시점·숨 참기·취소는\s*\r?\n?계속 가능하다') {
	throw 'The §27.5 lock must not take the camera or movement.'
}
# 잠금은 Knock의 첫 관문에서만 되돌린다. 다른 동사의 진입점에 같은 검사가
# 생기면 그건 전체 입력 잠금이고, §27.5가 금지한 것이다.
$assertionCount++
if ($characterSource -notmatch
	'CurrentWorld->GetTimeSeconds\(\) < KnockInputLockedUntil\)\s*\r?\n\s*\{\s*\r?\n\s*return;') {
	throw 'The knock lock must turn the knock away and nothing else (§27.5).'
}
# 한 번 걸고 한 번 읽는다. 세 번째 자리가 생기면 무엇이 잠기는지 알 수 없다.
$lockUses = ([regex]::Matches($characterSource, 'KnockInputLockedUntil')).Count
$assertionCount++
if ($lockUses -ne 2) {
	throw (
		'KnockInputLockedUntil appears {0} times; it is set once and read once (§27.5).' -f
			$lockUses)
}

# --- 버전별 실행 기록이 자기 계약을 대는가 --------------------------------------
#
# §24가 적어 둔 이유가 여기에도 그대로 걸린다. 실제로 §27~§32는 계약이 다섯
# 개나 있는데 그 이름을 아무 데도 대지 않고 있었다. 계약이 있는 것과, 있다는
# 사실을 문서를 읽는 사람이 아는 것은 다른 일이다.

$versionSections = [regex]::Matches(
	$story, '(?m)^## (?<number>2[7-9]|3[0-5])\. ')
$assertionCount++
if ($versionSections.Count -ne 9) {
	throw (
		'Expected nine version sections (§27~§35), found {0}.' -f
			$versionSections.Count)
}
for ($index = 0; $index -lt $versionSections.Count; $index++) {
	$start = $versionSections[$index].Index
	$end = if ($index + 1 -lt $versionSections.Count) {
		$versionSections[$index + 1].Index
	} else {
		$story.Length
	}
	$body = $story.Substring($start, $end - $start)
	$number = $versionSections[$index].Groups['number'].Value
	$named = [regex]::Matches($body, 'Test-[A-Za-z0-9-]+\.ps1')
	$assertionCount++
	if ($named.Count -lt 1) {
		throw "§$number records what was built but never says what watches it."
	}
	foreach ($script in $named) {
		$assertionCount++
		if (-not (Test-Path (
			Join-Path $projectRoot (Join-Path 'Scripts' $script.Value)))) {
			throw "§$number names a contract that does not exist: $($script.Value)"
		}
	}
}

# --- §12 진실 게이트 · §13 포어섀도 장부 ----------------------------------------
#
# 앞 절들과 성격이 다르다. 여기 걸린 것은 숫자가 아니라 **인과**다 — 무엇이
# 확정돼야 무엇이 열리는가, 심은 것이 회수되는가.

$narrativeSource = Read-ProjectText 'Source/IndieGame/Narrative/IGMissingFloorNarrativeSubsystem.cpp'

# 12 — 최종 선택 게이트는 T6·T7·T9 확정 + 벽 개방이다.
$gateRow = [regex]::Match(
	$story, '최종 선택 게이트: (?<truths>T[0-9]+(?:·T[0-9]+)*) 확정 \+ 벽 개방')
$assertionCount++
if (-not $gateRow.Success) {
	throw 'The §12 final-choice gate could not be read.'
}
$gateTruths = $gateRow.Groups['truths'].Value -split '·'
$assertionCount++
if ($gateTruths.Count -ne 3) {
	throw (
		'The §12 gate names {0} truths; the code checks three.' -f $gateTruths.Count)
}
# 표에서 그 태그의 열거형 이름을 끌어온다. 태그와 이름을 두 번 적지 않는다.
$truthNames = @{
	'T6' = 'SomeoneInTheWall'
	'T7' = 'WasStillAlive'
	'T9' = 'WaitingForAnAnswer'
}
$unlockBody = [regex]::Match(
	$narrativeSource,
	'bool UIGMissingFloorNarrativeSubsystem::IsFinalChoiceUnlocked\(\) const(?<body>[\s\S]*?)\r?\n\}')
$assertionCount++
if (-not $unlockBody.Success) {
	throw 'IsFinalChoiceUnlocked could not be isolated.'
}
foreach ($tag in $gateTruths) {
	$assertionCount++
	if (-not $truthNames.ContainsKey($tag)) {
		throw "The §12 gate names a truth this contract does not know: $tag"
	}
	$assertionCount++
	if (-not $unlockBody.Groups['body'].Value.Contains(
		('EIGMissingFloorTruth::{0}' -f $truthNames[$tag]))) {
		throw "The final choice must require $tag ($($truthNames[$tag]))."
	}
}
# 세 개뿐이다. 하나를 더 걸면 게이트가 문서보다 좁아진다.
$gateChecks = ([regex]::Matches(
	$unlockBody.Groups['body'].Value, 'HasTruth\(')).Count
$assertionCount++
if ($gateChecks -ne $gateTruths.Count) {
	throw (
		'The final choice checks {0} truths but §12 names {1}.' -f
			$gateChecks, $gateTruths.Count)
}

# 벽은 게이트가 열린 뒤에만 부술 수 있고, 선택은 벽이 열린 뒤에만 뜬다.
# 순서가 뒤집히면 진실을 모으지 않고도 엔딩에 닿는다.
$nightFourForGate = Read-ProjectText 'Source/IndieGame/Entity/IGMissingFloorNightFourDirector.cpp'
$assertionCount++
if ($nightFourForGate -notmatch
	'const bool bCanBreak = bNightFour\s*\r?\n\s*&& Narrative->IsFinalChoiceUnlocked\(\)') {
	throw 'The wall must stay shut until the §12 gate opens.'
}
$assertionCount++
if ($nightFourForGate -notmatch
	'const bool bCanChoose = bNightFour\s*\r?\n\s*&& bWallOpened') {
	throw 'The ending choice must wait for the wall (§12).'
}
# 첫 신고 없이 선택이 뜨면 §24 즉시 차단 14가 깨진다.
$assertionCount++
if ($nightFourForGate -notmatch
	'&& Narrative->WasFirstReportMade\(\)') {
	throw 'The ending choice must wait for the first report (§12, §24-14).'
}

# 13 — 심은 것은 반드시 회수된다. 표의 네 칸이 다 차 있어야 그 원칙이 읽힌다.
$ledgerTable = [regex]::Match(
	$story,
	'## 13\. 포어섀도 장부[^\r\n]*\r?\n(?<body>[\s\S]*?)\r?\n\r?\n회수 없는 심기')
$assertionCount++
if (-not $ledgerTable.Success) {
	throw 'The §13 foreshadow ledger could not be read.'
}
$ledgerRows = 0
foreach ($row in [regex]::Matches(
	$ledgerTable.Groups['body'].Value,
	'(?m)^\| (?<plant>[^|]+?) \| (?<where>[^|]+?) \| (?<payoff>[^|]+?) \| (?<when>[^|]+?) \|')) {
	$fields = @('plant', 'where', 'payoff', 'when')
	if ($row.Groups['plant'].Value.Trim() -eq '심기') {
		continue
	}
	$ledgerRows++
	foreach ($field in $fields) {
		$assertionCount++
		$value = $row.Groups[$field].Value.Trim()
		if ($value.Length -lt 2 -or $value -match '^-+$') {
			throw (
				'A §13 ledger row has an empty {0}: {1}' -f
					$field, $row.Groups['plant'].Value.Trim())
		}
	}
}
$assertionCount++
if ($ledgerRows -lt 16) {
	throw "The §13 ledger lists $ledgerRows plantings; sixteen were authored."
}
$assertionCount++
if ($story -notmatch '회수 없는 심기, 심기 없는 회수 금지 원칙 유지') {
	throw 'The §13 plant-and-payoff rule was removed.'
}

# 표에 오른 출처는 전부 세계에 실물이 있어야 한다. 그 약속이 사라지면
# 도달 불가인 진실이 다시 생긴다 — 실제로 T5와 T8이 그랬다.
$assertionCount++
if ($story -notmatch '\*\*표에 오른 출처는 전부 세계에 실물이 있어야 한다\.\*\*') {
	throw 'The §12 every-source-is-real rule was removed.'
}
$assertionCount++
if ($story -notmatch '`Test-ArtAssetContract\.ps1`이 열거형을 읽어 게임플레이 파일과 대조한다') {
	throw 'The §12 source-reachability contract lost its name.'
}

# --- §5.1 소음 모델 · §17 기믹 배치표 -------------------------------------------
#
# §5.1은 값이 코드에 흩어져 있고, §17은 「몇 개인가」와 「없는가」다.

$noiseHeader = Read-ProjectText 'Source/IndieGame/Entity/IGNoiseSubsystem.h'
$doorHeaderFor5 = Read-ProjectText 'Source/IndieGame/Interaction/IGSwingDoor.h'
$entitySourceFor5 = Read-ProjectText 'Source/IndieGame/Entity/IGListenerEntity.cpp'
$stressForNoise = Read-ProjectText 'Source/IndieGame/Player/IGStressComponent.cpp'

# 문 여닫기 두 값. 조용히 여는 쪽이 더 조용해야 홀드가 값을 한다.
$doorRow = [regex]::Match(
	$story, '\| 문 여닫기\(천천히/그냥\) \| (?<quiet>[0-9.]+) / (?<normal>[0-9.]+) \|')
$assertionCount++
if (-not $doorRow.Success) {
	throw 'The §5.1 door row could not be read.'
}
foreach ($pair in @(
	@{ Field = 'QuietSwingLoudness'; Expected = $doorRow.Groups['quiet'].Value },
	@{ Field = 'NormalSwingLoudness'; Expected = $doorRow.Groups['normal'].Value })) {
	$declared = [regex]::Match(
		$doorHeaderFor5, ('float {0} = (?<value>[0-9.]+)f;' -f $pair.Field))
	$assertionCount++
	if (-not $declared.Success) {
		throw ('The §5.1 door loudness is missing: {0}' -f $pair.Field)
	}
	$assertionCount++
	if ([double]$declared.Groups['value'].Value -ne [double]$pair.Expected) {
		throw (
			'{0} is {1} but §5.1 says {2}.' -f
				$pair.Field, $declared.Groups['value'].Value, $pair.Expected)
	}
}
$assertionCount++
if ([double](
	[regex]::Match($doorHeaderFor5, 'float QuietSwingLoudness = (?<v>[0-9.]+)f;').Groups['v'].Value) -ge
	[double](
	[regex]::Match($doorHeaderFor5, 'float NormalSwingLoudness = (?<v>[0-9.]+)f;').Groups['v'].Value)) {
	throw 'Opening a door slowly must be the quieter option (§5.1).'
}

# 앉아 이동의 소음.
$crouchRow = [regex]::Match(
	$story, '\| 정지·앉아 이동 \| (?<value>[0-9.]+) \|')
$assertionCount++
if (-not $crouchRow.Success) {
	throw 'The §5.1 crouch row could not be read.'
}
$crouchDeclared = [regex]::Match(
	$characterSource, 'constexpr float CrouchFootstepLoudness = (?<value>[0-9.]+)f;')
$assertionCount++
if (-not $crouchDeclared.Success) {
	throw 'CrouchFootstepLoudness could not be read.'
}
$assertionCount++
if ([double]$crouchDeclared.Groups['value'].Value -ne [double]$crouchRow.Groups['value'].Value) {
	throw (
		'CrouchFootstepLoudness is {0} but §5.1 says {1}.' -f
			$crouchDeclared.Groups['value'].Value, $crouchRow.Groups['value'].Value)
}

# 험 존: 반경 2m, 마스킹 0.2. 지금 서 있는 둘이 같은 값을 써야 한다.
$humRow = [regex]::Match(
	$story, '험 존\(반경 (?<radius>[0-9]+)m\)은 소음을 -(?<masking>[0-9.]+) 마스킹한다')
$assertionCount++
if (-not $humRow.Success) {
	throw 'The §5.1 hum-zone row could not be read.'
}
$expectedRadius = [double]$humRow.Groups['radius'].Value * 100.0
$humZones = 0
foreach ($file in @(
	'Source/IndieGame/Entity/IGListenerGreyboxDirector.cpp',
	'Source/IndieGame/Entity/IGNightOneBeatDirector.cpp')) {
	$text = Read-ProjectText $file
	$radius = [regex]::Match($text, 'HumRadius = (?<value>[0-9.]+)f;')
	$masking = [regex]::Match($text, 'HumMasking = (?<value>[0-9.]+)f;')
	$assertionCount++
	if (-not $radius.Success -or -not $masking.Success) {
		throw "A §5.1 hum zone lost its radius or masking: $file"
	}
	$humZones++
	$assertionCount++
	if ([double]$radius.Groups['value'].Value -ne $expectedRadius) {
		throw (
			'A hum zone reaches {0}cm but §5.1 says {1}cm.' -f
				$radius.Groups['value'].Value, $expectedRadius)
	}
	$assertionCount++
	if ([double]$masking.Groups['value'].Value -ne [double]$humRow.Groups['masking'].Value) {
		throw (
			'A hum zone masks {0} but §5.1 says {1}.' -f
				$masking.Groups['value'].Value, $humRow.Groups['masking'].Value)
	}
}
$assertionCount++
if ($humZones -ne 2) {
	throw "§5.1 says two hum zones stand; found $humZones."
}
# 보일러실에는 아직 없다. 문서가 그 사실을 말하고 있어야 다음 사람이
# 「기계가 있으니 가려지겠지」로 읽지 않는다.
$assertionCount++
if ($story -notmatch '4층 보일러실\(§6\)에는 아직 험 존이 없다') {
	throw 'The §5.1 note about the boiler room was removed.'
}

# 노크 3연 동안의 전역 마스킹.
$bangRow = [regex]::Match(
	$story, '노크 3연 동안은 모든 플레이어 소음이 -(?<value>[0-9.]+) 마스킹된다')
$assertionCount++
if (-not $bangRow.Success) {
	throw 'The §5.1 knock-masking row could not be read.'
}
$bangDeclared = [regex]::Match(
	$entitySourceFor5, 'constexpr float BangMasking = (?<value>[0-9.]+)f;')
$assertionCount++
if (-not $bangDeclared.Success) {
	throw 'BangMasking could not be read.'
}
$assertionCount++
if ([double]$bangDeclared.Groups['value'].Value -ne [double]$bangRow.Groups['value'].Value) {
	throw (
		'BangMasking is {0} but §5.1 says {1}.' -f
			$bangDeclared.Groups['value'].Value, $bangRow.Groups['value'].Value)
}

# 심박은 반경이 계약이다. 소음값 × 전달거리가 3m에 닿는지 계산해서 본다.
$heartRow = [regex]::Match(
	$story, '3m를 채우는 값이 (?<loudness>[0-9.]+)이고 코드가 그것을 쓴다')
$assertionCount++
if (-not $heartRow.Success) {
	throw 'The §5.1 heartbeat note could not be read.'
}
$carry = [regex]::Match(
	$noiseHeader, 'CarryPerLoudness = (?<value>[0-9.]+)f;')
$assertionCount++
if (-not $carry.Success) {
	throw 'CarryPerLoudness could not be read.'
}
$assertionCount++
if (-not $stressForNoise.Contains(
	($heartRow.Groups['loudness'].Value + 'f,'))) {
	throw (
		'The heartbeat must report {0} to reach three meters (§5.1).' -f
			$heartRow.Groups['loudness'].Value)
}
$heartReach = [double]$heartRow.Groups['loudness'].Value * [double]$carry.Groups['value'].Value
$assertionCount++
if ($heartReach -lt 290.0 -or $heartReach -gt 310.0) {
	throw (
		'The heartbeat carries {0}cm; §5.1 wants about three meters.' -f $heartReach)
}

# --- §17 기믹 배치표 -----------------------------------------------------------
$adopted = [regex]::Match(
	$story, '### 17\.1 채택 — (?<count>[0-9]+)종\r?\n(?<body>[\s\S]*?)\r?\n### 17\.2')
$assertionCount++
if (-not $adopted.Success) {
	throw 'The §17.1 adoption table could not be read.'
}
$adoptedRows = 0
foreach ($row in [regex]::Matches(
	$adopted.Groups['body'].Value, '(?m)^\| (?<gimmick>[^|]+?) \| (?<lineage>[^|]+?) \|')) {
	if ($row.Groups['gimmick'].Value.Trim() -eq '기믹' -or
		$row.Groups['gimmick'].Value -match '^-+$') {
		continue
	}
	$adoptedRows++
}
$assertionCount++
if ($adoptedRows -ne [int]$adopted.Groups['count'].Value) {
	throw (
		'§17.1 says {0} adopted gimmicks but lists {1}.' -f
			$adopted.Groups['count'].Value, $adoptedRows)
}
# 기믹은 한 비트에 1회다. 그 원칙이 사라지면 표는 그냥 목록이 된다.
$assertionCount++
if ($story -notmatch '\*\*기믹은 게임의 정체성\(소리\)을 통과해야만 채택된다\.\*\*') {
	throw 'The §17 adoption principle was removed.'
}
$assertionCount++
if ($story -notmatch '한 밤에 신규 기믹 등장은 최대 1개') {
	throw 'The §17.4 density rule was removed.'
}
# 메타는 두 곳뿐이다 — 실시간 시계와 밤 5.
$assertionCount++
if ($story -notmatch '메타는 §17\.1의 두 곳뿐') {
	throw 'The §17.3 two-metas-only rule was removed.'
}
$assertionCount++
if ($story -notmatch '세이브 조작·가짜 크래시 금지') {
	throw 'The §17.1 no-save-tampering rule was removed.'
}
foreach ($banned in @('DeleteSaveThreat', 'FakeCrash', 'CorruptSaveEffect')) {
	$assertionCount++
	if ($hudSource.Contains($banned) -or $controllerSource.Contains($banned)) {
		throw "§17 forbids breaking the player's trust: $banned"
	}
}
# 바디캠 회피의 이유는 남되, FOV는 §18.3에서 열렸다. 두 절이 갈라지면
# 어느 쪽이 지금 규칙인지 알 수 없다.
$assertionCount++
if ($story -match 'FOV 78 고정 유지') {
	throw 'The §17.3 bodycam row still says the FOV is fixed; §18.3 opened it.'
}
$assertionCount++
if ($story -notmatch '기본 FOV 78 유지\(§18\.3에서 68~100으로 열되 기본값은 그대로\)') {
	throw 'The §17.3 bodycam row must point at the §18.3 range.'
}

# --- §14 구현 매핑 --------------------------------------------------------------
#
# 이 절은 설계를 실제 클래스로 잇는 지도다. 이름이 틀리면 지도가 아니라
# 미로가 된다 — 실제로 여섯 개가 틀려 있었다. 셋은 접두사, 둘은 만들지 않은
# 컴포넌트, 하나는 바뀐 시그니처.

$mappingSection = [regex]::Match(
	$story, '## 14\. 구현 매핑[^\r\n]*\r?\n(?<body>[\s\S]*?)\r?\n## 15\.')
$assertionCount++
if (-not $mappingSection.Success) {
	throw 'The §14 implementation map could not be read.'
}
# 헤더 파일 전체에서 선언된 타입 이름을 한 번만 모은다.
$declaredTypes = @{}
foreach ($file in Get-ChildItem -Path (Join-Path $projectRoot 'Source/IndieGame') `
	-Filter '*.h' -Recurse) {
	$text = Get-Content -Raw -Encoding UTF8 -LiteralPath $file.FullName
	foreach ($type in [regex]::Matches(
		$text, 'class(?: INDIEGAME_API)? (?<name>[AUF]IG[A-Za-z0-9]+)')) {
		$declaredTypes[$type.Groups['name'].Value] = $true
	}
}
$assertionCount++
if ($declaredTypes.Count -lt 20) {
	throw 'The type sweep found too few classes to trust (§14).'
}
# 절이 백틱으로 감싼 타입 이름은 전부 실제로 있어야 한다. 만들지 않기로 한
# 것은 이름이 아니라 문장으로 적는다 — 그래야 「어디 있지」로 시간을 안 쓴다.
$mappedTypes = 0
foreach ($mention in [regex]::Matches(
	$mappingSection.Groups['body'].Value, '`(?<name>[AUF]IG[A-Za-z0-9]+)`')) {
	$name = $mention.Groups['name'].Value
	$mappedTypes++
	$assertionCount++
	if (-not $declaredTypes.ContainsKey($name)) {
		throw "§14 maps to a class that does not exist: $name"
	}
}
$assertionCount++
if ($mappedTypes -lt 10) {
	throw '§14 stopped naming the classes it maps to.'
}

# 만들지 않기로 한 둘은 이름으로 남기지 않는다. 백틱 안에 있으면 위 검사가
# 잡지만, 왜 안 만들었는지가 사라지면 다음 사람이 다시 만들려 한다.
$assertionCount++
if ($mappingSection.Groups['body'].Value -notmatch
	'두드리기와 엿듣기는 \*\*컴포넌트로 나누지 않았다\.\*\*') {
	throw 'The §14 note about the two components that were never built was removed.'
}

# 소음 API는 반경을 받지 않는다. 부르는 쪽이 크기와 거리를 따로 정하면
# 「시끄러울수록 멀리 간다」가 깨진다.
$assertionCount++
if ($mappingSection.Groups['body'].Value -notmatch
	'`ReportNoise\(Location,\s*\r?\n?\s*Loudness, Instigator\)`') {
	throw 'The §14 noise API signature drifted from the code.'
}
$reportSignature = [regex]::Match(
	$noiseHeader,
	'FIGNoiseEvent ReportNoise\((?<args>[\s\S]{0,240}?)\);')
$assertionCount++
if (-not $reportSignature.Success) {
	throw 'ReportNoise could not be read from the header.'
}
$assertionCount++
if ($reportSignature.Groups['args'].Value -match 'Radius') {
	throw 'ReportNoise must derive its radius from loudness, not take one (§14).'
}
$assertionCount++
if ($story -notmatch '반경은 인자가 아니라 \*\*소음값에서 나온다\*\*') {
	throw 'The §14 derived-radius rule was removed.'
}

# 시점 행 수는 화면과 컨트롤러가 함께 보는 값이다. 여기서도 코드에서 읽는다.
$lookRowMatch = [regex]::Match(
	$bindingHeader, 'LookRowCount = (?<count>[0-9]+);')
$assertionCount++
if (-not $lookRowMatch.Success) {
	throw 'LookRowCount could not be read.'
}
$lookRowCount = [int]$lookRowMatch.Groups['count'].Value

Write-Host (
	'MISSING_FLOOR_INPUT_BINDING_CONTRACT PASS actions={0} look={1} assertions={2}' -f `
		$entries.Count, $lookRowCount, $assertionCount) -ForegroundColor Green
