# 문구·언어·글꼴 관리

게임의 고정 UI 문구는 UTF-8 INI로 관리하며 화면별 문구 계약과 글꼴은 `Client/Texts`가 소유합니다.
`FingerDrumGame`이 하나의 `TextCatalog`를 소유하고 각 View/Presenter에 빌려주므로,
로고에서 바꾼 언어가 곡 선택·플레이·에디터에도 이어집니다. 기본 언어는 한국어입니다.

```text
Client/Texts/
  TextCatalog.*                         언어 목록, 현재 언어, 언어별 글꼴
  Languages/en.ini, ko.ini              기본 제공 UI 문구의 원본
  Submodules/LanguageFile.*             UTF-8 INI 파싱·자리표시자 검증
  GameIdentity.h                        언어와 무관한 제품 표시 이름
  Options/OptionTexts.*                 공통 옵션 패널
  GameScene/FingerDrumLogoScene/        로고 화면
  GameScene/MusicSelectScene/           플레이/에디터 곡 선택
  GameScene/RhythmTestScene/            플레이 화면
  GameScene/ResultScene/                리절트 문구·통계 축·액션
  EditorScene/                          공통 에디터 화면
    Taiko/TaikoEditorTexts.*            Taiko 도구/동·캇 이펙트 문구
```

물리 폴더와 `MRG.Client.vcxproj.filters`의 `Texts` 트리는 같습니다.
화면별 `*TextSet`은 INI 키를 해당 화면의 이름 있는 필드로 묶습니다.
문자열 값이나 언어 선택 분기는 이 C++ 표에 복제하지 않습니다.

## 언어파일 교체

실제로 읽는 파일은 `<자산 루트>/Languages/ko.ini`, `en.ini`입니다.
개발 실행은 `Client/Assets/Languages`, 배포는 EXE 옆 `Asset/Languages`를 사용합니다.
`Option.ini`의 기존 `General/Language=ko` 또는 `en` 선택을 그대로 사용합니다.
파일은 TextCatalog 생성 시 한 번 읽으므로 수정/교체 후 게임을 다시 실행합니다.
언어 선택 전환과 화면 그리기 중에는 파일을 다시 읽거나 디렉터리를 탐색하지 않습니다.

```ini
[Logo]
Start="게임 시작"
Editor="에디터"
Options="옵션"
Exit="종료"

[Options]
title="옵션 설정"
```

메인·옵션·곡 선택·플레이·일시정지·리절트·공통/Taiko 에디터의 고정 라벨/도움말과
판정명·단위·성능 표시도 같은 파일에 있습니다. 섹션과 키의 대소문자는 구분합니다.
곡명·아티스트·패턴 이름, 장치/물리 키 이름, 차트/엔진에서 전달한 진단 원문은 데이터이며
번역하지 않습니다. 진단 접두사/일반 UI 안내는 언어파일이 소유합니다.
게임 제품명은 아래 GameIdentity 계약을 따르고, PNG에 그려진 글자는 스킨 자산입니다.

누락/잘못된 항목은 **선택 언어 파일 → 외부 en.ini → 실행 파일의 영어 기본 문구** 순서로
찾습니다. 한국어 C++ 상수로 돌아가는 경로는 없습니다. 영어 기본 문구는 원본
`Client/Texts/Languages/en.ini`에서 빌드 시 생성해 EXE에 포함하므로 외부 파일이 없어도 유지됩니다.
빌드는 원본 en/ko를 개발 폴더에 처음 한 번 복사하고 이미 존재하는 편집본은 덮어쓰지 않습니다.
새 번역 기본본을 배포할 때는 원하는 INI를 배포 폴더의 `Asset/Languages`에 직접 넣습니다.

INI는 UTF-8(BOM 선택), LF/CRLF를 지원합니다. 빈 줄과 `;`/`#`로 시작하는 주석을 허용하고,
값 안의 `;`/`#`는 주석으로 자르지 않습니다. 따옴표 없는 값은 양끝 공백을 제거합니다.
따옴표 값은 공백을 보존하며 `\n`, `\r`, `\t`, `\\`, `\"` 이스케이프를 지원합니다.
빈 값은 명시적으로 빈 문구를 뜻합니다. 중복 키는 처음 값을 보존하고 뒤의 중복을 무시합니다.
잘못된 UTF-8/NUL은 해당 파일을, 잘못된 구문/이스케이프는 해당 항목만 무시합니다.
알 수 없는 키와 형식 오류는 디버그 출력에 남기고 게임 시작을 막지 않습니다.

서식 문구는 영어 기본본의 인수 개수와 서식 지정자를 유지해야 합니다.
`{}`를 `{0}`, `{1}` 등으로 바꾸어 순서를 바꿀 수 있지만 자동/명시 인덱스는 섞지 않습니다.
`{:.6f}` 같은 정밀도/타입을 바꾸거나 인수를 누락/추가한 항목은 로딩 때 제외합니다.
이는 잘못된 번역으로 플레이 중 format_error가 발생하거나 정확도 정밀도가 바뀌는 것을 막습니다.
문자 자체의 중괄호는 `{{`, `}}`로 적습니다.

TextCatalog가 읽은 문자열은 실행 중 불변이며 화면은 해당 소유자의 참조를 받습니다.
화면별 표의 string_view와 에디터 콜백은 그 소유자보다 오래 살아서는 안 됩니다.
카탈로그의 복사/이동을 허용하지 않으며 View/Scene은 같은 게임 수명의 카탈로그를 공유합니다.

## 게임 이름 변경

`Client/Texts/GameIdentity.h`의 `finger_drum::texts::GameName`이 제품 표시 이름의 유일한 정의입니다.
이 값을 바꾸고 다시 빌드하면 게임 창 제목과 시작/내장 자산 오류창에 함께 적용됩니다.
에디터의 미저장 변경 확인창은 같은 이름에 EditorTexts의 언어별 `editorTitleSuffix`를 붙입니다.
접미사 표에 게임 이름을 복사하거나 언어마다 같은 이름을 따로 정의하지 않습니다.
이름은 정적 수명의 UTF-16 문자열이므로 Win32 제목에도 변환 없이 전달합니다.

저장소·프로젝트/클래스명, Scene/Canvas ID, EXE 파일명과 자산 캐시 경로는 표시 이름과
분리된 내부 식별자입니다. 표시 이름을 바꿔도 기존 설정/캐시/스킨 참조는 유지됩니다.
재사용 엔진의 기본 창 제목은 Client가 GetEngineConfig에서 덮어씁니다.
현재 로고 PNG의 `Logo` 문구는 별도의 임시 그림이며 게임 이름 문자열을 포함하지 않습니다.

## 언어와 글꼴 추가

`TextCatalog.cpp`의 `LanguageProfile`은 언어 이름과 `TextFont`를 한 쌍으로
관리합니다. 현재 한국어와 영어는 Windows의 `Segoe UI`를 사용합니다. 시스템
글꼴은 `TextFontSource::System`, 배포 글꼴 파일은 `TextFontSource::File`로
지정할 수 있습니다. 파일 글꼴을 배포할 때는 글꼴 파일 옆에 라이선스를 함께 둡니다.
리절트는 Penpot 지정 글꼴인 내장 Noto Sans KR을 별도로 사용합니다.
경로는 `AssetPaths::fonts::Result`, 굵기·크기는 TaikoResultView의 Label/Button 호출에서
정하며 두 언어에 같은 글꼴을 적용합니다. 파일·라이선스는 [BuiltInAssets](BuiltInAssets.md)에 있습니다.

언어를 추가할 때는 다음을 함께 변경합니다.

1. 현재 지원 언어는 한국어/영어입니다. 새 언어 지원 시 `Language`, 파일/프로필 목록과
   Option.ini의 언어 코드 매핑을 함께 추가합니다.
2. 해당 언어 INI를 작성합니다. 빠진 키는 영어를 사용합니다. 화면별 C++ 언어 표는 추가하지 않습니다.
3. `TextCatalog::SetLanguage` 뒤 revision을 보는 View가 표시 문구와 글꼴을
   다시 적용하는지 확인합니다.

런타임 진단은 종류와 원문을 상태에 보관하고 View에서 현재 언어의 접두사와
조합합니다. 따라서 언어를 바꾸면 이미 표시 중인 오류 제목도 함께 갱신됩니다.

## 렌더링 수명

TextCatalog는 문구와 글꼴 선택을 소유하고, 엔진 TextVisual/ComboBox는 공유
`TextContent` payload를 게시합니다. 문자열·글꼴·굵기·크기·정렬이 바뀔 때 payload를
교체하며 색상·위치·clip 변경에는 같은 payload를 유지합니다. D3D12 renderer가
글꼴과 배치 handle을 유지하므로 Client에 DirectWrite 객체나 아틀라스 UV를 저장하지 않습니다.
직접 DrawPacket을 만드는 에디터도 `packet.text`에 이 payload를 전달합니다.
자세한 배치·측정·아틀라스 수명은
[엔진 텍스트 계약](../Dependencies/MRG-Engine/Docs/TextRendering.md), 구현/검증은
[텍스트 개선 기록](Reviews/TextRenderingFixes-2026-10-14.md)에 있습니다.

## 옵션 패널

`Client/Presentation/OptionsPanel.*`은 로고와 곡 선택 View가 조립하는 공통
Visual2D 패널입니다.

- 로고의 `옵션` 버튼, 곡 선택의 `OPTION SELECT` 버튼 또는 두 화면의 `Ctrl+O`로
  열고 닫습니다. 열린 패널은 `ESC`로도 닫습니다. 마우스가 밖에 있어도 처리하고
  펼친 ComboBox·드래그 상태를 정리합니다. 닫는 프레임의 ESC는 뒤 화면에 전달하지 않습니다.
- 1280×720 논리 화면에서 폭 320, 높이 720이며 왼쪽 밖에서 0.2초 동안
  smoothstep으로 들어옵니다.
- 패널 안에서 휠, 가운데 버튼 드래그, 빈 영역의 왼쪽 버튼 드래그로 세로
  스크롤합니다.
- 언어와 스킨 ComboBox에 오디오 미들웨어·출력 방식·드라이버 ComboBox 3개를
  추가했습니다. 현재 설치된 미들웨어는 FMOD 하나이며 일반(자동)·WASAPI·ASIO를
  선택합니다. 드라이버 목록은 현재 출력 방식이 제공한 실제 장치와 번호만 표시합니다.
- 출력 방식·드라이버는 같은 AudioSystem에서 즉시 바꾸고 목록을 갱신합니다.
  현재 제공하는 선택에는 게임 재시작이 필요하지 않습니다. 실패 시 기존 선택을
  유지하거나 복원하고 패널 상태 메시지를 표시합니다. 사용할 장치가 없으면
  드라이버 선택을 비활성화합니다.
- 언어 선택은 즉시 활성 화면의 문구를 다시 그리고, 스킨 선택은
  외부 자산 루트의 `Skins` 폴더 이름을 저장합니다. 스킨 자산의
  경로와 파일별 기본본 대체 규칙은 [BuiltInAssets](BuiltInAssets.md)에 있습니다.

패널이 열리거나 닫히는 동안에는 뒤쪽 화면의 선택·검색·전환 입력을 처리하지
않습니다. 옵션 패널의 노드는 다른 anchor보다 높은 Z 순서로 배치해 표시와
hit-test가 항상 앞에 오도록 합니다.

## 공통 UI 색상

`Client/Presentation/UiColors.h`는 로고·곡 선택·에디터·Result·일시정지·옵션의
공통 색상 값을 소유합니다. 각 화면의 Layout/Style은 그 색상을 자신의 표시 역할에
연결합니다. 공통 팔레트의 RGB/흰색/강조색은 한 곳에서 수정합니다.
Scene마다 필요한 색상군과 alpha 차이는 이름으로 구분하여 기존 디자인 값을 보존합니다.

옵션 배경은 기존 곡 선택 색상군의 `sky::PaleBorder`를 불투명한 표면으로 재사용합니다.
입력 칸은 `sky::Pale`, hover는 `sky::Focus`, 강조는 `sky::Accent`를 사용합니다.
배경·입력 칸·문구가 각각 구분되며 문구/오류 색도 공통 표에서 가져옵니다.
노트 종류·판정 구간/키빔의 의미 색은 기존 게임플레이 표시 정책이 소유합니다.

모드별 키 설정 참고 구조는 osu!lazer의
[RulesetBindingsSection](https://github.com/ppy/osu/blob/master/osu.Game/Overlays/Settings/Sections/Input/RulesetBindingsSection.cs)과
[VariantBindingsSubsection](https://github.com/ppy/osu/blob/master/osu.Game/Overlays/Settings/Sections/Input/VariantBindingsSubsection.cs)입니다.
모드/variant 단위의 섹션을 만들고 모드가 제공한 입력과 기본 binding으로 행을 구성합니다.
현재 Taiko 기본 4키(D/F/J/K)와 보조 4키(C/V/N/M)를 모드별 행으로 표시합니다.
행의 버튼을 클릭하고 새 키를 누르면 저장합니다. 동일 모드의 다른 기본/보조 키와
중복할 수 없으며 기존 게임 제어 ESC/Space/R/F7은 예약합니다. ESC는 키 지정 대기를
취소하고 패널을 닫습니다. 사용자 입력은 단일 Win32 Virtual-Key이며 키 조합/마우스 binding은
이번 범위에 없습니다. DLL 로더 자체는 아직 구현하지 않았습니다.

`IPlayGameMode::InputLayout`은 stable mode/binding ID, 표시 이름, semantic action,
기본·보조 physical key 목록을 제공합니다. PlayGameModeFactory의 실제 지원 모드 목록으로
옵션 행을 구성하고 ResolveInputLayout이 기본값과 저장한 변경값을 검증합니다.
PlaySession이 해결된 목록을 소유하며 판정·held·키 불빛·입력 기록에 같은 매핑을 사용합니다.
새 모드는 factory에 구현을 연결하고 자신의 목록을 제공합니다. 런타임 DLL 로딩/ABI 계약은 별도입니다.

해상도 목록은 현재 모니터가 지원하는 실제 픽셀 크기와 현재 창 크기를 표시합니다.
선택 즉시 client 영역 크기를 변경하고 저장하며 다음 실행의 EngineConfig에도 사용합니다.
저장이 실패하면 이전 창 크기 복원을 시도합니다. 엔진의 비소유 WindowControl 서비스가 없는
offscreen context에서는 선택을 비활성화합니다. GPU 한도에 따라 각 치수는 1~16,384px입니다.

글로벌 오프셋 필드는 클릭 → 정수 ms 입력 → Enter로 적용합니다. 숫자키/숫자패드,
음수 부호, Backspace/Delete를 사용할 수 있습니다. ESC는 저장하지 않고 패널을 닫습니다.
+100ms는 음원에 비해 노트·틱·마디선·BPM·YME 지점/영역 끝점을 100ms 늦춥니다.
음원 시작은 여전히 차트 타이머 0ms이며 효과 보간의 구간 길이는 유지합니다.
설정은 다음 플레이/에디터 진입 Snapshot에서 사용합니다. MusicalTimeline의 실행용 정수 us
오프셋으로 컴파일하고 원본 YMP/YME·Pattern Offset은 수정하지 않습니다.
에디터 Copy/Replace/타이밍 수정/저장에도 이 분리를 유지합니다.

OptionsPanel은 Canvas/스크롤/키보드 소유권을 맡고 Submodules의 OptionsDisplay,
OptionsKeys, OptionsOffset이 각 표시·적용 함수를 분리합니다. OptionSettings는 지속 값의
검증/원자적 저장을 맡으며 창·오디오 핸들은 소유하지 않습니다.

## Option.ini

`FingerDrumGame`이 `App/OptionSettings`를 소유하고 로고/곡 선택 패널에 빌려줍니다.
이 클래스는 파일 읽기·쓰기와 저장한 값만 담당합니다. `OptionsPanel`이 기존 엔진
API에 선택을 전달하며 별도 옵션 Controller나 새로운 오디오 미들웨어는 추가하지 않습니다.

EXE와 같은 폴더에 `Option.ini`가 없으면 처음 실행할 때 생성합니다. UTF-8 형식이며
`;`/`#`로 시작하는 주석과 UTF-8 BOM을 읽습니다. 기본값과 키 이름은 다음과 같습니다.

```ini
[General]
Language=ko
SkinSet=Default Skin
GlobalOffsetMs=0

[Display]
Width=1280
Height=720

[Keys.Taiko]
LeftKat=68,67
LeftDon=70,86
RightDon=74,78
RightKat=75,77

[Audio]
Middleware=FMOD
Output=Automatic
DriverIndex=-1
DriverName=
```

- `Language`: `ko` 또는 `en`.
- `GlobalOffsetMs`: signed int32 정수 ms, 기본 0.
- `Display/Width,Height`: 게임 화면 픽셀 크기. 누락 시 1280×720.
- `Keys.<mode ID>`: binding ID별 십진 Virtual-Key 목록. 첫 값은 기본 키, 이후는 보조 키입니다.
  목록을 저장하지 않은 입력은 모드의 기본값을 사용하며 변경한 행만 기록합니다.
  현재 설치되지 않은 모드의 유효한 키 설정도 저장/읽기에서 보존합니다.
  설치된 모드의 알 수 없는 binding/개수·키 충돌·예약 키는 오류입니다.
  위 예시는 기본 Taiko 값이며 실제 최초 파일에는 Keys 섹션을 만들지 않습니다.
- `Middleware`: 현재는 `FMOD`만 사용 가능합니다.
- `Output`: `Automatic`, `WASAPI`, `ASIO`.
- `DriverIndex`: 엔진이 보고한 0부터 시작하는 장치 번호. `-1`은 기본 장치입니다.
  패널의 행 번호와 드라이버 번호가 같다고 가정하지 않습니다.
- `DriverName`: 선택 시 함께 저장합니다. 다음 실행에 장치 번호가 바뀌었으면 이름으로
  찾아 복원하며, 장치가 사라지면 기본 장치를 유지합니다. 다른 출력 방식으로 fallback한
  경우에는 이전 방식의 드라이버 번호/이름을 적용하지 않습니다.
  번호만 직접 지정하려면 `DriverName`을 비워둡니다.

선택이 성공하면 임시 파일을 기록한 뒤 기존 파일을 원자적으로 교체합니다.
쓰기 실패는 기존 저장 값과 언어를 보존하고 변경한 오디오 선택의 복구를 시도합니다.
장치 문제로 복구도 실패하면 그 오류를 함께 표시합니다.
잘못된 값/미설치 미들웨어, 읽기 오류 또는 최초 파일 생성 실패는 기본 설정으로
복구하여 실행을 계속합니다. 원본 INI는 임의로 덮어쓰지 않으며 옵션 패널에 원인과
기본값 사용 경고를 표시합니다. 이후 사용자의 설정 저장이 성공하면 경고를 지웁니다.
설정은 Option.ini만 사용하며 최초 생성에는 위 기본값을 사용합니다.
