# ColoredCubeScene UI 예제

`ColoredCubeScene`은 하나의 `WorldSpaceCanvas`를 화면 공간과 회전된 곡면에 번갈아
표시한다. Widget 트리와 action 처리는 그대로이고 입력 좌표 변환과 D3D12 표현
함수만 바뀐다. 곡면 모드에서는 글자를 포함한 Canvas 전체를 RGBA8 render target에
그린 뒤 `CurvedRectangleShape`의 UV로 샘플링한다.

## 조작

- `F2`: Screen Space / World Curved 표시 전환
- `Tab`: 왼쪽 오디오 출력 패널 열기/닫기
- 위 오디오 ComboBox: `SYSTEM DEFAULT (FMOD AUTO)`, WASAPI, ASIO 중 출력 API 선택
- 아래 오디오 ComboBox: 위에서 선택한 API의 driver index와 장치명 선택
- `Z`: 현재 선택된 출력으로 `Assets/Sounds/pop.wav` 재생
- `CUBE ROTATION`: 큐브 회전 on/off
- `ROTATION SPEED`: pointer capture를 사용하는 회전 속도 Slider
- `SCREEN SPACE` 또는 `WORLD CURVED`: 누를 때 다음 표시 방식으로 바뀌는 ComboBox
- 마우스 왼쪽 버튼: UI 조작
- 마우스 오른쪽 버튼: 기존 카메라 회전

화면 모드에서는 옵션 패널 원점이 `(20, 20)` 픽셀이다. 월드 모드에서는 카메라
view-projection으로 pointer Ray를 만든 뒤, 표시 mesh와 같은 정점/인덱스/UV를 가진
`MeshUvUiSurface`가 UV를 반환한다. 따라서 곡률과 Y축 회전이 모두 click 좌표에
반영된다.

오디오 패널은 닫혔을 때 화면 왼쪽 바깥에 있고 `Tab`을 누르면 Update의
`deltaSeconds`로 X 위치를 보간해 나타난다. 위 ComboBox에서 API를 바꾸면 해당
API 목록을 다시 열거한 다음 첫 driver를 즉시 적용하고, 아래 ComboBox는 같은
API의 다른 driver를 순환해 선택한다. 따라서 실행 중 연결된 ASIO 장치도 API를
선택할 때 갱신된다. 장치 전환은 새 FMOD system 초기화와 `pop.wav` 재등록이 모두 성공한 뒤에만
확정되므로 실패한 ASIO 선택은 기존 출력을 끊지 않는다.

현재 FMOD 2.x에는 DirectSound output backend가 없으므로 첫 항목은 실제
DirectSound가 아닌 `SYSTEM DEFAULT (FMOD AUTO)`다. DirectSound를 표기해 선택되는
것처럼 보이게 하지 않고, Windows 기본 출력 선택이라는 실제 동작을 표시한다.
