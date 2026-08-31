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

Write-Host (
	'MISSING_FLOOR_INPUT_BINDING_CONTRACT PASS actions={0} assertions={1}' -f `
		$entries.Count, $assertionCount) -ForegroundColor Green
