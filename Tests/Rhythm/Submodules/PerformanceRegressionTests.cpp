#include "TestSupport.h"
#include "Utility/IntervalIndex.h"
#include <set>

namespace finger_drum::tests
{
    void TestIntervalRangeQueries()
    {
        using Index = chart::IntervalIndex<std::int64_t, std::size_t>;
        std::vector<Index::Entry> entries;
        for (std::size_t i = 0; i < 1000; ++i)
        {
            const auto begin = static_cast<std::int64_t>((i * 37) % 1000) - 500;
            entries.push_back({begin, begin + static_cast<std::int64_t>(i % 47), i});
        }
        Index index;
        index.Assign(entries);
        for (std::int64_t point = -520; point <= 520; point += 13)
        {
            std::set<std::size_t> actual, expected;
            index.Visit(point, point + 5, [&](auto id) { actual.insert(id); });
            for (const auto &entry : entries)
                if (entry.begin <= point + 5 && entry.end >= point)
                    expected.insert(entry.value);
            Require(actual == expected,
                    "Balanced interval queries must include closed boundaries and spanning intervals.");
        }
        index.Assign({});
        bool called = false;
        index.Visit(-1, 1, [&](auto) { called = true; });
        Require(!called, "Replacing an interval projection must remove its former entries.");
    }

    void TestLongViewportIndexEdits()
    {
        chart::PatternDocument pattern;
        pattern.baseBpm = 120;
        for (std::size_t i = 0; i < 1000; ++i)
        {
            const auto measure = static_cast<std::int64_t>(i * 2);
            pattern.notes.push_back({{measure, {}}, 11, 1, {}, {}, {}, i * 2});
            pattern.notes.push_back({{measure + 1, {1, 2}}, 11, 2, {}, {}, {}, i * 2 + 1});
        }
        chart::ChartEditor editor(pattern);
        const auto query = [&]
        { return editor.NotesInTimeRange(rhythm::RhythmTime{1'000'000}, rhythm::RhythmTime{1'100'000}); };
        Require(query().size() == 2, "A viewport between both endpoints must retain its long note.");
        editor.DeleteNote(1);
        Require(query().empty(), "Deleting through a tail must invalidate the interval projection.");
        const auto selected = editor.FindNote(2);
        Require(selected.has_value(), "Other stable IDs must survive deletion.");
        auto head = selected->head.note;
        head.position = {0, {}};
        editor.UpdateNote(2, head, chart::MusicalPosition{1, {1, 2}});
        Require(query().size() == 2 && query().front().note.sourceOrder == 2,
                "Moving a long note into the viewport must refresh its interval without changing its ID.");
        editor.ReplaceTiming({{{0, {}}, chart::TimingDirectiveType::Bpm, 240}});
        Require(editor.NotesInTimeRange(rhythm::RhythmTime{1'600'000}, rhythm::RhythmTime{1'700'000}).empty(),
                "Tempo edits must invalidate cached interval endpoints.");
    }

    void TestUnusedChartSoundsAreNotPrepared()
    {
        chart::PatternDocument pattern;
        pattern.mode = "Taiko";
        pattern.notes.push_back({{0, {}}, 1, 0, "2"});
        chart::EffectDocument effects;
        effects.hitSounds = {{"1", "unused.wav"}, {"2", "used.wav"}};
        auto loaded = mode::TaikoMode{}.CreateSession(pattern, effects);
        Require(loaded.Succeeded() && loaded.session->HitSoundFiles().size() == 1 &&
                    loaded.session->HitSoundFiles().contains("Chart.HitSound.2"),
                "Unused sound table entries must not become gameplay loading work.");
    }
} // namespace finger_drum::tests
