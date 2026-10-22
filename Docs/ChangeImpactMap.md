# 변경 영향 지도

요청에 맞는 행 하나에서 시작합니다. 먼저 주인 클래스의 public 선언과 해당 구현만 읽고,
계약이 바뀌는 경우에만 호출부·인접 계층으로 확장합니다. 모든 Scene/Note/문서를 미리 읽지 않습니다.
기능 추가 역시 아래 소유 경계를 따릅니다. 해석에 따라 구현이 크게 달라지면 먼저 질문합니다.

| 작업 | 처음 열 파일/폴더 | 함께 확인할 계약·테스트 |
| --- | --- | --- |
| 곡 선택 배치 | `Client/GameScene/MusicSelectScene/Submodules/SongSelectLayout.*`, `SongSelectPanels.cpp` | MusicSelectView.h; lobby smoke |
| 새 난이도 메타데이터/파일 생성 | MusicSelectScene/Submodules의 NewDifficultyDialog / EditorPatternCreation; SongSelectCards / SongSelectInput | EditorSongSelect; RunNewDifficultyTests / MusicSelectViewTests; Editor 진입 snapshot |
| 곡 카드 + 팝업·osu 가져오기·중복 음원/폴더 | 같은 Submodules의 SongSelectCreateMenu / EditorOsuImport; 변환기 Core/ConversionFiles / App/Main | EditorSongSelect / OsuTaikoConverter; MusicSelectViewTests / ConverterTests / CliTests |
| 곡 카드/난이도 모양 | 같은 폴더 `SongSelectCards.cpp` | MusicSelectView.h; lobby/editor selector |
| 곡/난이도 이동·검색·정렬 | 같은 폴더 `SongSelectionState.*`, 입력 연결은 SongSelectInput.cpp | SceneStateTests.cpp |
| 곡 미리듣기 | 같은 폴더 `SongPreviewController.*` | AudioPlayback 공개 계약, lobby smoke |
| 문구·언어 INI·영어 기본값·글꼴 | `Client/Texts/Languages`, TextCatalog/LanguageFile, 해당 화면의 `*Texts.*` | TextManagement; RunLanguageFileTests, 해당 화면 smoke/표시 회귀 |
| 게임 표시 이름 | `Client/Texts/GameIdentity.props`, `Scripts/GenerateGameIdentity.ps1` | YOUMUGame의 창 제목, Main의 오류창, EditorInput/EditorTexts의 확인창; TextManagement, 네 smoke |
| 텍스트 배치·clip·아틀라스/업로드·긴 제목 | 엔진 `Graphics.D3D12/Text`, `Graphics.D3D12/Visual2D`, `Core/Visual2D`; `Client/Presentation/MarqueeTextComponent.*` | 엔진 TextRendering 문서/SDK --text-rendering·--text-pixels; MusicSelectViewTests; Client 직접 DrawPacket 생성/검사, 네 smoke |
| 로고/곡 선택 옵션 패널·ESC 닫기 | `Client/Presentation/OptionsPanel.*` | LogoView/MusicSelectView 입력 소비; OptionsPanelTests; logo/lobby smoke |
| 공통 UI 색상 | `Client/Presentation/UiColors.h` | LogoStyle / SongSelectLayout / EditorSupport / TaikoResultLayout / GameplayPauseMenu; Options/MusicSelect/Editor/Result/Pause 표시 회귀 |
| Option.ini·오디오 출력/드라이버·해상도·키·글로벌 오프셋 선택 | `Client/App/OptionSettings.*`, `OptionsPanel.*` / `OptionsPanel/Submodules` | YOUMUGame 초기화; ModeInputLayout / PlayGameModeFactory / PlaySession; MusicalTimeline / ChartEditor; 엔진 WindowControl; OptionsPanelTests / OptionTimingTests, 네 smoke |
| Songs 경로·즉시 catalog 갱신·BG/offset | OptionSettings::SongsRoot / OptionsSongs, MusicSelectScene / SongCatalogLoader, GameplayLaunchRequest | PatternDocument / PatternParser / ChartEditor; GameplayPresenter / GameplayLayout / PrepareAssets; TaikoEditorAudio/Metadata; Options/MusicSelect/Gameplay 표시 회귀; 변환기 전용 세션 |
| skin.ini·씬 이미지·스킨 선택·내장 기본본 | `Client/App/SkinManifest.*`, `SkinSetSelection.*`, `AssetPaths.h`, `Presentation/SkinVisualComponent.*` | BuiltInAssets; RunSkinTests / 각 화면 표시 회귀, 네 smoke |
| 배포 빌드·자산 | `Client/YOUMU.Client.vcxproj`, `Client/App/Main.cpp` | BuiltInAssets; Debug/Release smoke, CheckArchitecture |
| Scene 전환/선택 전달 | Scene 주 파일, `Client/GameFlow/GameplayLaunchStore.h` | SceneStateTests; ExecutionFlow |
| 플레이 Clock·pause·reset·입력 | `Client/GameFlow/IPlaySessionController.h`, GameplaySessionController / Submodules의 GameplayState·GameplayInput | RhythmCoreTests; gameplay smoke |
| ESC 메뉴·음악 seek·마디 되감기·Retire·HP/리플레이 마커 | GameFlow/Submodules/GameplayPause / RhythmTestScene/Submodules/GameplayPauseMenu·GameplayPauseLayout, Texts의 PauseTexts; `Mode/Submodules/PlaySessionSeeking.cpp` | PlayControlEvent / IPlayResultCollector / TaikoResultCollector; PlayPauseTests / GameplayPauseTests / ResultPresentationTests; Gameplay/Pause 문서 |
| 노트 이미지·롱노트·마디선 스크롤 | 같은 폴더 `GameplayNoteVisuals.cpp` | GameplayPresenter의 LaneObjectPosition, PlaySession/MeasureLinePresentationInfo, TaikoSessionBuilder; YmeEffectsTests/GameplayJudgementTests; gameplay smoke |
| Lane·기어·resize | 같은 폴더 `GameplayLayout.cpp`, GameplaySupport.h | GameplayPresenter.h; gameplay smoke |
| 상단 진행바·현재/전체 시간 | 같은 폴더 `GameplayProgressView.*`, GameplayLayout.cpp | PlaySession의 컴파일된 head/tail; GameplayJudgementTests, 실제 전체 노트 replay |
| 정확도/특수 노트 표시 | 같은 폴더 `GameplayFeedbackVisuals.cpp` | NoteTypes.h의 이벤트/정확도; AccuracyTests/GameplayJudgementTests |
| 판정 구간·타이밍 마커·링/문구 | 같은 폴더 `GameplayJudgementView.*`, GameplayFeedbackVisuals.cpp | AccuracyRange, LaneKeyBeam의 색상/입력 응답; GameplayJudgementTests |
| 점수 정밀도·결과 수집·입력 로그 | `Judgement/AccuracyScore.h`, `Mode/Results`, `Taiko/Results` | NoteAccuracy/PlaySession, TaikoSessionBuilder; PlayResultTests |
| Result 디자인·전환·리트라이·모드별 통계/그래프 | `Client/GameScene/ResultScene`, `Texts/GameScene/ResultScene`, `Default Skin/Result` | YOUMUGame/GameplayLaunchRequest/MusicSelectScene; ResultPresentationTests; Noto Sans KR/엔진 fontWeight |
| 키 불빛 | 같은 폴더 `GameplayKeys.cpp`, `Client/Presentation/LaneKeyBeam.*` | GameplayFeedback.h; gameplay smoke |
| 에디터 모드 추가/파일 교체 | `Client/EditorScene/Submodules/Modes/IEditorMode.h`, EditorModeFactory; `YOUMU.Chart/Editing/IEditorDocument.h` | ChartEditor의 교체 경계; EditorModeTests, editor smoke |
| 에디터 상단 메뉴·저장 입력 | `Client/EditorScene/Submodules/EditorMenuBar.*`, EditorView / EditorInput | EditorWorkspace Save, EditorTexts; EditorMenuTests / EditorAudioViewTests, editor smoke |
| 에디터 도구/배치 취소 | `Modes/Taiko/TaikoEditorMode.*`, TaikoEditorTool.h / TaikoEditorTools.cpp | ChartEditorTests, SceneStateTests, EditorModeTests |
| 에디터 노트 선택/속성 | `Modes/Taiko/TaikoEditorNoteInspector.*`, TaikoEditorPlacement.h | IEditorContext/Workspace UpdateNote, IEditorDocument FindNote/UpdateNote, ChartEditor 인덱스/ID; ChartEditorTests/EditorModeTests |
| 구간·행 다중 선택·두 붙여넣기·32 Undo/Redo | EditorSelection / EditorClipboard / TaikoEditorSelection·Layout, EditorTiming/EffectsView | MusicalTimeline AddWholeNotes/WholeNotesBetween; ChartEditorHistory / IEditorDocument; EditorModeTests / ChartEditorTests |
| 에디터 노트 화면/입력 | `Modes/Taiko/TaikoEditorScore.cpp`, 공통 연결 EditorScoreView / EditorInput | Workspace.h, EditorModeTests, editor smoke |
| BPM·metadata·effect UI | EditorTimingView / EditorEffectsView / EditorForms, `Modes/Taiko/TaikoEditorMetadata` | Workspace의 수정 경계, IEditorContext, IEditorDocument Replace/ReplaceTiming/ReplaceEffects, 저장 왕복 |
| 오디오 탭·시간 영역 선택·타임라인/worker | EditorAudioView / EditorAudioSelection / EditorTimelineView / EditorAnalysisController | EditorInput / EditorWorkspace; `YOUMU.Editor/Audio`; EditorAudioAnalysisTests / EditorAudioViewTests |
| 에디터 offset/메트로놈·박자/예상 타격 눈금 | EditorAudioControls / EditorMetronome / EditorWorkspace | MusicalTimeline / EditorPlayback의 DSP 예약; EditorMetronomeTests / EditorAudioViewTests |
| 에디터 재생·배속·시간축 줌 | EditorPlayback / EditorWorkspace / EditorAudioControls / EditorScoreView | EditorInput / EditorScene / TaikoEditorScore; EditorPlaybackTests / EditorModeTests; 엔진 AudioClip/Voice |
| JudgeLevel 고정소수점/범위 | `YOUMU.Rhythm/Judgement/JudgeLevel.h`, AccuracyRange; PatternDocument / PatternParser / ChartEditor | TaikoSessionBuilder / IPlayResult / GameplayLayout; RhythmCoreTests / ChartParserTests / NewDifficultyTests |
| 노트 판정 규칙 | `YOUMU.Rhythm/Note/Submodules/<규칙명>.*` | INoteRule.h, NoteTypes.h; RhythmCore/AccuracyTests |
| 만료 노트 입력 전달·복수 물리 키 유지·rewind | Lane / HoldInputRule / PlaySession / GameplayInput | InputOrderingTests; PlayPauseTests / GameplayPauseTests; Raw Input timestamp·전이 전 snapshot |
| Taiko 종류 추가 | `YOUMU.Modes/Taiko/Submodules/TaikoNoteDefinition.h`, TaikoSessionBuilder / TaikoLongNoteFactory | TaikoEditorTool, NoteVisualKind→Client 이미지, TaikoModeTests |
| 노트 사운드·옵션/예측 | 같은 폴더 TaikoSoundPolicy / TaikoNoteOptions / TaikoChartAudio / TaikoAudioPreview | NoteSoundPolicy, TaikoEditorAudio; SoundPolicyTests / EditorModeTests |
| 문법 추가 | `YOUMU.Chart/Parsing/Submodules/<형식>Parser.cpp` | 해당 Model/Submodules 문서형, ChartEditor 저장, ChartParserTests; SongCatalogParserRevision 증가·SongCatalogIndex 캐시 재구축 회귀 |
| YME 보간·Whole/Separate | `YOUMU.Chart/Automation/InterpolationExpression.*`, `ScrollAutomation.*` | MusicalTimeline / PlaySession / GameplayPresenter / TaikoEditorScore; YmeEffectsTests, 실제 -Effects replay |
| 효과 검증·런타임 실패 복구 | `YOUMU.Chart/Automation/EffectValidation.*`, `PlaySession.*` | EffectParser / ChartEditor / TaikoMode; GameplaySessionController / Presenter 오류 HUD; YmeEffectsTests |
| 싱코페이션 판정 범위·Kiai 상태 | `YOUMU.Rhythm/Judgement/AccuracyRange.*`, `YOUMU.Modes/Taiko/Submodules/TaikoBuildSupport.h` | TaikoSessionBuilder / RuleBasedNote / PlaySession; YmeEffectsTests |
| BPM/마디 시간·delay 무효 구간 | `YOUMU.Chart/Timing/MusicalTimeline.*` | CompiledChart의 유효성 snapshot, TaikoChartNotes의 롱 결합; Formats/Timing; ChartParser/YmeEffects/EditorModeTests |
| 편집 원본 트리·안정 ID·구간 조회·저장 실패 복구 | `YOUMU.Chart/Editing/ChartEditor.*`, `IEditorDocument.h` | ChartEditorTests/ChartReviewRegressionTests; TaikoEditorScore, EditorWorkspace |
| 플레이 자동화 검색·DSP 준비 | `YOUMU.Modes/Mode/Submodules/PlaySession.*`, `Client/Audio/GameplayAudioRouter.*` | TaikoModeTests; GameplayAudio, 전체 노트 replay |

테스트 파일은 `Tests/Rhythm/Submodules/`에 있으며 같은 테스트 실행 파일에 링크됩니다.
ClientLogic는 곡 선택 상태를 Client와 테스트에 같은 라이브러리로 제공합니다.
테스트 프로젝트에서 Client 구현 cpp를 직접 재컴파일하거나 include하지 않습니다.

## 폴더·필터 규칙

```text
Client/GameScene/MusicSelectScene/
  MusicSelectScene.h/.cpp       수명주기, 객체 조립, 화면 전환
  Submodules/
    SongSelectionState.h/.cpp  순수 선택 상태/정책
    MusicSelectView.h/.cpp     Canvas/노드, 표시와 입력 의도
    SongSelectLayout.cpp       같은 View의 배치 구현
    SongSelectCards.cpp        같은 View의 카드 표시 구현
    SongPreviewController.*   voice/미리듣기
Client/EditorScene/
  EditorScene.h/.cpp
  Submodules/
    EditorWorkspace.*         문서/모드 수명과 공통 편집 상태
    EditorView.*              공통 UI 조립과 표시
    EditorAnalysisController.* worker 수명과 결과
    EditorForms.*              이름 있는 박자/이펙트 초안과 후보 생성
    EditorTime.h               정수 us 범위 및 마디 탐색 검증
    Modes/
      IEditorMode.h           모드별 도구/그리기/좌표/사운드 계약
      IEditorContext.h        모드용 문서 조회/편집 명령 경계
      IEditorAudioSource.h    worker용 읽기 전용 파일/marker 계약
      EditorModeFactory.*     진입 시 지원 모드 선택
      Taiko/                  Taiko 전용 상태와 구현
Client/Texts/
  TextCatalog.*                  공유 언어 상태와 언어별 글꼴
  <사용 위치>/                  화면별 한국어·영어 문구 표
  EditorScene/Taiko/              Taiko 편집 도구/사운드 문구
YOUMU.Rhythm/Note/
  Submodules/
    NoteTypes.*               이벤트/정확도 값
    INote.h / INoteRule.*      계약
    RuleBasedNote.*           규칙/사운드 조립
    NoteSoundPolicy.*         상태별 음향 매핑
    <규칙명>.h/.cpp            각 규칙의 상태와 구현
```

다른 제품 Scene도 동일한 트리입니다. 파일 분할은 책임 클래스의 소유권 분리와 함께
수행하며, 하나의 Scene의 멤버 함수를 여러 cpp로 옮기는 것으로 끝내지 않습니다.
UI 클래스 내부의 긴 배치 구현은 책임별 cpp로 나눌 수 있습니다.
물리 경로와 Visual Studio 필터를 일치시킵니다. 새 파일 등록 후
`Scripts/CheckArchitecture.ps1`와 [검증](Verification.md)을 실행합니다.

## 지켜야 할 경계

- Rhythm/Chart/Modes/Editor 분석 라이브러리는 Client/엔진을 역참조하지 않습니다.
- 영속 파일 ID는 파서 경계의 int/string으로 보존합니다. Taiko는 정의 표에서 의미를 해석하고,
  표시는 NoteVisualKind, 내장 cue는 TaikoSoundIds를 사용합니다. 사용자 사운드 인덱스는 문자열입니다.
- Scene은 Controller/Presenter를 소유합니다. UI는 전환 의도를 반환하고 SceneManager를 호출하지 않습니다.
- 공유 launch store의 Snapshot을 진입 시 복사합니다. 편집/플레이 중 상태를 다음 Scene과 공유하지 않습니다.
- 새 이벤트 버스·서비스 로케이터·범용 프레임워크를 이 구조의 전제조건으로 추가하지 않습니다.
