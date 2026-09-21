# 차트 문법 진입점

형식별 문법의 원본은 아래 문서입니다. 수정할 형식만 읽고, BPM/마디 변경일 때만 Timing을 추가합니다.

| 요청 | 문서 | 파서 구현 |
| --- | --- | --- |
| 음악 이름·아티스트·음원 경로 | [YMM](Formats/Ymm.md) | MusicParser.cpp |
| 패턴 metadata·노트·연타수 | [YMP](Formats/Ymp.md) | PatternParser.cpp |
| 분수 문법·BPM·마디 길이 | [Timing](Formats/Timing.md) | PatternTiming.h / MusicalPositionParser.cpp |
| 볼륨·마디선·속도·히트사운드 변경 | [YME](Formats/Yme.md) | EffectParser.cpp |
| 디버그 채보·1ms 이동 | [Debugging](Gameplay/Debugging.md) | GameplayInput.cpp |

파서 경로는 `FingerDrum.Chart/Parsing/Submodules`입니다.
`ChartParser.h/.cpp`는 파일 입출력 facade, `ParserSupport.h`는 공통 토큰/숫자 처리입니다.
모델도 `Model/Submodules` 아래 YMM/YMP/YME별로 나뉩니다.
저장 문법 변경 시 `Editing/ChartEditor.cpp`와 왕복 테스트를 함께 확인합니다.
