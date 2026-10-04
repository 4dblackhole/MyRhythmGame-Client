# 플레이 디버그와 테스트

## 롱노트 확인용 채보와 디버그 실행

실행 파일 옆 `assets/Songs/Pattern/angeldream/angeldream [long notes test].ymp`는
엔젤드림 핸드셰이크 음원을 참조합니다. Roll, BigRoll, TickRoll, BigTickRoll,
Balloon, DengDeng, Don Buzz, Kat Buzz의 시작점을 2박 간격으로 배치했습니다.
사용자 추가 곡·YMM·YMP·YME는 Git 제외 대상이므로 이 테스트 파일은 작업 PC에만 남습니다.
예외로 `FingerDrum.Assets/Assets/Songs`의 기본 AngelDream 음원·YMM·YMP 4개는 Git에서 추적합니다. 파일이
없는 clean clone에서는 디버그 실행이 같은 배열의 내장 패턴을 사용합니다.

```powershell
MyRhythmGame.exe --rhythm-debug
```

이 모드는 테스트 채보를 바로 열고 -2초에서 일시정지합니다.

- `1` / `2`: 설정 속도로 타이머를 뒤/앞으로 연속 이동
- `3` / `4`: 정확히 -1ms / +1ms 이동
- `-` / `+`: 연속 이동 속도 조절(200~4000ms/s)
- `Space`: 자동 진행/일시정지
- `R`: -2초로 초기화
- `D`, `K`: Kat, `F`, `J`: Don
- `Escape`: 곡 선택으로 복귀

Debug 빌드의 `Client/GameScene/RhythmTestScene/Submodules/GameplaySupport.h`에 있는 `ReferenceTimeDebug`를 켜면 위 조작을
사용합니다. 화면 좌측 상단 DEBUG 텍스트는 노트 객체가 반환하는 ID·상태·시작·
종료·진행도와 정확도 상세를 표시합니다. 뒤로 이동하면 이미
소비한 판정 상태와 점수를 초기화한 뒤 해당 시각까지 다시 갱신합니다.

Debug 빌드에서는 실행 인자와 무관하게 좌측 상단에 현재 포커스 노트의 ID,
상태, timing/expire(us), 개별 정확도와 마지막 확정 노트의 상세를 표시합니다.
큰 노트/보라 노트는 Hit1, Hit2, Final, Buzz는 시작 입력·유지 틱 비율·Final,
연타는 실제/목표 횟수·Final로 표시하여 최종값에 가려진 부분 항목도 확인할 수 있습니다.

상세 YMM, YMP와 롱노트 옵션은 [차트 문법](../ChartFormats.md)을 참고합니다.

순수 로직 회귀 테스트는 `FingerDrum.Rhythm.Tests.exe`이며 판정 scaling,
보간 점수, Lane focus, 큰 노트 사운드, tick 중복 방지, legacy YMP와 YME를
검증합니다. `--catalog-root <Songs 경로>`를 붙이면 번들된 AngelDream YMM/YMP와
선택적 로컬 곡·패턴의 연관,
음악 파일 존재 여부와 모든 패턴의 단일-Lane 세션 생성까지 검증합니다.

## 전체 노트와 짧은 Buzz 틱 재현

`FingerDrum.Assets/Assets/Songs/Pattern/angeldream/angeldream [all notes test].ymp`는
기존 AngelDream Handshaking 음원을 참조하는 180 BPM, JudgeLevel 50 테스트 채보입니다.
빌드 시 실행 파일 옆 Songs에 파일이 없을 때 복사되며 기존 사용자 파일은 덮어쓰지 않습니다.
곡 선택에서 AngelDream의 `All Notes Verification` 패턴을 선택해 플레이합니다.

앞의 빈 두 마디 이후 Don, Kat, BigDon, BigKat, Purple, Roll, BigRoll,
TickRoll, BigTickRoll, Balloon, DengDeng, Don Buzz, Kat Buzz 순서로 등장합니다.
연타/풍선/뎅뎅은 목표 4회, TickRoll은 분할 16이며 모든 롱노트 길이는 반 마디입니다.
뒤의 여섯 마디는 Don/Kat Buzz를 각각 TickDivision 64, 256, 1024로 배치했습니다.

밀집 Buzz 헤드를 +50ms(GOOD 이내)에 누르고 끝까지 유지하면 이미 지난 틱은
실패로 확정되고 이후 틱만 성공합니다. 분할 64/256/1024의 결과는 각각
실패 2/9/38개와 성공 29/118/473개입니다. 헤드는 따로 판정하므로
전체 몸통 틱은 31/127/511개이며 헤드와 유지 비율을 50%씩 합산합니다.
실패한 틱에는 히트사운드가 나오지 않으며 이후 입력으로 소급 성공하지 않습니다.

`BuzzReplayTests.cpp`는 GOOD 경계와 틱 시각 ±1µs, Don/Kat, 프레임 분할을
63개 조건으로 검사합니다. `--catalog-root`는 실제 위 YMP의 19개 논리 노트를
플레이 세션에 입력하여 완료/정확도/틱 순서와 시각을 검사합니다.
[별도 실제 엔진 replay](../../Tests/Presentation/README.md)는 같은 입력을 QPC 시간에
맞춰 기존 GameplayPresenter/GameplayAudioRouter로 D3D12/FMOD에서 실행합니다.
자동 입력 검사이며 실제 키보드 Raw Input, 픽셀 배치와 청음 수동 확인을 대체하지 않습니다.
