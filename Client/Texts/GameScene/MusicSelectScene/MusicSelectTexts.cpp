#include "MusicSelectTexts.h"
namespace finger_drum::texts
{
    MusicSelectTextSet MusicSelect(const TextCatalog &texts)
    {
        return {texts.Get("MusicSelect.all"),
                texts.Get("MusicSelect.personalRecord"),
                texts.Get("MusicSelect.noRecords"),
                texts.Get("MusicSelect.backgroundPreview"),
                texts.Get("MusicSelect.noImage"),
                texts.Get("MusicSelect.noSongSelected"),
                texts.Get("MusicSelect.difficulty"),
                texts.Get("MusicSelect.creator"),
                texts.Get("MusicSelect.emptyDetails"),
                texts.Get("MusicSelect.searchSongs"),
                texts.Get("MusicSelect.songCountFormat"),
                {texts.Get("MusicSelect.sortModes.0"), texts.Get("MusicSelect.sortModes.1"),
                 texts.Get("MusicSelect.sortModes.2")},
                texts.Get("MusicSelect.browserHint"),
                texts.Get("MusicSelect.optionSelect"),
                texts.Get("MusicSelect.back"),
                texts.Get("MusicSelect.go"),
                texts.Get("MusicSelect.noSongsAvailable"),
                texts.Get("MusicSelect.noDifficulties"),
                texts.Get("MusicSelect.noDifficulty"),
                texts.Get("MusicSelect.detailsFormat"),
                texts.Get("MusicSelect.catalogErrorPrefix"),
                texts.Get("MusicSelect.unsupportedModePrefix"),
                texts.Get("MusicSelect.patternErrorPrefix"),
                texts.Get("MusicSelect.unknownChartError"),
                texts.Get("MusicSelect.loadingSongs")};
    }
} // namespace finger_drum::texts
