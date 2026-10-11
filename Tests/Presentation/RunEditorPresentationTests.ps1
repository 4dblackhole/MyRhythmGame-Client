param([ValidateSet('Debug', 'Release')][string]$configuration = 'Debug')
$ErrorActionPreference = 'Stop'
$runtime = if ($configuration -eq 'Debug') { @('/MDd', '/D_DEBUG') } else { @('/MD', '/DNDEBUG') }
[xml]$clientProject = Get-Content Client/MRG.Client.vcxproj
$objects = @($clientProject.Project.ItemGroup.ClCompile | Where-Object { $_.Include -match '^(EditorScene\\|Texts\\EditorScene\\)' } | ForEach-Object { "build/obj/MRG.Client/x64/$configuration/$([IO.Path]::GetFileNameWithoutExtension($_.Include)).obj" })
$testObjectDirectory = "build/obj/EditorPresentationTests/$configuration"
New-Item -ItemType Directory -Path $testObjectDirectory -Force | Out-Null
$objects += @('Pch', 'TextCatalog', 'SkinSetSelection', 'SongPreviewController') | ForEach-Object { "build/obj/MRG.Client/x64/$configuration/$_.obj" }
$libraries = @('Rhythm', 'Chart', 'Modes', 'Editor', 'Assets') | ForEach-Object { "bin/x64/$configuration/FingerDrum.$_.lib" }
$taskFmodRoot = if ($env:FMOD_ROOT) { $env:FMOD_ROOT } else { 'C:/Program Files (x86)/FMOD SoundSystem/FMOD Studio API Windows' }
& cl /nologo /analyze /utf-8 /std:c++20 /EHsc @runtime /DNOMINMAX /IClient /IFingerDrum.Rhythm /IFingerDrum.Chart /IFingerDrum.Modes /IFingerDrum.Editor /IFingerDrum.Assets/Public /IDependencies/MRG-Engine/Engine/SDK Tests/Presentation/EditorAudioViewTests.cpp Tests/Presentation/EditorModeTests.cpp Tests/Presentation/EditorMenuTests.cpp @objects @libraries "Dependencies/MRG-Engine/bin/x64/$configuration/MRG.Core.lib" "/Fo$testObjectDirectory/" "/Febin/x64/$configuration/EditorAudioViewTests.exe" /link /OPT:NOICF /OPT:NOREF "/LIBPATH:$taskFmodRoot/api/core/lib/x64" "build/obj/FingerDrum.Assets/x64/$configuration/FingerDrum.Assets.res" user32.lib gdi32.lib ole32.lib mfplat.lib mfreadwrite.lib mfuuid.lib
if ($LASTEXITCODE -ne 0) { throw 'Editor audio view test build failed' }
& "./bin/x64/$configuration/EditorAudioViewTests.exe"
if ($LASTEXITCODE -ne 0) { throw 'Editor audio view tests failed' }
