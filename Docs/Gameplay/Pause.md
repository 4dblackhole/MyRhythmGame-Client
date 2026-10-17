# 플레이 일시정지와 되감기

ESC는 해당 Raw Input QPC 시각에 타이머·오디오를 멈추고 일시정지 메뉴를 엽니다.
그 프레임에서 ESC보다 늦은 게임 입력은 판정하지 않습니다. 메뉴에서는 ↑↓로 선택하고
←→로 값을 조절하며 Enter 또는 버튼 클릭으로 실행합니다.

| 선택 | 동작 |
| --- | --- |
| Resume | 기본 0.5초 이전부터 재생. 0.5초 단위로 0.5~3초를 선택. 이미 처리한 노트와 점수를 유지 |
| Rewind | 기본 4마디 이전 마디의 처음으로 이동. 1마디 단위 조절. 이동 시각 **이상**의 판정·점수를 취소하고 다시 판정 |
| Retry | 로드한 채보/Clip을 재사용하여 처음부터 새 시도. 노트·점수·입력/제어 기록·클리어/HP 자격 초기화 |
| Retire | 남은 논리 노트를 MISS로 확정하고 Result로 이동. 진행 중 롱노트는 부분 타격이 있어도 0점 |

Rewind는 현재 BPM으로 시간을 빼지 않고 컴파일된 원본 마디 시작 시각을 사용합니다.
10번째 마디의 3/4에서 4마디를 되감으면 6번째 마디의 0/1로 이동합니다.
첫 마디보다 앞서는 마디 수는 첫 마디로 제한하고, 이미 첫 마디 이전이면 시작 대기 시각으로
돌아갑니다. Resume으로 그보다 더 이전에 있다면 현재 위치를 유지하여 앞으로 이동하지 않습니다.
롱노트 도중으로 되감아도 대상 시각 이전의 헤드·타격·틱은 보존합니다.
그 이전에 확정된 노트의 점수와 Retire 이전에 확정된 부분 점수는 변경하지 않습니다.

모든 노트 처리 후 3초 자동 Result 전환은 유지합니다. 일시정지 중에는 대기 시간이 흐르지 않습니다.
화면/오디오 준비 중에도 ESC·Retry·Retire는 가능하며 Resume/Rewind는 준비 완료 후 활성화됩니다.
기존 Space 정지/재개와 R 초기화는 유지하며 메뉴를 열지 않는 Space 정지는 아래 메뉴 사용 플래그를
설정하지 않습니다. 디버그 시간 조작은 [Debugging](Debugging.md)을 참고합니다.

## 일시정지 화면

Penpot `Gameplay · Sky`의 `인게임 일시정지 · Pause Controls · 1920×1080` 보드를 따릅니다.
화면 중앙에 네 액션만 배치하며, 1080p 기준 전체 720×336px, 각 행 720×72px,
행 간격 16px, 모서리 반경 20px입니다. 선택 행은 Result와 같은 하늘색/흰색 글자,
다른 행은 밝은 바탕/하늘색 테두리/짙은 글자이며 배경은 #17344E의 38% 덮개입니다.
Noto Sans KR 700을 내장 폰트에서 공유하고 제목/설명/별도 큰 패널은 표시하지 않습니다.

Resume/Rewind 오른쪽에는 앞·뒤 화살표와 현재 값이 있는 선택기를 둡니다.
화살표 클릭은 키보드 ←→와 같은 `AdjustValue`를 사용하여 값만 바꾸고 게임을 재개하지 않습니다.
왼쪽 액션 영역 클릭이나 Enter가 동작을 실행합니다. 준비 중에는 두 액션과 선택기를 비활성화합니다.
좁은 화면에서는 메뉴 부모를 함께 축소하여 버튼·선택기·텍스트가 화면 안에 남습니다.

`GameplayPauseLayout`은 Canvas 노드와 배치를, `GameplayPauseMenu`는 포커스·값·입력 의도를
관리합니다. 세션·오디오 변경은 기존 Controller에 남습니다. 기본 모양과 화살표는 기존
Sprite/Transform으로 구성하고 기존 `skin.ini`의 PausePanel/PauseResume/Rewind/Retry/Retire
매핑을 유지합니다. 반투명 배경은 `InGame/Pause/Backdrop.png`의 1×1 RGBA 이미지를
화면 전체로 확장하며 `PauseBackdrop` 매핑으로 교체합니다. 화면용 단색 Panel의 불투명
경로 대신 기존 이미지 Sprite의 알파 합성을 사용합니다.
선택기 문구는 언어 INI의 `Pause.resumeValueFormat`/`rewindValueFormat`이며
누락 시 내장 영어를 사용합니다. 개발 폴더의 기존 편집 언어파일은 빌드로 덮어쓰지 않습니다.

## 모든 모드의 실행 상태

`Client/GameFlow/IPlaySessionController.h`는 Loading / Gameplay / Pause / Ending 네 상태와
조회·일시정지·재개·종료·결과 이동 계약입니다. Scene은 이 인터페이스를 소유합니다.
공통 `GameplaySessionController`와 `GameFlow/Submodules/GameplayState`가 전환을 실행하며,
Taiko의 노트 규칙이나 결과 형식에 상태 전환을 구현하지 않습니다. `IPlayGameMode`는
채보→PlaySession 생성, `IPlayResult`는 모드별 결과를 맡고 실행 상태 계약과 분리합니다.
새 모드는 같은 컨트롤러에 자신의 세션을 공급하고 화면·기본 사운드 연결만 확장합니다.

| 상태 | 반복 작업 / 전환 |
| --- | --- |
| Loading | Scene의 PNG 준비 후 오디오 Clip과 음악 seek 완료를 확인. 준비 후 Gameplay. Retry도 이 경로를 사용하되 로드한 자산은 재사용 |
| Gameplay | QPC 타이머·timestamp 입력·노트·YME 갱신. ESC→Pause, 전체 처리 후 3초→Ending |
| Pause | 게임 시각 고정, 메뉴 명령 접수, 필요 범위의 판정/점수 복원. 음악 탐색·시작 예약까지 완료한 뒤 Gameplay |
| Ending | 입력/판정·오디오 중단, 모드 결과를 한 번 확정. Result에 소유권을 이동한 뒤 Scene Shutdown에서 자원 정리 |

Resume/Rewind 버튼은 명령을 한 번 접수하면 메뉴를 닫습니다. 음악 준비 중에도 상태는
Pause이고 시각·노트 판정은 진행하지 않습니다. 중복 Resume은 이동량을 다시 빼지 않습니다.
준비 실패는 멈춘 시각을 유지하고 오류와 메뉴를 표시합니다. 준비 중 ESC도 메뉴로 돌아갑니다.
음악 Voice는 Pause에서 유지하고 히트사운드·틱만 정지합니다. Resume/Rewind/정상 Retry는
그 음악 채널을 재정렬하며, 자연 종료된 경우에만 새 Voice를 만듭니다. 엔진의 PreparationState가
Ready일 때 새 QPC/DSP 기준으로 시작합니다. 파일 로딩 실패도 플레이 진입을 막으며
실패 후 Retry는 파일을 재로드합니다. Resume/Rewind로 로딩 실패를 우회할 수 없습니다.
메뉴가 표시되면 해당 시도의 practice/HP 제한과 Pause 마커가 적용됩니다.
최종 결과는 Ending 진입 시 한 번만 생성하며 `TakeResult`는 이후 한 번만 소유권을 전달합니다.

## 책임과 복원 방식

- RhythmTestScene은 메뉴·Controller·Presenter를 조립하고 결과 전환을 요청합니다.
- GameplayPauseMenu는 별도 앞쪽 Canvas의 표시·포커스·값 조절을 소유하고 선택 값을 반환합니다.
  한국어/영어 문구는 Texts/GameScene/RhythmTestScene/PauseTexts가 소유합니다.
- 공통 GameFlow/GameplaySessionController의 GameplayPause 구현은 타이머 이동·오디오 재예약·세션 명령을 담당합니다.
- PlaySession의 PlaySessionSeeking 구현은 판정 상태 복원과 강제 종료를 담당합니다.
  입력·상태 변화가 있는 Update·held 상태 변화만 처리 이력에 남기며 매 프레임 로그를 쌓지 않습니다.
  Rewind 시 대상 이전 처리만 원래 실행 순서대로 규칙에 다시 전달합니다. 이전 입력 로그와 음성 재생을
  중복 생성하지 않습니다. 타이머를 되돌린 Resume 때문에 이력을 시간순 정렬로 가정하지 않습니다.
- TaikoResultCollector의 ResetJudgements는 통계만 초기화합니다. 입력/제어 기록은 유지하고
  현재 가지의 판정·오차·점수만 복원합니다. Reset은 새 시도를 시작하므로 모든 기록을 지웁니다.
  Retire는 실제 관측한 타격/틱을 남기되 미완료 논리 노트당 MISS 하나와 0점을 기록합니다.
  가상의 타이밍 오차·실패 틱을 생성하지 않습니다.

복원 비용은 보존한 처리 이력에 비례하며 일시정지 선택 시에만 발생합니다.
재생 시간의 각 프레임에서 채보 재로딩이나 BPM 적분을 하지 않습니다.
음악의 실제 소스 위치와 음수 대기 예약, 자동화 초기화는 [Audio](Audio.md)를 참고합니다.

## 향후 HP·리플레이·기록 구현 시 필수 계약

**HP와 리플레이 재생/디스크 저장은 아직 구현하지 않았습니다.** 다음 계약을 연결해야 합니다.

- 메뉴가 한 번이라도 나타나면 해당 시도의 `pauseMenuUsed`는 계속 true입니다.
  마지막까지 처리해 Completed로 끝나도 `PlayResultSummary::IsCleared()`는 false입니다.
  클리어/기록 저장은 종료 사유만으로 판단하지 말고 이 함수를 사용합니다.
- 실시간 HP 시스템은 `PlaySession::CanUseHp()`, 결과/기록은 `PlayResultSummary::CanUseHp()`를
  사용합니다. Resume/Rewind로 자격을 복구하지 않으며 Retry만 새 시도로 초기화합니다.
- `PlayControlEvent`는 Pause/Resume/Rewind/Retire 종류, 부호 있는 정수 us의 출발/도착 시각,
  메뉴 표시 여부와 입력 목록의 경계 `inputIndex`를 기록합니다.
  제어 목록은 실행 순서를 보존하며 같은 us의 입력·제어 순서는 이 경계로 구분합니다.
- Rewind로 취소한 입력도 `summary.inputs`에 남습니다. 제어 마커를 무시한 전체 입력 재판정으로
  최종 통계를 재구성하면 안 됩니다. 결과 통계는 현재 가지, 원본 로그는 시도 전체입니다.
- 리플레이에는 Pause의 위치와 Resume/Rewind의 출발·도착 위치를 표시해야 합니다.
  정지 시간이나 되감기 애니메이션을 재현할 필요는 없습니다. 현재 마커는 실제 정지 지속시간을
  저장하지 않으며 UI 메뉴 조작은 노트 입력으로 기록하지 않습니다.

검증 명령과 실제 수동 확인 구분은 [Verification](../Verification.md)에 기록합니다.
