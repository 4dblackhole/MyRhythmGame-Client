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
완료 3초 뒤 Lobby로 돌아갈 때 Canvas, audio voice, session과 함께 삭제됩니다.

플레이 화면의 기준 시안과 런타임 PNG 자산 원본은 Penpot의
[Gameplay · Sky](https://design.penpot.app/#/workspace?team-id=3be9e5e1-190f-8090-8008-7464803e8b40&file-id=3be9e5e1-190f-8090-8008-7468d9cc1bc8&page-id=618d0170-ff55-8025-8008-80646ceaa9d9)에 있습니다.
기존 곡 선택 화면의 Sky 색상과 타이포그래피를 유지하면서 짙은 밤하늘색 Lane,
ScrollGear 프레임, 마디선, 입력 키, 키빔, 롱노트 tick과 Balloon/DengDeng 남은
횟수 표시를 한 화면 계층으로 구성합니다.

입력 패널의 네 키는 Penpot 원본 좌표를 2/3로 축소해 배치합니다. 판정원 중심은
입력 패널 오른쪽에서 Lane 높이의 절반만큼 떨어져 있어, 중심에서 입력 패널과
Lane 위·아래까지의 거리가 같습니다. Lane과 ScrollGear 표면은 오른쪽 논리 화면
끝까지 이어지고 마지막 `Lane.png` 타일만 UV로 잘립니다.

노트가 처음 표시되는 위치는 고정 픽셀 거리가 아니라 현재 Canvas 논리 폭으로
정해지는 Lane 길이와 가장 큰 노트 head의 반지름에서 계산합니다. Taiko의 회전된
Lane에서는 head 전체가 화면 오른쪽 경계 밖에 있을 때부터 왼쪽 판정점으로
이동합니다. 창 크기가 바뀌면 노트 이동 거리와 롱노트 tick 간격을 함께 갱신합니다.

노트와 플레이 HUD의 배포 기본본은
`FingerDrum.Assets/Assets/Skins/Default Skin/InGame`의 PNG입니다. 실행 파일 옆
스킨에서 누락된 파일만 이 내장 기본본으로 대체합니다.
`note.png`, `bignote.png`, `LNBody.png`, `LNTail.png`에는 Don/Kat/Roll Ambient
색상을 곱하고 대응하는 `*Overlay.png`는 흰색 원본으로 위에 그립니다. 일반/큰
롱노트는 서로 다른 body와 tail을 사용하고 Buzz tick은 마름모 자산을 사용합니다.
tail과 tail overlay는 별도로 회전하지 않고, 둥근 면이 위를 향하는 원본 방향
그대로 Lane 부모의 회전만 상속합니다.
기본 히트사운드는 `don.wav`, `kat.wav`, `bigdon.wav`, `bigkat.wav`입니다.
음악은 Stream으로 읽어 하나의 `RhythmTimer`가 가리키는 DSP 시각 0에 예약합니다.
Raw Input으로 발생한 히트사운드는 입력 timestamp를 판정에만 사용하고 즉시
재생합니다. 따라서 QPC와 장치 DSP clock의 장시간 미세 드리프트가 실시간 입력음을
미래 시각으로 예약해 지연을 누적시키지 않습니다.
오디오 초기화·파일 등록·재생 요청이 실패하면 가능한 나머지 파일 등록은 계속하고,
게임플레이 화면 상단에 첫 오류를 표시합니다. 오류는 무음으로 삼키지 않으며 노트
판정 시간축은 오디오 오류와 독립적으로 유지됩니다.

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
우측 정확도 표시는 같은 HUD 영역의 동적 텍스트로 노트별 평균을 소수점 두 자리까지
표시합니다. 아직 확정된 노트가 없으면 `--.--%`입니다.

코드 진입점은 `Client/GameScene/RhythmTestScene/Submodules`입니다.
`GameplayPresenter`가 Canvas와 노드를 소유하고 `GameplayLayout`, `GameplayNoteVisuals`,
`GameplayKeys`, `GameplayFeedbackVisuals`에 표시별 구현을 둡니다. `GameplaySessionController`는
시간·입력·오디오를 소유하고 `GameplayFeedback`으로 표시할 이벤트만 전달합니다.
UI 수정 시 해당 표시 파일과 `GameplaySupport.h`의 관련 상수만 읽으면 됩니다.
