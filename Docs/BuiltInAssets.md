# EXE 내장 기본 자산

`FingerDrum.Assets`는 배포에 필요한 기본 자산을 한 팩으로 만들고 RCDATA 101로
`MyRhythmGame.exe`에 링크하는 전용 프로젝트입니다. 런타임 로더는 기존 PNG,
글꼴, FMOD, YMM/YMP 파일 로더를 바꾸지 않고 실제 파일 경로를 제공합니다.

## 포함 범위

- `Assets/Skins/Default Skin` 전체
- `Assets/Fonts`의 두 글꼴과 각각의 라이선스
- AngelDream Handshaking MP3와 YMM
- AngelDream YMP 3개

다른 곡과 사용자 스킨은 팩에 자동 포함하지 않습니다. 빌드 스크립트의 명시적인
source group만 포함되므로 사용자 데이터가 우연히 EXE에 들어가지 않습니다.

## 빌드와 실행 흐름

1. `Scripts/GenerateAssetPack.ps1`이 상대 경로, 크기, offset과 원본 bytes를
   결정적인 순서로 `BuiltInAssets.fdpak`에 기록합니다.
2. 생성된 RC를 컴파일한 `FingerDrum.Assets.res`를 `MRG.Client`가 링크합니다.
3. `wWinMain`이 엔진 초기화 전에 RCDATA를 검사합니다.
4. 팩은 `%LOCALAPPDATA%/FingerDrum/BuiltInAssets/<pack hash>`에 임시 디렉터리를
   거쳐 원자적으로 설치됩니다. 같은 hash의 파일 크기와 완료 marker가 유효하면
   기존 캐시를 재사용합니다.
5. 설치 실패나 리소스 누락은 시작 오류로 처리해 불완전한 기본 자산으로 게임을
   계속 실행하지 않습니다.

## 선택 우선순위

스킨 파일 하나를 요청할 때의 순서는 다음과 같습니다.

1. 실행 파일 옆 `assets/skins/Default Skin/<relative path>`
2. EXE에서 추출한 내장 `Default Skin/<relative path>`

따라서 외부 스킨 디렉터리에 일부 파일만 있어도 존재하는 파일은 그대로 사용하고
누락된 파일만 내장 기본본으로 대체합니다. `ResolveSkinAsset`에 다른 스킨 루트를
넘겨도 같은 파일별 fallback 규칙을 적용할 수 있습니다.

곡 선택은 내장 AngelDream 카탈로그를 항상 먼저 만들고, 실행 파일 옆
`assets/songs`가 존재하면 사용자 카탈로그를 합칩니다. 이름, 아티스트, 오디오
파일명이 모두 같은 항목은 중복 표시하지 않습니다. 내장 글꼴은 캐시 경로에서
직접 읽으며 라이선스도 같은 팩에 보존됩니다.

## 자산 변경

기본 자산은 `FingerDrum.Assets/Assets`에서 수정합니다. 새 배포 자산 종류를
추가하려면 프로젝트의 `BuiltInAsset` 목록과 `GenerateAssetPack.ps1`의 명시적
source group을 함께 갱신합니다. Scene에는 물리 경로를 새로 하드코딩하지 않고
`Client/App/AssetPaths.h` 또는 `FingerDrumAssets.h`의 resolver를 사용합니다.

현재 캐시는 새 팩마다 새 hash 디렉터리를 만들며 이전 버전 디렉터리를 자동으로
삭제하지 않습니다. 사용자 곡·스킨 선택 UI 자체는 이 자산 팩의 책임이 아닙니다.
