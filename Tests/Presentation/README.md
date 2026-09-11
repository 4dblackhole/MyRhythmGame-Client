# Lane key beam regression test

Build the Client solution first. From an x64 Visual Studio developer command
prompt in the repository root, compile the presentation test against the same
configuration's Client object and engine library (set `FMOD_ROOT` to the local SDK):

```bat
cl /nologo /std:c++20 /EHsc /MDd /D_DEBUG /DNOMINMAX /IClient /IFingerDrum.Rhythm /IDependencies\MRG-Engine\Engine\SDK Tests\Presentation\LaneKeyBeamTests.cpp build\obj\MRG.Client\x64\Debug\LaneKeyBeam.obj bin\x64\Debug\FingerDrum.Rhythm.lib Dependencies\MRG-Engine\bin\x64\Debug\MRG.Core.lib /Fobuild\obj\LaneKeyBeamTests.obj /Febin\x64\Debug\LaneKeyBeamTests.exe /link /LIBPATH:"%FMOD_ROOT%\api\core\lib\x64" user32.lib gdi32.lib ole32.lib
bin\x64\Debug\LaneKeyBeamTests.exe
```

For Release, replace `Debug` with `Release`, `/MDd /D_DEBUG` with `/MD /DNDEBUG`.
The test verifies free input, judgement colors/interpolation, input rejection,
roll ticks, missed-note rollover, alpha at 0/100/200ms, retrigger, sprite reuse,
inherited Lane rotation/scale, resize clipping, and reset. It does not initialize
an audio device or renderer; normal gameplay smoke covers resource loading.
