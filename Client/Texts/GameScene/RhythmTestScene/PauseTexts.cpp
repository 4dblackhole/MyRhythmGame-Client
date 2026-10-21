#include "PauseTexts.h"

PauseTextSet Pause(const TextCatalog &texts)
{
    return {{texts.Get("Pause.actions.0"), texts.Get("Pause.actions.1"), texts.Get("Pause.actions.2"),
             texts.Get("Pause.actions.3")},
            texts.Get("Pause.resumeValueFormat"),
            texts.Get("Pause.rewindValueFormat"),
            texts.Get("Pause.preparing")};
}
