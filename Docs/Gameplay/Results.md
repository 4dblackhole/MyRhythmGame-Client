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
히스토그램은 오차를 가까운 정수 ms로 대칭 반올림하여 1ms 구간별 입력 개수를 표시합니다.
같은 구간의 실제 판정들을 산점도와 같은 색상으로 MAX→BAD 순서로 아래부터 쌓습니다.
막대 전체 높이와 최고 빈도는 해당 구간의 총 입력 수이며, 표시 범위를 넓혀도 1ms 간격을 유지합니다.
입력이 있는 구간만 집계하여 오차 범위에 비례하는 배열을 할당하지 않습니다.
축소 화면에서는 집계 간격을 유지하고 막대의 그리기 폭만 최소 한 픽셀로 제한합니다.
범위는 최소 ±100ms이고 실제 입력 오차를 포함하도록 확장합니다.
두 그래프는 한 RectangleBatch로 그리고 진입·언어/해상도 변경 시에만 재구성합니다.

## Penpot 표시와 책임 경계

[Result · Sky](../DesignReferences.md)의 1920×1080 배치를 사용합니다.
상단 88px의 짙은 배너에는 모드·곡명·아티스트·난이도를 표시합니다.
왼쪽은 HP/점수/정확도/판정/그룹 통계, 가운데는 중앙 정렬한 랭크 장식과 제작자,
오른쪽 528px 열에는 히스토그램·시간별 오차와 두 256px 액션 버튼을 배치합니다.
화면이 좁으면 내용 전체를 축소하고 BACK은 화면 왼쪽 아래에 유지합니다.
하단 구름은 하늘 배경 위, 결과 통계와 액션 버튼 뒤에 그려 콘텐츠를 가리지 않습니다.
한국어·영어 문구는 ResultTexts가 소유합니다.

곡명·아티스트·첫 제작자명은 곡 선택의 GameplayLaunchRequest 값 snapshot에 보관합니다.
결과 화면에서 카탈로그/채보 파일을 다시 읽지 않습니다. 패턴명과 통계는 IPlayResult에서 읽습니다.
SCORE는 아직 계산 규칙이 없으므로 `—`, HP는 데이터 없음/추가 예정으로 표시합니다.
RANK는 아래 정확도 기준의 스킨 이미지를 표시합니다.
시안의 빈 자리를 채우기 위한 임의 점수·기록·HP 데이터는 생성하지 않습니다.

- ResultScene은 결과의 소유권·화면 수명·전환만 담당합니다. View는 Back/Retry 액션을 반환합니다.
- YOUMUGame은 선택 snapshot을 캡처한 retry callback을 조립합니다. 이전 세션의
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
2026-10-15의 배너·열 너비 갱신은 위 기능으로 표현할 수 있어 추가 엔진 변경이 없습니다.
배너 색은 공통 UiColors의 result::Banner가 소유하고 드럼 PNG는 갱신된 Penpot 원본을 사용합니다.

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

결과 데이터는 메모리에서 수집하며 결과 화면 진입 시 실제 시도의 요약과 플레이 옵션을
로컬 SQLite에 저장합니다. 곡 선택의 조건식 조회와 옵션 아이콘은
[PlayOptions](PlayOptions.md)을 따릅니다. 리플레이 재생은 구현하지 않았습니다.

리절트 PNG와 버튼 이미지는 스킨 `[Result]`의 요소 키와 폴더 상대 경로를 사용합니다.
파일별 기본본 대체와 버튼 상태 이미지 규칙은 [skin.ini](../BuiltInAssets.md#skinini와-씬별-이미지)를 따릅니다.

## 정확도 랭크

최종 전체 정확도의 고정소수점 정수 값을 비교하며 표시 문자열의 반올림은 사용하지 않습니다.
각 하한을 포함하고 다음 랭크 하한은 포함하지 않습니다. 100.000000%만 Y이며,
99.999999%는 PERFECT입니다. 클리어/일시정지 자격과는 별개인 정확도 표시입니다.

| 정확도 | 랭크 / `[Result]` 이미지 키 | 기본 PNG |
| --- | --- | --- |
| 100% | Y / `Rank.Y` | `Rank/Y.png` |
| 99% 이상 | PERFECT / `Rank.PERFECT` | `Rank/PERFECT.png` |
| 98% 이상 | SSS / `Rank.SSS` | `Rank/SSS.png` |
| 96% 이상 | SS / `Rank.SS` | `Rank/SS.png` |
| 93% 이상 | S / `Rank.S` | `Rank/S.png` |
| 90% 이상 | A / `Rank.A` | `Rank/A.png` |
| 85% 이상 | B / `Rank.B` | `Rank/B.png` |
| 80% 이상 | C / `Rank.C` | `Rank/C.png` |
| 75% 이상 | D / `Rank.D` | `Rank/D.png` |
| 70% 이상 | E / `Rank.E` | `Rank/E.png` |
| 70% 미만 | F / `Rank.F` | `Rank/F.png` |

TaikoResultView의 RankImageKey가 한 표에서 키를 선택하고 LoadAssets에서 해당 이미지 하나를
등록합니다. BuildMetadata는 기존 중앙 랭크 프레임 내부에 원본 비율로 맞춰 표시합니다.
Scene, 결과 수집기, 정확도 산식과 저장 데이터는 바꾸지 않습니다. 부분 스킨에서 빠진
랭크 이미지는 기존 SkinManifest 규칙에 따라 해당 내장 기본본으로 대체합니다.

### 랭크 PNG 제작

11개 모두 built-in image_gen으로 제작한 1774×887 투명 PNG입니다. 원본은
`YOUMU.Assets/Assets/Skins/Default Skin/Result/Rank`에 보관하고 기존 내장 자산
wildcard와 같은 Result/Rank 프로젝트 필터를 사용합니다. Y를 먼저 생성한 뒤 각 문구를
Y의 스타일을 유지하는 개별 편집으로 제작했습니다. 이미지의 문구/투명 배경은 직접 확인했고,
PNG 크기·알파와 실제 결과 화면의 자동 검증은 Verification에 별도로 기록합니다.

생성 프롬프트(Y):

```text
Use case: logo-brand. Asset type: transparent PNG rhythm-game result rank emblem.
Create exactly one rank emblem with the exact capital text "Y", and nothing else.
Crisp bold condensed slightly italic geometric display lettering. Flat polished 2D
game UI artwork, sky-blue face (#80C9EB), deep blue (#285374) outline and a thin
white inner highlight, matching a clean pale sky-blue rhythm game UI with white
circular rank frame. Strong readable silhouette when rendered at about 160 pixels
tall. No surrounding badge, circle, wings, stars, background, scene, icon, percentage
or extra words. Text must be precisely Y. Center the lettering on a landscape 2:1
transparent canvas, generous transparent padding, full lettering entirely inside
the canvas. Genuine alpha transparency. Do not include a checkerboard pattern or
black rectangle. This is the first of a matching rank family whose texts are
Y, PERFECT, SSS, SS, S, A, B, C, D, F; produce ONLY Y in this image.
```

편집 프롬프트 집합: 아래 `{rank}`에 `PERFECT`, `SSS`, `SS`, `S`, `A`, `B`, `C`,
`D`, `E`, `F`를 각각 넣어 별도 호출했습니다. 참조 이미지는 첫 Y PNG이며 투명 배경을 유지했습니다.

```text
Use case: text-localization. Asset type: transparent PNG rhythm-game result rank
emblem, one matching variant of the reference Y.
Edit the reference rank emblem: replace the letter Y with exactly the capital
text "{rank}". Preserve the same sky-blue glossy face, deep blue outline, thin white
bevel highlight, bold condensed slightly italic geometric lettering, clean readable
game UI style, transparent landscape 2:1 canvas and genuine alpha transparency.
Center the new lettering and scale it as needed so the whole exact text fits with
generous transparent padding; preserve letter proportions and legibility. The exact
text is "{rank}" and no other text. No Y should remain. Do not add a badge, frame,
circle, wings, stars, icons, percentage, shadow rectangle, scene, background or
watermark. Output only this one emblem as a standalone transparent PNG.
```
