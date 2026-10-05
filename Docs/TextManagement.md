# 문구·언어·글꼴 관리

게임에 표시되는 고정 문구와 언어별 글꼴은 `Client/Texts`가 소유합니다.
`FingerDrumGame`이 하나의 `TextCatalog`를 소유하고 각 View/Presenter에 빌려주므로,
로고에서 바꾼 언어가 곡 선택·플레이·에디터에도 이어집니다. 기본 언어는 한국어입니다.

```text
Client/Texts/
  TextCatalog.*                         언어 목록, 현재 언어, 언어별 글꼴
  Options/OptionTexts.*                 공통 옵션 패널
  GameScene/FingerDrumLogoScene/        로고 화면
  GameScene/MusicSelectScene/           플레이/에디터 곡 선택
  GameScene/RhythmTestScene/            플레이 화면
  EditorScene/                          공통 에디터 화면
    Taiko/TaikoEditorTexts.*            Taiko 도구/동·캇 이펙트 문구
```

물리 폴더와 `MRG.Client.vcxproj.filters`의 `Texts` 트리는 같습니다. 문구는 사용
화면의 `*TextSet`에 한국어·영어를 같은 필드 순서로 작성합니다. 새 화면 문구를
공용 대형 표에 모으지 않고 그 화면의 파일에 추가합니다.

## 언어와 글꼴 추가

`TextCatalog.cpp`의 `LanguageProfile`은 언어 이름과 `TextFont`를 한 쌍으로
관리합니다. 현재 한국어와 영어는 Windows의 `Segoe UI`를 사용합니다. 시스템
글꼴은 `TextFontSource::System`, 배포 글꼴 파일은 `TextFontSource::File`로
지정할 수 있습니다. 파일 글꼴을 배포할 때는 글꼴 파일 옆에 라이선스를 함께 둡니다.

언어를 추가할 때는 다음을 함께 변경합니다.

1. `Language` 열거형과 `LanguageProfile`을 추가합니다.
2. 각 사용 위치의 `*TextSet`에 새 언어 표를 추가합니다.
3. `TextCatalog::SetLanguage` 뒤 revision을 보는 View가 표시 문구와 글꼴을
   다시 적용하는지 확인합니다.

런타임 진단은 종류와 원문을 상태에 보관하고 View에서 현재 언어의 접두사와
조합합니다. 따라서 언어를 바꾸면 이미 표시 중인 오류 제목도 함께 갱신됩니다.

## 옵션 패널

`Client/Presentation/OptionsPanel.*`은 로고와 곡 선택 View가 조립하는 공통
Visual2D 패널입니다.

- 로고의 `옵션` 버튼, 곡 선택의 `OPTION SELECT` 버튼 또는 두 화면의 `Ctrl+O`로
  열고 닫습니다.
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
  `assets/skins`의 폴더 이름을 저장합니다. 스킨 자산의
  경로와 파일별 기본본 대체 규칙은 [BuiltInAssets](BuiltInAssets.md)에 있습니다.

패널이 열리거나 닫히는 동안에는 뒤쪽 화면의 선택·검색·전환 입력을 처리하지
않습니다. 옵션 패널의 노드는 다른 anchor보다 높은 Z 순서로 배치해 표시와
hit-test가 항상 앞에 오도록 합니다.

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

[Audio]
Middleware=FMOD
Output=Automatic
DriverIndex=-1
DriverName=
```

- `Language`: `ko` 또는 `en`.
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
잘못된 파일은 시작 오류로 알리며 임의로 덮어쓰지 않습니다.
처음 파일을 만들 때만 기존 `%LOCALAPPDATA%/FingerDrum/skin-set.txt`의 선택을 가져옵니다.
이후에는 Option.ini를 사용하며 기존 파일과 사용자 스킨은 삭제하지 않습니다.
