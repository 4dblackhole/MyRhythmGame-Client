# 첫 입력음·곡 목록·패턴 로딩 (2026-10-23)

첫 플레이 화면이 표시된 뒤 히트사운드 PCM 로딩과 음악 Voice 준비를 시작하는
순서가 있었습니다. Loading 중 입력은 의도적으로 판정에서 제외되므로 화면을 보고
누른 초기 입력의 소리가 빠질 수 있었습니다. RhythmTestScene은 세션을 받은 다음
오디오와 이미지 준비를 함께 시작하고, Controller가 실제 Gameplay에 진입한 뒤
플레이 Canvas를 표시합니다. 실패 시에는 기존 오류·Pause·Retry 경로를 사용합니다.
RhythmTimer와 Raw Input timestamp, 음악의 QPC/DSP 시작 여유는 유지합니다.

## 책임과 스레드

| 소유자 | 역할 |
| --- | --- |
| `SongCatalogLoader` | KeepAlive Scene의 단일 목록 worker. 최신 루트만 게시하고 경로와 데이터만 worker에 전달 |
| `SongCatalog::MetadataIndex` | 변경한 원본을 전체 파싱·검증하고 목록용 메타데이터, 노트 수와 진단만 보관. 기본 `SongCatalog::Load`는 전체 문서 API 유지 |
| `SongCatalogIndex.cpp` | 버전·루트·체크섬을 검증하는 재생성 가능한 디스크 캐시. 오류/쓰기 불가 시 원본 로딩 계속 |
| `PlaySessionLoader` | 선택 패턴/YME 읽기·검증·세션 구성. 작업이 끝난 future만 Poll로 이동 |
| `MusicSelectScene` | GO의 세션 로딩을 worker에 요청. 선택 변경·루트 변경·Scene 이탈 시 이전 진입 결과를 폐기. 성공한 세션만 store에 인계 |
| `GameplaySessionController` | 검증 세션을 재사용하고, Result Retry/직접 진입의 파일 로딩도 비동기로 처리. 자원 준비 후 타이머 시작 |
| `RhythmTestScene` | 오디오/이미지 준비를 병행하고 준비 전 플레이 화면 노출 방지. GPU 등록·Canvas 구성은 Scene 스레드 |

worker는 Scene/엔진 observer를 사용하지 않습니다. Scene 이탈은 작업 결과 게시를
취소하며, 진행 중 순수 데이터 작업은 완료됩니다. future 정리는 종료 시 완료를 기다릴
수 있습니다. 메인 Update는 파일 읽기 완료를 기다리지 않습니다.

디렉터리 검색에서 얻은 `directory_entry`의 파일 크기/수정 시각을 재사용합니다.
같은 YME를 여러 패턴이 참조할 때에도 발견한 파일 상태를 다시 조회하지 않습니다.
캐시 파일을 읽은 뒤에도 추가/삭제 파일과 누락된 음원·YME를 확인합니다.
카탈로그/선택 정렬은 비교마다 제목 문자열을 변환하지 않고 정렬 키를 한 번 만듭니다.

## 포맷·문법 변경 시 필요한 작업

런타임 캐시는 `%LOCALAPPDATA%/YOUMU/SongCatalog/<곡 루트 해시>.bin`입니다.
YMM/YMP/YME 원본과 별도로 저장하며 삭제해도 원본에서 재생성합니다.

`YOUMU.Chart/Catalog/SongCatalogCacheVersion.h`의 두 버전을 구분합니다.

- `SongCatalogParserRevision`: YMM/YMP/YME 문법, 기본값, 경고/오류 조건,
  노트 수 계산 또는 목록 메타데이터 해석을 바꾸면 올립니다. 허용 문법을 추가하는
  경우에도 이전 실패 진단이 캐시에 있을 수 있으므로 올려야 합니다.
- `SongCatalogCacheSchema`: 캐시의 필드·바이너리 배치를 바꾸면 올립니다.

일치하지 않는 캐시는 migration 없이 버리고 최신 파서로 재구축합니다. 이 값은
원본의 `Version:`을 바꾸지 않습니다. 원본 파일의 수정 시각/크기가 바뀌면 해당
문서만 다시 파싱합니다. YMP가 참조하는 YMM/YME도 각각 확인하며 실제 플레이
직전에는 캐시의 노트 대신 현재 원본으로 세션을 검증합니다.

수정 시각과 크기를 모두 강제로 유지한 외부 수정은 다음 목록 갱신에서도 감지하지
못할 수 있습니다. 선택 패턴의 실제 플레이 검증은 원본을 읽습니다. 포맷 변경 작업은
파서 revision 변경과 캐시 재구축 회귀를 함께 검증해야 합니다.

## osu! 공식 구현에서 참고한 점

stable의 [공식 DB 문서](https://github.com/ppy/osu/wiki/Legacy-database-file-structure#osudb)는
`osu!.db`를 설치된 채보 정보의 캐시로 설명합니다. 따라서 매 실행마다 모든 노트 본문을
목록에 다시 만드는 방식과는 비용 구조가 다릅니다. stable의 비공개 전체 구현이나
특정 라이브러리 크기에서의 실제 시간을 추정하지 않았습니다.

lazer의 [BeatmapInfo](https://github.com/ppy/osu/blob/master/osu.Game/Beatmaps/BeatmapInfo.cs)는
곡 선택에서 필터링할 메타데이터를 Realm 모델로 저장합니다.
[WorkingBeatmap](https://github.com/ppy/osu/blob/master/osu.Game/Beatmaps/WorkingBeatmap.cs)은
실제 채보의 비동기 로딩과 지연된 자원 접근을 제공하고,
[BeatmapManager](https://github.com/ppy/osu/blob/master/osu.Game/Beatmaps/BeatmapManager.cs)는
DB 조회와 working beatmap 캐시를 연결합니다. YOUMU에는 목록 메타데이터와 선택한
실제 채보 로딩의 분리, 파생 캐시 재사용을 적용했습니다. 외부 DB 의존성은 추가하지 않았습니다.

## 측정과 검증

Release 합성 카탈로그 2,000곡·10,000패턴, 패턴당 128노트로 측정했습니다.
테스트 데이터는 ignored build 폴더에만 생성했으며 사용자 Songs에 추가하지 않았습니다.

| 측정 | 시간 / 보관량 |
| --- | --- |
| 기존 코드, 파일 캐시가 준비된 전체 파싱 | 3,078~3,154ms |
| 개선 코드, 디스크 인덱스 최초 재구축 | 3,172ms |
| 개선 코드, 새 카탈로그 객체가 디스크 인덱스 읽기 | 343~373ms |
| 전체 문서 목록의 노트 보관 수 → 메타데이터 목록 | 1,280,000 → 0행 (노트 수 필드는 보존) |
| 디스크 인덱스 크기 | 2,416,078바이트 |
| 실제 목록 worker 최초 로딩 / Refresh 호출 / Poll 최대 | 2,678ms / 0.192ms / 0.0503ms |

같은 기기의 탐색/파싱 CPU·파일 I/O 비교이며 OS 파일 캐시가 준비된 측정입니다.
전체 게임 시작 시간, 렌더 FPS 또는 모든 사용자 라이브러리의 보장 시간이 아닙니다.
캐시 최초 생성과 파서 revision 변경 뒤에는 전체 원본 재검증 비용이 들지만 worker에서
수행됩니다. 오디오의 전체 PCM 디코딩, 최초 GPU 등록/Canvas 구성 비용은 남아 있습니다.

기존 RhythmTestScene으로 200ms 샘플 지연을 주입한 테스트는 첫 화면의 준비 상태
검사에서 실패했습니다. 수정 코드의 별도 새 프로세스는 첫 표시 직후 Don·Kat 모두
재생 요청을 생성하며 Debug/Release에서 통과했습니다. 실제 FMOD/WASAPI의 준비 완료
직후 24회 Don/Kat 요청도 모두 mixer 출력을 확인했습니다. 이는 물리 키보드부터 귀까지의
지연 측정이나 수동 청음은 아닙니다.

캐시 회귀는 UTF-8 경로, 새 객체에서의 재사용, 동일 크기 원본 수정, YME 오류 전파,
파일 삭제, parser/schema 불일치, 절단/체크섬 손상, 사용할 수 없는 캐시 경로를 검사합니다.
빌드·분석·smoke 및 기존 검증 결과는 [Verification](../Verification.md), 원시 측정과
임시 probe는 ignored `build/loading-20261023`에 있습니다.
