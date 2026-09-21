# 플레이 오디오

파일 등록/기본 cue: `RhythmTestScene/Submodules/GameplayAudio.cpp`.
규칙별 정책: `FingerDrum.Modes/Taiko/Submodules/TaikoSoundPolicy.cpp`, 공통 ID: `TaikoSoundIds.h`.
실제 재생: `Client/Audio/GameplayAudioRouter.*`. 엔진 API 변경 때만 엔진 Audio 문서를 읽습니다.

파일 등록은 라우터의 `RegisterSound`가 SoundId와 경로를 연결하고
`AudioSystem::LoadSound`로 Clip을 로드합니다. 음악은 Stream, 히트사운드는
Sample 방식이며 전역 파일 자동 검색이나 캐시는 없습니다. 같은 Sample을 가리키는
SoundId 별칭은 하나의 활성 Voice를 공유합니다. 이전 재생이 끝나기 전에 같은
Sample을 다시 요청하면 새 Channel을 만들지 않고 기존 Channel의 위치를 처음으로
되돌려 재생하며, 끝난 뒤 요청한 경우에만 새 Voice를 만듭니다. 재생 관리자는 재생 중
Clip/Bus의 공유 소유권을 유지합니다. 오디오 장치와 FMOD system은 `mrg::Run`이
소유하는 AudioSystem 하나가 관리합니다.

## 상태 기반 히트사운드

노트 규칙은 직접 FMOD를 호출하지 않습니다. 상태 전이인 `NoteEvent`를 만들고
`INoteSoundPolicy`가 이를 `AudioCueRequest`로 변환합니다. binding은 event,
이전/다음 상태, 판정 등급, hit index, tick index를 조건으로 사용할 수 있습니다.

- 큰 노트는 Good 이상인 첫 번째 accepted hit(`hitIndex == 0`)에서만 전용
  사운드를 냅니다.
- 범위 밖 입력은 노트 전용 사운드가 아니라 `UserInputFeedback` bus의 일반
  Don/Kat 사운드를 냅니다.
- 홀드 tick은 `TickAccepted`마다 한 번만 `TickSound` bus로 전달됩니다.

이 분리 덕분에 같은 노트 규칙을 유지한 채 스킨별 파일, 볼륨, pitch, pan,
우선순위와 bus를 바꿀 수 있습니다.

## 실시간 오디오 효과

YME의 `BusVolume`, `ReverbSend`, `LowPassCutoff`, `HighPassCutoff` 자동화는
`PlaySession::EvaluateAutomation`에서 timeline 값으로 평가한 후
`GameplayAudioRouter`가 엔진 bus/effect에 적용합니다. 향후 compressor,
delay 또는 모드 전용 효과는 차트 명령과 라우터 mapping을 추가하되 노트
규칙에서는 FMOD 타입을 참조하지 않습니다.

기본 히트사운드는 수락한 입력에 맞는 Don/Kat이며 명시적 hitsound도
지원합니다. 이는 게임별 라우터를 거치며 새 오디오 채널 정책을 추가하지 않습니다.

## 상태 기반 히트사운드

노트 규칙은 직접 FMOD를 호출하지 않습니다. 상태 전이인 `NoteEvent`를 만들고
`INoteSoundPolicy`가 이를 `AudioCueRequest`로 변환합니다. binding은 event,
이전/다음 상태, 판정 등급, hit index, tick index를 조건으로 사용할 수 있습니다.

- 큰 노트는 Good 이상인 첫 번째 accepted hit(`hitIndex == 0`)에서만 전용
  사운드를 냅니다.
- 범위 밖 입력은 노트 전용 사운드가 아니라 `UserInputFeedback` bus의 일반
  Don/Kat 사운드를 냅니다.
- 홀드 tick은 `TickAccepted`마다 한 번만 `TickSound` bus로 전달됩니다.

이 분리 덕분에 같은 노트 규칙을 유지한 채 스킨별 파일, 볼륨, pitch, pan,
우선순위와 bus를 바꿀 수 있습니다.

## 실시간 오디오 효과

YME의 `BusVolume`, `ReverbSend`, `LowPassCutoff`, `HighPassCutoff` 자동화는
`PlaySession::EvaluateAutomation`에서 timeline 값으로 평가한 후
`GameplayAudioRouter`가 엔진 bus/effect에 적용합니다. 향후 compressor,
delay 또는 모드 전용 효과는 차트 명령과 라우터 mapping을 추가하되 노트
규칙에서는 FMOD 타입을 참조하지 않습니다.

## 테스트 드라이버

`Client/Presentation/LaneKeyBeam`은 Lane 자식 `Lane.KeyBeam` 스프라이트와
200ms 선형 알파 페이드를 관리합니다. Scene은 등록된 게임 키의 Pressed 입력을
판정한 뒤 `OnKeyPressed(result)`를 전달합니다. 빈 입력/범위 밖 입력 및
정확도 없는 Roll tick은 흰색, 판정 결과는 RPG `AccuracyRange::DefaultAccInfo`의
흰색·하늘색·초록·노랑·보라와 점수 기반 보간, Good 이내 거부된 입력은 적색입니다.
수동 입력 결과만 빔을 시작하므로 자동 Miss나 키 해제는 빔을 생성하지 않습니다.
재입력은 같은 스프라이트의 색상과 알파를 초기화하고 200ms 후 숨깁니다.
페이드는 표시용 deltaSeconds로 진행되며 판정 타이머를 변경하지 않습니다.
`InGame/LaneLight.png`는 Lane 폭에 맞춰 비율을 유지하고 로컬 Y=0에서 시작하며,
Lane 끝에서 clip됩니다. PNG 원본 알파에 1→0의 표시 알파를 곱합니다.
Scene 종료 전에 빔의 observer를 해제하고 Canvas가 노드를 파기합니다.

`RhythmTestScene`은 Lobby가 선택한 YMP/YME를 `TaikoMode`에 전달해 세션을
만듭니다. `DestroyOnExit`이므로 매 플레이마다 새로 생성되고 Escape 또는 패턴
완료 3초 뒤 Lobby로 돌아갈 때 Canvas, audio voice, session과 함께 삭제됩니다.

플레이 화면의 기준 시안과 런타임 PNG 자산 원본은 Penpot의
[Gameplay · Sky](https://design.penpot.app/#/workspace?team-id=3be9e5e1-190f-8090-8008-7464803e8b40&file-id=3be9e5e1-190f-8090-8008-7468d9cc1bc8&page-id=618d0170-ff55-8025-8008-80646ceaa9d9)에 있습니다.
기존 곡 선택 화면의 Sky 색상과 타이포그래피를 유지하면서 짙은 밤하늘색 Lane,
ScrollGear 프레임, 마디선, 입력 키, 키빔, 롱노트 tick과 Balloon/DengDeng 남은
횟수 표시를 한 화면 계층으로 구성합니다.

입력 패널의 네 키는 Penpot 원본 좌표를 2/3로 축소해 배치합니다. 판정원 중심은
입력 패널 오른쪽에서 Lane 높이의 절반만큼 떨어져 있어, 중심에서 입력 패널과
Lane 위·아래까지의 거리가 같습니다. Lane과 ScrollGear 표면은 오른쪽 논리 화면
끝까지 이어지고 마지막 `Lane.png` 타일만 UV로 잘립니다.

노트와 플레이 HUD의 배포 기본본은
`FingerDrum.Assets/Assets/Skins/Default Skin/InGame`의 PNG입니다. 실행 파일 옆
스킨에서 누락된 파일만 이 내장 기본본으로 대체합니다.
`note.png`, `bignote.png`, `LNBody.png`, `LNTail.png`에는 Don/Kat/Roll Ambient
색상을 곱하고 대응하는 `*Overlay.png`는 흰색 원본으로 위에 그립니다. 일반/큰
롱노트는 서로 다른 body와 tail을 사용하고 Buzz tick은 마름모 자산을 사용합니다.
tail과 tail overlay는 별도로 회전하지 않고, 둥근 면이 위를 향하는 원본 방향
그대로 Lane 부모의 회전만 상속합니다.
기본 히트사운드는 `don.wav`, `kat.wav`, `bigdon.wav`, `bigkat.wav`입니다.
음악은 Stream으로 읽어 하나의 `RhythmTimer`가 가리키는 DSP 시각 0에 예약합니다.
Raw Input으로 발생한 히트사운드는 입력 timestamp를 판정에만 사용하고 즉시
재생합니다. 따라서 QPC와 장치 DSP clock의 장시간 미세 드리프트가 실시간 입력음을
미래 시각으로 예약해 지연을 누적시키지 않습니다.
오디오 초기화·파일 등록·재생 요청이 실패하면 가능한 나머지 파일 등록은 계속하고,
게임플레이 화면 상단에 첫 오류를 표시합니다. 오류는 무음으로 삼키지 않으며 노트
판정 시간축은 오디오 오류와 독립적으로 유지됩니다.
