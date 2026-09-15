# MyRhythmGame-Client

비공개 상용 리듬게임 FingerDrum의 Client 코드와 게임 asset 저장소입니다.
재사용 가능한 엔진은 `Dependencies/MRG-Engine` private submodule로 연결합니다.

처음 참여하는 개발자나 새 Codex 세션은 [문서 안내](Docs/README.md),
[엔진 기능 색인](Dependencies/MRG-Engine/Docs/EngineOverview.md),
[실행 흐름](Docs/ExecutionFlow.md)을 순서대로 읽습니다.

플레이 화면은 노트별 정확도의 누적 평균을 표시하고, Debug 빌드에서는 포커스
노트 상태와 마지막 노트의 개별 타격/틱 정확도를 함께 표시합니다.
계산 규칙과 보라 노트(ID 5)는 [리듬게임 문서](Docs/RhythmGameplay.md),
연타 목표 횟수의 공통 문법은 [차트 문법](Docs/ChartFormats.md)을 참고하세요.

타이틀의 `Editor`는 기록 패널 없이 곡·난이도 정보를 넓게 표시하는
[에디터 곡 선택 화면](Docs/EditorSongSelect.md)으로 이동합니다. 실제 차트 편집
workspace와 저장 기능은 후속 단계입니다.

## 구조

```text
Client/                    실행 파일, Scene, 게임별 오디오와 사용자 asset 위치
FingerDrum.Assets/         EXE 내장 기본 asset 팩, RCDATA 생성과 fallback 경로
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

.\bin\x64\Debug\FingerDrum.Rhythm.Tests.exe --catalog-root .\FingerDrum.Assets\Assets\Songs
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

기본 스킨, 글꼴과 AngelDream의 MP3/YMM/YMP 3개는 `FingerDrum.Assets`가 한
RCDATA 팩으로 만들어 `MyRhythmGame.exe` 안에 링크합니다. 실행 시 버전별 로컬
캐시에 풀며, 외부 스킨 파일은 파일별로 우선하고 누락된 항목만 내장 기본 스킨을
사용합니다. 자세한 흐름은 [내장 자산](Docs/BuiltInAssets.md)을 참고하세요.

## 현재 실행 흐름

공통 오디오 재생은 `SceneGameClient::AudioPlayback()`이 관리하며 gameplay는
자신의 재생 ID만 일시정지·정지합니다. 파일 등록과 리듬 시각 변환은 Client가 담당합니다.
화면 Sprite Canvas와 이미지 등록은 `SceneGameClient::ScreenVisuals()`가 소유합니다.
Logo, Lobby, Gameplay Scene은 표시 상태와 node만 바꾸며 직접 화면 제출을 하지 않습니다.

기본 실행은 AliceBlue 배경의 FingerDrum 로고로 시작합니다. Game Start는
Penpot 기반 곡 선택 화면으로 이동합니다. 곡 선택 화면은 기존 catalog 자산과
Git에서 제외한 로컬 곡·패턴을 표시하며, 기록 저장소가 없으므로 점수를 꾸며내지 않고 명시적인
빈 기록 상태를 표시합니다. 좌우키로 곡을 고르고 포커스된 곡 안에서 상하키로
펼쳐진 난이도를 선택합니다. 자세한 규칙은
[리듬게임 구조와 플레이 화면](Docs/RhythmGameplay.md),
[차트 문법](Docs/ChartFormats.md), 화면 구성은
[곡 선택 화면](Docs/SongSelect.md)을 참고합니다. 플레이 화면은 마디선·롱노트
tick·남은 횟수 이미지와 판정색/오입력 적색 key beam을 표시합니다.
