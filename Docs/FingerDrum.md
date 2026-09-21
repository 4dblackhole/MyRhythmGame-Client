# FingerDrum 첫 화면

`FingerDrumGame`은 현재 Client의 게임 진입점이며, 첫 Scene은
`FingerDrumLogoScene`이다. 배경 clear color는 AliceBlue (`#F0F8FF`)다.

## 로고 asset과 배치

세 PNG는 원본 크기와 가로 순서를 유지한다.

| 순서 | source asset | 원본 크기 |
| --- | --- | --- |
| 왼쪽 | `FingerDrum.Assets/Assets/Skins/Default Skin/TitleImage/Logo/LeftFade.png` | 1680×1280 |
| 가운데 | `FingerDrum.Assets/Assets/Skins/Default Skin/TitleImage/Logo/Center.png` | 2400×1280 |
| 오른쪽 | `FingerDrum.Assets/Assets/Skins/Default Skin/TitleImage/Logo/RightFade.png` | 1680×1280 |

세 node는 모두 `(0.5, 0.5)` pivot을 사용한다. 이 node들은 화면 중심 anchor를
부모로 하는 하나의 strip 안에 놓인다. strip scale은 Canvas의 논리 폭과 높이 중
더 제한적인 값으로 계산한다. 따라서 해상도·화면 비율이 바뀌어도 종횡비를 유지하고
좌우 외곽이 잘리지 않는다.

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
- `F7`: 모든 FingerDrum Scene 위 우측 하단 FPS/UPS 표시를 토글한다.

옵션 패널은 16:9 논리 폭의 1/4인 320 단위, 화면 전체 높이 720 단위다.
휠이나 가운데 버튼 드래그, 빈 영역 왼쪽 버튼 드래그로 스크롤하며 현재는
한국어/영어 언어 선택기만 제공한다. 문구와 글꼴 소유권은
[문구·언어·글꼴 관리](TextManagement.md)에 설명한다.

커서 기본 자산은 `FingerDrum.Assets/Assets/Skins/Default Skin/TitleImage/Menu/SelectionCursor.png`이며
투명 PNG다.
마우스가 정지된 상태에서는 키보드로 옮긴 선택을 다시 빼앗지 않도록, 새 mouse
event가 발생했을 때만 pointer hover를 선택 상태에 반영한다.

## 소스와 예제의 분리

- 실제 게임: `Client/App/FingerDrumGame.*`, `Client/GameScene/FingerDrumLogoScene/`,
  `Client/GameScene/MusicSelectScene/`, `Client/GameFlow/FingerDrumSceneIds.h`
- 로고 UI 수정: `FingerDrumLogoScene/Submodules/LogoView`, `LogoLayout`, `LogoInput`.
  Scene은 UI가 반환한 선택을 화면 전환으로 연결한다.
- 보관한 기술 예제: `Client/Examples/ColoredCube/`
- 예제 문서: `Docs/Examples/ColoredCube/`

ColoredCube 코드는 계속 빌드되어 회귀 확인과 엔진 기능 참고에 사용할 수 있지만,
기본 실행 경로에는 등록되지 않는다.
