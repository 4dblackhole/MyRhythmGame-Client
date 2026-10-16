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

osu!taiko 변환기 변경은 같은 부모 폴더의 독립 저장소에서 아래 검사도 두 구성으로 실행합니다.
실제 맵/오디오는 해당 저장소의 ignored `TestData`에 복사하고 Git에 포함하지 않습니다.

```powershell
..\OsuTaikoConverter\bin\x64\Debug\OsuTaikoConverter.Tests.exe
..\OsuTaikoConverter\Tests\CliTests.ps1 -Configuration Debug -MapPath ..\OsuTaikoConverter\TestData\Pandora\map.osu
```

변환기의 Core/EXE/Tests 프로젝트에도 실제 구성의 MSVC Code Analysis를 실행합니다.

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

재사용 엔진 변경은 엔진 저장소의 Debug/Release 전체 빌드와 관련 SDK 단위 테스트를 실행합니다.
Client에는 별도 기술 예제 route가 없습니다.
프로젝트/필터/include 경계는 `Scripts/CheckArchitecture.ps1`로 검사합니다.
키빔 표시는 [별도 회귀 테스트](../Tests/Presentation/README.md)로 검사할 수 있습니다.
판정 인디케이터/마커/링·문구 변경은 같은 문서의 Gameplay judgement guide 회귀를
Debug/Release에서 실행합니다.
상단 진행바도 같은 실행 파일의 진행률/시간/롱테일/타이머 일시정지/resize/빈 채보 검사와
All-notes actual gameplay replay의 D3D12/FMOD 진행바 검사를 두 구성에서 실행합니다.
결과 수집/표시/전환은 같은 문서의 Result graphs and actual Scene transition을 두 구성에서
실행합니다. 점수 정밀도와 입력/중도 종료 통계는 PlayResultTests의 도메인 검사도 사용합니다.
ESC 메뉴·음악 seek·판정 복원은 같은 문서의 Gameplay pause menu and transport를 두 구성에서
실행하며 PlayPauseTests의 Rewind/Retire 검사를 함께 사용합니다.
오디오 탭/타임라인 변경은 같은 문서의 Editor audio view regression을 두 구성에서 실행합니다.
곡 카드 가상화·비동기 카탈로그 변경은 `Tests/Presentation/RunMusicSelectViewTests.ps1`도
Debug/Release에서 실행합니다.
에디터 모드/문서 계약 변경도 같은 실행 파일에 연결된 EditorModeTests를 두 구성에서 실행합니다.
에디터 상단 메뉴 변경은 같은 실행 파일의 EditorMenuTests도 두 구성에서 실행합니다.
UI smoke는 초기화·실행·정리 검증이며 실제 모양/사용자 조작 검증과 구분합니다.
전체 노트/Buzz 회귀는 `--catalog-root` 로직 테스트와
[All-notes actual gameplay replay](../Tests/Presentation/README.md)를 두 구성에서 실행합니다.
옵션/설정 파일 변경은 같은 문서의 Options panel regression을 두 구성에서 실행합니다.
외부 자산 경로 변경은 같은 문서의 Development and deployment asset paths도
두 구성에서 실행합니다. 같은 EXE의 이동과 다른 작업 폴더에서의 실행을 검사합니다.
숫자 파서/자산 초기화의 예외 경계 변경은 같은 문서의 Allocation failure boundary regression도
두 구성에서 실행합니다. 이 테스트는 실제 자산 캐시를 사용하지 않습니다.

커밋 전 `git diff --check`, 사용자 파일 제외 여부, Client/엔진 status와 upstream을
확인합니다. main 직접 작업은 검증 후 commit/push하며 브랜치 요청 시에만 PR을 사용합니다.
FMOD SDK·DLL·import lib는 커밋하지 않습니다.

## 2026-10-17 Penpot 일시정지 UI

- 연결된 Penpot의 Gameplay · Sky / b0134a97-87eb-801a-8008-cca341851088 보드를
  직접 조회하고 내보낸 화면·치수·색상·글꼴과 비교했습니다. 중앙 720×336px 메뉴,
  72px 높이/16px 간격의 네 행, 252×48px 값 선택기와 Noto Sans KR 700을 반영했습니다.
- GameplayPauseLayout은 조립/배치, GameplayPauseMenu는 포커스/값/입력 의도,
  PauseTexts/언어 INI는 문구를 맡습니다. 키보드와 화살표 클릭은 같은 AdjustValue를
  사용하며 선택기 클릭으로 재개/되감기를 실행하지 않습니다. Controller/상태머신은 그대로입니다.
  좁은 화면에서는 메뉴 부모를 함께 축소합니다. 기존 사용자 스킨 버튼/패널 매핑도 유지합니다.
- 기존 Noto Sans KR 자산 경로 함수는 Result 전용 이름 대신 NotoSansKR로 공유했습니다.
  배경은 Default Skin/InGame/Pause/Backdrop.png의 1×1 RGBA(23,52,78,97)와
  skin.ini의 PauseBackdrop을 사용합니다. 화면 단색 Panel의 불투명 경로 대신 이미지 Sprite의
  알파 합성을 사용하여 배경을 38% 덮습니다. 버튼·화살표는 기존 Sprite 도형으로 조합하며
  엔진 소스/API/포인터는 변경하지 않았습니다.
- Debug/Release x64 전체 solution Build 및 MRG.Client MSVC 14.44 Code Analysis Build
  (실제 x64/C++20/include/define/PCH, RunCodeAnalysis=true/BuildProjectReferences=false) 성공.
  최종 GameplayPauseLayout도 두 구성의 CL.command.1.tlog 인수로 /analyze:only를 실행했습니다.
  초기 C4834와 테스트 C6011을 수정했고 최종 빌드/분석/테스트에 신규 경고·파싱 오류가 없습니다.
  범위 밖의 기존 EditorAnalysisController.cpp:51 C26115는 프로젝트 분석에 남았습니다.
- 두 구성 GameplayPause/ResultPresentation/Skin 회귀를 /W4 /analyze로 빌드·실행했습니다.
  시안의 행 크기/간격, 선택기 클릭과 값 제한, 실행 버튼·키보드·언어·좁은 화면·소유 Canvas 정리,
  실제 음악 seek·Resume/Rewind/Retry/Retire·노트/점수 복원·결과 전환·스킨 fallback을 통과했습니다.
  로직/기본곡 카탈로그는 구성별 63개/실패 0, 네 smoke 경로는 총 8개 exit 0입니다.
- production Presenter와 PauseMenu를 겹친 D3D12 offscreen 캡처를 1920×1080,
  720×1280에서 육안 확인했습니다. 메뉴 배치·글자·선택기·배경 투명도를 확인했으며,
  빈 배경 픽셀 RGB(157,173,188)로 반투명 합성도 확인했습니다. 자동 캡처/입력 검사이며
  실제 창에서 물리 키·마우스로 조작하거나 오디오를 직접 청취한 수동 검증은 미실시입니다.
- CheckArchitecture(11프로젝트), Client/엔진 git diff --check 통과.
  사용자 LaneKeyBeam.cpp, Debug/Release Option.ini와 외부 en/ko.ini 해시 일치로 보존했습니다.
  기존 외부 언어파일에서 새 선택기 키가 빠지면 내장 영어가 사용됩니다. 원본 ko/en 템플릿은 갱신했습니다.
  최종 로그/분석 인수는 ignored build/pause-design-2026-10-17/final, BMP/PNG는 그 상위 폴더입니다.

## 2026-10-17 공통 기어 Transform·화면 중앙선 배치

- GameplayLayout이 ScrollGear 부모 아래에 테두리·바탕·레인·입력 오버레이를 조립합니다.
  선택 스킨의 Lane 폭/입력 패널 높이와 테두리 여백으로 부모 Y를 한 번 계산하여
  바깥 테두리 하단을 화면 중앙선에 맞춥니다. 각 자식은 부모 기준 로컬 좌표만 사용하고,
  기존 Lane 하위 노트·마디선·판정원·키빔의 회전과 스크롤은 그대로 상속합니다.
- GameplayPresenter는 새 부모의 비소유 포인터를 보관하고 Shutdown에서 함께 해제합니다.
  노드 소유권은 기존 Canvas에 유지합니다. 화면 배경·상하단 HUD·중앙 풍선/뎅뎅 처리 UI와
  별도 일시정지 Canvas는 기어 밖에 유지합니다. 전체 실제 노드 계층은 Gameplay/Presentation에
  정리했습니다. 생성 순서·부모/자식 좌표·직접 호출부·정리 경로를 검토했습니다.
- Debug/Release x64 전체 solution Build, MRG.Client의 MSVC 14.44 Code Analysis Build
  (실제 x64/C++20/include/define/PCH, RunCodeAnalysis=true/BuildProjectReferences=false) 성공.
  신규 컴파일 경고·분석 진단·파싱 오류는 없으며 기존 EditorAnalysisController.cpp:51 C26115는 남았습니다.
- GameplayJudgementTests를 두 구성에서 /W4 /analyze로 빌드·실행했습니다. 부모 하나를
  이동하면 모든 자식의 World 위치가 같은 만큼 이동하고 진행바는 고정되는지 확인했습니다.
  기본 화면과 네 resize 크기에서 테두리 하단 Y=0도 확인했습니다. 기존 판정 링·문구,
  노트/마디선 속도효과·처리한 틱 숨김·롤 완료·진행바 회귀도 통과했습니다.
  로직/기본곡 카탈로그는 구성별 63개/실패 0, smoke-test/lobby/gameplay/editor 총 8개 exit 0입니다.
- 기본 Angel Dream 채보와 production Presenter의 D3D12 offscreen 캡처를
  1920×1080, 1280×720, 2560×1080에서 생성했습니다. 세 크기 모두 기어 테두리 하단 Y=0,
  레인/입력 패널/판정원 중심 Y=80(논리 좌표)이며 1920/2560 캡처를 육안 확인했습니다.
  1080p에서는 테두리 하단이 화면 y=540px입니다. 자동 캡처/좌표 검사이며 실제 창에서의
  물리 키·마우스 조작은 미실시입니다.
- CheckArchitecture(11프로젝트), Client/엔진 git diff --check 통과. 엔진 소스/포인터 변경 없음.
  사용자 LaneKeyBeam.cpp, Debug/Release Option.ini와 외부 en/ko.ini의 해시 일치로 보존을 확인했습니다.
  로그·검사 helper·BMP/PNG는 ignored build/gameplay-gear-2026-10-17에 있습니다.

## 2026-10-17 리절트 1ms 판정별 빈도 막대

- TaikoResultGraphs는 오차를 가까운 정수 ms로 대칭 반올림하여 입력이 있는 1ms 구간만
  집계합니다. 실제 판정을 산점도와 같은 색으로 MAX→BAD 순서로 아래부터 쌓으며,
  막대 높이와 최고 빈도는 구간 전체 입력 수를 사용합니다. 큰 오차에도 범위 크기에
  비례하는 배열을 만들지 않고 기존의 불변 RectangleBatch 하나로 전달합니다.
- TaikoResultGraphView는 화면 배율에 맞춰 막대의 그리기 폭만 최소 한 픽셀로 유지합니다.
  축소 화면에서도 1ms 집계와 색상별 개수를 합치지 않습니다. 판정·점수·입력 로그의
  수집이나 소유권은 변경하지 않았으며 변경 함수와 직접 호출부의 범위·변환·수명을 검토했습니다.
- Debug/Release x64 전체 solution Build 성공. MRG.Client를 실제 프로젝트 설정으로
  MSVC 14.44 Code Analysis Build(RunCodeAnalysis=true/BuildProjectReferences=false)했습니다.
  최종 변경된 두 구현 파일은 각 구성의 CL.command.1.tlog에서 얻은 실제 C++20/include/define/
  PCH/CRT 인수를 사용하여 별도 출력 경로에서 /analyze:only도 성공했습니다.
  신규 경고·분석 진단·파싱 오류는 없고 기존 EditorAnalysisController.cpp:51 C26115는 남았습니다.
- ResultPresentation 회귀를 두 구성에서 /W4 /analyze로 빌드·실행했습니다. ±0.5ms 경계와
  1ms 간격, 혼합 판정의 색상·누적 높이·최고 빈도, 입력 순서 독립성, 큰 오차의 희소 집계,
  최소 그리기 폭·경계와 기존 10,003개 입력의 단일 배치·실제 Result/Retry 전환을 통과했습니다.
  로직/기본곡 카탈로그는 구성별 63개/실패 0, smoke-test/lobby/gameplay/editor 총 8개 exit 0입니다.
- 기본 Angel Dream 채보의 실제 판정 결과를 production View에 전달한 D3D12 offscreen 캡처를
  1920×1080, 720×1280에서 육안 확인했습니다. 4.500ms MAX와 4.501ms PERFECT가 같은
  5ms 막대에 쌓이고 12ms GREAT 막대와 분리됩니다. 720px 캡처의 해당 그래프 영역에서도
  MAX/PERFECT/GREAT 색상 픽셀이 각각 38/38/76개 남는 것을 확인했습니다.
  자동 캡처 확인이며 물리 키·마우스를 이용한 실제 창 조작은 미실시입니다.
- CheckArchitecture(11프로젝트), Client/엔진 git diff --check 통과. 엔진 소스/포인터 변경 없음.
  사용자 LaneKeyBeam.cpp, Debug/Release Option.ini와 외부 en/ko.ini의 해시 일치로 보존을 확인했습니다.
  로그·분석 인수·BMP/PNG는 ignored build/result-histogram-2026-10-17에 있습니다.

## 2026-10-17 리절트 하단 구름의 렌더링 순서

- TaikoResultView::BuildBackground에서 하단 구름 ZIndex를 1에서 -9로 변경했습니다.
  같은 Center anchor의 하늘(-10) 위, 결과 콘텐츠(0) 뒤에 배치합니다. 화면 재구성/resize에서도
  같은 순서를 사용하며 버튼·통계의 배치, 입력 처리와 소유권은 변경하지 않았습니다.
- Debug/Release x64 전체 solution Build, MRG.Client의 MSVC 14.44 Code Analysis Build
  (실제 x64/C++20/include/define/PCH, RunCodeAnalysis=true/BuildProjectReferences=false) 성공.
  변경 파일과 직접 호출부를 검토했으며 신규 경고·분석 진단·파싱 오류는 없습니다.
  범위 밖의 기존 EditorAnalysisController.cpp:51 C26115는 두 구성에 남았습니다.
- 두 구성 로직/기본곡 카탈로그 각각 63개/실패 0, smoke-test/lobby/gameplay/editor 총 8개 exit 0.
  기존 ResultPresentation 회귀도 두 구성 통과: 결과 표시·버튼 클릭·언어/resize·Canvas 정리·실제 Result/Retry 전환.
  낮은 영향의 Z-order 수정이므로 새 회귀 테스트는 추가하지 않았습니다.
- 실제 production View의 D3D12 offscreen 캡처를 육안 비교했습니다.
  1920×1080에서 구름이 하단 통계 패널·버튼 일부를 덮던 문제가 해결되고,
  720×1280에서도 결과·버튼과 BACK이 구름 앞에 표시됩니다.
  자동 캡처 확인이며 물리 키·마우스를 이용한 실제 창 조작은 미실시입니다.
- CheckArchitecture(11프로젝트), Client/엔진 git diff --check 통과. 엔진 소스/포인터 변경 없음.
  사용자 LaneKeyBeam.cpp, Debug/Release Option.ini와 외부 en/ko.ini 해시 일치로 보존을 확인했습니다.
  로그와 수정 전후 BMP/PNG는 ignored build/result-cloud-z-2026-10-17에 있습니다.

## 2026-10-17 모드 공통 플레이 상태머신·음악 재개 동기화

- IPlaySessionController의 Loading/Gameplay/Pause/Ending 계약과 공통 Controller/Feedback를
  Client/GameFlow에 두었습니다. 상태 전환·입력·타이머 구현은 Taiko/Scene 헤더를 참조하지 않습니다.
  기존 Taiko 기본 사운드는 오디오 등록 파일의 지역 함수, 디버그 채보 생성은 세션 factory가 맡습니다.
  Scene은 인터페이스로 조립하고 표시·메뉴·deferred Result 전환만 연결합니다.
- Resume/Rewind는 명령 접수 후 Pause에서 시간을 고정하고 paused 음악 Voice의 비동기 seek를 기다립니다.
  준비 완료 후 새 snapshot의 QPC/DSP에 동일한 20ms 준비 시간을 예약합니다. 준비 중 입력은 판정하지
  않고 예약 시작 이후 입력은 전환 프레임에서도 보존합니다. 기존 히트사운드 Voice는 정지하며 복구하지 않습니다.
  Retry는 자산을 재사용하며 새 시도를 초기화하고, Ending은 모드별 결과를 한 번 확정·이동합니다.
- Debug/Release x64 전체 solution Build 성공. 영향 프로젝트 MRG.Client는 두 구성 모두 실제
  include/define/PCH/C++20/x64 설정의 MSVC 14.44 Code Analysis Rebuild와 최종 변경분 분석 Build 성공.
  신규 경고·분석 진단·파싱 오류 0. 기존 EditorAnalysisController.cpp:51 C26115는 두 구성에 남았습니다.
  Debug 전체 solution 분석에서 범위 밖 EditorAudioAnalysis.cpp:134의 기존 C6262도 관찰했습니다.
  이 작업에서 이동·수정한 디버그 입력의 상수 조건은 if constexpr로 명시해 기존 Release C6236을 제거했습니다.
- 두 구성 로직/기본곡 카탈로그 각각 63개, 실패 0. smoke-test/lobby/gameplay/editor 총 8개 exit 0.
  GameplayPause, ResultPresentation, GameplayJudgement, AllNotesGameplay 회귀가 두 구성 모두 통과했습니다.
  마지막 include/참조 방향 정리 후 Client 분석 Build, Pause/Result 회귀와 네 smoke도 두 구성 재확인했습니다.
- Pause 회귀는 40ms 탐색 지연, 탐색 전 unpause 거부, 대기 중 시각 고정, 중복 Resume 거부,
  QPC/DSP 예약 여유 일치, 실제 재생 중인 히트사운드 정리와 음악만 재생, EOF 이후 무음 진행,
  음악 준비 실패/재시도, Retry 자원 재사용, BPM/마디 길이 변경을 포함한 Rewind/판정 복원을 검사합니다.
  테스트 전용 비-Taiko IPlayGameMode도 같은 컨트롤러 인터페이스로 네 상태와 별도 IPlayResult 이동을 검사합니다.
  실제 FMOD NoSound/MP3에서 양수·0·끝부분의 반복 seek와 종료를 확인했습니다.
  메뉴 Resume/Retry/Retire의 Canvas 클릭, 키 포커스, 준비 제한, 언어/resize와 소유권도 자동 검사했습니다.
- 변경 코드와 직접 호출부의 상태 진입/중복 명령, 단일 타이머와 Raw Input 경계, 오디오 준비/예약 실패,
  Voice/Clip/결과 소유권, 정수 us/샘플 변환, 모드/Scene 참조 방향과 함수 책임을 검토했습니다.
  CheckArchitecture(11프로젝트), Client/엔진 git diff --check 통과. 엔진 소스/포인터 변경은 없습니다.
  사용자 LaneKeyBeam.cpp, Debug/Release Option.ini, Client/Assets의 외부 en/ko.ini 해시 일치로 보존을 확인했습니다.
- **수동 확인:** 물리 키·마우스 조작, 실제 청음, 화면 육안 확인은 미실시입니다.
  위 결과는 자동 입력 의도/Canvas·실제 D3D12 실행과 FMOD NoSound 검증이며 수동 확인과 구분합니다.
  로그는 ignored build/pause-state-2026-10-17에 있습니다. 최신 실행 파일은 bin/x64/Release/MyRhythmGame.exe입니다.

## 2026-10-16 새 난이도·offset 도구·JudgeLevel·Release 기본 배포본

- 에디터 곡 카드의 +, 메타데이터 입력/검증, 새 난이도 폴더의 빈 YMP/YME 생성과 기존 Editor 전환을 연결했습니다.
  Scene은 조립/전환, NewDifficultyDialog는 Unicode 입력/오류 표시, EditorPatternCreation은 경로/새 파일 생성과 정리를 담당합니다.
  기존 파일은 덮어쓰지 않으며 취소/잘못된 메타데이터에서는 생성하지 않습니다. 기본 Taiko/120 BPM/0ms/4/4입니다.
  공통 JudgeLevel 값은 1.00~100.00/0.01 단위 정수로 저장하며 파서/저장/판정 프로필/HUD/결과 snapshot에서 공유합니다.
  기본 50.00이고 두 메타데이터 화면에서 수정합니다. 유효한 소수 셋째 자리, 범위 밖, 비유한 값은 거부합니다.
- EditorWorkspace의 offset 변경과 EditorAudioControls의 입력/±1ms, 박자/예상 타격 눈금을 추가했습니다.
  EditorMetronome은 문서/음원 길이 revision별 박자·재생 cue를 캐시하고 기존 EditorPlayback의 DSP 예약에 전달합니다.
  분석용 히트사운드 FFT에는 섞지 않으며 토글은 음악 재로딩/seek를 하지 않습니다. 기본 클릭 WAV 두 개는 Default Skin/Editor에 내장합니다.
- Debug/Release x64 전체 solution Build 성공, 일반 빌드 새 경고/오류 0.
  두 구성 MSVC 14.44 Code Analysis(/analyze) Rebuild를 Rhythm, Chart, Modes, MRG.Client,
  Rhythm.Tests의 실제 include/define/C++20/x64 구성으로 실행했습니다. 파싱 오류·신규 진단 0.
  기존 C26115(EditorAnalysisController:51), Release C6236(GameplayInput:66)은 유지했습니다.
- 두 구성 전체 로직·기본곡 카탈로그 각각 63개/실패 0, 네 Client smoke씩 총 8개 성공했습니다.
  NewDifficulty, MusicSelectView, EditorPresentation(Audio/Mode/Menu/Playback/Metronome), LanguageFile,
  GameplayJudgement와 ResultPresentation의 생산 코드 연결 회귀도 두 구성에서 모두 성공했습니다.
  신규 테스트는 별도 build fixture만 사용하여 Unicode·취소·오류 후 수정·중복/경로 거부·상대경로·카탈로그 재발견,
  fractional JudgeLevel의 실제 판정범위/결과 보존, BPM/마디/offset/음수 delay의 메트로놈 grid와 cue 캐시를 확인합니다.
- 변경 코드와 직접 호출부에서 파일/폴더 소유권, CREATE_NEW/롤백 범위, modal 콜백 예외/문자열 수명,
  선택 상태/생성 결과의 이동, 값 범위/고정소수점과 us 변환, 분석 worker와 재생 전용 캐시 분리,
  프레임별 파일 I/O·템포 재적분 부재, 함수 책임을 검토했습니다.
  CheckArchitecture(11프로젝트), Client/엔진 git diff --check 성공. 엔진 변경은 없습니다.
  사용자 LaneKeyBeam.cpp, 개발 Debug/Release Option.ini, Client/Assets의 외부 en/ko.ini SHA256을 대조해 보존했습니다.
- 사용자의 명시적 승인으로 기존 C:/Users/AA/Desktop/demo 경로/링크 부재를 확인하고 삭제/재생성했습니다.
  곡 0개, Asset/Songs/Music·Patterns·Skins·Languages, 검증한 Release EXE/fmod.dll과 기본 설정,
  폰트 고지, Microsoft 서명이 Valid인 공식 VC_redist.x64.exe를 포함했습니다. 설치는 수행하지 않았습니다.
  실제 배포 위치에서 로고/빈 목록 smoke 종료 코드 0, 개발 자산 override 경로 부재와 Release EXE 해시 일치를 확인했습니다.
  곡 없는 묶음이므로 그 위치의 editor smoke는 실행하지 않았습니다. 원본 저장소의 editor smoke는 두 구성 성공했습니다.
  공개 배포에 필요한 음악/미들웨어 사용권·크레딧/로고 조건의 확정은 이 파일 검증 범위가 아닙니다.
- **화면 확인:** 실제 Win32 메타데이터 창의 자동 캡처에서 필드/기본 50.00과 버튼 배치를 확인했습니다.
  실제 D3D12 1920×1080 offscreen 캡처를 눈으로 확인하여 에디터 카드의 +,
  세 오디오 영역·박자/타격 눈금·offset/메트로놈 컨트롤을 확인했습니다.
  물리 키/마우스 조작, 실제 청음, 다른 PC에서의 새 런타임 설치 검증은 미실시입니다.
  자동 FMOD NoSound/MP3 seek와 입력/타이밍 회귀는 청음 수동 확인과 구분합니다.
  로그/캡처는 ignored build/new-difficulty-2026-10-16에 있습니다.
- 공용 미디어·라이선스·랭크 변조 검증은 RankedContentProposal의 설계 조언만 작성했습니다.
  현재 런타임/파일 문법에 배경 ID·BGA·서버 검증·라이선스 인증을 추가하지 않았습니다.

## 2026-10-16 skin.ini 씬별 폴더·이미지 경로

- Debug/Release x64 솔루션 빌드 성공. 실제 프로젝트 설정으로 Client 전체를 MSVC 14.44 Code Analysis(`/p:RunCodeAnalysis=true`, x64, 두 구성)로 Rebuild했고, 캡처에서 찾은 로고 배경 tint 수정도 두 구성으로 재빌드/분석했습니다.
- 신규 컴파일/분석 경고 없음. 기존 `EditorAnalysisController.cpp:51 C26115`(두 구성)와 `GameplayInput.cpp:66 C6236`(Release)는 이번 범위 밖의 기존 항목으로 남겼습니다.
- 로직·카탈로그는 각 구성 63개, 실패 0개. `--smoke-test`, `--smoke-lobby`, `--smoke-gameplay`, `--smoke-editor`가 두 구성에서 모두 exit 0입니다.
- `RunSkinTests.ps1`는 두 구성에서 UTF-8/BOM/CRLF/한글 폴더·파일명, 씬 폴더/파일 기준 경로, 절대/상위 경로 거부, 중복/누락/잘못된 UTF-8, 요소별 내장 fallback, 부분 스킨, revision 캐시/재선택 갱신을 확인했습니다. 실제 Logo/MusicSelect View를 조립해 버튼 상태, 전체 배경, 내부 채움 숨김·복원, 가상 카드 바인딩, Canvas 수명도 검사했습니다.
- OptionsPanel, MusicSelectView, GameplayJudgement, GameplayPause, ResultPresentation, EditorPresentation, AllNotesGameplay 회귀를 두 구성에서 실행했습니다. 테스트 소스도 MSVC Code Analysis를 사용합니다. 모든 노트 이미지/진행바/판정 이미지와 실제 D3D12/FMOD 플레이·결과·일시정지 경로를 포함합니다.
- `Scripts/CheckArchitecture.ps1`(11개 프로젝트)와 `git diff --check` 통과.
- 자동 캡처 확인: production Logo/MusicSelect View의 D3D12 1920×1080 기본/사용자 스킨 네 캡처를 열어 확인했습니다. 기존 기본 표시 유지, INI의 사용자 배경·버튼·패널·카드 적용과 런타임 문구를 확인했고, 로고 전체 배경의 기본 tint를 흰색으로 수정한 뒤 다시 확인했습니다. D3D12 경고/오류 출력 없음. 캡처 fixture는 기존 PNG를 재사용하며 배포 스킨을 변경하지 않습니다.
- 실제 수동 조작/청음: 물리 키보드·마우스 조작과 청음은 하지 않았습니다. UI 트리/입력·전환·음악 검사는 자동 회귀 결과입니다.
- 사용자 LaneKeyBeam.cpp, Debug/Release Option.ini와 외부 언어 INI의 SHA-256을 작업 전후 비교해 보존을 확인했습니다. 사용자 Songs/Skins는 쓰지 않았으며 새 테스트와 캡처는 ignored build의 독립 배포 폴더를 사용했습니다. 엔진 변경 없음.
- 로그와 캡처: ignored `build/skin-2026-10-16/`.

## 2026-10-16 전체 고정 UI의 UTF-8 언어파일·영어 기본값

- 로고/옵션/곡 선택/플레이/일시정지/리절트/공통·Taiko 에디터의 문구와 표 밖의
  판정명·단위·성능 표시를 총 261개 INI 키로 관리합니다. 원본은 `Client/Texts/Languages`,
  실제 편집본은 자산 루트의 `Languages/{ko,en}.ini`이며 재실행 시 반영합니다.
  선택 언어 → 외부 영어 → EXE 내장 영어 순서로 찾으며 한국어 코드 상수 fallback은 없습니다.
  GenerateEnglishTexts가 같은 en.ini 원본을 생성 헤더로 포함하고 빌드는 기존 편집본을 덮어쓰지 않습니다.
- LanguageFile은 UTF-8 INI/이스케이프/구문·서식 검증, TextCatalog는 한 번 읽은 불변 데이터·언어·글꼴을
  소유합니다. 화면별 TextSet은 키와 UI 필드의 대응만 소유합니다.
  에디터 모드도 언어 enum 대신 같은 카탈로그의 참조를 받으며 콜백과 string_view가 이를 관찰합니다.
  롱노트 캡션은 그리기 단위로 문구 표를 재사용합니다. 저장 성공 경로는 Workspace가 데이터로 보관하고
  View가 현재 언어의 서식으로 조합합니다. 제품명/Scene ID·설정/곡/스킨·점수/판정 규칙은 바꾸지 않습니다.
- Client Debug/Release x64 전체 solution Build 성공, 일반 컴파일 경고/오류 0.
  두 구성 전체 로직·기본곡 카탈로그 각각 63개/실패 0, 네 Client smoke 경로씩 총 8개 성공했습니다.
  LanguageFile, OptionsPanel, MusicSelectView, GameplayJudgement, GameplayPause, ResultPresentation,
  EditorPresentation 표시 회귀를 두 구성에서 모두 통과했습니다.
- LanguageFileTests는 BOM/Unicode/CRLF/이스케이프/공백/빈 값, 잘못된 UTF-8와 중복/잘못된 항목의 격리,
  외부 파일이 전혀 없는 영어 기본값, 부분 번역/영어 파일 우선순위, 잘못된 타입·인수/오버플로와
  유효한 서식 인수 재배치, 언어 전환 후 string_view 수명, 파일 변경을 다음 생성 시에만 읽는 것을 검사합니다.
  실행 스크립트는 소스의 UI 키에 영어 기본 정의가 있는지도 검사합니다. 모든 fixture는 ignored build 폴더에 둡니다.
- MSVC Code Analysis(`/analyze`)를 MRG.Client의 실제 Debug/Release x64 include/define/PCH/C++20
  설정으로 실행했습니다. 두 구성의 분석 Rebuild와 마지막 변경 범위의 분석 Build를 사용했습니다.
  신규 진단/파싱 오류 0. 기존 C26115(EditorAnalysisController:51), Release C6236(GameplayInput:66)은 유지합니다.
  새 언어파일 검사는 `/W4 /analyze`, 기존 화면 회귀도 각 스크립트의 `/analyze`로 검사했습니다.
- 변경 코드와 직접 호출부의 영어 대체 우선순위, 파싱/서식 실패 경계, UTF-16/NUL과 경로 인코딩,
  카탈로그의 복사 금지·문자열/프로필/콜백 수명, Scene 스레드의 언어 변경, 프레임 내 파일 I/O 부재와
  함수 책임을 검토했습니다. CheckArchitecture(11프로젝트), Client/엔진 git diff --check 성공.
  사용자 LaneKeyBeam.cpp 및 Debug/Release Option.ini의 SHA256을 대조해 보존했습니다.
  엔진·사용자 곡/스킨/PNG는 변경하지 않았습니다. 로그/캡처는 ignored `build/languages-2026-10-16`입니다.
- **화면 확인:** 실제 D3D12 1920×1080 offscreen 캡처에서 외부 파일 없는 영어 메뉴,
  부분 한국어 INI로 교체한 시작/옵션 버튼과 옵션 제목, 빠진 항목의 영어 표시를 눈으로 확인했습니다.
  같은 EXE로 서로 다른 파일을 읽었습니다. 실제 실행 창에서 물리 키/마우스를 조작하거나
  청음하는 수동 검증은 미실시입니다. 입력·저장·전환 검사는 자동 회귀입니다.

## 2026-10-16 게임 표시 이름 중앙화·변경

- 기존 창 제목, 시작/내장 자산 오류창 제목과 에디터 확인창의 한국어/영어 표에
  FingerDrum이 각각 하드코딩되어 있었습니다. 먼저 공통 GameIdentity.h의 GameName으로
  참조를 모은 뒤 요청한 새 표시 이름으로 변경했습니다. 이름의 코드 정의는 한 곳입니다.
  FingerDrumGame은 창 제목, Main의 ReportStartupError는 UTF-8 진단의 Unicode 오류창을 담당합니다.
  EditorTexts는 언어별 접미사만 보관하고 EditorInput에서 공통 이름과 조합합니다.
  내부 Scene 오류/데모 제목에서 불필요한 제품 이름도 제거했습니다.
  프로젝트/필터에 헤더를 등록했고 변경 방법은 [TextManagement](TextManagement.md#게임-이름-변경)에 기록했습니다.
- Client Debug/Release x64 전체 solution Build 성공, 일반 빌드 경고/오류 0.
  두 구성 로직·기본곡 카탈로그 각각 63개/실패 0, 네 Client smoke 경로씩 총 8개 성공했습니다.
  EditorPresentation 회귀(오디오/모드/메뉴/재생)도 두 구성 성공했습니다.
  소스 감사에서 이전 표시 이름 리터럴/제목 필드 잔여가 없음을 확인하고,
  두 구성 EXE의 UTF-16 데이터에 새 이름이 반영되었는지 확인했습니다.
- MSVC Code Analysis(`/analyze`)를 영향 프로젝트 MRG.Client의 실제 Debug/Release x64
  include/define/PCH/C++20 설정으로 실행했습니다.
  `/t:Rebuild /p:RunCodeAnalysis=true /p:BuildProjectReferences=false`를 사용했으며 신규 진단·파싱 오류 0.
  기존 C26115(EditorAnalysisController:51), Release C6236(GameplayInput:66)은 유지합니다.
  에디터 표시 회귀 소스도 실행 스크립트의 `/analyze`로 검사했습니다.
- 변경 코드와 직접 호출부의 이름 수명/NUL 종료, Win32 wide 문자열 전달,
  에디터 임시 제목의 동기 호출 수명, 언어 표의 constexpr/noexcept 유지,
  오류 디코딩·smoke 표시 정책과 함수 책임을 검토했습니다.
  CheckArchitecture(11프로젝트), Client/엔진 git diff --check 성공.
  사용자 LaneKeyBeam.cpp와 Debug/Release Option.ini의 SHA256을 대조해 보존했습니다.
  사용자 곡·스킨/로고 PNG, 캐시 경로, 내부 식별자와 EXE 파일명은 변경하지 않았습니다.
  엔진 변경은 없으며 로그는 ignored `build/game-name-2026-10-16`에 있습니다.
- **실제 수동 확인:** 창 제목·오류창/미저장 확인창을 직접 열어 확인하는 수동 조작은 미실시입니다.
  위 검증은 자동 빌드/분석·로직/표시 회귀·숨김 smoke와 소스/바이너리 감사입니다.
  기본 로고 PNG를 눈으로 확인했으며 제품 이름이 아닌 임시 `Logo` 문구가 들어 있습니다.

## 2026-10-15 Penpot 리절트 배치 갱신

- 현재 연결된 Result · Sky 보드를 다시 확인하여 88px 상단 배너의 모드·곡명·아티스트·난이도,
  중앙 랭크/제작자, 오른쪽 528px 그래프 열과 256px 액션 버튼을 반영했습니다.
  TaikoResultView의 조립, Panels의 정보/버튼, GraphView의 축/범례와 Layout의 좌표 경계를 유지합니다.
  아티스트는 GameplayLaunchRequest → ResultViewContext의 값 snapshot으로 전달합니다.
  ResultTexts/UiColors가 문구와 배너 색을 소유하고, 내장 Result/Mode/Taiko.png는 Penpot에서 재출력했습니다.
  이번 갱신에는 기존 Canvas/PNG/텍스트/RectangleBatch 기능으로 충분하여 엔진 변경이 없습니다.
- Client Debug/Release x64 전체 solution Build 성공, 일반 빌드 경고/오류 0.
  영향 프로젝트의 분석 Rebuild 후 최종 solution Build도 성공했습니다.
  두 구성 전체 로직·기본곡 카탈로그 각각 63개/실패 0, 네 Client smoke 경로씩 총 8개 성공했습니다.
- ResultPresentation 회귀 두 구성 성공: 새로운 배너/곡·아티스트/제작자/랭크 위치,
  좁아진 그래프 내 실제 샘플 경계, 버튼 너비/클릭·Replay 비활성, 6자리 점수,
  언어·portrait/wide resize·Canvas 정리·실제 deferred Result/Retry 전환을 검사했습니다.
  SceneStateTests는 선택 snapshot의 아티스트 독립 소유와 Clear를 검사합니다.
  MusicSelectView 회귀도 두 구성 성공했습니다. 그래프는 샘플 수와 무관하게 한 batch를 유지합니다.
- MSVC 14.44.35207 Code Analysis(`/analyze`)를 MRG.Client, ClientLogic, Rhythm.Tests의 실제
  Debug/Release x64 include/define/PCH/C++20 설정으로 실행했습니다.
  `/t:Rebuild /p:RunCodeAnalysis=true /p:BuildProjectReferences=false`를 사용했으며 신규 진단·파싱 오류 0.
  기존 C26115(EditorAnalysisController:51), Release C6236(GameplayInput:66)은 유지합니다.
  두 표시 회귀 소스도 실행 스크립트의 `/analyze`로 검사했습니다.
- 변경 코드와 직접 호출부의 선택/결과/Retry 데이터 수명, 빈 메타데이터 fallback,
  좌표 변환·문구 clipping·새 그래프 범위, Canvas 재구성과 함수 책임을 검토했습니다.
  CheckArchitecture(11프로젝트), 양쪽 git diff --check 성공.
  LaneKeyBeam.cpp와 Debug/Release Option.ini의 SHA256을 대조해 보존했고 사용자 Songs/Skins를 수정하지 않았습니다.
  로그와 캡처는 ignored `build/result-update-2026-10-15`에 있습니다.
- **화면 확인:** 실제 D3D12의 1920×1080 한국어(입력 오차 있음)/영어(입력 오차 없음),
  720×1280 한국어 offscreen 캡처를 Penpot과 비교하여 배너·중앙 정렬·그래프/버튼 표시를 눈으로 확인했습니다.
  캡처에는 실제 기본곡 메타데이터와 플레이 규칙으로 만든 결과를 사용했습니다.
  실행 창에서 물리 키/마우스 조작 또는 청음하는 수동 검증은 미실시입니다.
  버튼 입력과 Scene 전환은 자동 검사입니다.

## 2026-10-15 해상도·모드별 키·글로벌 오프셋 옵션

- [TextManagement](TextManagement.md)의 해상도 즉시 적용/저장, Taiko 기본·보조 8키,
  모드별 입력 목록 계약과 정수 ms 오프셋을 구현했습니다. 음악의 타이머 0ms 시작을
  유지하고 공통 MusicalTimeline에서 노트·틱·BPM·마디선·YME 시각을 함께 보정합니다.
  에디터의 원본/Dirty/저장 박자를 보존하며 Copy/Replace/타이밍 수정에도 보정을 유지합니다.
  DLL 로더는 이번 범위가 아닙니다. 색상/ESC 선행 변경도 그대로 유지합니다.
- Client 및 엔진 Debug/Release x64 전체 solution Rebuild 성공, 일반 빌드 경고/오류 0.
  영향 프로젝트의 이후 분석 Rebuild/Build도 성공했습니다. 두 구성 전체 로직·기본곡
  카탈로그 각각 63개/실패 0, 네 Client smoke 경로씩 총 8개 성공했습니다.
  엔진 기본 SDK 및 실제 숨김 D3D12/NoSound `--window-control` 두 구성 성공:
  native 해상도 중복 제거, 960×540 변경, 잘못된 크기 거부, 지연 OnResize와 렌더러 크기를 검사했습니다.
- OptionsPanelTests 두 구성 성공: 해상도 적용·native 실패·INI 실패 시 복구,
  기본/보조 키 캡처·기본값과의 중복/예약 키 거부·ESC 취소,
  양수/음수 정수 오프셋·잘못된 초안 보존·재실행 설정·미설치 모드 profile 보존을 검사했습니다.
  기존 ESC/popup/스크롤 입력·저장 실패·실제 FMOD Automatic→WASAPI/driver 검사도 성공했습니다.
  OptionTimingTests는 remap된 primary/auxiliary와 Buzz held·입력 로그 매핑,
  양수/음수 오프셋의 노트/틱/BPM/delay/마디선/YME 일치·Area 길이 유지·원본 저장 보존을 검사합니다.
- 두 구성 MusicSelectView, EditorPresentation, GameplayJudgement, GameplayPause,
  ResultPresentation 회귀 성공. 실제 D3D12/FMOD 전체 종류/효과 자동 플레이도 각각
  19노트/85.1837%, 15노트/97.8897%를 유지했습니다.
- MSVC 14.44.35207 Code Analysis(`/analyze`)를 Chart, Modes, MRG.Client,
  ClientLogic, Rhythm.Tests의 실제 Debug/Release x64 include/define/PCH/C++20 설정으로 실행했습니다.
  `/t:Rebuild /p:RunCodeAnalysis=true /p:BuildProjectReferences=false`와 최종 테스트의 분석 Build를 사용했습니다.
  새 진단·파싱 오류 0. 새 테스트의 C26817은 const reference로 수정 후 두 구성 분석/회귀를 통과했습니다.
  기존 C28020(held-key 배열 범위)은 해결된 키 목록의 예약 버퍼로 교체하여 없어졌습니다.
  기존 C26115(EditorAnalysisController:51), Release C6236(GameplayInput:66)은 유지합니다.
  표시 회귀 소스도 각 실행 스크립트의 `/analyze`로 검사했습니다.
- 변경 코드와 직접 호출부의 창 서비스/Canvas 관찰자 수명, resize 알림의 순서,
  옵션의 원자적 게시/실패 복구, 키 소유권·ESC/stale action 경계,
  모드 입력 매핑/표시/로그 일치와 오프셋의 순방향/역방향 좌표 및 편집 캐시를 검토했습니다.
  CheckArchitecture(11프로젝트), 양쪽 git diff --check 성공. 사용자 LaneKeyBeam.cpp와
  Debug/Release Option.ini의 SHA256을 대조해 보존했고 사용자 Songs/Skins는 수정하지 않았습니다.
  로그/캡처는 ignored `build/options-features-2026-10-15`에 있습니다.
- **화면 확인:** 실제 D3D12 1920×1080의 한국어/영어 offscreen 캡처에서 패널 대비,
  해상도·오프셋·8키의 표시/간격/클리핑을 눈으로 확인했습니다.
  실행 창에서 물리 키/마우스 조작 또는 청음하는 수동 검증은 미실시입니다.
  UI 입력은 자동 의도 검사이며 native 창 크기 변경도 숨김 실행의 자동 검사입니다.

## 2026-10-15 공통 UI 색상·옵션 ESC 선행 변경

- [TextManagement](TextManagement.md)의 UiColors.h와 화면별 역할 연결을 적용했습니다.
  옵션 배경은 기존 sky::PaleBorder, 입력 칸은 sky::Pale를 사용하여 대비를 높였습니다.
  그 외 로고/곡 선택/에디터/Result/일시정지의 기존 팔레트 값은 유지했습니다.
  ESC는 마우스 위치와 무관하게 목록/드래그를 정리하고 닫는 프레임의 입력을 소비합니다.
  해상도·모드 키 설정·글로벌 오프셋의 적용 방식은 사용자 확인 대기이며 구현하지 않았습니다.
- Client Debug/Release x64 전체 solution Rebuild 성공, 일반 빌드 경고/오류 0.
  두 구성 전체 로직·기본곡 카탈로그 각각 61개/실패 0, 네 smoke 경로씩 총 8개 성공.
- OptionsPanelTests 두 구성 성공: 배경/필드 대비, 열린 popup의 ESC 정리, 닫는 도중 반복 ESC,
  animation progress 0에서 즉시 닫기, 닫는 동안 stale selection 거부와 재개방을 검사했습니다.
  기존 UTF-8 INI/실패 복구·음원 소유권·실제 FMOD Automatic→WASAPI/driver 전환 검사도 성공했습니다.
  두 구성 MusicSelectView, EditorPresentation, ResultPresentation, GameplayPause 회귀 성공.
- MSVC 14.44.35207 Code Analysis(`/analyze`)를 영향 프로젝트 MRG.Client의 실제
  Debug/Release x64 include/define/PCH/C++20 설정으로 실행했습니다.
  `/t:Rebuild /p:RunCodeAnalysis=true /p:BuildProjectReferences=false`를 사용했습니다.
  신규 진단/파싱 오류 0. 기존 C26115(EditorAnalysisController:51),
  C28020(GameplaySessionController:126), Release C6236(GameplayInput:66)은 유지했습니다.
  표시 회귀 소스도 각 실행 스크립트의 `/analyze`로 검사했습니다.
- 변경 코드와 직접 호출부의 팔레트 역할/기존 alpha 값, Canvas 관찰자 수명/정리,
  ESC 우선 처리·입력 전달 차단·목록/포인터 상태·stale action 경계를 검토했습니다.
  CheckArchitecture(11프로젝트), 양쪽 git diff --check 성공. 사용자 LaneKeyBeam.cpp와
  Debug/Release Option.ini의 SHA256을 대조해 보존했습니다. 엔진·사용자 Songs/Skins는 변경하지 않았습니다.
  상세 로그와 캡처는 ignored `build/options-2026-10-15`에 있습니다.
- **화면 확인:** 실제 D3D12의 1920×1080 옵션 패널 offscreen 캡처에서 배경/필드/문구를
  눈으로 확인했습니다. 사용자 창에서 물리 ESC/마우스를 조작하는 수동 검증은 미실시입니다.
  ESC·popup·포인터 정리 검사는 자동 입력 의도 검사입니다.

## 2026-10-15 플레이 일시정지·마디 되감기

- [Pause](Gameplay/Pause.md)에 조작·복원/소유 경계와 향후 HP·리플레이 필수 계약을 기록했습니다.
  Resume 0.5~3초/처리 상태 유지, Rewind 원본 마디 시작/대상 시각 이상 판정 취소,
  Retry 새 시도 자격·로그 초기화, Retire 미완료 롱 포함 0점·MISS 확정을 적용했습니다.
- Client Debug/Release x64 전체 solution Rebuild 성공. 일반 빌드 경고/오류 0.
  마지막 직접 include/완주 자격 회귀 추가 후 영향 프로젝트의 두 구성 분석 Build도 성공했습니다.
  두 구성 전체 로직·기본곡 카탈로그 각각 61개/실패 0, 네 smoke 경로씩 총 8개 성공했습니다.
- GameplayPauseTests 두 구성 성공: 메뉴 준비 gating, ↑↓/←→/Enter, 0.5~3초 범위,
  클릭 의도·한국어/영어·portrait/wide resize·Canvas 정리, 실제 숨김 D3D12 실행의 시간 고정,
  Resume 판정 보존·음악 소스 seek, BPM/마디 길이 변경 시 4마디 되감기·점수 복원,
  10번째 마디 3/4→6번째 마디 0/1 예시, 음수 재개 DSP 예약·앞으로 이동 방지,
  Retry 자격 복구·Retire 실제 MISS/마커를 검사했습니다. 별도 실제 FMOD NoSound MP3의
  끝부분 seek 후 종료를 검사했고 소스/DSP 설정 검사는 Clip backend probe를 사용했습니다.
  PlayPauseTests는 반복 되감기·전체 입력 로그/제어 경계 보존·Buzz 중간 헤드/틱 보존,
  일곱 롱 종류의 강제 0점 MISS와 완주해도 메뉴 사용 자격이 복구되지 않는지 검사했습니다.
- 두 구성 ResultPresentation(기존 3초 완료→deferred Result→새 Retry/소유권), GameplayJudgement,
  EditorPresentation 회귀 성공. 실제 D3D12/FMOD 전체 노트/효과 자동 플레이는 각각
  19노트/85.1837%, 15노트/97.8897%를 유지했습니다.
- MSVC 14.44.35207 Code Analysis(`/analyze`)를 Modes, MRG.Client, Rhythm.Tests의 실제
  Debug/Release x64 include/define/PCH/C++20 설정으로 실행했습니다.
  `/t:Rebuild /p:RunCodeAnalysis=true /p:BuildProjectReferences=false`와 마지막 영향 영역의
  분석 Build를 사용했습니다. 신규 진단·파싱 오류 0. 기존 C26115(EditorAnalysisController:51),
  C28020(GameplaySessionController:126), Release C6236(GameplayInput:66)은 유지했습니다.
  표시 테스트 소스도 실행 스크립트의 `/analyze`로 분석했습니다.
- 변경 코드와 직접 호출부의 Scene/Canvas/Clip 수명, 판정/trace 분리, 닫힌 취소 경계,
  타이머/DSP 재기준화, 준비/실패/종료 경로, held 입력/긴 노트 복원과 함수 책임을 검토했습니다.
  CheckArchitecture(11프로젝트), 양쪽 git diff --check 성공. 사용자 LaneKeyBeam.cpp와
  Debug/Release Option.ini의 SHA256을 대조해 보존했습니다. 사용자 Songs/Skins와 엔진은 변경하지 않았습니다.
  상세 로그와 캡처는 ignored `build/gameplay-pause-2026-10-15`에 있습니다.
- **화면 확인:** 실제 D3D12의 1920×1080·720×1280 메뉴 캡처를 눈으로 확인했습니다.
  이는 숨김 offscreen 이미지 확인입니다. 실행 창에서 물리 ESC/화살표/마우스로 조작하거나
  실제 소리를 듣는 수동 플레이 검증은 실시하지 않았습니다. 위 UI 입력은 자동 의도 검사입니다.

## 2026-10-14 Penpot 리절트와 엔진 글꼴 굵기

- [플레이 결과](Gameplay/Results.md)에 엔진 기능 점검표·3열 표시·리트라이·책임 경계를,
  [BuiltInAssets](BuiltInAssets.md)에 Scene별 PNG와 글꼴/라이선스를 기록했습니다.
  엔진 fontWeight를 먼저 원격 main에 반영하고 Client gitlink를 갱신했습니다.
- Client/엔진 Debug·Release x64 전체 solution Rebuild 성공. 일반 빌드 경고/오류 0.
  Client 두 구성의 로직·기본곡 카탈로그 각각 58개, 네 smoke 경로씩 총 8개 성공.
  최종 자산팩 갱신 뒤에도 두 구성 Build·ResultPresentation·smoke-test를 다시 성공했습니다.
- ResultPresentation 두 구성 성공: 소수점 6자리·실제 표시 정보·한국어/영어·빈 그래프·
  10,003개 입력의 단일 불변 배치·다른 화면비·Back/Retry 클릭·비활성 Replay를 검사했습니다.
  실제 숨김 D3D12/FMOD 실행에서 노트 처리 후 3초 대기→결과 이동→게임플레이 파기→
  Retry 클릭→새 플레이 세션/Canvas 생성→복귀/정리를 확인했습니다.
  SceneStateTests는 선택이 바뀌어도 이전 곡명/제작자 snapshot을 보존하고 Clear 시 지우는지 검사합니다.
- 두 구성 MusicSelectView, EditorPresentation(Audio/Mode/Menu/Playback), GameplayJudgement,
  OptionsPanel 회귀 성공. 옵션 검사는 실제 FMOD Automatic→WASAPI/driver 전환도 포함합니다.
- 엔진 기본 SDK·`--text-rendering`·`--text-pixels` 두 구성 성공. 기본 굵기 400,
  1..999 경계와 잘못된 값 거절, payload/측정 무효화, 400/800 layout·글리프 분리를 검사했습니다.
  Debug 텍스트 검사의 D3D12 ERROR/CORRUPTION 0.
- MSVC 14.44.35207 Code Analysis를 MRG.Client와 Rhythm.Tests의 실제 Debug/Release x64
  include/define/PCH/C++20 설정으로 `/t:Rebuild /p:RunCodeAnalysis=true /p:BuildProjectReferences=false`
  실행했습니다. Presentation 소스도 각 스크립트의 `/analyze`로 검사했습니다. 신규 진단/파싱 오류 0.
  기존 C26115(EditorAnalysisController:51), C28020(GameplaySessionController:112),
  Release C6236(GameplayInput:60)는 유지하고 억제/범위 밖 수정을 하지 않았습니다.
- 변경 코드/직접 호출부의 결과·callback·Canvas 수명, 선택 snapshot, 파일별 스킨 fallback,
  인덱스/변환, 그림과 입력 좌표, 글꼴 굵기 cache key를 검토했습니다.
  CheckArchitecture(11프로젝트), 양쪽 git diff --check 성공.
- **실제 눈으로 확인:** 1920×1080 한국어 D3D12 렌더 캡처를 Penpot의 좌표/색/폰트/장식과
  비교했습니다. 실제 AngelDream 채보에 자동 입력을 넣은 통계/그래프와 빈 그래프 화면을 확인했습니다.
  캡처의 D3D12 오류 없음도 확인했습니다. 물리 키보드/마우스 조작·청음은 수동 확인하지 않았습니다.
- 원시 로그와 SVG/PNG/BMP/capture probe는 ignored `build/result-penpot-2026-10-14`에 있습니다.
  사용자 LaneKeyBeam.cpp와 두 구성 Option.ini의 SHA256을 대조해 보존했고 사용자 Songs/Skins는 변경하지 않았습니다.

## 2026-10-14 텍스트 렌더링 후속 개선

- Client/엔진 Debug·Release x64 전체 solution Rebuild 성공. 일반 빌드 경고/오류 0.
- 마지막 글꼴 조회 실패의 캐시 게시 경계 수정까지 Debug 전체 Rebuild와
  로직/네 smoke/엔진 SDK·텍스트·headless 오류 경로를 다시 실행해 성공했습니다.
- 각 구성 로직·기본곡 카탈로그 58개와 네 smoke 경로씩 성공.
  GameplayJudgement, EditorAudioView/Mode/Menu/Playback, MusicSelectView,
  OptionsPanel, ResultPresentation 및 일반/효과 실제 D3D12/FMOD replay도 두 구성 성공.
  기존 19노트/85.1837%, 효과 15노트/97.8897% 결과를 유지했습니다.
- 엔진 기본 SDK 검사와 `--headless-error`, 새 `--text-rendering` / `--text-pixels`를
  각각 별도 프로세스로 Debug/Release에서 성공했습니다. 유지 배치 2,049개 재제출의
  layout/rasterization 0, clip 문구 4,096글자 중 실제 제출 5개, 새 한글 512개의
  upload arena 1개, 아틀라스 22→16페이지 회수와 회수 글리프 재생성을 검사했습니다.
  Debug D3D12 validation의 ERROR/CORRUPTION 0. 다중 페이지 합성 readback도 성공했습니다.
- MSVC 14.44.35207 Code Analysis를 변경된 Client 소유 `MRG.Client.vcxproj`의 실제
  Debug/Release x64 include/define/C++20 설정으로
  `/t:Rebuild /p:RunCodeAnalysis=true /p:BuildProjectReferences=false` 실행했습니다.
  표시 회귀 테스트 소스도 실행 스크립트의 `/analyze`로 분석했습니다. 신규 진단/파싱 오류 0.
  기존 C26115(EditorAnalysisController:51), C28020(GameplaySessionController:112),
  Release C6236(GameplayInput:60)은 유지했고 자동 억제/수정하지 않았습니다.
- 변경 코드/직접 호출부의 배치·font-face 소유권, payload 무효화, clip bounds,
  페이지·upload·descriptor fence 수명, 합성 순서, 실패한 글꼴 조회의 캐시 게시 순서와
  범위/변환을 검토했습니다. CheckArchitecture(11프로젝트)와 양쪽 git diff --check 성공.
- Release CPU 비교 두 번과 실제 렌더 샘플 비교는
  [텍스트 개선 기록](Reviews/TextRenderingFixes-2026-10-14.md)에 있습니다.
  원시 로그/BMP/probe는 ignored `build/text-rendering-fixes-2026-10-14`에 있습니다.
- **실제 눈으로 확인:** 밝고 어두운 배경의 영문·한글·일본어 16/24/32px 수정 전후
  D3D12 렌더 이미지에서 글자 경계와 겹친 색상 합성을 비교했습니다.
  전체 게임의 물리 마우스/키보드 조작·청음과 전체 FPS/GPU timestamp는 확인하지 않았습니다.
  smoke와 숨김 엔진 replay, 픽셀 수치 검사는 자동 검증입니다.
- 사용자 LaneKeyBeam.cpp와 Debug/Release Option.ini의 SHA256을 대조해 보존했고
  사용자 Songs/Skins는 변경하지 않았습니다.

## 2026-10-14 성능 보고서 8개 항목 수정

- Client/엔진 Debug·Release x64 전체 solution Rebuild 성공. 마지막 오디오 alias 수정도
  두 구성의 Client 분석 Build와 전체 solution Build로 확인했습니다. 일반 빌드 경고/오류 0.
- 각 구성 58개 로직·기본곡 카탈로그 검사 및 네 smoke씩 총 8개 성공.
  IntervalIndex 경계/구간 겹침, 롱노트 삭제·이동·BPM 변경 후 조회, 사용하지 않는 사운드
  등록 제외를 추가했습니다. 편집 원본 트리/ID와 기존 판정·점수 정밀도를 유지합니다.
- 두 구성 GameplayJudgement(느린 노트/뒤로 이동/배율 변경/resize 포함),
  EditorAudioView/Mode/Menu/Playback(FFT 타일 공유·줌 무효화 포함), MusicSelectView
  (5,000곡 가시 카드/포커스/검색/resize 및 실제 worker 결과), OptionsPanel 회귀 성공.
- 실제 숨김 D3D12/FMOD 자동 replay는 각 구성 전체 19노트/85.1837%와 효과 15노트/97.8897%를
  유지했습니다. SampleAsync/StreamAsync와 스킨 PrepareImages의 준비 완료 후 재생,
  밀집 Buzz/처리된 tick/OK/MISS/진행바/뒤로 이동을 확인했습니다.
  Result 그래프·소수점 6자리·소유권과 실제 deferred Scene 전환도 두 구성 성공했습니다.
- 엔진 SDK 검사와 `--headless-error`의 숨김 치명 오류 정리도 두 구성 성공했습니다.
- MSVC 14.44.35207 Code Analysis(`/analyze`)를 Chart, Modes, ClientLogic, MRG.Client,
  Rhythm.Tests의 실제 Debug/Release x64 include/define/C++20 설정으로 실행했습니다.
  `/t:Rebuild /p:RunCodeAnalysis=true /p:BuildProjectReferences=false`를 사용했고,
  마지막 router 수정은 같은 구성의 분석 Build로 재검사했습니다. 신규 진단/파싱 오류 0.
  기존 C26115(EditorAnalysisController:51), C28020(GameplaySessionController:112),
  Release C6236(GameplayInput:60)은 유지하며 자동 억제/수정하지 않았습니다.
  표시 회귀 테스트 소스도 실행 스크립트의 `/analyze`로 분석했습니다.
- 변경 코드와 직접 호출부의 query 경계·세대 수명·cache 무효화·효과 순서·worker 관찰자·
  async Clip 정리·upload fence 실패/종료 경로를 검토했습니다. CheckArchitecture(11프로젝트),
  양쪽 git diff --check 성공. 사용자 LaneKeyBeam.cpp와 Debug/Release Option.ini의 SHA256을
  실행 전후 대조해 보존했고 사용자 Songs/Skins를 변경하지 않았습니다.
- 같은 Release probe 두 번의 CPU 비교는 [성능 수정 기록](Reviews/PerformanceFixes-2026-10-14.md)에
  있습니다. 로그는 ignored `build/performance-fixes-2026-10-14`에 보관합니다.
- **수동 확인:** 실제 픽셀 배치·물리 입력·청음 및 전체 게임 FPS/GPU frame time은
  수동 확인하거나 측정하지 않았습니다. 위 숨김 엔진 실행과 좌표/캐시 검사는 자동입니다.

## 선택 실행·서식·도메인 CI

`FingerDrum.Rhythm.Tests.exe --list`는 테스트 이름을 나열하고 `--filter Chart`는
이름에 Chart가 있는 검사만 실행합니다. `--catalog-root`와 함께 사용할 수 있습니다.
실패한 검사가 있어도 나머지를 실행하며 실패가 하나라도 있거나 선택 결과가 없으면
종료 코드는 1입니다. 전체 필수 검증에서는 filter를 지정하지 않습니다.

`.clang-format`/`.editorconfig`는 변경 영역의 서식 기준입니다. `.clang-tidy`는
clang-analyzer/bugprone/performance 검사를 설정하며 컴파일 DB에는 실제 x64 프로젝트의
include/define/C++20을 사용해야 합니다. MSVC Code Analysis를 사용하는 경우에는
해당 프로젝트에 `/t:Rebuild /p:RunCodeAnalysis=true`를 지정합니다. 종속 프로젝트를
이미 빌드한 분석 실행에서는 `/p:BuildProjectReferences=false`로 엔진/SDK 재분석을 피합니다.

`.github/workflows/domain-tests.yml`은 Windows의 Debug/Release에서
`Tests/Rhythm/FingerDrum.Rhythm.Tests.vcxproj`를 직접 빌드하고 로직·기본곡 카탈로그를
검사합니다. 엔진 submodule과 FMOD SDK를 가져오지 않습니다. 이 CI는 Client 전체 빌드,
presentation·smoke·실제 오디오/렌더링 검사나 수동 확인을 대신하지 않습니다.

## 2026-10-12 뎅뎅 자유 시작·순서 오류의 카운트 보존

- TimedSequenceInputRule이 첫 유효 입력으로 순서의 시작을 선택하고 다음 요구 action을
  보관합니다. TaikoLongNoteFactory는 뎅뎅에서만 자유 시작을 켜며 풍선의 동 전용은 유지합니다.
  거부된 입력은 Unjudged로 내보내고 카운트/다음 action/점수를 변경하지 않습니다.
  Reset은 카운트와 순서 상태를 초기화합니다. 구간/요구 횟수/성공·실패 시점은 유지합니다.
- TaikoSoundPolicy는 뎅뎅의 실제 인정된 inputAction으로 두 sound binding을 선택합니다.
  캇 시작도 Kat 사운드를 사용하며 명시적 YME 히트사운드는 우선합니다.
  타격수만큼 binding을 만들던 홀짝 방식은 제거했습니다. 파일 형식·엔진 코드는 변경하지 않았습니다.
- Debug/Release x64 전체 solution Rebuild 성공, 일반 빌드 경고/오류 0.
  각 구성 55개 로직·기본곡 카탈로그 검사와 네 smoke씩 총 8개 성공.
  TestDengDengEitherStart는 동/캇 시작, 같은 색 반복 시 카운트/상태/다음 색 보존,
  구간 전 입력·키 해제의 시작 선택 방지, 실제 입력 사운드·명시적 사운드 우선,
  Reset 후 반대 색 시작, 최종 100%와 2/3의 66.666666%, POOR/오차 통계 제외를 검사합니다.
- 두 구성의 GameplayJudgement 회귀는 양쪽 시작과 잘못된 반복을 섞어 실제 Presenter의
  POOR/마커 억제, 흰색 키빔, 카운트 유지와 성공 OK를 확인했습니다.
  실제 숨김 D3D12/FMOD 전체 노트 replay는 각 19개 노트 완료/85.1837%로 기존 기대값을 유지합니다.
  두 구성의 Result 그래프·소수점 6자리·소유권·실제 deferred Scene 전환도 성공했습니다.
- MSVC Code Analysis(`/analyze`, 실제 C++20/include/define/x64 설정)는 변경된 Rhythm,
  Modes, Rhythm.Tests 프로젝트를 Debug/Release Rebuild로 검사했습니다. 경고/오류 0.
  표시·실제 재생·Result 회귀 소스도 실행 스크립트의 MSVC `/analyze`로 컴파일했습니다.
  변경 코드와 직접 호출부의 상태 전이·소유권·입력 경계·인덱스 변환·사운드 중복을 검토했습니다.
- CheckArchitecture(11개 프로젝트)·git diff --check 성공. 사용자 LaneKeyBeam.cpp·곡/스킨·
  Option.ini를 보존했고 기존 파일/INI 해시가 실행 전후 동일합니다.
  실제 픽셀 배치·물리 키/마우스 조작·청음은 수동 확인하지 않았습니다.
  로그는 ignored `build/dengdeng-start-2026-10-12`에 보관합니다.

## 2026-10-12 X 양끝 이동·마디선 스크롤 통일·횟수형 롤 헤드 판정 제거

- Workspace/Input은 X를 마지막 노트(테일 포함)→음원 끝으로 연결합니다.
  Z/X 변경·수동 Seek·Space는 이동 순서를 초기화합니다. AnalysisController가 현재 파일의
  완료된 분석 길이만 제공하며, 준비되지 않았으면 현재 시간과 두 번째 X 재시도를 보존합니다.
  X 입력에서 파일을 다시 읽거나 디코딩하지 않습니다.
- TaikoSessionBuilder는 살아남은 표시 마디 시작의 Separate 배율을 컴파일하고 PlaySession은
  시각/배율과 표시 범위의 최소 속도를 보관합니다. Presenter의 LaneObjectPosition으로
  노트·마디선·틱을 같은 Whole 거리/Separate 배율/중심 좌표에 배치합니다.
  마디선의 모양·표시 여부는 표시 책임으로 유지하며 판정/점수/오디오에는 참여하지 않습니다.
- RuleBasedNote의 HitCount Preview는 Unjudged입니다. Roll/BigRoll/Balloon/DengDeng은
  시작 입력/헤드 정확도 없이 구간 안의 타격수로 점수를 결정합니다. 동/캇 제한, 소수점 6자리
  부분 점수와 OK/MISS 완료 표시는 유지하며 TickRoll/Buzz의 시작 판정은 그대로입니다.
- Debug/Release x64 전체 solution Rebuild 성공, 일반 빌드 경고/오류 0.
  각 구성 54개 로직·기본곡 카탈로그 검사와 네 smoke씩 총 8개 성공.
  새 도메인 검사는 늦은 첫 타격의 100%, 양 끝 입력·2/3 점수·범위 밖 입력,
  TickRoll/Buzz 시작 판정 보존, 마디선의 실제 시간 Area 보간·Whole 거리·느린 표시 범위,
  음수 delay의 잘린 시작·잘못된 배율 교체 실패 시 기존 데이터 보존을 확인합니다.
- 두 구성의 EditorAudioView/Mode/Menu/Playback 회귀 성공. 실제 음원 분석 캐시로 연속 X,
  테일/음원 끝 구분, Z와의 전환·수동 Seek 초기화·분석 무효화 후 거부/재시도를 확인합니다.
  두 구성의 GameplayJudgement 회귀는 여러 시각·해상도에서 노트/마디선 중심 일치와
  네 횟수형 롤의 헤드 문구/마커 억제·백색 키빔·OK/MISS를 확인합니다.
- 실제 숨김 D3D12/FMOD 자동 replay 성공: 각 구성 전체 종류 19개 노트(85.1837%),
  효과 채보 15개 노트(97.8897%) 완료. 밀집 Buzz의 기존 GOOD 입력/미스 기대값도 유지합니다.
  두 구성의 Result 회귀는 그래프·소수점 6자리·결과 소유권·실제 deferred Scene 전환을 확인합니다.
- MSVC Code Analysis(`/analyze`, 실제 C++20/include/define/x64 설정)는 Rhythm, Chart, Modes,
  Rhythm.Tests, MRG.Client를 Debug/Release로 Rebuild했습니다. 새 경고/오류 0.
  기존 C26115(AudioAnalysisWorker의 scoped lock), C28020(GameplaySessionController의 tm),
  Release C6236(GameplayInput)는 남아 있으며 이번 동작 변경으로 추가된 진단은 없습니다.
  표시 회귀 테스트 소스도 각 스크립트의 MSVC `/analyze`로 컴파일했습니다.
- CheckArchitecture(11개 프로젝트)·git diff --check 성공. 엔진 코드는 변경하지 않았습니다.
  사용자 LaneKeyBeam.cpp·곡/스킨·현재 Option.ini를 보존했습니다. 실행 전후 INI 해시가 동일합니다.
  자동 검사는 실제 픽셀 배치·물리 키/마우스 조작·청음을 수동 확인한 결과가 아닙니다.
  로그는 ignored `build/editor-end-seek-2026-10-12`에 보관합니다.

## 2026-10-12 에디터 시간축 줌·DSP 재생

- 실시간 레인/서클 크기는 유지하고 −/+로 시간축을 25~400% 조절합니다.
  EditorPlayback은 음악/예상 head·tick의 source/cue 캐시, DSP 시각, 자기 Voice의 수명만 맡습니다.
  Workspace가 Space 재생/일시정지와 Z 첫 노트→0ms 순서를 연결합니다.
  다른 수동 Seek/Space는 Z 순서를 초기화하며 재생 중 Seek는 재생 의도를 유지합니다.
  당시 미정이던 X는 위 후속 검증의 마지막 노트→음원 끝 이동으로 연결했습니다.
- Client/엔진 Debug·Release x64 전체 solution Rebuild 성공, 일반 빌드 경고/오류 0.
  두 구성의 52개 로직·기본곡 카탈로그와 네 smoke 경로씩 총 8개 성공.
  엔진 SDK 테스트는 길이/us 시작 위치/DSP 예약 전달, Loading Clip의 지연 해제와
  Ready 후 해제를 검사했습니다. 엔진을 먼저 원격에 반영한 뒤 Client gitlink를 갱신합니다.
- 두 구성의 EditorAudioViewTests/EditorModeTests/EditorMenuTests/EditorPlaybackTests 성공.
  실제 View의 줌 버튼/배율 문구, 다섯 배율의 동일한 레인·헤드 크기/스냅,
  source beat·컴파일 시각·문서 revision 보존, 연속 Z와 수동 Seek 초기화를 검사했습니다.
  제어 가능한 DSP clock으로 음수 pre-roll/일시정지·재개/5ms 틱 예약/1,000회 캐시·Voice 재사용,
  UI 정지 뒤 지난 cue 건너뛰기/종료 시각/오류 시 정지/범위 밖 Seek 보존을 확인했습니다.
  실제 AngelDream MP3의 정확한 비동기 열기/길이 조회/끝부분 seek·종료/범위 밖 거부와
  EditorPlayback의 음악 끝 도달은 FMOD NoSound로 검사했습니다.
- OptionsPanelTests, 실제 D3D12/FMOD AllNotesGameplayTests와 ResultPresentationTests도
  두 구성 성공. 기존 일반 재생·예약·설정/결과 전환 경로를 유지합니다.
- MSVC 14.44.35207 Code Analysis: 실제 include/define/C++20 및 Debug·Release x64로
  `MRG.Client.vcxproj /t:Rebuild /p:RunCodeAnalysis=true /p:BuildProjectReferences=false` 실행.
  Seek 검증 순서/한국어·영어 안내 문구의 최종 수정도 분석 Build로 확인했습니다.
  파싱/컴파일 오류 및 새 분석 경고 0. 기존 C26115(EditorAnalysisController:51),
  C28020(GameplaySessionController:98), Release C6236(GameplayInput:60)은 그대로 보고합니다.
  위 presentation 테스트 소스도 `/analyze`로 분석했으며 새 경고가 없습니다.
- CheckArchitecture(11프로젝트)와 양쪽 git diff --check 성공. 기존 LaneKeyBeam.cpp,
  Debug·Release Option.ini는 SHA256이 같고 사용자 Songs/Skins를 수정하지 않았습니다.
  로그는 ignored `build/editor-playback-2026-10-12`에 있습니다.
- **수동 확인:** 실제 픽셀 배치·물리 키/마우스 조작·청취는 수행하지 않았습니다.
  위 Canvas 좌표/문구/명령 검사는 자동이며, GPU smoke와 NoSound 재생도 자동 실행입니다.
  YME 볼륨/DSP 효과 미리듣기와 자유 롤/풍선 파열 시각의 생성은 이 구현에 포함하지 않습니다.

## 2026-10-12 마지막 노트 기준 상단 진행바

- GameplayProgressView가 유효한 노트의 마지막 head/tail 시각을 진입 때 캐시합니다.
  기존 RhythmTimer 시각으로 PNG 채움/마커와 현재/전체 m:ss를 갱신하며 진행을 0~100%로
  제한합니다. 음원 길이·판정 유예·Result 대기 시간은 포함하지 않습니다.
  배경/트랙/채움/마커 PNG 4개를 별도로 제작하고 단일 GameProgressBar.png를 제거했습니다.
  선택 스킨의 파일별 fallback/내장 팩을 사용합니다. 기본 팩은 71개에서 74개 파일이 됩니다.
- Debug/Release x64 전체 solution Rebuild 성공, 일반 빌드 경고/오류 0.
  두 구성의 로직/기본곡 카탈로그 52개(1곡/5패턴), 네 smoke 경로씩 총 8개 성공.
  GameplayJudgementTests의 실제 Presenter/View 검사로 마지막 롱테일, 0/50/100%, 음수 시작,
  종료 이후 유지, 타이머 일시정지/재개, Reset, 네 해상도, BPM/delay 컴파일 시각,
  빈/0/음수 종료 시각, 1,000회 갱신의 노드/이미지 재사용을 확인했습니다.
- 두 구성의 ResultPresentationTests 성공: 기존 결과 데이터 이동/실제 Scene 전환을 유지합니다.
  AllNotesGameplayTests의 실제 D3D12/FMOD 실행에서 AngelDream 19노트와 dense Buzz 회귀,
  진행바의 중간 진행과 종료 시 100%를 확인했습니다. 스크립트는 `/W4 /analyze`로
  테스트 소스도 분석합니다. 기존 Require(song != nullptr)가 분석기에 전달되지 않아 나온
  C6011은 같은 테스트 안의 명시적인 null 검사로 정리했습니다.
- MSVC 14.44.35207 Code Analysis: 실제 Client 프로젝트의 include/define/C++20과 Debug/Release x64로
  `Client/MRG.Client.vcxproj /t:Rebuild /p:RunCodeAnalysis=true /p:BuildProjectReferences=false` 실행.
  파싱/컴파일 오류 및 변경 코드의 새 분석 경고 0. 기존 C26115(EditorAnalysisController:51),
  C28020(GameplaySessionController:98), Release C6236(GameplayInput:60)은 별도 기존 항목입니다.
  억제하거나 진행바 범위 밖으로 수정하지 않았습니다. 진행바/판정 테스트 소스의 분석도 경고 0입니다.
- CheckArchitecture(11프로젝트)와 git diff --check 성공. LaneKeyBeam.cpp의 기존 사용자 변경,
  Debug/Release Option.ini, 사용자 Songs/Skins는 보존했습니다. 엔진 코드/gitlink는 변경하지 않았습니다.
  빌드·분석·최종 회귀 로그는 ignored `build/progress-2026-10-12`에 있습니다.
- PNG 파일을 직접 열어 분리 구성요소를 확인했습니다. 실제 게임 화면의 픽셀 배치,
  물리 키보드/마우스 조작과 청취는 수동 검증하지 않았습니다. 실제 렌더/재생은 위 자동 실행입니다.

## 2026-10-12 Result·입력 로그·6자리 정확도

- AccuracyScore는 퍼센트 소수점 6자리의 정수 단위이며 입력 판정/논리 노트/평균에서
  아래 자리를 버립니다. 기존 등급·판정 창·노트별 가중치는 유지했습니다.
  IPlayResultCollector/IPlayResult와 Taiko 구현은 실제 생성 노트·정수 us 입력·실제
  타이밍 오차를 수집합니다. ResultScene은 이동된 데이터와 모드별 IResultView를 소유하고
  gameplay는 모든 노트 처리 3초 뒤/ESC로 종료합니다. 미처리 0점은 실제 MISS와 구분합니다.
- Debug/Release x64 전체 solution Rebuild 성공: 일반 빌드 경고/오류 0.
  두 구성에서 로직/기본곡 카탈로그 52개 성공(새 점수·결과·롱/Reset 검사 3개 포함),
  네 smoke 경로씩 총 8개 exit 0. 마지막 서식 정리 후 영향 프로젝트의 분석 Build 및
  52개 로직/카탈로그도 다시 실행했습니다.
- 실제 프로젝트 include/define/C++20 설정의 MSVC 14.44.35207 Code Analysis를
  FingerDrum.Rhythm/FingerDrum.Modes/FingerDrum.Rhythm.Tests/MRG.Client 각각 Debug/Release에
  `/t:Rebuild /p:RunCodeAnalysis=true /p:BuildProjectReferences=false`로 실행했습니다.
  파싱/컴파일 오류와 새 분석 경고 0. 기존 C26115(EditorAnalysisController),
  C28020(GameplaySessionController의 heldStorage), Release C6236(GameplayInput)는 유지되며
  억제나 범위 밖 수정은 하지 않았습니다. ResultPresentationTests 소스의 초기 C6011은
  명시적인 null 검사로 수정했고 재분석에서 경고 0입니다.
- ResultPresentationTests는 두 구성 성공: 한국어/영어, 정확한 6자리 표시, 실제 입력으로
  만든 부호 있는 오차/히스토그램, 빈 상태, 10,003개 샘플의 단일 RectangleBatch,
  반복 update에서 일정한 draw list, 좁은/넓은 화면 배치와 pointer-router 버튼 의도를 확인했습니다.
  숨김 D3D12/FMOD 실행의 한 노트 회귀 세션에서 생산용 gameplay → Result factory 전환,
  처리 후 3초 대기, unique_ptr 소비, 이전 gameplay Canvas 제거와 Result 종료 정리를 확인했습니다.
- GameplayJudgementTests도 두 구성 성공. 실제 D3D12/FMOD 전체 노트 자동 replay는
  두 구성 19개 완료/85.1837%로 기존 규칙을 유지했습니다(Debug 1,557 / Release 1,541프레임).
  종료 정확도의 기록 정밀도는 6자리이며 이 기존 replay의 콘솔 출력 정밀도는 4자리입니다.
- CheckArchitecture(11개 프로젝트)와 git diff --check 성공. 변경 코드/직접 호출부의
  수명·경계/변환·중복 이벤트·실패 처리와 책임 분리를 검토했습니다. 완료 이벤트 검색은
  각 노트에 terminal state를 저장하여 대량 자동 MISS에서 결과 배열을 반복 탐색하지 않습니다.
  결과는 backend 객체를 참조하지 않고 그래프는 진입/언어/해상도 변경에만 구성합니다.
- 로그는 ignored `build/result-2026-10-12`입니다. 기존 LaneKeyBeam.cpp와 Option.ini,
  사용자 곡/스킨은 보존했습니다. 엔진·변환기 소스/자산 변경은 없습니다.
  숨김 실행과 자동 입력/포인터 의도 검사이며 실제 픽셀·물리 ESC/키·마우스·청음은
  수동 확인하지 않았습니다. 디스크 기록/리플레이 재생/곡 선택 기록 표시는 이번 범위 밖입니다.

## 2026-10-12 개발/배포 자산 경로·난이도별 파일 쌍

- FingerDrum.Assets의 ExecutableDirectory는 EXE 위치를, UserAssetRoot는 개발용
  `../../../Client/Assets` 또는 배포용 `Asset`을 프로세스마다 한 번 결정합니다.
  Songs/기본 외부 스킨/SkinSetSelection이 같은 루트를 사용하고, OptionSettings는
  자산 폴더와 관계없이 EXE 옆 INI를 사용합니다. Scene/엔진/내장 팩의 범위는 유지합니다.
- Client 빌드는 기본곡을 개발용 Songs에 없는 경우에만 복사하며 난이도별 하위 폴더를
  보존합니다. AngelDream 5개 난이도는 Patterns/angeldream 아래 이름별 YMP/YME 쌍입니다.
  효과가 없는 4개에 빈 YME를 만들었고, 효과 채보의 YME/Sounds는 같은 난이도로 옮겼습니다.
  기존 노트/타이밍/효과/사운드 데이터는 그대로이며 Effect file 경로만 변경했습니다.
  프로젝트/필터와 직접 경로를 사용하는 디버그·Buzz/효과/실제 재생 테스트도 갱신했습니다.
- 개발용 사용자 YMP 5개도 난이도별 폴더로 정리하고 빈 YME를 연결했습니다.
  같은 Stream 이름은 기존 1.5x 파일명으로 폴더만 구분했습니다. 변경 전 원본은 ignored
  `build/asset-layout-2026-10-12/OriginalUserPatterns`에 보관했고 노트 데이터 동일성을
  검사했습니다. Omoiyoshino는 Music/Omoiyoshino 및 Patterns/Omoiyoshino/Inner Oni에
  복사했습니다. YMP 참조 두 필드 외 데이터는 그대로이며 음원/YMM/YME의 SHA-256이
  기존 변환본과 같습니다. 사용자 곡/스킨은 Git/내장 팩에 넣지 않았습니다.
- Debug/Release x64 전체 solution Rebuild 성공, 최종 일반 빌드 경고/오류 0.
  Release의 첫 시도는 ClientLogic CL.exe 종료 코드 1(MSB6006, 추가 컴파일 진단 없음)로
  실패했으며 빌드 병렬도를 4로 제한한 전체 Rebuild에서 성공했습니다.
- 실제 include/define/C++20 구성의 MSVC 14.44.35207 Code Analysis를
  FingerDrum.Assets/MRG.Client/FingerDrum.Rhythm.Tests에 Debug/Release 각각
  `/t:Rebuild /p:RunCodeAnalysis=true /p:BuildProjectReferences=false`로 실행했습니다.
  파싱/컴파일 오류와 새 분석 경고 0. 기존 C26115(EditorAnalysisController),
  C28020(GameplaySessionController), Release C6236(GameplayInput)는 그대로이며 억제를
  추가하지 않았습니다. AssetPathTests 및 옵션/실제 재생 테스트 소스도 MSVC 분석을 사용합니다.
- 두 구성의 기본곡 카탈로그/로직 49개 모두 성공(1곡/5패턴). 분리된 배포용 Songs에
  기본곡과 Omoiyoshino를 넣은 검사도 두 구성 49개 성공(2곡/6패턴), 각 Taiko 세션을
  생성하고 저장 왕복 및 파일 존재를 확인했습니다.
- 추가 검사인 개발용 Songs 전체 검사는 Debug에서 기존 사용자 채보 5개의
  `#measurelinevisible` 명령으로 TestSongCatalog가 실패했습니다(49개 중 1개).
  현재 YMP가 지원하지 않는 구형 명령이며 이동 전 파일에도 동일하게 존재합니다.
  요청 범위를 넘어 삭제/재해석하지 않았고 원본을 보존했습니다. 이 결과를 전체 성공으로
  취급하지 않습니다. 제품 카탈로그는 기존 규칙대로 해당 패턴만 제외합니다.
- AssetPathTests는 두 구성에서 같은 EXE를 독립된 개발/배포 트리에 복사하고 작업
  폴더를 반대로 지정해 실행했습니다. Songs/외부 기본 스킨/INI 위치 모두 성공했고
  사용자 설정/스킨/내장 캐시는 변경하지 않았습니다. 옵션 저장 및 실제 FMOD
  Automatic→WASAPI/드라이버 변경 회귀도 두 구성 성공했습니다.
- 개발 실행의 네 smoke는 각 구성 성공했습니다. Release EXE를 별도 TEMP Game 폴더에
  복사하고 `Asset/Songs`만 배치한 뒤, CWD를 개발용 Assets로 지정해 실행한 네 smoke도
  성공했습니다. 경로만 변경한 EXE에서 내장 기본 스킨/글꼴도 정상 초기화됐습니다.
- 실제 D3D12/FMOD 자동 replay는 두 구성 모두 전체 종류 19개 완료/85.1837%/
  1,557프레임, 효과 15개 완료/97.8897%/1,302프레임으로 기존 결과와 동일합니다.
  창은 숨김이고 입력은 자동 공급입니다. 실제 픽셀/물리 키·마우스/청음은 수동 확인하지
  않았습니다. 구조 검사는 11개 프로젝트, git diff --check도 성공했습니다.
- 로그와 사용자 파일 hash 기록은 ignored `build/asset-layout-2026-10-12`입니다.
  기존 LaneKeyBeam.cpp 및 Debug/Release Option.ini의 hash를 보존했습니다.
  엔진 및 변환기 C++ 코드는 변경하지 않았습니다. 변환기의 향후 파일 배치 변경은
  사용자에게 적용 범위를 질문한 상태이며 확정 전에는 구현하지 않습니다.

## 2026-10-12 변환기 안내의 Client 최신 문서 참조

- 변환기 Core/Converter.cpp의 ClientRulesNotice는 판정/슬라이더/스피너 안내에
  Client `main`의 `Docs/Gameplay/Rules.md` 링크를 붙입니다. 세부 틱 끝점/입력/점수 규칙과
  기본 JudgeLevel 수치를 별도 설명으로 복제하지 않습니다. Converter README와 Client
  OsuTaikoConverter 문서도 같은 판정 문서를 기준으로 하고 구형 테일 제외 설명을 제거했습니다.
- 경고 문자열 외 변환 로직, JSON 필드 구조, YMM/YMP/YME/음원과 플레이 코드/엔진은
  변경하지 않았습니다. JSON의 warnings는 기존 배열 형식을 유지합니다. 공통 함수는
  호출 중 메시지를 소유 string으로 복사해 반환하며 Scene/런타임 소유권에 영향이 없습니다.
- Debug/Release x64 전체 solution Rebuild 성공: 경고/오류 0. 실제 구성의 include/define/C++20
  설정으로 Converter.Core/EXE/Tests 3개 프로젝트의 MSVC 14.44.35207 Code Analysis를
  각 구성 `/t:Rebuild /p:RunCodeAnalysis=true /p:BuildProjectReferences=false`로 실행했습니다.
  파싱/컴파일 오류 및 분석 경고 0이며 억제는 추가하지 않았습니다.
- 각 구성 변환기 Core/기존 Omoiyoshino 카탈로그·861개 세션 및 CliTests.ps1 성공.
  게임 EXE 옆의 최신 변환기로 같은 원본 복사본을 새 ignored TestData 출력에 재변환했습니다.
  두 구성 exit 0, 861개 객체/864행/최대 이동 1.1428571428114083ms 유지.
  판정/롤/스피너 안내 3개에 최신 문서 링크가 있고 구형 테일 제외 안내는 없습니다.
  새 YMM/YMP/YME/음원을 이전 검증 결과와 파일 해시로 대조해 모두 동일함을 확인했습니다.
- 각 구성 Client 로직/기본 카탈로그 49개/실패 0, 네 smoke 총 8개 exit 0.
  CheckArchitecture 성공(11개 프로젝트), Client/변환기 git diff --check 성공.
  기존 runtime Omoiyoshino 파일과 Option.ini, 사용자 LaneKeyBeam.cpp를 보존했습니다.
  실행 폴더의 변환기 EXE는 정적 분석 후 빌드된 최신 EXE로 갱신했습니다.
- 이번 수정은 안내와 문서 변경이며 수동 UI/물리 입력/청음은 확인하지 않았습니다.
  변환 데이터가 동일하므로 이전의 861개 실제 D3D12/FMOD replay 결과를 아래 기록에서
  참고합니다. 이번 작업에서 곡 전체 replay를 다시 실행한 것으로 보고하지 않습니다.

## 2026-10-12 Omoiyoshino / Inner Oni 실제 변환·재생

- 입력은 설치된 osu!의 `1006535 loos feat Hiiragi Rian - Omoiyoshino` 폴더,
  `loos feat. Hiiragi Rian - Omoiyoshino (Konpaku Sariel) [Inner Oni].osu`입니다.
  v14/Mode 1, BeatmapID 2106494, 제작자/난이도를 확인했습니다.
- 최신 Debug/Release OsuTaikoConverter.exe를 기본 분모 1~32로 실행했습니다.
  양쪽 exit 0 / JSON success true. 원본 861개(일반 858, 슬라이더 1, 스피너 2)가
  YMP 864행/실제 Taiko 861개 논리 노트로 보존됐습니다. 877개 이벤트를 스냅했고
  최대 이동은 1.1428571428114083ms입니다. 두 구성의 생성 텍스트/음원은 동일합니다.
- 결과는 변환기 ignored `TestData/Omoiyoshino-InnerOni-20261012/{Debug,Release}/Songs`
  아래에 있습니다. 게임 두 구성의 `assets/Songs/Imported/Omoiyoshino - Inner Oni (Konpaku Sariel)`에도
  YMM/YMP/YME/audio.mp3를 복사하고 파일 해시를 대조했습니다. 원본 osu/MP3의 해시도 유지됐습니다.
  YMP→YMM은 Songs 기준, YMP→YME와 YMM→음원은 각 파일 폴더 기준으로 로드됐습니다.
- 변환기 Core/실제 카탈로그·세션 테스트가 두 구성 성공했습니다.
  CliTests.ps1도 해당 원본 복사본으로 두 구성 성공: 변환, 기존 출력 보존,
  필수/중복/미지원 인자, 잘못된 격자, 출력 범위, 없는 입력과 미지원 Mode 종료 코드를 검사했습니다.
- Release의 실제 D3D12/FMOD(출력 1, NoSound fallback 금지)에서 숨김 자동 재생 성공:
  채보 끝까지 861개 완료, 자동 입력 정확도 100%, 8,130 렌더 프레임.
  기존 GameplayPresenter/GameplayAudioRouter/카탈로그/PlaySession을 사용했습니다.
  임시 replay는 두 스피너의 요구 입력을 각각의 실제 롱 구간 안에 분산했습니다.
  오디오/자동화 오류, 중복 완료, 종료 후 노트 노드 누적 없이 완료했고 Reset 재생성도 검사했습니다.
- 검증용 소스/로그는 Client ignored `build/omoiyoshino-2026-10-12`에만 둡니다.
  임시 probe의 초기 C6011은 명시적 null 검증으로 수정하고 Release 실제 include/C++20/MD 설정의
  MSVC 14.44.35207 `/W4 /c /analyze` 재실행에서 경고/오류 0을 확인했습니다.
  제품 C++·프로젝트/엔진 코드는 변경하지 않았으며 기존 빌드 실행 파일을 사용했습니다.
- Debug/Release 네 Client smoke 총 8개 exit 0. 기존 Option.ini와 사용자 LaneKeyBeam.cpp를
  보존했습니다. 문서 변경의 git diff --check 성공. 실제 화면 픽셀·물리 입력·청음은 수동 확인하지 않았습니다.
- 변환 경고의 배경/영상/스토리보드와 커스텀 샘플 미지원, JudgeLevel/스피너 판정 차이는
  기존 변환 계약입니다. 추가로 `exclude the tail` 안내가 현재 Client의 닫힌 틱 구간과
  다른 구형 문구임을 확인했습니다. 이번 검증에서는 경고 문구의 제품 코드를 변경하지 않았습니다.

## 2026-10-12 테일 틱의 닫힌 구간

- MusicalTimeline::CompileSubdivisions는 헤드 기준 유리수 격자에 테일이 정확히 맞고
  그 위치가 유효할 때 마지막 틱을 추가합니다. us 변환 전에 판단하여 반올림으로 생긴
  일치는 제외합니다. 같은 위치의 닫힌 구간은 한 틱이고 역전/0분할 구간은 비어 있습니다.
  CountSubdivisions는 자유 연타의 자동 목표용으로 기존 끝 제외 개수를 유지합니다.
- TaikoLongNoteFactory와 TaikoAudioPreview의 기존 공유 호출을 통해 플레이/오디오 예측에
  함께 적용됩니다. Buzz 헤드는 별도 시작 판정이며 몸통 틱에 중복 포함되지 않습니다.
  기존 RuleBasedNote/틱 규칙/Presenter가 테일의 성공·실패, 정확도 분모와 처리 후 숨김을
  그대로 담당합니다. 종료 시점·파일 형식·Scene/엔진 소유권은 변경하지 않았습니다.
- 회귀는 정렬/비정렬 테일, us 반올림, 헤드 기준 격자, BPM/마디 길이/양수 delay,
  음수 delay의 무효 테일/유효 경계, TickRoll/BigTickRoll/Buzz의 테일 판정·중복 완료 방지,
  표시/리셋/OK·MISS와 에디터 오디오 예측을 검사합니다. AllNotesReplay의 밀집 Buzz
  기준도 테일이 포함된 일반 몸통 8틱에 맞춰 기존 늦은 헤드 입력 대상을 유지했습니다.
- Debug/Release x64 전체 solution Rebuild 성공: 일반 빌드 경고/오류 0.
  각 구성 로직·실제 카탈로그 49개/실패 0, 네 smoke route 총 8개 exit 0.
  Editor audio/mode/menu 및 Gameplay judgement 회귀도 두 구성 성공했습니다.
- 각 구성 실제 D3D12/FMOD 숨김 자동 replay 성공: 전체 노트 19개 완료/85.1837%/1557프레임,
  효과 채보 15개 완료/97.8897%/1302프레임. 밀집 Buzz의 +50ms GOOD 헤드 실패 틱은
  2/9/38개로 유지하며, 정렬된 테일을 포함한 몸통은 32/128/512개입니다.
  테일이 정렬되지 않은 효과 채보의 몸통 37틱/실패 2개 결과는 유지됐습니다.
- MSVC 14.44.35207 Code Analysis/NativeRecommendedRules를 Chart, Modes, Rhythm.Tests,
  MRG.Client의 실제 x64 Debug/Release include/define/C++20/PCH 설정으로 실행했습니다.
  BuildProjectReferences=false로 엔진/SDK 재분석을 제외했고 Editor 표시 3개 번역 단위와
  GameplayJudgementTests도 두 구성 /analyze 성공했습니다. 파싱/컴파일 오류와 새 경고 0.
  기존 C26115(EditorAnalysisController.cpp:51), C28020(GameplaySessionController.cpp:64),
  Release C6236(GameplayInput.cpp:60)는 이번 수정 범위 밖이며 억제하지 않았습니다.
- CheckArchitecture 성공(11개 프로젝트), git diff --check 성공.
  사용자 LaneKeyBeam.cpp와 Debug/Release Option.ini의 파일 해시가 유지됐습니다.
  엔진 변경은 없으며 실제 픽셀 배치·물리 키보드/마우스 입력·청음은 수동 확인하지 않았습니다.

## 2026-10-12 delay 무효 구간·롱노트/Area 끝점

- 음수 delay가 덮는 일반 노트·틱·BPM·delay·YME 지점/히트사운드 변경을 컴파일 결과에서
  제외하고 원본 마디·박자·ID를 보존합니다. 롱노트와 Area는 기존 끝점 시각을 유지하고,
  끝이 시작보다 앞설 때만 제외합니다. 같은 시각이면 유지하며 0길이 Area는 To를 적용합니다.
  원본 박자 기준 끝점의 순서는 여전히 검증합니다. 엔진이나 파일 버전은 변경하지 않았습니다.
- MusicalTimeline은 원본 위치 prefix와 실제 시간 기준점, 살아남은 BPM 상태, 무효 구간,
  표시용 마디 범위를 분리합니다. 침범된 BPM/delay를 되돌린 상태로 이후 시간을 계산하고,
  Area 보간과 효과 조회는 실제 us를 사용합니다. 소스 무효 구간은 이진 검색하며,
  틱 개수는 제거될 틱 시각 배열을 만들지 않고 구간 인덱스로 계산합니다.
- ChartEditor는 원본 균형 트리/롱 연결/안정 ID를 유지합니다. 시간 변경 때 유효성·시간·속도·
  효과 시간 인덱스를 후보에서 계산하고 함께 반영합니다. 효과 인덱스의 포인터는 소유 map의
  노드를 참조하며 교체·복사 회귀로 수명을 확인했습니다.
- TaikoChartNotes는 플레이 생성·오디오 예측·에디터 표시/배치에서 source 순서의 롱 결합과
  같은 제외 규칙을 공유합니다. TaikoEditorScore의 악보 표시와 스냅은 표시 마디 시간을 쓰고,
  실시간 마디선은 Whole 거리만 사용합니다. Scene/오디오 backend의 소유권은 변경하지 않았습니다.
- Debug/Release x64 최종 전체 solution Rebuild 성공: 일반 빌드 경고/오류 0.
  각각 로직·실제 카탈로그 48개/실패 0, 네 Client smoke route 총 8개 exit 0.
  각 구성 Editor audio/mode/menu와 Gameplay judgement 회귀 성공.
- 새 회귀는 2/4→3/4→4/4 예시와 원본 1/4 위치 보존, 경계/마디 중간의 양·음수 delay,
  닫힌 마디 끝, 여러 마디/중첩 침범, BPM/delay 상태 복원, 양수 gap의 남은 시간,
  시작 전 시간 조회, 무효 틱/count 일치, 저장/편집/복사 후 원본/ID/인덱스 복원을 검사합니다.
  7종 Taiko 롱 타입에서 시작만·끝만·양쪽 무효 및 같은/역전 시각을 검사했습니다.
  Area의 동일 정책과 실제 시간 보간, 지점·히트사운드 무효화, 에디터 표시/스냅 일치도 검사했습니다.
- 각 구성 실제 D3D12/FMOD 숨김 자동 replay 성공: 전체 노트 19개 완료/85.1694%/1557프레임,
  효과 채보 15개 완료/97.8897%/1302프레임. 밀집 Buzz의 +50ms GOOD 헤드 입력과
  기존 missed-tick 기대값도 유지했습니다.
- MSVC 14.44.35207 Code Analysis/NativeRecommendedRules를 Chart, Modes, Rhythm.Tests,
  MRG.Client의 실제 x64 Debug/Release include/define/C++20/PCH 설정으로 실행했습니다.
  BuildProjectReferences=false로 엔진/SDK 재분석을 제외했습니다. Editor 회귀 테스트의
  3개 번역 단위도 두 구성 /analyze 성공. 파싱/컴파일 오류와 최종 새 경고 0.
  초기 C26820 시간 기준점 복사는 참조로 수정하고 최종 분석을 재실행했습니다.
  기존 C26115(EditorAnalysisController.cpp:51), C28020(GameplaySessionController.cpp:64),
  Release C6236(GameplayInput.cpp:60)는 범위 밖으로 구분했고 억제하지 않았습니다.
- CheckArchitecture(11개 프로젝트), git diff --check 통과. 실행 전후 기존 Option.ini SHA256 동일.
  사용자 곡/스킨과 기존 LaneKeyBeam.cpp 작업을 보존했습니다.
- 실제 픽셀 배치·물리 키보드/마우스 입력·청음은 수동 확인하지 않았습니다.
  자동 Canvas/입력 회귀와 실제 GPU/오디오 실행 결과를 수동 확인으로 간주하지 않습니다.
- 원시 로그: ignored `build/delay-2026-10-12/`의 Release 검사와 `final-Debug/`의 최종 Debug 검사.
  재현 실행기: `build/VerifyDelay.ps1`, 마지막 수정 후 Debug 재검증은 `build/VerifyDelayFinalDebug.ps1`.

## 2026-10-12 노트 마디·박자 입력

- 사용자 선택에 따라 노트 정보 패널의 시작·끝은 마디(1부터)/박자(N/D, 온음표 기준)만
  입력합니다. ms는 저장된 노트의 컴파일 결과를 읽기 전용으로 표시합니다. 일반 노트에서
  롱노트로 바꿀 때 끝점을 명시적으로 입력하고, 일반 노트로 돌아오면 연결된 끝점만 제거합니다.
- TaikoEditorNoteInspector가 선택 snapshot과 미적용 초안/입력 UI를 소유합니다.
  기존 Position 파서, Taiko 배치 검증, Workspace UpdateNote와 ChartEditor의 부분 수정
  트랜잭션을 재사용합니다. 추가 시간 API·객체·엔진 변경이나 파일 문법 변경은 없습니다.
  문구는 TaikoEditorTexts의 한국어/영어 표에 추가했습니다.
- Debug/Release x64 전체 solution Rebuild 성공(일반 빌드 경고/오류 0).
  각각 로직·실제 카탈로그 44개/실패 0, 네 smoke route 총 8개 exit 0.
  두 구성 Editor audio/mode/menu 회귀 성공: 시작/끝 입력, 일반↔롱노트 변환,
  안정 ID/구간 인덱스, 빈 끝점·0/100,001 마디·마디 밖 분수·ms 입력·동일 끝점의
  원본/revision 보존, BPM 수정 후 위치/ID 보존과 시간 갱신, 한국어/영어 필드와
  ms 버튼이 없는 것을 검사했습니다.
- MSVC 14.44.35207 Code Analysis/NativeRecommendedRules를 MRG.Client의 실제 x64
  Debug/Release include/define/C++20/PCH로 실행했습니다. BuildProjectReferences=false로
  엔진/SDK 재분석을 제외했습니다. 세 Editor 회귀 테스트 단위도 두 구성 /analyze 통과.
  파싱/컴파일 오류와 새 경고 0. 기존 C26115(EditorAnalysisController.cpp:51),
  C28020(GameplaySessionController.cpp:64), Release C6236(GameplayInput.cpp:60)는
  이번 수정 범위 밖이며 억제하지 않았습니다.
- CheckArchitecture(11개 프로젝트), git diff --check 통과. 실행 전후 Option.ini SHA256 동일.
  사용자 곡/스킨 및 기존 LaneKeyBeam.cpp 작업은 보존했습니다.
- 실제 화면 픽셀 배치·물리 키보드/마우스 조작은 수행하지 않았습니다. 위 회귀는
  자동 입력/Canvas draw packet과 실제 Client 실행 검사입니다.
- 이 검사 당시 delay 후속 요청은 미적용이었습니다. 이후 확정된 정책의 구현과 검증은
  위의 delay 무효 구간·롱노트/Area 끝점 절에 기록했습니다.
- 원시 로그: ignored `build/editor-timing-2026-10-12/`의 rebuild, analysis, logic, editor,
  architecture와 diff-check 로그. 재현 실행기: `build/VerifyEditorTiming.ps1`.

## 2026-10-12 에디터 노트 속성·롤 표시

- 현재 구현한 범위: 선택 스킨의 서클/overlay·풍선·뎅뎅, 여섯 도구 묶음과 W/E/R/T/Y/U
  순환, 롤 종류/요구 연타·틱 간격, 우측 노트 선택/속성 영역, 처리된 틱 숨김,
  성공 OK/실패 MISS와 사용하지 않는 기본 JudgeImage PNG 7개 제거입니다.
- 이 검사 당시 타이밍은 읽기 전용이었습니다. 후속 사용자 결정과 시작/끝 입력 및
  일반↔롱노트 변환의 완료 검증은 위의 노트 마디·박자 입력 절에 기록했습니다.
- TaikoEditorNoteInspector는 선택 ID/초안을 소유하고 Workspace가 UpdateNote를 연결합니다.
  ChartEditor는 선택한 시작/끝과 시간/속도 인덱스만 준비한 뒤 교체하며 실패 시 원본을 보존합니다.
  본체 Scene, 음악 clock·판정·점수·오디오 및 파일 문법은 바꾸지 않았습니다.
  스킨 경로는 선택 revision별로 캐시하고 이미지는 공통 renderer가 소유합니다.
- Debug/Release x64 전체 solution Rebuild 및 마지막 변경의 solution Build 성공.
  최종 일반 빌드 경고/오류 0. 각 구성 로직·카탈로그 44개, 실패 0(실제 1곡/5패턴),
  smoke-test/lobby/gameplay/editor 총 8개 exit 0. 기존 Option.ini SHA256 보존.
- Editor audio/mode/menu 회귀 두 구성 성공. 버튼/단축키의 6개 묶음 순환·복귀,
  기본 PNG 경로/종류·연타 라벨, 시작/끝 ID 및 무관 옵션 보존, 유효하지 않은 수정의
  원본/revision 보존과 직렬화 왕복, 화면 밖 시작/끝을 가진 몸통의 선택과 커서 보존을 검사했습니다.
- Gameplay judgement 회귀 두 구성 성공. 조기 수용된 미래 틱의 즉시 숨김, Reset 복원,
  모든 틱 성공의 OK/누락 틱의 MISS와 200ms 페이드 및 기존 입력 우선순위를 검사했습니다.
  음수 delay로 틱의 작성/시간 순서가 달라지고 동일 시각의 틱이 둘 있는 채보에서도
  두 틱 처리 후 다음 위치의 마커만 남는 것을 검사했습니다. 표시 목록은 규칙과 같은 시간순입니다.
- 실제 D3D12/FMOD 자동 전체 노트 replay 두 구성 성공: 19개 완료, 정확도 85.1694%,
  각각 1,557 렌더 프레임. 기존 점수/틱 성공·실패 결과와 같고 오디오 오류가 없습니다.
- MSVC 14.44.35207 Code Analysis/NativeRecommendedRules를 실제 x64 Debug/Release의
  include/define/C++20/PCH로 Rhythm, Chart, Modes, Rhythm.Tests, MRG.Client에 실행했습니다.
  마지막 Modes/Client 수정도 재분석했고 테스트 C++ 단위도 /analyze를 통과했습니다.
  파싱/컴파일 오류와 새 경고 0. 새 C4244/C26829는 키 코드 타입/명시적 끝점 검증으로 수정했고
  억제하지 않았습니다. 기존 C26115(EditorAnalysisController.cpp:51), C28020
  (GameplaySessionController.cpp:64), Release C6236(GameplayInput.cpp:60)는 범위 밖으로 남겼습니다.
- CheckArchitecture(11개 프로젝트), 물리 폴더/필터/엔진 경계와 git diff --check 통과.
  엔진 및 사용자 곡/외부 스킨/Option.ini를 변경하지 않았습니다.
  OK는 built-in image_gen으로 GREAT를 참조해 만들고 원본 그대로 복사했으며,
  1254×1254 RGBA와 중앙/모서리 alpha 0을 확인했습니다. 프롬프트는 JudgementAssets에 있습니다.
- **수동 확인:** 실제 화면 픽셀 배치·물리 키보드/마우스·청음은 수행하지 않았습니다.
  draw packet/노드·자동 입력/실행 검사 및 생성 이미지 관찰과 구분합니다.
- 원시 로그: ignored `build/editor-notes-2026-10-12/`의 rebuild/analysis/final/selection/
  completion 검사 로그. 후속 마디·박자 입력의 원시 로그는 위 절의 별도 폴더에 있습니다.

## 2026-10-12 코드 품질 재검토 수정

- [품질 수정 기록](Reviews/QualityFixes-2026-10-12.md)에 따라 보간식의 런타임 실패 복구,
  Separate 실제 노트 배율의 조회 범위, 편집/플레이 효과 검증 불일치와 숨김 엔진 실행의
  모달 오류를 수정했습니다. 분석 Controller의 복사/이동도 금지했습니다.
- 엔진과 Client Debug/Release x64 solution 전체 Rebuild 성공(컴파일 경고/오류 0).
  엔진 SDK 기본 테스트와 실제 D3D12/FMOD NoSound `--headless-error`도 두 구성에서 통과했습니다.
  숨김 Update 예외 후 실패 반환/Client Shutdown 호출과 30초 안의 비차단 종료를 확인했습니다.
- 두 구성 각각 로직 44개, 실패 0. 실제 카탈로그는 1곡/5패턴입니다.
  새 세 테스트는 샘플 사이의 특이점·음수 볼륨, 트랙/다음 명령별 정상값 유지와 복구,
  Reset/SetEffects, 0.25배 Separate 조회/인덱스 편집, DTO 검증 일치와 식 재컴파일을 검사합니다.
- 각 구성 smoke-test/lobby/gameplay/editor 총 8개 exit 0. Editor audio/mode/menu와
  Gameplay judgement/error HUD 회귀도 두 구성에서 통과했습니다.
  느린 노트 그리기·삭제, 느린 노트 삭제 후 스냅 그리드의 존속, 오류 HUD 표시/숨김을
  프로덕션 객체와 자동 좌표/노드 검사로 확인했습니다.
- 실제 D3D12/FMOD 자동 플레이도 두 구성에서 전체 종류 19개/85.1694%/1,557프레임과
  효과 채보 15개/97.8897%/1,302프레임을 완료했습니다. 기존 결과와 같고 오디오 오류가 없습니다.
- MSVC 14.44 Code Analysis/NativeRecommendedRules를 실제 include/define/PCH/C++20과
  x64 Debug/Release 설정으로 Chart, Modes, Rhythm.Tests, MRG.Client 네 프로젝트에 실행했습니다.
  BuildProjectReferences=false로 엔진/SDK 재분석을 제외했고 파싱/컴파일 오류 및 새 경고가 없습니다.
  Editor 회귀 3개 및 GameplayJudgementTests의 테스트 단위도 두 구성 `/analyze`를 통과했습니다.
  기존 경고는 EditorAnalysisController.cpp:51 C26115, GameplaySessionController.cpp:64
  C28020(이전 62행, 구현 동일), Release GameplayInput.cpp:60 C6236입니다. 억제하지 않았습니다.
- CheckArchitecture(8개 프로젝트), 물리 폴더/필터, 의존성 순환/엔진 경계와 git diff --check 통과.
  기존 Option.ini는 실행 전후 SHA256이 같고 사용자 곡/스킨은 변경하지 않았습니다.
- 실제 화면 픽셀 배치·물리 입력·청음은 수동 확인하지 않았습니다. 자동 표시/입력과
  실행/정리 검사를 수동 확인으로 보고하지 않습니다.

## 2026-10-06 에디터 자체 상단 저장 메뉴

- `EditorMenuBar`는 Canvas 노드의 비소유 참조와 메뉴 상태만 유지합니다.
  EditorInput이 키/화면 좌표를 메뉴 의도로 변환하고 EditorView가 저장 요청을
  기존 Workspace Save에 전달합니다. 하단 저장과 Ctrl+S도 유지했습니다.
  탭/박자 디바이더를 메뉴 아래에 배치하고 문구는 EditorTexts 표에 추가했습니다.
  엔진·게임플레이·차트 형식·저장 구현은 변경하지 않았습니다.
- Debug/Release x64 전체 solution Rebuild와 최종 Client 빌드 성공.
  두 구성의 로직/카탈로그(실제 곡 1개·패턴 5개), 네 smoke route씩 총 8개 모두 exit 0.
- 두 구성의 Editor audio/mode/menu 회귀 성공. 메뉴 저장 의도의 1회 발생,
  Escape/외부 좌·우클릭 소비, 닫힌 메뉴의 입력 반환, 한국어/영어/미저장 표시,
  1920×1080·1280×720·900×720·2560×1080 좌표와 정리 및 기존 화면 공존을 확인했습니다.
  기존 타임라인/오디오 분석/모드/저장 adapter 회귀도 같은 실행 파일에서 통과했습니다.
- MSVC Code Analysis는 실제 Client 프로젝트 설정(C++20/PCH/include/define)으로
  Debug 63개·Release 57개 translation unit을 `/analyze`로 컴파일했습니다.
  마지막 메뉴 수정도 Release에서 재분석했으며 파싱/컴파일 오류와 새 경고는 없습니다.
  테스트 소스 3개도 두 구성에서 `/analyze` 컴파일, 경고 없이 통과했습니다.
  변경하지 않은 기존 경고는 `EditorAnalysisController.cpp:51`의 C26115,
  `GameplaySessionController.cpp:62`의 C28020(두 구성),
  `GameplayInput.cpp:60`의 C6236(Release)입니다. 억제하거나 범위를 넓혀 수정하지 않았습니다.
- CheckArchitecture(8개 프로젝트), 프로젝트/필터/엔진 경계, `git diff --check` 통과.
  사용자 곡·스킨·Option.ini와 엔진은 변경하지 않았습니다.
- 자동 입력 의도/좌표와 draw packet, smoke 실행을 검사했습니다. 실제 화면 픽셀 배치,
  물리 키보드/마우스 조작과 청취는 수동 확인하지 않았습니다.

## 2026-10-06 판정 인디케이터·마커·링/문구

- `GameplayJudgementView`가 색 구간·마커·7종 링/문구 노드와 표시 수명을 소유합니다.
  Presenter가 기존 NoteEvent를 전달하고 LaneKeyBeam의 색상/입력 응답 정책을 공유합니다.
  점수·노트 판정 규칙·오디오·파일 형식과 엔진은 변경하지 않았습니다.
- 기본 판정레벨에 따른 구간 길이, BAD 바깥 ±10ms 여유, 플레이 중 기본 범위 유지,
  싱코페이션 입력의 실제 오차 표시, 빠름/늦음/0/끝점 제한과 7종 판정을 검사했습니다.
  적색 키빔의 조건에만 POOR를 표시하고 범위 밖 입력은 백색 키빔만 생성합니다.
  자동 MISS에는 새 입력 마커가 없으며 키 해제/정확도 없는 tick은 표시를 시작하지 않습니다.
- 판정 표시 회귀는 실제 내장/fallback PNG 메타데이터를 읽고 프로덕션 객체를 링크합니다.
  200ms 페이드/재입력/reset/종료, 판정원 중심·부모 회전과 똑바른 문구 방향,
  좁은 창과 판정레벨 100, 노드/이미지 재사용, MISS와 성공이 겹친 입력의 우선순위를
  Debug/Release에서 통과했습니다. 새 테스트 소스의 MSVC `/W4 /analyze`도 경고/오류 없습니다.
- Debug/Release x64 솔루션 전체 Rebuild와 최종 Client 분석 빌드 성공.
  두 구성 로직·카탈로그(1곡/5패턴), 판정 표시/기존 키빔 회귀 성공,
  각 구성 smoke-test/lobby/gameplay/editor 종료 코드 0.
- 실제 D3D12/FMOD 자동 플레이도 두 구성 전체 노트 19개/85.1694%/1,557프레임,
  효과 채보 15개/97.8897%/1,302프레임을 완료했습니다.
  자동 replay의 실제 입력 결과도 판정 표시 경로로 전달하며 오디오 오류 없습니다.
- MSVC 14.44 Code Analysis/NativeRecommendedRules를 실제 MRG.Client 프로젝트의
  include/define/PCH/C++20과 x64 Debug/Release 설정으로 실행했습니다.
  각각 62/56개 분석 컴파일 호출 및 최종 변경 단위 재분석이 성공했습니다.
  새 코드 경고/오류/분석 파싱 오류는 없습니다. 기존 경고는 Debug 2개/Release 3개:
  EditorAnalysisController.cpp:51 C26115(잠금 해제), GameplaySessionController.cpp:62
  C28020(12개 키 배열 범위), Release GameplayInput.cpp:60 C6236(상수 디버그 조건)입니다.
  해당 원본 코드와 엔진/SDK는 이 작업에서 수정하지 않았습니다.
- 8개 프로젝트의 소스/필터/의존성/엔진 경계 및 git diff --check 통과.
  새 PNG는 내장 기본 스킨에 포함되며 사용자 곡·스킨·Option.ini는 보존했습니다.
  이미지 생성 도구/프롬프트는 [판정 이미지 기록](Gameplay/JudgementAssets.md)에 있습니다.
- 수동 확인: 생성된 PNG의 문구/형태를 도구 출력에서 확인했고 중심/모서리 투명도를
  파일에서 검사했습니다. 실제 게임 화면의 픽셀 배치·물리 키 입력·마우스 조작·청음은
  수동 확인하지 않았습니다. 위 실제 엔진 플레이와 좌표 검사는 자동 검증입니다.

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

## 2026-10-06 코드 품질 검사 지적 수정

- 다른 세션의 `build/quality-2026-10-06/Report.md`와 48개 고유 경고를 검토했습니다.
  `ParseDouble`은 strtod의 반환 포인터가 필드의 실제 끝인지 검사하여 NUL 뒤의 추가 값을
  거부합니다. 임시 문자열 할당 실패도 기존 noexcept/false 반환 계약 안에서 처리합니다.
  YMP의 BPM/offset/delay와 YME 값의 잘못된 입력, 기존 부호·지수·16진수/공백 표기를 검사했습니다.
- 자산 초기화의 오류 저장은 별도 StoreInitializationError가 맡습니다. 오류 메시지의 할당도
  실패하면 빈 메시지와 false를 반환합니다. 자원 없는 별도 실행 파일에서 두 실패 경계의
  C++ 할당을 강제로 실패시켜 Debug/Release 모두 종료 없이 처리함을 확인했습니다.
  이 테스트는 실제 캐시를 생성/수정하지 않으며 `/analyze` 빌드의 경고·오류도 없습니다.
- YMM/YMP/YME 파서는 경로를 문서로 이동하고 진단은 문서가 소유한 경로를 참조합니다.
  외부 함수 서명은 유지했습니다. 사운드 정책의 읽기 전용 문자열과 동기 DecodeAudio의
  stop token 복사를 줄이고, 의미 없는 배열 move를 제거했습니다.
  순서 입력은 인덱스를 먼저 검사하고 iterator 변환을 명시합니다. 시간은 정수 µs로 유지하며
  표시/점수의 부동소수점 변환만 명시했습니다. 테스트의 optional 접근과 VK 값/문자열도 정리했습니다.
- clang-tidy 19.1.5: 수정한 Rhythm/Chart/Modes/Editor/Assets/Rhythm.Tests 6개 프로젝트의
  48개 번역 단위를 Debug x64와 Release x64에서 각각 분석했습니다.
  각 프로젝트의 MSBuild ClangTidy 컴파일 DB로 실제 include/define/C++20 설정을 사용했고,
  `clang-analyzer-*`, `bugprone-*`, `performance-*`를 실행했습니다. 96개 실행 모두 파싱 오류 없이 완료.
  엔진/표준 라이브러리/SDK는 수정 및 Client 진단 집계 범위에서 제외했습니다.
- 고유 경고는 기존 48개에서 각 구성 18개로 감소했습니다. 복사/불필요한 move, 암시적 변환,
  명시적 noexcept 경계, 테스트 optional/문자 처리 지적을 해소했으며 새 경고는 없습니다.

| 남은 진단 | 수 | 검토 및 처리 |
| --- | ---: | --- |
| 쉽게 바뀔 수 있는 인자 순서 | 10 | 이름과 직접 호출부의 순서를 확인했습니다. 실제 뒤바뀜은 없고, 타입을 구분하기 위한 API/자료형 재설계를 추가하지 않았습니다. |
| enum 크기 | 6 | TaikoAction은 NoteAction, 노트 ID는 파일의 int 계약을 사용합니다. 작은 UI enum과 보간 명령의 크기 변경도 실제 성능 이득을 확인하지 못해 유지했습니다. |
| 암시적 생성자의 exception-escape | 2 | EffectDocument/ParseResult 및 해당 STL map의 기본/이동 생성자는 실제 MSVC Debug/Release 모두 noexcept=false임을 trait 실행으로 확인했습니다. Client가 예외를 금지한 계약 위반은 아니며, 경고를 없애기 위한 특수 멤버 추가/억제는 하지 않았습니다. |

- 원래 보고서의 MSVC C26115는 break 시 unique_lock 소멸자가 해제하는 경로이고,
  C28020은 4개 바인딩 × (primary 1 + secondary 2)가 크기 12의 배열 안에 기록되는 경로임을
  재검토했습니다. 두 Client 소스는 수정하지 않았습니다. 현재 검사의 raw 로그/CSV/컴파일 DB는
  `build/quality-fixes-2026-10-06/{Debug,Release}`에 있으며 원래 검사 산출물은 보존했습니다.
- Debug/Release x64 솔루션 전체 Rebuild와 최종 테스트 수정 후 증분 Build 성공.
  일반 빌드 경고·오류 없음. 두 구성 로직·카탈로그(1곡/5패턴), 4개 smoke,
  EditorAudioViewTests/EditorModeTests, FailureBoundaryTests 성공.
- 실제 D3D12/FMOD replay도 두 구성 성공: 효과 채보 15개/97.8897%/1,302프레임,
  기존 전체 종류 채보 19개/85.1694%/1,557프레임. 효과 Buzz는 2개 실패/35개 성공,
  기존 밀집 Buzz 63개 로직 조건도 유지했습니다. 오디오 오류 없음.
- 프로젝트/필터/의존성/엔진 경계와 git diff --check 통과. 엔진·파일 형식·사용자 곡/스킨은
  변경하지 않았습니다. 실제 화면/물리 키·마우스/청취는 수동 확인하지 않았습니다.

## 2026-10-11 Claude 리뷰 수정

- [리뷰 반영표](Reviews/ClaudeReview-2026-10-11.md)의 변경을 검증했습니다.
  사용자 선택에 따라 마디 수는 최대 100,000이며, 마지막 마디 경계 조회와
  범위를 넘는 파일/직접 문서/편집 입력을 구분합니다.
- Debug/Release x64 솔루션 전체 Rebuild 성공. 최종 경계 조건 수정 후 두 구성의
  증분 Build와 MSVC Code Analysis도 성공했습니다. 일반 빌드의 새 경고/오류는 없습니다.
- 두 구성 로직·카탈로그 각각 **41개, 실패 0**(실제 1곡/5패턴).
  `--filter`로 선택한 테스트가 없으면 종료 코드 1을 반환하는 것도 확인했습니다.
  밀집 Buzz의 63개 판정 경계 조건은 그대로 통과합니다.
- 저장 중 각 원본 파일의 rename을 잠금으로 실패시켜 이전 파일/dirty/revision 보존,
  임시 파일 정리 및 재시도 성공을 검사했습니다. 기존 복구 파일은 보존하고,
  YMP/YME 같은 대상 저장은 거부합니다. 프로세스 강제 종료나 OS 복구 실패의
  완전한 원자성을 보장하는 테스트는 아닙니다.
- UTF-8 한글 경로·BOM·알 수 없는 metadata Warning·파일 읽기 진단, 100,000마디
  경계 및 큰 정수 입력, 부분 타이밍/효과 수정의 ID·범위 조회·소유권·실패 보존을 검사했습니다.
  Taiko 롱노트 겹침 거부/인접 허용, 실패한 배치의 pending 상태와 기존 파일의
  건너뛴 노트 Warning도 검사했습니다.
- 두 구성에서 smoke-test/lobby/gameplay/editor, EditorAudioViewTests/EditorModeTests/
  EditorMenuTests, OptionsPanelTests, FailureBoundaryTests, GameplayJudgementTests 성공.
  옵션 검증은 실제 FMOD Automatic → WASAPI 및 현재 드라이버 전환도 포함합니다.
  실제 ASIO 장치 검증은 수행하지 않았습니다.
- 독립 폴더에 EXE와 FMOD DLL을 복사하여 잘못된 언어, 잘못된 INI 문법,
  미설치 미들웨어, INI 경로가 디렉터리인 경우를 각각 실행했습니다.
  두 구성 총 8건 모두 기본 설정으로 시작하고 smoke 종료 코드 0이며 원본을 보존했습니다.
  기존 bin 폴더의 사용자 Option.ini는 변경하지 않았습니다.
- 실제 D3D12/FMOD 자동 replay도 두 구성 성공: 전체 종류는 19개 완료/85.1694%,
  Debug 1,554프레임·Release 1,557프레임. 효과 채보는 15개 완료/97.8897%,
  두 구성 모두 1,302프레임이며 Buzz 37틱 중 35개 성공/2개 실패와 오디오 오류 없음을 확인했습니다.
- MSVC **14.44.35207 Code Analysis**, Native Recommended Rules, 실제 프로젝트의
  include/define/C++20 및 Debug/Release x64 설정으로 Rhythm/Chart/Modes/ClientLogic/
  Rhythm.Tests/Client 6개 프로젝트를 분석했습니다. 분석 명령은 Debug 111개,
  Release 105개이며 파싱/컴파일 오류와 새 경고는 없습니다. 엔진/SDK는 Client 수정 범위 밖입니다.

| 기존 MSVC 진단 | 위치 | 확인 결과 |
| --- | --- | --- |
| C26115 (두 구성) | EditorAnalysisController.cpp:51 | break 이후 unique_lock 소멸자가 해제하는 경로. 이번에 수정하지 않은 코드. |
| C28020 (두 구성) | GameplaySessionController.cpp:62 | 4개 바인딩 × 3개 키가 12개 배열 안에 기록됨. 이번에 수정하지 않은 코드. |
| C6236 (Release) | GameplayInput.cpp:60 | Debug 전용 조건이 Release에서 상수인 기존 경로. 억제나 범위 밖 수정을 추가하지 않음. |

- clang-format/clang-tidy 19.1.5로 설정 파일을 읽을 수 있음을 확인했습니다.
  이번 C++ 정적 분석 실행 도구는 위 MSVC Code Analysis이며 clang-tidy 실행으로 혼동하지 않습니다.
- 8개 프로젝트의 소스/필터/의존성/엔진 경계와 `git diff --check` 통과.
  엔진 및 사용자 곡/스킨은 변경하지 않았습니다. FMOD/엔진을 체크아웃하지 않는
  GitHub Windows 도메인 CI도 `e36fda2`에서 Debug/Release 빌드·테스트에 성공했습니다
  ([실행 38140842315](https://github.com/4dblackhole/MyRhythmGame-Client/actions/runs/38140842315)).
- 로컬 원시 로그: `build/review-rebuild-{debug,release}.log`, `build/review-quality/`
  (구성별 분석 로그와 Summary.csv), `build/review-final-*.log`, `build/review-startup-results.log`.
- **수동 확인:** 실제 화면 배치·마우스/물리 키 입력·청취는 수행하지 않았습니다.
  위 D3D12/FMOD replay와 옵션 입력은 숨겨진 창 및 지정 입력을 사용하는 자동 검증입니다.

## 2026-10-11~12 레거시·기술 예제 제거

- 변경 범위와 유지한 현재 계약은 [레거시 제거 기록](Reviews/LegacyAudit-2026-10-11.md)에 있습니다.
  ColoredCube 계열 소스 13개·문서 5개와 프로젝트/필터 등록 및 예제 진입 분기를 제거했습니다.
  구형 설정 가져오기·빈 모드의 Taiko 대체·호환 헤더/별칭도 제거하고,
  YMP의 알 수 없는 타이밍 명령은 오류로 처리합니다. 효과는 YME 소유입니다.
- Client와 엔진의 Debug/Release x64 솔루션 전체 Rebuild 성공.
  최종 곡 선택 진입 검사도 Debug에서 증분 분석 빌드 후 smoke로 확인했고,
  Release 전체 재빌드에는 해당 수정이 포함됩니다. 일반 빌드의 새 경고/오류는 없습니다.
  엔진 공개 API/구현 변경은 없으며 두 구성의 MRG.Collision.Tests는
  audio/Camera/collision/Visual2D SDK 검사를 모두 통과했습니다.
- 두 구성의 로직·카탈로그 각각 **41개, 실패 0**(실제 1곡/5패턴).
  빈/미지원 모드의 세션 생성과 에디터 factory 거부, 구형 YMP 효과 명령의
  오류/행 번호 및 현재 타이밍/YME 처리를 검사했습니다.
  기존 밀집 Buzz 63개 판정 경계 조건도 통과했습니다.
- 두 구성에서 EditorAudioViewTests/EditorModeTests/EditorMenuTests,
  OptionsPanelTests, FailureBoundaryTests, GameplayJudgementTests, LaneKeyBeamTests 성공.
  옵션은 최초 기본값과 INI 지속성 및 실제 FMOD Automatic → WASAPI/드라이버 전환을 검사했습니다.
  실제 ASIO 장치 검증은 수행하지 않았습니다. 테스트 채보도 Mode: Taiko를 명시하도록 수정했습니다.
- 실제 D3D12/FMOD 자동 replay는 두 구성 모두 전체 종류 19개 완료/85.1694%/
  1,557프레임, 효과 채보 15개 완료/97.8897%/1,302프레임입니다.
  효과 Buzz는 37틱 중 35개 성공/2개 실패이며 오디오 오류는 없습니다.
- 최종 두 구성의 smoke-test/lobby/gameplay/editor 총 8개 종료 코드 0.
  실행 전후 기존 bin 폴더의 Option.ini 해시가 동일하며 사용자 곡/스킨 원본의 Git 변경은 없습니다.
- MSVC **14.44.35207 Code Analysis**, Native Recommended Rules, 실제 프로젝트의
  include/define/PCH/C++20 및 Debug/Release x64 설정으로 Rhythm/Chart/Modes/ClientLogic/
  Rhythm.Tests/Client 6개 프로젝트를 분석했습니다. 파싱/컴파일 오류와 새 경고는 없습니다.
  엔진/SDK/생성 파일은 Client 분석 대상에서 제외했습니다.
  수정하지 않은 기존 진단은 EditorAnalysisController.cpp:51 C26115와
  GameplaySessionController.cpp:62 C28020(두 구성), GameplayInput.cpp:60 C6236(Release)이며,
  억제나 범위 밖 수정을 추가하지 않았습니다. Presentation 분석 빌드도 새 경고 없이 통과했습니다.
- CheckArchitecture(8개 프로젝트)의 소스/필터/의존성/엔진 경계와 git diff --check 통과.
  엔진 문서 커밋을 먼저 원격에 반영한 뒤 Client gitlink를 갱신했습니다.
  별도 엔진 검증 실행 프로젝트는 추가하지 않았습니다.
- 로컬 원시 로그: `build/legacy-2026-10-11/`의 engine/solution/analysis/logic,
  Run*Tests/keybeam/replay/effects 및 final-smokes 로그. 앞선 실패 로그 대신 최종 구성별 로그를 기준으로 합니다.
- **수동 확인:** 실제 화면 배치·물리 키/마우스 조작·청취는 수행하지 않았습니다.
  위 D3D12/FMOD replay와 UI 입력/좌표 검사는 자동 검증입니다.

## 2026-10-12 osu!taiko 독립 변환기

- 같은 부모 폴더의 private `OsuTaikoConverter` 저장소에 C++20 Core/EXE/Tests를
  추가했습니다. Client는 EXE 프로젝트를 빌드하고 실행 폴더에 복사합니다.
  기존 파서/게임플레이/엔진 구현과 사용자 곡/스킨은 변경하지 않았습니다.
- Debug/Release x64 전체 Client 솔루션 Rebuild 성공, 일반 빌드 경고·오류 없음.
  변환기 회귀 테스트와 실제 EXE CLI 테스트도 두 구성 모두 통과했습니다.
  1..32 모든 분모, 추가 격자/동률/무제한 스냅, omit/변칙마디/미터 변경,
  빨강·초록 동시점/SV 초기화/볼륨/Kiai, 노트·틱롤·뎅뎅과 오류 경계를 검사했습니다.
  CLI 검사는 한글 경로, JSON/종료 코드, 기존 출력 보존 및 잘못된 인자를 포함합니다.
- D 드라이브 osu!의 `PANDORA PARADOXXX (Firce777) [Firce777's Taiko]`와 오디오를
  변환기 `TestData/Pandora`에 복사했습니다. 원본/복사본 osu SHA256은
  `DF4B13E78047F0DE652B0AB965D84C38106D1F549E95CAA033CB368F9A68D7FE`로 같습니다.
  기본 격자와 추가 48로 변환한 원본 오브젝트 1,233개를 실제 SongCatalog와
  TaikoMode 세션에서 모두 읽었습니다. 출력 노트 행 1,236개(롱노트 끝 포함),
  스냅 이벤트 352개, 최대 이동 약 1.333334ms입니다.
  생성 YMP/YME는 기존 파서·직렬화 왕복 및 실제 MusicalTimeline의 2us 오차 검사도 통과합니다.
- 기존 로직·카탈로그 테스트 두 구성 각각 44개, 실패 0.
  smoke-test/lobby/gameplay/editor 두 구성 총 8개 종료 코드 0.
- MSVC 14.44 Code Analysis, 실제 include/define/C++20 및 Debug/Release x64로
  변환기 Core/EXE/Tests(구성별 새 C++ 번역 단위 6개)와 MRG.Client를 분석했습니다.
  변환기 경고·오류와 Client 새 경고는 없습니다. 기존 Client 진단은
  EditorAnalysisController.cpp:51 C26115 및 GameplaySessionController.cpp:64 C28020
  (두 구성), GameplayInput.cpp:60 C6236(Release)이며 범위 밖 수정을 하지 않았습니다.
- CheckArchitecture의 11개 프로젝트 소스/필터/의존성 검사와 git diff --check 통과.
  원시 로그는 `build/converter-logs/`의 solution/analysis/logic/cli 로그와
  `real-map-Release.json`, 실제 출력은 변환기 `TestData/Songs/Pandora-Release`에 있습니다.
  맵·오디오·생성 결과·빌드 산출물은 Git에서 제외합니다.
- 배경/스토리보드/커스텀 샘플은 경고와 함께 생략하며 Client 스킨을 사용합니다.
  실제 화면·물리 입력·청음은 수동 검증하지 않았습니다. CLI/포맷 통합이며
  Client의 가져오기 UI나 프로세스 실행 Controller를 추가한 작업은 아닙니다.
