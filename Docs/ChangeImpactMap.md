# 변경 영향 지도

요청에 맞는 행 하나에서 시작합니다. 먼저 주인 클래스의 public 선언과 해당 구현만 읽고,
계약이 바뀌는 경우에만 호출부·인접 계층으로 확장합니다. 모든 Scene/Note/문서를 미리 읽지 않습니다.
기능 추가 역시 아래 소유 경계를 따릅니다. 해석에 따라 구현이 크게 달라지면 먼저 질문합니다.

| 작업 | 처음 열 파일/폴더 | 함께 확인할 계약·테스트 |
| --- | --- | --- |
| 곡 선택 배치 | `Client/GameScene/MusicSelectScene/Submodules/SongSelectLayout.*`, `SongSelectPanels.cpp` | MusicSelectView.h; lobby smoke |
| 곡 카드/난이도 모양 | 같은 폴더 `SongSelectCards.cpp` | MusicSelectView.h; lobby/editor selector |
| 곡/난이도 이동·검색·정렬 | 같은 폴더 `SongSelectionState.*`, 입력 연결은 SongSelectInput.cpp | SceneStateTests.cpp |
| 곡 미리듣기 | 같은 폴더 `SongPreviewController.*` | AudioPlayback 공개 계약, lobby smoke |
| 문구·언어·글꼴 | `Client/Texts/TextCatalog.*`, 해당 화면의 `Texts/**/<화면>*Texts.*` | TextManagement; 해당 화면 smoke |
| 로고/곡 선택 옵션 패널 | `Client/Presentation/OptionsPanel.*` | LogoView/MusicSelectView 입력 연결; logo/lobby smoke |
| Scene 전환/선택 전달 | Scene 주 파일, `Client/GameFlow/GameplayLaunchStore.h` | SceneStateTests; ExecutionFlow |
| 플레이 Clock·pause·reset·입력 | `RhythmTestScene/Submodules/GameplaySessionController.*`, GameplayInput.cpp | RhythmCoreTests; gameplay smoke |
| 노트 이미지·롱노트 | 같은 폴더 `GameplayNoteVisuals.cpp` | GameplayPresenter.h, NoteVisualKind; gameplay smoke |
| Lane·기어·resize | 같은 폴더 `GameplayLayout.cpp`, GameplaySupport.h | GameplayPresenter.h; gameplay smoke |
| 정확도/특수 노트 표시 | 같은 폴더 `GameplayFeedbackVisuals.cpp` | NoteTypes.h의 이벤트/정확도; AccuracyTests |
| 키 불빛 | 같은 폴더 `GameplayKeys.cpp`, `Client/Presentation/LaneKeyBeam.*` | GameplayFeedback.h; gameplay smoke |
| 에디터 도구/배치 취소 | `Client/EditorScene/Submodules/EditorWorkspace.*`, EditorTool.h | ChartEditorTests, SceneStateTests |
| 에디터 화면/입력 | 같은 폴더 `EditorView.h`, EditorScoreView.cpp / EditorInput.cpp | Workspace.h, editor smoke |
| BPM·metadata·effect UI | 같은 폴더 `EditorTimingView`, `EditorMetadataView`, `EditorEffectsView` | ChartEditor의 Replace 계약, 저장 왕복 |
| 스펙트럼 화면/worker | EditorAudioView / EditorAnalysisController | `FingerDrum.Editor/Audio`; EditorAudioAnalysisTests |
| 노트 판정 규칙 | `FingerDrum.Rhythm/Note/Submodules/<규칙명>.*` | INoteRule.h, NoteTypes.h; RhythmCore/AccuracyTests |
| Taiko 종류 추가 | `FingerDrum.Modes/Taiko/Submodules/TaikoNoteDefinition.h`, TaikoSessionBuilder / TaikoLongNoteFactory | EditorTool, NoteVisualKind→Client 이미지, TaikoModeTests |
| 노트 사운드 정책 | 같은 폴더 TaikoSoundPolicy.cpp / TaikoSoundIds.h | NoteSoundPolicy.h; SoundPolicyTests |
| 문법 추가 | `FingerDrum.Chart/Parsing/Submodules/<형식>Parser.cpp` | 해당 Model/Submodules 문서형, ChartEditor 저장, ChartParserTests |
| BPM/마디 시간 | `FingerDrum.Chart/Timing/MusicalTimeline.*` | Formats/Timing; ChartParser/ChartEditorTests |

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
    EditorWorkspace.*         문서와 편집 상태
    EditorView.*              UI 조립과 표시
    EditorAnalysisController.* worker 수명과 결과
Client/Texts/
  TextCatalog.*                  공유 언어 상태와 언어별 글꼴
  <사용 위치>/                  화면별 한국어·영어 문구 표
FingerDrum.Rhythm/Note/
  Note.h                      기존 호출부 호환 facade
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
