# Claude 코드 리뷰 반영

입력은 사용자가 제공한 `YOUMU 코드 리뷰.html`의 본문이며, 리뷰 기준은
Client `c7fda7c`입니다. 리뷰 자체는 실행/빌드 없이 작성되어 실제 코드와 대조했습니다.
아래는 적용 판단이며, 실행한 검증 결과는 [Verification](../Verification.md)에 기록합니다.

| 지적 | 반영 |
| --- | --- |
| 에디터 롱노트 겹침/플레이 중 노트 소실 | Taiko 배치에서 컴파일 시각과 같은 시각의 ID 순서로 충돌 검사. 기존 파일의 중복 head/일반 노트/짝 없는 tail/head는 기존 플레이 정책을 유지하면서 항목별 Warning 반환. |
| 저장 실패 후 재저장 불가 | 임시 파일 기록도 rollback 경계에 포함. 이번 작업의 임시 파일 정리, 기존 복구 파일 보존, YMP/YME 동일 대상 거부, 디스크 반영 이후 비할당 상태 게시. |
| Option.ini 때문에 시작 실패 | 잘못된 값/읽기/최초 생성 실패에 기본값 사용. 원본 보존 및 옵션 패널 경고. 저장 성공 시 경고 해제. |
| 한글 경로/BOM/알 수 없는 키 | UTF-8 경로 변환, 공통 BOM 제거, YMM/YMP metadata Warning, 파일 입출력 오류를 ParseResult 진단으로 통일. |
| 무제한 마디로 과도한 할당 | 사용자 선택에 따라 100,000마디 상한. 파일·직접 문서 구성·편집·저장·탐색 경계 검사. |
| 수정마다 전체 문서 재구축 | 타이밍과 이펙트 수정 명령 분리. 노트의 소유 트리/안정 ID 유지, 필요한 시간/배율 캐시만 게시. 마디 이월/전체 metadata 교체는 전체 후보 검증 유지. |
| 오류 메시지가 모두 옵션 오류로 표시 | 최상위 오류는 일반 YOUMU 오류로 표시하고 UTF-8을 Win32 Unicode 메시지로 변환. smoke는 모달 창 대신 오류 코드/디버그 출력. |
| 매 프레임 held-key 임시 set | 입력 바인딩 설정 시 버퍼 예약, 프레임별 재사용 및 중복 action 제거. |
| 화면 좌표 중복 | Taiko 악보의 시작점·마디 폭·행 간격·행/열 개수·오른쪽 경계를 그리기/입력에서 공유. |
| 기본 범위 조회가 전체 노트 복사 | 구간 조회를 adapter의 필수 계약으로 변경. 예제 adapter도 실제 범위/롱노트 연결 조회로 위임. |
| 복제된 미사용 include | 노트 규칙 구현에서 사용하지 않는 format/numeric/iterator 제거. |
| 테스트 첫 실패 중단/선택 불가 | 이름별 결과와 합계, 실패 후 계속 실행, `--filter`/`--list`, 실패/빈 선택 시 비정상 종료. 신규 회귀는 실패 경계별 함수로 분리. |
| 긴 수동 presentation 링크 명령 | 에디터 검증은 `RunEditorPresentationTests.ps1`로 통합하고 실제 프로젝트의 object 목록을 읽음. 기존 옵션/판정/replay 스크립트 유지. |
| 서식/분석/CI 설정 없음 | clang-format/clang-tidy/editorconfig 기준과 FMOD 없는 Windows 도메인 CI 추가. 기존 전체 코드 자동 재서식은 하지 않음. |

설계 판단은 다음과 같습니다.

- `IEditorMode`는 현재 하나의 활성 모드가 소유하는 정책이며 분석 소비자는 이미
  `IEditorAudioSource`만 참조합니다. 새 소비자가 없는 인터페이스 분해는 추가하지 않았습니다.
- Taiko의 단일 Lane과 고정 입력 바인딩은 현재 게임 규칙입니다. 다중 Lane/사용자 키 설정과
  Undo/Redo는 기능 제안으로 분리하며 이번 오류/품질 수정에 포함하지 않았습니다.
- 빈 `NoteProcessResult`의 vector는 생성만으로 저장소를 할당하지 않습니다. 실제 이벤트가
  있을 때의 전달 계약은 유지하며 Lane/Gear 합산의 첫 결과는 저장소를 재사용합니다.
  확인된 프레임별 set 할당도 제거했습니다.
- 효과 조회 인덱스는 원본 트리의 비소유 참조입니다. 효과 수정에서 소유 트리와 함께
  게시하고 복사/실패/교체 회귀로 수명을 검사합니다. 포인터를 임의로 공개하지 않습니다.
- formatter/clang-tidy 설정은 변경 영역에 적용하기 위한 기준입니다. 정적 분석에는 실제
  프로젝트 설정의 MSVC Code Analysis도 사용할 수 있으며 필수 빌드/실행을 대체하지 않습니다.

CI 문법은 공식 [checkout](https://github.com/actions/checkout)과
[setup-msbuild](https://github.com/microsoft/setup-msbuild) 사용법을 확인했습니다.
CI가 실제 원격에서 실행되었는지는 Verification의 결과와 구분합니다.
