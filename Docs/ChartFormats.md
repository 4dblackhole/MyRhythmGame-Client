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
파싱 오류이며, 실제 음원 파일이 없으면 catalog 검증에 실패합니다.

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

### 마디와 위치

`[Time Signature]`와 `[Pattern]`은 각각 독립적인 현재 마디 번호를 가집니다.
`--` 한 줄을 만날 때 해당 섹션의 현재 마디가 1 증가합니다. 위치 `N/D`는 현재
마디 길이의 `N/D` 지점을 뜻하며, `0/1`은 마디 시작입니다. `N`은 `D`보다 커도
되지만 일반 채보에서는 다음 마디로 넘기기보다 `--`를 사용하는 편이 명확합니다.

```text
[Time Signature]
0/1,#bpm 180
2/4,#delay 25
#measure 3/4
--
#measure C
```

| 지시문 | 의미 |
| --- | --- |
| `N/D,#bpm 값` | 해당 위치부터 BPM을 변경합니다. |
| `N/D,#delay 밀리초` | 해당 위치부터 누적 지연을 적용합니다. |
| `#measure N/D` | 현재 마디부터 마디 길이 비율을 변경합니다. 예: `3/4`. |
| `#measure C` | 현재 마디부터 기본 `4/4` 길이로 되돌립니다. |

동일 위치의 BPM은 마지막 선언이 적용됩니다. BPM, 마디 길이, delay와
`Pattern Offset`은 패턴을 불러올 때 정수 마이크로초로 한 번 컴파일됩니다.
기존 YMP의 화면·음향 효과 지시문은 읽을 때 경고만 내며, YME로 옮기는 대상입니다.

### HitSounds

```text
[HitSounds]
CustomHit: Sounds/custom.wav
```

`이름: 경로`를 등록하는 영역입니다. 현재 파서는 이 표를 보존하지만 Client가
파일을 자동 등록하지는 않습니다. `[Pattern]`의 hitsound 칸을 사용할 때는 실행
중 등록된 `SoundId`와 일치해야 합니다.

### 일반 노트와 롱노트

기본 한 줄은 다음 순서입니다.

```text
N/D,KeyType,ActionType,HitSound,ExtraData...
```

- `HitSound`와 `ExtraData`는 선택 사항입니다.
- `HitSound`는 비워 두고 옵션만 쓰려면 쉼표를 하나 더 둡니다.
  예: `0/4,12,1,,TickDivision=16`
- `ActionType`은 `0`이 일반 입력, `1`이 롱노트 시작, `2`가 롱노트 끝입니다.
- 롱노트 시작과 끝은 같은 `KeyType`이어야 합니다. 시작 뒤 같은 Lane에서 끝을
  만나기 전까지 나타난 일반 입력 줄은 현재 Taiko 단일-Lane 규칙상 무시됩니다.

| KeyType | 이름 | 동작 |
| ---: | --- | --- |
| 1 | Don | 동 1회 |
| 2 | Kat | 캇 1회 |
| 3 | BigDon | Good 이내의 동 2회 |
| 4 | BigKat | Good 이내의 캇 2회 |
| 11 | Roll | 구간 안에서 동·캇 입력을 횟수 제한 없이 처리 |
| 12 | TickRoll | 작성된 각 틱마다 동·캇 입력을 최대 1회 처리 |
| 13 | BigRoll | Roll과 같은 규칙, 큰 노트 두께로 표시 |
| 14 | BigTickRoll | TickRoll과 같은 규칙, 큰 노트 두께로 표시 |
| 15 | Balloon | 구간 안에서 동을 `HitCount`회 입력하면 완료하고 풍선 파열음을 재생 |
| 16 | DengDeng | 구간 안에서 동부터 시작해 동·캇을 번갈아 `HitCount`회 입력하면 완료 |
| 17 | Buzz | 지정한 동 또는 캇을 누르는 동안 각 틱의 히트사운드를 재생 |

롱노트 옵션 이름과 `Don`/`Kat` 값은 대소문자를 구분하지 않습니다. 양의 정수
옵션 범위는 1~1024입니다.

| 노트 | 시작 줄의 옵션 | 설명 |
| --- | --- | --- |
| TickRoll, BigTickRoll | `TickDivision=16` | 온음표를 몇 등분할지 지정합니다. 16은 16분음표 간격입니다. 생략 시 16입니다. 각 틱은 현재 `JudgeLevel`의 Good 범위 안에서 한 번만 받을 수 있습니다. |
| Balloon | `HitCount=8` | 필요한 동 입력 횟수이며 필수입니다. |
| DengDeng | `HitCount=8` | 필요한 교대 입력 총횟수이며 필수입니다. 첫 입력은 동입니다. |
| Buzz | `Action=Don,TickDivision=16` | 유지할 입력은 `Don` 또는 `Kat`이며 필수입니다. 틱 분할은 생략 시 16입니다. |

예시는 다음과 같습니다.

```text
[Pattern]
0/4,11,1
1/4,11,2
2/4,12,1,,TickDivision=16
3/4,12,2
--
0/4,15,1,,HitCount=8
1/4,15,2
2/4,17,1,,Action=Kat,TickDivision=16
3/4,17,2
```

## YME: 마디선 표시 문구

마디선 표시 전환 문구는 다음 모습으로 예약합니다.

```text
#measureLineVisible ON
#measureLineVisible OFF
```

마디선 전환의 구체적인 위치를 어떤 문법으로 지정할지는 아직 정하지 않았습니다.
따라서 위치 접두사나 섹션 구조를 이 문서에서 정의하지 않으며, 위 두 문구도 현재
YME 파서와 화면에 아직 연결되지 않은 설계 표기입니다.

## 롱노트 확인용 채보와 디버그 실행

로컬 파일 `Client/Assets/Songs/Pattern/angeldream/angeldream [long notes test].ymp`는
엔젤드림 핸드셰이크 음원을 참조합니다. Roll, BigRoll, TickRoll, BigTickRoll,
Balloon, DengDeng, Don Buzz, Kat Buzz의 시작점을 2박 간격으로 배치했습니다.
곡·YMM·YMP·YME는 Git 제외 대상이므로 이 파일은 작업 PC에만 남습니다. 파일이
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

화면의 DEBUG 텍스트는 현재 타이머, 이동 속도, 누적 accepted hit 수, 대상 노트
ID·표시 종류·상태·시간 차이를 표시합니다. 뒤로 이동하면 이미 소비한 판정 상태와
점수를 초기화한 뒤 해당 시각까지 다시 갱신합니다.
