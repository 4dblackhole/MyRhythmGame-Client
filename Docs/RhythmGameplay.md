# 플레이 기능 진입점

렌더 시간이 아닌 timestamp 입력과 하나의 RhythmTimer가 판정의 기준입니다.
변경하려는 책임에 해당하는 문서만 읽습니다.

| 요청 | 최소 문서 | 코드 |
| --- | --- | --- |
| 판정·정확도·노트 규칙 | [Rules](Gameplay/Rules.md) | Rhythm/Note/Submodules |
| 키빔·노트·HUD·Lane 배치 | [Presentation](Gameplay/Presentation.md) | RhythmTestScene/Submodules/GameplayPresenter |
| 파일 등록·사운드 정책·재생 | [Audio](Gameplay/Audio.md) | GameplayAudio / GameplayAudioRouter / TaikoSoundPolicy |
| 1ms 타이머·디버그 표시 | [Debugging](Gameplay/Debugging.md) | GameplayInput / GameplayFeedbackVisuals |
| YMP/YME 의미 변경 | [ChartFormats](ChartFormats.md)에서 해당 형식 | Chart/Parsing/Submodules |

`RhythmTestScene`은 SessionController와 Presenter를 조립하고 Scene 전환만 요청합니다.
Controller는 타이머·PlaySession·오디오 라우터를, Presenter는 Canvas·노드·키빔을 소유합니다.
노트 처리는 `NoteEvent`/`AudioCueRequest`를 만들고 UI 전달은 `GameplayFeedback`을 사용합니다.
모드는 타입이 있는 `NoteVisualKind`를 전달하며 PNG 매핑은 Client에 남습니다.
진입 선택은 `GameplayLaunchStore::Snapshot()`으로 복사되어 현재 세션에서 독립적으로 유지됩니다.
