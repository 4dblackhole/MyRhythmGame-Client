# 첫 화면과 게임 표시 이름

`YOUMUGame`은 현재 Client의 게임 진입점이며, 첫 Scene은
`YOUMULogoScene`이다. 배경 clear color는 AliceBlue (`#F0F8FF`)다.
제품 표시 이름은 [GameIdentity.h](../Client/Texts/GameIdentity.h)의 `GameName`을 참조한다.
창/오류창/에디터 확인창의 이름 변경 방법과 내부 식별자와의 구분은
[문구 관리](TextManagement.md#게임-이름-변경)를 따른다.

## 로고 asset과 배치

세 PNG는 원본 크기와 가로 순서를 유지한다. 경로는 스킨의 `[Logo]` 항목에서
지정하며 전체 화면 `Background`와 메뉴 버튼 이미지도 교체할 수 있다.
문법·기본본 대체 규칙은 [skin.ini](BuiltInAssets.md#skinini와-씬별-이미지)를 따른다.

| 순서 | source asset | 원본 크기 |
| --- | --- | --- |
| 왼쪽 | `YOUMU.Assets/Assets/Skins/Default Skin/Logo/LeftFade.png` | 1680×1280 |
| 가운데 | `YOUMU.Assets/Assets/Skins/Default Skin/Logo/Center.png` | 2400×1280 |
| 오른쪽 | `YOUMU.Assets/Assets/Skins/Default Skin/Logo/RightFade.png` | 1680×1280 |

세 node는 모두 `(0.5, 0.5)` pivot을 사용한다. 이 node들은 화면 중심 anchor를
부모로 하는 하나의 strip 안에 놓인다. strip scale은 원본 높이 1280을 Canvas의 논리 높이에 맞춰 계산한다.
세로와 종횡비를 유지하고 화면보다 넓은 좌우 외곽은 뷰포트 밖에 둔다.

`Visual2DCanvas`는 `1280×720`, `FixedHeight` 기준이다. 세로 기준 크기는 유지하고
와이드 화면에서는 논리 폭만 늘어나므로, 화면 확대 시에도 로고가 중심에서 흔들리지
않는다.

## 타이틀 메뉴 조작

- `Game Start`: YMM/YMP catalog를 표시하는 `MusicSelectScene`으로 이동한다.
- `Editor`: 같은 catalog 탐색을 사용하는 에디터 곡 선택 Scene으로 이동한다.
  기록 패널은 생성하지 않고 곡·난이도 정보 패널이 그 영역까지 차지한다.
- `Option`: 왼쪽 옵션 패널을 열고 닫는다.
- `Exit`: `SceneManager::Quit`을 요청해 게임을 정상 종료한다.
- 마우스 이동: hover한 버튼으로 삼각형 선택 커서가 이동한다.
- 마우스 왼쪽 클릭: 해당 버튼을 실행한다.
- `↑`/`↓` 또는 `W`/`S`: 선택 커서를 이동한다.
- `Enter` 또는 `Space`: 현재 선택한 버튼을 실행한다.
- `Ctrl+O`: 옵션 패널을 연다. 다시 누르면 0.2초 슬라이드로 닫힌다.
- `F7`: 모든 YOUMU Scene 위 우측 하단 FPS/UPS 표시를 토글한다.

옵션 패널은 16:9 논리 폭의 1/4인 320 단위, 화면 전체 높이 720 단위다.
휠이나 가운데 버튼 드래그, 빈 영역 왼쪽 버튼 드래그로 스크롤한다.
옵션 항목과 문구·글꼴 소유권은
[문구·언어·글꼴 관리](TextManagement.md)에 설명한다.

커서 기본 자산은 `YOUMU.Assets/Assets/Skins/Default Skin/Logo/Menu/SelectionCursor.png`이며
투명 PNG다.
마우스가 정지된 상태에서는 키보드로 옮긴 선택을 다시 빼앗지 않도록, 새 mouse
event가 발생했을 때만 pointer hover를 선택 상태에 반영한다.

## 소스

- 실제 게임: `Client/App/YOUMUGame.*`, `Client/GameScene/YOUMULogoScene/`,
  `Client/GameScene/MusicSelectScene/`, `Client/GameFlow/YOUMUSceneIds.h`
- 로고 UI 수정: `YOUMULogoScene/Submodules/LogoView`, `LogoLayout`, `LogoInput`.
  Scene은 UI가 반환한 선택을 화면 전환으로 연결한다.
