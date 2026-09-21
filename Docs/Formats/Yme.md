# YME 자동화·사운드 변경

코드: `Parsing/Submodules/EffectParser.cpp`, `Model/Submodules/EffectDocument.h`.
평가: `FingerDrum.Modes/Mode/Submodules/PlaySession.cpp`, 박자 속도 보간: `Timing/MusicalTimeline.cpp`.
저장: `FingerDrum.Chart/Editing/ChartEditor.cpp`. 검증: `SoundPolicyTests`, `ChartEditorTests`.

UTF-8 텍스트이며 빈 줄과 `//`로 시작하는 줄을 무시합니다. 경로에는 `\\`와 `/`를 사용할 수 있습니다.

## YME: automation과 박자 영역

기존 수치 automation 형식을 유지합니다. `[Effects]`의 `--`는 YMP처럼 마디를 진행시킵니다.
다음 예는 첫 마디 0/4부터 2/4까지 스크롤 배율을 1에서 2로 선형 증가시킵니다.

```text
Version: 1
[Effects]
0/4, #ScrollSpeed, , 1, 2, 0, Linear, End=1:2/4
--
0/4, #MeasureLineVisible, , 0, 0, 0, Step
```

필드는 `N/D, #종류, 대상, 시작값, 끝값, 기존 ms길이, 보간[, End=마디:N/D]`입니다.
끝 마디는 1부터 셉니다. End가 있으면 유리수 박자 영역이 우선합니다. ScrollSpeed와
NoteSpeed는 노트의 박자를 기준으로 보간하며, 둘을 함께 지정하면 배율을 곱합니다.
영역 종료 후 끝값을 유지하고 다음 지시에서 바뀝니다. End가 없는 고정값은 시작/끝을
같게 둡니다. BusVolume 대상은 `Music`, `HitSound`, `TickSound`, `UserInputFeedback`,
`UI` 중 버스 이름입니다. MeasureLineVisible은 0=숨김, 1=표시입니다.
과거 문서의 단독 `#measureLineVisible ON/OFF`는 예약 문구였으며 저장 문법은 위와 같습니다.

### YME 히트사운드 변경

YME는 YMP와 같은 폴더에서 같은 stem을 사용합니다. 표 자체는 YMP의 `[HitSounds]`에
두고 YME에서 참조합니다. 다음은 네 번째 마디의 2/4부터 캇을 `1`로 바꾸는 예입니다.

```text
Version: 1
[HitSound Changes]
4, 2/4, 1, 2
```

순서는 `마디번호(1부터), N/D, 히트사운드 인덱스, 노트ID`입니다. ID 1은 동,
ID 2는 캇입니다. 둘 다 바꾸려면 같은 위치에 ID 1과 2를 각각 적습니다.
각 설정은 다음 해당 입력의 변경까지 유지됩니다. 같은 입력/위치에서는 마지막 줄이
우선합니다. BPM·마디 길이·offset을 반영한 cue 시각 기준으로 적용되며 빈 입력,
큰 노트, 보라 노트, 연타와 Buzz에도 적용됩니다. 풍선 파열음은 동·캇음과 별개입니다.
미등록 인덱스, 마디 밖 위치, 잘못된 노트 ID는 로드 오류입니다.
