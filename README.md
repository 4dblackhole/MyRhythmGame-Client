# MyRhythmGame-Client

비공개 리듬게임 FingerDrum의 Client 코드와 게임 자산 저장소입니다.
재사용 엔진은 `Dependencies/MRG-Engine` private submodule입니다.

## 작업 시작

루트 `AGENTS.md` → [짧은 인수인계](Docs/SessionHandoff.md) →
[작업별 문서 표](Docs/README.md)의 해당 행만 읽습니다.
[변경 영향 지도](Docs/ChangeImpactMap.md)에서 소유 클래스와 테스트를 찾고,
관련 선언·구현·직접 호출부만 확인합니다. 전체 문서나 엔진 SDK는 필독이 아닙니다.
요구가 불명확하여 구현이 크게 달라지면 멈추고 질문합니다.

## 코드 구성

| 위치 | 책임 |
| --- | --- |
| `Client/` | 실행 파일, Scene 조립, 화면·오디오 연결; ClientLogic는 선택 상태 로직 |
| `FingerDrum.Rhythm/` | 판정, 노트 규칙, Lane, 스크롤, 타이머 |
| `FingerDrum.Chart/` | YMM/YMP/YME 파싱, 타이밍 컴파일, 편집·저장 |
| `FingerDrum.Modes/` | PlaySession과 Taiko 노트 생성·사운드 정책 |
| `FingerDrum.Editor/` | 에디터용 오디오 디코딩·분석 |
| `FingerDrum.Assets/` | 기본 스킨/글꼴 RCDATA와 외부 기본 제공곡 |
| `Tests/Rhythm/` | 분야별 로직·파일·선택 상태 회귀 테스트 |
| `Dependencies/MRG-Engine/` | 독립 저장소의 재사용 엔진 |

Scene마다 전용 폴더와 `Submodules/`를 둡니다. Scene은 작은 호출로 책임 클래스를
조립하며, UI·세션·분석 상태는 각 클래스가 관리합니다. Note도 계약·규칙별 트리를
사용하며 Visual Studio 필터를 물리 폴더와 일치시킵니다.

## 빌드·실행

Visual Studio 2022 / MSVC v143 / Windows SDK / FMOD Studio API for Windows가
필요합니다. FMOD SDK·DLL은 커밋하지 않습니다.

```powershell
git clone --recurse-submodules https://github.com/4dblackhole/MyRhythmGame-Client.git
cd MyRhythmGame-Client
msbuild MyRhythmGame-Client.sln /m /p:Configuration=Debug /p:Platform=x64
```

일반 실행은 로고 → 플레이/에디터 곡 선택으로 시작합니다.
빌드·테스트·smoke 명령은 [검증](Docs/Verification.md), Scene 수명과 공통 관리자는
[실행 흐름](Docs/ExecutionFlow.md), 외부 파일 우선 및 내장 fallback/배포곡 정책은
[자산](Docs/BuiltInAssets.md)에서 관리합니다.
