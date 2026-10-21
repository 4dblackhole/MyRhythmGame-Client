# 레거시 제거 기록

사용자 요청에 따라 Client와 독립 엔진 저장소를 검사하고, Client에 보관된
ColoredCube 계열 기술 예제까지 제거했습니다. 새 엔진 검증 실행 프로젝트는 만들지 않았습니다.
빌드/실행 결과는 [Verification](../Verification.md)에 기록합니다.

| 대상 | 변경과 현재 계약 |
| --- | --- |
| ColoredCubeGame, ColoredCube/Blank/Mesh/Collision/Widget Scene | 소스 13개와 보관 문서 5개 삭제. Main의 예제 선택 분기와 프로젝트/필터 등록도 제거. Debug/Release 모두 YOUMU 게임만 실행. |
| 구형 skin-set.txt 가져오기 | ImportLegacySkin과 Win32 known-folder 의존 제거. 최초 Option.ini는 현재 기본값으로 생성하고 이후 Option.ini만 사용. |
| 빈 모드의 Taiko 대체 | EditorModeFactory, 에디터 파일 열기, 곡 선택 플레이 진입, Taiko 세션 생성에서 빈/다른 모드를 거부. 유효 채보는 Mode: Taiko를 명시. |
| YMP 안의 구형 효과 명령 | 알 수 없는 타이밍 명령을 Warning으로 무시하던 경로 제거. #measure/#bpm/#delay만 허용하고 나머지는 Error. 효과는 YME 소유. |
| JudgementProfile | 호환 별칭/헤더 삭제. 모든 선언·호출·테스트는 AccuracyRange를 직접 사용. |
| Note.h/ChartDocument.h | 호환 통합 헤더와 빈 ChartDocument.cpp 삭제. 소비 파일은 실제 계약·모델·규칙 헤더를 포함. 프로젝트/필터 등록도 정리. |
| 아키텍처 예외와 문서 | 예제 Scene의 디렉터리 검사 예외 제거. AGENTS·라우팅·시작 흐름·기능 문서를 현재 코드에 맞춤. |
| 엔진 | 원본/공개 헤더, SDK 생성 목록, 타입 별칭과 직접 호출 관계 검사에서 별도의 구형 호환 API는 발견되지 않음. 제거된 Client 예제를 가리키던 문서 링크 정리. 엔진 API/구현 변경 없음. |

현재 계약으로 구분한 항목은 다음과 같습니다.

- `PhantomCore.h`는 현재 지원되는 단일 공개 SDK이며 과거 호출부용 호환 헤더가 아닙니다.
  Texture/Mesh handle과 callback 별칭도 현재 소유권·호출 계약에 사용됩니다.
- Collision/Geometry/Visual2D 등 재사용 엔진 기능은 예제 Scene과 독립된 기능입니다.
  엔진의 SDK 단위 테스트와 Client의 로직/presentation/smoke 테스트는 유지합니다.
- `RhythmTestScene`은 현재 Taiko 게임플레이 Scene의 이름입니다. 이를 더미 Scene으로
  취급해 삭제하지 않으며 실제 플레이/오디오/판정 회귀로 검증합니다.
- YMP의 `--`/`---`, 마디 길이, 이름/숫자형 히트 횟수 옵션은 현재 문법입니다.
  예전 테스트 이름에 Legacy가 포함되어 있었지만 유효한 마디/정수 µs 검사는 유지하고
  구형 명령 허용 검사를 명령 거부 검사로 교체했습니다.
- `Preview Time`은 이미 전용 파서/모델/런타임 기능이 없는 알 수 없는 metadata입니다.
  문법 문서의 지원 항목에서 제거했습니다. 사용자 곡/스킨/Option.ini 및 구형 설정 파일,
  빌드에 포함되지 않는 Unused 이미지 원본은 임의 삭제하거나 마이그레이션하지 않습니다.

과거 날짜의 검증 기록은 당시 실행한 결과이므로 과거 예제/호환 기능의 언급을
다시 쓰지 않습니다. 현재 동작과 파일 존재 여부는 위 계약과 최신 검증 기록을 기준으로 합니다.
