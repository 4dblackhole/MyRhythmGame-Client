param()
$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path -Parent $PSScriptRoot
$problems = [System.Collections.Generic.List[string]]::new()
$projectPaths = @(& rg --files $repoRoot -g '*.vcxproj' -g '!Dependencies/**' -g '!build/**' -g '!bin/**' |
    Where-Object { $_ -notmatch '[\\/]Dependencies[\\/]' })
$edges = @{}
foreach ($projectPath in $projectPaths) {
    $directory = Split-Path -Parent $projectPath
    [xml]$project = Get-Content -Raw -LiteralPath $projectPath
    [xml]$filters = Get-Content -Raw -LiteralPath ($projectPath + '.filters')
    $entries = @($project.SelectNodes('//*[local-name()="ClCompile" or local-name()="ClInclude"][@Include]'))
    $filterEntries = @($filters.SelectNodes('//*[local-name()="ClCompile" or local-name()="ClInclude"][@Include]'))
    foreach ($entry in $entries) {
        $source = Join-Path $directory $entry.Include
        if (!(Test-Path -LiteralPath $source)) { $problems.Add("Missing source: $source") }
        $matching = @($filterEntries | Where-Object { $_.LocalName -eq $entry.LocalName -and $_.Include -eq $entry.Include })
        if ($matching.Count -ne 1) { $problems.Add("Missing/duplicate filter: $projectPath / $($entry.Include)"); continue }
        $expected = Split-Path -Parent $entry.Include
        $actual = [string]$matching[0].Filter
        if ($actual -ne $expected) { $problems.Add("Filter differs from directory: $($entry.Include): $actual != $expected") }
    }
    foreach ($entry in $filterEntries) {
        if (!($entries | Where-Object { $_.LocalName -eq $entry.LocalName -and $_.Include -eq $entry.Include })) {
            $problems.Add("Stale filter: $projectPath / $($entry.Include)")
        }
    }
    $references = @($project.SelectNodes('//*[local-name()="ProjectReference"]'))
    $edges[[IO.Path]::GetFullPath($projectPath)] = @($references | ForEach-Object {
        [IO.Path]::GetFullPath((Join-Path $directory $_.Include))
    })
    foreach ($reference in $edges[[IO.Path]::GetFullPath($projectPath)]) {
        if (!(Test-Path -LiteralPath $reference)) { $problems.Add("Missing project reference: $reference") }
        if ($directory -match 'FingerDrum\.(Rhythm|Chart|Modes|Editor)$' -and $reference -match '[\\/](Client|Dependencies)[\\/]') {
            $problems.Add("Domain depends on Client/engine: $projectPath -> $reference")
        }
    }
}
function Visit-Project([string]$path, [string[]]$ancestors) {
    if ($ancestors -contains $path) { $problems.Add("Project cycle: $($ancestors -join ' -> ') -> $path"); return }
    if ($edges.ContainsKey($path)) {
        foreach ($target in $edges[$path]) { Visit-Project $target ($ancestors + $path) }
    }
}
foreach ($projectPath in $edges.Keys) { Visit-Project $projectPath @() }

$sources = @(& rg --files $repoRoot -g '*.cpp' -g '*.h' -g '!Dependencies/**' -g '!build/**' -g '!bin/**' |
    Where-Object { $_ -notmatch '[\\/](Dependencies|build|bin)[\\/]' })
foreach ($source in $sources) {
    $body = Get-Content -Raw -LiteralPath $source
    if ($body -match '#include\s*[<"][^">]+\.cpp[">]') { $problems.Add("Implementation include: $source") }
    if ($source -match '[\\/]FingerDrum\.(Rhythm|Chart|Modes|Editor)[\\/]' -and
        $body -match '#include\s*[<"](?:MRG_Core|.*(?:Client/|Client\\|Dependencies/|Engine/))') {
        $problems.Add("Domain includes Client/engine: $source")
    }
    if ($source -match '[\\/]Client[\\/]' -and
        $body -match '#include\s*[<"][^">]*(?:MRG-Engine|Engine/|Engine\\)[^">]*[">]') {
        $problems.Add("Client bypasses MRG_Core.h: $source")
    }
    if ($source -notmatch '[\\/]Examples[\\/]' -and
        $body -match 'class\s+(\w+Scene)\s+final\s*:\s*public\s+mrg::scene::GameScene') {
        if ((Split-Path -Leaf (Split-Path -Parent $source)) -ne $Matches[1]) {
            $problems.Add("Scene lacks its own directory: $source")
        }
    }
}
if ($problems.Count) { throw ($problems -join "`n") }
Write-Output "Architecture OK: $($projectPaths.Count) projects; source/filter paths, project cycles and engine boundaries checked."
