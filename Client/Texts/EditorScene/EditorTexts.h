#pragma once

#include "Texts/TextCatalog.h"

#include <array>
#include <string_view>

namespace finger_drum::texts
{
    struct EditorTextSet
    {
        std::array<std::wstring_view, 4> tabs;
        std::wstring_view save;
        std::wstring_view saveDirty;

        std::wstring_view finishLongNote;
        std::wstring_view overview;
        std::wstring_view realtime;
        std::wstring_view beatDivider;
        std::wstring_view directInput;
        std::wstring_view beatDivisionDialog;
        std::wstring_view tools;
        std::array<std::wstring_view, 7> toolGroups;
        std::array<std::wstring_view, 13> toolVariants;

        std::wstring_view timingTitle;
        std::wstring_view timingColumns;
        std::wstring_view measureType;
        std::wstring_view delayType;
        std::wstring_view remove;
        std::wstring_view startMeasure;
        std::wstring_view changePosition;
        std::wstring_view addOrUpdateBpm;
        std::wstring_view measureLength;
        std::wstring_view applyMeasureLength;
        std::wstring_view timingHelp;

        std::wstring_view metadataTitle;
        std::wstring_view ymmRelativePath;
        std::wstring_view patternName;
        std::wstring_view makers;
        std::wstring_view tags;
        std::wstring_view offsetMilliseconds;
        std::wstring_view baseBpm;
        std::wstring_view hitSoundTable;
        std::wstring_view metadataHelp;

        std::wstring_view effectsTitle;
        std::wstring_view effectsColumns;
        std::array<std::wstring_view, 6> effectTypes;
        std::wstring_view startBeat;
        std::wstring_view endMeasureOptional;
        std::wstring_view endBeatOptional;
        std::wstring_view hitSoundIndex;
        std::wstring_view startValue;
        std::wstring_view endValue;
        std::array<std::wstring_view, 4> curves;
        std::wstring_view audioBus;
        std::wstring_view addOrUpdateEffect;
        std::wstring_view effectsHelp;
        std::wstring_view don;
        std::wstring_view kat;

        std::wstring_view audioAnalysis;
        std::wstring_view currentTime;
        std::wstring_view audioHelp;
        std::wstring_view analyzingAudio;

        std::wstring_view discardChanges;
        std::wstring_view editorTitle;
        std::wstring_view ok;
        std::wstring_view cancel;
    };

    [[nodiscard]] const EditorTextSet &Editor(Language language) noexcept;
} // namespace finger_drum::texts
