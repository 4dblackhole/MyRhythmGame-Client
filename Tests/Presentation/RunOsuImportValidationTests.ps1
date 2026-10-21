param([ValidateSet('Debug','Release')][string]$Configuration = 'Debug')
$ErrorActionPreference = 'Stop'
$runtime = if ($Configuration -eq 'Debug') { @('/MDd','/D_DEBUG') } else { @('/MD','/DNDEBUG') }
$objects = @('Pch','OsuImportValidation') | ForEach-Object { "build/obj/YOUMU.Client/x64/$Configuration/$_.obj" }
$libraries = @('Modes','Chart','Rhythm') | ForEach-Object { "bin/x64/$Configuration/YOUMU.$_.lib" }
$testObjects = "build/obj/OsuImportValidationTests/$Configuration"
New-Item -ItemType Directory -Force $testObjects | Out-Null
& cl /nologo /analyze /W4 /utf-8 /std:c++20 /EHsc @runtime /DNOMINMAX /IClient /IYOUMU.Chart /IYOUMU.Modes /IYOUMU.Rhythm /IDependencies/PhantomCore/Engine/SDK Tests/Presentation/OsuImportValidationTests.cpp @objects @libraries "Dependencies/PhantomCore/bin/x64/$Configuration/PhantomCore.lib" "/Fo$testObjects/" "/Febin/x64/$Configuration/OsuImportValidationTests.exe" /link /OPT:NOICF /OPT:NOREF "/LIBPATH:$env:FMOD_ROOT/api/core/lib/x64" user32.lib
if ($LASTEXITCODE -ne 0) { throw 'Osu import validation tests failed to compile.' }
& "./bin/x64/$Configuration/OsuImportValidationTests.exe"
if ($LASTEXITCODE -ne 0) { throw 'Osu import validation tests failed.' }
