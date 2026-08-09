# FingerDrum 리듬게임 구조

이 구조의 기준은 “렌더 프레임 시각”이 아니라 timestamp가 붙은 입력 시각과
하나의 리듬 timeline입니다. 렌더 FPS가 흔들려도 판정 시각은 바뀌지 않습니다.

```mermaid
flowchart LR
    Raw["Raw Input · QPC timestamp"] --> Timer["RhythmTimer"]
    Timer --> Session["PlaySession"]
    Parser["YMM · YMP · YME"] --> Mode["IPlayGameMode"]
    Mode --> Session
    Session --> Lane["ScrollGear · Lane"]
    Lane --> Note["INote · INoteRule"]
    Note --> Result["NoteEvent · AudioCueRequest"]
    Result --> View["Scene presentation"]
    Result --> Audio["GameplayAudioRouter · DSP clock"]
```

## 프로젝트 경계

- `FingerDrum.Rhythm`: 외부 라이브러리에 의존하지 않는 시간, 판정, 노트,
  Lane과 ScrollGear 논리입니다.
- `FingerDrum.Chart`: YMM/YMP/YME 파싱과 박자+BPM을 마이크로초로 한 번
  컴파일하는 계층입니다.
- `FingerDrum.Modes`: `IPlayGameMode`와 실제 노트 규칙을 생성하는 모드
  구현입니다. 현재 `TaikoMode`가 테스트 드라이버 역할을 합니다.
- `Client/Audio/GameplayAudioRouter`: 의미 기반 SoundId와 BusId를 엔진
  `AudioClip`, `AudioBus`, `AudioEffect`에 연결하는 게임 전용 계층입니다.

## 한 개의 타이머

`RhythmTimer` 하나가 QPC와 리듬 시간을 연결합니다. `AnchorDspClock`은 같은
리듬 시간을 오디오 장치의 sample clock에도 연결합니다. 배경음악, BMS 예약
음, 히트사운드는 모두 이 변환을 사용해야 하므로 별도의 `AudioTimeline`은
필요하지 않습니다. 일시정지 후에는 DSP clock이 계속 흘렀으므로 현재 리듬
시각과 현재 DSP clock을 다시 anchor해야 합니다.

## 판정과 점수

`JudgementProfile`은 여러 개 생성할 수 있어 기본 노트와 싱코페이션 노트에
서로 다른 범위를 적용할 수 있습니다. 판정 레벨 50의 반범위는 다음과
같습니다.

| 등급 | ± 범위 |
| --- | ---: |
| MAX | 4.5 ms |
| Perfect | 9.5 ms |
| Great | 23.5 ms |
| Good | 54.5 ms |
| Bad | 90 ms |

레벨 `L`의 범위는 `50 / L`배가 됩니다. 따라서 레벨 100은 정확히 절반입니다.
점수는 판정 경계 사이를 선형보간합니다. 예를 들어 MAX 5 ms=100점,
Perfect 9 ms=90점이면 7 ms는 95점입니다.

## Lane과 배드말림 방지

Lane은 노트를 시간순으로 안정 정렬하고 가장 앞의 미처리 노트에 우선권을
줍니다. early Bad 입력은 시도만 보고하고 focus를 유지합니다. 현재 노트가
late Bad이며 다음 노트가 같은 입력을 Good 이내로 받을 수 있으면 앞 노트를
Miss 처리한 뒤 같은 입력을 다음 노트에 전달합니다.

## 교체 가능한 노트 규칙

`RuleBasedNote`는 `INoteRule`과 `INoteSoundPolicy`를 조합합니다.

- `TapInputRule`: 한 번 누르는 노트
- `CountedHitInputRule`: 큰 노트처럼 횟수가 필요한 노트
- `SequenceInputRule`: 제한 시간 안에 순서가 필요한 노트
- `HoldInputRule`: 시작 판정, held 상태와 tick을 갖는 롱노트
- `DrumRollInputRule`: 구간 안의 반복 입력을 받는 드럼롤

새 노트는 `INote` 전체를 다시 만들거나, 대부분의 경우 기존
`RuleBasedNote`에 새 `INoteRule`만 주입해 추가합니다.

## 상태 기반 히트사운드

노트 규칙은 직접 FMOD를 호출하지 않습니다. 상태 전이인 `NoteEvent`를 만들고
`INoteSoundPolicy`가 이를 `AudioCueRequest`로 변환합니다. binding은 event,
이전/다음 상태, 판정 등급, hit index, tick index를 조건으로 사용할 수 있습니다.

- 큰 노트는 Good 이상인 첫 번째 accepted hit(`hitIndex == 0`)에서만 전용
  사운드를 냅니다.
- 범위 밖 입력은 노트 전용 사운드가 아니라 `UserInputFeedback` bus의 일반
  Don/Kat 사운드를 냅니다.
- 홀드 tick은 `TickAccepted`마다 한 번만 `TickSound` bus로 전달됩니다.

이 분리 덕분에 같은 노트 규칙을 유지한 채 스킨별 파일, 볼륨, pitch, pan,
우선순위와 bus를 바꿀 수 있습니다.

## 실시간 오디오 효과

YME의 `BusVolume`, `ReverbSend`, `LowPassCutoff`, `HighPassCutoff` 자동화는
`PlaySession::EvaluateAutomation`에서 timeline 값으로 평가한 후
`GameplayAudioRouter`가 엔진 bus/effect에 적용합니다. 향후 compressor,
delay 또는 모드 전용 효과는 차트 명령과 라우터 mapping을 추가하되 노트
규칙에서는 FMOD 타입을 참조하지 않습니다.

## 테스트 드라이버

Lobby의 PLAY를 누르면 `RhythmTestScene`에 진입합니다.

- `D`, `K`: Kat
- `F`, `J`: Don
- `Space`: 일시정지/재개
- `R`: 처음부터 다시 시작
- `Escape`: 곡 선택으로 복귀

순수 로직 회귀 테스트는 `FingerDrum.Rhythm.Tests.exe`이며 판정 scaling,
보간 점수, Lane focus, 큰 노트 사운드, tick 중복 방지, legacy YMP와 YME를
검증합니다.
