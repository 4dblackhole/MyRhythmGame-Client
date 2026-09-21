# 타이머·판정·노트 규칙

코드: `FingerDrum.Rhythm/Note/Submodules/<규칙명>.*`, `Lane`, `Judgement`, `Time`.
모드 조립: `FingerDrum.Modes/Taiko/Submodules/TaikoSessionBuilder`, `TaikoLongNoteFactory`.
검증: `RhythmCoreTests`, `AccuracyTests`, `TaikoModeTests`.

## 한 개의 타이머

`RhythmTimer` 하나가 QPC와 리듬 시간을 연결합니다. `AnchorDspClock`은 같은
리듬 시간을 오디오 장치의 sample clock에도 연결합니다. 배경음악과 BMS처럼
미래 시각에 예약할 음은 이 변환을 사용합니다. 실시간 입력 히트사운드는 QPC
timestamp로 판정한 직후 즉시 재생하며 DSP 미래 예약을 사용하지 않습니다.
별도의 `AudioTimeline`은 필요하지 않으며, 일시정지 후에는 DSP clock이 계속
흘렀으므로 현재 리듬 시각과 현재 DSP clock을 다시 anchor해야 합니다.

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

현재 `TaikoMode`는 RPG `PlayScene`과 같은 방식으로 Don, Kat, BigDon,
BigKat, Purple, Roll, TickRoll, Balloon, DengDeng, Buzz를 모두 **한 Lane**에 넣습니다. long-note head를
받은 뒤 일치하는 tail을 받을 때까지 같은 Lane의 중복 head와 일반 Down은
무시합니다. 따라서 종류별 Lane을 따로 두지 않고 전체 노트의 시간순 focus와
배드말림 방지 규칙을 한 곳에서 적용합니다.

## 교체 가능한 노트 규칙

`RuleBasedNote`는 `INoteRule`과 `INoteSoundPolicy`를 조합합니다.

- `TapInputRule`: 한 번 누르는 노트
- `CountedHitInputRule`: 큰 노트처럼 횟수가 필요한 노트
- `SequenceInputRule`: 제한 시간 안에 순서가 필요한 노트. `allowAnyOrder`는
  보라 노트의 동·캇 각 1회를 순서와 무관하게 받습니다.
- `HoldInputRule`: 시작 판정, held 상태와 tick을 갖는 롱노트
- `DrumRollInputRule`: 구간 안의 반복 입력을 받는 드럼롤
- `TickRollInputRule`: 각 틱을 Good 범위에서 한 번만 받는 속도 제한 드럼롤
- `TimedSequenceInputRule`: Balloon의 반복 Don과 DengDeng의 Don/Kat 교대를
  제한 시간 및 필요 횟수로 처리하는 규칙

새 노트는 `INote` 전체를 다시 만들거나, 대부분의 경우 기존
`RuleBasedNote`에 새 `INoteRule`만 주입해 추가합니다.

## 노트별 정확도와 세션 집계

노트의 정확도 집계는 히트사운드/키빔의 개별 `JudgementResult`와 분리합니다.
`RuleBasedNote`가 rule의 목표와 수락된 이벤트로 `NoteAccuracy`를 갱신하고,
Completed/Missed에서 `NoteProcessResult::finalizedAccuracies`로 한 번만 내보냅니다.
`PlaySession`은 각 논리 노트에 동일한 가중치를 주어 평균을 계산합니다.
완료 전 노트는 누적 평균에서 제외하고, 미스 확정 노트는 부분 점수 또는 0점으로
포함합니다. `Reset()`은 현재/마지막 노트 상세와 누적 집계를 모두 초기화합니다.

| 노트 | 최종 정확도 |
| --- | --- |
| Don/Kat | 기존 선형 보간 타이밍 정확도 |
| BigDon/BigKat/Purple | `(1타 정확도 + 2타 정확도) / 2`, 미입력 항목은 0 |
| Roll/BigRoll/Balloon/DengDeng | `min(타격수 / 목표 타격수, 1)`; 시간 초과에도 부분 점수 보존 |
| TickRoll/BigTickRoll | `성공 틱 / 전체 틱`; 시작점도 틱, Good 이내면 1, 나머지는 0 |
| Buzz | `시작 정확도 × 0.5 + 시작점 이후 유지 틱 비율 × 0.5` |

Buzz가 틱 간격보다 짧아 몸통 틱이 하나도 없으면 존재하는 시작 판정만 사용합니다.
입력 시각 이전 틱은 이전 held 상태로 확정하고 입력과 같은 시각의 틱은 변경된
상태로 판정합니다. 같은 동작에 여러 키를 할당한 경우 마지막 키를 놓아야
유지가 해제됩니다. 예를 들어 시작 96%, 유지 98/100이면 최종 97%입니다.
보라 노트의 기본 히트사운드는 수락한 입력에 맞는 Don/Kat이며 명시적 hitsound도
지원합니다. 이는 게임별 라우터를 거치며 새 오디오 채널 정책을 추가하지 않습니다.

노트 규칙은 `INoteRule.h`, 이벤트·정확도는 `NoteTypes.h`, 사운드 정책은 `NoteSoundPolicy.h`만
읽습니다. `Note.h`는 기존 호출부용 facade입니다. 새 규칙 때문에 UI나 엔진 문서를 읽을 필요는 없습니다.
