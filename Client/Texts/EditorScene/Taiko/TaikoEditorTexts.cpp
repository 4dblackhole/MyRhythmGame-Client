#include "TaikoEditorTexts.h"
namespace finger_drum::texts
{
    TaikoEditorTextSet TaikoEditor(const TextCatalog &texts)
    {
        return {{texts.Get("TaikoEditor.toolGroups.0"), texts.Get("TaikoEditor.toolGroups.1"),
                 texts.Get("TaikoEditor.toolGroups.2"), texts.Get("TaikoEditor.toolGroups.3"),
                 texts.Get("TaikoEditor.toolGroups.4"), texts.Get("TaikoEditor.toolGroups.5"),
                 texts.Get("TaikoEditor.toolGroups.6")},
                {texts.Get("TaikoEditor.toolVariants.0"), texts.Get("TaikoEditor.toolVariants.1"),
                 texts.Get("TaikoEditor.toolVariants.2"), texts.Get("TaikoEditor.toolVariants.3"),
                 texts.Get("TaikoEditor.toolVariants.4"), texts.Get("TaikoEditor.toolVariants.5"),
                 texts.Get("TaikoEditor.toolVariants.6"), texts.Get("TaikoEditor.toolVariants.7"),
                 texts.Get("TaikoEditor.toolVariants.8"), texts.Get("TaikoEditor.toolVariants.9"),
                 texts.Get("TaikoEditor.toolVariants.10"), texts.Get("TaikoEditor.toolVariants.11"),
                 texts.Get("TaikoEditor.toolVariants.12")},
                texts.Get("TaikoEditor.effectsHelp"),
                texts.Get("TaikoEditor.don"),
                texts.Get("TaikoEditor.kat"),
                texts.Get("TaikoEditor.donSound"),
                texts.Get("TaikoEditor.katSound"),
                texts.Get("TaikoEditor.hits"),
                texts.Get("TaikoEditor.tickInterval"),
                texts.Get("TaikoEditor.beats"),
                texts.Get("TaikoEditor.noteInfo"),
                texts.Get("TaikoEditor.selectNoteHelp"),
                texts.Get("TaikoEditor.startPosition"),
                texts.Get("TaikoEditor.endPosition"),
                texts.Get("TaikoEditor.hitCount"),
                texts.Get("TaikoEditor.tickDivision"),
                texts.Get("TaikoEditor.hitSound"),
                texts.Get("TaikoEditor.apply"),
                texts.Get("TaikoEditor.resetDraft"),
                texts.Get("TaikoEditor.tickCount"),
                texts.Get("TaikoEditor.measure"),
                texts.Get("TaikoEditor.beatPosition")};
    }
} // namespace finger_drum::texts
