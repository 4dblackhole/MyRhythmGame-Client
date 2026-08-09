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

## 실제 곡 catalog와 빈 기록

`FingerDrum.Chart/Catalog/SongCatalog`가 실행 파일의 `assets/songs`를 재귀
탐색합니다. YMM의 음악 파일을 해석하고 YMP의 `Music metadata` 상대 경로로
패턴을 결합합니다. 현재 RPG에서 옮긴 YMM 5개와 YMP 8개가 모두 catalog에
포함되며, 패턴이 없는 음악도 SONG LIST에는 표시됩니다.

- 카테고리는 현재 `ALL` 하나입니다.
- LOCAL RECORD는 기록 저장소가 생기기 전까지 `NO RECORDS`만 표시합니다.
- SONG LIST 행에는 번호·BPM·난이도 없이 곡명과 아티스트만 표시합니다.
- 중앙에는 선택한 곡명과 아티스트, 아래에는 그 곡의 패턴 목록을 표시합니다.
- 우측 상단은 이름·레이팅 없이 임시 파일럿 이미지만 표시합니다.
- catalog가 비어 있으면 기존 `NO SONGS AVAILABLE`, `NO SONG SELECTED`,
  `NO PATTERNS AVAILABLE` empty state로 자동 복귀합니다.

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

각 패널의 자식은 그 패널 안에서만 Z-Order를 비교합니다. 곡, 패턴, PLAY와
BACK은 표시 노드 자체가 collider와 button behavior를 가지므로 클릭 영역과
시각 영역이 일치합니다.

## 조작

- `↑` / `↓`: 곡 선택
- `←` / `→`: 선택한 곡의 패턴 선택
- `Enter` / `Space` 또는 `PLAY` 클릭: gameplay 진입
- `Escape` 또는 좌측 상단 `< BACK` 클릭: 로고 화면으로 돌아가기

곡 목록 행의 번호, BPM, 난이도와 플레이 기록은 별도 영역의 책임입니다.
선택된 곡명이나 아티스트가 상자보다 길면 `MarqueeTextComponent`가 고정 길이
문자 창을 천천히 이동시켜 모든 글자를 순서대로 보여줍니다. 짧은 텍스트는
움직이지 않습니다.

Lobby가 `GameplayLaunchRequest`에 선택한 pattern, optional YME, music path와
mode를 기록한 다음 `ChangeScene`을 요청합니다. SceneManager에 미리 등록된
`DestroyOnExit` factory가 이 요청을 읽는 새 gameplay 객체를 생성합니다.

## 디자인 확장

현재 엔진은 사각 패널과 텍스트를 사용하므로 Penpot의 둥근 모서리와 복합
gradient는 가까운 단색으로 표현했습니다. 향후 PNG 스킨을 적용할 때는
`SpriteVisualComponent::SetImage`로 패널 이미지를 교체합니다. 실제 곡 항목을
추가할 때 같은 노드에 collider와 button behavior를 함께 구성하면 디자인과
클릭 영역이 분리되지 않습니다.
