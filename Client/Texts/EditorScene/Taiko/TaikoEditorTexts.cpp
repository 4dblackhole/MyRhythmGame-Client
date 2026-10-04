#include "TaikoEditorTexts.h"
namespace finger_drum::texts
{
    const TaikoEditorTextSet &TaikoEditor(Language language) noexcept
    {
        static constexpr TaikoEditorTextSet Korean{{L"선택", L"동", L"큰 동", L"롤노트", L"풍선", L"BPM", L"마디"},
                                                   {L"동", L"캇", L"큰 동", L"큰 캇", L"보라노트", L"롤노트", L"틱롤",
                                                    L"큰 롤노트", L"큰 틱롤", L"풍선", L"뎅뎅", L"동 버즈", L"캇 버즈"},
                                                   L"동·캇은 독립 설정입니다. 둘 다 변경하려면 같은 위치에 각각 "
                                                   L"추가하세요. 변경 값은 다음 지시까지 유지합니다.",
                                                   L"동",
                                                   L"캇",
                                                   L"동 사운드",
                                                   L"캇 사운드"};
        static constexpr TaikoEditorTextSet English{
            {L"SELECT", L"DON", L"BIG DON", L"ROLL", L"BALLOON", L"BPM", L"MEASURE"},
            {L"Don", L"Kat", L"Big Don", L"Big Kat", L"Purple Note", L"Roll", L"Tick Roll", L"Big Roll",
             L"Big Tick Roll", L"Balloon", L"DengDeng", L"Don Buzz", L"Kat Buzz"},
            L"Don and Kat are independent. Add each one at the same position to change both. Values remain until the "
            L"next command.",
            L"Don",
            L"Kat",
            L"DON SOUND",
            L"KAT SOUND"};
        return language == Language::Korean ? Korean : English;
    }
} // namespace finger_drum::texts
