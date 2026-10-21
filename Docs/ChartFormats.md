# 차트 문법 진입점

외부 osu!taiko 채보 가져오기는 별도 [변환기 EXE](OsuTaikoConverter.md)를 사용합니다.

YMP의 타이밍 명령은 `#measure`, `#bpm`, `#delay`만 허용합니다.
효과 명령은 YME에 작성하며 YMP의 알 수 없는 명령은 오류로 처리합니다.

형식별 문법의 원본은 아래 문서입니다. 수정할 형식만 읽고, BPM/마디 변경일 때만 Timing을 추가합니다.

| 요청 | 문서 | 파서 구현 |
| --- | --- | --- |
| 음악 이름·아티스트·음원 경로 | [YMM](Formats/Ymm.md) | MusicParser.cpp |
| 패턴 metadata·노트·연타수 | [YMP](Formats/Ymp.md) | PatternParser.cpp |
| 분수 문법·BPM·마디 길이 | [Timing](Formats/Timing.md) | PatternTiming.h / MusicalPositionParser.cpp |
| 히트사운드 표/볼륨·Whole/Separate·보간·싱코페이션/Kiai | [YME](Formats/Yme.md) | EffectParser.cpp / Automation |
| 디버그 채보·1ms 이동 | [Debugging](Gameplay/Debugging.md) | GameplayInput.cpp |

파서 경로는 `YOUMU.Chart/Parsing/Submodules`입니다.
`ChartParser.h/.cpp`는 파일 입출력 facade, `ParserSupport.h`는 공통 토큰/숫자 처리입니다.
모델도 `Model/Submodules` 아래 YMM/YMP/YME별로 나뉩니다.
세 형식 모두 UTF-8 BOM을 허용합니다. YMM/YMP의 알 수 없는 metadata 항목은
행 번호가 있는 Warning으로 반환합니다. `Parse*File`의 파일 읽기 실패도 해당 경로의
Error 진단으로 반환하며, 경로 문자열은 UTF-8을 사용합니다. 메모리 할당 등 예기치 않은
실패까지 정상 파싱 오류로 숨기지는 않습니다. 마디 상한은 [Timing](Formats/Timing.md)이 기준입니다.
저장 문법 변경 시 `Editing/ChartEditor.cpp`와 왕복 테스트를 함께 확인합니다.
YMP가 `Effect file`로 YME를 참조하고 YME가 사운드 표를 소유합니다.
YME의 효과 명령은 Speed/Sounds/Zone 세 섹션이며 음악 볼륨 명령은 지원하지 않습니다.
공통 실수 파서는 입력 필드의 실제 끝까지 확인합니다. NUL 뒤의 추가 문자나 후행 쓰레기
값은 오류이며 기존 부호·지수·16진수와 주변 공백 표기는 유지합니다. 임시 문자열 할당이
실패하면 false를 반환합니다. 파서가 받은 source 경로는 문서가 소유하고 진단도 그 경로를 사용합니다.
