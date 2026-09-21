#include "GameplayTexts.h"

namespace finger_drum::texts
{
    const GameplayTextSet &Gameplay(const Language language) noexcept
    {
        static constexpr GameplayTextSet Korean{
            L"오디오: ", L"오디오 초기화에 실패했습니다.", L"포커스: 없음", L"포커스: ",
            L"\n마지막 #{}: {}", L"\n마지막: 없음"};
        static constexpr GameplayTextSet English{
            L"AUDIO: ", L"Audio initialization failed.", L"Focus: none", L"Focus: ",
            L"\nLast #{}: {}", L"\nLast: none"};
        return language == Language::Korean ? Korean : English;
    }
} // namespace finger_drum::texts
