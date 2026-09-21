#include "EditorTexts.h"

namespace finger_drum::texts
{
    const EditorTextSet &Editor(const Language language) noexcept
    {
        static constexpr EditorTextSet Korean{
            {L"패턴", L"박자표", L"메타데이터", L"이펙트"},
            L"저장", L"저장 *",
            L"롱노트 끝 위치를 클릭하세요 · 도구 변경/Escape: 취소",
            L"전체 악보 뷰", L"실시간 뷰", L"박자 디바이더", L"직접 입력",
            L"박자 분할 (1/N)", L"도구",
            {L"선택", L"동", L"큰 동", L"롤노트", L"풍선", L"BPM", L"마디"},
            {L"동", L"캇", L"큰 동", L"큰 캇", L"보라노트", L"롤노트", L"틱롤",
             L"큰 롤노트", L"큰 틱롤", L"풍선", L"뎅뎅", L"동 버즈", L"캇 버즈"},
            L"박자표 · BPM / 마디 길이", L"마디     위치          종류            값",
            L"마디", L"지연", L"삭제", L"시작 마디", L"변경 위치 N/D",
            L"BPM 추가 / 수정", L"마디 길이 N/D", L"마디 길이 적용",
            L"넘친 노트는 초과 박자를 유지해 다음 마디로 이동합니다.\n마디 밖의 BPM/이펙트는 먼저 이동해 주세요.",
            L"메타데이터", L"YMM 상대 경로", L"패턴 이름", L"제작자 (한 줄에 한 명)",
            L"태그 (한 줄에 하나)", L"오프셋 (ms)", L"기본 BPM", L"히트사운드 테이블",
            L"히트사운드는 인덱스: 상대경로로 등록합니다. 예: 1: Sounds/pop.wav\nYMP와 같은 폴더에 같은 이름의 YME를 저장합니다. Ctrl+S: 저장",
            L"이펙트", L"시작 마디 / 박자      종류           값 / 참조 인덱스",
            {L"볼륨", L"마디선", L"노트 속도", L"스크롤 속도", L"동 사운드", L"캇 사운드"},
            L"시작 박자", L"끝 마디 (선택)", L"끝 박자 (선택)", L"히트사운드 인덱스",
            L"시작 값 (마디선 0/1)", L"끝 값", {L"단계", L"선형", L"부드럽게", L"지수"},
            L"오디오 버스", L"추가 / 같은 위치 수정",
            L"동·캇은 독립 설정입니다. 둘 다 변경하려면 같은 위치에 각각 추가하세요. 변경 값은 다음 지시까지 유지합니다.",
            L"동", L"캇",
            L"오디오 분석 / 히트사운드", L"현재 시간 (ms)",
            L"마우스 휠: 마디 이동 · ← / →: 1ms · Ctrl+S: 저장", L"오디오 분석 중…",
            L"저장하지 않은 변경을 버리고 나가시겠습니까?", L"FingerDrum 에디터", L"확인", L"취소"};

        static constexpr EditorTextSet English{
            {L"PATTERN", L"TIMING", L"METADATA", L"EFFECTS"},
            L"SAVE", L"SAVE *",
            L"Click the long-note end · Change tool/Escape: cancel",
            L"SCORE VIEW", L"REALTIME VIEW", L"BEAT DIVIDER", L"DIRECT INPUT",
            L"Beat division (1/N)", L"TOOLS",
            {L"SELECT", L"DON", L"BIG DON", L"ROLL", L"BALLOON", L"BPM", L"MEASURE"},
            {L"Don", L"Kat", L"Big Don", L"Big Kat", L"Purple Note", L"Roll", L"Tick Roll",
             L"Big Roll", L"Big Tick Roll", L"Balloon", L"DengDeng", L"Don Buzz", L"Kat Buzz"},
            L"TIMING · BPM / MEASURE LENGTH", L"MEASURE     POSITION      TYPE            VALUE",
            L"Measure", L"Delay", L"DELETE", L"START MEASURE", L"POSITION N/D",
            L"ADD / UPDATE BPM", L"MEASURE LENGTH N/D", L"APPLY MEASURE LENGTH",
            L"Overflowing notes keep their excess beat and move to the next measure.\nMove BPM/effects outside the measure first.",
            L"METADATA", L"YMM RELATIVE PATH", L"PATTERN NAME", L"MAKERS (ONE PER LINE)",
            L"TAGS (ONE PER LINE)", L"OFFSET (ms)", L"BASE BPM", L"HIT-SOUND TABLE",
            L"Register hit sounds as index: relative path, for example 1: Sounds/pop.wav.\nThe matching YME is saved beside the YMP. Ctrl+S: save",
            L"EFFECTS", L"START MEASURE / BEAT    TYPE            VALUE / REFERENCE INDEX",
            {L"VOLUME", L"MEASURE LINE", L"NOTE SPEED", L"SCROLL SPEED", L"DON SOUND", L"KAT SOUND"},
            L"START BEAT", L"END MEASURE (OPTIONAL)", L"END BEAT (OPTIONAL)", L"HIT-SOUND INDEX",
            L"START VALUE (MEASURE LINE 0/1)", L"END VALUE",
            {L"Step", L"Linear", L"Smoothstep", L"Exponential"}, L"AUDIO BUS",
            L"ADD / UPDATE AT POSITION",
            L"Don and Kat are independent. Add each one at the same position to change both. Values remain until the next command.",
            L"Don", L"Kat",
            L"AUDIO ANALYSIS / HIT SOUNDS", L"CURRENT TIME (ms)",
            L"Mouse wheel: move measures · ← / →: 1 ms · Ctrl+S: save", L"ANALYZING AUDIO…",
            L"Discard unsaved changes and leave?", L"FingerDrum Editor", L"OK", L"Cancel"};
        return language == Language::Korean ? Korean : English;
    }
} // namespace finger_drum::texts
