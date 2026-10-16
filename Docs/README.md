# 작업별 문서 진입점

처음에는 루트 `AGENTS.md`와 [SessionHandoff](SessionHandoff.md)만 읽고,
아래에서 요청에 해당하는 **한 행**을 고릅니다. 기능 추가도 같은 경로를 사용합니다.
선택한 문서의 해당 절 → public 선언 → 구현 → 직접 호출부 → 관련 테스트 순서로
읽습니다. 소유권/호출 흐름이 바뀔 때만 인접 계층으로 범위를 넓힙니다.

| 수정/추가할 기능 | 먼저 읽을 문서 | 코드 진입점 |
| --- | --- | --- |
| 노트 판정·정확도·타이머 | [판정 규칙](Gameplay/Rules.md) | `FingerDrum.Rhythm/Note`, `Judgement`, `Time` |
| Taiko 종류·사운드 정책 | [판정 규칙](Gameplay/Rules.md), [오디오](Gameplay/Audio.md) | `FingerDrum.Modes/Taiko` |
| YMM/YMP/YME 문법·히트사운드 볼륨·보간·Whole/Separate·영역 상태 | [ChartFormats](ChartFormats.md) | `FingerDrum.Chart/Parsing`, `Model`, `Automation` |
| osu!taiko 외부 EXE 변환·격자 보정·omit 마디 | [OsuTaikoConverter](OsuTaikoConverter.md) | 형제 저장소 `../OsuTaikoConverter`, Client 프로젝트 참조 |
| BPM·마디·유리수 위치·음수 delay 무효 구간 | [타이밍 문법](Formats/Timing.md) | `FingerDrum.Chart/Timing`, `Utility` |
| 편집 트리·노트 선택/속성·모드 확장·메뉴/저장·오디오 탭·재생/시간축 줌 | [ChartEditor](ChartEditor.md) | ChartEditor의 원본 트리/조회 snapshot; EditorPlayback / EditorMenuBar / EditorWorkspace / Modes/Taiko; `FingerDrum.Editor` |
| 곡 목록·포커스·UI·미리듣기 | [SongSelect](SongSelect.md) | `Client/GameScene/MusicSelectScene`, `FingerDrum.Chart/Catalog` |
| 에디터용 곡 선택 | [EditorSongSelect](EditorSongSelect.md) | 같은 MusicSelectScene의 Editor 구성 |
| 플레이 화면·노트/마디선 스크롤·키빔·판정 인디케이터/문구·상단 진행바 | [플레이 표시](Gameplay/Presentation.md) | `Client/GameScene/RhythmTestScene`, `Client/Presentation` |
| ESC 일시정지·Resume/Rewind/Retry/Retire·향후 HP/리플레이 계약 | [플레이 일시정지](Gameplay/Pause.md) | GameplayPauseMenu / GameplaySessionController; PlaySessionSeeking / PlayControlEvent |
| Penpot 결과 화면·상단 곡 정보·리트라이·판정 통계·오차 그래프·us 입력 로그 | [플레이 결과](Gameplay/Results.md) | `FingerDrum.Modes/Mode/Results`, `Taiko/Results`, `Client/GameScene/ResultScene`; 내장 Result PNG |
| 플레이 디버그 조작·전체 노트/Buzz 재현 | [디버깅](Gameplay/Debugging.md) | `RhythmTestScene/Submodules/GameplayInput.cpp`, `Tests/Rhythm/Submodules/BuzzReplayTests.cpp` |
| 로고·타이틀 | [FingerDrum](FingerDrum.md) | `Client/GameScene/FingerDrumLogoScene` |
| 게임 표시 이름·문구·언어·글꼴·공통 UI 색상·옵션/ESC·해상도·키·글로벌 오프셋·Option.ini | [TextManagement](TextManagement.md) | `Client/Texts/GameIdentity.h`, 화면별 Texts; `Client/Presentation/UiColors.h` / `OptionsPanel.*`, `Client/App/OptionSettings.*` |
| Canvas·입력·클리핑 API 사용 | [Visual2DGuide](Visual2DGuide.md) | 해당 Scene의 Submodules |
| 개발/배포 자산 경로·난이도별 YMP/YME·스킨/fallback | [BuiltInAssets](BuiltInAssets.md) | `Client/App/SkinSetSelection.*`, `AssetPaths.h`, `FingerDrum.Assets` |
| 시작·Scene 전환·종료·소유권 | [ExecutionFlow](ExecutionFlow.md) | `Client/App`, `Client/GameFlow` |
| 엔진 API 자체 수정 | 엔진 AGENTS, EngineOverview의 해당 기능 | `Dependencies/MRG-Engine` |
| Penpot 디자인 찾기 | [DesignReferences](DesignReferences.md) | 해당 화면 문서 |
| Claude 리뷰 반영·정적 분석·테스트 실행기/CI | [리뷰 반영 기록](Reviews/ClaudeReview-2026-10-11.md), [검증](Verification.md) | `Tests/Rhythm`, `Tests/Presentation`, `.github/workflows/domain-tests.yml` |
| 코드 품질 재검토·효과 오류 복구/조회/검증 일치 | [품질 수정 기록](Reviews/QualityFixes-2026-10-12.md), [검증](Verification.md) | EffectValidation / PlaySession / ChartEditor; 엔진 Run 숨김 오류 |
| 레거시 제거·예제 폐기 범위 | [레거시 제거 기록](Reviews/LegacyAudit-2026-10-11.md) | Main/OptionSettings, EditorModeFactory, PatternTiming; 직접 계약 헤더 |
| 프레임 비용·카탈로그/자원 로딩·표시 캐시 | [성능 수정 기록](Reviews/PerformanceFixes-2026-10-14.md) | GameplayPresenter, TaikoSessionBuilder, ChartEditor, SongCatalogLoader; 엔진 Texture/Text/Audio |
| 텍스트 캐시·글리프 업로드·긴 제목·아틀라스 성능 조사 | [텍스트 심층 조사](Reviews/TextRenderingAudit-2026-10-14.md) | 엔진 TextRendering / Visual2DRendering; Client MarqueeTextComponent / SongSelectCards |
| 텍스트 유지 배치·회색조·아틀라스 회수 구현/검증 | [텍스트 개선 기록](Reviews/TextRenderingFixes-2026-10-14.md) | 엔진 TextRendering / Visual2D TextContent; Client MarqueeTextComponent; 엔진 SDK 텍스트 회귀 |

[변경 영향 지도](ChangeImpactMap.md)는 소유 파일과 테스트를,
[검증](Verification.md)은 빌드·실행 명령을 제공합니다.
엔진 검증은 엔진 저장소의 SDK 단위 테스트를 사용합니다.

전체 문서/엔진 SDK/과거 채팅은 필독이 아닙니다. 계약과 이유는 기능 문서 한 곳에만
기록하고 색인에서는 링크를 유지합니다. API 목록과 구현을 문서에 복제하지 않습니다.
요구가 불명확하여 구현이 크게 달라지면 루트 AGENTS에 따라 멈추고 질문합니다.
