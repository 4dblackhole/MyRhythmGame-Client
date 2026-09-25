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
  EditorScene/                          에디터 화면
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
- 언어와 스킨 ComboBox가 있습니다. 언어 선택은 즉시 활성 화면의 문구를 다시
  그리고, 스킨 선택은 `assets/skins`의 폴더 이름을 저장합니다. 스킨 자산의
  경로와 파일별 기본본 대체 규칙은 [BuiltInAssets](BuiltInAssets.md)에 있습니다.

패널이 열리거나 닫히는 동안에는 뒤쪽 화면의 선택·검색·전환 입력을 처리하지
않습니다. 옵션 패널의 노드는 다른 anchor보다 높은 Z 순서로 배치해 표시와
hit-test가 항상 앞에 오도록 합니다.
