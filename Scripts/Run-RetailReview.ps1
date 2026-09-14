[CmdletBinding()]
param([switch]$Measure)
$ErrorActionPreference = 'Stop'
$reviewRoot = Split-Path -Parent $PSScriptRoot
$reviewProject = Join-Path $reviewRoot 'IndieGame.uproject'
$reviewEditor = & (Join-Path $PSScriptRoot 'Resolve-UnrealEditor.ps1') -ProjectPath $reviewProject -Commandlet
$reviewLog = Join-Path $reviewRoot 'Saved/Logs/RetailReview.log'
$reviewStarted = Get-Date
$reviewArgs = @($reviewProject, '-game', '-unattended', '-nosplash', '-NoLoadingScreen',
    '-RenderOffscreen', '-d3d12', '-nosound', '-Windowed', '-ResX=1920', '-ResY=1080', '-ForceRes',
    '-IGMissingFloor', '-IGIgnoreDirectStart', '-IGArrivalCapture', '-IGRetailCapture', '-IGSkipFrontend', "-abslog=$reviewLog")
if ($Measure) { $reviewArgs += @('-IGRetailMetrics', '-IGCaptureMetricsOnly', '-csvGpuStats', '-ExecCmds=t.MaxFPS 0,r.VSync 0') }
& $reviewEditor @reviewArgs | Out-Null
if ($LASTEXITCODE -ne 0 -or -not (Select-String -LiteralPath $reviewLog -Pattern 'RETAIL_CAPTURE PASS shots=') -or
    (Select-String -LiteralPath $reviewLog -Pattern 'missing usage flag|Failed to compile Material|RETAIL_STOCK FAIL|Fatal error:')) {
    throw "매장 화면 검증 실패: $reviewLog"
}
foreach ($shot in $(if ($Measure) { @() } else { @('exterior', 'counter', 'stock', 'cooler', 'delivery-lane') })) {
    $file = Get-Item -LiteralPath (Join-Path $reviewRoot "Docs/Media/retail-$shot.png")
    if ($file.LastWriteTime -lt $reviewStarted -or $file.Length -lt 10000) {
        throw "새 화면이 저장되지 않았다: $($file.FullName)"
    }
}
Select-String -LiteralPath $reviewLog -Pattern 'RETAIL_STOCK|RETAIL_CAPTURE' | ForEach-Object { Write-Host $_.Line }
if ($Measure -and -not (Select-String -LiteralPath $reviewLog -Pattern 'Writing CSV to file')) { throw '성능 CSV가 저장되지 않았다.' }
Write-Host "RETAIL_REVIEW PASS measure=$Measure production=1 resolution=1920x1080"
