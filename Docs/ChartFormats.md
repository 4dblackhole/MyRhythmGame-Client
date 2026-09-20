# YMM·YMP·YME 문법

이 문서는 현재 `FingerDrum.Chart` 파서와 `TaikoMode`가 실제로 처리하는 문법을
기준으로 합니다. 세 형식은 UTF-8 텍스트이며, 빈 줄과 `//`로 시작하는 줄은
무시합니다. 경로는 Windows 구분자(`\`)와 슬래시(`/`)를 모두 사용할 수 있습니다.

## YMM: 음악 메타데이터

YMM은 한 음원에 대한 정보입니다. 섹션 없이 `이름: 값` 형식으로 작성합니다.

```text
Version: 1
File: angel dream hand shaking.mp3

Music Name Count: 1
Music Name 1: Angel Dream Handshaking

Artist Count: 1
Artist Composer: Takahashi yoko

Tags: taiko no tatsujin,test
```

| 필드 | 의미 |
| --- | --- |
| `Version` | 정수 형식 버전입니다. 생략하면 1입니다. |
| `File` | 필수 음원 경로입니다. YMM 파일이 있는 폴더를 기준으로 해석합니다. |
| `Music Name ...` | 한 개 이상의 표시 곡명입니다. `Music Name Count` 자체는 개수 안내용이며 현재 파서는 사용하지 않습니다. |
| `Artist ...` | 한 개 이상의 표시 아티스트입니다. `Artist Count` 자체는 현재 파서가 사용하지 않습니다. |
| `Tags` | 쉼표로 구분한 태그입니다. |

곡 선택 화면은 첫 번째 곡명과 첫 번째 아티스트를 표시합니다. `File`이 없으면
파싱 오류이며, `Version`이 정수가 아니거나 실제 음원 파일이 없으면 catalog
검증에 실패합니다. 손상된 YMM 하나는 catalog 전체를 중단시키지 않고 해당 곡만
제외합니다.

## YMP: 플레이 가능한 패턴

YMP는 음악 연결 정보, 난이도, 시간축과 실제 노트를 담습니다.

### Metadata와 Difficulty

```text
[Metadata]
Music metadata: Music/Angeldream/angel dream hand shaking.ymm
Pattern Maker Count: 1
Pattern Maker 1: Maker Name
Pattern Name: Long Notes Test
Tags: test,long-note

Pattern Offset: 104
Preview Time: 1,0/1
Base BPM: 180

[Difficulty]
Mode: Taiko
JudgeLevel: 50
```

| 필드 | 의미 |
| --- | --- |
| `Version` | 정수 형식 버전입니다. 생략하면 1입니다. |
| `Music metadata` | Songs 루트를 기준으로 한 YMM 상대 경로입니다. catalog가 YMM과 YMP를 연결할 때 사용합니다. |
| `Pattern Maker ...` | 제작자 문자열입니다. `Pattern Maker Count`는 현재 개수 검증에 사용하지 않습니다. |
| `Pattern Name` | 곡 선택 화면에 표시할 패턴 이름입니다. |
| `Tags` | 쉼표로 구분한 패턴 태그입니다. |
| `Pattern Offset` | 밀리초 단위 패턴 오프셋입니다. 양수이면 모든 노트가 늦어집니다. |
| `Preview Time` | 기존 파일과의 호환을 위해 남아 있으나 현재 런타임은 사용하지 않습니다. |
| `Base BPM` | 최초 BPM입니다. 0보다 커야 하며, 생략하면 120입니다. |
| `Mode` | 현재 플레이 가능한 값은 `Taiko`입니다. |
| `JudgeLevel` | 판정 창 크기를 정하는 양의 정수입니다. 생략하면 50입니다. 값이 커질수록 판정 창이 좁아집니다. |

`Version`, `JudgeLevel`, BPM·offset 등의 숫자 형식이 잘못된 YMP는 진단을 남기고
곡 선택 catalog에서 해당 패턴만 제외합니다. 같은 catalog의 정상 곡과 패턴은
계속 사용할 수 있습니다. YME의 begin/end 값은 유한한 수여야 하고 duration은
유한한 0 이상의 밀리초여야 하며, 잘못된 명령은 진단과 함께 제외됩니다.

### 마디와 위치

`[Time Signature]`와 `[Pattern]`은 각각 독립적인 현재 마디 번호를 가집니다.
`--` 또는 `---` 한 줄을 만날 때 해당 섹션의 현재 마디가 1 증가합니다.
`---`는 마디를 진행하는 것에 더해 바로 다음 마디를 악보의 새 줄 시작 지점으로
기록합니다. 위치 `N/D`는 현재 마디 시작부터 온음표 `N/D`만큼 떨어진 절대
위치이며, `0/1`은 마디 시작입니다.
마디 길이에 다시 `N/D`를 곱하지 않습니다. 예를 들어 `#measure 3/4`인 마디의
`1/2`는 4분음표 두 개 위치입니다. `N`은 `D`보다 클 수 있지만 현재 마디 길이
미만이어야 합니다. 2/4 마디의 3/4처럼 마디 밖에 놓인 노트나 timing 지시문은
경고와 함께 해당 항목만 무시합니다.

```text
[Time Signature]
0/1,#bpm 180
2/4, #delay 25
#measure 3/4
--
#measure C
```

두 마디 구분선은 다음 의미를 가집니다.

| 구분선 | 의미 |
| --- | --- |
| `--` | 현재 마디를 끝내고 다음 마디로 진행합니다. |
| `---` | 현재 마디를 끝내고 다음 마디로 진행하며, 다음 마디를 새 악보 줄의 시작으로 기록합니다. |

예를 들어 다음 `[Pattern]`에서 두 번째 마디는 같은 악보 줄에 이어지고, 세 번째
마디는 새 악보 줄에서 시작하도록 기록됩니다.

```text
[Pattern]
0/4,1,0
--
0/4,2,0
---
0/4,1,0
```

`---`가 `[Time Signature]`와 `[Pattern]`의 같은 마디 경계에 모두 작성되어도
파서는 새 줄 시작 마디를 하나로 합칩니다. 현재 파서는 이 정보를
`PatternDocument::systemBreakMeasures`에 0부터 시작하는 다음 마디 번호로
보존합니다. 에디터 악보 줄바꿈과 메트로놈 루프 초기화는 아직 구현하지 않았으며,
추후 이 값을 사용합니다.

| 지시문 | 의미 |
| --- | --- |
| `N/D,#bpm 값` | 해당 위치부터 BPM을 변경합니다. |
| `N/D,#delay 밀리초` | 해당 위치부터 누적 지연을 적용합니다. |
| `#measure N/D` | 현재 마디부터 마디 길이 비율을 변경합니다. 예: `3/4`. |
| `#measure C` | 현재 마디부터 기본 `4/4` 길이로 되돌립니다. |

쉼표 앞뒤 공백과 명령·값 사이의 연속 공백은 무시합니다. 분수 내부에는 공백을
넣을 수 없으며 `#`와 명령, 명령과 값은 각각 정확히 붙이거나 띄워야 합니다.

| 문구 | 결과 |
| --- | --- |
| `1/2, #bpm 150` | 허용 |
| `1/2,        #bpm   150` | 허용 |
| `1/ 2, #bpm 150` | 오류 |
| `1/2, #bpm150` | 오류 |
| `#measure 4/4` | 허용 |
| `#measure C` | 허용 |
| `#measure4/4` | 오류 |
| `# measure 4/4` | 오류 |
| `#measure 4/  4` | 오류 |

동일 위치의 BPM은 마지막 선언이 적용됩니다. BPM, 마디 길이, delay와
`Pattern Offset`은 패턴을 불러올 때 정수 마이크로초로 한 번 컴파일됩니다.
마디 길이, 노트 위치, BPM 변경 위치와 subdivision은 64비트 유리수로 누적하고,
BPM 구간별 시간을 계산할 때만 부동소수점으로 변환합니다. 각 노트는 자신보다
앞선 마지막 BPM 변경점의 누적 시간에서 남은 유리수 위치만큼을 더해 계산합니다.
기존 YMP의 화면·음향 효과 지시문은 읽을 때 경고만 내며, YME로 옮기는 대상입니다.

### HitSounds

```text
[HitSounds]
CustomHit: Sounds/custom.wav
```

`인덱스: 경로`를 등록하는 영역입니다. 숫자 인덱스(`1: Sounds/pop.wav`)와 기존
문자열 이름을 모두 지원합니다. 경로는 YMP 기준이며 Client가 곡 로드 시 한 번
등록합니다. 같은 파일의 별칭은 하나의 샘플/재생 채널을 공유합니다.
`[Pattern]`의 hitsound 칸에 지정한 표의 인덱스는 아래 동·캇 기본값 변경보다 우선합니다.

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

### 일반 노트와 롱노트

기본 한 줄은 다음 순서입니다.

```text
N/D,KeyType,ActionType,HitSound,ExtraData...
```

- `HitSound`와 `ExtraData`는 선택 사항입니다.
- `HitSound`는 비워 두고 옵션만 쓰려면 쉼표를 하나 더 둡니다.
  예: `0/4,12,1,,TickDivision=16`, `0/4,15,1,,8`
- `ActionType`은 `0`이 일반 입력, `1`이 롱노트 시작, `2`가 롱노트 끝입니다.
- 롱노트 시작과 끝은 같은 `KeyType`이어야 합니다. 시작 뒤 같은 Lane에서 끝을
  만나기 전까지 나타난 일반 입력 줄은 현재 Taiko 단일-Lane 규칙상 무시됩니다.

| KeyType | 이름 | 동작 |
| ---: | --- | --- |
| 1 | Don | 동 1회 |
| 2 | Kat | 캇 1회 |
| 3 | BigDon | Good 이내의 동 2회 |
| 4 | BigKat | Good 이내의 캇 2회 |
| 5 | Purple | Good 이내 동·캇 각 1회, 순서 무관. 큰 노트 이미지에 보라색 Ambient 적용 |
| 11 | Roll | 구간 안에서 동·캇 연타. 목표 횟수 이상이면 성공, 정확도는 최대 100% |
| 12 | TickRoll | 작성된 각 틱마다 동·캇 입력을 최대 1회 처리 |
| 13 | BigRoll | Roll과 같은 규칙, 큰 노트 두께로 표시 |
| 14 | BigTickRoll | TickRoll과 같은 규칙, 큰 노트 두께로 표시 |
| 15 | Balloon | 구간 안에서 동을 지정 횟수만큼 입력하면 완료하고 풍선 파열음을 재생 |
| 16 | DengDeng | 구간 안에서 동부터 시작해 동·캇을 번갈아 `HitCount`회 입력하면 완료 |
| 17 | Buzz | 지정한 동 또는 캇을 누르는 동안 각 틱의 히트사운드를 재생 |

롱노트 옵션 이름과 `Don`/`Kat` 값은 대소문자를 구분하지 않습니다. 양의 정수
옵션 범위는 1~1024입니다.

| 노트 | 시작 줄의 옵션 | 설명 |
| --- | --- | --- |
| TickRoll, BigTickRoll | `TickDivision=16` | 온음표를 몇 등분할지 지정합니다. 16은 16분음표 간격입니다. 생략 시 16입니다. 각 틱은 현재 `JudgeLevel`의 Good 범위 안에서 한 번만 받을 수 있습니다. |
| Roll, BigRoll, Balloon, DengDeng | `8` | 목표 입력 횟수입니다. bare 양의 정수와 `HitCount=8`을 모두 지원합니다. 생략하면 롱노트의 정확한 유리수 길이 `L`에 대해 `ceil(L × 12)`회입니다. 길이 `1/4`는 3회, `1/7`은 2회입니다. Balloon은 동만, DengDeng은 동부터 교대합니다. |
| Buzz | `Action=Don,TickDivision=16` | 유지할 입력은 `Don` 또는 `Kat`이며 필수입니다. 틱 분할은 생략 시 16입니다. |

TickRoll은 시작점도 하나의 틱이며 모든 틱은 Good 범위 내 성공/실패만
판정합니다. 세부 등급이나 BAD 감점은 없습니다. Buzz는 시작점을 별도의
타이밍 판정으로 처리하고, 이후 틱만 유지 비율의 분모에 포함합니다.
정확도 계산 및 Debug 표시 계약은 [RhythmGameplay.md](RhythmGameplay.md)를 참조합니다.

예시는 다음과 같습니다.

```text
[Pattern]
0/4,11,1,,8
1/4,11,2
2/4,12,1,,TickDivision=16
3/4,12,2
--
0/4,15,1,,8
1/4,15,2
2/4,17,1,,Action=Kat,TickDivision=16
3/4,17,2
```

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

## 롱노트 확인용 채보와 디버그 실행

로컬 파일 `Client/Assets/Songs/Pattern/angeldream/angeldream [long notes test].ymp`는
엔젤드림 핸드셰이크 음원을 참조합니다. Roll, BigRoll, TickRoll, BigTickRoll,
Balloon, DengDeng, Don Buzz, Kat Buzz의 시작점을 2박 간격으로 배치했습니다.
사용자 추가 곡·YMM·YMP·YME는 Git 제외 대상이므로 이 테스트 파일은 작업 PC에만 남습니다.
예외로 `FingerDrum.Assets/Assets/Songs`의 기본 AngelDream 음원·YMM·YMP 3개는 Git에서 추적합니다. 파일이
없는 clean clone에서는 디버그 실행이 같은 배열의 내장 패턴을 사용합니다.

```powershell
MyRhythmGame.exe --rhythm-debug
```

이 모드는 테스트 채보를 바로 열고 -2초에서 일시정지합니다.

- `1` / `2`: 설정 속도로 타이머를 뒤/앞으로 연속 이동
- `3` / `4`: 정확히 -1ms / +1ms 이동
- `-` / `+`: 연속 이동 속도 조절(200~4000ms/s)
- `Space`: 자동 진행/일시정지
- `R`: -2초로 초기화
- `D`, `K`: Kat, `F`, `J`: Don

Debug 빌드의 `RhythmTestScene.cpp`에 있는 `ReferenceTimeDebug`를 켜면 위 조작을
사용합니다. 화면 좌측 상단 DEBUG 텍스트는 노트 객체가 반환하는 ID·상태·시작·
종료·진행도와 현재 타이머, 이동 속도, 시간 차이를 표시합니다. 뒤로 이동하면 이미
소비한 판정 상태와 점수를 초기화한 뒤 해당 시각까지 다시 갱신합니다.
