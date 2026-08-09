# MyRhythmGame-Client 문서 안내

이 문서는 새 개발자와 새 Codex 세션이 현재 Client 예제와 연결된 엔진을 빠르게
찾아가기 위한 목차다.

## 권장 읽기 순서

1. 저장소 루트의 `AGENTS.md`
2. [엔진 전체 안내서](../Dependencies/MRG-Engine/Docs/EngineOverview.md)
3. [실행 흐름과 객체 수명](ExecutionFlow.md)
4. 작업에 해당하는 기능 문서

## Client 문서

| 문서 | 내용 |
| --- | --- |
| [ExecutionFlow.md](ExecutionFlow.md) | 진입점, 엔진 초기화, Update/Render, Scene 수명과 종료 |
| [EngineFeatureExamples.md](EngineFeatureExamples.md) | 숫자 1/2/3으로 실행하는 Mesh·충돌·위젯 예제 |
| [Visual2DGuide.md](Visual2DGuide.md) | Sprite·위젯 component, Canvas, 입력, PNG와 Z-Order |
| [Visual2DExample.md](Visual2DExample.md) | 화면·곡면 Canvas와 오디오 설정 패널 구현 |
| [PerformanceOverlay.md](PerformanceOverlay.md) | Client 소유 FPS/UPS 표시 |

엔진 자체의 Audio, Collision, Text, Visual2D 내부 설계 문서는
`Dependencies/MRG-Engine/Docs`에 있다. 전체 링크는 엔진의
`Docs/EngineOverview.md` 문서 색인에서 확인한다.

## 작업 위치를 정하는 기준

- 게임 규칙, Scene ID, 게임별 asset·문구·패널 배치는 이 Client에 둔다.
- 다른 게임에서도 같은 의미로 쓰일 backend-neutral 계약은 MRG-Engine에 둔다.
- D3D12와 FMOD 세부 구현은 각각 엔진의 Graphics와 Audio backend에 둔다.
- Client는 엔진의 `MRG_Core.h`만 포함하고 `MRG.Core.vcxproj` 하나만 참조한다.
- 엔진을 수정해야 한다면 독립 `MRG-Engine` 저장소에서 먼저 작업·병합하고,
  마지막에 `Dependencies/MRG-Engine` gitlink를 갱신한다.

## 현재 예제를 찾는 위치

| 목적 | 코드 위치 |
| --- | --- |
| 프로그램 진입과 CRT leak 검사 | `Client/App/Main.cpp` |
| 엔진 설정·Scene 등록·성능 표시 | `Client/App/ColoredCubeGame.*` |
| 큐브, 화면/곡면 UI, 오디오 패널 | `Client/GameScene/ColoredCubeScene.*` |
| 게임별 Scene route | `Client/GameFlow/SceneIds.h` |
| Mesh 예제 | `Client/GameScene/Examples/MeshExampleScene.*` |
| 충돌 예제 | `Client/GameScene/Examples/CollisionExampleScene.*` |
| Visual2D 위젯 예제 | `Client/GameScene/Examples/WidgetExampleScene.*` |

구현 전에는 관련 문서뿐 아니라 현재 코드를 함께 확인한다. 문서는 의도와 경계를
설명하고, 정확한 public signature는 submodule의 생성된 `MRG_Core.h`가 기준이다.
