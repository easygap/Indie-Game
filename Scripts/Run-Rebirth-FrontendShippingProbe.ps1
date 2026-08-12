[CmdletBinding()]
param(
	[Parameter(Mandatory = $true)]
	[string]$ArchiveDirectory,
	[string]$EvidenceDirectory,
	[ValidateRange(20, 180)]
	[int]$TimeoutSeconds = 60
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$projectRoot = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$archiveRoot = (Resolve-Path -LiteralPath $ArchiveDirectory).Path.TrimEnd(
	[char[]]@('\', '/'))
$launcher = Join-Path $archiveRoot 'Windows\IndieGame.exe'
$shippingExecutable = Join-Path $archiveRoot (
	'Windows\IndieGame\Binaries\Win64\IndieGame-Win64-Shipping.exe')
if (-not (Test-Path -LiteralPath $launcher -PathType Leaf) -or
	-not (Test-Path -LiteralPath $shippingExecutable -PathType Leaf)) {
	throw (
		'ArchiveDirectory must be a BuildCookRun archive root containing ' +
		"Windows\IndieGame.exe and the Shipping executable: $archiveRoot")
}
if ([string]::IsNullOrWhiteSpace($EvidenceDirectory)) {
	$runId = [DateTime]::UtcNow.ToString('yyyyMMddTHHmmssfffZ')
	$EvidenceDirectory = Join-Path $projectRoot (
		"Saved\Validation\RebirthFrontendShipping\$runId")
}
$evidenceRoot = [IO.Path]::GetFullPath($EvidenceDirectory)
if (Test-Path -LiteralPath $evidenceRoot) {
	$existingEntries = @(Get-ChildItem -LiteralPath $evidenceRoot -Force)
	if ($existingEntries.Count -gt 0) {
		throw "Frontend evidence directory must be fresh: $evidenceRoot"
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

function Get-PngDimensions {
	param([Parameter(Mandatory = $true)][string]$Path)

	$bytes = [IO.File]::ReadAllBytes($Path)
	if ($bytes.Length -lt 24 -or
		$bytes[0] -ne 137 -or
		$bytes[1] -ne 80 -or
		$bytes[2] -ne 78 -or
		$bytes[3] -ne 71) {
		throw "Dialogue capture is not a valid PNG: $Path"
	}
	$width = [Net.IPAddress]::NetworkToHostOrder(
		[BitConverter]::ToInt32($bytes, 16))
	$height = [Net.IPAddress]::NetworkToHostOrder(
		[BitConverter]::ToInt32($bytes, 20))
	return [pscustomobject]@{ width = $width; height = $height }
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
			'Shipping archive file count changed during frontend validation: ' +
			"before=$($Before.Count) after=$($after.Count)")
	}
	for ($index = 0; $index -lt $Before.Count; ++$index) {
		if ($Before[$index].path -cne $after[$index].path -or
			[long]$Before[$index].sizeBytes -ne [long]$after[$index].sizeBytes -or
			$Before[$index].sha256 -cne $after[$index].sha256) {
			throw (
				'Shipping archive changed during frontend validation: ' +
				$Before[$index].path)
		}
	}
}

function Invoke-FrontendCase {
	param(
		[Parameter(Mandatory = $true)][int]$Width,
		[Parameter(Mandatory = $true)][int]$Height
	)

	$caseName = "${Width}x${Height}"
	$caseRoot = Join-Path $evidenceRoot $caseName
	$userDirectory = Join-Path $caseRoot 'User'
	$receiptPath = Join-Path $caseRoot 'receipt.txt'
	$accessibilityScreenshotPath = Join-Path $caseRoot 'settings-accessibility.png'
	$displayScreenshotPath = Join-Path $caseRoot 'settings-display.png'
	$titleScreenshotPath = Join-Path $caseRoot 'title-first-run.png'
	$defaultScreenshotPath = Join-Path $caseRoot 'dialogue-default.png'
	$screenshotPath = Join-Path $caseRoot 'dialogue.png'
	New-Item -ItemType Directory -Force -Path $userDirectory | Out-Null
	$startedAtUtc = [DateTime]::UtcNow
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
		"-ResX=$Width",
		"-ResY=$Height",
		'-ForceRes',
		'-NoVSync',
		'-IGFrontendShippingProbe',
		"-IGFrontendExpectedWidth=$Width",
		"-IGFrontendExpectedHeight=$Height",
		"-IGFrontendResultPath=$receiptPath",
		"-IGFrontendAccessibilityScreenshotPath=$accessibilityScreenshotPath",
		"-IGFrontendDisplayScreenshotPath=$displayScreenshotPath",
		"-IGFrontendTitleScreenshotPath=$titleScreenshotPath",
		"-IGFrontendDefaultScreenshotPath=$defaultScreenshotPath",
		"-IGFrontendScreenshotPath=$screenshotPath",
		"-UserDir=$userDirectory"
	)
	$processArguments = @(
		$arguments | ForEach-Object { ConvertTo-ProcessArgument -Value $_ }
	)
	$process = Start-Process `
		-FilePath $launcher `
		-ArgumentList $processArguments `
		-WorkingDirectory (Split-Path -Parent $launcher) `
		-PassThru `
		-WindowStyle Hidden
	try {
		if (-not $process.WaitForExit($TimeoutSeconds * 1000)) {
			$process.Kill()
			[void]$process.WaitForExit(5000)
			throw "Frontend Shipping probe timed out: $caseName"
		}
		$process.Refresh()
		if ($process.ExitCode -ne 0) {
			$failureReceipt = if (Test-Path -LiteralPath $receiptPath -PathType Leaf) {
				(Get-Content -Raw -Encoding UTF8 -LiteralPath $receiptPath).Trim()
			}
			else {
				'<missing>'
			}
			throw (
				"Frontend Shipping probe exited with code $($process.ExitCode): " +
				"$caseName receipt=$failureReceipt")
		}
	}
	finally {
		$process.Dispose()
	}

	if (-not (Test-Path -LiteralPath $receiptPath -PathType Leaf)) {
		throw "Frontend Shipping receipt is missing: $receiptPath"
	}
	if (-not (Test-Path -LiteralPath $screenshotPath -PathType Leaf)) {
		throw "Frontend Shipping dialogue screenshot is missing: $screenshotPath"
	}
	if (-not (Test-Path -LiteralPath $defaultScreenshotPath -PathType Leaf)) {
		throw "Frontend Shipping default dialogue screenshot is missing: $defaultScreenshotPath"
	}
	if (-not (Test-Path -LiteralPath $titleScreenshotPath -PathType Leaf)) {
		throw "Frontend Shipping first-run title screenshot is missing: $titleScreenshotPath"
	}
	foreach ($settingsScreenshotPath in @(
		$accessibilityScreenshotPath,
		$displayScreenshotPath
	)) {
		if (-not (Test-Path -LiteralPath $settingsScreenshotPath -PathType Leaf)) {
			throw "Frontend Shipping settings screenshot is missing: $settingsScreenshotPath"
		}
		$settingsDimensions = Get-PngDimensions -Path $settingsScreenshotPath
		if ($settingsDimensions.width -ne $Width -or
			$settingsDimensions.height -ne $Height) {
			throw (
				'Frontend Shipping settings screenshot dimensions failed: ' +
				"$caseName path=$settingsScreenshotPath")
		}
	}
	$defaultScreenshotDimensions = Get-PngDimensions -Path $defaultScreenshotPath
	if ($defaultScreenshotDimensions.width -ne $Width -or
		$defaultScreenshotDimensions.height -ne $Height) {
		throw (
			'Frontend Shipping default dialogue screenshot dimensions failed: ' +
			"$caseName actual=$($defaultScreenshotDimensions.width)x$($defaultScreenshotDimensions.height)")
	}
	$titleScreenshotDimensions = Get-PngDimensions -Path $titleScreenshotPath
	if ($titleScreenshotDimensions.width -ne $Width -or
		$titleScreenshotDimensions.height -ne $Height) {
		throw (
			'Frontend Shipping first-run title screenshot dimensions failed: ' +
			"$caseName actual=$($titleScreenshotDimensions.width)x$($titleScreenshotDimensions.height)")
	}
	$screenshotDimensions = Get-PngDimensions -Path $screenshotPath
	if ($screenshotDimensions.width -ne $Width -or
		$screenshotDimensions.height -ne $Height) {
		throw (
			'Frontend Shipping dialogue screenshot dimensions failed: ' +
			"$caseName actual=$($screenshotDimensions.width)x$($screenshotDimensions.height)")
	}
	$receiptFile = Get-Item -LiteralPath $receiptPath
	if ($receiptFile.LastWriteTimeUtc -lt $startedAtUtc.AddSeconds(-2)) {
		throw "Frontend Shipping receipt is stale: $receiptPath"
	}
	$receipt = (
		Get-Content -Raw -Encoding UTF8 -LiteralPath $receiptPath
	).Trim()
	$pattern = (
		'^REBIRTH_FRONTEND PASS contract=4 ' +
		"resolution=${Width}x${Height} " +
		'keyboard_access=1 gamepad_access=1 dpad_down=1 ' +
		'keyboard_up=1 gamepad_close=1 keyboard_pause=1 ' +
		'gamepad_pause=1 display=1 title=1 first_run=1 ' +
		'dialogue=1 dialogue_default=1 ' +
		'speaker=1 continuation=1 default_scale=100 max_scale=200 ' +
		'sound_lane=1 samples=11 elements_min=([0-9]+) ' +
		'input_events=11 bounds=(-?[0-9]+),(-?[0-9]+),(-?[0-9]+),(-?[0-9]+)$')
	$match = [regex]::Match($receipt, $pattern)
	if (-not $match.Success) {
		throw "Frontend Shipping receipt contract failed: $receipt"
	}
	$minimumElements = [int]$match.Groups[1].Value
	$minimumX = [int]$match.Groups[2].Value
	$minimumY = [int]$match.Groups[3].Value
	$maximumX = [int]$match.Groups[4].Value
	$maximumY = [int]$match.Groups[5].Value
	if ($minimumElements -lt 8 -or
		$minimumX -lt -1 -or $minimumY -lt -1 -or
		$maximumX -gt ($Width + 1) -or $maximumY -gt ($Height + 1)) {
		throw "Frontend Shipping HUD bounds failed: $receipt"
	}
	return [pscustomobject]@{
		resolution = $caseName
		minimumElements = $minimumElements
		bounds = [pscustomobject]@{
			minimumX = $minimumX
			minimumY = $minimumY
			maximumX = $maximumX
			maximumY = $maximumY
		}
		inputEvents = 11
		layoutSamples = 11
		receipt = $receipt
		receiptPath = $receiptPath
		receiptSha256 = (
			Get-FileHash -Algorithm SHA256 -LiteralPath $receiptPath
		).Hash
		accessibilityScreenshotPath = $accessibilityScreenshotPath
		accessibilityScreenshotSha256 = (
			Get-FileHash -Algorithm SHA256 -LiteralPath $accessibilityScreenshotPath
		).Hash
		displayScreenshotPath = $displayScreenshotPath
		displayScreenshotSha256 = (
			Get-FileHash -Algorithm SHA256 -LiteralPath $displayScreenshotPath
		).Hash
		titleScreenshotPath = $titleScreenshotPath
		titleScreenshotWidth = $titleScreenshotDimensions.width
		titleScreenshotHeight = $titleScreenshotDimensions.height
		titleScreenshotSha256 = (
			Get-FileHash -Algorithm SHA256 -LiteralPath $titleScreenshotPath
		).Hash
		dialogueScreenshotPath = $screenshotPath
		dialogueScreenshotWidth = $screenshotDimensions.width
		dialogueScreenshotHeight = $screenshotDimensions.height
		dialogueScreenshotSha256 = (
			Get-FileHash -Algorithm SHA256 -LiteralPath $screenshotPath
		).Hash
		defaultDialogueScreenshotPath = $defaultScreenshotPath
		defaultDialogueScreenshotWidth = $defaultScreenshotDimensions.width
		defaultDialogueScreenshotHeight = $defaultScreenshotDimensions.height
		defaultDialogueScreenshotSha256 = (
			Get-FileHash -Algorithm SHA256 -LiteralPath $defaultScreenshotPath
		).Hash
	}
}

$archiveManifestBefore = @(Get-ArchiveManifest -Root $archiveRoot)
if ($archiveManifestBefore.Count -eq 0) {
	throw "Shipping archive is empty: $archiveRoot"
}
$resolutions = @(
	[pscustomobject]@{ width = 1280; height = 720 },
	[pscustomobject]@{ width = 1600; height = 900 },
	[pscustomobject]@{ width = 1920; height = 1080 },
	[pscustomobject]@{ width = 2560; height = 1440 }
)
$results = @(
	foreach ($resolution in $resolutions) {
		Invoke-FrontendCase `
			-Width $resolution.width `
			-Height $resolution.height
	}
)
Assert-ArchiveUnchanged -Root $archiveRoot -Before $archiveManifestBefore

$summaryPath = Join-Path $evidenceRoot 'summary.json'
[pscustomobject]@{
	schemaVersion = 4
	generatedAtUtc = [DateTime]::UtcNow.ToString('o')
	archiveDirectory = $archiveRoot
	archiveFileCount = $archiveManifestBefore.Count
	archiveUnchanged = $true
	shippingExecutable = $shippingExecutable
	shippingExecutableSha256 = (
		Get-FileHash -Algorithm SHA256 -LiteralPath $shippingExecutable
	).Hash
	resolutionCount = $results.Count
	dialogueCaseCount = $results.Count * 2
	titleCaseCount = $results.Count
	inputEventCount = ($results | Measure-Object -Property inputEvents -Sum).Sum
	layoutSampleCount = ($results | Measure-Object -Property layoutSamples -Sum).Sum
	results = $results
} | ConvertTo-Json -Depth 6 | Set-Content -LiteralPath $summaryPath -Encoding UTF8
Write-Host (
	'REBIRTH_FRONTEND_SHIPPING PASS resolutions=4 input_events=44 ' +
	"layout_samples=44 dialogue_cases=8 title_cases=4 archive_files=$($archiveManifestBefore.Count) " +
	"summary=$summaryPath") -ForegroundColor Green
