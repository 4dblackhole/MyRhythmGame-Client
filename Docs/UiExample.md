# ColoredCubeScene UI 예제

`ColoredCubeScene`은 하나의 `WorldSpaceCanvas`를 화면 공간과 회전된 월드 평면에
번갈아 표시한다. Widget 트리와 action 처리는 그대로이고 입력 좌표 변환과 D3D12
표현 함수만 바뀐다.

## 조작

- `F2`: Screen Space / World Plane 표시 전환
- `CUBE ROTATION`: 큐브 회전 on/off
- `ROTATION SPEED`: pointer capture를 사용하는 회전 속도 Slider
- `SCREEN SPACE` 또는 `WORLD PLANE`: 누를 때 다음 표시 방식으로 바뀌는 ComboBox
- 마우스 왼쪽 버튼: UI 조작
- 마우스 오른쪽 버튼: 기존 카메라 회전

화면 모드에서는 옵션 패널 원점이 `(20, 20)` 픽셀이다. 월드 모드에서는 카메라
view-projection으로 pointer Ray를 만든 뒤 `PlaneUiSurface`가 UV를 반환한다. 평면은
Y축으로 회전되어 있으므로 axis-aligned 2D Rectangle 충돌을 월드 좌표에 직접
적용해서는 같은 결과를 얻을 수 없다.

World Plane의 현재 직접 표현 경로는 사각형 UI primitive를 표시하며 screen-space
DirectWrite 글자는 표시하지 않는다. 곡면 글자까지 포함하려면 엔진 문서
`Docs/UiArchitecture.md`의 canvas-to-render-target 확장 경계를 사용한다.
