# FMOD 재생·탐색 오류 수정 — 2026-10-17

[선행 조사](FmodErrorAudit-2026-10-17.md)의 네 가지 재현 경로를 수정했습니다.
채널을 두 개로 늘리지 않고 살아 있는 음악 Voice를 유지하며, 비동기 탐색의 완료를 확인합니다.
파일 형식·점수·Resume/Rewind의 판정 복원 규칙은 변경하지 않습니다.

## 책임과 변경

| 소유자 | 책임 / 변경 |
| --- | --- |
| 엔진 FmodAudioVoice | 초기 스트림 준비와 목표 위치 탐색을 직렬화. PreparationState로 Loading/Ready/Failed 전달. 준비 중 unpause는 완료까지 보류 |
| 엔진 AudioPlaybackManager | 같은 ID로 살아 있는 샘플 재시작, 종료/탈취된 채널만 교체. 실제 활성 채널 오류는 보존. Update에서 준비 진행 |
| 엔진 FmodAudioClip | 음원 끝 미만의 유효한 us는 마지막 PCM 프레임까지 제한. 파일 끝 이상의 요청은 채널 생성 전에 거부 |
| 엔진 AudioSystem | 최종 종료에서 retired Sound를 FMOD 시스템보다 먼저 해제. 비동기 탐색 중 즉시 종료의 대기 방지 |
| GameplayAudioRouter | 음악 Clip/Voice/Bus/Effect 유지. Pause에서 음악만 paused로 두고 transient 샘플 정리. 재개·되감기·정상 Retry에서 음악 위치를 다시 설정 |
| 공통 GameplaySessionController | 준비 완료까지 게임 시각 고정, 새 QPC/DSP 기준과 20ms 시작 여유 적용. 자산 로딩 실패 시 오류와 Pause 메뉴 표시 |
| EditorPlayback | Space/seek에서 음악 Voice 유지, 종료된 경우만 재생성. 엔진 준비 완료 후 DSP clock 예약. Scene 종료/파일 revision 변경 때 전체 정리 |

오디오 실패 정책은 사용자 확인대로 플레이 시작·재개를 막습니다. Resume/Rewind는 비활성이고
Retry/Retire는 사용 가능합니다. 실패 후 Retry는 파일을 재로드하며 성공하면 새 시도로 시작합니다.
정상 Retry는 로드한 Clip과 음악 채널을 재사용합니다. 파일이 존재해도 손상·지원하지 않는 형식·
읽기 실패 등이 있으므로 “경로 존재”와 “로딩 성공”을 구분합니다.

음악이 끝나거나 채널을 이미 잃은 경우만 새 paused Voice가 필요합니다. 음수 pre-roll에서는
소스 0을 차트 0ms의 DSP 시각까지 기다리게 하고, 음악 길이 이후의 채보는 음악 없이 진행합니다.
Ending/Shutdown에서는 paused 음악을 포함한 모든 세션 Voice를 정리합니다.
노트 규칙·입력 QPC timestamp·결과/리플레이용 제어 로그는 기존 경계를 유지합니다.

엔진은 준비 상태를 전달하고 Client는 언제 게임을 시작할지 결정합니다.
런타임 재생·탐색에는 고정 sleep이나 UI 스레드를 막는 대기, 추가 채널/스트림을 사용하지 않습니다.
직접 만든 Voice는 PreparationState를 polling하고, 매니저 사용 시 Update도 준비를 진행합니다.
준비 실패를 무한한 Loading으로 처리하지 않으며 실제 오류는 호출자에게 전달합니다.
Manager의 Restart로 종료 채널이 교체되면 빌린 Voice 포인터를 다시 조회해야 합니다.

추가 취소 진단에서는 진행 중인 MP3 탐색을 Stop/Retire하고 곧바로 시스템을 종료할 때
두 구성에서 15초 제한을 넘었습니다. 기존에는 FMOD 시스템부터 닫고 retired Clip을 버렸습니다.
이를 retired Sound release → FMOD close 순서로 변경한 후 두 구성에서 3회씩 종료됐습니다.
FMOD 내부의 정확한 대기 스택까지 확인한 것은 아니며, 정리 순서 변경의 효과를 확인했습니다.
프로그램 최종 Shutdown만 남은 파일 작업을 기다릴 수 있습니다. Scene 전환과 일반 Update의
RetireClip은 Ready/Failed polling을 유지합니다.

## 검증

최종 결과는 [Verification](../Verification.md)의 같은 날짜 수정 항목에 기록했습니다.
엔진 `7b7d34c821b4be0d273bf2c7bde57672142eb390`의 Debug/Release 전체 Rebuild·SDK 검사,
실제 MP3/PCM NoSound 진단을 통과했습니다. Client의 두 구성 솔루션·MSVC Code Analysis,
각 63개 로직/카탈로그, 네 smoke, 에디터·일시정지 회귀도 통과했습니다.
실제 출력 장치 청취·장치 분리·키보드/마우스 수동 검사는 수행하지 않았습니다.
