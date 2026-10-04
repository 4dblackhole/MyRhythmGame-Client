# 빌드와 검증

코드 변경 후 저장소 루트에서 실행합니다. MSBuild는 설치된 Visual Studio의
vswhere로 찾고 FMOD SDK 경로는 Directory.Build.props의 설정을 사용합니다.

```powershell
git submodule update --init --recursive
msbuild MyRhythmGame-Client.sln /m /t:Rebuild /p:Configuration=Debug /p:Platform=x64
msbuild MyRhythmGame-Client.sln /m /t:Rebuild /p:Configuration=Release /p:Platform=x64
```

각 구성(Debug/Release)에서 아래 명령을 실행합니다.

```powershell
.\bin\x64\Debug\FingerDrum.Rhythm.Tests.exe --catalog-root FingerDrum.Assets/Assets/Songs
.\bin\x64\Debug\MyRhythmGame.exe --smoke-test
.\bin\x64\Debug\MyRhythmGame.exe --smoke-lobby
.\bin\x64\Debug\MyRhythmGame.exe --smoke-gameplay
.\bin\x64\Debug\MyRhythmGame.exe --smoke-editor
```

PowerShell에서 Windows GUI 실행 파일을 직접 호출하면 `$LASTEXITCODE`가 비어
있을 수 있습니다. 종료 코드를 판정할 때는 다음처럼 실행하고 `Debug`를
`Release`로 바꿔 같은 경로를 반복합니다.

```powershell
$exe = (Resolve-Path .\bin\x64\Debug\MyRhythmGame.exe).Path
foreach ($route in '--smoke-test', '--smoke-lobby', '--smoke-gameplay', '--smoke-editor') {
    $process = Start-Process -FilePath $exe -ArgumentList $route -WorkingDirectory (Split-Path $exe) -WindowStyle Hidden -Wait -PassThru
    if ($process.ExitCode -ne 0) { throw "$route failed: $($process.ExitCode)" }
}
```

재사용 엔진 변경은 Debug 빌드의 ColoredCubeGame 관련
`--example=mesh|collision|widgets` route와 엔진 자체 테스트도 실행합니다.
예: `--smoke-test --example=mesh`. Release에는 이 예제 route가 없습니다.
프로젝트/필터/include 경계는 `Scripts/CheckArchitecture.ps1`로 검사합니다.
키빔 표시는 [별도 회귀 테스트](../Tests/Presentation/README.md)로 검사할 수 있습니다.
오디오 탭/타임라인 변경은 같은 문서의 Editor audio view regression을 두 구성에서 실행합니다.
에디터 모드/문서 계약 변경도 같은 실행 파일에 연결된 EditorModeTests를 두 구성에서 실행합니다.
UI smoke는 초기화·실행·정리 검증이며 실제 모양/사용자 조작 검증과 구분합니다.
전체 노트/Buzz 회귀는 `--catalog-root` 로직 테스트와
[All-notes actual gameplay replay](../Tests/Presentation/README.md)를 두 구성에서 실행합니다.

커밋 전 `git diff --check`, 사용자 파일 제외 여부, Client/엔진 status와 upstream을
확인합니다. main 직접 작업은 검증 후 commit/push하며 브랜치 요청 시에만 PR을 사용합니다.
FMOD SDK·DLL·import lib는 커밋하지 않습니다.

## 2026-10-04 에디터 모드/문서 책임 분리 검증

- Debug/Release x64 솔루션 전체 Rebuild 성공.
- 두 구성의 로직·카탈로그 테스트 성공(실제 제공곡 1곡/3패턴).
- 두 구성의 smoke-test, smoke-lobby, smoke-gameplay, smoke-editor 종료 코드 0.
- 두 구성의 EditorAudioViewTests/EditorModeTests 성공. 실제 음원 FFT/타임라인,
  Taiko 도구·롱노트·버즈·사운드·스냅·삭제·취소와 다른 모드/문서 adapter 연결을 검사.
- 8개 프로젝트의 소스/필터, 의존성 순환, 엔진 경계와 git diff --check 통과.
- 실제 화면 배치·마우스 조작·청취는 수동 확인하지 않음. 테스트용 모드는
  게임에 등록하지 않았고 BMS/7키 실제 채보 지원을 검증한 것은 아님.

## 2026-10-04 AngelDream 전체 노트/Buzz 검증

- 기본 Songs에 `angeldream [all notes test].ymp` 추가. 기존 음원/YMM을 참조하며
  12종 노트 ID와 Don/Kat Buzz 변형, 총 19개 논리 노트를 사용합니다.
- Debug/Release x64 솔루션 전체 Rebuild 성공. 두 구성 로직·카탈로그 테스트 성공
  (제공곡 1곡/4패턴), 각 구성의 smoke-test/lobby/gameplay/editor 종료 코드 0.
- 두 구성에서 GOOD 경계(-54.5/0/+50/+54.5/+54.501ms), 틱 시각 ±1µs,
  Don/Kat, 분할 64/256/1024, 작은 프레임/한 번의 갱신 등 63개 조건 통과.
  틱 시각/인덱스, 누락·중복·소급 성공 없음, 실패 틱의 TickSound 없음과
  정확도 1회 확정을 검사합니다.
- 두 구성에서 실제 YMP의 전체 세션 replay 성공. +50ms Buzz의 실패/성공 틱은
  2/29, 9/118, 38/473개이며 헤드/몸통 점수까지 검증합니다.
- 두 구성의 별도 D3D12/실제 오디오 실행에서 QPC 타이머와 DSP 음악 예약,
  기존 GameplayPresenter/GameplayAudioRouter로 전체 채보 진행 성공.
  최종 Debug 1,550프레임, Release 1,553프레임을 렌더링했으며 오디오 오류 없음.
  의도적인 늦은 GOOD 입력/틱 실패가 포함된 세션 정확도는 85.1694%입니다.
- 프로젝트/필터/의존성/엔진 경계 검사(8개 프로젝트), git diff --check 통과.
  판정 규칙과 엔진 코드는 변경하지 않았습니다.
- 수동 확인: 게임 실행 후 네이티브 화면 캡처가 재시도 포함 두 번 시간 초과하여
  화면을 보며 키를 누르는 확인은 수행하지 못했습니다. 위 실제 엔진 replay는
  숨겨진 창에서 PlaySession에 시각 지정 입력을 전달하는 자동 검사입니다.
  실제 Raw Input, 픽셀 배치·청음·마우스 조작의 수동 검증을 뜻하지 않습니다.

## 2026-10-04 에디터 실시간 뷰 마디선 두께

- `TaikoEditorMode::DrawChart`의 실시간 마디선 두께를 기준 좌표 2에서 3으로 조정.
- Debug/Release x64 전체 Rebuild, 두 구성 로직·카탈로그 테스트와
  smoke-test/lobby/gameplay/editor 종료 코드 0.
- 두 구성 기존 EditorAudioViewTests/EditorModeTests 통과.
- 프로젝트/필터/의존성/엔진 경계와 git diff --check 통과.
- 실제 화면에서의 가독성·마우스 조작은 수동 확인하지 않음.

## 2026-10-04 에디터 리뷰 수정과 편집 경계 정리

- 실시간 레인의 공유 사각형으로 그리기/추가/삭제의 가로·세로 경계를 검사.
  네 방향 레인 밖 클릭은 문서를 변경하지 않고 내부 추가/삭제는 유지됩니다.
- Taiko 옵션 파서를 플레이/예측에서 공유. `Action = Kat`, `TickDivision = 64`의
  예측은 Kat head + body 31개로 플레이의 시각/사운드와 일치합니다.
  Purple의 Don/Kat 두 cue, 시각별 변경과 명시적 사운드 우선순위도 검사했습니다.
- NaN/무한대/clock 범위 및 1e12ms 마디 탐색 초과 입력은 이전 시간을 보존합니다.
  강제로 화면 구성 예외를 발생시켜 상태 메시지, 부분 버튼 제거, 자동 재시도 중지와
  탭 이동 후 복구를 검사했습니다.
- `IEditorContext`/`IEditorAudioSource`로 편집과 worker 조회를 분리했습니다.
  Workspace 수정 명령, 읽기 전용 문서/악보 설정, 이름 있는 입력 초안과
  typed 이펙트 선택을 사용합니다. 후보 검증 실패의 revision/dirty/캐시 보존도 검사했습니다.
- Debug/Release x64 전체 Rebuild 성공. 두 구성 로직·카탈로그(1곡/4패턴),
  EditorAudioViewTests/EditorModeTests 성공; 각 구성 smoke-test/lobby/gameplay/editor 종료 코드 0.
- 두 구성 D3D12/FMOD 전체 노트 replay 성공: 각각 1,557프레임,
  19개 노트 완료, 85.1694% 정확도. Buzz 63개 로직 경계 조건도 통과했습니다.
  이 replay는 숨겨진 창과 시각 지정 입력을 사용하는 자동 검증입니다.
- 8개 프로젝트의 소스/필터/의존성/엔진 경계, git diff --check 통과.
  엔진·사용자 곡/스킨·파일 형식은 변경하지 않았습니다.
- 수동 확인: 실제 화면의 픽셀 배치·마우스/키보드 조작·청취는 수행하지 않았습니다.
