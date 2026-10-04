# Lane key beam regression test

Build the Client solution first. From an x64 Visual Studio developer command
prompt in the repository root, compile the presentation test against the same
configuration's Client object and engine library (set `FMOD_ROOT` to the local SDK):

```bat
cl /nologo /std:c++20 /EHsc /MDd /D_DEBUG /DNOMINMAX /IClient /IFingerDrum.Rhythm /IDependencies\MRG-Engine\Engine\SDK Tests\Presentation\LaneKeyBeamTests.cpp build\obj\MRG.Client\x64\Debug\LaneKeyBeam.obj build\obj\MRG.Client\x64\Debug\Pch.obj bin\x64\Debug\FingerDrum.Rhythm.lib Dependencies\MRG-Engine\bin\x64\Debug\MRG.Core.lib /Fobuild\obj\LaneKeyBeamTests.obj /Febin\x64\Debug\LaneKeyBeamTests.exe /link /OPT:NOICF /OPT:NOREF /LIBPATH:"%FMOD_ROOT%\api\core\lib\x64" user32.lib gdi32.lib ole32.lib
bin\x64\Debug\LaneKeyBeamTests.exe
```

For Release, replace `Debug` with `Release`, `/MDd /D_DEBUG` with `/MD /DNDEBUG`.
Include the Client `Pch.obj`: the production key-beam object uses its PCH.
Release may additionally pass `/LTCG` to the linker to avoid its automatic restart.
The test verifies free input, judgement colors/interpolation, input rejection,
roll ticks, missed-note rollover, alpha at 0/100/200ms, retrigger, sprite reuse,
inherited Lane rotation/scale, resize clipping, and reset. It does not initialize
an audio device or renderer; normal gameplay smoke covers resource loading.

## Editor audio view regression

After building both configurations, run this from PowerShell in an x64 Visual Studio
developer environment. Set `FMOD_ROOT` to the installed SDK and change `$configuration`
to `Release` with `$runtime = @('/MD', '/DNDEBUG')` for the other build.

```powershell
$configuration = 'Debug'
$runtime = @('/MDd', '/D_DEBUG')
$objects = @(Get-ChildItem "build/obj/MRG.Client/x64/$configuration/Editor*.obj").FullName
$objects += @('Pch', 'TextCatalog', 'SkinSetSelection') | ForEach-Object { "build/obj/MRG.Client/x64/$configuration/$_.obj" }
$libraries = @('Rhythm', 'Chart', 'Modes', 'Editor', 'Assets') | ForEach-Object { "bin/x64/$configuration/FingerDrum.$_.lib" }
& cl /nologo /utf-8 /std:c++20 /EHsc @runtime /DNOMINMAX /IClient /IFingerDrum.Rhythm /IFingerDrum.Chart /IFingerDrum.Modes /IFingerDrum.Editor /IFingerDrum.Assets/Public /IDependencies/MRG-Engine/Engine/SDK Tests/Presentation/EditorAudioViewTests.cpp @objects @libraries "Dependencies/MRG-Engine/bin/x64/$configuration/MRG.Core.lib" "/Fobuild/obj/EditorAudioViewTests-$configuration.obj" "/Febin/x64/$configuration/EditorAudioViewTests.exe" /link /OPT:NOICF /OPT:NOREF "/LIBPATH:$env:FMOD_ROOT/api/core/lib/x64" "build/obj/FingerDrum.Assets/x64/$configuration/FingerDrum.Assets.res" user32.lib gdi32.lib ole32.lib mfplat.lib mfreadwrite.lib mfuuid.lib
if ($LASTEXITCODE -ne 0) { throw 'Editor audio view test build failed' }
& "./bin/x64/$configuration/EditorAudioViewTests.exe"
if ($LASTEXITCODE -ne 0) { throw 'Editor audio view tests failed' }
```

This checks the production view's draw packets for real song/hit spectra, negative
note/music time ranges, slider input actions, tab visibility, localization, resize,
and Canvas cleanup. It records rendering commands without initializing a GPU or
audio device; it does not replace visual inspection or mouse testing in the app.
