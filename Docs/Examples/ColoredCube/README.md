# ColoredCube 보관 예제

이 폴더의 문서는 기존 DX12 기술 샘플을 보관한다. 실제 FingerDrum 실행 경로는
이 예제를 등록하거나 시작하지 않는다.

- [기존 실행 흐름](ExecutionFlow.md)
- [Mesh·충돌·위젯 예제](EngineFeatureExamples.md)
- [Visual2D 예제](Visual2DExample.md)
- [FPS/UPS overlay 예제](PerformanceOverlay.md)

대응하는 소스는 `Client/Examples/ColoredCube/`에 있으며 Debug에서만 빌드한다.
큐브 텍스처에는 내장 스킨의 위젯/선택 커서 PNG를 사용한다. `Client/Assets/Unused`의
PNG는 예제에서도 사용하지 않는다.

기존 `--smoke-test --example=mesh|collision|widgets` 경로는 첫 화면을 렌더한 뒤
Blank Scene으로 전환하면서 공통 AudioPlaybackManager의 재생 유지를 확인한다.
기존 pop.wav를 일시정지 상태로 재생하고 로컬 Clip 참조를 해제한 뒤에도 Voice가
유효한지, 전환 후 재개·명시적 정지가 가능한지 검증한다.
Widget route는 `ScreenVisual2DManager`가 소유한 Canvas를 사용해 Scene의 직접
`SubmitScreen` 호출 없이 화면 표시·resize·정리를 함께 검증한다.
