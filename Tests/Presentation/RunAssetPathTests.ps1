param([ValidateSet('Debug', 'Release')][string]$Configuration = 'Debug')
$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$testRoot = Join-Path $repoRoot ('build/asset-path-tests/' + $Configuration + '-' + [guid]::NewGuid().ToString('N'))
$objects = Join-Path $testRoot 'obj'
New-Item -ItemType Directory -Path $objects -Force | Out-Null
$runtime = if ($Configuration -eq 'Debug') { @('/MDd', '/D_DEBUG') } else { @('/MD', '/DNDEBUG') }
$compiledExe = Join-Path $testRoot 'AssetPathTests.exe'
Push-Location $repoRoot
try {
    & cl /nologo /W4 /analyze /utf-8 /std:c++20 /EHsc @runtime /DNOMINMAX /IClient /IYOUMU.Assets/Public `
        /IDependencies/PhantomCore/Engine/SDK Tests/Presentation/AssetPathTests.cpp `
        "build/obj/YOUMU.Client/x64/$Configuration/OptionSettings.obj" `
        "build/obj/YOUMU.Client/x64/$Configuration/Pch.obj" "bin/x64/$Configuration/YOUMU.Assets.lib" `
        "bin/x64/$Configuration/YOUMU.Modes.lib" "bin/x64/$Configuration/YOUMU.Chart.lib" "bin/x64/$Configuration/YOUMU.Rhythm.lib" `
        "Dependencies/PhantomCore/bin/x64/$Configuration/PhantomCore.lib" `
        "/Fo$objects/" "/Fe$compiledExe" /link /OPT:NOICF /OPT:NOREF `
        "/LIBPATH:$env:FMOD_ROOT/api/core/lib/x64" user32.lib gdi32.lib ole32.lib shell32.lib
    if ($LASTEXITCODE -ne 0) { throw 'Asset path test compilation failed.' }

    $developmentExe = Join-Path $testRoot "development/bin/x64/$Configuration"
    $developmentAssets = Join-Path $testRoot 'development/Client/Assets'
    $deploymentExe = Join-Path $testRoot 'deployment/game'
    $deploymentAssets = Join-Path $deploymentExe 'Asset'
    foreach ($assets in $developmentAssets, $deploymentAssets) {
        $skin = Join-Path $assets 'Skins/Default Skin'
        New-Item -ItemType Directory -Path $skin -Force | Out-Null
        [IO.File]::WriteAllText((Join-Path $skin 'path-test.dat'), 'isolated skin fixture')
    }
    foreach ($directory in $developmentExe, $deploymentExe) {
        New-Item -ItemType Directory -Path $directory -Force | Out-Null
        Copy-Item -LiteralPath $compiledExe -Destination (Join-Path $directory 'AssetPathTests.exe')
        $fmodDll = if ($Configuration -eq 'Debug') { 'fmodL.dll' } else { 'fmod.dll' }
        Copy-Item -LiteralPath "bin/x64/$Configuration/$fmodDll" -Destination $directory
    }

    # The same binary must switch roots when moved, even when CWD points to the other environment.
    Push-Location $deploymentExe
    try {
        & (Join-Path $developmentExe 'AssetPathTests.exe') $developmentAssets $developmentExe
        if ($LASTEXITCODE -ne 0) { throw 'Development path selection failed.' }
    } finally { Pop-Location }
    Push-Location $developmentExe
    try {
        & (Join-Path $deploymentExe 'AssetPathTests.exe') $deploymentAssets $deploymentExe
        if ($LASTEXITCODE -ne 0) { throw 'Deployment path selection failed.' }
    } finally { Pop-Location }
    Write-Output "Asset path tests passed; retained fixtures: $testRoot"
} finally { Pop-Location }
