#include "GameplayTexts.h"
namespace finger_drum::texts
{
    GameplayTextSet Gameplay(const TextCatalog &texts)
    {
        return {texts.Get("Gameplay.errorPrefix"), texts.Get("Gameplay.audioInitializationFailed"),
                texts.Get("Gameplay.focusNone"),   texts.Get("Gameplay.focusPrefix"),
                texts.Get("Gameplay.lastFormat"),  texts.Get("Gameplay.lastNone"),
                texts.Get("Gameplay.early"),       texts.Get("Gameplay.late")};
    }
} // namespace finger_drum::texts
