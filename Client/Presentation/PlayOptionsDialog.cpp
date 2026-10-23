#include "PlayOptionsDialog.h"
#include "Texts/TextCatalog.h"
#include "ValueDialog.h"
#include <charconv>
#include <format>

namespace
{
    template <class T> T Number(const std::wstring &value)
    {
        std::string ascii;
        for (const auto ch : value)
        {
            if (ch > 127)
                throw std::invalid_argument("Enter an ASCII number (decimal point: .).");
            ascii.push_back(static_cast<char>(ch));
        }
        T number{};
        const auto result = std::from_chars(ascii.data(), ascii.data() + ascii.size(), number);
        if (result.ec != std::errc{} || result.ptr != ascii.data() + ascii.size())
            throw std::invalid_argument("Enter a valid number (decimal point: .).");
        return number;
    }
} // namespace
bool EditPlayOptions(PlayOptions &options, const TextCatalog &texts)
{
    using Kind = DialogValue::Kind;
    const auto label = [&texts](const char *key) { return std::wstring(texts.Get(std::string("PlayOptions.") + key)); };
    const auto flag = [](bool value) { return value ? L"1" : L"0"; };
    std::vector<DialogValue> fields{
        {label("scrollUnit"),
         options.scrollUnit == PlayOptions::ScrollUnit::Bpm ? L"1" : L"0",
         Kind::Choice,
         {L"x", L"BPM"}},
        {label("scrollSpeed"), std::format(L"{:g}", options.scrollSpeed)},
        {label("ignoreScroll"), flag(options.ignoreScrollEffects), Kind::Check},
        {label("metronome"), flag(options.metronome), Kind::Check},
        {label("mirror"), flag(options.mirror), Kind::Check},
        {label("random"), flag(options.random), Kind::Check},
        {label("seed"), std::to_wstring(options.randomSeed)},
        {label("probability"), std::format(L"{:g}", options.randomAttemptProbability * 100)},
        {label("accelerate"), std::format(L"{:g}", options.accelerate)},
        {label("musicPitch"), flag(options.preserveMusicPitch), Kind::Check},
        {label("hitPitch"), flag(options.preserveHitSoundPitch), Kind::Check},
        {label("overrideJudge"), flag(options.judgeLevel.has_value()), Kind::Check},
        {label("judgeLevel"), std::format(L"{:.2f}", options.judgeLevel.value_or(JudgeLevel{}).Value())},
    };
    PlayOptions edited = options;
    const bool accepted =
        ShowValueDialog(label("title"), fields, label("ok"), label("cancel"),
                        [&]
                        {
                            edited.scrollUnit = fields[0].value == L"1" ? PlayOptions::ScrollUnit::Bpm
                                                                        : PlayOptions::ScrollUnit::Multiplier;
                            edited.scrollSpeed = Number<double>(fields[1].value);
                            edited.ignoreScrollEffects = fields[2].value == L"1";
                            edited.metronome = fields[3].value == L"1";
                            edited.mirror = fields[4].value == L"1";
                            edited.random = fields[5].value == L"1";
                            edited.randomSeed = Number<std::uint64_t>(fields[6].value);
                            edited.randomAttemptProbability = Number<double>(fields[7].value) / 100.0;
                            edited.accelerate = Number<double>(fields[8].value);
                            edited.preserveMusicPitch = fields[9].value == L"1";
                            edited.preserveHitSoundPitch = fields[10].value == L"1";
                            edited.judgeLevel = fields[11].value == L"1"
                                                    ? std::optional{JudgeLevel{Number<double>(fields[12].value)}}
                                                    : std::nullopt;
                            edited.Validate();
                        });
    if (accepted)
        options = edited;
    return accepted;
}
