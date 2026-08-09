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

## 현재 빈 상태

아직 곡 catalog와 기록 저장소가 없으므로 화면에 존재하지 않는 데이터를
만들어 표시하지 않습니다.

- 카테고리는 `ALL` 하나만 표시합니다.
- LOCAL RECORD는 `NO RECORDS`만 표시합니다.
- 중앙 곡 정보는 `NO SONG SELECTED`를 표시합니다.
- 패턴 영역은 `NO PATTERNS AVAILABLE`을 표시합니다.
- SONG LIST는 번호 없이 `NO SONGS AVAILABLE`만 표시합니다.
- 우측 상단은 임시 파일럿 이미지 하나만 표시하며 이름, 레이팅, 크레딧은
  표시하지 않습니다.

실제 YMM catalog와 기록 파일 관리 계층이 생긴 후에만 이 빈 상태를 실제
view-model로 교체합니다.

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
   └─ Footer
```

각 패널의 자식은 그 패널 안에서만 Z-Order를 비교합니다. 현재는 선택할
항목이 없으므로 클릭 collider나 임시 버튼을 만들지 않습니다.

## 조작

- `Escape`: 로고 화면으로 돌아가기

실제 YMM catalog가 준비되면 catalog view-model을 추가하고, 선택 시 중앙
제목·아티스트·재킷·패턴 목록을 같은 노드에 갱신합니다.

곡 목록 행에는 곡명과 아티스트명처럼 음악 자체의 정보만 표시합니다. 번호,
BPM, 난이도와 플레이 기록은 곡 목록 행에 섞지 않고 각각의 상세 패널에서
관리합니다. 선택된 곡명이 상자보다 길면 `MarqueeTextComponent`가 고정 길이
문자 창을 천천히 이동시켜 모든 글자를 순서대로 보여줍니다. 짧은 텍스트는
움직이지 않습니다.

## 디자인 확장

현재 엔진은 사각 패널과 텍스트를 사용하므로 Penpot의 둥근 모서리와 복합
gradient는 가까운 단색으로 표현했습니다. 향후 PNG 스킨을 적용할 때는
`SpriteVisualComponent::SetImage`로 패널 이미지를 교체합니다. 실제 곡 항목을
추가할 때 같은 노드에 collider와 button behavior를 함께 구성하면 디자인과
클릭 영역이 분리되지 않습니다.
