# MyRhythmGame-Client

비공개 상용 리듬게임 FingerDrum의 Client 코드와 게임 asset 저장소입니다.
재사용 가능한 엔진은 `Dependencies/MRG-Engine` private submodule로 연결합니다.

처음 참여하는 개발자나 새 Codex 세션은 [문서 안내](Docs/README.md),
[엔진 기능 색인](Dependencies/MRG-Engine/Docs/EngineOverview.md),
[실행 흐름](Docs/ExecutionFlow.md)을 순서대로 읽습니다.

## 구조

```text
Client/                    실행 파일, Scene, 게임별 오디오와 asset
FingerDrum.Rhythm/         판정, 노트, Lane, 스크롤, 단일 타이머
FingerDrum.Chart/          YMM/YMP/YME 파서와 음악 시간 컴파일
FingerDrum.Modes/          PlayGameMode 계약과 모드 구현
Tests/Rhythm/              외부 장치가 필요 없는 로직 테스트
Dependencies/MRG-Engine/   private 엔진 submodule
Docs/                      사용법과 설계 문서
```

## 받기와 빌드

```powershell
git clone --recurse-submodules https://github.com/4dblackhole/MyRhythmGame-Client.git
cd MyRhythmGame-Client

& 'C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe' `
  MyRhythmGame-Client.sln /m /p:Configuration=Debug /p:Platform=x64

.\bin\x64\Debug\FingerDrum.Rhythm.Tests.exe --catalog-root .\Client\Assets\Songs
.\bin\x64\Debug\MyRhythmGame.exe --smoke-test
.\bin\x64\Debug\MyRhythmGame.exe --smoke-lobby
.\bin\x64\Debug\MyRhythmGame.exe --smoke-gameplay
.\bin\x64\Debug\MyRhythmGame.exe --smoke-test --example=mesh
```

재사용 엔진 기능은 `Client/Examples/ColoredCube`의 `ColoredCubeGame` route로
검증한다. `--example=mesh`, `--example=collision`, `--example=widgets`를 지원한다.
Visual2D Canvas 논리 좌표는 화면 정중앙이 원점이고 `+Y`가 위쪽이다.

Visual Studio 2022, MSVC v143, Windows SDK와 FMOD Studio API for Windows가
필요합니다. FMOD SDK와 DLL은 저장소에 커밋하지 않습니다.

## 현재 실행 흐름

기본 실행은 AliceBlue 배경의 FingerDrum 로고로 시작합니다. Game Start는
Penpot 기반 곡 선택 화면으로 이동합니다. 곡 선택 화면은 기존 catalog 자산과
Git에서 제외한 로컬 곡·패턴을 표시하며, 기록 저장소가 없으므로 점수를 꾸며내지 않고 명시적인
빈 기록 상태를 표시합니다. 자세한 규칙은
[리듬게임 구조와 플레이 화면](Docs/RhythmGameplay.md),
[차트 문법](Docs/ChartFormats.md), 화면 구성은
[곡 선택 화면](Docs/SongSelect.md)을 참고합니다. 플레이 화면은 마디선·롱노트
tick·남은 횟수 이미지와 판정색/오입력 적색 key beam을 표시합니다.
