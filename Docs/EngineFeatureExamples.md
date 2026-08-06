# 엔진 주요 기능 예제

메인 `ColoredCubeScene`에서 숫자 `1`, `2`, `3`을 누르면 각각 독립된 예제
Scene으로 이동한다. 세 Scene은 모두 `DestroyOnExit`이므로 진입할 때 동적으로
생성되고 `Space`로 돌아오면 `Shutdown` 후 삭제된다. 예제 코드는 클라이언트가
엔진의 단일 공개 헤더인 `MRG_Core.h`만 포함하는 조건을 그대로 지킨다.

| 키 | Scene | 보여주는 기능 |
| --- | --- | --- |
| `1` | `MeshExampleScene` | 기본 Shape, 사용자 정의 Shape, 컴파일타임 정점 형식, GPU Mesh, Material, MeshInstance, 텍스트 |
| `2` | `CollisionExampleScene` | 렌더 Mesh와 충돌체의 상태 동기화, `Sphere3D`-`Obb3D` 충돌, 결과 색상 표시 |
| `3` | `WidgetExampleScene` | Canvas 소유권, 위젯 동적 추가/삭제, 안정적인 ID 검색, 포인터 입력과 UI Action |

모든 예제에서 `Space`는 메인 Scene 복귀, `Escape`는 종료다.

## 1. Shape와 Mesh 렌더링

[MeshExampleScene.cpp](../Client/GameScene/Examples/MeshExampleScene.cpp)는 다음
세 가지 CPU Shape를 만든다.

- `RectangleShape`: 엔진 기본 사각형
- `SphereShape`: 엔진 기본 구
- `PyramidShape`: Client에서 `Shape`를 상속해 만든 사용자 정의 도형

사용자 정의 Shape는 생성자에서 정점 속성과 인덱스를 만들고 보호 함수
`SetMeshData`로 넘기면 된다. Shape에는 D3D12 객체가 들어 있지 않다.

```cpp
class PyramidShape final : public mrg::geometry::Shape
{
public:
    PyramidShape()
    {
        std::vector<mrg::geometry::VertexAttributes> vertices = /* ... */;
        std::vector<std::uint32_t> indices = /* ... */;
        SetMeshData(std::move(vertices), std::move(indices));
    }
};
```

Scene 초기화에서 원하는 GPU 정점 형식을 템플릿 인자로 선택한다. 아래 코드는
Shape의 좌표와 색상만 `GpuMesh`에 넣는다.

```cpp
const mrg::geometry::SphereShape shape(1.15F, 32, 20);
const auto mesh = services.meshRendering.CreateMesh<
    mrg::geometry::VertexPositionColor>(shape);
const auto material = services.meshRendering.CreateMaterial(
    mrg::graphics::BuiltInMaterial::UnlitVertexColor);

mrg::scene::MeshInstance instance;
instance.SetMesh(mesh);
instance.SetMaterial(material);
instance.SetColor({1.0F, 0.3F, 0.7F, 1.0F});
instance.Transform().SetPosition(0.0F, 0.0F, 0.0F);
```

`Render`에서는 `Submit`만 호출한다. 이 호출은 즉시 Draw Call을 실행하지 않고
인스턴스 정보를 모은다. 엔진은 프레임 끝에서 같은 Mesh/Material 조합을 묶어
실제 D3D12 명령을 기록한다.

```cpp
instance.Submit(context, camera);
```

## 2. 렌더 Mesh와 충돌체 연결

[CollisionExampleScene.cpp](../Client/GameScene/Examples/CollisionExampleScene.cpp)는
자동으로 왕복하는 구 Mesh와 Y축으로 회전된 상자 Mesh를 보여준다. `Left`와
`Right`로 구의 이동 중심을 보정하고 `R`로 보정값을 초기화할 수 있다.

엔진은 Mesh 모양을 보고 충돌체를 암묵적으로 만들지 않는다. 게임 규칙에 맞는
충돌체를 Scene이 명시적으로 만들고 렌더 Transform과 같은 값으로 갱신한다.
이 분리는 렌더 메시보다 단순하고 안정적인 판정용 모양을 사용할 수 있게 한다.

```cpp
const mrg::collision::Sphere3D sphereCollider{
    movingCenter,
    MovingSphereRadius};
const mrg::collision::Obb3D boxCollider{
    fixedBoxCenter,
    fixedBoxHalfExtents,
    fixedBoxOrientationQuaternion};

const bool colliding =
    mrg::collision::Intersects(sphereCollider, boxCollider);
```

충돌하면 두 `MeshInstance`가 적색, 분리되면 청색/녹색으로 바뀐다. 충돌 API는
D3D12에 의존하지 않으므로 Update에서 여러 번 호출해도 렌더 주기와 결합되지
않는다.

## 3. 위젯 동적 추가와 삭제

[WidgetExampleScene.cpp](../Client/GameScene/Examples/WidgetExampleScene.cpp)는
`ADD WIDGET`/`REMOVE LAST` 버튼과 `A`/`D` 키로 Canvas의 자식을 변경한다.

추가는 부모의 `EmplaceChild`를 사용한다. 반환된 참조는 초기 설정에만 쓰고,
Scene은 이후 검색과 삭제를 위해 `UiElementId`를 저장한다.

```cpp
auto& widget = container.EmplaceChild<mrg::ui::UiButton>(L"DYNAMIC");
widget.SetBounds({8.0F, 6.0F, 228.0F, 40.0F});
dynamicWidgetIds.push_back(widget.Id());
```

삭제하기 전에는 입력 라우터를 Reset한다. 그러면 제거될 객체를 hover/capture
대상으로 기억하지 않는다. `RemoveChild`는 해당 객체의 직접 부모에서 호출하며,
Canvas가 소유하던 `unique_ptr`가 제거되면서 위젯이 파괴된다.

```cpp
inputRouter.Reset(canvas);
container.RemoveChild(dynamicWidgetIds.back());
dynamicWidgetIds.pop_back();
```

마우스 상태는 `UiPointerInput`으로 변환해 라우터에 전달하고, 버튼 동작은
Canvas의 Action 큐로 받는다.

```cpp
inputRouter.Process(canvas, pointer);
for (const mrg::ui::UiAction& action : canvas.TakeActions())
{
    if (action.type == mrg::ui::UiActionType::Clicked)
    {
        // action.source의 UiElementId로 동작을 구분한다.
    }
}
```

## Scene 등록과 자동 검증

세 경로는 [ColoredCubeGame.cpp](../Client/App/ColoredCubeGame.cpp)에서 같은 등록
함수를 사용한다.

```cpp
scenes.RegisterScene<MeshExampleScene>(
    "Example.Mesh",
    mrg::scene::SceneRetention::DestroyOnExit);
```

화면에서 숫자 키를 누르지 않아도 각 초기화/렌더 경로를 숨김 창으로 검사할 수
있다.

```powershell
.\bin\x64\Debug\MyRhythmGame.exe --smoke-test --example=mesh
.\bin\x64\Debug\MyRhythmGame.exe --smoke-test --example=collision
.\bin\x64\Debug\MyRhythmGame.exe --smoke-test --example=widgets
```
