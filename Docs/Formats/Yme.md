# YME 이펙트·히트사운드

파서/모델: `FingerDrum.Chart/Parsing/Submodules/EffectParser.cpp`,
`Model/Submodules/EffectDocument.h`. 보간/스크롤: `Automation/InterpolationExpression.*`,
`Automation/ScrollAutomation.*`. 저장: `Editing/ChartEditor.cpp`.
검증: `Tests/Rhythm/Submodules/YmeEffectsTests.cpp`, 실제 실행은
[Presentation 테스트](../../Tests/Presentation/README.md)의 `-Effects`입니다.

UTF-8 텍스트입니다. 빈 줄과 `//`로 시작하는 줄은 무시합니다. 형식 버전은 1이며
이전 `[Effects]`, `[HitSound Changes]`, `--`, ms 길이 문법은 사용하지 않습니다.
YMP `[Metadata]`의 `Effect file`로 YME를 지정합니다. **YME 경로는 YMP 기준**,
**히트사운드 파일 경로는 YME 기준**입니다. 같은 이름 YME를 자동 검색하지 않습니다.

## 섹션

| 섹션 | 책임 |
| --- | --- |
| `[Interpolation]` | 사용자 보간식 등록. 기본 데이터입니다. |
| `[HitSounds]` | 히트사운드 인덱스와 파일 표. 기본 데이터입니다. |
| `[Speed]` | `#ScrollSpeed Whole`, `#ScrollSpeed Separate` |
| `[Sounds]` | `#HitSound`, `#Volume` 및 기존 오디오 DSP 명령 |
| `[Zone]` | `#Syncopation`, `#Kiai`, `#MeasureLineVisible` |

효과 명령은 마지막 세 섹션으로만 분류합니다. 정의와 사용의 파일 내 순서는 자유입니다.

## 지점과 구간

```text
Version: 1
[Speed]
2, 0/4, #ScrollSpeed Whole, Value=2
3, 0/4, Area, 5, 0/4, #ScrollSpeed Whole, From=2, To=1, Curve=Linear
```

지점형은 `마디, N/D, #명령 대상, Value=값`입니다. 구간형은 시작 위치 바로 다음에
`Area`를 넣고 끝 위치와 보간을 적습니다:
`시작마디, N/D, Area, 끝마디, N/D, #명령 대상, From=값, To=값, Curve=이름`.
마디는 1부터 세고 `N/D`는 해당 마디 안의 **온음표 기준 위치**입니다.
에디터 박자 디바이더의 4분음표 기준 분수와 구분합니다.

위치는 마디 길이 안에 있어야 하고 구간 끝은 시작보다 뒤여야 합니다.
BPM·delay·Pattern Offset을 반영해 정수 µs로 컴파일합니다. 구간은 컴파일된 시간에서도
양의 길이여야 합니다. `u`는 구간의 실제 경과 시간 비율이며 0~1로 제한합니다.
끝에 도달하면 `To`를 유지합니다. 다음 동일 종류/대상의 지시가 이전 구간을 덮어쓰고,
동일 위치에서는 파일의 마지막 지시가 우선합니다. 새 지시의 `From`이 이전 값과
다르면 그 지점에서 값이 바뀝니다.

## 스크롤

- `Whole`: 현재 시각의 공통 속도 배율입니다. 모든 노트가 함께 가속/감속합니다.
  세션/편집 문서 생성 때 속도를 적분한 누적 거리 표를 만들고 표시 시 거리 차를 조회합니다.
  효과가 바뀌어도 기존 노트 위치가 갑자기 이동하지 않습니다.
- `Separate`: 노트의 판정 시각에 해당하는 배율을 노트마다 고정합니다.
  롱노트는 head의 배율을 body/tail/tick에도 사용합니다. 개별 노트 ID를 지정하지 않습니다.
- 두 배율은 곱합니다. 기본 속도는 Base BPM이며 BPM/delay 변경은 노트의 실제 시간 간격에
  반영됩니다. 효과 없는 delay에서 이동을 멈추지 않습니다. 플레이와 에디터 실시간 뷰는
  같은 계산을 사용합니다. 스크롤 배율은 항상 유한한 양수여야 합니다.

## 보간

`A=From`, `B=To`일 때 기본 보간은 다음 세 가지입니다.

| 이름 | 값 |
| --- | --- |
| `Linear` | `A + (B-A) * u` |
| `Exponential` | `A^(1-u) * B^u` |
| `Harmonic` | `A*B / ((1-u)*B + u*A)` |

지수/조화수열은 양수 끝값을 요구합니다. 볼륨 0까지 내려갈 때는 선형 또는 적합한
사용자 함수를 사용합니다. 사용자 함수는 다음처럼 등록합니다.

```text
[Interpolation]
EaseIn: From + (To - From) * u * u
Soft: From + (To - From) * Bezier(u, 0.25, 0.10, 0.25, 1.00)
[Sounds]
3, 0/4, Area, 4, 0/4, #Volume HitSound, From=0.5, To=1, Curve=Soft
```

변수는 `u`, `From`, `To`; 연산자는 `+ - * / ^`, 괄호와 단항 부호입니다.
`Bezier(u,x1,y1,x2,y2)`는 양 끝 `(0,0)`, `(1,1)`이 고정된 **3차 베지어**입니다.
X축 시간에 해당하는 매개변수를 찾은 뒤 Y값을 반환합니다. X 제어점은 0~1입니다.
기본 세 이름을 재정의할 수 없습니다. 식은 로드/수정 때 한 번 컴파일하고 평가 시
재파싱/할당하지 않습니다. 코드 실행이나 임의 함수 호출은 지원하지 않습니다.
잘못된 식, 정의하지 않은 이름, 비유한 값, 음수 볼륨/0 이하 속도는 오류입니다.
사용자 식은 여러 지점에서 검증하며 Whole 적분에도 작업량 제한을 둡니다.

## 히트사운드와 볼륨

```text
[HitSounds]
1: Sounds/don.wav
2: Sounds/kat.wav
[Sounds]
2, 0/4, #HitSound Don 2
2, 0/4, #HitSound Kat 1
2, 0/4, #Volume HitSound, Value=0.5
3, 0/4, Area, 4, 0/4, #Volume TickSound, From=1, To=0.5, Curve=Harmonic
```

표의 인덱스는 숫자 또는 문자열이며 경로는 YME 폴더 기준입니다. 같은 파일의 별칭은
Sample/Voice를 공유합니다. YMP 노트의 HitSound 칸도 이 표의 인덱스를 참조합니다.
명시적인 노트 인덱스가 YME의 시각별 Don/Kat 기본값보다 우선합니다.
변경은 다음 지점까지 유지하며 빈 입력·큰 노트·보라 노트·연타·Buzz에 기존 사운드 정책대로
전달합니다. 풍선 파열음은 별도의 스킨 cue입니다. `#HitSound`는 지점형만 지원합니다.

**`#Volume`은 노래 볼륨이 아닌 히트사운드 볼륨**입니다. 값 1은 원래 크기, 0은 무음입니다.
대상은 `HitSound`(판정 입력음), `TickSound`(롱노트/연타 틱음),
`UserInputFeedback`(빈 입력음)입니다. `Music`과 `UI`는 볼륨 대상으로 허용하지 않습니다.
음악은 기존대로 인게임 0ms에 DSP 예약합니다.

기존 `#ReverbSend`, `#LowPassCutoff`, `#HighPassCutoff`도 Sounds에서 지점/구간 수치
문법을 사용합니다. 이 기존 DSP 명령의 대상은 오디오 버스 이름입니다.
향후 distortion 등을 넣을 장소는 Sounds이며 현재 새 DSP 효과는 추가하지 않았습니다.

## 싱코페이션과 Kiai

```text
[Zone]
3, 0/4, #Syncopation ON, Exclude=1|2|4
5, 0/4, #Syncopation OFF
3, 0/4, #Kiai ON
7, 0/4, #Kiai OFF
```

ON/OFF는 지점만 지정합니다. ON 위치부터 OFF 위치 직전까지 활성 상태입니다.
싱코페이션 대상 노트는 기본 객체와 별도인 공유 `AccuracyRange`를 참조합니다.
JudgeLevel 배율을 적용한 **모든 판정 등급의 좌우 범위에 각각 10ms**를 더합니다.
레벨 100에서도 추가량은 10ms입니다. 노트마다 객체를 복제하지 않습니다.
롱노트의 적용 여부는 head의 위치로 결정하고 그 객체를 전체 규칙에서 사용합니다.

`Exclude`는 선택 사항이며 4분음표를 나누는 정수 분모(1~1024)를 `|`로 구분합니다.
1/2/4는 각각 4분/8분/16분음표 격자입니다. 해당 마디 시작을 기준으로 어느 지정 격자에든
정확히 놓인 노트는 보정하지 않습니다. 유리수로 검사하며 ms 근사 오차를 쓰지 않습니다.
생략하면 활성 구간 내 모든 노트가 대상입니다.

Kiai는 `PlaySession::IsKiaiActive`로 상태만 조회하며 표시·점수·오디오를 변경하지 않습니다.
기존 마디선은 `#MeasureLineVisible, Value=0` 또는 `Value=1`로 지정합니다.

## 테스트 채보

`Assets/Songs/Pattern/angeldream/angeldream [effects test].ymp`가
`Effects/angeldream-effects.yme`를 명시적으로 참조합니다. 기존 실제 음원/YMM을 사용하며
15개 논리 노트, BPM 변경, Whole/Separate, 세 기본 보간·사용자 식·베지어,
히트사운드 교체/볼륨, 싱코페이션 제외와 Kiai 상태를 검사합니다.
YME와 YMP의 폴더를 달리해 히트사운드 기준 경로도 검사합니다.
