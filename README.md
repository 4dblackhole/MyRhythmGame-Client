# MyRhythmGame-Client

비공개 상용 리듬게임의 Client 코드와 게임 asset을 보관하는 저장소다. 엔진은
별도 private 저장소인 `MRG-Engine`을 Git submodule로 연결한다.

## 처음 받기

```powershell
git clone --recurse-submodules https://github.com/4dblackhole/MyRhythmGame-Client.git
cd MyRhythmGame-Client
```

일반 clone을 이미 실행했다면 다음 명령으로 엔진을 받는다.

```powershell
git submodule update --init --recursive
```

private submodule 접근을 위해 `4dblackhole/MRG-Engine` 읽기 권한이 있는 GitHub
계정 인증이 필요하다.

## 저장소 구성

```text
Client/                    게임 코드와 asset
Dependencies/MRG-Engine/   private 엔진 submodule
Docs/                      게임 실행 흐름 문서
MyRhythmGame-Client.sln
```

Client는 엔진에서 `MRG_Core.h` 하나만 포함하고 `MRG.Core` 프로젝트 하나만
참조한다. submodule 포인터를 변경하지 않는 한 엔진 버전은 자동으로 바뀌지 않는다.

## 요구 사항

- Visual Studio 2022, MSVC v143, Windows 10/11 SDK
- FMOD Studio API for Windows
- `FMOD_ROOT` 환경 변수 또는 `Directory.Build.props`가 탐지하는 기본 FMOD 경로

FMOD SDK 및 DLL은 저장소에 포함하지 않는다.

## 빌드와 실행

```powershell
& 'C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe' `
  MyRhythmGame-Client.sln /m /t:Rebuild `
  /p:Configuration=Debug /p:Platform=x64

.\bin\x64\Debug\MyRhythmGame.exe --smoke-test
```

## 엔진 버전 갱신

```powershell
git -C Dependencies/MRG-Engine fetch origin
git -C Dependencies/MRG-Engine checkout main
git -C Dependencies/MRG-Engine pull --ff-only
git add Dependencies/MRG-Engine
git commit -m "Update MRG-Engine"
```

엔진 자체 수정은 `MRG-Engine` 저장소에서 먼저 커밋·검증·push한 뒤 Client의
submodule 포인터를 갱신한다.
