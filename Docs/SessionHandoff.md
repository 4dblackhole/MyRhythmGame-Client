# 새 관리자·세션 인수인계

## 시작

1. 루트 `AGENTS.md`를 읽고 Client/엔진 status 및 submodule gitlink를 확인합니다.
2. [작업별 문서 표](README.md)에서 필요한 기능만 선택합니다. 전체 문서/채팅은 읽지 않습니다.
3. [변경 영향 지도](ChangeImpactMap.md)로 소유 파일·호출부·테스트를 찾습니다.
4. 요구가 불명확하거나 해석에 따라 동작/설계가 크게 달라지면 구현을 멈추고 질문합니다.

## 작업 위치와 경계

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
- 영구 기록·리플레이 재생, 에디터 Undo/Redo·드래그·다중 선택·음악 재생은 미구현입니다.
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
  로고와 곡 선택은 공통 `OptionsPanel`에서 언어·스킨·FMOD·자동/WASAPI/ASIO·드라이버를 선택합니다.
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
