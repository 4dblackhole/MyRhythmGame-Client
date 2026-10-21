# 새 관리자·세션 인수인계

## 시작

1. 루트 `AGENTS.md`를 읽고 Client/엔진 status 및 submodule gitlink를 확인합니다.
2. [작업별 문서 표](README.md)에서 필요한 기능만 선택합니다. 전체 문서/채팅은 읽지 않습니다.
3. [변경 영향 지도](ChangeImpactMap.md)로 소유 파일·호출부·테스트를 찾습니다.
4. 요구가 불명확하거나 해석에 따라 동작/설계가 크게 달라지면 구현을 멈추고 질문합니다.

## 작업 위치와 경계

2026-10-22 통합 리뷰 수정: [책임과 회귀 검사](Reviews/CodeReviewFixes-2026-10-22.md).
Lane은 입력 시각 전에 만료된 노트를 먼저 처리하고 PlaySession은 물리 키 유지 상태와
rewind 기록을 소유합니다. ChartEditor는 여섯 저장/복구 경로의 별칭을 변경 전에 거부합니다.
엔진은 Raw Input 전체 순서와 GPU fence 이후 SRV 블록 회수를, 변환기는 생략된 볼륨
기본값·스피너 횟수·Songs 루트의 중복 import 즉시 거부를 담당합니다. 사용자 곡/스킨과
기존 LaneKeyBeam.cpp 변경은 보존합니다. 검증·커밋은 Verification의 같은 날짜 절을 봅니다.

변환기 오프셋(2026-10-20): [변환기 계약](OsuTaikoConverter.md). 새 변환은 원본 격자/마디
계산 후 Pattern Offset과 출력 절대 검증 시각에 +25ms를 한 번 적용합니다. 원본 채보/음원과
기존 변환 파일은 바꾸지 않으며, 패키지/라이브러리 배치가 같은 정책을 사용합니다.

공통 PCM(2026-10-20): [플레이 오디오](Gameplay/Audio.md), [에디터 분석](ChartEditor.md).
재생/분석이 엔진 DecodeAudioPcm의 같은 시간축을 사용합니다. MP3는 dr_mp3로 Info/LAME
패딩을 제거하고 Vorbis/Windows 코덱은 기존 경로를 공유합니다. FingerDrum.Editor는
호스트가 주입한 PCM reader로 FFT만 수행하며 엔진 의존성이 없습니다. FMOD 출력/버스/DSP는
유지합니다. Stream도 전체 float PCM을 준비하므로 메모리 비용과 취소/수명 계약은 엔진 문서를
참고합니다. 개발용 Omoiyoshino YMP의 Pattern Offset은 사용자 요청으로 752ms입니다.


Songs/BG·편집 이력(2026-10-19~20): [경로/배경](BuiltInAssets.md), [에디터 조작](ChartEditor.md).
옵션의 Songs 폴더는 Option.ini의 General/SongsPath에 저장합니다. 빈 값은 기존 개발/배포
자동 경로, 상대경로는 EXE 폴더 기준입니다. 곡 선택은 변경 즉시 최신 루트로 갱신하며,
진입 Snapshot이 Songs 루트와 BG 경로/offset을 소유합니다. YMP의 BG는 Songs/BG 기준,
`offset: x, y`는 엔진 2D 좌표(+Y 위쪽)입니다. 플레이 Presenter는 원본 비율을 유지해 화면을
덮고 누락/실패는 기본 스킨 배경과 오류 메시지로 처리합니다. 기본 배포에는 빈 BG 폴더를 제공합니다.
변환기는 별도 세션/저장소에서 BG 복사·바이트 재사용·충돌 보존을 구현하며 osu! 오프셋을
`(1.5*x, -1.5*y)`로 변환합니다. [변환기 계약](OsuTaikoConverter.md)을 함께 참고합니다.

Workspace의 EditorSelection/EditorClipboard는 노트 드래그, 박자표/이펙트 행 Ctrl/Shift 선택,
공백 포함/제외 붙여넣기를 소유합니다. 붙여넣기는 현재 재생 위치의 격자에 첫 위치를 맞추며,
MusicalTimeline의 정확한 유리수 덧셈/뺄셈으로 BPM/delay와 무관한 박자 간격을 보존합니다.
ChartEditorHistory는 편집 원본의 최대 32개 동작을 기록합니다. 덮어쓴 노트와 롱노트 끝점도
같은 한 동작으로 복원하며 저장한 상태로 돌아가면 dirty가 해제됩니다. 노트 단독 이력은
추가/수정/삭제와 같은 트리 갱신 경로를 사용합니다. Space는 재생/정지, 실시간 휠은 격자 탐색입니다.
첫 저장의 자동 YME 참조는 저장 성공 시에만 이력에도 확정하므로 메타데이터 Undo/Redo와
저장된 파일이 일치합니다. Release의 기존 FingerDrum.Editor PDB LNK4020 경고는 별도 제한으로
Verification에 기록했습니다. 소스 진단 억제나 범위 밖 빌드 설정 변경은 하지 않았습니다.

판정 인디케이터는 최근 32개 타격을 고정 원형 큐로 유지하고 밀려난 최대 8개를 0.2초 동안
페이드합니다. 자동 MISS/빈 입력에는 가짜 시각을 만들지 않습니다. 키 레이아웃 flash에도
기존 동/캇 빨강/파랑 색을 적용합니다. 엔진 API·gitlink와 사용자 LaneKeyBeam.cpp는 변경하지 않습니다.
검증 결과와 수동 확인 범위는 Verification의 같은 날짜 절을 참고합니다.

에디터 오디오 도구(2026-10-17): [선택 시간·배속](ChartEditor.md).
세 트랙의 드래그 선택은 Workspace의 절대 us 영역이며 View는 ms 시작/끝/길이를 표시합니다.
0.25~2.00배 미리듣기는 원본/FFT를 바꾸지 않고 Voice 주파수와 DSP↔원본 시각 환산을 함께
조절합니다. 음높이도 함께 변하며 음악 seek 없이 살아 있는 채널을 유지합니다. 엔진 변경은 없습니다.

에디터 OGG 분석(2026-10-17): [오디오 분석 경로](ChartEditor.md).
게임 재생은 FMOD를 유지하며 분석에만 정적 stb_vorbis를 추가했습니다. Ogg Vorbis는
Windows 코덱 없이 파형/FFT를 생성합니다. 다른 형식은 기존 Media Foundation을 사용합니다.
디코더/PCM 전달은 FingerDrum.Editor/Audio/Submodules, FFT는 EditorAudioAnalysis가 소유합니다.
출처·한계·외부 경고 정책은 ThirdParty/stb/README.md, 검증은 Verification을 참고합니다.

에디터 osu 가져오기(2026-10-17): [곡 카드 + 메뉴·가져오기](EditorSongSelect.md#osutaiko-가져오기).
+는 New Difficulty/Convert .osu 팝업을 열며 전자는 기존 새 난이도 입력/진입을 유지합니다.
EditorOsuImport가 파일 선택과 비동기 변환 EXE/결과 수명을 소유하고 Scene은 카탈로그를 갱신합니다.
변환기의 --layout library는 로마자 Artist - Title의 Music/Patterns 배치를 만들고 등록된
음원이 바이트까지 같으면 기존 YMM/음원을 재사용합니다. 충돌은 덮어쓰지 않습니다.

FMOD 수정(2026-10-17): [수정 책임과 검증](Reviews/FmodErrorFixes-2026-10-17.md).
플레이/에디터 Pause는 음악 채널을 유지하고 재개 시 위치를 재정렬합니다. 엔진 PreparationState가
내부/목표 탐색을 직렬화하며, Client는 Ready 이후 새 clock을 예약합니다. 종료 샘플 채널 교체,
로딩 실패 차단/Retry 재로드, PCM 끝 경계, 종료 중 retired Sound 해제 순서도 수정했습니다.
실제 장치 청취/분리는 미검증입니다. 선행 재현 조건은 [조사](Reviews/FmodErrorAudit-2026-10-17.md)에 있습니다.

플레이 상태머신(2026-10-17): [공통 상태·재개 계약](Gameplay/Pause.md#모든-모드의-실행-상태).
IPlaySessionController와 공통 Controller/Feedback는 Client/GameFlow, 실행 구현은 그 Submodules로 이동했습니다.
Scene은 인터페이스로 조립하며 Loading/Gameplay/Pause/Ending을 모든 모드에서 공유합니다.
음악은 유지한 paused Voice 탐색 완료 후 QPC/DSP에 같은 20ms 여유를 예약합니다. 재개 대기는 Pause이며
히트사운드를 되살리지 않습니다. Retry는 자산을 재사용하고 Ending은 모드별 결과를 한 번만 이동합니다.

새 난이도·offset 도구·JudgeLevel(2026-10-16): [EditorSongSelect](EditorSongSelect.md), [ChartEditor](ChartEditor.md).
에디터 곡 카드 + → New Difficulty → 메타데이터 입력 → 새 YMP/YME 검증/생성 → 기존 진입 snapshot을 사용합니다.
NewDifficultyDialog는 입력/오류, EditorPatternCreation은 새 폴더·파일 생성/정리를 소유합니다.
JudgeLevel은 1.00~100.00/0.01 단위의 공통 고정소수점 값이며 YMP·AccuracyRange·HUD·결과까지 전달합니다.
기본 Taiko/120 BPM/offset0/4/4/50.00이며 메타데이터 탭에서도 JudgeLevel을 수정합니다.
오디오 탭에 offset 직접 입력/±1ms·박자/타격 눈금·메트로놈 토글을 추가했습니다.
EditorMetronome은 박자/재생 cue만 캐시하고 기존 EditorPlayback DSP 시계를 사용합니다. FFT 입력에는 넣지 않습니다.
사용자 LaneKeyBeam.cpp와 개발 Option.ini/외부 언어파일은 보존합니다.

배포 기본본(같은 날짜): 사용자의 명시적 지시로 바탕화면 demo를 삭제/재생성했습니다.
곡 없이 Asset/Songs/Music·Patterns, Asset/Skins, 원본 언어파일, Release EXE/fmod.dll,
기본 Option.ini와 서명 확인한 Microsoft VC++ 설치 파일을 배치했습니다. [배포 절차](BuiltInAssets.md#release-배포-절차).
공용 BG 이미지의 참조/표시는 위 2026-10-19 변경에 포함됩니다. BGA와 라이선스/랭크 무결성은
[설계 제안](RankedContentProposal.md)만 작성했으며 아직 구현하지 않았습니다.

skin.ini(2026-10-16): [BuiltInAssets의 경로·키·fallback](BuiltInAssets.md#skinini와-씬별-이미지).
`[Folders]`에서 Logo/InGame/MusicSelect/Result 폴더, 각 씬 섹션에서 파일을 지정합니다.
SkinManifest는 UTF-8 경로/기본본을 해석하고 SkinSetSelection은 선택 revision당 캐시합니다.
버튼/패널의 SkinVisualComponent는 노드와 함께 소멸하므로 가상 카드 참조를 남기지 않습니다.
기본 로고는 TitleImage/Logo에서 Logo로 이동했습니다. 에디터도 InGame 이미지 키를 공유합니다.

UI 언어파일(2026-10-16): [TextManagement의 경로·문법·소유권](TextManagement.md#언어파일-교체).
모든 고정 UI 문구를 UTF-8 INI에서 읽고 선택 언어 → 외부 en → 내장 영어 순서로 대체합니다.
원본은 Client/Texts/Languages, 실제 편집본은 자산 루트의 Languages이며 재실행 때 한 번 읽습니다.
빌드는 기존 편집본을 덮어쓰지 않고 영어 기본본을 생성 헤더로 내장합니다.
화면/에디터 모드는 같은 TextCatalog를 참조하고 읽은 문자열은 수명 내내 불변입니다.

게임 표시 이름(2026-10-16): `Client/Texts/GameIdentity.h`의 `GameName` 한 곳에서 변경합니다.
게임 창·시작/자산 오류창과 에디터 확인창이 참조하며 언어 표에는 에디터 접미사만 둡니다.
기존 FingerDrum 클래스/Scene ID·캐시와 MyRhythmGame.exe는 내부 식별자로 유지합니다.
범위와 변경 방법은 [TextManagement](TextManagement.md#게임-이름-변경)를 참고합니다.

옵션 패널 선행 변경(2026-10-15): [TextManagement](TextManagement.md).
공통 UI 색상을 UiColors.h로 모으고 기존 하늘색을 재사용하여 옵션 배경/필드 대비를 높였습니다.
ESC는 마우스 위치와 무관하게 목록/드래그를 정리하고 패널을 닫으며 뒤 화면에 전달하지 않습니다.
해상도는 실제 모니터의 지원 목록에서 즉시 창 크기를 변경하고 Option.ini에 저장합니다.
모드의 InputLayout으로 Taiko 기본·보조 8키를 표시/변경하며 입력 판정·held·키 표시·로그가
해결된 키 목록을 공유합니다. DLL 로더 자체는 미구현입니다. 글로벌 오프셋은 정수 ms이며
양수는 음악 타이머 0ms를 유지하면서 노트·틱·BPM·마디선·YME를 늦춥니다.
선택 시 실행용 Snapshot에 넣고 에디터에도 같은 timeline을 사용합니다. 채보 원본/저장 박자는
보존하며 Copy/Replace/타이밍 수정에도 오프셋을 유지합니다. 자세한 설정/조작은 TextManagement,
엔진 서비스는 엔진 Docs/WindowControl.md, 검증 결과는 Verification을 참고합니다.

플레이 일시정지(2026-10-15): [Resume/Rewind/Retry/Retire와 향후 HP·리플레이 계약](Gameplay/Pause.md).
ESC는 메뉴를 열고 Resume은 처리 상태 유지, Rewind는 지정 마디 시작 이후의 판정만 취소합니다.
Retire는 미완료 롱을 포함한 남은 노트를 0점·MISS로 확정합니다. 메뉴 사용 시 클리어/HP 자격은
해당 시도에서 복구되지 않으며 Retry가 새 시도로 초기화합니다. 음악 소스 seek·음수 대기 예약과
정수 us 제어 마커를 연결했습니다. HP/리플레이 자체는 미구현이며 위 문서의 계약을 따라야 합니다.

리절트 Penpot 구현·배치 갱신(2026-10-14~15): [플레이 결과](Gameplay/Results.md).
엔진 TextStyle/Visual2D fontWeight를 먼저 반영하고 Noto Sans KR의 네 굵기로 시안의
3열 배치·장식·실제 통계를 구현했습니다. Retry는 같은 선택 경로의 새 세션,
Replay는 비활성이며 SCORE/HP는 미구현 상태를 명시합니다.
2026-10-17에 정확도 랭크 PNG 11개를 추가했습니다. 100% Y, 99% PERFECT부터
70% E, 그 미만 F까지 고정소수점 정수로 비교하며 [랭크 표와 이미지](Gameplay/Results.md#정확도-랭크)가 기준입니다.
갱신한 시안의 상단 배너에 모드·곡명·아티스트·난이도를 표시하고, 중앙 랭크/제작자와
오른쪽 528px 그래프 열을 맞췄습니다. 아티스트도 선택 Snapshot으로 전달하며 파일을 다시 읽지 않습니다.
이번 배치 변경은 기존 엔진 기능을 사용하고 내장 드럼 PNG만 Penpot에서 다시 내보냈습니다.
Scene별 PNG는 내장 Default Skin/Result 아래에 있고 사용자 스킨의 파일별 fallback을 사용합니다.

최근 성능 수정(2026-10-14): [8개 병목 수정 기록](Reviews/PerformanceFixes-2026-10-14.md).
객체별 표시 구간 조회/Render 갱신, 효과 시각 인덱스, 편집 롱 구간 인덱스, FFT 타일,
비동기 카탈로그/가시 카드, 비동기 플레이 자원, 엔진 텍스트 LRU/업로드 fence/2ms 오디오
유지관리를 적용했습니다. 상세 검증과 수동 확인 구분은 [Verification](Verification.md)에 있습니다.

텍스트 후속 개선(2026-10-14): [유지 배치·아틀라스·긴 제목 수정 기록](Reviews/TextRenderingFixes-2026-10-14.md).
TextContent payload와 준비된 글리프 배치를 재사용하며 clip 밖 글리프를 제외합니다.
회색조 출력·프레임 upload arena·16MiB 기본 아틀라스 확대/회수·합성 순서 보존을 적용했습니다.
Marquee는 원본 Unicode를 유지하고 측정 폭으로 픽셀 이동합니다.

- 작업 폴더: `D:\Projects\c++\asdf\MyRhythmGame-Client`. 별도 지시 없으면 `main`에서 작업합니다.
- Client: `4dblackhole/MyRhythmGame-Client`, 엔진: private `Dependencies/MRG-Engine` submodule.
- 전체 솔루션 빌드는 같은 부모 폴더의 독립 저장소 `OsuTaikoConverter`도 필요합니다.
  osu!taiko 전용 EXE의 인자·변환 규칙·제한은 [변환기](OsuTaikoConverter.md)를 참고합니다.
- 커밋과 동기화 상태는 Git이 기준입니다. 문서에 고정 SHA를 복제하지 않습니다.
- C++20 / MSVC v143 / Windows / D3D12. Client의 엔진 경계는 `MRG_Core.h` 하나입니다.
- 판정/차트/모드/편집 분석은 엔진 비종속 프로젝트입니다. Scene은 수명주기와 조립을 맡습니다.
- 기본 스킨/글꼴은 RCDATA fallback, AngelDream MP3 1개·YMM 1개·YMP/YME 5쌍과
  효과 테스트 WAV 2개는 외부 Songs입니다.
- 사용자 곡·스킨과 `TODOLIST.txt`를 임의로 stage/덮어쓰지 않습니다.

## 최근 구현: 에디터 시간축 줌·재생·양끝 이동 (2026-10-12)

실시간 레인 크기를 유지하고 −/+로 시간축을 25~400% 조절합니다.
EditorPlayback이 DSP 시각/음악 seek/예상 히트사운드의 Clip·Voice를 소유하며,
Space 재생/일시정지와 Z 첫 노트→0ms 이동을 Workspace/Input에 연결했습니다.
X는 마지막 노트(테일 포함)→음원 끝을 번갈아 이동합니다. 분석이 준비되지 않았으면
현재 시간과 재시도 순서를 보존하고 상태 메시지를 표시합니다. Z/X 변경·수동 Seek·Space는
이동 순서를 초기화합니다. 음악 길이는 현재 파일에 해당하는 분석 캐시만 사용합니다.
상세 책임·범위는 [ChartEditor](ChartEditor.md), 검증은 [Verification](Verification.md)에서 확인합니다.

## 최근 구현: 마디선 스크롤 통일·횟수형 롤 헤드 판정 제거 (2026-10-12)

마디선의 Separate 배율을 표시 시작 위치에서 컴파일하고 노트와 같은
GameplayPresenter::LaneObjectPosition으로 Whole 거리와 함께 배치합니다.
음수 delay의 살아남은 마디 시작과 느린 마디선의 표시 시간 범위도 반영합니다.
Roll/BigRoll/Balloon/DengDeng의 Preview는 Unjudged이며 구간·타격수로만 점수를
계산합니다. 동/캇 제한과 OK/MISS 완료 표시는 유지하고 TickRoll/Buzz는 시작 판정을 유지합니다.
계약은 [플레이 표시](Gameplay/Presentation.md)와 [판정 규칙](Gameplay/Rules.md),
검증은 [Verification](Verification.md)를 참고합니다.

## 최근 변경: 뎅뎅의 동·캇 자유 시작 (2026-10-12)

TimedSequenceInputRule에 첫 유효 입력으로 순서의 시작을 선택하는 동작을 추가하고
Taiko 뎅뎅에만 연결했습니다. 이후는 마지막 인정 입력의 반대 색만 인정합니다.
순서 오류는 Unjudged이며 카운트/다음 색/점수를 유지합니다. Reset은 초기 상태로 되돌립니다.
TaikoSoundPolicy는 횟수 홀짝 대신 실제 인정된 action을 사용하므로 캇 시작도 올바른 소리를 냅니다.
풍선의 동 전용·시간 범위·완료/부분 점수는 유지합니다. 상세는 [판정 규칙](Gameplay/Rules.md),
검증은 [Verification](Verification.md)를 참고합니다.

## 최근 구현: 상단 진행바 (2026-10-12)

`GameplayProgressView`는 세션 진입 시 컴파일된 마지막 head/tail 시각을 캐시하고
Presenter가 전달하는 같은 RhythmTimer 시각으로 채움/마커/현재 시간을 갱신합니다.
0ms→마지막 노트가 0→100%이며 판정 유예·음원 길이·Result 대기 시간은 제외합니다.
일시정지/Reset/탐색은 기존 타이머를 따릅니다. 빈 채보는 0%, 0 이하 종료 시각은
해당 마지막 시각에 100%로 전환합니다. 시간 문구는 0:00 미만을 표시하지 않습니다.
단일 GameProgressBar.png 대신 내장/선택 스킨 `InGame/Progress`의 Background/Track/Fill/Marker
PNG를 사용하고 현재/전체 m:ss를 런타임에 표시합니다. 노드 수명/스킨/크기 규칙은
[플레이 표시](Gameplay/Presentation.md), 생성 프롬프트는 [자산 기록](Gameplay/ProgressAssets.md)에 있습니다.
새 PNG는 기존 스킨 wildcard로 팩에 포함됩니다. 사용자 외부 스킨/설정/곡은 수정하지 않습니다.
검증 명령은 Verification의 진행바 항목과 Tests/Presentation의 두 회귀를 참고합니다.

## 최근 구현: Result·입력 로그·정확도 정밀도 (2026-10-12)

노트 처리와 Result의 정확도는 퍼센트 기준 소수점 6자리 아래를 버립니다.
AccuracyScore 정수 단위(100%=100,000,000)로 합산하며 기존 판정 창/등급은 유지합니다.
PlaySession은 모드별 IPlayResultCollector를 소유하고 Taiko는 실제 생성된 논리 노트의
판정·오차·최종 점수와 매핑된 게임 키 Pressed/Released를 정수 us로 수집합니다.
모든 노트 처리 3초 뒤 또는 ESC 입력으로 Result에 이동하며, ESC는 그 QPC 시각에
판정/held 상태를 고정합니다. 중도 종료의 미처리와 진행 중 롱노트는 0점/별도 개수이며
실제 MISS와 구분합니다. R/뒤로 탐색 Reset은 집계와 입력 로그를 초기화합니다.

RhythmTestScene → 루트 대기 슬롯 → Result factory로 unique_ptr를 이동합니다.
ResultScene은 결과와 IResultView를 소유하고 TaikoResultView가 일반/큰/롤 정확도,
MAX~BAD/OK/MISS/POOR 개수, 오차 통계와 두 그래프를 표시합니다. 두 Scene 모두
DestroyOnExit이며 결과는 플레이 세션/노트/오디오 포인터를 보관하지 않습니다.
Result의 버튼·ESC·Enter는 곡 선택으로 돌아갑니다. 모드 확장, 카운트 정의,
입력 로그 범위와 점수/그래프 계약은 [플레이 결과](Gameplay/Results.md)가 기준입니다.
디스크 저장·리플레이 재생·곡 선택 기록은 이번 범위에 포함하지 않았습니다.
검증은 Verification 및 Tests/Presentation의 Result 회귀 명령을 참고합니다.

## 최근 구현: 개발/배포 자산 경로·난이도별 파일 쌍 (2026-10-12)

외부 자산 경로는 FingerDrum.Assets의 UserAssetRoot에서 한 번 결정합니다.
EXE 기준 `../../../Client/Assets`가 있으면 개발용 폴더, 없으면 EXE 옆 `Asset`을
사용합니다. 작업 폴더와 Debug/Release 구성에 의존하지 않습니다.
Songs와 Skins는 이 루트를 공유하고 Option.ini는 ExecutableDirectory를 사용합니다.
내장 기본 스킨/글꼴은 유지합니다. 빌드는 기본곡을 개발용 Songs에 없는 경우에만 복사합니다.

AngelDream 5개 난이도는 `Songs/Patterns/angeldream/<Pattern Name>`에 YMP/YME를
같은 이름으로 두며 효과용 Sounds도 해당 난이도가 소유합니다. 효과가 없는 패턴은
빈 YME를 참조합니다. 현재 개발용 사용자 채보도 난이도별 폴더로 정리했습니다.
같은 Stream 이름의 두 패턴은 기존 파일명의 1.5x로 폴더만 구분합니다.
음원/노트 데이터는 보존했고 이동 전 사용자 YMP는 ignored build의
`asset-layout-2026-10-12/OriginalUserPatterns`에도 보관했습니다.
Omoiyoshino는 개발용 `Music/Omoiyoshino` 및 `Patterns/Omoiyoshino/Inner Oni`에
복사했으며 Git/내장 자산에 넣지 않았습니다. 이전 bin의 외부 곡은 삭제하지 않았습니다.

기존 사용자 채보 5개에는 지원되지 않는 #measurelinevisible 명령이 남아 있습니다.
개발 Songs 전체 검사는 이 기존 오류로 실패하지만 정상 패턴은 카탈로그에 유지됩니다.
이 요청에서 구형 명령의 의미나 파일 형식을 임의로 바꾸지는 않았습니다.
변환기의 향후 출력 배치까지 바꿀지는 사용자 확인 대기이며 변환기 코드는 변경하지 않았습니다.
경로/배포/파일 쌍 계약은 BuiltInAssets, 실제 검증 결과는 Verification이 기준입니다.

## 최근 수정: 변환기 안내의 Client 문서 참조 (2026-10-12)

변환기의 판정/틱롤/뎅뎅 안내와 README는 Client `main`의
`Docs/Gameplay/Rules.md`를 최신 설명의 기준으로 링크합니다. Converter.cpp의
ClientRulesNotice가 링크를 한 곳에서 붙이며 세부 플레이 규칙과 기본 판정 레벨 수치를
따로 복제하지 않습니다. 객체 대응과 OD 기반 HitCount는 변환기 설명으로 유지합니다.
변환 데이터/JSON 구조/플레이 규칙은 변경하지 않았습니다. Debug/Release 전체 빌드,
변환기 3개 프로젝트 MSVC 분석(경고/오류 0), Core/CLI/실제 카탈로그,
Client 49개 로직/카탈로그와 각 4개 smoke, 구조/차이 검사를 통과했습니다.
Omoiyoshino 재변환에서 새 안내를 확인했고 기존 YMM/YMP/YME/음원의 해시가 동일했습니다.
최신 변환기 EXE도 두 구성의 게임 EXE 옆에 반영했습니다. 상세는 Verification을 참고합니다.

## 이전 검증: Omoiyoshino 변환·실제 재생 (2026-10-12)

osu! 설치 폴더 `D:/Rhythm Games/osu!/Songs`의 Omoiyoshino / Inner Oni /
Konpaku Sariel(v14, Mode 1)을 변환기 Debug/Release EXE로 변환했습니다.
861개 원본 객체가 864개 YMP 행, 861개 플레이 노트로 보존되며 최대 스냅 이동은
약 1.143ms입니다. YMM/YMP/YME/음원은 변환기 ignored
`TestData/Omoiyoshino-InnerOni-20261012`에 있고, 두 구성 실행 폴더의
`assets/Songs/Imported/Omoiyoshino - Inner Oni (Konpaku Sariel)`에도 복사했습니다.
원본과 사용자 설정은 보존했고 게임 기본곡이나 Git에 추가하지 않았습니다.
변환기 Core/카탈로그 및 CLI 회귀는 두 구성 성공했고, Release 실제 D3D12/FMOD
자동 재생으로 861개 노트 완료/100%/8,130프레임을 확인했습니다. 두 구성의 네 smoke도
성공했습니다. 수동 화면·물리 입력·청음은 확인하지 않았습니다.
이 검증 당시 변환기 JSON의 `exclude the tail` 안내는 구형 문구가 남은 상태였습니다. 실제 플레이는
닫힌 틱 구간을 사용합니다. 이번에는 제품 C++ 코드를 변경하지 않고 검증 결과만 기록했습니다.

## 최근 구현: 테일 틱의 닫힌 구간 (2026-10-12)

TickRoll/BigTickRoll/Buzz의 헤드 기준 틱 격자에 테일 위치가 정확히 맞으면 마지막 틱도
생성합니다. MusicalTimeline의 유리수 위치로 판단하므로 us 반올림만으로 일치한 테일은
추가하지 않습니다. 음수 delay의 무효 구간은 테일에도 적용하고, Buzz 헤드는 별도 시작
판정으로 유지합니다. 플레이의 틱 판정/정확도/처리 후 숨김과 에디터 오디오 예측은
같은 컴파일 결과를 사용합니다. 종료 시점과 자유 연타의 자동 요구 횟수는 유지합니다.
Debug/Release 전체 Rebuild·49개 로직/카탈로그·각 4개 smoke·에디터/판정 표시 회귀·
실제 D3D12/FMOD 자동 replay를 통과했습니다. Client 소유 4개 프로젝트 및 표시 테스트의
MSVC 분석에서 새 경고 0이며 기존 경고는 Verification에 구분했습니다.
사용자 LaneKeyBeam.cpp와 Option.ini를 보존했고 엔진 변경은 없습니다.
수동 화면/물리 입력/청음은 확인하지 않았습니다. 상세 계약은
[판정 규칙](Gameplay/Rules.md), 결과는 [Verification](Verification.md)를 참고합니다.

## 이전 완료: delay 의미 변경 (2026-10-12)

선택 스킨 서클, 6개 도구 묶음(W/E/R/T/Y/U)과 순환, 롤 종류/연타·틱 간격,
우측 노트 정보/속성, 처리된 틱 숨김과 OK/MISS 표시 및 JudgeImage 제거가 있습니다.
OK는 모든 요구 연타/틱을 채운 성공 완료에만 표시하는 것으로 사용자가 확정했습니다.
노트 타이밍은 사용자 확정에 따라 마디·박자만 입력하고 ms는 읽기 전용입니다.
시작/끝 입력과 일반 노트↔롱노트 변환을 연결했습니다.
사용자는 delay를 마디 중간에도 허용하고, 경계는 앞이 열리고 끝이 닫힌 구간으로
확정했습니다. 파일의 마디/박자는 보존하며 노트·BPM·효과의 실제 us 시각에만 delay를
적용하고 YME Area 보간은 실제 경과 시간으로 계산합니다.
후속 확정: 음수 delay가 덮는 일반 노트·틱·BPM·delay·YME 지점은 컴파일 결과에서
제외하고 원본은 보존합니다. 롱노트와 Area는 양 끝의 기존 시각을 유지하고, 끝이
시작보다 앞설 때만 제외합니다. 같은 시각은 유지합니다. Area는 롱노트와 같은 규칙으로
처리하라는 최종 지시가 이전의 “끝점이 무효면 전체 제외” 선택을 대체합니다.
공유 timeline의 표시 마디 범위/무효 구간과 Taiko의 플레이/예측/에디터 선별을 구현했습니다.
주신 2/4→3/4→4/4 예시, BPM/delay 복원, 모든 Taiko 롱 타입과 Area 끝점 정책,
원본 저장/ID 보존과 에디터 스냅을 회귀 검사합니다. Debug/Release 전체 재빌드·48개 로직/카탈로그·4개 smoke·표시 회귀·실제 자동 replay와
Client 소유 4개 프로젝트의 정적 분석을 통과했습니다. 새 경고 0이며 기존 경고는
Verification에 구분했습니다. 실제 화면/물리 입력/청음은 수동 확인하지 않았습니다.
소유 경계/현재 조작은 [ChartEditor](ChartEditor.md), 검증 상태는 [Verification](Verification.md)를 확인합니다.

## 현재 범위와 주의점

- 기본 진입은 로고 → 플레이 또는 에디터 곡 선택 → 플레이/에디터입니다.
- YMP/YME 편집·저장, BPM/마디/히트사운드 변경과 오디오 분석을 지원합니다.
- 에디터 전용 Canvas 상단 메뉴는 파일 → 저장하기만 제공합니다. `EditorMenuBar`는
  메뉴 상태/저장 의도를 맡으며 하단 저장·Ctrl+S와 함께 기존 Workspace Save를 사용합니다.
  메뉴 입력 차단/언어/해상도 회귀는 Presentation의 EditorMenuTests에 있습니다.
- YMP의 Effect file은 YMP 기준이고 히트사운드 표/파일 경로는 YME 소유/기준입니다.
  YME는 Interpolation/HitSounds 기본 데이터와 Speed/Sounds/Zone 명령을 분리합니다.
  볼륨은 히트사운드 대상입니다. 문법·보간·Whole/Separate·싱코페이션/Kiai는
  [YME](Formats/Yme.md), 실제 AngelDream 효과 테스트는 Presentation의 -Effects를 참고합니다.
- 영구 기록·리플레이 재생, 에디터 Undo/Redo·드래그·다중 선택은 미구현입니다.
  이 목록은 추가 구현 지시가 아닙니다. 기능별 제한은 해당 문서가 기준입니다.
- Scene 전용 폴더와 `Submodules`, Note 계약/규칙 트리를 사용합니다.
  파일 추가 시 프로젝트와 Visual Studio 필터도 같은 트리로 등록합니다.
- 에디터는 `IEditorMode`와 `IEditorDocument`로 도구/표시/입력과 파일 편집/저장을
  교체합니다. 현재 구현은 Taiko + YMP/YME이며 BMS/7키는 아직 미구현입니다.
  확장 경계는 [ChartEditor](ChartEditor.md)의 모드/파일 형식 절에서 확인합니다.
- 모드는 Workspace 전체 대신 `IEditorContext`를 참조합니다. 문서 수정/시간 이동은
  Workspace 명령으로 연결하고, 분석은 `IEditorAudioSource` 조회만 사용합니다.
  Taiko 옵션/사운드 예측은 플레이의 공통 규칙을 사용합니다.
- 편집 원본 노트/BPM/마디/이펙트는 위치 기반 균형 트리로 관리합니다. 고유 노트 ID는
  배열 인덱스가 아니며 저장/플레이/렌더링 조회는 읽기 전용 snapshot입니다.
  BPM/이펙트 수정은 `ReplaceTiming`/`ReplaceEffects`로 필요한 캐시만 갱신합니다.
  마디 상한은 사용자 선택에 따라 100,000이며 Taiko 새 배치에서 롱노트 겹침을 거부합니다.
  분석 취소는 Scene에서 기다리지 않고, 미리듣기는 StreamAsync의 준비 상태를 확인합니다.
- AngelDream `All Notes Verification`은 전 노트 및 밀집 Don/Kat Buzz를 재현합니다.
  채보 위치와 +50ms GOOD 입력의 틱 기대값은 [Gameplay Debugging](Gameplay/Debugging.md)을 참고합니다.
- 고정 문구와 언어별 글꼴은 `Client/Texts`의 사용 위치별 표가 소유합니다.
  플레이의 판정 구간/타이밍 마커와 8종 링·문구는 `GameplayJudgementView`가 소유하며,
  기본 범위를 세션 중 유지합니다. 동작/스킨 경로는 [Presentation](Gameplay/Presentation.md) 참고.
  로고와 곡 선택은 공통 `OptionsPanel`에서 언어·스킨·FMOD·자동/WASAPI/ASIO·드라이버·해상도·모드별 키·글로벌 오프셋을 선택합니다.
  출력 방식/드라이버는 즉시 적용하고 모든 옵션은 EXE 옆 `Option.ini`에 유지합니다.
  `OptionSettings`는 파일 저장/읽기만 담당하며 별도 옵션 Controller는 없습니다.
  잘못되거나 생성할 수 없는 INI는 원본을 보존하고 기본 설정으로 실행하며 패널에 경고합니다.
- 선택한 스킨 이름은 `Option.ini`에 유지하며,
  각 파일이 없을 때 내장 기본 스킨으로 대체합니다. 풍선 완료음 `pop.wav`도
  `Default Skin/HitSounds/TaikoMode`에 포함됩니다.
- Debug/Release 모두 게임 Scene만 빌드합니다. 별도 엔진 검증용 실행 프로젝트는 미구현입니다.
  구형 설정 가져오기·빈 모드의 Taiko 대체·YMP 효과 명령 허용·호환 헤더/별칭은 제거했습니다.
  `Client/Assets/Unused`는 어떤 구성에서도 로드·복사하지 않으며 원본은 보존합니다.

## 마무리

최근 변경은 [코드 품질 수정 기록](Reviews/QualityFixes-2026-10-12.md)에 있습니다.
효과 값/보간 검증을 EffectValidation에서 공유하고 직접 세션 생성에도 적용했습니다.
플레이 중 보간식 오류는 해당 트랙의 마지막 정상값을 유지하고 공통 오류 HUD에 표시합니다.
첫 실패의 초기값은 검증된 From이며, 다음 정상 평가에서 재개하고 Reset/SetEffects 때 오류를 지웁니다.
Separate 조회 범위는 실제 노트 배율 인덱스를 반영하고 사용자 식의 그리드는 끝값으로 제외하지 않습니다.
분석 Controller는 복사/이동할 수 없습니다. 엔진 숨김 실행의 fatal error는 모달 UI 없이 종료합니다.
엔진과 Client의 Debug/Release x64 전체 재빌드, 엔진 SDK 및 숨김 오류 주입 테스트,
각 구성 44개 로직·카탈로그와 4개 Client smoke, 에디터·판정/오류 HUD 회귀를 통과했습니다.
실제 D3D12/FMOD 자동 replay도 효과 채보 15개와 전체 종류 채보 19개 노트를 완료했습니다.
Client 소유 4개 프로젝트의 MSVC 정적 분석은 새 경고 없이 완료했고 기존 3종 경고는
Verification에 구분했습니다. 사용자 곡/스킨·기존 Option.ini는 보존했습니다.
실제 화면/물리 입력/청음은 수동 확인하지 않았습니다. 상세 결과는 Verification이 기준입니다.

[Verification](Verification.md)에 따라 검증하고 실제 결과만 보고합니다.
smoke 통과는 픽셀/마우스 수동 검증을 뜻하지 않습니다.
영구 계약은 기능 문서 한 곳에서 갱신하고 이 문서는 현재 주의점만 유지합니다.
엔진 변경 시 엔진 원격 반영 → Client gitlink 순서로 동기화합니다.
완료 보고에는 변경 클래스/파일과 책임 분리, 기존 동작에 미치는 영향 및 검증 결과를 브리핑합니다.
