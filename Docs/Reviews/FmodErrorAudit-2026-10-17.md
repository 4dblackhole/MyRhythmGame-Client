# FMOD 오류 가능성 조사 — 2026-10-17

후속 구현은 [FMOD 수정 기록](FmodErrorFixes-2026-10-17.md)을 참고합니다.
아래 내용은 수정 전 조사 기준이며 재현 결과를 보존합니다.

조사 기준은 Client `c3efdec2bf067defa729b2b7f4dcb3a41fa264a4`, 엔진
`e2751ac33c28cf1be4adc7131b5554861058a33f`입니다.
설치 SDK와 Debug/Release DLL은 FMOD 2.02.22입니다.
이번 작업은 조사이며 제품 코드·오디오 API·설정·자산을 수정하지 않았습니다.

## 결과

Client에서 도달하는 오류 세 가지와 엔진 일반 재생 API의 오류 한 가지를 실제 FMOD로
재현했습니다. 현재 음악 재개 경로와 엔진 일반 API의 문제를 구분해야 합니다.

| 우선순위 | 위치 | 재현한 조건과 결과 |
| --- | --- | --- |
| P1 | EditorPlayback의 히트사운드 재사용 | 샘플 자연 종료 후 UI 갱신 지연 → INVALID_HANDLE(30) → 에디터 전체 재생 정지 |
| P1 | GameplayAudioRouter의 비동기 샘플 준비 | 로딩 실패에도 Ready=true → 해당 소리 재생 요청이 NOTREADY(46)로 실패 |
| P2 | 엔진의 us→PCM 프레임 변환 | 유효한 음원 끝 직전 시각이 끝 프레임으로 반올림 → INVALID_POSITION(32) |
| P2 | 엔진 AudioClip::Play의 비동기 스트림 재사용 | Play 내부 탐색 직후 양수 시작 위치를 설정 → NOTREADY(46); 현재 Client 음악 재개는 이 경로를 회피 |

P1은 정상 편집/플레이 흐름의 재생 지속에 영향을 주는 오류, P2는 좁은 경계 또는
현재 음악 호출부가 회피 중인 엔진 API 문제로 분류했습니다.

## 1. 에디터는 끝난 히트사운드 Voice를 다시 사용할 수 있음

- 소유자: `Client/EditorScene/Submodules/EditorPlayback.cpp:216`의 ScheduleHitSounds.
- 직접 호출부: 같은 파일 Update의 270~271행.
- 관련 계약: 엔진 `Engine/Audio/System/AudioPlaybackManager.cpp:55`의 FindVoice와
  68행의 Restart, `Engine/Audio/Backend/Fmod/FmodAudioClip.cpp:88`의 native Restart.

ScheduleHitSounds는 FindVoice가 포인터를 반환하면 Restart합니다. FindVoice는 관리자의
map에 항목이 남아 있는지만 확인합니다. FMOD 채널의 실제 재생 상태는 확인하지 않습니다.
게다가 끝난 Voice를 지우는 voices_.Update는 새 소리를 예약한 **뒤**에 실행됩니다.

따라서 직전 에디터 갱신 후 샘플이 자연 종료되고, 다음 갱신이 다음 타격의 예약 범위에
들어오면 끝난 native 채널에 setPaused/setPosition을 요청합니다. FMOD의 자연 종료된
Channel은 INVALID_HANDLE을 반환하는 것이 정상적인 계약입니다.
[FMOD의 채널 수명 설명](https://qa.fmod.com/t/manage-invalid-handle-channel-correctly/20277)

실제 EditorPlayback 오브젝트를 사용해 다음을 재현했습니다.

1. 실제 Angel Dream MP3를 음악으로 사용합니다.
2. 44.1kHz, 16bit, mono, 100ms PCM WAV를 임시 히트사운드로 사용합니다.
3. 같은 소리의 marker를 0ms와 300ms에 둡니다.
4. 첫 소리 시작 후 에디터 갱신만 약 260ms 쉬고 FMOD 믹서는 계속 진행시킵니다.
5. 다음 marker가 20ms 예약 범위에 들어온 경우 Restart가 INVALID_HANDLE(30)로 실패합니다.

최종 검사에서 Debug는 3회 중 3회, Release는 3회 중 2회 오류가 재현됐습니다.
Release의 나머지 1회는 갱신 시각이 278.666ms로, 아직 다음 marker의 예약 범위 밖이어서
Voice가 먼저 정리됐습니다. 모든 지연에서 반드시 실패한다는 뜻은 아닙니다.
정상 1ms 주기 갱신의 대조군은 두 구성 모두 300ms marker를 지나 재생을 유지했습니다.

오류는 Update의 catch로 들어가 Pause를 호출하므로 히트사운드 하나의 수명 문제로
음악과 편집 타이머까지 멈춥니다. 게임플레이 Router는 이 지점에 IsPlaying 검사도 있어
동일한 단순 stale-entry 조건은 회피합니다.

개선 방향은 종료된 Voice를 새 Voice로 교체하는 것입니다. 사전 정리/IsPlaying 검사만으로는
독립 믹서가 검사 직후 종료하는 경합까지 보장하지 못하므로, 실제 Restart의 종료/탈취 오류도
구별해 재생성을 시도해야 합니다. 다른 종류의 FMOD 오류까지 무조건 무시하면 안 됩니다.

## 2. 비동기 샘플 실패와 준비 완료를 구분하지 않음

- `Client/Audio/GameplayAudioRouter.cpp:150`의 Ready.
- `Client/GameFlow/Submodules/GameplayState.cpp:4`의 PrepareResources.
- `Client/GameFlow/Submodules/GameplayAudio.cpp`의 기본/YME SampleAsync 등록.

Ready는 Loading만 false로 처리합니다. Failed는 LastError에 적지만 반환값은 true입니다.
PrepareResources는 이 bool만 resourcesReady_에 저장합니다. 음악 파일의 실패는 별도
PrepareMusicAt 경로가 Failed로 처리하지만, 히트사운드 실패는 이 검사에서 걸러지지 않습니다.

실제 존재하지 않는 WAV를 SampleAsync로 등록했을 때 두 구성에서 모두 다음 결과가 나왔습니다.

```text
RegisterSound = true
Ready = true
LastError = Missing.Sample / getOpenState / File not found (18)
PlayNow -> playSound / NOTREADY (46)
```

이는 비동기 Sound handle의 생성 성공과 파일 로딩 성공이 별개이기 때문입니다.
FMOD도 비동기 생성 후 getOpenState를 검사하고, 준비된 뒤 재생하도록 요구합니다.
[FMOD 2.02 비동기 로딩 계약](https://www.fmod.com/docs/2.02/api/loading-and-playing-sounds-in-the-core-api.html)

현재 오류가 완전히 숨겨지는 것은 아닙니다. Controller::ErrorMessage가 Router::LastError를
화면으로 전달합니다. 문제는 실패한 소리를 여전히 재생 대상으로 남기고 준비 완료로 취급해
후속 입력에서 FMOD 재생 오류가 반복될 수 있다는 것입니다.

개선 시 로딩 중/완료/실패를 분리해 Controller에 전달해야 합니다. 단순히 Failed를 false로
바꾸면 끝나지 않는 로딩 상태가 되므로 충분하지 않습니다. 실패 시 진입 중단 또는 오류 소리만
제외할지는 구현 전에 정책을 확정해야 합니다.

## 3. 음원 끝 직전의 유효한 시각도 범위 밖으로 반올림됨

- 엔진 `Engine/Audio/Backend/Fmod/FmodAudioClip.cpp:46`의 SetSourcePosition.
- 같은 파일 377행의 GetDurationMicroseconds.
- 직접 호출부: EditorPlayback::PrimeMusic/BeginPreparedPlayback,
  GameplayAudioRouter::PrepareMusicAt.

길이는 PCM 프레임 수를 us로 반올림하고, 탐색은 us를 PCM 프레임으로 반올림합니다.
마지막 프레임 반 개 정도의 시간 범위에서는 `position < duration`을 만족해도 변환 결과가
`frames`가 되어 엔진이 INVALID_POSITION(32)를 반환합니다.

100ms/44.1kHz WAV에서 다음을 두 구성 모두 재현했습니다.

| 요청 위치 | 변환/결과 |
| --- | --- |
| 99,950us | 프레임 범위 안, 성공 |
| 99,999us | 4,410으로 반올림; 유효 인덱스는 0~4,409이므로 실패(32) |

엔진 API만 호출한 결과가 아닙니다. 같은 WAV를 음악으로 사용하는 실제 EditorPlayback에서
99,999us로 Seek 후 재생했을 때도 Active=false와 setPosition 오류(32)를 확인했습니다.
현재 시간축 UI의 조작 정밀도에 따라 드물게 나타나지만 us 단위 공개 transport 경로에서
도달 가능한 경계입니다. 음원 끝과 정확히 같은 시각을 처리하는 기존 분기와도 별개입니다.

개선 시 `[0, duration)` 안의 요청을 마지막 유효 PCM 프레임 안에 제한하는 등, 길이 조회와
탐색의 경계 계약을 일치시켜야 합니다. 실제 음원 길이 밖의 잘못된 요청까지 허용할 필요는 없습니다.

## 4. 비동기 스트림의 일반 Play API에는 기존 NOTREADY 조건이 남아 있음

- 엔진 `Engine/Audio/Backend/Fmod/FmodAudioClip.cpp:271`의 Play,
  특히 296행 playSound 이후 318행의 양수 시작 위치 설정.
- 현재 Client의 회피 경로: EditorPlayback::BeginPreparedPlayback와
  GameplayAudioRouter::PrepareMusicAt.

같은 StreamAsyncAccurate MP3를 정지 후 재사용하면 playSound 자체가 0 위치로의 비동기
탐색을 시작할 수 있습니다. 이 직후 같은 Play 호출 안에서 양수 위치를 setPosition하면
NOTREADY(46)가 납니다. Play 전에 Ready를 확인해도 **Play가 시작한** 탐색은 막지 못합니다.

공개 엔진 API를 사용해 매회 Ready를 확인하고, 시작 위치 2초로 paused Play → 준비 확인 →
unpause → 100ms 재생 → Stop을 반복했습니다. Debug/Release 모두 첫 회는 성공했고
두 번째 Play가 setPosition의 NOTREADY(46)로 실패했습니다.

현재 Client 음악 재개는 다음 순서로 이 조건을 피합니다.

1. 시작 위치 0의 paused Play로 Voice 생성.
2. Clip의 Ready/Playing을 확인.
3. 같은 Voice에 목표 위치 Restart.
4. 탐색 완료 후 새 QPC/DSP 기준과 20ms 여유로 시작.

그러므로 이번 결과를 “현재 Resume/Rewind가 같은 이유로 여전히 실패한다”는 증거로
해석하면 안 됩니다. 현재 호출부의 정상 반복 회귀는 통과했습니다. 위험은 엔진의 일반
Play 설정을 다른 호출부가 그대로 사용할 때 남아 있습니다.

개선 방향은 이 비동기 제약을 엔진 계약과 회귀 테스트에 명시하고, 준비 중 Voice/탐색을
어떻게 제공할지 정하는 것입니다. 호출자마다 준비 순서를 생략할 수 없도록 해야 합니다.

추가 관찰: 이 실패 직후 남은 비동기 정리 시간을 주지 않고 AudioSystem::Shutdown을 호출한
별도 프로세스는 Debug에서 10초 안에 복귀하지 않아 종료했습니다. Release는 종료됐습니다.
동일 검사에서 약 100회의 1ms Update를 진행한 뒤 종료하면 두 구성 모두 복귀했습니다.
정확한 native 대기 원인은 스택 추적으로 확정하지 않았으며, 현재 Scene 종료에서 재현한
문제도 아닙니다. 고정 시간 sleep을 해결책으로 제안하는 것은 아닙니다.

## 코드상 남은 위험 — 하드웨어/경합 재현은 하지 않음

- **게임플레이 샘플 종료 경합:** Router::Route의 IsPlaying 이후 native Restart의
  setPaused 사이에 믹서가 채널을 끝내거나 탈취할 수 있습니다. Restart 실패 시 LastError를
  기록하고 다음 cue로 넘어가므로 해당 타격 소리가 빠질 수 있습니다. 실제 경합 발생은
  이번에 재현하지 않았습니다. 1번과 같은 종료 오류 복구 범위에서 다룰 수 있습니다.
- **출력 장치 분리/재연결:** FmodAudioBackend::Update(159행)는 System::update 오류를
  디버거 로그에만 남깁니다. DspClock(383행)은 실패 시 0을 반환하며 ClockSnapshot에는
  성공 여부가 없습니다. 장치 목록은 명시적 초기화/구성 변경 시 캐시하고 장치 변경 콜백으로
  갱신하지 않습니다. 실제 장치 변경 시 목록/선택 인덱스가 오래되거나, 시계 오류를 Client가
  정상 스냅샷과 구분하지 못할 위험입니다. 장치 상태 알림, 목록 재조회, 오류가 있는 시계의
  전달/재준비 처리가 필요합니다. 실제 WASAPI/ASIO 전환·장치 분리는 시험하지 않았습니다.
- **미리듣기 진단 손실:** SongPreviewController는 로드/재생 실패 시 failedPreviewIndex를
  저장하고 해당 포커스에서 재시도하지 않습니다. Fade의 SetVolume 실패도 재생을 정리하지만
  오류 내용은 전달하지 않습니다. 앱 예외로 전파되는 문제보다 “미리듣기가 안 나오는 이유를
  알 수 없는” 상태에 가깝습니다. 오류 전달은 preview 책임 안에서 처리하는 편이 적절합니다.

## 이미 방어하는 경계

- 옵션의 출력 API 변경 실패와 저장 실패에는 OptionsPanel::RestoreAudio가 있습니다.
  출력 전환을 무조건 복구하지 않는다고 분류하지 않았습니다.
- SampleRate/DSP buffer 변경은 AudioSystem::CanRestartMixer가 살아 있는 Clip/Voice/Bus/Effect를
  검사합니다. 이 경로를 현재 핸들의 무조건적인 use-after-free로 분류하지 않았습니다.
- native 핸들에는 엔진 lifetime/generation 검사가 있고 Scene/세션의 소리가 전역 AudioSystem보다
  먼저 정리됩니다. 재생 ID가 map에 남는 문제와 C++ 객체의 댕글링 포인터 문제는 다릅니다.
- 비동기 음악 탐색의 Loading은 현재 Controller/EditorPlayback가 기다립니다.
  정상적인 준비 대기를 곧바로 FMOD 실패라고 분류하지 않았습니다.
- YME 효과의 SetParameter/SetBypass/SetVolume 실패는 Router::LastError로 전달합니다.

## 검증과 한계

제품 오브젝트를 다시 구현하지 않고 기존 Debug/Release의 EditorPlayback.obj,
GameplayAudioRouter.obj와 MRG.Core.lib를 연결한 별도 진단 실행 파일을 사용했습니다.
FMOD NoSound, 출력 48kHz에서 실제 FMOD/코덱/믹서/비동기 로더를 실행했습니다.
이 방식은 가짜 오디오 백엔드 검사와 다르지만 실제 스피커 청취 검증은 아닙니다.
임시 WAV/진단 소스/로그는 ignored `build/fmod-audit-2026-10-17/`, 실행 파일은 ignored bin에 있습니다.

| 검사 | Debug | Release |
| --- | --- | --- |
| EditorAudioViewTests 기존 회귀 | 통과 | 통과 |
| GameplayPauseTests 기존 회귀 | 통과 | 통과 |
| 짧은 샘플 + 편집 갱신 지연 | 오류30 재현 | 오류30 재현 |
| 실패한 SampleAsync 준비 및 PlayNow | 오류18 후 준비 완료, 후속 오류46 | 동일 |
| 음원 끝 1us 전 Restart 및 EditorPlayback Seek | 오류32 재현 | 동일 |
| 재사용한 async MP3에 양수 위치 Play | 두 번째 요청 오류46 | 동일 |
| 오류46 직후 즉시 Shutdown 별도 관찰 | 10초 제한 초과 | 정상 종료 |
| Update 후 Shutdown 대조군 | 정상 종료 | 정상 종료 |

추가로 기존 실제 MP3 EditorPlayback 진단의 정지·재개 12회를 Debug에서 재실행해
오류 없이 시각이 진행하는 것을 확인했습니다. GameplayPauseTests에는 native 음악 탐색과
D3D12 렌더링 검사가 포함되지만, 키보드·마우스로 직접 재현하거나 청취한 것은 아닙니다.

이번 변경은 조사 문서뿐이므로 솔루션 재빌드·전체 로직/smoke·정적 분석을 새로 실행하지
않았습니다. 문서에 git diff --check를 적용하고 사용자 LaneKeyBeam.cpp·Option.ini·외부 언어파일의
기존 SHA-256이 그대로인지 확인했습니다. 엔진/Client 코드·사용자 곡·스킨은 수정하지 않았습니다.

수정 우선순위는 1번 Voice 종료 복구, 2번 실패/준비 상태 분리, 3번 PCM 끝 경계,
4번 엔진 비동기 Play 계약 순서입니다. 제품 코드 수정은 후속 작업입니다.
