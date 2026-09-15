param(
    [Parameter(Mandatory = $true)]
    [string] $ProjectDirectory,

    [Parameter(Mandatory = $true)]
    [string] $OutputDirectory
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

function Get-Sha256([string] $Path) {
    $algorithm = [Security.Cryptography.SHA256]::Create()
    $inputStream = [IO.File]::OpenRead($Path)
    try {
        return [BitConverter]::ToString(
            $algorithm.ComputeHash($inputStream)).Replace('-', '').ToLowerInvariant()
    }
    finally {
        $inputStream.Dispose()
        $algorithm.Dispose()
    }
}

$assetProjectRoot = [IO.Path]::GetFullPath($ProjectDirectory)
$generatedRoot = [IO.Path]::GetFullPath($OutputDirectory)
$packPath = Join-Path $generatedRoot 'BuiltInAssets.fdpak'
$resourcePath = Join-Path $generatedRoot 'FingerDrum.Assets.rc'
$temporaryPack = $packPath + '.tmp'

$sourceGroups = @(
    @{
        Source = Join-Path $assetProjectRoot 'Assets\Fonts'
        Logical = 'assets/fonts'
        Filter = '*'
    },
    @{
        Source = Join-Path $assetProjectRoot 'Assets\Skins\Default Skin'
        Logical = 'assets/skins/Default Skin'
        Filter = '*'
    },
    @{
        Source = Join-Path $assetProjectRoot 'Assets\Songs\Music\Angeldream'
        Logical = 'assets/songs/Music/Angeldream'
        Filter = '*'
    },
    @{
        Source = Join-Path $assetProjectRoot 'Assets\Songs\Pattern\angeldream'
        Logical = 'assets/songs/Pattern/angeldream'
        Filter = '*.ymp'
    }
)

$entries = [Collections.Generic.List[object]]::new()
foreach ($group in $sourceGroups) {
    if (-not (Test-Path -LiteralPath $group.Source -PathType Container)) {
        throw "Built-in asset directory is missing: $($group.Source)"
    }
    $sourcePrefix = [IO.Path]::GetFullPath($group.Source).TrimEnd('\') + '\'
    foreach ($file in Get-ChildItem -LiteralPath $group.Source -File -Recurse -Filter $group.Filter) {
        if (-not $file.FullName.StartsWith($sourcePrefix, [StringComparison]::OrdinalIgnoreCase)) {
            throw "Built-in asset escaped its source directory: $($file.FullName)"
        }
        $relative = $file.FullName.Substring($sourcePrefix.Length).Replace('\', '/')
        $logicalPath = ($group.Logical + '/' + $relative).Replace('//', '/')
        $pathBytes = [Text.Encoding]::UTF8.GetBytes($logicalPath)
        $entries.Add([pscustomobject]@{
            SourcePath = $file.FullName
            LogicalPath = $logicalPath
            PathBytes = $pathBytes
            Size = [UInt64]$file.Length
            Offset = [UInt64]0
        })
    }
}

$entries = @($entries | Sort-Object -Property LogicalPath -CaseSensitive)
if ($entries.Count -eq 0) {
    throw 'The built-in asset pack cannot be empty.'
}

[UInt64]$headerSize = 8 + 4 + 4
foreach ($entry in $entries) {
    $headerSize += 4 + $entry.PathBytes.Length + 8 + 8
}
[UInt64]$offset = $headerSize
foreach ($entry in $entries) {
    $entry.Offset = $offset
    $offset += $entry.Size
}

New-Item -ItemType Directory -Force -Path $generatedRoot | Out-Null
$stream = [IO.File]::Open($temporaryPack, [IO.FileMode]::Create, [IO.FileAccess]::Write, [IO.FileShare]::None)
try {
    $writer = [IO.BinaryWriter]::new($stream, [Text.Encoding]::UTF8, $true)
    try {
        $writer.Write([Text.Encoding]::ASCII.GetBytes('FDRPAK01'))
        $writer.Write([UInt32]1)
        $writer.Write([UInt32]$entries.Count)
        foreach ($entry in $entries) {
            $writer.Write([UInt32]$entry.PathBytes.Length)
            $writer.Write([byte[]]$entry.PathBytes)
            $writer.Write([UInt64]$entry.Offset)
            $writer.Write([UInt64]$entry.Size)
        }
        foreach ($entry in $entries) {
            $writer.Write([IO.File]::ReadAllBytes($entry.SourcePath))
        }
    }
    finally {
        $writer.Dispose()
    }
}
finally {
    $stream.Dispose()
}

$packHash = Get-Sha256 $temporaryPack
$replacePack = -not (Test-Path -LiteralPath $packPath -PathType Leaf)
if (-not $replacePack) {
    $currentHash = Get-Sha256 $packPath
    $replacePack = $currentHash -ne $packHash
}
if ($replacePack) {
    Move-Item -LiteralPath $temporaryPack -Destination $packPath -Force
}
else {
    Remove-Item -LiteralPath $temporaryPack -Force
}

$resourcePackPath = $packPath.Replace('\', '/')
$resourceText = "// Generated from FingerDrum.Assets. Pack SHA-256: $packHash`r`n101 RCDATA `"$resourcePackPath`"`r`n"
$currentResourceText = if (Test-Path -LiteralPath $resourcePath -PathType Leaf) {
    [IO.File]::ReadAllText($resourcePath)
} else {
    $null
}
if ($currentResourceText -ne $resourceText) {
    [IO.File]::WriteAllText($resourcePath, $resourceText, [Text.Encoding]::ASCII)
}

Write-Output "Packed $($entries.Count) built-in assets into $packPath"
