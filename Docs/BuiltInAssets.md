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
- `Option.ini`: 자산 루트와 별개로 항상 EXE 옆

개발용 폴더의 존재 여부로 구분하므로 배포 폴더는 저장소의 빌드 위치와 분리합니다.
내장 글꼴과 누락 스킨의 기본본은 기존 RCDATA 캐시에서 읽습니다.

Release 배포 폴더에는 `MyRhythmGame.exe`와 같은 위치의 `fmod.dll`이 필요합니다.
기본 곡을 제공할 때는 개발용 Songs에서 제공할 곡만 골라 `Asset/Songs`에 넣습니다. 로고·기본 스킨·
글꼴만 필요하면 Songs 없이도 시작할 수 있으나 곡 목록은 비어 있습니다.
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

## 선택 우선순위

스킨 파일 하나를 요청할 때의 순서는 다음과 같습니다.

1. `<자산 루트>/Skins/<선택한 스킨 폴더>/<relative path>`
2. EXE에서 추출한 내장 `Default Skin/<relative path>`

따라서 외부 스킨 디렉터리에 일부 파일만 있어도 존재하는 파일은 그대로 사용하고
누락된 파일만 내장 기본본으로 대체합니다. 옵션 패널의 스킨 목록은 선택한 자산 루트의
`Skins` 하위 폴더를 읽으며 `Default Skin`은 외부 폴더가 없어도 표시합니다.
선택한 폴더 이름은 EXE 옆 `Option.ini`의 `General/SkinSet`에 UTF-8로 저장되어
다음 실행 때 복원됩니다. 설정 파일의 계약은 [TextManagement](TextManagement.md)에 있습니다.
폴더가 사라지면 내장 기본 스킨을 선택합니다.
스킨 전환 후 로고 이미지는 즉시 다시 읽고, 플레이/에디터 자산은 해당 화면에
진입할 때 선택된 스킨에서 읽습니다. 리절트 PNG도 진입 시 선택한 스킨의 `Result`에서 읽습니다.

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
플레이는 `InGame`, 기존 로고는 `TitleImage/Logo`, 리절트는 `Result`를 사용합니다.
리절트의 장식은 Penpot Result · Sky의 SVG/PNG에서 내보내고 래스터화한 자산입니다.
텍스트·실제 통계·그래프·패널·버튼은 런타임 요소로 남겨 언어/값/해상도에 대응합니다.

| 리절트 상대 경로 | PNG 크기 | 용도 |
| --- | --- | --- |
| `Result/Background/Sky.png` | 1920×1080 | 시안의 RightFade 배경과 하늘 veil |
| `Result/Background/Clouds.png` | 1920×168 | 하단 투명 구름 |
| `Result/Mode/Taiko.png` | 48×48 | 모드 드럼 아이콘 |
| `Result/Rank/Frame.png` | 509×336 | 링·날개 장식(배치 폭 508.5) |

외부 스킨도 같은 상대 경로로 각각 교체합니다. 글꼴은 내장 `Assets/Fonts/NotoSansKR.ttf`이며,
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
