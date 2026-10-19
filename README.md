# MyRhythmGame-Client

비공개 리듬게임 **Y's Orchestral Unison: Melodic Ultimatum**의 Client 코드와 게임 자산 저장소입니다.
재사용 엔진은 `Dependencies/MRG-Engine` private submodule입니다.
게임 표시 이름은 [GameIdentity.h](Client/Texts/GameIdentity.h)의 `GameName` 한 곳에서 관리합니다.
고정 UI 문구는 외부 `Languages/ko.ini`, `en.ini`를 수정하고 재실행하면 반영됩니다.
누락/잘못된 항목은 항상 영어 기본 문구를 사용합니다. [언어파일 경로·문법](Docs/TextManagement.md#언어파일-교체)

표시 구간 인덱스·FFT 타일·비동기 자원/곡 목록 준비와 엔진 캐시의 최신 변경은
[성능 수정 기록](Docs/Reviews/PerformanceFixes-2026-10-14.md)에 정리합니다.
텍스트의 유지 배치·글리프 업로드·아틀라스 회수와 긴 제목 스크롤은
[텍스트 개선 기록](Docs/Reviews/TextRenderingFixes-2026-10-14.md)에 정리합니다.

## 작업 시작

루트 `AGENTS.md` → [짧은 인수인계](Docs/SessionHandoff.md) →
[작업별 문서 표](Docs/README.md)의 해당 행만 읽습니다.
[변경 영향 지도](Docs/ChangeImpactMap.md)에서 소유 클래스와 테스트를 찾고,
관련 선언·구현·직접 호출부만 확인합니다. 전체 문서나 엔진 SDK는 필독이 아닙니다.
요구가 불명확하여 구현이 크게 달라지면 멈추고 질문합니다.

## 코드 구성

| 위치 | 책임 |
| --- | --- |
| `Client/` | 실행 파일, Scene 조립, 화면·오디오 연결; `Texts`는 문구·언어별 글꼴 소유 |
| `FingerDrum.Rhythm/` | 판정, 노트 규칙, Lane, 스크롤, 타이머 |
| `FingerDrum.Chart/` | YMM/YMP/YME 파싱, 타이밍 컴파일, 편집·저장 |
| `FingerDrum.Modes/` | PlaySession과 Taiko 노트 생성·사운드 정책 |
| `FingerDrum.Editor/` | 에디터용 오디오 디코딩·분석 |
| `FingerDrum.Assets/` | 기본 스킨/글꼴 RCDATA와 외부 기본 제공곡 |
| `Tests/Rhythm/` | 분야별 로직·파일·선택 상태 회귀 테스트 |
| `Dependencies/MRG-Engine/` | 독립 저장소의 재사용 엔진 |
| `../OsuTaikoConverter/` | 별도 C++ 저장소의 osu!taiko → YMM/YMP/YME 변환기 EXE |

Scene마다 전용 폴더와 `Submodules/`를 둡니다. Scene은 작은 호출로 책임 클래스를
조립하며, UI·세션·분석 상태는 각 클래스가 관리합니다. Note도 계약·규칙별 트리를
사용하며 Visual Studio 필터를 물리 폴더와 일치시킵니다.
플레이의 Loading·Gameplay·Pause·Ending은 공통 `IPlaySessionController` 계약과
`Client/GameFlow`의 상태머신을 사용합니다. [상태 전환·재개·종료](Docs/Gameplay/Pause.md#모든-모드의-실행-상태)
에디터는 공통 화면과 `IEditorMode`의 도구/노트 표시/입력 정책,
`IEditorDocument`의 파일 형식별 편집·저장을 분리합니다. 현재 구현은 Taiko와
YMP/YME이며 새 모드 연결 경계는 [에디터 구조](Docs/ChartEditor.md)에 있습니다.
Q로 박자 구간을 드래그하고 박자표·이펙트 탭에서 Ctrl/Shift 다중 선택을 사용합니다.
Ctrl+C, Ctrl+V(공백 제외), Ctrl+Shift+V(공백 포함)를 지원하며 편집 전체의 최근 32동작을
Ctrl+Z / Ctrl+Y로 되돌리고 다시 실행합니다. 덮어쓴 항목도 함께 복원됩니다.
곡 카드 +에서 새 난이도 생성 또는 [.osu 가져오기](Docs/EditorSongSelect.md#osutaiko-가져오기)를
선택할 수 있습니다. 가져오기는 기존 음원의 내용이 같으면 재사용하고 충돌 시 보존합니다.
osu!taiko 변환기의 새 출력은 첫 빨간선에 +25ms를 더한 Pattern Offset을 사용합니다.
모드는 제한된 편집 context를 통해 문서를 수정하고, 비동기 분석은 별도의 읽기 전용
오디오 계약을 사용합니다. Taiko 옵션·사운드 해석은 플레이와 에디터가 공유합니다.
편집 원본 노트·타이밍·이펙트는 균형 트리로 관리하며, 저장/플레이 snapshot과
렌더링 임시 목록은 vector를 사용합니다. 조회·분석·표시 캐시의 경계는 에디터 문서에 있습니다.
YMP의 `Effect file`이 참조하는 [YME](Docs/Formats/Yme.md)는 히트사운드 표와
Speed/Sounds/Zone 명령을 소유합니다. Whole/Separate, 세 기본 보간·사용자 식·3차 베지어,
히트사운드 볼륨, 싱코페이션 판정 보정과 Kiai 상태를 지원합니다.
플레이 화면은 기본 판정 범위의 색 구간·입력 타이밍 마커와
MAX/PERFECT/GREAT/GOOD/BAD/MISS/POOR와 롤 성공의 OK 링·문구를 표시합니다.
상단 진행바는 0ms부터 마지막 노트/롱테일 시각까지 진행하며 독립 PNG 네 요소와
현재/전체 시간 문구로 구성됩니다.
에디터는 선택 스킨과 W/E/R/T/Y/U 도구 묶음, 우측 노트 정보/속성 영역을 제공합니다.
노트의 시작·끝은 마디/박자로 수정하며 ms는 계산 결과만 표시합니다.
동작과 스킨 경로는 [플레이 표시](Docs/Gameplay/Presentation.md)에 있습니다.
모든 노트 처리 3초 뒤 [Result](Docs/Gameplay/Results.md)에 이동합니다. ESC는
[일시정지 메뉴](Docs/Gameplay/Pause.md)를 열어 Resume/Rewind/Retry/Retire를 선택합니다.
Pause에서는 음악 채널을 유지하고 재개 위치로 비동기 탐색한 뒤 타이머와 함께 재생합니다.
히트사운드 로딩 실패는 시작을 막으며 일시정지 메뉴의 Retry로 다시 불러올 수 있습니다.
메뉴는 Penpot의 네 버튼 배치를 사용하며 Resume/Rewind 값은 ←→ 또는 오른쪽 선택기의 화살표로 조절합니다.
Resume은 판정을 유지하고 0.5~3초 이전부터 재생하며 Rewind는 지정 마디 시작 이후 판정을 취소합니다.
Retire는 남은 노트를 0점·MISS로 확정합니다. 메뉴 사용 시 해당 시도의 클리어/HP 자격을 해제합니다.
일반·큰·롤 계열 및 최종 정확도는 소수점 6자리로 기록/표시하고 판정 개수,
오차 분산·시간별 산점도·히스토그램을 제공합니다. 모드별 결과/View를 교체할 수 있으며
정수 us 입력·일시정지/이동 로그를 메모리에 수집합니다. 기록 저장·리플레이 재생은 미구현입니다.
리절트는 Penpot Result · Sky의 배치·내장 PNG·Noto Sans KR 굵기를 사용하며,
상단 배너에 모드·곡명·아티스트·난이도를 표시하고 중앙 랭크/제작자와 오른쪽 통계를 배치합니다.
최종 정확도에 따라 Y~F의 11개 랭크 PNG를 표시하며 `skin.ini`에서 교체할 수 있습니다.
리트라이로 같은 패턴의 새 세션을 시작할 수 있습니다. 리플레이 버튼은 비활성입니다.

## 빌드·실행

Visual Studio 2022 / MSVC v143 / Windows SDK / FMOD Studio API for Windows가
필요합니다. FMOD SDK·DLL은 커밋하지 않습니다.

```powershell
git clone --recurse-submodules https://github.com/4dblackhole/MyRhythmGame-Client.git
cd MyRhythmGame-Client
git clone https://github.com/4dblackhole/OsuTaikoConverter.git ../OsuTaikoConverter
msbuild MyRhythmGame-Client.sln /m /p:Configuration=Debug /p:Platform=x64
msbuild MyRhythmGame-Client.sln /m /t:Rebuild /p:Configuration=Release /p:Platform=x64
```

전체 솔루션 빌드에는 형제 폴더의 비공개 변환기 저장소도 필요합니다.
`OsuTaikoConverter.exe`는 게임 EXE 옆에 복사됩니다. 인자와 격자/omit 규칙,
지원 범위는 [변환기 문서](Docs/OsuTaikoConverter.md)를 참고합니다.

도메인 테스트만 빌드할 때는 `Tests/Rhythm/FingerDrum.Rhythm.Tests.vcxproj`를 직접
빌드할 수 있으며 엔진 checkout/FMOD SDK가 필요하지 않습니다. Windows CI가
Debug/Release 로직·카탈로그를 검사합니다. 선택 실행(`--filter`)과 정적 분석 방법은
[검증 문서](Docs/Verification.md)를 참고하세요.

일반 실행은 로고 → 플레이/에디터 곡 선택으로 시작합니다.
두 곡 선택 화면에서 난이도 상자를 클릭하면 해당 플레이/편집으로 바로 진입합니다.
에디터 곡 카드의 +에서 메타데이터를 입력해 빈 YMP/YME 난이도를 생성하고 바로 편집합니다.
에디터 최상단의 자체 메뉴바에서 파일 → 저장하기를 선택할 수 있으며,
기존 하단 저장 버튼과 Ctrl+S도 같은 저장 동작을 사용합니다.
에디터의 오디오 탭은 음악 파형·음악 FFT·예상 히트사운드 FFT를 별도로 표시하며,
패턴/오디오 탭의 하단 타임라인을 드래그해 현재 편집 시각을 탐색합니다.
Ogg Vorbis는 내장 디코더로 분석하며 MP3/WAV 등은 기존 Media Foundation 경로를 사용합니다.
오디오 트랙을 드래그해 시작·끝·길이(ms)를 확인하고, 상단 슬라이더로 음높이도 함께 변하는
0.25~2.00배 미리듣기를 사용할 수 있습니다. 원본 음원과 분석 파형/FFT는 변경하지 않습니다.
오디오 탭에는 패턴 offset 입력/1ms 조절, 박자·예상 타격 눈금과 메트로놈 재생을 제공합니다.
실시간 뷰의 −/+로 시간축을 25~400% 확대·축소하며 Space로 음악/예상 히트사운드를
재생·일시정지합니다. Z는 첫 노트/0ms, X는 마지막 노트(테일 포함)/음원 끝을 번갈아 이동합니다.
Q는 노트 선택 도구이며, 우측 노트 정보의 종류 변경은 같은 도구 묶음 안에서만 허용합니다.
Debug/Release 모두 게임 Scene만 포함합니다. 배포할 때는 깨끗한 폴더에
`MyRhythmGame.exe`, `fmod.dll`, 제공할 `Asset/Songs`를 복사합니다.
개발 실행은 `Client/Assets`, 다른 폴더에 복사한 EXE는 그 옆 `Asset`을 사용합니다.
YMP/YME는 `Songs/Patterns/<곡>/<난이도>`에 한 쌍씩 정리합니다.
빌드·테스트·smoke 명령은 [검증](Docs/Verification.md), Scene 수명과 공통 관리자는
[실행 흐름](Docs/ExecutionFlow.md), 외부 파일 우선 및 내장 fallback/배포곡 정책은
[자산](Docs/BuiltInAssets.md)에서 설명합니다. 스킨의 `skin.ini`에서 씬 폴더와 이미지 상대 경로를
지정할 수 있습니다. 스킨은 선택한 자산 루트의
`Skins` 폴더를 옵션 패널에서 선택하며 다음 실행에도 유지됩니다.
한국어/영어와 옵션 패널은
[텍스트 관리](Docs/TextManagement.md)에서 관리합니다. 패널에서 FMOD, 일반(자동)/WASAPI/ASIO,
실제 출력 드라이버를 선택하며 출력 방식/장치 변경은 즉시 적용합니다.
열린 옵션 패널은 ESC로 닫으며 뒤 화면의 ESC 동작은 실행하지 않습니다.
옵션 버튼을 다시 눌러도 닫힙니다. Songs 폴더를 패널에서 선택하고 Option.ini에 저장하며,
변경 후 곡 목록을 갱신합니다. 채보 배경은 `Songs/BG`에 공유하고 YMP의 `BG:`와
`offset: x, y`로 경로와 엔진 좌표 오프셋을 지정합니다.
UI 색상은 `Client/Presentation/UiColors.h`에서 공유하고 옵션 배경은 기존 하늘색 중
더 짙은 색을 사용합니다.
해상도는 즉시 창 크기에 적용하며 모드별 기본·보조 키와 글로벌 오프셋(ms)을 설정할 수 있습니다.
모든 옵션은 EXE 옆 `Option.ini`에 저장하고 다음 실행에 복원합니다.

음악 재생과 에디터 파형/스펙트로그램은 엔진의 공통 PCM 시간축을 사용합니다. MP3 패딩과 전체 PCM의 메모리 비용은 [플레이 오디오](Docs/Gameplay/Audio.md)를 참고합니다.
