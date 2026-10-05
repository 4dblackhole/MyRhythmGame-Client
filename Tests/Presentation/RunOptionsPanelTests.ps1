param([ValidateSet('Debug', 'Release')][string]$Configuration = 'Debug')
$ErrorActionPreference = 'Stop'
$runtime = if ($Configuration -eq 'Debug') { @('/MDd', '/D_DEBUG') } else { @('/MD', '/DNDEBUG') }
$testObjects = "build/obj/OptionsPanelTests/$Configuration"
New-Item -ItemType Directory -Path $testObjects -Force | Out-Null
$objects = @('Pch', 'OptionSettings', 'OptionsPanel', 'OptionTexts', 'TextCatalog', 'SkinSetSelection') |
    ForEach-Object { "build/obj/MRG.Client/x64/$Configuration/$_.obj" }
& cl /nologo /utf-8 /std:c++20 /EHsc @runtime /DNOMINMAX /IClient /IFingerDrum.Assets/Public `
    /IDependencies/MRG-Engine/Engine/SDK Tests/Presentation/OptionsPanelTests.cpp @objects `
    "bin/x64/$Configuration/FingerDrum.Assets.lib" "Dependencies/MRG-Engine/bin/x64/$Configuration/MRG.Core.lib" `
    "/Fo$testObjects/" "/Febin/x64/$Configuration/OptionsPanelTests.exe" /link /OPT:NOICF /OPT:NOREF `
    "/LIBPATH:$env:FMOD_ROOT/api/core/lib/x64" user32.lib gdi32.lib ole32.lib
if ($LASTEXITCODE -ne 0) { throw 'Options test compilation failed.' }
& "./bin/x64/$Configuration/OptionsPanelTests.exe"
if ($LASTEXITCODE -ne 0) { throw 'Options tests failed.' }
