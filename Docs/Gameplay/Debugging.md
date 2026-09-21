# 플레이 디버그와 테스트

## 롱노트 확인용 채보와 디버그 실행

실행 파일 옆 `assets/Songs/Pattern/angeldream/angeldream [long notes test].ymp`는
엔젤드림 핸드셰이크 음원을 참조합니다. Roll, BigRoll, TickRoll, BigTickRoll,
Balloon, DengDeng, Don Buzz, Kat Buzz의 시작점을 2박 간격으로 배치했습니다.
사용자 추가 곡·YMM·YMP·YME는 Git 제외 대상이므로 이 테스트 파일은 작업 PC에만 남습니다.
예외로 `FingerDrum.Assets/Assets/Songs`의 기본 AngelDream 음원·YMM·YMP 3개는 Git에서 추적합니다. 파일이
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
