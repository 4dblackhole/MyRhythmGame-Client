#include "OptionTexts.h"

std::wstring_view InputBindingName(const TextCatalog &texts, std::string_view mode, std::string_view id) noexcept
{
    if (mode != "Taiko")
        return {};
    if (id == "LeftKat")
        return texts.Get("Bindings.LeftKat");
    if (id == "LeftDon")
        return texts.Get("Bindings.LeftDon");
    if (id == "RightDon")
        return texts.Get("Bindings.RightDon");
    if (id == "RightKat")
        return texts.Get("Bindings.RightKat");
    return {};
}
OptionTextSet Options(const TextCatalog &texts)
{
    return {texts.Get("Options.resolution"),     texts.Get("Options.globalOffset"),
            texts.Get("Options.offsetHint"),     texts.Get("Options.keys"),
            texts.Get("Options.primary"),        texts.Get("Options.auxiliary"),
            texts.Get("Options.pressKey"),       texts.Get("Options.windowError"),
            texts.Get("Options.inputError"),     texts.Get("Options.title"),
            texts.Get("Options.language"),       texts.Get("Options.skin"),
            texts.Get("Options.middleware"),     texts.Get("Options.output"),
            texts.Get("Options.driver"),         texts.Get("Options.automatic"),
            texts.Get("Options.noDrivers"),      texts.Get("Options.noSound"),
            texts.Get("Options.saveError"),      texts.Get("Options.audioError"),
            texts.Get("Options.outputFallback"), texts.Get("Options.driverUnavailable"),
            texts.Get("Options.loadWarning"),    texts.Get("Options.middlewareFmod"),
            texts.Get("Options.outputWasapi"),   texts.Get("Options.outputAsio")};
}
