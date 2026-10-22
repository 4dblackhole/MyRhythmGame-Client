param([ValidateSet('Debug', 'Release')][string]$configuration = 'Debug', [string]$SongFilter = '')
$ErrorActionPreference = 'Stop'
$runtime = if ($configuration -eq 'Debug') { @('/MDd', '/D_DEBUG') } else { @('/MD', '/DNDEBUG') }
[xml]$clientProject = Get-Content Client/YOUMU.Client.vcxproj
$objects = @($clientProject.Project.ItemGroup.ClCompile | Where-Object { $_.Include -match '^(EditorScene\\|Texts\\EditorScene\\)' } | ForEach-Object { "build/obj/YOUMU.Client/x64/$configuration/$([IO.Path]::GetFileNameWithoutExtension($_.Include)).obj" })
$testObjectDirectory = "build/obj/EditorPresentationTests/$configuration"
New-Item -ItemType Directory -Path $testObjectDirectory -Force | Out-Null
$objects += @('Pch', 'TextCatalog', 'LanguageFile', 'SkinSetSelection', 'SkinManifest', 'SkinVisualComponent', 'SongPreviewController') | ForEach-Object { "build/obj/YOUMU.Client/x64/$configuration/$_.obj" }
$libraries = @('Rhythm', 'Chart', 'Modes', 'Editor', 'Assets') | ForEach-Object { "bin/x64/$configuration/YOUMU.$_.lib" }
$taskFmodRoot = if ($env:FMOD_ROOT) { $env:FMOD_ROOT } else { 'C:/Program Files (x86)/FMOD SoundSystem/FMOD Studio API Windows' }
& cl /nologo /analyze /FC /utf-8 /std:c++20 /EHsc @runtime /DNOMINMAX /IClient /IYOUMU.Rhythm /IYOUMU.Chart /IYOUMU.Modes /IYOUMU.Editor /IYOUMU.Assets/Public /IDependencies/PhantomCore/Engine/SDK Tests/Presentation/EditorAudioViewTests.cpp Tests/Presentation/EditorAudioAnalysisIntegrationTests.cpp Tests/Presentation/EditorModeTests.cpp Tests/Presentation/EditorMenuTests.cpp Tests/Presentation/EditorPlaybackTests.cpp Tests/Presentation/EditorMetronomeTests.cpp @objects @libraries "Dependencies/PhantomCore/bin/x64/$configuration/PhantomCore.lib" "/Fo$testObjectDirectory/" "/Febin/x64/$configuration/EditorAudioViewTests.exe" /link /OPT:NOICF /OPT:NOREF "/LIBPATH:$taskFmodRoot/api/core/lib/x64" "build/obj/YOUMU.Assets/x64/$configuration/YOUMU.Assets.res" user32.lib gdi32.lib ole32.lib mfplat.lib mfreadwrite.lib mfuuid.lib
if ($LASTEXITCODE -ne 0) { throw 'Editor audio view test build failed' }
if ($SongFilter) { & "./bin/x64/$configuration/EditorAudioViewTests.exe" $SongFilter }
else { & "./bin/x64/$configuration/EditorAudioViewTests.exe" }
if ($LASTEXITCODE -ne 0) { throw 'Editor audio view tests failed' }
