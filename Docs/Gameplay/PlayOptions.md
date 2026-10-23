# Taiko 플레이 옵션과 로컬 기록

게임용 곡 선택 하단에는 실제 등록 모드를 선택하는 Game mode와 Play options가 있습니다.
현재 등록 모드는 Taiko입니다. 전역 창/장치/키 설정은 기존 Ctrl+O로 엽니다.
에디터용 곡 선택 하단의 기존 전역 설정 버튼은 유지합니다.
플레이 옵션은 현재 앱 실행 동안 곡 선택 Scene에 유지하고, 시작 시
GameplayLaunchRequest → PlaySessionOptions → PlaySession/PlayResultSummary로 복사합니다.
Result의 Retry는 현재 선택 화면 값 대신 방금 플레이한 옵션/시드를 다시 사용합니다.
YMM/YMP/YME 파일은 수정하지 않습니다.

- Scroll Speed는 x 또는 BPM을 선택하고 양수를 입력합니다. BPM은 기본 BPM에
  대한 비율로 전체 스크롤 거리를 조절합니다. Ignore scroll effects는 YME의
  Whole/Separate 스크롤 자동화만 무시합니다. 실제 BPM·음악·노트 판정 시각은 유지합니다.
- Metronome은 살아 있는 마디의 4분음표 박을 재생하며 마디 첫 박을 강조합니다.
  박 위치는 세션 worker에서 미리 컴파일하고 공통 시계로 DSP 예약합니다.
  일시정지 시 예약 클릭을 버리고, 복귀/되감기/재시작 시 커서를 다시 정렬합니다.
- Mirror는 작은 동/캇, 큰 동/큰 캇, 동 버즈/캇 버즈의 색과 입력을 반전합니다.
  보라·롤·풍선·덴덴은 유지합니다. 명시한 커스텀 히트사운드는 유지합니다.
- Random은 0..100%의 **재배치 시도 확률**입니다. 시도한 작은/큰/버즈 노트는
  각각 같은 그룹의 두 종류에서 균등 추첨하므로 같은 색이 나올 수도 있습니다.
  보라는 제외합니다. 롱노트를 짝지은 후 한 번만 추첨합니다. uint64 시드와
  정의된 mt19937_64 draw를 사용하고 Mirror를 먼저, Random을 다음에 적용합니다.
- Accelerate는 0.5..2.0x입니다. 하나의 RhythmTimer가 QPC/차트/DSP 좌표를
  변환하므로 실제 판정 시간은 기본 판정 시간 / 배속입니다. 판정 프로필을 다시
  나누지 않습니다. 입력/결과 그래프는 원본 차트 시간 좌표를 유지합니다.
  배경음악·히트사운드의 음정 유지 여부는 독립적입니다.
  BGM 음정 유지는 로딩 worker의 고정 속도 PCM 변환을 사용합니다. 히트사운드는
  유지 시 원래 샘플 음정/길이로 즉시 재생하고 해제 시 배속만큼 pitch를 조절합니다.
- JudgeLevel은 채보 값을 사용하거나 1.00..100.00, 0.01 간격으로 덮어씁니다.
  일반·Syncopation 판정과 결과/기록의 실제 JudgeLevel 모두 같은 값을 사용합니다.

결과 중앙에는 적용한 옵션의 이름/값과 식별 아이콘을 표시합니다.
RND 값에 시드와 시도 확률을, Accelerate 값에 BGM/HIT 음정 유지 상태를 표시합니다.

## 로컬 기록

실제 패턴의 결과 화면에 진입할 때마다 한 번 저장합니다. 디버그/스모크 결과는
저장하지 않습니다. Pause Retry로 결과 없이 초기화한 중간 시도는 별도 저장하지 않습니다.
완료·연습·중단을 구분하고, 기존 pauseMenuUsed 클리어 자격 규칙을 유지합니다.
정확도는 기존 0.000001% 정수 단위로 보존합니다.

위치: `%LOCALAPPDATA%/YOUMU/Records/plays.sqlite3`.
Windows 내장 SQLite를 사용하며 `user_version=1`로 스키마를 확인합니다.
지원하지 않는 버전이나 손상 데이터는 오류를 표시하고 보존합니다.
캐시와 달리 원본에서 복구 가능한 데이터가 아니므로 자동 삭제/재생성하지 않습니다.
패턴의 정규화된 절대 경로와 모드로 묶습니다. 같은 경로의 편집본은 같은 기록칸을
사용하며 파일을 이동하면 새 경로의 기록칸을 사용합니다.

기록 저장·조회는 worker에서 실행하고, 화면에는 최신순 여섯 개씩 페이지로
표시합니다. 결과 화면에 저장 진행/실패를 표시하며 곡 선택에서 다른 패턴으로
이동한 동안 완료된 이전 쿼리는 게시하지 않습니다.

## 조건식

기록칸의 Filter records에서 입력합니다. 비워두면 선택 패턴의 모든 기록입니다.
예: `JudgeLevel >= 60 AND Random = true`, `Random = true`,
`Accelerate >= 1.5 AND NOT (Mirror = true)`.

필드 이름과 AND/OR/NOT/true/false는 대소문자를 구분하지 않습니다.
괄호, `=`, `==`, `!=`, `<>`, `<`, `<=`, `>`, `>=`를 지원합니다.
불리언과 시드에는 동등/부등 비교만 허용합니다. 값은 SQL parameter로 바인딩하며
임의 SQL, 함수 호출, SELECT, 세미콜론, 알려지지 않은 필드는 거절합니다.

| 필드 | 값 |
| --- | --- |
| JudgeLevel | 채보 기본값 또는 실제 덮어쓴 값 |
| Random, Mirror, Metronome, IgnoreScrollEffects | true/false |
| Accelerate | 0.5..2.0 배속 |
| ScrollSpeed | 입력한 x 또는 BPM 값 |
| ScrollUnit | 0=x, 1=BPM |
| ScrollBPM | BPM 입력 때만 값 존재; x 입력은 NULL |
| RandomProbability | 재배치 시도 확률, 0..100 |
| RandomSeed | 0..18446744073709551615 정수; 정확한 동등 비교 |
| PreserveMusicPitch, PreserveHitSoundPitch | true/false |
| JudgeOverride | 채보 JudgeLevel을 덮어썼는지 |
| Completed, Cleared, PauseMenuUsed | 실제 결과 상태, true/false |

원본 차트 문법이 바뀌어도 기록 옵션 스냅샷과 SQLite 스키마는 독립적입니다.
기록 필드 의미나 레이아웃을 바꾸면 스키마 마이그레이션 정책을 명시해야 합니다.

## 책임 경계

`PlayOptions`는 순수 값/검증, `TaikoNoteTransform`은 모드별 색 추첨,
`TaikoSessionBuilder`는 판정/스크롤/박 컴파일을 맡습니다.
`RhythmTimer`는 배속을 포함한 공통 시간 변환,
`GameplayAudioRouter`는 음정 정책과 clip/bus/예약 재생을 맡습니다.
`ValueDialog`와 `PlayOptionsDialog`는 Client 입력 UI,
`PlayRecordStore`/`RecordCondition`은 SQLite 저장/제한된 조건식,
`SongSelectRecords`는 비동기 페이지 게시와 기록칸 표현을 맡습니다.

BGM PCM 변환은 엔진의 backend-neutral PrepareTimeStretch를 사용하며,
고정한 [Signalsmith Stretch](https://github.com/Signalsmith-Audio/signalsmith-stretch)의
MIT 라이선스와 Linear 라이선스를 EXE 옆 ThirdPartyNotices에 배포합니다.
