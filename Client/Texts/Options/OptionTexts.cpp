#include "OptionTexts.h"

namespace finger_drum::texts
{
    const OptionTextSet &Options(const Language language) noexcept
    {
        static constexpr OptionTextSet Korean{
            L"옵션", L"언어", L"스킨", L"오디오 미들웨어", L"오디오 출력 방식", L"출력 드라이버",
            L"일반 (자동)", L"사용 가능한 장치 없음", L"소리 없음",
            L"Option.ini를 저장하지 못했습니다.", L"오디오 설정을 적용하지 못했습니다.",
            L"저장된 출력 방식을 사용할 수 없어 다른 출력으로 시작했습니다.",
            L"저장된 드라이버가 없어 기본 장치를 사용합니다."};
        static constexpr OptionTextSet English{
            L"OPTIONS", L"LANGUAGE", L"SKIN", L"AUDIO MIDDLEWARE", L"AUDIO OUTPUT", L"OUTPUT DRIVER",
            L"Normal (Automatic)", L"No available devices", L"No sound",
            L"Could not save Option.ini.", L"Could not apply the audio setting.",
            L"The saved output is unavailable. Another output is active.",
            L"The saved driver is unavailable. Using the default device."};
        return language == Language::Korean ? Korean : English;
    }
} // namespace finger_drum::texts
