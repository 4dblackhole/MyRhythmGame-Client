param([ValidateSet('Debug','Release')][string]$Configuration = 'Debug')
$ErrorActionPreference = 'Stop'
$runtime = if ($Configuration -eq 'Debug') { @('/MDd','/D_DEBUG') } else { @('/MD','/DNDEBUG') }
$objects = @('Pch','EditorPatternCreation','NewDifficultyDialog','TextCatalog','LanguageFile') | ForEach-Object { "build/obj/MRG.Client/x64/$Configuration/$_.obj" }
$libraries = @('Chart','Rhythm','Assets') | ForEach-Object { "bin/x64/$Configuration/FingerDrum.$_.lib" }
$testObjects = "build/obj/NewDifficultyTests/$Configuration"
New-Item -ItemType Directory -Force $testObjects | Out-Null
& cl /nologo /analyze /W4 /utf-8 /std:c++20 /EHsc @runtime /DNOMINMAX /IClient /IFingerDrum.Chart /IFingerDrum.Rhythm /IFingerDrum.Assets/Public /IDependencies/MRG-Engine/Engine/SDK Tests/Presentation/NewDifficultyTests.cpp @objects @libraries "Dependencies/MRG-Engine/bin/x64/$Configuration/MRG.Core.lib" "/Fo$testObjects/" "/Febin/x64/$Configuration/NewDifficultyTests.exe" /link /OPT:NOICF /OPT:NOREF "/LIBPATH:$env:FMOD_ROOT/api/core/lib/x64" "build/obj/FingerDrum.Assets/x64/$Configuration/FingerDrum.Assets.res" user32.lib gdi32.lib ole32.lib
if ($LASTEXITCODE -ne 0) { throw 'New difficulty tests failed to compile.' }
& "./bin/x64/$Configuration/NewDifficultyTests.exe"
if ($LASTEXITCODE -ne 0) { throw 'New difficulty tests failed.' }
