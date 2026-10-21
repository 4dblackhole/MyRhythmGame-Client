# 엔진·변환기·클라이언트 리뷰 수정

대상은 `엔진·컨버터·클라이언트 코드리뷰` 세션에서 재현한 C1/C2/E1/E2/V1/V2/V3/C3입니다.
원본 리뷰는 ignored `build/code-review-2026-10-21/CodeReview-2026-10-22.md`에 있습니다.

| 항목 | 변경과 책임 | 회귀 검사 |
| --- | --- | --- |
| C1 판정 유실 | Lane이 입력 시각 전에 완전히 만료된 노트를 이전 유지 상태로 처리한 뒤 다음 노트에 같은 입력을 전달합니다. Controller는 Raw Input 묶음의 첫 전이 전 물리 상태를 복원합니다. | 마지막 Update가 89ms인 100ms 정타, 연속 만료, 닫힌 만료 경계, 롱테일 뒤 헤드, Update 분할 유무의 동일 결과 |
| C2 Buzz 유지 입력 | PlaySession이 수락 여부와 관계없이 mapped 물리 키를 소유합니다. NoteTypes의 입력에 전이 후 action 유지 여부를 값으로 전달하고 HoldInputRule은 이를 사용합니다. rewind도 물리 snapshot을 복원합니다. | 시작 전 보조 키, 추가 키, 마지막 해제, 같은 tick의 전이, Pause snapshot과 rewind의 exactly-once tick |
| E1 descriptor 회수 | 엔진 TextureSet의 lease가 원래 heap pool을 소유합니다. 업로드·material·RenderToTexture의 GPU 프레임 참조가 해제될 때만 64칸 SRV 블록을 반환합니다. | 즉시 해제 생성 200회, 동시 16세트 용량 거부, upload fence, 실제 GPU 64프레임의 유지·회수와 descriptor 중복 방지 |
| E2 입력 손실 | InputState의 256건 고정 배열을 확장/재사용 버퍼로 바꿨습니다. 메모리 부족은 Win32 callback 밖에서 오류로 보고하여 불완전한 이벤트 묶음을 넘기지 않습니다. | 4,096개 혼합 이벤트의 키 전이·timestamp·순서와 snapshot/다음 Update 재사용 |
| V1 볼륨 기본값 | 변환기가 General SampleVolume을 TimingPoints 생략 필드에 적용하고 명시값을 우선합니다. | 생략/명시/0/100과 잘못된 기본값 |
| V2 스피너 횟수 | 원작의 DifficultyRange→승격된 `1.65f`→duration/1000→rate 연산을 보존합니다. 다른 타이밍 계산 정밀도는 유지합니다. | 고정 원본 공식에 따른 OD/시간 경계 25개, OD5·4초=32회 |
| V3 동시 변환 | 패키지·라이브러리 모두 import 전체와 BG rollback 동안 해당 Songs 루트의 프로세스 간 배타 잠금을 소유합니다. 같은 루트는 즉시 오류(code 5), 다른 루트는 독립 진행합니다. | 별도 프로세스 BG 게시 중 실제 CLI 거부, 다른 루트 진행, 기존 파일/재사용 BG 보존, 실패/강제 종료 후 잠금 정리 |
| C3 저장 별칭 | ChartEditor가 YMP/YME/각 tmp/bak 여섯 경로를 먼저 검사합니다. Windows 이름 비교·canonical parent·기존 hardlink 별칭을 거부하며 원본과 dirty/Undo/Redo를 보존합니다. | 양방향 tmp/bak 충돌, 직접/대소문자/상대경로/hardlink 별칭, 기존 recovery 보호와 실패 후 재저장 |

판정의 시간 원천은 QPC→RhythmTimer의 정수 us입니다. 렌더 시각을 판정에 사용하지 않습니다.
노트 규칙은 물리 장치를 소유하지 않고 값으로 받은 action 상태만 사용합니다. 직접 규칙을
사용하는 테스트/호출자는 기존 로컬 키 추적을 사용할 수 있으며 실제 PlaySession은 명시적인
전이 후 상태를 제공합니다. 입력 로그에는 이 값도 복사되어 수명에 종속된 span을 남기지 않습니다.
일반 입력과 rewind는 같은 `ApplyPhysicalInput`/`RefreshHeldActions`를 사용합니다.
되감기의 별도 set과 입력마다 생성하던 임시 물리 키 vector를 제거해 상태 해석을 한 곳에 둡니다.

저장은 경로 검사→직렬화 검증→두 파일 staging→원본 backup→두 파일 설치→메모리 상태
게시 순서입니다. 별칭 검사는 디렉터리 생성·쓰기·rename보다 먼저 수행합니다.
기존 복구 파일을 자동 삭제하거나 사용자 곡의 형식을 임의로 고치지 않습니다.

변환기 세 항목은 사용자 지시에 따라 기존 변환기 전용 세션과 독립 저장소에서 수정했습니다.
변환기 커밋은 `e670d636360e598f36266ac1ed58f7f17bebbf9a`, 엔진 커밋은
`ebe5726136506763236317f42955d30a2fd75e6e`이며 엔진 원격 게시 후 Client gitlink를 갱신했습니다.
완료 검증의 구성·도구·기존 진단·수동 확인 범위는 [Verification](../Verification.md)의
2026-10-22 절이 소유합니다.
