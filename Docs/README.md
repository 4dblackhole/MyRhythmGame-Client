# 작업별 문서 진입점

처음에는 루트 `AGENTS.md`와 [SessionHandoff](SessionHandoff.md)만 읽고,
아래에서 요청에 해당하는 **한 행**을 고릅니다. 기능 추가도 같은 경로를 사용합니다.
선택한 문서의 해당 절 → public 선언 → 구현 → 직접 호출부 → 관련 테스트 순서로
읽습니다. 소유권/호출 흐름이 바뀔 때만 인접 계층으로 범위를 넓힙니다.

| 수정/추가할 기능 | 먼저 읽을 문서 | 코드 진입점 |
| --- | --- | --- |
| 노트 판정·정확도·타이머 | [판정 규칙](Gameplay/Rules.md) | `FingerDrum.Rhythm/Note`, `Judgement`, `Time` |
| Taiko 종류·사운드 정책 | [판정 규칙](Gameplay/Rules.md), [오디오](Gameplay/Audio.md) | `FingerDrum.Modes/Taiko` |
| YMM/YMP/YME 문법 | [ChartFormats](ChartFormats.md) | `FingerDrum.Chart/Parsing`, `Model` |
| BPM·마디·유리수 위치 | [타이밍 문법](Formats/Timing.md) | `FingerDrum.Chart/Timing`, `Utility` |
| 편집·저장·오디오 분석 | [ChartEditor](ChartEditor.md) | `Client/EditorScene`, `FingerDrum.Chart/Editing`, `FingerDrum.Editor` |
| 곡 목록·포커스·UI·미리듣기 | [SongSelect](SongSelect.md) | `Client/GameScene/MusicSelectScene`, `FingerDrum.Chart/Catalog` |
| 에디터용 곡 선택 | [EditorSongSelect](EditorSongSelect.md) | 같은 MusicSelectScene의 Editor 구성 |
| 플레이 화면·노트 이미지·키빔 | [플레이 표시](Gameplay/Presentation.md) | `Client/GameScene/RhythmTestScene`, `Client/Presentation` |
| 플레이 디버그 조작 | [디버깅](Gameplay/Debugging.md) | `RhythmTestScene/Submodules/GameplayInput.cpp` |
| 로고·타이틀 | [FingerDrum](FingerDrum.md) | `Client/GameScene/FingerDrumLogoScene` |
| Canvas·입력·클리핑 API 사용 | [Visual2DGuide](Visual2DGuide.md) | 해당 Scene의 Submodules |
| 자산 경로·fallback·배포곡 | [BuiltInAssets](BuiltInAssets.md) | `Client/App/AssetPaths.h`, `FingerDrum.Assets` |
| 시작·Scene 전환·종료·소유권 | [ExecutionFlow](ExecutionFlow.md) | `Client/App`, `Client/GameFlow` |
| 엔진 API 자체 수정 | 엔진 AGENTS, EngineOverview의 해당 기능 | `Dependencies/MRG-Engine` |
| Penpot 디자인 찾기 | [DesignReferences](DesignReferences.md) | 해당 화면 문서 |

[변경 영향 지도](ChangeImpactMap.md)는 소유 파일과 테스트를,
[검증](Verification.md)은 빌드·실행 명령을 제공합니다.
예제 작업일 때만 [ColoredCube](Examples/ColoredCube/README.md)를 읽습니다.

전체 문서/엔진 SDK/과거 채팅은 필독이 아닙니다. 계약과 이유는 기능 문서 한 곳에만
기록하고 색인에서는 링크를 유지합니다. API 목록과 구현을 문서에 복제하지 않습니다.
요구가 불명확하여 구현이 크게 달라지면 루트 AGENTS에 따라 멈추고 질문합니다.
