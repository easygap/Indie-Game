[CmdletBinding()]
param(
	# 에셋 이름(SM_...) 또는 빌더 이름(unit_door). 비우면 전부.
	[string[]]$Only = @()
)

# Scripts/blender/build_*.py를 Blender 헤드리스로 돌려 Content/SourceArt/Blender에
# FBX·구운 텍스처·manifest.json·미리보기를 만든다. 다음 단계는
# Import-BlenderAssets.ps1.
#
# Blender는 설치하지 않고 MSI를 사용자 폴더에 풀어 쓴다(관리자 권한 불필요).
#   msiexec /a blender-5.2.0-windows-x64.msi /qn TARGETDIR=%LOCALAPPDATA%\Programs\Blender-5.2
# IG_BLENDER 환경 변수로 다른 blender.exe를 가리킬 수 있다.

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$projectRoot = Split-Path -Parent $PSScriptRoot
$builderRoot = Join-Path $PSScriptRoot 'blender'
$outRoot = Join-Path $projectRoot 'Content\SourceArt\Blender'
New-Item -ItemType Directory -Force -Path $outRoot | Out-Null

$candidates = @()
if ($env:IG_BLENDER) { $candidates += $env:IG_BLENDER }
$candidates += Join-Path $env:LOCALAPPDATA 'Programs\Blender-5.2\Blender Foundation\Blender 5.2\blender.exe'
$candidates += Get-ChildItem -Path (Join-Path $env:ProgramFiles 'Blender Foundation') -Filter 'blender.exe' -Recurse -ErrorAction SilentlyContinue | ForEach-Object { $_.FullName }
$blender = $candidates | Where-Object { $_ -and (Test-Path -LiteralPath $_ -PathType Leaf) } | Select-Object -First 1
if (-not $blender) {
	throw 'blender.exe를 찾지 못했다. IG_BLENDER를 설정하거나 MSI를 %LOCALAPPDATA%\Programs\Blender-5.2에 풀어라.'
}

# 빌더 하나가 에셋 여럿을 만들 수 있다. 어떤 빌더가 무엇을 만드는지는 여기 표가
# 유일한 출처다. Import 쪽은 manifest만 보므로 표는 이쪽에만 필요하다.
$builders = [ordered]@{
	'unit_door' = @('SM_UnitDoorLeaf', 'SM_UnitDoorLeafL', 'SM_UnitDoorHardware', 'SM_UnitDoorHardwareL', 'SM_UnitDoorFrame')
	'corridor_fixtures' = @('SM_FireExtinguisherBox', 'SM_FireExtinguisher')
	'lobby_mailboxes' = @('SM_MailboxUnit')
	'ceiling_light' = @('SM_CeilingLightRing', 'SM_CeilingLightDome')
	'fridge' = @('SM_FridgeBody', 'SM_FridgeDoor')
	'kitchen' = @('SM_KitchenBaseRun', 'SM_DrumWasher', 'SM_KitchenWallUnits', 'SM_RangeHood', 'SM_Microwave', 'SM_KitchenSink', 'SM_InductionHob')
	'apartment_props' = @('SM_Wardrobe', 'SM_WallAirConditioner')
	'alley_props' = @('SM_TrafficCone', 'SM_UtilityPole')
	'store_fixtures' = @('SM_StoreCoolerBank', 'SM_StoreCoolerDoor', 'SM_StoreGondola', 'SM_StoreCounter', 'SM_CardTerminal', 'SM_HotSnackWarmer', 'SM_ChestFreezer', 'SM_OpenShowcase', 'SM_RamyeonRack')
	'apartment_fixtures' = @('SM_ApartmentWindow', 'SM_VenetianBlind', 'SM_VideoIntercom', 'SM_WallSwitch', 'SM_ShoeCabinet')
	'villa_window' = @('SM_VillaWindow')
}

$selected = @()
foreach ($entry in $builders.GetEnumerator()) {
	if ($Only.Count -eq 0) { $selected += $entry.Key; continue }
	if ($Only -contains $entry.Key) { $selected += $entry.Key; continue }
	foreach ($asset in $entry.Value) {
		if ($Only -contains $asset) { $selected += $entry.Key; break }
	}
}
if ($selected.Count -eq 0) {
	throw "고른 이름에 맞는 빌더가 없다: $($Only -join ', ')"
}

$logRoot = Join-Path $projectRoot 'Saved\Logs'
New-Item -ItemType Directory -Force -Path $logRoot | Out-Null
foreach ($builder in $selected) {
	$script = Join-Path $builderRoot "build_$builder.py"
	$logPath = Join-Path $logRoot ('Blender_{0}_{1}.log' -f $builder, (Get-Date -Format 'yyyyMMdd_HHmmss'))
	Write-Host "BLENDER_BUILD running build_$builder.py"
	& $blender -b --factory-startup --python $script -- $outRoot 2>&1 | Tee-Object -FilePath $logPath | Where-Object { $_ -match '^\[IGBL\]|Error|Traceback' } | ForEach-Object { Write-Host "  $_" }
	if ($LASTEXITCODE -ne 0) {
		throw "Blender 빌더 실패 ($LASTEXITCODE): build_$builder.py, 로그 $logPath"
	}
	foreach ($asset in $builders[$builder]) {
		$manifest = Join-Path $outRoot "$asset\manifest.json"
		if (-not (Test-Path -LiteralPath $manifest -PathType Leaf)) {
			throw "빌더가 manifest를 남기지 않았다: $manifest"
		}
	}
}
Write-Host ('BLENDER_BUILD PASS builders={0}' -f $selected.Count)
