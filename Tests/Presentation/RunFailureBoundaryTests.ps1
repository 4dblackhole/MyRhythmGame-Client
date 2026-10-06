param([ValidateSet('Debug', 'Release')][string]$Configuration = 'Debug')
$ErrorActionPreference = 'Stop'
$runtime = if ($Configuration -eq 'Debug') { @('/MDd', '/D_DEBUG') } else { @('/MD', '/DNDEBUG') }
$objectDirectory = "build/obj/FailureBoundaryTests/$Configuration"
New-Item -ItemType Directory -Path $objectDirectory -Force | Out-Null
# Deliberately omit the RCDATA resource: initialization must fail before any cache access.
& cl /nologo /utf-8 /std:c++20 /EHsc /W4 /analyze @runtime /DNOMINMAX /IFingerDrum.Chart /IFingerDrum.Rhythm /IFingerDrum.Assets/Public Tests/Presentation/FailureBoundaryTests.cpp "bin/x64/$Configuration/FingerDrum.Assets.lib" "/Fo$objectDirectory/" "/Febin/x64/$Configuration/FailureBoundaryTests.exe" /link /OPT:NOREF /OPT:NOICF user32.lib ole32.lib shell32.lib
if ($LASTEXITCODE -ne 0) { throw 'Failure boundary test build failed.' }
& "./bin/x64/$Configuration/FailureBoundaryTests.exe"
if ($LASTEXITCODE -ne 0) { throw 'Failure boundary test failed.' }
