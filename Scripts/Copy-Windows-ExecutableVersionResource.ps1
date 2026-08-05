[CmdletBinding()]
param(
	[Parameter(Mandatory = $true)]
	[string]$SourceExecutable,
	[Parameter(Mandatory = $true)]
	[string]$DestinationExecutable
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

foreach ($candidate in @($SourceExecutable, $DestinationExecutable)) {
	if (-not (Test-Path -LiteralPath $candidate -PathType Leaf)) {
		throw "Executable does not exist: $candidate"
	}
	if ((Get-Item -LiteralPath $candidate).Length -le 0) {
		throw "Executable is empty: $candidate"
	}
}

$resolvedSource = (Resolve-Path -LiteralPath $SourceExecutable).Path
$resolvedDestination = (Resolve-Path -LiteralPath $DestinationExecutable).Path
if ($resolvedSource -ieq $resolvedDestination) {
	throw 'Source and destination executables must be different files.'
}

if (-not ('IGWindowsVersionResource' -as [type])) {
	Add-Type -TypeDefinition @'
using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Runtime.InteropServices;
using System.Text;

public static class IGWindowsVersionResource
{
    private const uint LoadLibraryAsDataFile = 0x00000002;
    private const uint LoadLibraryAsImageResource = 0x00000020;
    private static readonly IntPtr VersionType = new IntPtr(16);
    private static readonly IntPtr VersionName = new IntPtr(1);

    private delegate bool EnumResourceLanguagesCallback(
        IntPtr module,
        IntPtr type,
        IntPtr name,
        ushort language,
        IntPtr parameter);

    [DllImport("kernel32.dll", CharSet = CharSet.Unicode, SetLastError = true)]
    private static extern IntPtr LoadLibraryEx(
        string fileName,
        IntPtr file,
        uint flags);

    [DllImport("kernel32.dll", SetLastError = true)]
    private static extern bool FreeLibrary(IntPtr module);

    [DllImport("kernel32.dll", CharSet = CharSet.Unicode, SetLastError = true)]
    private static extern bool EnumResourceLanguages(
        IntPtr module,
        IntPtr type,
        IntPtr name,
        EnumResourceLanguagesCallback callback,
        IntPtr parameter);

    [DllImport("kernel32.dll", CharSet = CharSet.Unicode, SetLastError = true)]
    private static extern IntPtr FindResourceEx(
        IntPtr module,
        IntPtr type,
        IntPtr name,
        ushort language);

    [DllImport("kernel32.dll", SetLastError = true)]
    private static extern IntPtr LoadResource(IntPtr module, IntPtr resource);

    [DllImport("kernel32.dll", SetLastError = true)]
    private static extern IntPtr LockResource(IntPtr resourceData);

    [DllImport("kernel32.dll", SetLastError = true)]
    private static extern uint SizeofResource(IntPtr module, IntPtr resource);

    [DllImport("kernel32.dll", CharSet = CharSet.Unicode, SetLastError = true)]
    private static extern IntPtr BeginUpdateResource(
        string fileName,
        [MarshalAs(UnmanagedType.Bool)] bool deleteExistingResources);

    [DllImport("kernel32.dll", CharSet = CharSet.Unicode, SetLastError = true)]
    private static extern bool UpdateResource(
        IntPtr update,
        IntPtr type,
        IntPtr name,
        ushort language,
        byte[] data,
        uint dataSize);

    [DllImport("kernel32.dll", SetLastError = true)]
    private static extern bool EndUpdateResource(
        IntPtr update,
        [MarshalAs(UnmanagedType.Bool)] bool discard);

    private static Win32Exception LastError(string operation)
    {
        return new Win32Exception(
            Marshal.GetLastWin32Error(),
            operation + " failed");
    }

    private static void ReplaceUtf16Value(
        byte[] data,
        string sourceValue,
        string destinationValue)
    {
        if (String.IsNullOrEmpty(sourceValue))
        {
            throw new InvalidOperationException(
                "The source VERSIONINFO has no OriginalFilename value");
        }
        if (destinationValue.Length > sourceValue.Length)
        {
            throw new InvalidOperationException(
                "Destination OriginalFilename is longer than the source value");
        }

        byte[] sourceBytes = Encoding.Unicode.GetBytes(sourceValue + "\0");
        byte[] destinationBytes = Encoding.Unicode.GetBytes(
            destinationValue + "\0");
        int matchOffset = -1;
        for (int offset = 0; offset <= data.Length - sourceBytes.Length; offset += 2)
        {
            bool matches = true;
            for (int index = 0; index < sourceBytes.Length; index++)
            {
                if (data[offset + index] != sourceBytes[index])
                {
                    matches = false;
                    break;
                }
            }
            if (matches)
            {
                if (matchOffset >= 0)
                {
                    throw new InvalidOperationException(
                        "OriginalFilename occurs more than once in VERSIONINFO");
                }
                matchOffset = offset;
            }
        }
        if (matchOffset < 0)
        {
            throw new InvalidOperationException(
                "OriginalFilename value was not found in VERSIONINFO");
        }

        Array.Clear(data, matchOffset, sourceBytes.Length);
        Buffer.BlockCopy(
            destinationBytes,
            0,
            data,
            matchOffset,
            destinationBytes.Length);
    }

    public static ushort Copy(
        string source,
        string destination,
        string sourceOriginalFilename,
        string destinationOriginalFilename)
    {
        IntPtr module = LoadLibraryEx(
            source,
            IntPtr.Zero,
            LoadLibraryAsDataFile | LoadLibraryAsImageResource);
        if (module == IntPtr.Zero)
        {
            throw LastError("LoadLibraryEx");
        }

        byte[] versionBytes;
        ushort sourceLanguage;
        try
        {
            List<ushort> languages = new List<ushort>();
            EnumResourceLanguagesCallback callback =
                delegate(IntPtr ignoredModule, IntPtr ignoredType,
                    IntPtr ignoredName, ushort language, IntPtr ignoredParameter)
                {
                    languages.Add(language);
                    return true;
                };
            if (!EnumResourceLanguages(
                module, VersionType, VersionName, callback, IntPtr.Zero) ||
                languages.Count == 0)
            {
                throw LastError("EnumResourceLanguages");
            }

            sourceLanguage = languages.Contains(1033)
                ? (ushort)1033
                : languages[0];
            IntPtr resource = FindResourceEx(
                module, VersionType, VersionName, sourceLanguage);
            if (resource == IntPtr.Zero)
            {
                throw LastError("FindResourceEx");
            }
            uint size = SizeofResource(module, resource);
            if (size == 0)
            {
                throw LastError("SizeofResource");
            }
            IntPtr loadedResource = LoadResource(module, resource);
            if (loadedResource == IntPtr.Zero)
            {
                throw LastError("LoadResource");
            }
            IntPtr resourceData = LockResource(loadedResource);
            if (resourceData == IntPtr.Zero)
            {
                throw new InvalidOperationException("LockResource returned null");
            }
            versionBytes = new byte[size];
            Marshal.Copy(resourceData, versionBytes, 0, checked((int)size));
            ReplaceUtf16Value(
                versionBytes,
                sourceOriginalFilename,
                destinationOriginalFilename);
        }
        finally
        {
            FreeLibrary(module);
        }

        IntPtr update = BeginUpdateResource(destination, false);
        if (update == IntPtr.Zero)
        {
            throw LastError("BeginUpdateResource");
        }
        bool committed = false;
        try
        {
            if (!UpdateResource(
                update,
                VersionType,
                VersionName,
                sourceLanguage,
                versionBytes,
                checked((uint)versionBytes.Length)))
            {
                throw LastError("UpdateResource");
            }
            if (!EndUpdateResource(update, false))
            {
                throw LastError("EndUpdateResource");
            }
            committed = true;
            return sourceLanguage;
        }
        finally
        {
            if (!committed)
            {
                EndUpdateResource(update, true);
            }
        }
    }
}
'@
}

$sourceVersion = (Get-Item -LiteralPath $resolvedSource).VersionInfo
$destinationOriginalFilename = [IO.Path]::GetFileName($resolvedDestination)
$resourceLanguage = [IGWindowsVersionResource]::Copy(
	$resolvedSource,
	$resolvedDestination,
	[string]$sourceVersion.OriginalFilename,
	$destinationOriginalFilename)
$destinationVersion = (Get-Item -LiteralPath $resolvedDestination).VersionInfo

$metadataFields = @(
	'FileDescription',
	'FileVersion',
	'ProductName',
	'ProductVersion',
	'CompanyName',
	'LegalCopyright',
	'InternalName'
)
foreach ($field in $metadataFields) {
	$sourceValue = ([string]$sourceVersion.$field).Trim()
	$destinationValue = ([string]$destinationVersion.$field).Trim()
	if ($sourceValue -cne $destinationValue) {
		throw (
			"Copied metadata mismatch for ${field}: " +
			"source='$sourceValue' destination='$destinationValue'")
	}
}
if (([string]$destinationVersion.OriginalFilename).Trim() -cne
	$destinationOriginalFilename) {
	throw (
		"Copied metadata mismatch for OriginalFilename: expected=" +
		"'$destinationOriginalFilename' actual=" +
		"'$($destinationVersion.OriginalFilename)'")
}

$sha256 = (
	Get-FileHash -Algorithm SHA256 -LiteralPath $resolvedDestination
).Hash
Write-Host (
	'WINDOWS_EXECUTABLE_METADATA_SYNC PASS ' +
	"language=$resourceLanguage " +
	"version='$($destinationVersion.ProductVersion)' " +
	"sha256=$sha256"
) -ForegroundColor Green
