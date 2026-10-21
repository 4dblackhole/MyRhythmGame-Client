# 2026-10-22 제품 이름과 변환 결과 검증 책임

엔진은 PhantomCore, 게임 내부 프로젝트·코드 식별자는 YOUMU로 통일합니다.
최상위 저장소/폴더는 각각 PhantomCore와 YOUMU이며, 게임 표시 이름은
`Y's Orchestral Unison: Melodic Ultimatum`입니다.

## 이름과 빌드

- Client 솔루션은 `YOUMU.sln`, 실행 프로젝트는 `Client/YOUMU.Client.vcxproj`입니다.
  독립 로직 프로젝트는 YOUMU.Rhythm, Chart, Modes, Editor, Assets, ClientLogic입니다.
- `Client/Texts/GameIdentity.props`의 GameDisplayName이 표시 이름의 원본입니다.
  GenerateGameIdentity.ps1이 UTF-16 GameName 헤더를 만들고 TargetName도 같은 속성에서
  계산합니다. Windows 파일명에서 콜론은 ` -`로 치환하므로 두 구성의 EXE는
  `Y's Orchestral Unison - Melodic Ultimatum.exe`입니다.
- 엔진 소비자는 `Dependencies/PhantomCore/Engine/SDK/PhantomCore.vcxproj` 하나를
  참조하고 `PhantomCore.h`와 `PhantomCore.lib`를 사용합니다. 생성 헤더는 원본 공개
  선언과 GenerateCoreHeader.ps1에서 만들며 이전 이름의 별칭·전달 헤더는 없습니다.
- YOUMUGame과 YOUMULogoScene은 기존 조립·전환 책임을 유지합니다.
  YOUMUSceneIds는 Client의 중앙 라우트 목록이며 값은 YOUMU 접두사입니다.
  기본 자산 캐시와 설치 mutex도 YOUMU 식별자를 사용합니다.

## C++ 이름 범위

엔진의 제품/기능 네임스페이스와 Client의 제품/공통 도메인 네임스페이스를 제거하고
공개 타입을 전역으로 옮겼습니다. using 지시문·별칭으로 이전 API를 유지하지 않습니다.
파일 내부 익명 네임스페이스는 내부 링크를 보존합니다. 반복되는 Ink, Background,
Width, Bounds 등의 UI 색상/배치 상수와 테스트의 공통 보조 범위는 충돌 방지를 위해
유지합니다. std, DirectX, Microsoft 및 외부 라이브러리의 범위는 해당 소유자의 것입니다.

실제 충돌은 용도가 드러나는 이름으로 해소했습니다. 자산의 실행 폴더 조회는
AssetExecutableDirectory, 내부 PCM 창 소비자는 DecodedAudioConsumer,
오디오 미리듣기 틱 간격은 PreviewTickDivision, 에디터 판정 레벨 입력은 ParseJudgeLevel,
결과 색상 팩터리는 ResultColor를 사용합니다. Scene ID 상수도 LogoSceneId 등으로
구별하여 화면별 문구 함수와 충돌하지 않습니다.

## 변환과 검증의 경계

독립 OsuTaikoConverter는 공통 Chart/Rhythm 라이브러리와 태고 노트 규약 헤더를
사용하며 Client UI·Scene·엔진·실제 플레이 세션을 참조하지 않습니다. 변환기의
입력/숫자/경로 검사, 시간 계산, 직렬화·파싱 왕복 및 안전한 파일 저장은 그대로
변환기 책임입니다. 태고 플레이 세션 생성과 원본 대비 노트 누락 검사는 Client 책임입니다.

- EditorOsuImport는 파일 선택, 복사한 경로와 자식 프로세스/비동기 결과의 수명을
  소유합니다. 종료 코드 0을 받으면 OsuImportValidation을 호출합니다.
- OsuImportValidation의 전용 JSON v1 reader는 파일 목록과 sourceObjects/noteRows를
  읽습니다. 저장된 YMM/YMP/YME와 음원이 선택된 Songs 안에 있는지, 서로의 참조 및
  저장 행 수가 결과와 일치하는지 확인합니다. 파싱 오류·누락/중복 필드·잘못된 수치와
  문자열·범위를 벗어나는 경로는 실패입니다.
- 동일 worker 안에서 모드 factory로 순수 플레이 세션을 만들고 모든 레인의 실제
  노트 수를 sourceObjects와 비교합니다. 롱노트의 두 저장 행은 한 플레이 노트로 셉니다.
  세션과 데이터는 worker 지역 객체이며 Scene/오디오/렌더 객체를 참조하지 않습니다.
- MusicSelectScene은 검증 성공 뒤 완료를 알리고 카탈로그를 갱신합니다. 실패 시
  가져오기 오류를 표시하며 원본·기존/공유 Songs 파일을 삭제하지 않습니다.

OsuImportValidationTests는 Unicode 경로, 롱노트의 행/객체 수, 잘못된 JSON·수치,
지원하지 않는 모드/노트, 경로 경계와 기존 파일 보존을 검사합니다. 실제 변환 JSON과
Songs 루트도 같은 실행기에 전달할 수 있습니다. 빌드·분석·회귀 결과는
[Verification](../Verification.md)의 같은 날짜 제품 이름 변경 절에서 기록합니다.
