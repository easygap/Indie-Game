[CmdletBinding()]
param(
	[Parameter(Mandatory = $true)]
	[string]$ArchiveDirectory,
	[string]$EvidenceDirectory,
	[ValidateRange(30, 600)]
	[int]$TimeoutSeconds = 240
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$startedAtUtc = [DateTime]::UtcNow
$projectRoot = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$archiveRoot = (Resolve-Path -LiteralPath $ArchiveDirectory).Path.TrimEnd(
	[char[]]@('\', '/'))
$archiveLauncher = Join-Path $archiveRoot 'Windows\IndieGame.exe'
$archiveShippingExecutable = Join-Path $archiveRoot (
	'Windows\IndieGame\Binaries\Win64\IndieGame-Win64-Shipping.exe')
if (-not (Test-Path -LiteralPath $archiveLauncher -PathType Leaf) -or
	-not (Test-Path -LiteralPath $archiveShippingExecutable -PathType Leaf)) {
	throw (
		'ArchiveDirectory must be the BuildCookRun archive root containing ' +
		"Windows\IndieGame.exe and the Shipping executable: $archiveRoot")
}
if ([string]::IsNullOrWhiteSpace($EvidenceDirectory)) {
	$runId = [DateTime]::UtcNow.ToString('yyyyMMddTHHmmssfffZ')
	$EvidenceDirectory = Join-Path $projectRoot (
		"Saved\Validation\LensDropletCapture\$runId")
}
$evidenceRoot = [IO.Path]::GetFullPath($EvidenceDirectory)
if (Test-Path -LiteralPath $evidenceRoot) {
	$existingEntries = @(Get-ChildItem -LiteralPath $evidenceRoot -Force)
	if ($existingEntries.Count -gt 0) {
		throw "Lens capture evidence directory must be fresh: $evidenceRoot"
	}
}
else {
	New-Item -ItemType Directory -Force -Path $evidenceRoot | Out-Null
}

function ConvertTo-ProcessArgument {
	param([Parameter(Mandatory = $true)][string]$Value)

	if ($Value -notmatch '[\s"]') {
		return $Value
	}
	return '"' + $Value.Replace('"', '\"') + '"'
}

function Get-ArchiveManifest {
	param([Parameter(Mandatory = $true)][string]$Root)

	$resolvedRoot = (Resolve-Path -LiteralPath $Root).Path.TrimEnd(
		[char[]]@('\', '/'))
	return @(
		Get-ChildItem -LiteralPath $resolvedRoot -Recurse -File |
			Sort-Object FullName |
			ForEach-Object {
				[pscustomobject]@{
					path = $_.FullName.Substring($resolvedRoot.Length).TrimStart(
						[char[]]@('\', '/'))
					sizeBytes = $_.Length
					sha256 = (
						Get-FileHash -Algorithm SHA256 -LiteralPath $_.FullName
					).Hash
				}
			}
	)
}

function Assert-ArchiveUnchanged {
	param(
		[Parameter(Mandatory = $true)][string]$Root,
		[Parameter(Mandatory = $true)][object[]]$Before
	)

	$after = @(Get-ArchiveManifest -Root $Root)
	if ($after.Count -ne $Before.Count) {
		throw (
			'Shipping archive file count changed during lens capture: ' +
			"before=$($Before.Count) after=$($after.Count)")
	}
	for ($index = 0; $index -lt $Before.Count; ++$index) {
		if ($Before[$index].path -cne $after[$index].path -or
			[long]$Before[$index].sizeBytes -ne [long]$after[$index].sizeBytes -or
			$Before[$index].sha256 -cne $after[$index].sha256) {
			throw "Shipping archive changed during lens capture: $($Before[$index].path)"
		}
	}
}

function Get-PngDimensions {
	param([Parameter(Mandatory = $true)][string]$Path)

	$bytes = [IO.File]::ReadAllBytes($Path)
	if ($bytes.Length -lt 24 -or
		$bytes[0] -ne 137 -or
		$bytes[1] -ne 80 -or
		$bytes[2] -ne 78 -or
		$bytes[3] -ne 71) {
		throw "Capture is not a valid PNG: $Path"
	}
	$width = [Net.IPAddress]::NetworkToHostOrder(
		[BitConverter]::ToInt32($bytes, 16))
	$height = [Net.IPAddress]::NetworkToHostOrder(
		[BitConverter]::ToInt32($bytes, 20))
	return [pscustomobject]@{ width = $width; height = $height }
}

function Invoke-LensCaptureCase {
	param(
		[Parameter(Mandatory = $true)]
		[ValidateSet('default', 'reduced')]
		[string]$Mode
	)

	$caseRoot = Join-Path $evidenceRoot $Mode
	$runtimeCopy = Join-Path $caseRoot 'Runtime'
	New-Item -ItemType Directory -Force -Path $runtimeCopy | Out-Null
	$robocopyArguments = @(
		$archiveRoot,
		$runtimeCopy,
		'/E',
		'/COPY:DAT',
		'/DCOPY:DAT',
		'/R:2',
		'/W:1',
		'/XJ',
		'/NFL',
		'/NDL',
		'/NP',
		'/NJH',
		'/NJS'
	)
	& robocopy.exe @robocopyArguments | Out-Null
	if ($LASTEXITCODE -ge 8) {
		throw "Lens capture runtime copy failed ($LASTEXITCODE): $Mode"
	}

	$runtimeExecutables = @(
		Get-ChildItem -LiteralPath $runtimeCopy -Recurse -File |
			Where-Object {
				$_.Name -ieq 'IndieGame-Win64-Shipping.exe' -and $_.Length -gt 0
			}
	)
	if ($runtimeExecutables.Count -ne 1) {
		throw (
			'Lens capture requires exactly one Shipping runtime executable: ' +
			"mode=$Mode found=$($runtimeExecutables.Count)")
	}
	$runtimeExecutable = $runtimeExecutables[0]
	$receiptPath = Join-Path $caseRoot 'receipt.txt'
	$userDirectory = Join-Path $caseRoot 'User'
	New-Item -ItemType Directory -Force -Path $userDirectory | Out-Null
	$arguments = @(
		'/Game/Maps/Prologue_Morning',
		'-game',
		'-unattended',
		'-nosplash',
		'-NoLoadingScreen',
		'-RenderOffscreen',
		'-d3d12',
		'-nosound',
		'-Windowed',
		'-ResX=1280',
		'-ResY=720',
		'-ForceRes',
		'-IGCaptureCH03LensDroplet',
		"-IGCaptureResultPath=$receiptPath",
		"-UserDir=$userDirectory"
	)
	if ($Mode -eq 'reduced') {
		$arguments += '-IGReducedMotion'
	}
	$processArguments = @(
		$arguments | ForEach-Object { ConvertTo-ProcessArgument -Value $_ }
	)
	$process = Start-Process `
		-FilePath $runtimeExecutable.FullName `
		-ArgumentList $processArguments `
		-WorkingDirectory $runtimeExecutable.DirectoryName `
		-PassThru `
		-WindowStyle Hidden
	try {
		if (-not $process.WaitForExit($TimeoutSeconds * 1000)) {
			$process.Kill()
			[void]$process.WaitForExit(5000)
			throw "Lens capture timed out after $TimeoutSeconds seconds: $Mode"
		}
		$process.Refresh()
		if ($process.ExitCode -ne 0) {
			throw "Lens capture exited with code $($process.ExitCode): $Mode"
		}
	}
	finally {
		$process.Dispose()
	}

	if (-not (Test-Path -LiteralPath $receiptPath -PathType Leaf)) {
		throw "Lens capture receipt is missing: $receiptPath"
	}
	$receipt = (
		Get-Content -Raw -Encoding UTF8 -LiteralPath $receiptPath
	).Trim()
	$expectedReduced = if ($Mode -eq 'reduced') { 1 } else { 0 }
	$requiredPrefix =
		"REBIRTH_CH03_LENS_CAPTURE PASS mode=$Mode " +
		"reduced_motion=$expectedReduced stills=2 "
	if (-not $receipt.StartsWith(
			$requiredPrefix,
			[StringComparison]::Ordinal) -or
		$receipt -notmatch 'hud_safe=1$') {
		throw "Lens capture returned an invalid receipt: $receipt"
	}
	$travelMatch = [regex]::Match($receipt, 'travel_y=([0-9]+(?:\.[0-9]+)?)')
	$alphaMatch = [regex]::Match($receipt, 'alpha_min=([0-9]+(?:\.[0-9]+)?)')
	$canvasMatch = [regex]::Match($receipt, 'canvas=([0-9]+)x([0-9]+)')
	if (-not $travelMatch.Success -or
		-not $alphaMatch.Success -or
		-not $canvasMatch.Success) {
		throw "Lens capture receipt metrics are malformed: $receipt"
	}
	$travelY = [double]::Parse(
		$travelMatch.Groups[1].Value,
		[Globalization.CultureInfo]::InvariantCulture)
	$minimumAlpha = [double]::Parse(
		$alphaMatch.Groups[1].Value,
		[Globalization.CultureInfo]::InvariantCulture)
	$canvasWidth = [int]$canvasMatch.Groups[1].Value
	$canvasHeight = [int]$canvasMatch.Groups[2].Value
	if ($canvasWidth -ne 1280 -or $canvasHeight -ne 720) {
		throw "Lens capture canvas mismatch: ${canvasWidth}x${canvasHeight}"
	}
	if ($minimumAlpha -lt 0.55) {
		throw "Lens droplet is too faint: mode=$Mode alpha=$minimumAlpha"
	}
	if (($Mode -eq 'default' -and $travelY -lt 8.0) -or
		($Mode -eq 'reduced' -and $travelY -gt 0.5)) {
		throw "Lens droplet motion contract failed: mode=$Mode travel_y=$travelY"
	}

	$imageDirectory = Join-Path $runtimeCopy 'Windows\IndieGame\Docs\Media'
	$images = @(
		foreach ($phase in @('early', 'late')) {
			$imagePath = Join-Path $imageDirectory (
				"ch03-lens-droplet-$Mode-$phase.png")
			if (-not (Test-Path -LiteralPath $imagePath -PathType Leaf)) {
				throw "Lens capture image is missing: $imagePath"
			}
			$image = Get-Item -LiteralPath $imagePath
			if ($image.Length -lt 10000 -or
				$image.LastWriteTimeUtc -lt $startedAtUtc.AddSeconds(-2)) {
				throw "Lens capture image is empty or stale: $imagePath"
			}
			$dimensions = Get-PngDimensions -Path $imagePath
			if ($dimensions.width -ne 1280 -or $dimensions.height -ne 720) {
				throw (
					"Lens capture PNG dimensions failed: $imagePath " +
					"$($dimensions.width)x$($dimensions.height)")
			}
			[pscustomobject]@{
				phase = $phase
				path = $imagePath
				sizeBytes = $image.Length
				sha256 = (
					Get-FileHash -Algorithm SHA256 -LiteralPath $imagePath
				).Hash
			}
		}
	)
	return [pscustomobject]@{
		mode = $Mode
		reducedMotion = $expectedReduced -eq 1
		travelYPixels = $travelY
		minimumAlpha = $minimumAlpha
		canvas = "${canvasWidth}x${canvasHeight}"
		receipt = $receipt
		receiptPath = $receiptPath
		images = $images
	}
}

$archiveManifestBefore = @(Get-ArchiveManifest -Root $archiveRoot)
if ($archiveManifestBefore.Count -eq 0) {
	throw "Shipping archive is empty: $archiveRoot"
}
$results = @(
	Invoke-LensCaptureCase -Mode 'default'
	Invoke-LensCaptureCase -Mode 'reduced'
)
Assert-ArchiveUnchanged -Root $archiveRoot -Before $archiveManifestBefore

$summaryPath = Join-Path $evidenceRoot 'summary.json'
[pscustomobject]@{
	schemaVersion = 1
	generatedAtUtc = [DateTime]::UtcNow.ToString('o')
	archiveDirectory = $archiveRoot
	archiveFileCount = $archiveManifestBefore.Count
	archiveUnchanged = $true
	results = $results
} | ConvertTo-Json -Depth 7 | Set-Content -LiteralPath $summaryPath -Encoding UTF8
Write-Host (
	'REBIRTH_LENS_CAPTURE_HARNESS PASS modes=2 stills=4 ' +
	"archive_files=$($archiveManifestBefore.Count) summary=$summaryPath")
