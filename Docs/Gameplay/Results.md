# 플레이 결과와 입력 기록

모든 논리 노트가 완료/MISS로 확정된 뒤 3초가 지나면 Result로 이동합니다.
ESC는 [일시정지 메뉴](Pause.md)를 엽니다. Retire 선택은 남은 노트를 MISS로 확정하고
즉시 중도 종료 결과로 이동합니다. Result의 BACK·ESC·Enter는 곡 선택으로 돌아갑니다.
리트라이는 방금 플레이한 패턴·효과·음원 경로로 새 세션을 생성합니다.
리플레이 버튼은 비활성 표시이며 입력 로그의 재생 기능을 제공하지 않습니다.

## 수집과 소유권

- `PlaySession`은 모드가 만든 `IPlayResultCollector`에 판정 입력과 의미 이벤트를 전달합니다.
- `TaikoSessionBuilder`는 실제 생성된 논리 노트만 `TaikoResultCollector`에 등록합니다.
  롱노트 테일, 음수 delay로 제외된 노트, 무시된 겹침 행은 별도 점수 항목이 아닙니다.
- `TaikoResultCollector`는 판정·오차·완료 점수를 수집하고 종료 시 그룹/분산을 계산합니다.
  `IPlayResult`의 공통 요약과 `TaikoPlayResult`의 모드별 데이터를 반환합니다.
- `RhythmTestScene`이 결과 `unique_ptr`를 루트의 대기 슬롯으로 이동합니다.
  미리 등록된 Result factory가 슬롯을 소비하여 새 `ResultScene`을 생성합니다.
  gameplay와 Result 모두 `DestroyOnExit`이며 다음 update에 전환을 적용합니다.
- `ResultScene`은 결과 데이터를 소유하고 `IResultView`를 조립합니다. 모드 선택은
  `ResultViewFactory`, 현재 Taiko 표시/Canvas는 `TaikoResultView`가 소유합니다.
  결과는 노트·세션·오디오 객체의 포인터를 보관하지 않습니다. View는 Scene 데이터만
  읽으며 View/Canvas가 데이터보다 먼저 정리됩니다.

다른 모드는 자신의 수집기·결과·View를 구현하고 모드/결과 factory에 연결합니다.
공통 Scene에 Taiko 판정 항목이나 그래프 규칙을 추가할 필요가 없습니다.

## 점수와 판정 개수

점수 정밀도는 [판정 규칙](Rules.md)의 AccuracyScore가 기준입니다.
최종 정확도는 **전체 유효 논리 노트 점수의 합 / 전체 유효 논리 노트 수**입니다.
Retire의 남은 노트와 진행 중 롱노트는 0점·실제 MISS로 집계하여 미처리가 남지 않습니다.
이미 확정된 롱노트의 부분 점수는 기존 규칙대로 보존합니다. 수집기에 별도로 미처리 상태의
종료를 요청한 경우에는 해당 노트를 0점·미처리로 구분하며 가상의 MISS를 만들지 않습니다.
노트가 없는 그룹은 화면에 `— %`를 표시합니다.

Taiko는 일반(Don/Kat), 큰(BigDon/BigKat/Purple), 롤 계열의 정확도를 각각 표시합니다.
롤 계열에는 Roll/BigRoll/TickRoll/BigTickRoll/Balloon/DengDeng/Buzz가 포함됩니다.
각 노트의 정확도·종류·목표와 수락된 타격/틱은 결과 데이터에도 남습니다.
그룹 평균끼리 평균내지 않고 각 논리 노트에 같은 가중치를 줍니다.

- MAX/PERFECT/GREAT/GOOD/BAD: 실제 `HitAccepted`의 타이밍 판정 개수입니다.
  큰/보라 노트의 두 입력은 각각 셉니다. Buzz의 `HoldStarted`는 중복 집계하지 않습니다.
- OK: 롤 계열이 요구 타격/틱을 모두 채운 성공 완료입니다.
- MISS: 실패로 확정된 논리 노트입니다. 틱롤의 완료 이벤트에도 실패 틱이 있으면 MISS입니다.
- POOR: Good 이내에서 거부된 **Pressed** 입력입니다. 빈 입력과 Release는 포함하지 않습니다.
- 유지/틱롤의 성공·실패 틱은 별도 집계합니다. 자유 연타 입력은 타이밍 판정으로 세지 않습니다.

따라서 판정 카운트의 합이 논리 노트 수와 항상 같지는 않습니다.

## 오차와 그래프

타이밍 판정 입력마다 노트 ID·타격 인덱스·입력 시각·부호 있는 오차·등급·점수를 남깁니다.
자동 MISS, 미처리, 자유 연타, 판정 없는 틱에 가상의 오차를 만들지 않습니다.
모집단 분산(ms²), 평균/표준편차(ms)를 Welford 방식으로 계산합니다.
타이밍 입력이 없으면 통계/그래프에 명시적인 빈 상태를 표시합니다.

시간별 오차 산점도의 가로축은 세션 시간(s), 세로축은 오차(ms)입니다.
양수는 늦은 입력, 음수는 빠른 입력이며 점 색은 키빔의 등급 색입니다.
히스토그램은 같은 오차 범위를 41개 구간으로 나눈 입력 개수이며 최고 개수를 표시합니다.
범위는 최소 ±100ms이고 실제 입력 오차를 포함하도록 확장합니다.
두 그래프는 한 RectangleBatch로 그리고 진입·언어/해상도 변경 시에만 재구성합니다.

## Penpot 표시와 책임 경계

[Result · Sky](../DesignReferences.md)의 1920×1080 배치를 사용합니다.
왼쪽은 HP/점수/정확도/판정/그룹 통계, 가운데는 랭크 장식과 곡·패턴·제작자,
오른쪽은 히스토그램·시간별 오차와 액션 버튼입니다. 화면이 좁으면 내용 전체를
축소하고 BACK은 화면 왼쪽 아래에 유지합니다. 한국어·영어 문구는 ResultTexts가 소유합니다.

곡명과 첫 제작자명은 곡 선택의 GameplayLaunchRequest 값 snapshot에 보관합니다.
결과 화면에서 카탈로그/채보 파일을 다시 읽지 않습니다. 패턴명과 통계는 IPlayResult에서 읽습니다.
SCORE·RANK는 아직 계산 규칙이 없으므로 `—`, HP는 데이터 없음/추가 예정으로 표시합니다.
시안의 빈 자리를 채우기 위한 임의 점수·기록·HP 데이터는 생성하지 않습니다.

- ResultScene은 결과의 소유권·화면 수명·전환만 담당합니다. View는 Back/Retry 액션을 반환합니다.
- FingerDrumGame은 선택 snapshot을 캡처한 retry callback을 조립합니다. 이전 세션의
  노트·타이머·음성을 재사용하지 않고 DestroyOnExit route에서 새 플레이 객체를 생성합니다.
- TaikoResultView는 Canvas·입력·스킨/글꼴을 소유합니다. Panels는 통계/메타데이터/버튼,
  GraphView는 그래프 장식/축/범례, Graphs는 실제 입력에서 범위/빈/점 좌표를 도출합니다.
- TaikoResultLayout은 좌표계·색상·패널 위치를 소유하고 모드의 점수/판정 규칙을 변경하지 않습니다.

디자인 대응에 필요한 엔진 기능을 먼저 점검했습니다.

| 표현 | 엔진 기능과 조치 |
| --- | --- |
| 1920×1080 배치·화면 축소·레이어·버튼 입력 | 기존 Canvas/Transform/Z-order/입력 router 재사용 |
| 투명 배경·구름·드럼·링 장식 PNG | 기존 이미지 등록·Sprite·tint 재사용 |
| 둥근 패널·테두리 | 기존 corner radius의 안쪽/바깥쪽 사각형 조합 |
| 실제 입력 수가 많은 그래프 | 기존 RectangleBatch, 노드 수는 샘플 수와 무관 |
| Noto Sans KR 500/600/700/800 굵기 | 엔진 TextStyle/TextContent/Visual2D의 fontWeight 추가 |

글꼴 굵기는 DirectWrite layout cache key·유지 payload·측정 무효화에 포함됩니다.
기본값은 기존과 같은 400이며 1..999 범위를 사용합니다. 리절트만 라이선스와 함께 내장한
Noto Sans KR 글꼴을 사용합니다. 새 PNG의 경로·제작 크기는 [BuiltInAssets](../BuiltInAssets.md)에 있습니다.

## 입력 로그와 종료 시각

판정에 전달된 매핑된 게임 키의 Pressed/Released를 `RhythmInputEvent` 값으로 기록합니다.
QPC timestamp를 현재 세션의 단일 RhythmTimer로 변환한 **부호 있는 정수 us**를 사용합니다.
물리 키·의미 action·edge를 보존하고 같은 us의 입력은 저장 순서로 구분합니다.
빈 입력/오입력도 로그에 남습니다. 실제 바인딩, 판정 레벨, 패턴 경로와 시작/종료 시각을
공통 결과 요약에 보관합니다. Retry/R/디버그 Reset은 이전 시도의 집계와 입력 로그를 지웁니다.
Resume/Rewind는 전체 입력과 제어 마커를 유지합니다. Rewind가 취소한 판정은 결과 통계에서 제외합니다.
HP/클리어 자격 및 미래 리플레이의 마커 계약은 [Pause](Pause.md)가 기준입니다.

ESC가 포함된 프레임은 ESC의 QPC 시각까지만 판정하고 이후 입력을 제외합니다.
held 상태도 ESC 뒤의 edge를 되돌려 구하므로 뒤늦은 키 해제로 Buzz 틱이 바뀌지 않습니다.
메뉴를 여는 순간의 노트 상태를 유지하며 Retire를 선택할 때에만 미처리를 MISS로 확정합니다.

이번 범위는 메모리 수집과 Result 표시입니다. 디스크 저장·리플레이 재생·곡 선택 기록
표시는 아직 제공하지 않습니다. 검증은 [Verification](../Verification.md)을 참고합니다.
