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
