[CmdletBinding()]
param()
$ErrorActionPreference = 'Stop'
$utilityRoot = Split-Path -Parent $PSScriptRoot
$utilityStage = Join-Path $env:LOCALAPPDATA 'IndieGame/ArtImport'
$utilityProject = Join-Path $utilityStage 'ArtImport.uproject'
if (-not (Test-Path -LiteralPath $utilityProject)) {
    # 콘텐츠 전용 프로젝트 생성과 기존 재질 동기화를 맡긴다.
    & (Join-Path $PSScriptRoot 'Import-BlenderAssets.ps1') -Only 'SM_BoothMonitor'
}
# Import-BlenderAssets.ps1이 사용하는 콘텐츠 프로젝트의 이름을 그대로 찾는다.
$utilityProject = (Get-ChildItem -LiteralPath $utilityStage -Filter '*.uproject' | Select-Object -First 1).FullName
foreach ($part in @('Content/Prototype/Textures', 'Content/Prototype/Materials')) {
    & robocopy.exe (Join-Path $utilityRoot $part) (Join-Path $utilityStage $part) /E /R:2 /W:1 /NFL /NDL /NP /NJH /NJS | Out-Null
    if ($LASTEXITCODE -ge 8) { throw "설비 재질 동기화 실패: $part" }
}
$utilitySources = Join-Path $utilityStage 'UtilitySources'
New-Item -ItemType Directory -Force -Path $utilitySources | Out-Null
Copy-Item -LiteralPath (Join-Path $utilityRoot 'Content/SourceArt/AI/TankSatinSteel_20260915.png') -Destination $utilitySources -Force
& python (Join-Path $PSScriptRoot 'build_utility_prints.py')
if ($LASTEXITCODE -ne 0) { throw '검침창 인쇄 원본 생성 실패' }
foreach ($name in @('MeterCounter', 'MeterLabel', 'BoothAgentNote', 'BoothReceipts', 'BoothCalendar', 'LobbyWaterNotice', 'LobbyContactNotice', 'LobbyMeterSheet', 'LobbyForumPrint', 'PumpProcedure')) {
    Copy-Item -LiteralPath (Join-Path $utilityRoot "Content/SourceArt/UtilityPrints/$name.png") -Destination $utilitySources -Force
}
$atlas = Join-Path $utilityRoot 'Content/SourceArt/Cctv/CctvStandby.png'
& python (Join-Path $PSScriptRoot 'build_cctv_standby_atlas.py')
if ($LASTEXITCODE -ne 0) { throw '실제 맵 CCTV 화면 생성 실패' }
Copy-Item -LiteralPath $atlas -Destination $utilitySources -Force
$graniteSources = Join-Path $utilityStage 'Content/SourceArt/AI'
New-Item -ItemType Directory -Force -Path $graniteSources | Out-Null
Copy-Item -LiteralPath (Join-Path $utilityRoot 'Content/SourceArt/AI/PocheonGranite_20260915.png') -Destination $graniteSources -Force
Copy-Item -LiteralPath (Join-Path $utilityRoot 'Content/SourceArt/AI/LandingPaint_20260915.png') -Destination $graniteSources -Force
Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'retail_surface_contract.py') -Destination (Join-Path $utilityStage 'Scripts/retail_surface_contract.py') -Force
$utilityScript = Join-Path $utilityStage 'Scripts/build_utility_materials.py'
Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'build_utility_materials.py') -Destination $utilityScript -Force
$utilityEditor = & (Join-Path $PSScriptRoot 'Resolve-UnrealEditor.ps1') -ProjectPath $utilityProject -Commandlet
$utilityLog = Join-Path $utilityRoot 'Saved/Logs/UtilityMaterials.log'
& $utilityEditor $utilityProject -unattended -nop4 -nosplash -nullrhi -nosound -stdout -FullStdOutLogOutput "-abslog=$utilityLog" "-ExecutePythonScript=$utilityScript" | Out-Null
if ($LASTEXITCODE -ne 0 -or -not (Select-String -LiteralPath $utilityLog -Pattern 'UTILITY_MATERIALS PASS')) {
    throw "설비 재질 생성 실패: $utilityLog"
}
foreach ($name in @('M_Stucco_X', 'M_Stucco_Y', 'M_StuccoCeil', 'M_StuccoDado_X', 'M_StuccoDado_Y', 'M_UtilityStreetBrick', 'M_UtilityVillaBrick', 'M_GraniteTile_XY', 'M_UtilityTankSteel', 'M_UtilityFoundation', 'M_UtilityGraniteCladding', 'M_UtilityConcreteDark', 'M_CctvStandby', 'M_UtilityMeterCounter', 'M_UtilityMeterLabel', 'M_BoothAgentNote', 'M_BoothReceipts', 'M_BoothCalendar', 'M_LobbyWaterNotice', 'M_LobbyContactNotice', 'M_LobbyMeterSheet', 'M_LobbyForumPrint', 'M_PumpProcedure')) {
    Copy-Item -LiteralPath (Join-Path $utilityStage "Content/Prototype/Materials/$name.uasset") -Destination (Join-Path $utilityRoot 'Content/Prototype/Materials') -Force
}
foreach ($name in @('T_LandingPaint_20260915_D', 'T_UtilityTankSteel_D', 'T_CctvStandby_D', 'T_UtilityMeterCounter_D', 'T_UtilityMeterLabel_D', 'T_BoothAgentNote_D', 'T_BoothReceipts_D', 'T_BoothCalendar_D', 'T_LobbyWaterNotice_D', 'T_LobbyContactNotice_D', 'T_LobbyMeterSheet_D', 'T_LobbyForumPrint_D', 'T_PumpProcedure_D')) {
    $source = Join-Path $utilityStage "Content/Prototype/Textures/$name.uasset"
    if (Test-Path -LiteralPath $source) { Copy-Item -LiteralPath $source -Destination (Join-Path $utilityRoot 'Content/Prototype/Textures') -Force }
}
Select-String -LiteralPath $utilityLog -Pattern 'UTILITY_MATERIALS PASS' | ForEach-Object { Write-Host $_.Line }
