[CmdletBinding()]
param(
	[Parameter(Mandatory = $true)]
	[string]$Executable,
	[Parameter(Mandatory = $true)]
	[string]$ExpectedIco,
	[Parameter(Mandatory = $true)]
	[string]$EvidencePng
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

$resolvedExecutable = (Resolve-Path -LiteralPath $Executable).Path
$resolvedExpectedIco = (Resolve-Path -LiteralPath $ExpectedIco).Path
$evidenceDirectory = Split-Path -Parent $EvidencePng
if (-not [string]::IsNullOrWhiteSpace($evidenceDirectory)) {
	New-Item -ItemType Directory -Force -Path $evidenceDirectory | Out-Null
}

Add-Type -AssemblyName System.Drawing
$icoBytes = [IO.File]::ReadAllBytes($resolvedExpectedIco)
$icoEntryCount = [BitConverter]::ToUInt16($icoBytes, 4)
$expectedPayload = $null
for ($entry = 0; $entry -lt $icoEntryCount; $entry++) {
	$entryOffset = 6 + (16 * $entry)
	$entryWidth = if ($icoBytes[$entryOffset] -eq 0) {
		256
	}
	else {
		[int]$icoBytes[$entryOffset]
	}
	$entryHeight = if ($icoBytes[$entryOffset + 1] -eq 0) {
		256
	}
	else {
		[int]$icoBytes[$entryOffset + 1]
	}
	if ($entryWidth -ne 32 -or $entryHeight -ne 32) {
		continue
	}
	$payloadSize = [int][BitConverter]::ToUInt32($icoBytes, $entryOffset + 8)
	$payloadOffset = [int][BitConverter]::ToUInt32($icoBytes, $entryOffset + 12)
	$expectedPayload = New-Object byte[] $payloadSize
	[Array]::Copy($icoBytes, $payloadOffset, $expectedPayload, 0, $payloadSize)
	break
}
if ($null -eq $expectedPayload) {
	throw 'WINDOWS_EXECUTABLE_ICON FAIL: 기준 ICO에 32x32 레벨이 없습니다.'
}
$expectedStream = New-Object IO.MemoryStream(,$expectedPayload)
$expectedBitmap = [System.Drawing.Bitmap][System.Drawing.Image]::FromStream(
	$expectedStream)
$actualIcon = [System.Drawing.Icon]::ExtractAssociatedIcon($resolvedExecutable)
if ($null -eq $actualIcon) {
	$expectedBitmap.Dispose()
	$expectedStream.Dispose()
	throw "WINDOWS_EXECUTABLE_ICON FAIL: 실행 파일 아이콘을 읽을 수 없습니다: $resolvedExecutable"
}

try {
	$actualBitmap = $actualIcon.ToBitmap()
	try {
		if ($expectedBitmap.Width -ne 32 -or $expectedBitmap.Height -ne 32) {
			throw "WINDOWS_EXECUTABLE_ICON FAIL: 기준 ICO의 32px 레벨을 읽지 못했습니다."
		}
		if ($actualBitmap.Width -ne 32 -or $actualBitmap.Height -ne 32) {
			throw (
				'WINDOWS_EXECUTABLE_ICON FAIL: Shipping EXE 대표 아이콘이 ' +
				"32x32가 아닙니다: $($actualBitmap.Width)x$($actualBitmap.Height)")
		}

		$matchedPixels = 0
		for ($y = 0; $y -lt 32; $y++) {
			for ($x = 0; $x -lt 32; $x++) {
				if ($expectedBitmap.GetPixel($x, $y).ToArgb() -ne
					$actualBitmap.GetPixel($x, $y).ToArgb()) {
					throw (
						'WINDOWS_EXECUTABLE_ICON FAIL: Shipping EXE가 기준 ICO와 ' +
						"다릅니다: pixel=($x,$y)")
				}
				$matchedPixels++
			}
		}
		$actualBitmap.Save(
			$EvidencePng,
			[System.Drawing.Imaging.ImageFormat]::Png)
	}
	finally {
		$actualBitmap.Dispose()
	}
}
finally {
	$actualIcon.Dispose()
	$expectedBitmap.Dispose()
	$expectedStream.Dispose()
}

$evidenceHash = (Get-FileHash -Algorithm SHA256 -LiteralPath $EvidencePng).Hash
Write-Host (
	"WINDOWS_EXECUTABLE_ICON PASS size=32 matched_pixels=$matchedPixels " +
	"evidence_sha256=$evidenceHash evidence=$EvidencePng"
) -ForegroundColor Green
