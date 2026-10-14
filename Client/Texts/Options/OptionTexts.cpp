#include "OptionTexts.h"

namespace finger_drum::texts
{
    std::wstring_view InputBindingName(Language language, std::string_view mode, std::string_view id) noexcept
    {
        if (mode != "Taiko" || language != Language::Korean)
            return {};
        if (id == "LeftKat")
            return L"왼쪽 캇";
        if (id == "LeftDon")
            return L"왼쪽 동";
        if (id == "RightDon")
            return L"오른쪽 동";
        if (id == "RightKat")
            return L"오른쪽 캇";
        return {};
    }
    const OptionTextSet &Options(const Language language) noexcept
    {
        static constexpr OptionTextSet Korean{
            L"해상도",
            L"글로벌 오프셋 (ms)",
            L"클릭 → 숫자 입력 → Enter\n양수: 노트·효과가 음악보다 늦음",
            L"키 설정",
            L"기본",
            L"보조",
            L"키를 누르세요",
            L"창 크기를 변경하지 못했습니다.",
            L"설정 값을 확인해주세요.",
            L"옵션",
            L"언어",
            L"스킨",
            L"오디오 미들웨어",
            L"오디오 출력 방식",
            L"출력 드라이버",
            L"일반 (자동)",
            L"사용 가능한 장치 없음",
            L"소리 없음",
            L"Option.ini를 저장하지 못했습니다.",
            L"오디오 설정을 적용하지 못했습니다.",
            L"저장된 출력 방식을 사용할 수 없어 다른 출력으로 시작했습니다.",
            L"저장된 드라이버가 없어 기본 장치를 사용합니다.",
            L"Option.ini를 읽거나 만들지 못했습니다.\n기본 설정을 사용합니다. 원본 파일은 보존됩니다."};
        static constexpr OptionTextSet English{
            L"RESOLUTION",
            L"GLOBAL OFFSET (ms)",
            L"Click → type number → Enter\nPositive: notes/effects follow music",
            L"KEY BINDINGS",
            L"Primary",
            L"Auxiliary",
            L"Press a key",
            L"Could not resize the window.",
            L"Please check the setting.",
            L"OPTIONS",
            L"LANGUAGE",
            L"SKIN",
            L"AUDIO MIDDLEWARE",
            L"AUDIO OUTPUT",
            L"OUTPUT DRIVER",
            L"Normal (Automatic)",
            L"No available devices",
            L"No sound",
            L"Could not save Option.ini.",
            L"Could not apply the audio setting.",
            L"The saved output is unavailable. Another output is active.",
            L"The saved driver is unavailable. Using the default device.",
            L"Could not load or create Option.ini.\nUsing defaults; the original file is preserved."};
        return language == Language::Korean ? Korean : English;
    }
} // namespace finger_drum::texts
