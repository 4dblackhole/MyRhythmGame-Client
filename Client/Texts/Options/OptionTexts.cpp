#include "OptionTexts.h"

namespace finger_drum::texts
{
    const OptionTextSet &Options(const Language language) noexcept
    {
        static constexpr OptionTextSet Korean{L"옵션", L"언어"};
        static constexpr OptionTextSet English{L"OPTIONS", L"LANGUAGE"};
        return language == Language::Korean ? Korean : English;
    }
} // namespace finger_drum::texts
