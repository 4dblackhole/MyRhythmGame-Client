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
옵션/설정 파일 변경은 같은 문서의 Options panel regression을 두 구성에서 실행합니다.

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

## 2026-10-05 편집 원본 트리와 실행 중 작업량 최적화

- 노트·BPM/마디 길이·이펙트·히트사운드 변경의 편집 원본은 균형 트리로 관리합니다.
  읽기 전용 DTO/컴파일 결과와 렌더링 임시 목록은 vector를 유지합니다.
  노트 ID 유지, 시간/마디 범위 조회, 범위를 가로지르는 롱노트, 끝점 삭제,
  BPM/delay 변경 후 재색인, 실패한 수정의 원상 보존과 복사본의 이펙트 소유권을 검사했습니다.
- 엔진과 Client의 Debug/Release x64 솔루션 전체 Rebuild 성공.
  두 구성의 엔진 Collision.Tests(공유 사각형 배치의 수명·변환·clip·순서 포함),
  Client 로직·카탈로그(1곡/4패턴), EditorAudioViewTests/EditorModeTests 성공.
  오디오 분석 취소, 실제 MP3 비동기 열기, 미리듣기 실패 재시도 제한과 선택 복구도 검사했습니다.
- 두 구성의 smoke-test/lobby/gameplay/editor 종료 코드 0.
  Debug의 mesh/collision/widgets 엔진 예제 route도 종료 코드 0.
- 두 구성의 실제 D3D12/FMOD 전체 노트 replay 성공: Debug 1,411프레임,
  Release 1,557프레임, 19개 노트 완료, 85.1694% 정확도.
  시작 시 표시 객체 지연 생성, 종료 후 제거, 되돌아가기 시 재생성도 검사했습니다.
  Buzz 63개 로직 경계 조건은 그대로 통과했습니다.
- 8개 프로젝트의 소스/필터/의존성/엔진 경계와 두 저장소의 git diff --check 통과.
  빌드 시 생성한 기본 에셋 식별값은 이전 런타임 FNV 계산값
  `00c8cf20448ad6fe`와 두 구성 모두 일치합니다.
  사용자 곡/스킨과 YMP/YME 파일 형식, 판정 규칙은 변경하지 않았습니다.

성능 수치는 같은 PC의 Release에서 실제 AngelDream 채보/음원을 이용한 CPU 측정의
중앙값입니다. 뷰 구성은 같은 상태에서 10회, 화면 명령 수집은 30회 측정했습니다.
렌더러는 명령만 수집하므로 아래 값은 GPU 렌더링 시간이나 FPS를 의미하지 않습니다.

| 측정 항목 | 변경 전(ms) | 변경 후(ms) |
| --- | ---: | ---: |
| 실시간 뷰, 디바이더 1024 구성 | 50.9835 | 2.7731 |
| 전체 악보 뷰, 디바이더 1024 구성 | 9.3769 | 0.7254 |
| 전체 악보 뷰, 디바이더 1024 화면 명령 수집 | 24.7468 | 0.0135 |
| 오디오 탭 10초 위치 구성 | 5.6064 | 3.9996 |
| 오디오 탭 10초 위치 화면 명령 수집 | 7.2997 | 0.0191 |

- 변경 후 10,000개 노트의 단일 추가/삭제 쌍은 100회 중앙값 0.0022ms입니다.
  전체 snapshot 생성이나 BPM 지도 재구축 시간은 포함하지 않습니다.
  롱노트 쌍 재구성/교차 범위 검사는 롱노트 끝점 수에 비례하며,
  전체 문서 교체와 읽기 전용 snapshot 생성에는 전체 데이터 작업이 남아 있습니다.
- 고밀도 격자는 화면에 겹치는 보조선만 줄이며 입력의 전체 스냅 후보를 유지합니다.
  공유 사각형 배치는 매 프레임 명령 복사를 줄입니다. 오디오 탭의 실제 GPU 사각형
  수는 동일하며, 재생 위치를 바꾸는 구성 작업은 중앙값 5.8035ms/최대 7.4347ms였습니다.
- 분석은 단일 worker에서 취소 가능하게 처리하고, PCM 전체 보관을 피합니다.
  곡 목록 재진입 갱신, 세션 진입의 파일 로딩/검증과 최초 기본 에셋 설치는
  여전히 전환 시 동기 작업입니다. 모든 파일 로딩이나 지연 가능성을 제거한 것은 아닙니다.
- 수동 확인: 실제 화면의 픽셀 배치·마우스/물리 키 입력·청취는 수행하지 않았습니다.
  숨겨진 창의 실제 엔진 replay와 미리듣기 검사는 자동 검증입니다.

## 2026-10-05 오디오 옵션과 Option.ini

- 기존 OptionsPanel에 미들웨어(FMOD만 설치)·일반(자동)/WASAPI/ASIO·실제 드라이버
  ComboBox 3개를 추가했습니다. 별도 옵션 Controller는 없으며 엔진 API를 직접 사용합니다.
  언어·스킨·오디오 설정은 EXE 옆 UTF-8 Option.ini로 통합했습니다.
- Debug/Release x64 솔루션 전체 Rebuild와 마지막 UI 수정의 최종 증분 빌드 성공.
  두 구성의 로직·카탈로그(1곡/4패턴), smoke-test/lobby/gameplay/editor 종료 코드 0.
- 두 구성 OptionsPanelTests 성공. 새 파일 생성, 한국어 스킨 이름/언어/오디오 값의
  저장 왕복, 잘못된 값 거절, 비연속 드라이버 번호, 출력 방식별 목록 갱신,
  변경 실패와 읽기 전용 INI 저장 실패 시 복구, 장치 없는 상태와 종료 후 입력을 검사했습니다.
  실제 Canvas hit-test로 ASIO 목록 행이 아래 드라이버 필드보다 먼저 클릭되는 것도 검사했습니다.
- 두 구성에서 실제 FMOD Automatic → WASAPI 변경 및 현재 드라이버 선택 성공.
  변경 전 로드한 pop.wav clip이 변경 후에도 Ready/유효한 상태임을 확인했습니다.
  ASIO 전환/실패 사례는 테스트 backend를 이용했으며 실제 ASIO 하드웨어 검증은 아닙니다.
- Debug/Release 출력 폴더에 Option.ini가 생성되는 것을 확인했습니다. 테스트용 설정은
  별도 임시 폴더에 만들고 기존 사용자 설정·곡·스킨과 구형 skin-set.txt는 보존했습니다.
- 8개 프로젝트의 소스/필터/의존성/엔진 경계와 git diff --check 통과.
  엔진 저장소/SDK, 파일 형식과 판정 규칙은 변경하지 않았습니다.
- 수동 확인: 실제 화면 배치·마우스 조작·청취·실제 ASIO 장치 전환은 수행하지 않았습니다.
  위 포인터 입력과 실제 FMOD 검사는 자동 검증입니다.

## 2026-10-06 YME 문법·효과 적용 검증

- YMP의 Effect file 참조와 YME 소유 HitSounds 표, Interpolation/Speed/Sounds/Zone을
  새 문법으로 교체했습니다. 지점/Area, 세 기본 보간·수식·3차 베지어와 저장 왕복을 검사했습니다.
  Volume의 Music/UI 대상 거절, 히트사운드 대상 및 음수 값 거절도 검사했습니다.
- 기존 AngelDream 음원/YMM을 사용하는 효과 테스트 YMP/YME와 기본 Don/Kat를 재사용한
  상대경로 WAV 2개를 추가했습니다. 제공곡은 1곡/5패턴입니다. 기존 원본 곡/스킨은 보존했습니다.
- Debug/Release x64 솔루션 전체 Rebuild 성공. 새 코드의 변환 경고 정리와 서식 정리 후
  최종 증분 Build 및 두 구성의 로직·카탈로그 테스트도 성공했습니다.
- 두 구성 smoke-test/lobby/gameplay/editor 종료 코드 0.
  두 구성 EditorAudioViewTests/EditorModeTests 성공. 플레이와 에디터의 Whole 누적 거리,
  Separate head 배율, 등록식 수정 후 재컴파일과 YME 상대경로를 검사했습니다.
- 기본/싱코페이션 AccuracyRange 공유, 모든 등급의 ±(기존 범위+10ms) 경계와 다음 1µs,
  JudgeLevel 100에서도 +10ms 유지, 4분/8분/16분 격자 제외, 실제 노트 입력과
  롱노트 head의 객체 선택을 검사했습니다. Kiai ON/OFF 경계와 역방향 조회도 통과했습니다.
- 두 구성의 실제 D3D12/FMOD 효과 replay 성공: 15개 논리 노트 완료, Buzz 2개 실패/35개 성공
  (총 37개 body tick), 정확도 97.8897%. Debug 1,302프레임, Release 1,290프레임을 렌더링했습니다.
  음악 DSP 예약과 히트사운드 교체·볼륨 자동화 실행에 오디오 오류가 없었습니다.
- 기존 전체 노트 replay도 두 구성에서 19개 완료, 정확도 85.1694%, 각각 1,557프레임으로 성공.
  밀집 Buzz의 63개 로직 경계 조건도 통과했습니다.
- 8개 프로젝트의 소스/필터/의존성/엔진 경계와 git diff --check 통과. 엔진은 변경하지 않았습니다.
- 수동 확인: 위 실제 엔진 replay는 숨겨진 창과 지정 시각 입력을 사용하는 자동 검증입니다.
  실제 화면의 픽셀·효과 움직임·물리 키/마우스 조작·청취와 볼륨 차이는 수동 확인하지 않았습니다.
  Kiai는 상태만 처리하고 새 시각 효과나 전용 편집 UI를 추가하지 않았습니다.
