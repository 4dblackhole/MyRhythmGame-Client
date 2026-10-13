# 프레임 비용·로딩 병목 수정 (2026-10-14)

이전 조사 보고서의 8개 항목을 Client·Chart·Modes와 MRG-Engine에서 수정했습니다.
입력 판정은 계속 timestamped Raw Input과 정수 us를 사용합니다. 무제한 Update,
Base BPM 속도, Whole 적분, 노트별 Separate 배율과 저장 형식은 유지합니다.

## 수정 범위와 책임

| 조사 항목 | 변경한 책임 객체 | 처리 방식 |
| --- | --- | --- |
| 1. 느린 노트가 전체 표시량 확대 | GameplayPresenter, RhythmTestScene | 누적 Whole 거리와 개별 Separate 배율로 객체별 가시 구간 인덱스 생성. 화면과 겹치는 노트·마디선만 생성/갱신. 틱도 같은 거리에서 범위 검색. 표시 세대 번호로 선형 수명 정리. 표시 갱신은 Render에서만 수행 |
| 2. 128개 텍스트 캐시 경계 | 엔진 TextRenderSystem | 전체 삭제를 2,048개 LRU의 항목별 축출로 교체. 변경 문구가 정적 문구 전체를 지우지 않음 |
| 3. 노트 수 × 효과 수의 진입 비용 | TaikoSessionBuilder/EffectLookup | 효과 시각을 한 번 컴파일하고 종류·시각 인덱스로 Separate/싱코페이션/마디선 배율 검색. 세션 생성 중에만 컴파일된 배열을 빌림 |
| 4. 모든 롱노트 순회 | ChartEditor/IntervalIndex | 시간·마디 구간 겹침 인덱스. 원본은 기존 트리와 안정 ID로 소유. revision 변경 뒤 첫 조회에서 읽기 전용 인덱스 재생성 |
| 5. FFT 재집계·도형 재생성 | EditorView | 128열 시간 타일별 집계/불변 geometry 캐시. 재생은 기존 타일의 transform과 ruler/cursor를 갱신하고 새로 진입한 타일만 생성. 줌·주파수·분석·문서 변경 시 무효화 |
| 6. 전체 곡 카드·카탈로그 재파싱 | MusicSelectView/SongCatalogLoader/SongCatalog | 화면 인근 행만 카드 생성. 단일 worker에서 파일 발견과 변경된 문서만 파싱. Scene은 완료 결과만 게시하고 첫 진입은 로딩 상태 표시 |
| 7. 동기 오디오·이미지/GPU 대기 | GameplayAudioRouter/Controller/Presenter, 엔진 FMOD/TextureManager | 실제 참조한 히트사운드만 수집. SampleAsync/StreamAsync 준비 완료 뒤 clock 시작. 이미지 WIC 디코드는 worker에서 수행. GPU 업로드는 같은 queue 순서와 fence 수명 보관으로 처리하고 이미지별 대기 제거 |
| 8. 중복 오디오 유지관리 | GameplayAudioRouter/SongPreviewController, 엔진 SceneGameClient | 성공한 동일 자동화 값의 setter 생략. 종료 Voice는 공통 관리자 조회 결과 재사용. native 종료 확인은 2ms 유지관리 주기로 분리 |

IntervalIndex는 불변 조회 projection입니다. 편집 원본 노트·BPM·delay·효과 트리를
vector로 바꾸지 않았습니다. 모델 pointer는 PlaySession보다 먼저 Presenter에서 해제하며,
worker는 Scene/엔진 observer를 참조하지 않습니다. 업로드 command list/allocator/buffer와
destination texture는 fence 완료까지 엔진이 함께 소유합니다.

## 성능 비교

이전과 같은 Release probe, 하드웨어와 입력 조건으로 두 번 실행했습니다.
비교 대상은 해당 작업의 CPU 시간이며 전체 게임 FPS/GPU frame time은 아닙니다.

| 조건/측정 | 이전 | 수정 후 |
| --- | ---: | ---: |
| 5,000노트, 마지막만 Separate 0.001: Canvas 노드 수 | 15,086 | 137 |
| 같은 조건: 표시 갱신 중앙값 | 2.86~3.35ms | 약 0.002ms |
| 10,000노트, 마지막만 Separate 0.001: Canvas 노드 수 | 30,086 | 137 |
| 같은 조건: 최초 표시 갱신 / 반복 중앙값 | 39.4~44.9ms / 9.29~10.68ms | 0.068~0.072ms / 약 0.002ms |
| 5,000노트·1,000효과: 세션 생성 | 1,482~1,589ms | 12.845~12.938ms |
| 10,000노트·1,000효과: 세션 생성 | 2,987~3,258ms | 24.027~25.324ms |
| 10,000 롱노트 쌍, 2끝점 반환: 캐시 구축 후 구간 조회 중앙값 | 10.51~10.69ms | 0.0011ms |
| 고정 텍스트 129개: Submit 평균 / p95 | 3.490ms / 3.952ms | 0.259~0.291ms / 약 0.350ms |
| 고정 텍스트 200개: Submit 평균 / p95 | 5.161ms / 6.020ms | 0.439~0.453ms / 0.499~0.580ms |
| 개발 카탈로그 6곡·정상 6패턴: warm Load 중앙값 | 6.14~6.26ms | 2.309~2.425ms |

표시 probe는 실제 생산 Presenter 객체를 사용하되 GPU 없는 PNG 메타데이터 renderer를
사용합니다. 텍스트 probe는 실제 숨김 D3D12/NoSound에서 120프레임 중 20프레임을 제외하고
Submit만 측정합니다. 편집 조회는 기존과 같이 3회 워밍업 뒤 100회를 측정합니다.
카탈로그는 새 객체를 매번 만들던 이전 경로와 유지된 파서 캐시를 사용하는 수정 경로를
같은 파일 집합에서 비교했습니다. 실제 UI에서는 worker가 이 작업을 수행합니다.

FFT 타일/오디오 setter/비동기 자원은 회귀와 실제 재생으로 동작을 확인했으며 개별 ms
개선율은 측정하지 않았습니다. 편집 후 첫 구간 조회의 projection 재생성, 세션 생성의
선형 노트 구성과 첫 스킨 GPU 등록 비용은 남아 있습니다.
원본 probe/출력과 빌드·분석·회귀 로그는 ignored `build/performance-fixes-2026-10-14`에 있습니다.

## 검증 범위와 한계

자동 검증 명령·결과는 [Verification](../Verification.md)에 기록합니다.
캐시와 범위 조회 회귀는 경계, 수정 후 무효화, 뒤로 이동, resize, 느린 개별 노트 유지와
FFT geometry 재사용을 검사합니다. 실제 엔진 replay는 판정·틱·오디오·결과를 검사합니다.

대형 합성 채보는 테스트 프로세스 메모리에서만 생성했습니다. 사용자 곡/스킨/Option.ini는
수정하지 않았습니다. 실제 화면 배치·물리 입력·청음 및 전체 프레임 p95/p99는 수동 측정하지
않았습니다. FFT geometry 캐시는 GPU primitive 자체를 제거하는 기능이 아닙니다.
초기 다른 화면의 직접 LoadImage 호출은 기존 동기 디코드 API를 유지하며, gameplay가
PrepareImages를 사용합니다. DirectWrite 최초 글리프 생성과 실제 장치/파일 I/O 비용도 남습니다.
