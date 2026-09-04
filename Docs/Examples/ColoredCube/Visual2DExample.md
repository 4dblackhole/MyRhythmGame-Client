# ColoredCubeScene Visual2D 예제

`ColoredCubeScene`은 Sprite와 위젯이 통합된 `Visual2DNode` 트리를 두 가지 방식으로
보여준다. 옵션 Canvas는 화면 공간과 회전된 곡면을 전환하고, 오디오 Canvas는
화면 왼쪽 중앙 앵커를 기준으로 슬라이드한다.

## 조작

- `F2`: 옵션 Canvas의 Screen Space / World Curved 표시 전환
- `Tab`: 왼쪽 오디오 출력 패널 열기/닫기
- `OUTPUT API`: SYSTEM DEFAULT(FMOD AUTO), WASAPI, ASIO 순환 선택
- `DEVICE`: 선택한 output API에서 탐지된 driver를 ComboBox로 선택
- `DSP BUFFER LENGTH`: buffer당 sample 수 선택, 기본 256
- `Z`: 현재 선택된 출력으로 `Assets/Sounds/pop.wav` 재생
- 마우스 왼쪽 버튼: Visual2D 입력
- 마우스 오른쪽 버튼: 카메라 회전

## 옵션 Canvas

`WorldSpaceVisual2DCanvas`는 하나의 `Visual2DCanvas`와 교체 가능한
`IVisual2DSurface`를 합성한다. 화면 모드에서는 `SubmitScreen`으로 직접 그리고,
곡면 모드에서는 `RenderToTexture`로 Sprite·도형·텍스트를 RGBA8 target에 그린 후
`CurvedRectangleShape`의 UV로 샘플링한다.

월드 입력은 카메라 view-projection으로 pointer ray를 만든 뒤
`MeshUvVisual2DSurface`와 충돌시킨다. 반환된 UV가 Canvas 좌표가 되므로 mesh
곡률과 Y축 회전이 표시뿐 아니라 클릭에도 반영된다.
화면·평면·곡면 Canvas는 모두 정중앙 원점과 Y-up 논리 좌표를 사용한다.

## 오디오 Canvas

오디오 Canvas는 패널에 필요한 `480x340` 영역만 차지하는 `Fixed` Canvas다.
내부 패널은 `TopLeft` 앵커의 `(0,0)` offset으로 Canvas 전체를 채우고, 화면 왼쪽
중앙에서 계산한 패널 좌상단 픽셀의 X 좌표를 `deltaSeconds`로 보간한다.
렌더링과 입력 매핑에 같은 화면 원점을 전달하므로
화면비가 바뀌어도 표시·히트박스·슬라이드 위치가 일치한다.

각 컨트롤은 파생 위젯 클래스가 아니라 다음 컴포넌트 조합이다.

- panel/label: `SpriteVisualComponent` + 선택적인 `TextVisualComponent`
- output API: `CycleSelectorBehaviorComponent`
- device: `ComboBoxBehaviorComponent`
- 입력 대상: `RectangleCollider2DComponent` + 동작 컴포넌트

`Widget1.png`, `Widget2.png`는 엔진 소유
`EngineServices::visual2DRendering.LoadImage`로 로드된다. Scene은 렌더러를
생성하거나 종료하지 않고 Canvas와 이미지 핸들만 소유한다.
두 PNG 크기가 달라도 독립 Texture2D와 하나의 descriptor table을 사용한다.
장식용 하이라이트에는 충돌/입력 컴포넌트가 없으므로 조작을 가로막지 않는다.

output API를 바꿀 때만 FMOD system을 해당 output으로 다시 초기화하고 driver
목록을 한 번 열거한다. `DEVICE` 선택은 이미 보관한 목록의 index만 변경한다.
DSP sample 수 변경 시 Client 소유 `AudioClip`을 해제하고 mixer를 다시 구성한 뒤
`pop.wav`를 다시 로드한다.

화면 Z-Order는 옵션 Canvas `0`, 오디오 Canvas `1`이다. 따라서 오디오 패널의
전체 트리가 옵션 트리보다 앞에 있으며, ComboBox popup도 자신의 부모 트리
순서를 그대로 따른다. Canvas depth band가 먼저 결정되고 각 band 안에서만 트리의
`ZIndex`가 적용되므로 옵션 Canvas의 자식이 오디오 Canvas 앞으로 튀어나오지 않는다.
