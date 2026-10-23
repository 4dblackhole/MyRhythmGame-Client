# Taiko 플레이 옵션·로컬 기록 구현 검토 (2026-10-23)

## 요청과 확정한 동작

게임용 곡 선택 하단에 게임 모드 선택/플레이 옵션을 배치했습니다. 현재 등록된
실제 모드는 Taiko입니다. 편집용 곡 선택의 기존 전역 설정은 유지합니다.

사용자 답변에 따라 BGM/HIT 음정 유지 여부를 분리했습니다. Random은 작은 동/캇,
큰 동/큰 캇, 동 버즈/캇 버즈 그룹의 재배치 시도 확률입니다. 보라는 제외하고
같은 시드의 롱노트는 한 번만 추첨합니다. 시도 후 같은 종류가 나올 수도 있습니다.
로컬 기록은 재실행 후에도 유지하며 조건식으로 조회합니다.

세부 사용법과 기록 스키마/쿼리 필드는 [PlayOptions](../Gameplay/PlayOptions.md)에 모았습니다.

## 구현 경계

| 코드 | 책임 |
| --- | --- |
| YOUMU.Modes/Mode/PlayOptions.h | 배속/음정/스크롤/Random/JudgeLevel 값과 검증 |
| TaikoNoteTransform / TaikoSessionBuilder | Mirror 다음 Random, 논리 노트 단위 그룹 변환, 유효 JudgeLevel, 스크롤·메트로놈 컴파일 |
| RhythmTimer | 배속을 포함하는 QPC↔차트↔DSP 변환; 실제 판정 시간은 기본 / 배속 |
| GameplayLaunchRequest / PlaySessionLoader / IPlayResult | 선택→세션→결과/Retry의 옵션 값 스냅샷 |
| GameplayAudioRouter / GameplayAudio | 독립 음정 정책, BGM PCM 변환 준비, 메트로놈 예약·중단·커서 복구 |
| ValueDialog / PlayOptionsDialog | 검증 실패 시 편집 유지, 취소 시 적용하지 않는 입력창 |
| PlayRecordStore / RecordCondition | Windows SQLite, 정확도 정수·전체 uint64 시드 보존, 허용한 조건식만 parameter binding |
| SongSelectRecords / TaikoResultOptions / ResultScene | 비동기 기록 페이지 게시, 옵션 식별 아이콘, 결과 저장 상태 |
| PhantomCore Audio Processing / FmodPcmClip | 고정 속도 PCM 변환과 원본 좌표 탐색; 공개 SDK에는 FMOD 타입 없음 |

옵션은 원본 YMM/YMP/YME를 수정하지 않습니다. metadata cache의 문법/레이아웃도
변경하지 않았으므로 SongCatalog cache revision은 유지합니다. 플레이 옵션 선택은
앱 실행 동안 유지하고, 기록에는 각 시도의 실제 값을 저장합니다. 기록은 정규화된
패턴 절대 경로+모드 기준이며 이동한 파일은 새 경로의 기록칸을 사용합니다.

## 자체 검토

- worker는 경로/옵션/기록/PCM을 값으로 소유하며 Scene, node, native audio 객체를
  비동기 lambda에서 참조하지 않습니다. 엔진 PCM 변환은 완료 후 native clip을 엽니다.
- BGM을 미리 변환해 실시간 pitch DSP의 지연을 입력 반응 경로에 넣지 않습니다.
  공개 길이/seek는 원본 좌표이고 DSP 예약은 배속 시계의 좌표를 사용합니다.
- 판정 프로필은 차트 시간에서 한 번만 적용합니다. 배속으로 시계를 조절한 뒤
  프로필을 다시 나누는 이중 축소가 없습니다.
- Random은 무효/겹친 노트를 걸러 짝지은 뒤 적용하며 원본 PatternDocument와
  명시적 커스텀 히트사운드 참조를 보존합니다.
- 쿼리의 식별자/연산자는 고정 목록에서 생성하고 값은 bind합니다. 길이 2048,
  깊이 32, term 수 128로 제한합니다. 잘못된 조건식은 DB를 수정하지 않습니다.
- 저장은 원자적 SQLite INSERT입니다. 새 스키마/손상 데이터는 오류를 표시하고
  기존 기록을 보존합니다. 구 버전 데이터를 삭제하거나 임의 기록을 채우지 않습니다.
- 선택/쿼리/페이지가 바뀌면 기존 행을 즉시 비우고, 작업 revision이 일치한 결과만
  게시합니다. 기록 목록은 여섯 행씩 페이지로 만들며 전체 Canvas 노드를 늘리지 않습니다.
- 메트로놈은 예약된 박들이 서로 덮어쓰지 않게 별도 voice를 사용합니다. pause/seek
  이후 과거 클릭을 몰아 재생하지 않고 현재 공통 시계에서 다시 예약합니다.
- Signalsmith Stretch/Linear를 고정한 MIT 소스로 사용하고 라이선스를 EXE 옆
  ThirdPartyNotices에 복사합니다. 외부 헤더 분석만 MSVC external scope로 제외하며
  엔진 어댑터/Client 자체 코드는 계속 분석합니다.

## 완료된 검증

- 엔진 Debug/Release x64 전체 Rebuild, 공개 SDK 기본 검사와 --audio-pcm 통과.
  0.5/1/1.5/2배속 PCM 길이/440Hz 음정/스테레오/지연 보정/취소 및 원본 seek 검사.
- Client Debug/Release x64 빌드와 MSVC Code Analysis 통과. 구성별 로직 70개/0 실패.
- Logo/Lobby/Gameplay/Editor smoke 4개씩, 총 8개 통과. Option.ini 해시 보존.
- 실제 옵션 입력창의 값/확인/취소, 기록칸 pagination/조건식, 결과 옵션 아이콘·저장
  worker, 결과→Retry의 지연 전환을 자동 검사했습니다.
- 실제 D3D12/FMOD all-notes replay(Debug): 19개 논리 노트 완료, 1555 render frames.
- pause/rewind/retry·원본 음악 seek, 첫 화면의 Don/Kat, 독립 BGM/HIT 음정 조합과
  1.5배속 cold-start를 검사했습니다.

Debug/Release 각각 MusicSelectView, GameplayPause(기본/cold-start/1.5x cold-start),
ResultPresentation, GameplayJudgement, AllNotesGameplay, EditorPresentation을 통과했습니다.
all-notes 실제 native 재생은 Debug 1555, Release 1554 frames로 19개 논리 노트를
완료했습니다. 공유한 4분음표 박 컴파일러도 에디터 메트로놈의 BPM/delay/offset/음수
시간 이동 회귀를 통과했습니다. Architecture 검사(11개 프로젝트)와 diff --check도 통과했습니다.

## 검증 중 발견한 보조 수정

- 기존 Result 전환 테스트가 비동기 준비 전의 새 gameplay Canvas를 즉시 요구했습니다.
  이전 Result Canvas의 소멸은 즉시 검사하고 새 Canvas는 준비 완료까지 기다리게 수정했습니다.
- 기존 all-notes presentation 테스트의 Require가 namespace 없이 호출되어 컴파일되지
  않았습니다. tests::Require를 명시하여 실제 재생 회귀를 다시 실행했습니다.
- 오래된 Release LTCG 중간 산출물에서 LNK1103이 발생한 뒤 전체 Rebuild로 복구했습니다.
  전체 Rebuild와 후속 증분 빌드가 통과했으며 진단을 숨기거나 최적화를 끄지 않았습니다.

## 남겨 둔 기존 상태와 검증 한계

기존 에디터 로컬 변경 27개를 보존합니다. 번역 INI는 기존 파일 바이트를 그대로
앞부분에 유지하고 새 PlayOptions/Records 절만 추가하며, 커밋에는 새 절만 포함합니다.
실제 Songs 폴더, 원본 채보·음원·이미지, Option.ini, converter 소스는 변경하지 않습니다.

Client 분석에 나온 기존 엔진 진단은 ScreenVisual2DManager.cpp:225 C26478,
Visual2DSurface.cpp:260 C26498, :437 C6011입니다. 엔진 SDK 테스트의 기존 진단
CollisionTests.cpp:164 C26800, WindowControlTests.cpp:34 C6011도 별도로 남아 있습니다.
이번 어댑터와 Client 코드에는 새 컴파일러/분석 경고가 없습니다.

자동 native 실행/수치 검사와 실제 사람이 듣고 플레이한 검증은 구분합니다.
물리 출력 장치에서 배속 음질을 청취하거나 손으로 타격감을 평가하지 않았습니다.
로그는 ignored build/play-options-20261023 및 엔진의 동명 폴더에 있습니다.

## 게시 상태

엔진 main의 216d5d4(고정 PCM 변환, 분석 범위, 프로젝트 filter 동기화)를 먼저 게시한 뒤
Client의 Dependencies/PhantomCore를 같은 커밋으로 갱신했습니다. Client 변경은 기존
편집 로컬 변경과 분리하여 main에 게시합니다. 완료 응답에 원격/로컬 exact SHA를 기록합니다.
