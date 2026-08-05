[CmdletBinding()]
param(
	[ValidateRange(30, 600)]
	[int]$TimeoutSeconds = 120,
	[string]$EvidenceDirectory
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$projectRoot = Split-Path -Parent $PSScriptRoot
$projectFile = Join-Path $projectRoot 'IndieGame.uproject'
$resolver = Join-Path $PSScriptRoot 'Resolve-UnrealEditor.ps1'
$runId = '{0}_{1}' -f (
	[DateTime]::UtcNow.ToString('yyyyMMddTHHmmssfffZ')),
	$PID
if ([string]::IsNullOrWhiteSpace($EvidenceDirectory)) {
	$EvidenceDirectory = Join-Path (
		Join-Path $projectRoot 'Saved\Validation\CH02Freedom') $runId
}
if (Test-Path -LiteralPath $EvidenceDirectory -PathType Container) {
	if (@(Get-ChildItem -LiteralPath $EvidenceDirectory -Force).Count -gt 0) {
		throw "Evidence directory must be absent or empty: $EvidenceDirectory"
	}
}
else {
	New-Item -ItemType Directory -Path $EvidenceDirectory -Force | Out-Null
}

$editorOutput = @(
	& powershell.exe `
		-NoProfile `
		-ExecutionPolicy Bypass `
		-File $resolver `
		-ProjectPath $projectFile `
		-Commandlet 2>&1
)
if ($LASTEXITCODE -ne 0 -or $editorOutput.Count -eq 0) {
	throw 'Unreal Engine resolution failed.'
}
$editorCommand = ([string]$editorOutput[-1]).Trim()
if (-not $editorCommand.EndsWith(
		'UnrealEditor-Cmd.exe',
		[StringComparison]::OrdinalIgnoreCase)) {
	throw "Headless CH02 validation requires UnrealEditor-Cmd.exe: $editorCommand"
}

function ConvertTo-ProcessArgument {
	param([Parameter(Mandatory = $true)][string]$Value)
	if ($Value -notmatch '[\s"]') {
		return $Value
	}
	return '"' + $Value.Replace('"', '\"') + '"'
}

$cases = @(
	[pscustomobject]@{
		Route = 'P1ThenP2'; Order = 'p1_p2'; P1 = 1; P2 = 1
		Truths = 3; PressureCaps = 2
	},
	[pscustomobject]@{
		Route = 'P2ThenP1'; Order = 'p2_p1'; P1 = 1; P2 = 1
		Truths = 3; PressureCaps = 2
	},
	[pscustomobject]@{
		Route = 'SkipP1'; Order = 'skip_p1'; P1 = 0; P2 = 1
		Truths = 2; PressureCaps = 1
	},
	[pscustomobject]@{
		Route = 'SkipP2'; Order = 'skip_p2'; P1 = 1; P2 = 0
		Truths = 2; PressureCaps = 1
	},
	[pscustomobject]@{
		Route = 'SkipBoth'; Order = 'skip_both'; P1 = 0; P2 = 0
		Truths = 2; PressureCaps = 0
	}
)
$results = [System.Collections.Generic.List[object]]::new()
foreach ($case in $cases) {
	$caseDirectory = Join-Path $EvidenceDirectory "UserData_$($case.Route)"
	New-Item -ItemType Directory -Path $caseDirectory -Force | Out-Null
	$logPath = Join-Path $EvidenceDirectory "$($case.Route).log"
	$arguments = @(
		$projectFile,
		'-game',
		'-unattended',
		'-nosplash',
		'-nullrhi',
		'-nosound',
		'-RenderOffscreen',
		'-stdout',
		'-FullStdOutLogOutput',
		"-abslog=$logPath",
		"-UserDir=$caseDirectory",
		'-IGRebirthEndToEndValidation',
		'-IGRebirthCH02FreedomProbe',
		"-IGRebirthCH02Route=$($case.Route)"
	)
	$process = Start-Process `
		-FilePath $editorCommand `
		-ArgumentList @(
			$arguments |
				ForEach-Object { ConvertTo-ProcessArgument -Value $_ }) `
		-PassThru `
		-WindowStyle Hidden
	try {
		if (-not $process.WaitForExit($TimeoutSeconds * 1000)) {
			$process.Kill()
			[void]$process.WaitForExit(5000)
			throw "$($case.Route) timed out after $TimeoutSeconds seconds."
		}
		$process.Refresh()
		if ($process.ExitCode -ne 0) {
			throw "$($case.Route) exited with code $($process.ExitCode)."
		}
	}
	finally {
		$process.Dispose()
	}
	if (-not (Test-Path -LiteralPath $logPath -PathType Leaf)) {
		throw "$($case.Route) did not create a log."
	}
	$logText = Get-Content -Raw -Encoding UTF8 -LiteralPath $logPath
	$expectedMarkers = @(
		'REBIRTH_E2E PASS ch01_router',
		'REBIRTH_E2E PASS s6_human_gate',
		(
			'REBIRTH_E2E PASS ch02_router p1={0} p2={1} truths={2}' -f
				$case.P1, $case.P2, $case.Truths),
		'authored_housings=2',
		'layered_displays=2',
		'time_entry_physical=2',
		'time_entry_mesh_components=23',
		"pressure_caps=$($case.PressureCaps)",
		"route_order=$($case.Order)",
		'REBIRTH_E2E PASS ch03_handoff',
		"REBIRTH_CH02_FREEDOM PASS route=$($case.Route) handoff=1"
	)
	if ($logText.Contains('REBIRTH_E2E FAIL')) {
		throw "$($case.Route) reported an E2E failure."
	}
	foreach ($marker in $expectedMarkers) {
		if (-not $logText.Contains($marker)) {
			throw "$($case.Route) did not report '$marker'."
		}
	}
	$hash = (Get-FileHash -Algorithm SHA256 -LiteralPath $logPath).Hash
	[void]$results.Add([pscustomobject]@{
		route = $case.Route
		status = 'PASS'
		p1Resolved = [bool]$case.P1
		p2Resolved = [bool]$case.P2
		truthCount = $case.Truths
		pressureCaps = $case.PressureCaps
		physicalContracts = 2
		meshComponents = 23
		authoredHousings = 2
		layeredDisplays = 2
		logPath = $logPath
		logSha256 = $hash
	})
	Write-Host "REBIRTH_CH02_FREEDOM_HARNESS PASS route=$($case.Route)"
}

$commitSha = @(
	& git -C $projectRoot rev-parse --verify HEAD 2>$null
)[-1].Trim()
$summary = [pscustomobject]@{
	schemaVersion = 1
	runId = $runId
	status = 'PASS'
	commitSha = $commitSha
	editor = $editorCommand
	finishedAtUtc = [DateTime]::UtcNow.ToString('o')
	routeCount = $cases.Count
	results = @($results)
}
$summaryPath = Join-Path $EvidenceDirectory 'summary.json'
$summary |
	ConvertTo-Json -Depth 5 |
	Set-Content -LiteralPath $summaryPath -Encoding UTF8
Write-Host (
	"REBIRTH_CH02_FREEDOM_HARNESS PASS complete routes=5 " +
	"summary=$summaryPath")
