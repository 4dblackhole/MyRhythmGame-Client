#include "ResultTexts.h"
namespace finger_drum::texts
{
    ResultTextSet Result(const TextCatalog &texts)
    {
        return {texts.Get("Result.accuracy"),
                texts.Get("Result.percent"),
                texts.Get("Result.noteCounts"),
                {texts.Get("Result.groups.0"), texts.Get("Result.groups.1"), texts.Get("Result.groups.2")},
                texts.Get("Result.hpGraph"),
                texts.Get("Result.hpFuture"),
                texts.Get("Result.noHpData"),
                texts.Get("Result.timeAxis"),
                texts.Get("Result.start"),
                texts.Get("Result.end"),
                texts.Get("Result.judgements"),
                texts.Get("Result.ticks"),
                texts.Get("Result.tickFormat"),
                texts.Get("Result.groupTitle"),
                texts.Get("Result.rank"),
                texts.Get("Result.score"),
                texts.Get("Result.song"),
                texts.Get("Result.pattern"),
                texts.Get("Result.creator"),
                texts.Get("Result.histogram"),
                texts.Get("Result.peakFormat"),
                texts.Get("Result.frequency"),
                texts.Get("Result.errorAxis"),
                texts.Get("Result.early"),
                texts.Get("Result.late"),
                texts.Get("Result.scatter"),
                texts.Get("Result.mean"),
                texts.Get("Result.standardDeviation"),
                texts.Get("Result.variance"),
                texts.Get("Result.noTimingData"),
                texts.Get("Result.replay"),
                texts.Get("Result.retry"),
                texts.Get("Result.back"),
                texts.Get("Result.backShortcut"),
                texts.Get("Result.artist")};
    }
} // namespace finger_drum::texts
