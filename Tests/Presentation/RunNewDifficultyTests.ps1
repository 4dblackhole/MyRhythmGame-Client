param([ValidateSet('Debug','Release')][string]$Configuration = 'Debug')
$ErrorActionPreference = 'Stop'
$runtime = if ($Configuration -eq 'Debug') { @('/MDd','/D_DEBUG') } else { @('/MD','/DNDEBUG') }
$objects = @('Pch','EditorPatternCreation','NewDifficultyDialog','TextCatalog','LanguageFile') | ForEach-Object { "build/obj/YOUMU.Client/x64/$Configuration/$_.obj" }
$libraries = @('Chart','Rhythm','Assets') | ForEach-Object { "bin/x64/$Configuration/YOUMU.$_.lib" }
$testObjects = "build/obj/NewDifficultyTests/$Configuration"
New-Item -ItemType Directory -Force $testObjects | Out-Null
& cl /nologo /analyze /W4 /utf-8 /std:c++20 /EHsc @runtime /DNOMINMAX /IClient /IYOUMU.Chart /IYOUMU.Rhythm /IYOUMU.Assets/Public /IDependencies/PhantomCore/Engine/SDK Tests/Presentation/NewDifficultyTests.cpp @objects @libraries "Dependencies/PhantomCore/bin/x64/$Configuration/PhantomCore.lib" "/Fo$testObjects/" "/Febin/x64/$Configuration/NewDifficultyTests.exe" /link /OPT:NOICF /OPT:NOREF "/LIBPATH:$env:FMOD_ROOT/api/core/lib/x64" "build/obj/YOUMU.Assets/x64/$Configuration/YOUMU.Assets.res" user32.lib gdi32.lib ole32.lib
if ($LASTEXITCODE -ne 0) { throw 'New difficulty tests failed to compile.' }
& "./bin/x64/$Configuration/NewDifficultyTests.exe"
if ($LASTEXITCODE -ne 0) { throw 'New difficulty tests failed.' }
