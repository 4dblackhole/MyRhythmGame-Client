# Visual2D·Sprite·위젯 사용 가이드

`Visual2DNode`는 Sprite와 UI가 공유하는 구체 클래스다. 상속으로 Button 종류를
늘리지 않고, 필요한 시각·충돌·입력·동작 컴포넌트를 붙여 기능을 구성한다.
클라이언트는 엔진 공개 헤더 `MRG_Core.h`만 포함한다.

## 좌표계와 아홉 앵커

기본 Canvas는 `1280x720` 디자인 좌표에서 세로 720을 고정한다. 실제 픽셀 배율은
`viewportHeight / 720`이며, 화면비가 넓어지면 논리 폭만 늘어난다. 따라서 1440p
화면의 논리 크기는 `1280x720`, 배율은 2이고, 2560x1080 화면의 논리 크기는
약 `1706.67x720`, 배율은 1.5다.

```cpp
canvas_ = std::make_unique<mrg::visual2d::Visual2DCanvas>();
canvas_->SetViewportSize({
    static_cast<float>(viewportWidth),
    static_cast<float>(viewportHeight)});
```

Canvas는 렌더링되지 않는 앵커 노드 아홉 개를 항상 소유한다.

```text
TopLeft       TopCenter       TopRight
MiddleLeft    Center          MiddleRight
BottomLeft    BottomCenter    BottomRight
```

앵커에 연결한 노드의 `SetPosition`은 해당 기준점에서 노드의 pivot까지의 거리다.
예를 들어 중앙에 `100x60` Sprite를 배치하면 `(0,0)`이 Sprite 중앙을 화면 중앙에
맞춘다. 우측 상단에서 안쪽으로 20만큼 띄우려면 다음과 같이 만든다.

```cpp
auto& badge = canvas_->CreateNode(
    mrg::visual2d::Anchor::TopRight,
    "Badge");
badge.SetBounds({-20.0F, 20.0F, 100.0F, 50.0F});
badge.AddComponent<mrg::visual2d::SpriteVisualComponent>();
```

창 크기가 바뀔 때마다 `SetViewportSize`를 호출한다. 앵커 Transform이 새 논리 폭과
높이에 맞춰 갱신되므로 각 모서리·변 중앙·화면 중앙 기준 배치가 유지된다.

## Sprite와 동작 컴포넌트

아무 동작이 없는 Sprite는 시각 컴포넌트 하나만 가진다.

```cpp
auto& sprite = mrg::visual2d::CreateSprite(
    canvas_->AnchorNode(mrg::visual2d::Anchor::Center),
    {0.0F, 0.0F, 180.0F, 180.0F},
    imageHandle,
    "Character");

sprite.Transform().SetRotationRollPitchYaw(0.1F, 0.25F, 0.0F);
sprite.Transform().SetScale(1.0F, 1.0F, 1.0F);
```

같은 Sprite를 클릭 가능하게 만들려면 상속하지 않고 컴포넌트를 추가한다.

```cpp
sprite.AddComponent<mrg::visual2d::RectangleCollider2DComponent>();
sprite.AddComponent<mrg::visual2d::PointerReceiverComponent>(
    [](mrg::visual2d::Visual2DNode& node,
       const mrg::visual2d::PointerEvent& event,
       std::vector<mrg::visual2d::Action>& actions)
    {
        if (event.type == mrg::visual2d::PointerEventType::Click)
        {
            actions.push_back({
                mrg::visual2d::ActionType::Clicked,
                node.Id(),
                0.0F,
                0,
                event.timestampTicks});
        }
    });
```

제공 컴포넌트는 다음과 같다.

| 영역 | 컴포넌트 |
| --- | --- |
| 표시 | `SpriteVisualComponent`, `TextVisualComponent` |
| 충돌 | `RectangleCollider2DComponent`, `CircleCollider2DComponent`, `CustomCollider2DComponent` |
| 입력 | `PointerReceiverComponent` |
| 동작 | `ButtonBehaviorComponent`, `ToggleBehaviorComponent`, `SliderBehaviorComponent`, `CycleSelectorBehaviorComponent`, `ComboBoxBehaviorComponent` |
| 갱신 | `AnimatorComponent` |

컴포넌트는 `AddComponent`, `GetComponent`, `RemoveComponent`로 실행 중에도 교체할
수 있다. `CustomCollider2DComponent`에는 삼각형, 알파 마스크 같은 게임 전용
판정 전략을 전달할 수 있다.

## 위젯 팩토리

Button이나 ComboBox도 별도 Node 파생형이 아니다. 팩토리는 자주 쓰는 컴포넌트
조합을 만든 뒤 일반 `Visual2DNode&`를 반환한다.

```cpp
auto& panel = mrg::visual2d::CreatePanel(
    canvas_->AnchorNode(mrg::visual2d::Anchor::MiddleLeft),
    {0.0F, 0.0F, 480.0F, 340.0F});

auto& apply = mrg::visual2d::CreateButton(
    panel,
    {16.0F, 64.0F, 200.0F, 40.0F},
    L"APPLY");
const mrg::visual2d::NodeId applyId = apply.Id();

auto& device = mrg::visual2d::CreateComboBox(
    panel,
    {16.0F, 132.0F, 448.0F, 38.0F},
    {L"Driver 0", L"Driver 1"});
auto* combo = device.GetComponent<
    mrg::visual2d::ComboBoxBehaviorComponent>();
combo->SetItemHeight(36.0F);
combo->SetMaxVisibleItems(4);
```

노드는 부모가 `unique_ptr`로 소유한다. 장기간 보관할 때는 참조 대신 `NodeId`를
보관하고 `canvas.FindNode(id)`로 다시 찾는다. 삭제 직전에는 입력 라우터를
`Reset`하여 hover/capture 대상이 소멸한 노드를 가리키지 않도록 한다.

## 화면 입력과 히트박스

화면 픽셀 좌표를 Canvas 논리 좌표로 바꾼 후 입력 라우터에 전달한다.

```cpp
const auto point = mrg::visual2d::MapScreenPointer(
    mousePosition,
    viewportSize,
    *canvas_);

mrg::visual2d::PointerInput pointer{};
pointer.available = point.has_value();
pointer.position = point.value_or(mrg::visual2d::Point{});
pointer.leftButtonDown = input.IsMouseButtonDown(
    mrg::platform::MouseButton::Left);
pointer.leftButtonPressed = input.WasMouseButtonPressed(
    mrg::platform::MouseButton::Left);
pointer.leftButtonReleased = input.WasMouseButtonReleased(
    mrg::platform::MouseButton::Left);
pointer.wheelDelta = input.MouseWheelDelta();
inputRouter_.Process(*canvas_, pointer);
```

기본 사각형 충돌체는 노드의 local bounds를 사용한다. 라우터는 Canvas ray를
노드의 역 World Transform으로 변환하므로 XYZ 회전, scale, 부모 Transform이
표시와 판정에 똑같이 반영된다. ComboBox popup은 포커스가 다른 곳으로 이동해도
자동으로 닫히지 않으며, 다시 field를 누르거나 항목을 선택할 때 닫힌다.

## PNG와 상태별 디자인

이미지는 렌더러에서 한 번 로드하고 opaque handle을 시각 컴포넌트에 지정한다.
서로 크기가 다른 PNG도 각각 독립 `Texture2D`로 올라가며, 같은 SRV descriptor
table에서 texture index로 선택되므로 크기를 강제로 맞출 필요가 없다.

```cpp
const auto normal = visualRenderer_.LoadImage(
    RuntimeAssetPath(L"Images\\ButtonNormal.png"));
const auto hover = visualRenderer_.LoadImage(
    RuntimeAssetPath(L"Images\\ButtonHover.png"));

mrg::visual2d::VisualStyle style{};
style.normalImage = normal;
style.hoveredImage = hover;
style.pressedImage = hover;
style.disabledImage = normal;
style.normal = style.hovered = style.pressed =
    {1.0F, 1.0F, 1.0F, 1.0F};

apply.GetComponent<mrg::visual2d::SpriteVisualComponent>()->SetStyle(style);
```

같은 texture table과 렌더 상태를 쓰는 Sprite/위젯은 함께 인스턴싱된다. 투명 PNG
여백은 히트박스에 포함되므로, 보이는 영역과 클릭 영역이 달라야 할 때는
`RectangleCollider2DComponent(Rect)`로 별도 local bounds를 지정하거나
`CustomCollider2DComponent`를 사용한다.

## 트리 기반 Z-Order

각 부모가 stacking context다. 부모의 시각 요소를 먼저 그리고, 자식을
`ZIndex` 오름차순·추가 순서대로 재귀 렌더링한다. hit-test는 이 순서를 뒤집어
실행하므로 가장 앞에 보이는 대상이 먼저 클릭된다. 한 자식의 전체 subtree가
그 다음 형제보다 앞이나 뒤에 놓이며, 자식 하나만 다른 부모의 stacking
context를 뚫고 나가지 않는다.

서로 다른 Canvas는 `SubmitScreen`의 `canvasZOrder`로 순서를 정한다.

```cpp
visualRenderer_.SubmitScreen(optionsCanvas, context, {}, 0);
visualRenderer_.SubmitScreen(audioCanvas, context, {}, 1); // 전체가 더 앞
```

## 평면과 곡면 표시

화면은 `SubmitScreen`, 평면은 `SubmitPlane`을 쓴다. 곡면에서는 Canvas를 RGBA8
렌더 텍스처로 먼저 그린 뒤 그 텍스처를 mesh UV로 샘플링한다. Sprite, 사각형,
회전 Transform, 글자가 모두 같은 off-screen 결과에 포함된다.

입력은 표현 방식과 분리되어 있다. `PlaneVisual2DSurface` 또는
`MeshUvVisual2DSurface`가 world ray 충돌점의 UV를 반환하고,
`WorldSpaceVisual2DCanvas::MapPointer`가 이를 Canvas 좌표로 바꾼다. 따라서
회전된 평면과 UV가 있는 곡면에서도 같은 입력 라우터와 동작 컴포넌트를 쓴다.

## 프레임 처리

Update에서는 `canvas.Update(deltaSeconds)`, 입력 라우팅, `TakeActions()` 순서로
게임 상태를 갱신한다. Render에서는 Canvas를 제출하기만 하고 실제 D3D12 명령
기록과 인스턴스 배치는 엔진이 프레임 끝에 수행한다.

실제 사용 코드는 `Client/GameScene/ColoredCubeScene.cpp`와
`Client/GameScene/Examples/WidgetExampleScene.cpp`에 있다.
