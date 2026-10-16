# 내장·외부 자산과 배포

`FingerDrum.Assets`는 배포에 필요한 기본 자산을 한 팩으로 만들고 RCDATA 101로
`MyRhythmGame.exe`에 링크하는 전용 프로젝트입니다. 런타임 로더는 기존 PNG,
글꼴, FMOD 파일 로더를 바꾸지 않고 실제 파일 경로를 제공합니다.

## 포함 범위

- `Assets/Skins/Default Skin` 전체
- `Assets/Fonts`의 세 글꼴과 각각의 라이선스(리절트용 Noto Sans KR 포함)

AngelDream Handshaking MP3 1개·YMM 1개·YMP/YME 5쌍과 이펙트 테스트 WAV 2개는
EXE에 넣지 않습니다. 추가 WAV는 기본 스킨의 Don/Kat 원본을 재사용한 테스트 파일입니다.
Git에서 추적하는 `FingerDrum.Assets/Assets/Songs` 원본을 Client 빌드 시
개발용 `Client/Assets/Songs`에 복사합니다. 이미 존재하는 파일은 편집본일 수
있으므로 덮어쓰지 않습니다. 곡은 개발·배포 모두 외부 파일입니다.

## 외부 자산 루트

`UserAssetRoot`는 EXE 위치 기준 `../../../Client/Assets`가 디렉터리로 존재하면
그 경로를 선택하고, 없으면 EXE 옆 `Asset`을 선택합니다. 선택은 프로세스마다
한 번 수행합니다. 작업 폴더나 Debug/Release 구성에 의존하지 않으며 개발자의
절대 경로를 EXE에 기록하지 않습니다.

- 저장소의 `bin/x64/Debug` 또는 `bin/x64/Release`에서 실행: `Client/Assets`
- 다른 폴더로 EXE를 복사해 실행: `<EXE 폴더>/Asset`
- 곡: `<자산 루트>/Songs`, 사용자 스킨: `<자산 루트>/Skins`
- 편집 가능한 UTF-8 UI 문구: `<자산 루트>/Languages/{ko,en}.ini`
- `Option.ini`: 자산 루트와 별개로 항상 EXE 옆

개발용 폴더의 존재 여부로 구분하므로 배포 폴더는 저장소의 빌드 위치와 분리합니다.
내장 글꼴과 누락 스킨의 기본본은 기존 RCDATA 캐시에서 읽습니다.

Release 배포 폴더에는 `MyRhythmGame.exe`와 같은 위치의 `fmod.dll`이 필요합니다.
기본 곡을 제공할 때는 개발용 Songs에서 제공할 곡만 골라 `Asset/Songs`에 넣습니다. 로고·기본 스킨·
글꼴만 필요하면 Songs 없이도 시작할 수 있으나 곡 목록은 비어 있습니다.
한국어 문구/외부 편집 기능을 제공하려면 원본 `Client/Texts/Languages`의 INI를
`Asset/Languages`에 함께 제공합니다. 언어파일 없이도 내장 영어 기본 문구로 실행됩니다.
영어 기본본은 Client 빌드의 생성 헤더로 포함하며 RCDATA 자산 팩에는 넣지 않습니다.
자세한 키/이스케이프/영어 fallback 규칙은 [TextManagement](TextManagement.md#언어파일-교체)를 참고합니다.
`Client/Assets/Unused`의 예제 PNG는 어떤 구성에서도 복사하거나 로드하지 않습니다.
기존 빌드 출력에 남아 있는 파일까지 빌드가 삭제하지는 않으므로 배포 파일은
깨끗한 폴더에 필요한 파일만 복사해서 만듭니다.

다른 곡과 사용자 스킨은 팩에 자동 포함하지 않습니다. 빌드 스크립트의 명시적인
source group만 포함되므로 사용자 데이터가 우연히 EXE에 들어가지 않습니다.

## 빌드와 실행 흐름

1. `FingerDrum.Assets/Scripts/GenerateAssetPack.ps1`이 상대 경로, 크기, offset과 원본 bytes를
   결정적인 순서로 `BuiltInAssets.fdpak`에 기록합니다. 기존 캐시와 같은 FNV hash를
   빌드 시 계산해 작은 RCDATA 102로 함께 넣으므로 실행 때 팩 전체를 hash하지 않습니다.
2. 생성된 RC를 컴파일한 `FingerDrum.Assets.res`를 `MRG.Client`가 링크합니다.
3. `wWinMain`이 엔진 초기화 전에 RCDATA를 검사합니다.
4. 팩은 `%LOCALAPPDATA%/FingerDrum/BuiltInAssets/<pack hash>`에 임시 디렉터리를
   거쳐 원자적으로 설치됩니다. 같은 hash의 파일 크기와 완료 marker가 유효하면
   기존 캐시를 재사용합니다.
5. 설치 실패나 리소스 누락은 시작 오류로 처리해 불완전한 기본 자산으로 게임을
   계속 실행하지 않습니다.

`InitializeBuiltInAssets`는 실패 시 false를 반환합니다. 오류 메시지 저장 중에도
메모리 할당이 실패하면 메시지를 비우고 false를 유지하며 예외를 밖으로 내보내지 않습니다.
자원 없는 별도 테스트 실행 파일로 이 실패 경계를 검사하므로 사용자 캐시는 건드리지 않습니다.

## skin.ini와 씬별 이미지

스킨 루트에 UTF-8 `skin.ini`를 두고 씬별 폴더를 같은 레벨에 둡니다.
전체 이미지 키와 기본 경로는
[`Default Skin/skin.ini`](../FingerDrum.Assets/Assets/Skins/Default%20Skin/skin.ini)가 원본입니다.
사용자 스킨은 `<자산 루트>/Skins/<스킨 이름>`에 INI를 복사하고 필요한 PNG를 넣습니다.
기본 스킨과 INI는 EXE에 포함되므로 외부 파일이 없어도 실행됩니다.

```text
My Skin/
  skin.ini
  Logo/center.png
       Menu/start.png
  InGame/circle.png
  MusicSelect/background.png
  Result/sky.png
```

```ini
[Folders]
Logo=Logo
InGame=InGame
MusicSelect=MusicSelect
Result=Result

[Logo]
LogoCenter=center.png
StartButton=Menu/start.png
StartButton.Hovered=Menu/start-hover.png

[InGame]
note=circle.png

[MusicSelect]
Background=background.png

[Result]
Background.Sky=sky.png
```

`Folders.Logo`는 **skin.ini 폴더 기준**, `Logo.LogoCenter`는 **지정된 Logo 폴더 기준**입니다.
씬 폴더도 `Images/Title`처럼 바꿀 수 있습니다. 절대/드라이브 경로와 `..`는 허용하지 않습니다.
`/`와 역슬래시를 모두 경로 구분자로 사용할 수 있습니다. 섹션/키는 대소문자를 구분합니다.
경로는 한글·공백을 지원하고 따옴표로 감쌀 수 있습니다. 역슬래시는 이스케이프하지 않습니다.
UTF-8 BOM과 CRLF/LF를 허용합니다. `;`/`#`로 시작하는 전체 줄만 주석이며,
파일명 안의 기호는 그대로 보존합니다. 중복 키는 첫 항목을 사용합니다.
잘못된 경로/항목은 진단 후 해당 요소의 기본본을 사용하고 잘못된 UTF-8은 파일 전체를 무시합니다.

1. 선택 스킨의 씬 폴더·파일 설정을 사용합니다. 빠진 설정은 내장 INI에서 가져옵니다.
2. 이 조합의 외부 파일이 있으면 사용합니다.
3. 파일이 없으면 **같은 요소의 내장 INI 경로와 PNG**로 대체합니다. 사용자 파일명을 내장 경로에 붙이지 않습니다.
4. 내장 INI도 비어 있는 선택 항목은 기존 색상 패널/버튼 디자인을 사용합니다.

| 섹션 | 이미지 항목 |
| --- | --- |
| `Logo` | `LogoCenter/LogoLeft/LogoRight`, `SelectionCursor`; 선택 `Background`, `StartButton/EditorButton/OptionsButton/ExitButton` |
| `InGame` | 노트·오버레이·롱노트·틱·특수 노트, 레인·판정원·키/불빛, `Judgements.MAX`~`OK`, `Progress.Background/Track/Fill/Marker`; 선택 `PausePanel`, `PauseResume/Rewind/Retry/Retire` |
| `MusicSelect` | 배경·카테고리·기록/곡정보/브라우저 패널, 검색/정렬 필드, 스크롤바, `OptionSelect/Back/Go`, `SongCard/SongCardFocused`, `Difficulty/DifficultySelected` |
| `Result` | `Background.Sky/Background.Clouds`, `Mode.Taiko`, `Rank.Frame`; 선택 `BackButton/RetryButton/ReplayButton` |

표의 `/`는 여러 키를 줄여 쓴 것입니다. 정확한 키는 내장 INI를 참조합니다.
버튼/패널 PNG는 기존 영역에 맞춰 표시하고 배치·입력 영역·런타임 텍스트는 유지합니다.
`키.Hovered`, `키.Pressed`, `키.Disabled`로 상태별 이미지를 지정할 수 있습니다.
생략한 상태는 기본 이미지를 사용하고 원래 RGB와 기존 불투명도를 적용합니다.
기본 이미지가 없는 키는 상태 이미지만으로 활성화되지 않습니다. 언어별 문구는 PNG에 굽지 않는 것을 권장합니다.
부모 패널에 이미지를 지정하면 내부 단색 채움만 숨겨 PNG가 가려지지 않게 합니다.
로고 `Background`는 기존 세 장의 로고 스트립 대신 화면 전체에 표시합니다.

`SkinSetSelection`은 INI와 파일 경로를 선택 revision당 한 번 캐시합니다.
프레임마다 파일을 읽거나 탐색하지 않습니다. 살아 있는 로고/곡 선택의 버튼·패널은 노드가
`SkinVisualComponent`를 소유하며 스킨 변경 시 이미지/기본 디자인을 갱신합니다.
INI를 편집했다면 재실행하거나 다른 스킨을 선택했다 돌아오면 다시 읽습니다.
플레이·에디터·리절트는 화면 진입 시 같은 매핑을 사용합니다. 에디터 노트도 `InGame` 키를 공유합니다.

옵션의 목록과 `Option.ini`의 `General/SkinSet`은 계속 스킨 **폴더 이름**을 사용합니다.
`Default Skin`은 외부 폴더가 없어도 표시하며 사라진 선택 폴더는 기본본으로 대체합니다.
이미지 설정과 히트사운드의 기존 상대 경로는 독립적입니다.

풍선 완료 효과음은 `HitSounds/TaikoMode/pop.wav`입니다. 다른 스킨에서 이 파일을
제공하면 해당 파일을 사용하고, 없으면 내장 기본 스킨의 소리로 대체합니다.
판정 링·문구 8종도 `InGame/Judgements/MAX.png`부터 `POOR.png`, `OK.png`까지 같은 파일별
fallback을 사용합니다. 인디케이터는 별도 색 구간 요소로 구성합니다.
판정별 파일명과 표시 계약은 [Presentation](Gameplay/Presentation.md)에 있습니다.
상단 진행바는 `InGame/Progress/{Background,Track,Fill,Marker}.png` 네 파일을 사용합니다.
각 요소는 따로 교체할 수 있고 누락 파일만 내장본으로 대체합니다. 시간 문구는 PNG에
포함하지 않고 런타임에 표시합니다. 제작 크기와 시간 기준은 같은 Presentation 문서에 있습니다.

## Scene별 PNG

내장 원본은 `FingerDrum.Assets/Assets/Skins/Default Skin` 아래 실제 폴더/VS 필터로 정리합니다.
플레이는 `InGame`, 로고는 `Logo`, 곡 선택은 `MusicSelect`, 리절트는 `Result`를 사용합니다.
각 폴더는 루트의 `skin.ini`와 같은 레벨입니다. 곡 선택의 기본본은 코드의 색상 패널을 유지하므로
PNG가 필수는 아닙니다.
리절트의 장식은 Penpot Result · Sky의 SVG/PNG에서 내보내고 래스터화한 자산입니다.
텍스트·실제 통계·그래프·패널·버튼은 런타임 요소로 남겨 언어/값/해상도에 대응합니다.

| 리절트 상대 경로 | PNG 크기 | 용도 |
| --- | --- | --- |
| `Result/Background/Sky.png` | 1920×1080 | 시안의 RightFade 배경과 하늘 veil |
| `Result/Background/Clouds.png` | 1920×168 | 하단 투명 구름 |
| `Result/Mode/Taiko.png` | 48×48 | 짙은 상단 배너용 모드 드럼 아이콘(흰색 채, 2026-10-15 Penpot 갱신본) |
| `Result/Rank/Frame.png` | 509×336 | 링·날개 장식(배치 폭 508.5) |

외부 스킨은 `skin.ini`의 상대 경로로 각각 교체합니다. 글꼴은 내장 `Assets/Fonts/NotoSansKR.ttf`이며,
원본 Google Fonts의 가변 굵기 파일과 `NotoSansKR-OFL.txt`(SIL OFL 1.1)를 함께 배포합니다.
원본 파일: https://github.com/google/fonts/tree/main/ofl/notosanskr . 글꼴 bytes는 수정하지 않았습니다.

곡 선택은 선택한 자산 루트의 `Songs`만 읽습니다. 곡을 캐시에서 다시 생성하지
않으며, YMP의 `Effect file`이 참조하는 YME도 사용자가 직접 편집할 수 있습니다.
히트사운드 표의 경로는 YME 폴더 기준입니다. 내장 글꼴은 캐시 경로에서
직접 읽으며 라이선스도 같은 팩에 보존됩니다.

## 난이도별 파일 배치

`Songs/Patterns/<곡 폴더>/<난이도 이름>`마다 YMP와 YME 한 쌍을 둡니다.
두 파일은 같은 이름에 확장자만 다르게 작성하고, 효과가 없는 난이도에도 빈
YME를 둡니다. YMP의 `Effect file`에는 같은 폴더의 YME 파일명을 적습니다.
효과 전용 사운드는 그 난이도 폴더의 `Sounds`에 두고 YME 기준으로 참조합니다.

```text
Songs/
  Music/Angeldream/angel dream hand shaking.ymm
  Patterns/angeldream/
    Dream/angeldream.ymp
          angeldream.yme
    All Notes Verification/angeldream [all notes test].ymp
                           angeldream [all notes test].yme
    YME Effects Verification/angeldream [effects test].ymp
                             angeldream [effects test].yme
                             Sounds/don.wav
                             Sounds/kat.wav
```

YMP의 `Music metadata`는 계속 Songs 기준으로 공유 YMM을 참조합니다.
카탈로그는 하위 폴더를 재귀 검색하므로 이 정리 때문에 파일 문법이나
플레이 타이밍 규칙을 바꾸지는 않습니다. 에디터는 선택한 쌍을 같은 위치에 저장합니다.

## 자산 변경

기본 자산은 `FingerDrum.Assets/Assets`에서 수정합니다. 새 배포 자산 종류를
추가하려면 프로젝트의 `BuiltInAsset` 목록과 `GenerateAssetPack.ps1`의 명시적
source group을 함께 갱신합니다. Scene에는 물리 경로를 새로 하드코딩하지 않고
`Client/App/AssetPaths.h` 또는 `FingerDrumAssets.h`의 resolver를 사용합니다.

현재 캐시는 새 팩마다 새 hash 디렉터리를 만들며 이전 버전 디렉터리를 자동으로
삭제하지 않습니다. 사용자 곡과 스킨 선택 UI는 Client의 책임입니다.

기본 스킨의 사용되지 않는 `JudgeImage` PNG 7개와 resolver는 제거했습니다.
현재 판정 자산은 `InGame/Judgements`가 소유합니다. 사용자 외부 스킨은 삭제하지 않습니다.
