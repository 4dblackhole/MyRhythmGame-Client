# 작업별 문서 진입점

처음에는 루트 `AGENTS.md`와 [SessionHandoff](SessionHandoff.md)만 읽고,
아래에서 요청에 해당하는 **한 행**을 고릅니다. 기능 추가도 같은 경로를 사용합니다.
선택한 문서의 해당 절 → public 선언 → 구현 → 직접 호출부 → 관련 테스트 순서로
읽습니다. 소유권/호출 흐름이 바뀔 때만 인접 계층으로 범위를 넓힙니다.

| 수정/추가할 기능 | 먼저 읽을 문서 | 코드 진입점 |
| --- | --- | --- |
| 제품/프로젝트 이름·EXE·namespace·변환 결과 검증 | [제품 이름과 검증 책임](Reviews/IdentityMigration-2026-10-22.md), [검증](Verification.md) | GameIdentity.props / GenerateGameIdentity; YOUMU.sln; EditorOsuImport / OsuImportValidation |
| 노트 판정·정확도·타이머 | [판정 규칙](Gameplay/Rules.md) | `YOUMU.Rhythm/Note`, `Judgement`, `Time` |
| 공통 PCM·MP3 패딩·재생/파형 시간축 | [오디오](Gameplay/Audio.md), [에디터 분석](ChartEditor.md) | 엔진 Audio/Decode; EditorAnalysisController의 reader 주입 |
| Taiko 종류·사운드 정책 | [판정 규칙](Gameplay/Rules.md), [오디오](Gameplay/Audio.md) | `YOUMU.Modes/Taiko` |
| YMM/YMP/YME 문법·히트사운드 볼륨·보간·Whole/Separate·영역 상태 | [ChartFormats](ChartFormats.md) | `YOUMU.Chart/Parsing`, `Model`, `Automation` |
| osu!taiko 외부 EXE 변환·+25ms 오프셋·격자 보정·omit 마디 | [OsuTaikoConverter](OsuTaikoConverter.md) | 형제 저장소 `../OsuTaikoConverter`, Client 프로젝트 참조 |
| BPM·마디·유리수 위치·음수 delay 무효 구간 | [타이밍 문법](Formats/Timing.md) | `YOUMU.Chart/Timing`, `Utility` |
| 편집 트리·드래그/다중 선택·공백 포함/제외 붙여넣기·32단계 Undo/Redo·노트 속성·모드 확장·오디오 탭/OGG 분석·재생/배속/휠 탐색 | [ChartEditor](ChartEditor.md) | ChartEditor/ChartEditorHistory의 원본 트리·편집 이력; EditorSelection / EditorClipboard / EditorWorkspace / Modes/Taiko; `YOUMU.Editor` |
| 곡 목록·포커스·난이도 클릭 진입·UI·미리듣기 | [SongSelect](SongSelect.md) | `Client/GameScene/MusicSelectScene`, `YOUMU.Chart/Catalog` |
| 에디터용 곡 선택·+ 메뉴·새 난이도·osu 가져오기 | [EditorSongSelect](EditorSongSelect.md) | 같은 MusicSelectScene의 Editor 구성; EditorOsuImport / 변환기 ConversionFiles |
| 플레이 화면·노트/마디선 스크롤·키빔·판정 인디케이터/문구·상단 진행바 | [플레이 표시](Gameplay/Presentation.md) | `Client/GameScene/RhythmTestScene`, `Client/Presentation` |
| 공통 플레이 상태머신·ESC·Penpot 일시정지 UI·Resume/Rewind/Retry/Retire·향후 HP/리플레이 계약 | [플레이 일시정지](Gameplay/Pause.md) | GameFlow의 IPlaySessionController / GameplayState; GameplayPauseMenu / GameplayPauseLayout / PlaySessionSeeking / PlayControlEvent |
| Penpot 결과 화면·정확도 랭크·상단 곡 정보·리트라이·판정 통계·오차 그래프·us 입력 로그 | [플레이 결과](Gameplay/Results.md) | `YOUMU.Modes/Mode/Results`, `Taiko/Results`, `Client/GameScene/ResultScene`; 내장 Result PNG |
| 플레이 디버그 조작·전체 노트/Buzz 재현 | [디버깅](Gameplay/Debugging.md) | `GameFlow/Submodules/GameplayInput.cpp`, `Tests/Rhythm/Submodules/BuzzReplayTests.cpp` |
| 로고·타이틀 | [YOUMU](YOUMU.md) | `Client/GameScene/YOUMULogoScene` |
| 게임 표시 이름·UTF-8 언어 INI/영어 fallback·문구·글꼴·공통 UI 색상·옵션/ESC·Songs 경로·해상도·키·글로벌 오프셋·Option.ini | [TextManagement](TextManagement.md) | `Client/Texts/Languages`, TextCatalog/LanguageFile, GameIdentity/화면별 Texts; OptionsPanel/OptionSettings |
| Canvas·입력·클리핑 API 사용 | [Visual2DGuide](Visual2DGuide.md) | 해당 Scene의 Submodules |
| 개발/배포 자산 경로·Songs/BG·YMP BG/offset·난이도별 YMP/YME·skin.ini/씬별 이미지/fallback | [BuiltInAssets](BuiltInAssets.md), [YMP](Formats/Ymp.md) | OptionSettings / GameplayLaunchRequest / GameplayPresenter; SkinManifest / SkinSetSelection / SkinVisualComponent; `YOUMU.Assets` |
| 시작·Scene 전환·종료·소유권 | [ExecutionFlow](ExecutionFlow.md) | `Client/App`, `Client/GameFlow` |
| 엔진 API 자체 수정 | 엔진 AGENTS, EngineOverview의 해당 기능 | `Dependencies/PhantomCore` |
| Penpot 디자인 찾기 | [DesignReferences](DesignReferences.md) | 해당 화면 문서 |
| Claude 리뷰 반영·정적 분석·테스트 실행기/CI | [리뷰 반영 기록](Reviews/ClaudeReview-2026-10-11.md), [검증](Verification.md) | `Tests/Rhythm`, `Tests/Presentation`, `.github/workflows/domain-tests.yml` |
| 코드 품질 재검토·효과 오류 복구/조회/검증 일치 | [품질 수정 기록](Reviews/QualityFixes-2026-10-12.md), [검증](Verification.md) | EffectValidation / PlaySession / ChartEditor; 엔진 Run 숨김 오류 |
| 엔진·변환기·클라이언트 리뷰 8건 수정 | [리뷰 수정 기록](Reviews/CodeReviewFixes-2026-10-22.md), [검증](Verification.md) | Lane/PlaySession/HoldInputRule, ChartEditor; 엔진 Input/TextureManager; 변환기 General/스피너/저장 잠금 |
| FMOD 오류·샘플 수명·비동기 로딩/탐색·음원 끝 경계 조사 | [FMOD 오류 조사](Reviews/FmodErrorAudit-2026-10-17.md) | EditorPlayback / GameplayAudioRouter; 엔진 AudioPlaybackManager / FmodAudioClip / FmodAudioBackend |
| FMOD 오류 수정·음악 채널 Pause 유지·준비 상태/실패·취소 종료 | [FMOD 수정 기록](Reviews/FmodErrorFixes-2026-10-17.md), [검증](Verification.md) | 같은 오디오 소유자; GameplayPause / GameplayState; 엔진 Voice PreparationState / AudioSystem Shutdown |
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
