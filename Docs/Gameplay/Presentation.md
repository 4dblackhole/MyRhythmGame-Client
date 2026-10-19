# 플레이 화면·노트 표현

`Client/Presentation/LaneKeyBeam`은 Lane 자식 `Lane.KeyBeam` 스프라이트와
200ms 선형 알파 페이드를 관리합니다. Scene은 등록된 게임 키의 Pressed 입력을
판정한 뒤 `OnKeyPressed(result)`를 전달합니다. 빈 입력/범위 밖 입력 및
정확도 없는 Roll tick은 흰색, 판정 결과는 RPG `AccuracyRange::DefaultAccInfo`의
흰색·하늘색·초록·노랑·보라와 점수 기반 보간, Good 이내 거부된 입력은 적색입니다.
수동 입력 결과만 빔을 시작하므로 자동 Miss나 키 해제는 빔을 생성하지 않습니다.
재입력은 같은 스프라이트의 색상과 알파를 초기화하고 200ms 후 숨깁니다.
페이드는 표시용 deltaSeconds로 진행되며 판정 타이머를 변경하지 않습니다.
`InGame/LaneLight.png`는 Lane 폭에 맞춰 비율을 유지하고 로컬 Y=0에서 시작하며,
Lane 끝에서 clip됩니다. PNG 원본 알파에 1→0의 표시 알파를 곱합니다.
Scene 종료 전에 빔의 observer를 해제하고 Canvas가 노드를 파기합니다.

`RhythmTestScene`은 Lobby가 선택한 YMP/YME를 `TaikoMode`에 전달해 세션을
만듭니다. `DestroyOnExit`이므로 매 플레이마다 새로 생성되고 Escape 또는 패턴
완료 3초 뒤 Result로 이동할 때 Canvas, audio voice, session과 함께 삭제됩니다.
Result로는 값 데이터만 이동합니다. 통계/그래프와 소유권은 [플레이 결과](Results.md)에 있습니다.
`GameplaySessionController::ErrorMessage`는 세션 자동화 오류 또는 오디오 오류를 전달하며,
`GameplayPresenter::PresentErrorMessage`는 공통 HUD에 표시하고 오류가 비면 숨깁니다.
사용자 보간식의 트랙별 정상값 유지/복구 규칙은 [YME](../Formats/Yme.md)의 보간 절을 따릅니다.

플레이 화면의 기준 시안과 런타임 PNG 자산 원본은 Penpot의
[Gameplay · Sky](https://design.penpot.app/#/workspace?team-id=3be9e5e1-190f-8090-8008-7464803e8b40&file-id=3be9e5e1-190f-8090-8008-7468d9cc1bc8&page-id=618d0170-ff55-8025-8008-80646ceaa9d9)에 있습니다.
기존 곡 선택 화면의 Sky 색상과 타이포그래피를 유지하면서 짙은 밤하늘색 Lane,
ScrollGear 프레임, 마디선, 입력 키, 키빔, 롱노트 tick과 롤/큰 롤/Balloon/DengDeng 남은
횟수 표시를 한 화면 계층으로 구성합니다.

입력 패널의 네 키는 Penpot 원본 좌표를 2/3로 축소해 배치합니다. 판정원 중심은
입력 패널 오른쪽에서 Lane 높이의 절반만큼 떨어져 있어, 중심에서 입력 패널과
Lane 위·아래까지의 거리가 같습니다. Lane과 ScrollGear 표면은 오른쪽 논리 화면
끝까지 이어지고 마지막 `Lane.png` 타일만 UV로 잘립니다.
기어의 바깥 테두리 하단은 화면 중앙 가로선(Y=0)에 맞춥니다. `ScrollGear` 부모 아래에
테두리·배경·`ScrollGear.Lane`·`InputPresentation`을 두고, 기어 전체의 이동은 이 부모의
Transform 한 곳에서 지정합니다. Lane 아래의 노트·판정원·키빔과 입력 패널 아래의 키
오버레이도 이 변환을 상속합니다. 선택 스킨의 Lane 폭과 입력 패널 높이 중 큰 값에
테두리 여백을 반영해 부모 높이를 계산합니다. 세로 고정 Canvas이므로 16:9의 해상도
변경이나 다른 화면비에서도 같은 중앙선 기준을 유지합니다. 화면 배경·진행바·정확도와
화면 하단 타이밍 인디케이터는 기어 밖의 HUD로 유지합니다.

## 인게임 World 부모·자식 관계

아래는 실제 표시 노드의 Transform 계층입니다. `*`는 여러 개, 괄호는 노트 종류나
표시 상태에 따라 생성되는 요소입니다. Canvas의 비표시 앵커는 생략했습니다.

```text
플레이 Canvas 루트 (화면 중앙 원점, +Y 위쪽)
├─ Background                         화면 배경
├─ ScrollGear                         기어 전체의 위치·회전·배율
│  ├─ ScrollGear.Border               기어 테두리
│  ├─ ScrollGear.Surface              기어 바탕
│  ├─ ScrollGear.Lane                 레인 로컬 +Y, Taiko에서 -90° 회전
│  │  ├─ Lane.SurfaceTile *           레인 배경 타일
│  │  ├─ Lane.KeyBeam                 입력 키빔
│  │  ├─ Lane.JudgementCircle         판정원
│  │  │  └─ Lane.Judgement.{등급}      MAX/PERFECT/GREAT/GOOD/BAD/MISS/POOR/OK 링·문구
│  │  ├─ Lane.MeasureLine *           마디선
│  │  └─ Lane.Notes                   노트 사이의 앞뒤 순서를 격리하는 레이어
│  │     └─ Lane.Note.{id} *          노트별 표시 루트
│  │        ├─ AmbientHead           헤드
│  │        ├─ UntintedOverlay        헤드 무채색 덧그림 (일반 이미지)
│  │        ├─ AmbientBody / BodyOverlay 롱노트 몸통 (해당 종류)
│  │        ├─ AmbientTail / TailOverlay 롱노트 테일 (해당 종류)
│  │        └─ LongNote.Tick *        현재 보이는 미처리 틱 (해당 종류)
│  └─ InputPresentation
│     └─ Input.Panel                  키 오버레이 바탕
│        ├─ Input.Key.Glow ×4         강/약 키 점등
│        ├─ Input.Key.Face ×4         키 이미지
│        └─ Input.Key.PressFlash ×4   누를 때 섬광
├─ Hud.GameProgress                   상단 진행바 바탕
│  ├─ Progress.Track / Progress.Fill / Progress.Marker
│  └─ Progress.CurrentTime / Progress.TotalTime
├─ Hud.Accuracy                       정확도 텍스트
├─ Hud.Error                          오류 메시지
├─ Hud.JudgementGuide                 화면 하단 타이밍 인디케이터
│  ├─ Judgement.Margin / Judgement.Band.0~4
│  ├─ Judgement.Caption.0~2           EARLY / 0 / LATE
│  ├─ Judgement.Zero                  0 기준선
│  └─ Judgement.Marker
│     └─ Judgement.MarkerFill
├─ Hud.NoteProcessing.{id} *          중앙 풍선/뎅뎅 처리 이미지
├─ Hud.NoteSuccess.{id} *             중앙 풍선 완료 이미지
├─ Hud.NoteCounter.{id} *             롤·큰 롤·풍선·뎅뎅의 남은 타격 수 바탕
│  └─ RemainingHits                   남은 타격 수 텍스트
└─ Debug.NoteState                    Debug 구성의 상태 텍스트

일시정지 Canvas (별도 화면 레이어 10)
└─ Center 앵커 → Pause.Root
   ├─ Pause.Dim                       화면 어둡게 덮기
   └─ Pause.Panel                     메뉴 컨테이너
      ├─ Pause.SkinPanel              사용자 스킨의 전체 패널 (지정 시)
      └─ Pause.Row.0~3                각 행의 테두리 또는 사용자 스킨
         ├─ Pause.RowFill             선택/일반 상태 바탕
         ├─ Pause.0~3                 Resume / Rewind / Retry / Retire 실행 영역
         │  └─ Pause.Label            액션 이름
         └─ Pause.Selector            Resume/Rewind 값 선택기
            ├─ Pause.SelectorFill
            ├─ Pause.{0/1}.Previous/Next
            │  └─ Pause.Arrow → Pause.ArrowStroke ×2
            └─ Pause.Label            현재 값
```

`ScrollGear`의 Transform을 바꾸면 그 아래의 모든 표시 요소가 함께 이동합니다.
레인의 회전은 레인 하위에만 적용되므로 입력 키 이미지는 같은 기어 아래에서도 바로 서 있습니다.
판정 링·문구는 판정원을 따라가되 Taiko의 레인 회전을 반대로 보정하여 글자를 세웁니다.
중앙 풍선/뎅뎅 처리 이미지와 하단 타이밍 인디케이터·일시정지 메뉴는 화면 기준 UI입니다.

Canvas가 표시 노드를 소유하고 `GameplayPresenter`는 비소유 포인터만 보관합니다.
`Shutdown`은 포인터를 해제한 뒤 소유 Canvas handle을 반납합니다. 판정용 노트·타이밍·점수는
`PlaySession` 데이터이며 World 계층에는 해당 노트의 표시 노드만 들어갑니다.

타이밍 마커는 최근 32개의 실제 타격을 원형 큐로 유지합니다. 33번째부터 밀려난 타격은
별도의 최대 8개 순환 슬롯에서 0.2초 동안 페이드아웃하고 삭제됩니다. 노드/이미지는 초기화 시
한 번 만들고 재사용하며 자동 MISS·빈 입력·타이밍 없는 연타는 기록을 만들지 않습니다.
resize에서 유지/페이드 마커 모두 같은 시간→좌표 변환으로 갱신합니다.
위 계층의 Marker는 실제로 32개, RetiringMarker는 8개이며 각자의 MarkerFill을 가집니다.
키 오버레이의 누름 섬광도 기존 강/약 빛과 동일한 동 적색·캇 청색 Ambient를 사용합니다.
레인 판정 키빔의 판정별 색상 정책은 유지합니다.

YMP의 BG/offset은 선택한 Songs/BG 이미지와 화면 중심의 +Y 위쪽 좌표입니다.
기존 Background 노드에 이미지를 연결하고 비율을 유지한 cover 크기로 표시합니다.
오프셋을 반영한 상태에서도 화면 밖까지 덮도록 resize 때 배율을 갱신합니다.
상세 문법과 선택 필드의 실패 처리는 [YMP](../Formats/Ymp.md)를 참고합니다.

## 노트 스크롤과 자산

노트가 처음 표시되는 위치는 고정 픽셀 거리가 아니라 현재 Canvas 논리 폭으로
정해지는 Lane 길이와 가장 큰 노트 head의 반지름에서 계산합니다. Taiko의 회전된
Lane에서는 head 전체가 화면 오른쪽 경계 밖에 있을 때부터 왼쪽 판정점으로
이동합니다. Base BPM에서 16분음표 중심 간격은 기본 노트 이미지 지름의 85%입니다.
별도 스크롤 배율 효과가 없으면 이동 속도는 Base BPM을 기준으로 곡 전체에서
일정하며, BPM 변경이나 delay가 반영된 노트 시각 차이에 따라 화면상 간격이
달라집니다. YME `Separate`는 노트/head의 시각으로 정한 고정 배율이며,
`Whole`은 현재 시각의 공통 속도 배율을 적분한 거리로 표시합니다. 둘을 함께 반영하고
Whole 변경 시 노트가 순간적으로 이동하지 않습니다. 에디터 실시간 뷰도 같은 계산입니다.
플레이 마디선도 시각과 Separate 배율을 가진 스크롤 오브젝트입니다.
TaikoSessionBuilder가 표시 마디 시작 위치에서 배율을 고정하고, GameplayPresenter의
`LaneObjectPosition`이 노트 헤드·마디선·틱의 레인 좌표를 같은 거리 계산으로 구합니다.
마디선과 노트는 동일한 Lane 부모와 중심 pivot을 사용하며 Whole/Separate를 함께 반영합니다.
음수 delay로 잘린 마디는 살아남은 시작 위치를 사용합니다. 느린 마디선도 표시 시간 범위에
포함하므로 화면에 진입한 뒤 늦게 생성되지 않습니다. 마디선에는 노트 판정/오디오 책임을 부여하지 않습니다.
판정과 오디오 예약은 기존의 정수 마이크로초 시각을 계속 사용합니다. 창 크기가 바뀌어도
노트와 롱노트 tick 간격은 유지되며, 출현 위치와 표시 범위만 새 Lane 길이에 맞춥니다.
시작 대기 시간은 최소 2초이고, 낮은 BPM이나 넓은 화면에서 첫 노트가 화면 밖에서
들어올 시간이 더 필요하면 그만큼 늘어납니다.

노트와 플레이 HUD의 배포 기본본은
`FingerDrum.Assets/Assets/Skins/Default Skin/InGame`의 PNG입니다. 실행 파일 옆
스킨에서 누락된 파일만 이 내장 기본본으로 대체합니다.
`note.png`, `bignote.png`, `LNBody.png`, `LNTail.png`에는 Don/Kat/Roll Ambient
색상을 곱하고 대응하는 `*Overlay.png`는 흰색 원본으로 위에 그립니다. 일반/큰
롱노트는 서로 다른 body와 tail을 사용하고 Buzz tick은 마름모 자산을 사용합니다.
tail과 tail overlay는 별도로 회전하지 않고, 둥근 면이 위를 향하는 원본 방향
그대로 Lane 부모의 회전만 상속합니다.
기본 히트사운드는 `don.wav`, `kat.wav`, `bigdon.wav`, `bigkat.wav`이며 풍선 완료음은
같은 `HitSounds/TaikoMode` 폴더의 `pop.wav`입니다.
음악은 StreamAsync로 준비한 뒤 하나의 `RhythmTimer`가 가리키는 DSP 시각 0에 예약합니다.
Raw Input으로 발생한 히트사운드는 입력 timestamp를 판정에만 사용하고 즉시
재생합니다. 따라서 QPC와 장치 DSP clock의 장시간 미세 드리프트가 실시간 입력음을
미래 시각으로 예약해 지연을 누적시키지 않습니다.
오디오 초기화·파일 등록·재생 요청이 실패하면 가능한 나머지 파일 등록은 계속하고,
게임플레이 화면 상단에 첫 오류를 표시합니다. 오류는 무음으로 삼키지 않으며 노트
판정 시간축은 오디오 오류와 독립적으로 유지됩니다.

`GameplayPresenter`는 누적 Whole 거리에서 객체별 가시 구간을 한 번 인덱싱합니다.
Separate가 느린 미래 노트 하나가 다른 화면 밖 노트들의 표시 범위를 확대하지 않습니다.
노트/마디선/틱은 같은 거리 계산으로 조회하고, 노트 수명 정리는 표시 세대 번호를 사용합니다.
창 크기나 세션 presentation revision 변경 시 인덱스를 재구성합니다. Scene의 Render가 표시를
갱신하며 Update는 입력·세션·피드백을 처리합니다. 초기 대기 시각은 첫 실제 가시 구간의
진입 거리를 역산하되 최소 2초를 유지합니다.

Scene 진입에서 `PrepareImages`로 선택 스킨과 fallback PNG를 비동기 디코드합니다.
준비가 끝난 뒤 Canvas를 만들고 오디오 자원을 준비합니다. 오디오 준비가 끝나기 전에
플레이 clock을 시작하지 않습니다. 상세 병목/검증은 [성능 수정 기록](../Reviews/PerformanceFixes-2026-10-14.md)에 있습니다.

Lane 표시는 RPG `PlayScene`의 Transform 계층도 유지합니다. 기본 Lane은 로컬
`+Y` 방향으로 아래에서 위로 진행하는 세로 노드이며, Taiko 화면에서는 이 부모
노드 하나만 Z축 `-90°` 회전합니다. 어두운 단색 Lane, 마디선, key beam,
`JudgeLine`, 노트 head/overlay와 롱노트 body/tail/tick은 모두 Lane의 자식이므로
회전을 함께 상속합니다. 업데이트는 화면 X가 아니라 Lane 로컬 Y만 변경합니다.

`InGame` PNG는 1920×1080 제작 크기이므로 1280×720 논리 Canvas에서는 2/3
배율로 표시합니다. `Lane.png`는 원본 비율을 유지한 채 로컬 Y 방향으로 반복하고
마지막 조각만 UV로 잘라 창 폭을 채웁니다. Canvas의 `FixedHeight` 규칙에 따라
실제 1080p 출력에서는 PNG 원본 픽셀 크기로 표시됩니다.

현재 플레이 화면의 노트와 롱노트 파츠 크기는 PNG 메타데이터에서 계산합니다.
Canvas는 `FixedHeight`이므로 창 높이에 비례해 이미지와 글자도
함께 확대·축소되고, 넓은 창에서는 배경과 ScrollGear가 늘어나며 헤더·시간 표시는
각각 좌·우 기준으로 배치됩니다.

현재 플레이 화면은 마디선, key beam, 노트 head,
롱노트 body/tail/tick을 모두 회전된 Lane 부모 아래 둡니다. Balloon과 DengDeng은
일반 롱노트 body/tail 없이 전용 PNG만 사용합니다. 시작 시각부터 종료 시각까지
입력이 없으면 판정원에 멈추고, 첫 유효 입력부터 Penpot의 중앙 Focus UI로
전환합니다. Balloon은 `accepted/required` 진행률만큼 점차 커지고 완료하면
`BalloonBurst.png`를 200ms 표시합니다. DengDeng은 중앙의 소고를 회전시키고
완료하면 같은 처리 이미지를 200ms 동안 페이드아웃합니다. 제한 시간 안에
완료하지 못한 노트는 종료 시각부터 원래 Lane 속도로 판정원을 지나갑니다.
남은 횟수는 처리 중 `HitCounterCloud.png` 위에 표시합니다. 입력 패널은 각 키의 첫 번째
할당 키에 `KeyLightStrong`, 나머지 할당 키에 `KeyLightWeak`를 표시합니다.
점등 이미지는 흰색 알파 마스크에 노트와 같은 Ambient 색상을 곱하므로 Kat은
청색, Don은 적색으로 표시됩니다. 약한 불빛은 Ambient 색조와 45% tint 알파를
사용하고, 강한 불빛은 같은 색조의 채도를 높여 100% tint 알파로 표시하므로
PNG 자체의 밝기 차이와 함께 강·약 상태가 명확히 구분됩니다. 키를 누를
때마다 홀드 점등 위에 반투명 흰색 `KeyPressFlash` 십자광이 최대 불투명도로
다시 켜지고 0.1초 동안 선형으로 투명해진 뒤 사라집니다.
우측 정확도 표시는 같은 HUD 영역의 동적 텍스트로 노트별 평균을 소수점 여섯 자리까지
표시합니다. 아직 확정된 노트가 없으면 `--.--%`입니다.

## 상단 플레이 진행바

`GameplayProgressView`는 세션 진입 시 실제 생성된 노트의 마지막 head/tail 시각을 한 번
계산합니다. BPM·delay·offset이 이미 적용된 정수 us 시각을 사용하며, 플레이 중에는
`GameplayPresenter::UpdatePresentation`이 전달한 같은 `RhythmTimer` 시각으로 채움과 마커를
갱신합니다. 판정 유예/음원 길이/Result 전환 대기 3초는 100% 기준에 포함하지 않습니다.
0ms가 시작이고 마지막 노트(롱노트는 tail)가 100%입니다. 음수 시작 대기에는 0%,
마지막 시각 이후에는 100%를 유지합니다. 일시정지·재개·Reset·디버그 탐색은 기존 타이머를
따르므로 별도 진행 시계가 없습니다. 노트가 없으면 0%; 마지막 시각이 0ms 이하인 채보는
그 시각에 100%로 전환합니다. 이 경우 시간 문구는 음수 대신 `0:00`입니다.

기본 스킨 `InGame/Progress`의 `Background.png`(1820×30), `Track.png`(1704×10),
`Fill.png`(1704×10), `Marker.png`(14×14)를 독립 Sprite로 조합합니다. 원본은 RGBA PNG이며
바깥 여백은 투명합니다. 채움은 진행률만큼 bounds와 UV를 잘라 표시하고 마커 중심을
같은 끝점에 둡니다. 현재/전체 시간은 별도 `m:ss` 텍스트로 표시하고 현재 시간의
정수 초가 달라질 때만 문자열을 갱신합니다. 전체 시간 문구는 마지막 노트 시각입니다.
기존 문구가 포함된 단일 `GameProgressBar.png`는 제거했습니다.

Canvas는 노드를 소유하며 View는 observer만 보관하고 종료 시 해제합니다.
PNG는 진입 때 등록하고 프레임 갱신에서 파일 읽기/노트 전체 검색/노드 생성을 하지 않습니다.
폭은 `FixedHeight` 논리 화면 폭에서 좌우 20px를 제외하고, 시간 문구와 마커 크기는
이미지의 2/3 기준 배율을 유지합니다. 선택 스킨 INI의 `Progress.*` 이미지 매핑이 우선하며
없는 요소만 내장 기본본을 사용합니다. 제작 도구/프롬프트는 [진행바 자산](ProgressAssets.md)에 있습니다.
입력/탐색 동작이 없는 표시 전용 진행바이며 판정·오디오·Result 전환 조건은 바꾸지 않습니다.

## 판정 인디케이터와 링·문구

`GameplayJudgementView`가 하단 인디케이터와 판정원의 링·문구 상태를 소유합니다.
Canvas가 노드를 소유하고 Presenter는 초기화·입력 피드백·표시 시간 갱신·종료만 연결합니다.
기존 NoteEvent의 판정 등급/오차를 사용하며 점수·노트 처리·오디오 동작을 바꾸지 않습니다.

인디케이터는 MAX/PERFECT/GREAT/GOOD/BAD를 각각 독립적인 색 사각형으로 조합합니다.
넓은 BAD부터 작은 MAX까지 중심에 겹쳐 구성하며 색은 LaneKeyBeam의 공통 GradeColor와
같습니다. 0 기준선과 빠름/늦음 문구는 별도 요소입니다. 단일 JudgementIndicator.png를
늘리는 방식은 더 이상 사용하지 않습니다.
판정레벨 50에서 BAD 전체 폭은 논리 400px(±90ms)이고 양쪽에 10ms씩 회색 여유를 두어
전체 표시 범위는 ±100ms입니다. 다른 판정레벨의 기본 범위에 비례해 길이가 바뀌되,
세션 진입 때 정한 범위를 플레이 중 유지합니다. 싱코페이션의 노트별 추가 범위로
길이나 색 구간을 바꾸지 않습니다. 좁은 창/매우 넓은 범위에서는 좌우 20px 여백 안에
들어오도록 배율만 줄입니다.

실제 입력의 signedError를 왼쪽(빠름)·오른쪽(늦음) 위치로 표시하고 범위를 넘으면
마커를 끝점에 제한합니다. 최신 마커 하나를 200ms 표시하며 재입력하면 위치/수명을
갱신합니다. 범위 밖 입력은 백색 키빔만 생성하고 마커/문구를 시작하지 않습니다.
정확도 없는 Roll/count tick과 키 해제도 타이밍 마커/일반 판정 문구를 시작하지 않습니다.
자동 MISS는 MISS 링·문구만 표시하며 입력 시각이 없으므로 새 마커를 만들지 않습니다.

MAX/PERFECT/GREAT/GOOD/BAD/MISS/POOR/OK는 링과 상단 문구를 합친 투명 PNG 8종입니다.
`InGame/Judgements/<판정명>.png`를 세션 진입 때 등록하고 같은 노드를 재사용합니다.
입력마다 파일을 읽거나 노드를 만들지 않습니다. 판정원 자식이므로 위치·배율을 따르고
Taiko Lane의 -90° 회전을 되돌려 문구를 똑바로 표시합니다.
최근 판정 하나를 200ms 선형 페이드로 표시합니다. POOR 조건은 적색 키빔과 동일한
Good 이내 InputRejected이며 BAD는 기존 조기/늦은 Bad 입력을 표시합니다.
한 입력에서 이전 노트의 MISS와 다음 노트의 성공이 함께 오면 실제 입력 응답을 우선합니다.
reset/되감기/종료 시 마커와 링·문구를 숨깁니다.

롤·큰 롤·틱롤·큰 틱롤·풍선·뎅뎅·버즈는 최종 판정 뒤에도 링·문구를 표시합니다.
Completed이고 요구된 모든 연타/틱을 채우면 OK, 실패하거나 수용하지 못한 틱이 있으면 MISS를
같은 200ms 수명으로 표시합니다. 점수/노트 상태/오디오 규칙은 바꾸지 않습니다.
다른 노트의 실제 입력 판정이 동시에 있으면 기존 입력 우선순위를 유지합니다.
틱 마커는 규칙의 ProcessedTickCount를 ScrollGear snapshot으로 전달받아 처리된 prefix를
그리지 않습니다. 수용/실패된 틱 모두 숨기며, 조기 입력으로 처리된 미래 틱도 즉시 숨깁니다.
시각만으로 처리 여부를 추정하지 않습니다. 표시 틱은 규칙과 같은 시간순으로 정렬하여
음수 delay로 작성 순서와 시각이 다르거나 같은 시각의 틱이 둘 있어도 처리 인덱스가 일치합니다.
헤드 기준 틱 간격에 맞는 유효한 테일도 같은 마커로 표시하고 처리 뒤 숨깁니다.
Reset은 처리 개수를 0으로 되돌립니다.


8종 PNG는 내장 기본 스킨 팩에 포함됩니다. 선택 스킨 INI의 `Judgements.*` 매핑이 우선하고
누락된 판정 이미지만 내장본으로 대체합니다. 생성 도구/프롬프트는
[판정 이미지 제작 기록](JudgementAssets.md)에 있습니다.

코드 진입점은 `Client/GameScene/RhythmTestScene/Submodules`입니다.
`GameplayPresenter`가 Canvas를 소유하고 판정 표시는 `GameplayJudgementView`, 진행바는
`GameplayProgressView`에 위임합니다.
`GameplayLayout`, `GameplayNoteVisuals`,
`GameplayKeys`, `GameplayFeedbackVisuals`에 표시별 구현을 둡니다. `GameplaySessionController`는
`Client/GameFlow`에서 시간·입력·오디오와 모드 공통 상태머신을 소유하고
`GameplayFeedback`으로 표시할 이벤트만 전달합니다.
UI 수정 시 해당 표시 파일과 `GameplaySupport.h`의 관련 상수만 읽으면 됩니다.

## 표시 객체의 생성과 재사용

세션 진입 때 스킨 이미지를 등록하고 노트 모델의 비소유 ID 인덱스를 만듭니다.
노트의 head/body/HUD 표시 노드는 화면 snapshot에 처음 포함될 때 생성합니다.
롱노트 tick은 현재 보이는 시각 구간만 이진 검색하고 표시 객체를 재사용하며,
원래 tick 시각 배열과 판정 규칙은 유지합니다. 마디선도 보이는 구간의 pool만 갱신합니다.
화면 구간과 완료 연출을 벗어난 노드 subtree는 제거하여 지나간 노트가 누적되지 않습니다.
되돌아 탐색하면 같은 모델 ID와 등록된 이미지로 표시를 다시 생성합니다.
이미지 경로를 노트 출현 때 다시 조회하거나 파일을 다시 읽지 않습니다.
표시가 관찰하는 PlaySession은 Presenter보다 오래 살아야 하며 Shutdown에서 관찰을 해제합니다.

`Lane.Notes` 안에서는 시각이 빠른 노트가 앞에 그려집니다. 같은 시각이면 ID가 작은
노트를 앞에 두며, 헤드·몸통·테일·틱을 노트 루트와 함께 정렬합니다. 화면에 보이는
목록만 정렬하므로 Separate 속도 차이·되감기·표시 재생성에도 생성 순서에 의존하지 않습니다.
노트 레이어는 판정원 위에 두고 키빔 → 판정원 → 노트 순서로 그립니다.
판정원의 자식인 판정 링·문구도 같은 부모 순서를 상속합니다.

일반 롤·큰 롤(단무지)도 `DrumRollInputRule::Progress`의 실제 타격 수/요구 타격 수로
기존 HitCounterCloud를 표시합니다. 첫 유효 타격 후 나타나며 요구 횟수를 넘겨도
남은 수는 0으로 유지하고, 종료/실패/초기화 시 숨깁니다. 풍선·뎅뎅과 같은 카운터를
재사용하지만 롤의 헤드·몸통 이동과 판정·점수·종료 시각은 바꾸지 않습니다.

이미지 파일명은 기본본의 예시이며 실제 경로는 스킨 `[InGame]`과 씬 폴더 설정에서 해석합니다.
에디터도 같은 노트 이미지 키를 사용합니다. 자세한 계약은 [skin.ini](../BuiltInAssets.md#skinini와-씬별-이미지)에 있습니다.
