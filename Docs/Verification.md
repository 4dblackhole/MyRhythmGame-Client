# 빌드와 검증

코드 변경 후 저장소 루트에서 실행합니다. MSBuild는 설치된 Visual Studio의
vswhere로 찾고 FMOD SDK 경로는 Directory.Build.props의 설정을 사용합니다.

```powershell
git submodule update --init --recursive
msbuild MyRhythmGame-Client.sln /m /t:Rebuild /p:Configuration=Debug /p:Platform=x64
msbuild MyRhythmGame-Client.sln /m /t:Rebuild /p:Configuration=Release /p:Platform=x64
```

각 구성(Debug/Release)에서 아래 명령을 실행합니다.

```powershell
.\bin\x64\Debug\FingerDrum.Rhythm.Tests.exe --catalog-root FingerDrum.Assets/Assets/Songs
.\bin\x64\Debug\MyRhythmGame.exe --smoke-test
.\bin\x64\Debug\MyRhythmGame.exe --smoke-lobby
.\bin\x64\Debug\MyRhythmGame.exe --smoke-gameplay
.\bin\x64\Debug\MyRhythmGame.exe --smoke-editor
```

재사용 엔진 변경은 Debug 빌드의 ColoredCubeGame 관련
`--example=mesh|collision|widgets` route와 엔진 자체 테스트도 실행합니다.
예: `--smoke-test --example=mesh`. Release에는 이 예제 route가 없습니다.
프로젝트/필터/include 경계는 `Scripts/CheckArchitecture.ps1`로 검사합니다.
키빔 표시는 [별도 회귀 테스트](../Tests/Presentation/README.md)로 검사할 수 있습니다.
UI smoke는 초기화·실행·정리 검증이며 실제 모양/사용자 조작 검증과 구분합니다.

커밋 전 `git diff --check`, 사용자 파일 제외 여부, Client/엔진 status와 upstream을
확인합니다. main 직접 작업은 검증 후 commit/push하며 브랜치 요청 시에만 PR을 사용합니다.
FMOD SDK·DLL·import lib는 커밋하지 않습니다.
