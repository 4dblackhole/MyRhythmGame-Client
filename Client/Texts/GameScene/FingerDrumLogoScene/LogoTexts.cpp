#include "LogoTexts.h"

namespace finger_drum::texts
{
    const LogoTextSet &Logo(const Language language) noexcept
    {
        static constexpr LogoTextSet Korean{{L"게임 시작", L"에디터", L"옵션", L"종료"}};
        static constexpr LogoTextSet English{{L"Game Start", L"Editor", L"Option", L"Exit"}};
        return language == Language::Korean ? Korean : English;
    }
} // namespace finger_drum::texts
