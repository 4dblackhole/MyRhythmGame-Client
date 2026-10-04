# 차트 에디터

타이틀 → Editor → 곡/난이도 선택으로 진입합니다. Penpot `Editor UI`의 Pattern,
Realtime View, Time Signature, Metadata, Effects 보드를 기준으로 구성했습니다.
일반 실행은 여전히 로고 화면에서 시작합니다.

수정 시 `Client/EditorScene/EditorScene.*`는 조립 흐름만 확인합니다.
`Submodules/EditorWorkspace`는 문서와 모드의 수명 및 공통 편집 상태를,
`EditorView`와 각 `Editor*View.cpp`는 공통 탭/타임라인/분석 화면을 맡습니다.
`EditorInput`은 공통 입력을 연결하고 모드별 좌표 해석은 모드 객체에 위임합니다.
`EditorAnalysisController`는 worker와 marker 캐시를 맡습니다.
PCM/FFT 자체는 `FingerDrum.Editor/Audio/EditorAudioAnalysis`만 읽으면 됩니다.
문법 변경이 없으면 다른 형식 문서와 엔진 내부를 읽을 필요가 없습니다.

## 조작

- 작은/큰 노트 도구를 좌클릭해 선택하고 우클릭 팝업에서 동·캇·보라 변형을 선택합니다.
- 롤/풍선 도구 우클릭에서 Roll, TickRoll, 큰 변형, Balloon, DengDeng, Don/Kat Buzz를 선택합니다.
- 악보 또는 실시간 레인 좌클릭으로 추가, 노트 우클릭으로 삭제합니다. 롱노트는 두 번
  클릭합니다. 도구를 바꾸거나 Escape를 누르면 미완성 배치를 취소합니다.
- 선택 도구 클릭은 현재 시간을 변경합니다. 휠은 악보 마디를 이동하고 좌우키는 1ms 이동합니다.
- 악보 화면의 박자 디바이더는 4분음표를 기준으로 나눕니다. 1/2는 8분음표,
  1/4는 16분음표이며 기본값은 1/4입니다. 슬라이더로 1/1부터 1/16까지
  정수 분모 눈금을 선택하고, 1/17부터 1/1024까지는 직접 입력합니다.
  YMP에 저장되는 노트 위치의 분수는 그대로 온음표 기준입니다.
- 실시간 뷰는 오디오 영역의 현재 시간(ms)을 판정선 위치로 사용합니다. 차트 offset과
  delay를 포함한 노트의 컴파일 시각 차이로 간격을 계산합니다. 이동 속도는
  Base BPM으로 고정하며, Base BPM에서 16분음표 간격은 일반 노트 지름의 85%입니다.
  노트별 스크롤 배율도 반영합니다.
- 패턴 탭의 기존 오디오 분석 영역은 긴 타임라인 슬라이더입니다. 트랙을 클릭하거나
  손잡이를 드래그해 현재 편집 시각을 ms 단위로 바꾸며 실시간 뷰와 공유합니다.
  범위는 음원 전체 길이와 차트 노트 시각을 포함하고 음수 노트 시각도 탐색합니다.
- 오디오 탭은 음악 파형, 음악 FFT 스펙트로그램, 예상 히트사운드 FFT 스펙트로그램의
  세 영역을 같은 시간축으로 표시합니다. 하단 타임라인/현재 시간 입력으로 탐색하며,
  +/− 버튼은 현재 시각을 기준으로 보이는 시간 범위를 확대/축소합니다.
- 박자표에서 마디/위치/BPM과 마디 길이를 편집합니다. 마디 밖으로 넘친 노트는 초과분을
  다음 마디로 이월합니다. 마디 밖 BPM/이펙트가 생기는 변경은 데이터 손실 대신 오류를 표시합니다.
  롱노트 시작과 끝이 역전되거나 같은 위치가 되는 변경도 전체 취소하고 원본을 보존합니다.
- 메타데이터에서 YMM 상대경로, 이름, 제작자, 태그, offset, 기본 BPM, 히트사운드 표를 편집합니다.
- 이펙트 목록 선택 후 같은 위치의 값을 수정하거나 삭제할 수 있습니다. 동/캇 변경은 각각
  독립 지시문입니다. 각 입력 필드는 Unicode 입력을 지원하는 Windows 텍스트 창에서 편집합니다.
- Ctrl+S 또는 저장 버튼으로 YMP와 같은 폴더의 같은 이름 YME를 저장합니다. 미저장 상태에서
  Escape를 누르면 폐기 여부를 확인합니다. 저장 전 파싱 왕복 검증 및 두 파일의 임시/복구본을
  사용합니다. 저장 실패로 `.editor.tmp`/`.editor.bak`가 남으면 원본 복구를 확인한 뒤 제거하세요.

## 내부 구조와 검증 경계

### 모드와 파일 형식의 교체 경계

```text
EditorScene
  EditorWorkspace
    IEditorMode                 도구/배치/그리기/좌표/히트사운드 정책
      Modes/Taiko/TaikoEditorMode
    IEditorDocument             공통 편집용 데이터와 파일 형식별 수정/저장
      ChartEditor               현재 YMP/YME 구현
    EditorAnalysisController    모드가 제공한 파일/marker의 비동기 분석
  EditorView                    공통 탭/박자 디바이더/타임라인/FFT 화면
    IEditorModeCanvas           모드에 제공하는 Box/Text/Button 그리기 계약
```

`EditorModeFactory`가 진입 요청의 `mode`에 맞는 객체를 생성합니다. 현재 등록된
모드는 Taiko 하나이고 빈 모드 ID는 기존처럼 Taiko입니다. 미지원 ID 또는
Taiko로 열었는데 다른 모드가 기록된 문서는 오류로 거부하며 Taiko 도구로 덮어쓰지 않습니다.
활성 에디터 안에서 모드를 바꾸는 UI는 제공하지 않습니다.

Taiko의 도구 ID/그룹/선택 상태, 미완성 롱노트, 팝업, 노트 색/모양,
악보/실시간 레인, 노트 hit-test/스냅/마디 스크롤, YMP 메타데이터 편집과
동·캇/틱 사운드 정책은 `Modes/Taiko/`가 소유합니다. 도구 문구와 동·캇 이펙트
문구는 `Client/Texts/EditorScene/Taiko/`에 있습니다. 공통 화면에는 Taiko
노트 enum/숫자 의미나 도구 개수/그룹을 넣지 않습니다. 이펙트의 공통 자동화
편집 UI는 유지하고 사운드 대상 목록/표시는 모드에서 받습니다.

새 모드를 추가할 때는 `Modes/<Mode>/`의 `IEditorMode` 구현과 문구 표를
만들고 factory에 연결합니다. 도구 개수와 그리기 모양/방향은 모드가 결정하며,
`DrawChart`에서 만든 좌표를 해당 모드의 `EditScore`가 해석합니다.
`OpenDocument`는 파일 형식에 맞는 `IEditorDocument`를 반환합니다.
UI 그리기는 전달받은 Canvas를 해당 호출에서만 사용하고, Button 콜백은
활성 Workspace/모드의 수명 안에서 실행됩니다. 모드가 Scene 전환을 호출하지 않습니다.
`AudioFiles`의 `Music` 키는 음악 파형/FFT용이며 나머지 키는
`AudioMarker.sound`와 대응하는 히트사운드입니다. 파일 목록은 worker에 복사합니다.

`IEditorDocument`의 Pattern/Effects/Notes/Timeline은 **편집용 공통 표현**입니다.
새 파일 형식은 adapter가 원본의 추가 필드/지시문을 보관하고,
수정·검증·롱노트 연결·저장을 그 형식의 규칙에 따라 구현해야 합니다.
공통 표현으로 변환한 뒤 YMP로 저장하는 계약이 아닙니다.
현재 `ChartEditor`의 동·캇 키 검증과 롱노트 연결 규칙은 현재 YMP 편집 구현에
남아 있으며, 새 형식이 이를 상속하거나 그대로 재사용할 필요는 없습니다.
메타데이터 화면/오디오 파일 해석도 모드가 소유하므로 YMM 경로를 강제하지 않습니다.

BMS 파싱·저장, 7키/스크래치 배치와 전용 도구는 아직 구현하지 않았습니다.
나중에 BMS를 실제로 지원할 때는 위 모드/문서 adapter 외에도 현재 YMM/YMP
중심인 곡 카탈로그의 검색·진입 연결을 추가해야 합니다. 이번 변경은 에디터 내부의
교체 경계를 마련한 것이며 파일 지원이 추가된 것은 아닙니다.

### 공통 처리와 검증

`FingerDrum.Chart/Editing/ChartEditor`는 `IEditorDocument`의 현재 구현으로
엔진 비종속 편집 상태와 YMP/YME 저장을 소유합니다.
노트 위치·마디 길이는 Rational이며 수정할 때만 MusicalTimeline의 누적합과 정수 us 캐시를
재생성합니다. 화면 좌표를 위한 부동소수점은 렌더링/마우스 스냅 경계에서만 사용합니다.

`EditorScene`은 ScreenVisual2DManager의 Canvas에 draw packet component를 등록합니다.
박자 디바이더는 같은 Canvas의 엔진 `CreateSlider`와 `Visual2DInputRouter`를 사용합니다.
화면 비율이 좁아져도 Penpot 작업영역이 잘리지 않게 동일 비율로 축소합니다.
`EditorAudioAnalysis`는 Media Foundation으로 실제 음악/히트사운드 PCM을 디코딩하고
2048-point Hann FFT를 계산합니다. 96개 로그 주파수 구간은 20Hz부터 최대
20kHz(파일의 Nyquist 이하)까지이며, 색상은 −90~0dBFS 범위입니다.
스테레오 등 다채널 음원은 채널별 FFT 전력을 평균해 위상 상쇄로 사라지지 않습니다.
파형은 각 10ms 구간의 모든 채널 최솟값/최댓값을 표시합니다.
작업 스레드는 Scene/renderer에 접근하지 않습니다.
YMP의 `Music metadata`는 Songs 루트 기준으로 해석한 뒤 YMM이 가리키는 음원을 분석합니다.
음악 파형은 녹색이고 음악/히트사운드 스펙트럼은 각각 검정·보라·적색·황색 강도 팔레트를
사용합니다. 예상 노트 시작과 TickRoll/Buzz 틱은 실제 컴파일 시각에 배치합니다.
줌을 축소해도 짧은 타격을 놓치지 않도록 시간/주파수 구간의 최대 강도를 유지합니다.
겹치는 히트사운드도 구간별 최대 강도로 표시하며 실제 믹스된 오디오의 FFT는 아닙니다.
틱은 모두 정확히 처리한 경우를 표시하며, 동·캇 자유 선택인 TickRoll은 동을 기준으로 합니다. 자유 연타의
실제 타격 시점/풍선 파열 시점은 편집 단계에서 결정되지 않으므로 임의로 생성하지 않습니다.

현재 오디오 영역은 분석과 수동 시간 탐색용이며 음악 재생·배속 컨트롤은 제공하지 않습니다.
Undo/Redo, 드래그 이동,
다중 선택, 신규 곡 생성은 이번 요청에 포함되지 않아 구현하지 않았습니다.

`--smoke-editor`는 외부 Songs의 실제 차트를 읽어 숨겨진 에디터를 렌더링하는 검증 경로입니다.
기본 실행 동작과 배포 데이터는 바꾸지 않습니다. 리듬 테스트에는 마디 이월, 유리수 누적합
역변환, YMP/YME 저장 왕복, 박자 기반 속도 보간, 동·캇 변경 경계 및 실제 MP3 분석을 포함합니다.
오디오 분석 회귀는 반대 위상의 스테레오 1kHz 신호의 Hz/dBFS와 무음을 검사합니다.
별도 [오디오 뷰 회귀](../Tests/Presentation/README.md)는 Client object를 사용해 실제 곡의
세 영역 draw packet, 타임라인 범위/슬라이더 action, 탭별 숨김, 언어 전환과 정리를 검사합니다.
같은 실행 파일의 `EditorModeTests`는 Taiko 전체 도구의 ID/롱노트/버즈 옵션/예상
히트사운드, 악보 스냅/삭제/취소를 검사합니다. 테스트에만 존재하는 다른 모드와
문서 adapter로 도구/세로 그리기/좌표/메타데이터/사운드 대상/저장/marker 캐시가
공통 화면에 연결되는지도 검사합니다. 이 테스트 모드는 게임에 등록하거나 배포하지 않습니다.

## 이전 기능 검증의 한계

이전 세션에서는 Windows 화면 캡처가 `SetIsBorderRequired ... 0x80004002`로 실패해
실제 화면의 시각적 확인과 마우스 조작 검증을 완료하지 못했습니다.
현재 자동 검증 명령은 [Verification](Verification.md), 기능별 테스트는
[변경 영향 지도](ChangeImpactMap.md)가 기준입니다. smoke 통과를 시각적 검증으로 대체하지 않습니다.
