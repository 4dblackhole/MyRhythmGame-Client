#include "PauseTexts.h"
namespace finger_drum::texts
{
    PauseTextSet Pause(const TextCatalog &texts)
    {
        return {texts.Get("Pause.title"),
                texts.Get("Pause.practice"),
                texts.Get("Pause.controls"),
                {texts.Get("Pause.actions.0"), texts.Get("Pause.actions.1"), texts.Get("Pause.actions.2"),
                 texts.Get("Pause.actions.3")},
                texts.Get("Pause.resumeFormat"),
                texts.Get("Pause.rewindFormat"),
                texts.Get("Pause.retry"),
                texts.Get("Pause.retire"),
                texts.Get("Pause.preparing")};
    }
} // namespace finger_drum::texts
