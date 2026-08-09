# Penpot 곡 선택 화면

`LobbyScene`은 Penpot 파일의 `Music Select · Sky` 페이지에 있는
`곡 선택 화면 · Logo Sky Theme · 1920×1080` 보드를 기준으로 구현했습니다.

원본 링크:
<https://design.penpot.app/#/workspace?team-id=3be9e5e1-190f-8090-8008-7464803e8b40&file-id=3be9e5e1-190f-8090-8008-7468d9cc1bc8&page-id=3be9e5e1-190f-8090-8008-7468d9cc1bc9>

## 좌표와 화면비

Penpot의 1920×1080 좌표에 `2/3`를 적용해 엔진의 1280×720 기준으로
변환합니다. 전체 보드는 `Anchor::Center`에 있고 `FixedHeight` Canvas를
사용합니다. 따라서 창이 넓어지면 좌우 여백이 늘어나며, 보드의 양 끝이
임의로 잘리지 않습니다.

## 트리와 Z-Order

```text
Visual2DCanvas
└─ SongSelect.Board
   ├─ Background
   ├─ Header
   ├─ Categories
   ├─ RecordPanel
   ├─ SongInformation
   ├─ PatternPanel
   ├─ SongList
   ├─ BottomBar
   └─ ModifierPopup (ZIndex 100)
```

각 패널의 자식은 그 패널 안에서만 Z-Order를 비교합니다. ModifierPopup은
보드의 최상위 형제이며 배경에도 collider가 있어 빈 영역을 클릭해도 뒤쪽
곡 행으로 입력이 새지 않습니다. 팝업을 숨길 때는 `Visible`과 `Enabled`를
함께 끄고 input hit-test cache를 무효화합니다.

## 조작

- 곡 행 클릭 또는 `Up/Down`: 곡 선택
- `Enter` 또는 하단 PLAY: 테스트 플레이
- `M` 또는 MODIFIERS: 설정 팝업 열기/닫기
- `Escape`: 팝업을 닫거나 로고 화면으로 돌아가기

현재 곡과 기록은 기능 검증용 가상 데이터입니다. 실제 YMM catalog가
준비되면 `SongTitles` 고정 배열을 catalog view-model로 교체하고, 선택 시
중앙 제목·아티스트·재킷·패턴 목록을 같은 노드에 갱신하면 됩니다.

## 디자인 확장

현재 엔진은 사각 패널과 텍스트를 사용하므로 Penpot의 둥근 모서리와 복합
gradient는 가까운 단색으로 표현했습니다. 향후 PNG 스킨을 적용할 때는
`SpriteVisualComponent::SetImage`로 패널 이미지만 교체하고, collider와
button behavior는 기존 노드에 유지합니다. 따라서 디자인과 실제 클릭 영역이
분리되지 않습니다.
