# YMP 패턴·노트

코드: `Parsing/Submodules/PatternParser.cpp`, `Model/Submodules/PatternDocument.h`.
노트 ID·표시 계약: `FingerDrum.Modes/Taiko/Submodules/TaikoNoteDefinition.h`.
위치/BPM 변경일 때만 [Timing](Timing.md), 사운드 변경일 때만 [YME](Yme.md)를 읽습니다.

UTF-8 텍스트이며 빈 줄과 `//`로 시작하는 줄을 무시합니다. 경로에는 `\\`와 `/`를 사용할 수 있습니다.

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

### HitSounds

```text
[HitSounds]
CustomHit: Sounds/custom.wav
```

`인덱스: 경로`를 등록하는 영역입니다. 숫자 인덱스(`1: Sounds/pop.wav`)와 기존
문자열 이름을 모두 지원합니다. 경로는 YMP 기준이며 Client가 곡 로드 시 한 번
등록합니다. 같은 파일의 별칭은 하나의 샘플/재생 채널을 공유합니다.
`[Pattern]`의 hitsound 칸에 지정한 표의 인덱스는 아래 동·캇 기본값 변경보다 우선합니다.

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
정확도 계산 및 Debug 표시 계약은 [판정과 정확도](../Gameplay/Rules.md)를 참조합니다.

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
