# FingerDrum 새 세션 인수인계

이 문서는 긴 채팅 기록을 다시 읽지 않고 현재 작업을 이어가기 위한 요약입니다.
구체적인 API와 동작은 연결된 기능 문서 및 현재 소스가 최종 기준입니다.

## 새 세션에서 먼저 할 일

1. 저장소 루트의 `AGENTS.md`를 읽습니다.
2. 이 문서를 읽고 작업이 Engine, Client, Design 중 어디에 속하는지 정합니다.
3. Client 작업이면 `Docs/ExecutionFlow.md`와 관련 기능 문서를 읽습니다.
4. Engine 작업이면 `Dependencies/MRG-Engine/Docs/EngineOverview.md`와 해당 엔진
   기능 문서를 읽습니다.
5. `git status -sb`, Client `origin/main`, 엔진 gitlink/HEAD/`origin/main`을
   확인한 뒤 작업 브랜치를 만듭니다. 이 문서에 적힌 상태보다 Git이 우선합니다.

## 프로젝트와 저장소

- 게임 이름은 **FingerDrum**이며 Windows, Visual Studio 2022, C++20,
  DirectX 12를 사용합니다.
- Client 비공개 저장소는 `4dblackhole/MyRhythmGame-Client`입니다.
- 재사용 엔진 비공개 저장소는 `4dblackhole/MRG-Engine`이며 Client의
  `Dependencies/MRG-Engine` 서브모듈로 고정됩니다.
- 두 저장소를 함께 다룰 때는
  `D:\Projects\c++\asdf\MyRhythmGame-Client`에서 시작합니다.
- 과거 DX11 구현과 게임 규칙 참고 자료는
  `D:\Projects\c++\asdf\RPG`에 있습니다. 참고만 하고 새 코드의 소유 경계와
  API를 과거 구조에 그대로 종속시키지 않습니다.

## 엔진 상태와 경계

- 엔진은 `MRG.Core.lib` 하나와 생성된 공개 헤더 `MRG_Core.h` 하나로 Client에
  제공됩니다. Client는 이 헤더와 `MRG.Core.vcxproj`만 참조합니다.
- 엔진은 Win32 창/Raw Input, D3D12, FMOD 오디오, Scene, Camera,
  TransformNode, Mesh/Material/instancing, 충돌, 텍스트, Visual2D를 포함합니다.
- FMOD 구현은 교체 가능한 오디오 backend 뒤에 있습니다. AUTO/WASAPI/ASIO,
  출력 장치와 DSP buffer를 다루며 게임별 WAV handle은 Client가 소유합니다.
- Visual2D는 Sprite와 위젯을 별도 상속 계층으로 나누지 않습니다.
  `Visual2DNode`에 이미지, 텍스트, collider, pointer receiver, button/combobox
  behavior를 component로 조합합니다.
- 엔진이 D3D12 Visual2D renderer 하나를 소유합니다. Canvas는 필요한 영역만
  가질 수 있고 Canvas Z-order 후 내부 트리 Z-order를 비교합니다.
- 평면·곡면 UI는 surface mapping으로 Canvas 좌표를 얻습니다. 곡면 pointer
  질의는 mesh UV와 가속 구조를 사용합니다.
- 엔진을 수정할 때 Client의 서브모듈 checkout에서 임의 커밋하지 말고 엔진
  저장소 브랜치와 PR을 먼저 병합한 다음 Client gitlink를 갱신합니다.

## Client 실행 흐름

```text
wWinMain
└─ FingerDrumGame
   └─ SceneManager
      ├─ FingerDrumLogoScene       KeepAlive
      ├─ LobbyScene                KeepAlive
      └─ RhythmTestScene           DestroyOnExit
```

- Logo는 AliceBlue 배경과 세 로고 이미지를 표시합니다. `Game Start`는 Lobby,
  `Exit`는 정상 종료입니다.
- Lobby는 YMM/YMP catalog를 읽는 곡 선택 화면입니다. 현재 카테고리는 `ALL`,
  로컬 기록은 `NO RECORDS`, 프로필은 임시 이미지입니다.
- 곡 목록 행에는 번호나 BPM 없이 실제 곡명과 아티스트만 표시합니다. 행 배경이
  입력을 받고 곡명/아티스트는 별도 Text 자식입니다. 긴 곡명은 marquee로
  표시합니다.
- Lobby가 `GameplayLaunchRequest`에 music/pattern/optional YME/mode를 기록한
  뒤 gameplay Scene 전환을 요청합니다.
- Gameplay Scene은 진입 시 동적 생성되고 ESC 또는 패턴 종료 3초 뒤 Lobby로
  돌아가며 삭제됩니다. 결과 Scene은 아직 설계되지 않았습니다.
- F7은 `FingerDrumGame`이 소유하는 FPS/UPS 표시를 토글합니다.

## 리듬게임 도메인

- 한 play session은 `RhythmTimer` 하나를 사용합니다. QPC 입력 timestamp와
  FMOD DSP 예약을 같은 리듬 시간에 연결하고 render 시간은 판정에 사용하지
  않습니다.
- `AccuracyRange`는 MAX, Perfect, Great, Good, Bad 범위 사이 점수를 선형
  보간합니다. 판정 레벨 50의 기본 ms 범위는 4.5, 9.5, 23.5, 54.5, 90이며
  레벨에 반비례해 축소됩니다.
- `Lane`은 노트를 시간순으로 소유하고 맨 앞 미처리 노트의 focus 및
  early/late Bad 말림 방지 규칙을 담당합니다.
- 노트 처리 조건은 교체 가능한 rule로 표현합니다. 일반, 동시 입력, 순차 입력,
  큰 노트, Roll/TickRoll, Balloon, DengDeng, Buzz 등의 상태에 따라 semantic `NoteEvent`와
  `AudioCueRequest`를 발생시킵니다.
- 게임 규칙은 FMOD를 직접 호출하지 않습니다. `GameplayAudioRouter`가 cue를
  clip, bus, volume, effect에 연결합니다.
- YMM은 음악 metadata, YMP는 playable score, YME는 시각·오디오 automation을
  담당합니다. beat/BPM 위치는 로드 시 정수 microseconds로 컴파일합니다.
- 새 게임 모드는 `IPlayGameMode`를 구현하고 차트를 실제 note로 만드는 정책을
  소유합니다. 현재 `TaikoMode`와 `RhythmTestScene`은 테스트 드라이버입니다.

## 현재 태고 테스트 표시

- Don, Kat, BigDon, BigKat과 모든 태고 롱노트를 한 Lane에서 관리합니다.
- `--rhythm-debug`는 엔젤드림 롱노트 테스트 YMP를 열고 1ms 수동 타이머와
  현재 대상 노트 디버그 텍스트를 제공합니다.
- RPG의 `Files/Skins/test Skin`에서 옮긴 Ambient/Overlay 및 hit sound를
  사용합니다.
- Lane은 기본 로컬 `+Y` 방향의 세로 계층으로 작성하고 Taiko에서는 부모만
  Z축 `-90°` 회전합니다. 배경, light, judge, note, long-note part가 그 회전을
  상속하고 note 이동은 로컬 Y만 갱신합니다.
- `LaneBackground.png`는 늘이지 않고 원본 비율로 반복 배치합니다.

## Penpot 디자인 상태

- 파일 ID: `3be9e5e1-190f-8090-8008-7468d9cc1bc8`
- 페이지 ID: `3be9e5e1-190f-8090-8008-7468d9cc1bc9`
- 페이지: `Music Select · Sky`
- 기본 화면과 게임 모드/Modifier 팝업 상태의 배경은 동일한 실제 5곡 목록,
  `ALL`, `NO RECORDS`, 임시 프로필을 사용합니다.
- 최근 저장 버전은 `Song list title and artist refresh`입니다.
- Penpot 팝업 디자인이 존재한다고 해서 해당 팝업 기능이 Client에 구현됐다고
  가정하지 않습니다. 요청된 범위와 실제 코드부터 확인합니다.

## 아직 완성되지 않은 범위

- `RhythmTestScene`과 `TaikoMode`는 최종 FingerDrum 플레이 화면이 아닙니다.
- 결과 Scene 및 영구 기록 파일은 아직 없습니다.
- Penpot의 게임 모드/Modifier 팝업은 디자인 시안이며 Client 기능 범위는 별도
  확인이 필요합니다.
- YME automation과 실시간 effect의 확장점은 있지만 모든 지시문과 편집 UI가
  구현된 것은 아닙니다.
- 모호한 요청을 받았다고 이 항목들을 자동으로 구현하지 않습니다.

## 검증과 병합

Client 변경 후 Debug/Release x64에서 다음을 모두 수행합니다.

```powershell
msbuild MyRhythmGame-Client.sln /m /t:Build /p:Configuration=Debug /p:Platform=x64
msbuild MyRhythmGame-Client.sln /m /t:Build /p:Configuration=Release /p:Platform=x64
bin\x64\Debug\FingerDrum.Rhythm.Tests.exe --catalog-root Client\Assets\Songs
bin\x64\Release\FingerDrum.Rhythm.Tests.exe --catalog-root Client\Assets\Songs
```

두 구성의 `MyRhythmGame.exe`에 `--smoke-test`, `--smoke-lobby`,
`--smoke-gameplay`도 각각 실행합니다. 재사용 엔진 기능은 추가로
`Client/Examples/ColoredCube`의 `ColoredCubeGame`과 관련 예제 route를 사용합니다.
Mesh·Camera·렌더 변경은 `--smoke-test --example=mesh`가 기본 통합 경로입니다.
검증 후 작업 PR을 병합하고 Client main, 엔진 gitlink, 엔진 main이 일치하는지
확인합니다.

현재 사용자 소유로 남아 있을 수 있는 다음 파일은 별도 요청 없이 stage하거나
덮어쓰지 않습니다.

- `Client/Assets/Images/Logo/LeftFade.png`
- `Client/Assets/Images/Logo/RightFade.png`
- `Client/Assets/Songs/Music/angel dream hand shaking.mp3`
- `TODOLIST.txt`

## 세션 분리 권장안

토큰 절약만이 목적이면 **새 메인 세션 하나**가 기본 권장안입니다. 세 세션은
공통 문서를 각각 읽어야 하므로 총 토큰이 줄기보다 늘 수 있고, 현재는 엔진 API와
Client 사용부가 함께 바뀌는 일이 많아 통합 비용도 큽니다.

필요할 때만 다음 역할을 별도 세션으로 엽니다.

| 역할 | 수정 범위 | 금지 사항 |
| --- | --- | --- |
| Engine | `MRG-Engine` 저장소, 공개 API와 엔진 문서 | Client 게임 규칙·Scene·asset 수정 |
| Client | `MyRhythmGame-Client`의 게임 코드·문서·asset, 병합된 엔진 gitlink | 서브모듈의 미병합 엔진 코드 직접 수정 |
| Design | Penpot 파일과 명시적인 design handoff | 승인 없이 코드 기능·가상 데이터 생성 |

세 역할을 동시에 사용할 때는 같은 파일을 편집하지 않고 각자 독립 브랜치 또는
worktree를 사용합니다. 통합 순서는 **Engine PR 병합 → Client gitlink 및 사용부
변경 → Client PR 병합**입니다. Design은 node 이름, 좌표, 상태, asset과 실제
구현 범위를 문서로 넘기고 Client가 그 계약을 적용합니다.

## 새 세션 시작 문구

일반 작업은 다음 정도면 충분합니다.

> `D:\Projects\c++\asdf\MyRhythmGame-Client`의 `AGENTS.md`와
> `Docs/SessionHandoff.md`를 먼저 읽고 현재 Git/서브모듈 상태를 확인하세요.
> 요청 범위만 최소 변경으로 구현하고 관련 검증과 PR 병합까지 수행하세요.

전용 세션을 열 때는 위 문구 뒤에 `이번 세션은 Engine만`, `Client만`, 또는
`Penpot Design만 수정`이라고 소유 범위를 한 줄로 추가합니다.
