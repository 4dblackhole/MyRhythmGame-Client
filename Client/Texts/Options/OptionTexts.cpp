#include "OptionTexts.h"

namespace finger_drum::texts
{
    const OptionTextSet &Options(const Language language) noexcept
    {
        static constexpr OptionTextSet Korean{L"옵션", L"언어", L"스킨", L"스킨 설정을 저장하지 못했습니다."};
        static constexpr OptionTextSet English{L"OPTIONS", L"LANGUAGE", L"SKIN", L"Could not save the skin setting."};
        return language == Language::Korean ? Korean : English;
    }
} // namespace finger_drum::texts
