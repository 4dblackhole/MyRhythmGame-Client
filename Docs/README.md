# MyRhythmGame-Client 문서 안내

신규 개발자와 새 Codex 세션은 다음 순서로 읽습니다.

1. 저장소 루트의 `AGENTS.md`
2. [현재 프로젝트 인수인계 요약](SessionHandoff.md)
3. 엔진 기능 색인 `Dependencies/MRG-Engine/Docs/EngineOverview.md`
4. [실행 흐름과 객체 수명](ExecutionFlow.md)
5. 작업 대상 기능 문서

## Client 문서

| 문서 | 내용 |
| --- | --- |
| [SessionHandoff.md](SessionHandoff.md) | 새 세션용 현재 상태, 저장소 경계, 검증 및 역할 분담 요약 |
| [ExecutionFlow.md](ExecutionFlow.md) | 프로그램 진입, Scene 전환, 게임 루프, 종료 순서 |
| [FingerDrum.md](FingerDrum.md) | 로고 화면과 반응형 이미지 배치 |
| [SongSelect.md](SongSelect.md) | Penpot 곡 선택 화면의 Visual2D 구현과 조작법 |
| [RhythmGameplay.md](RhythmGameplay.md) | 타이머, 판정, Lane, Note 규칙, 차트, 모드, 사운드 계층 |
| [ChartFormats.md](ChartFormats.md) | YMM·YMP 문법, 태고 롱노트 옵션과 YME 마디선 표기 |
| [Visual2DGuide.md](Visual2DGuide.md) | Sprite·위젯 공통 노드, Canvas, 입력, PNG와 Z-Order |
| [Examples/ColoredCube/README.md](Examples/ColoredCube/README.md) | 보관된 Mesh·충돌·Visual2D 예제 |

재사용 가능한 Audio, Collision, Graphics, Text, Visual2D 계약은 엔진
submodule의 `Docs`에 있습니다. 게임 규칙, Scene ID, 차트 문법, 게임별
화면은 이 Client 저장소에 둡니다.

## 코드 위치 빠른 찾기

| 목적 | 위치 |
| --- | --- |
| 프로그램 진입과 CRT 누수 검사 | `Client/App/Main.cpp` |
| Client 설정과 Scene 등록 | `Client/App/FingerDrumGame.*` |
| 로고 화면 | `Client/GameScene/FingerDrumLogoScene.*` |
| Penpot 곡 선택 화면 | `Client/GameScene/LobbyScene.*` |
| 리듬 구조 통합 테스트 Scene | `Client/GameScene/RhythmTestScene.*` |
| 게임 재생용 오디오 라우터 | `Client/Audio/GameplayAudioRouter.*` |
| 판정·노트·Lane·스크롤 | `FingerDrum.Rhythm/` |
| YMM·YMP·YME와 음악 시간 변환 | `FingerDrum.Chart/` |
| 게임모드 계약과 임시 태고 모드 | `FingerDrum.Modes/` |
| 순수 로직 테스트 | `Tests/Rhythm/` |

문서보다 현재 public signature와 생성된 `MRG_Core.h`가 최종 기준입니다.
