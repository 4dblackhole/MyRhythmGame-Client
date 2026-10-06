#include "TestSupport.h"

namespace finger_drum::tests
{
    void TestChartEditingAndSave()
    {
        using namespace finger_drum;
        {
            chart::PatternDocument source;
            source.baseBpm = 120;
            source.notes = {{{0, {}}, 11, 1, {}, {}, {}, 90},
                            {{10, {}}, 11, 2, {}, {}, {}, 2}};
            chart::ChartEditor indexed(source);
            Require(indexed.NotesInMeasures(4, 5).size() == 2 &&
                        indexed.NotesInTimeRange(rhythm::RhythmTime{8'000'000}, rhythm::RhythmTime{9'000'000}).size() == 2,
                    "Viewport queries must retain long notes whose endpoints both lie outside, regardless of ID order.");
            indexed.AddNote({5, {1, 4}}, 1);
            const auto addedId = indexed.NotesInMeasures(5, 6)[1].note.sourceOrder;
            indexed.DeleteNote(addedId);
            indexed.Replace(indexed.Pattern(), indexed.Effects());
            indexed.AddNote({5, {1, 4}}, 2);
            Require(indexed.Pattern().notes[1].sourceOrder > addedId,
                    "Deleting and recompiling must not reuse IDs of removed notes.");
            const auto audioRevision = indexed.AudioSourceRevision();
            const auto oldTime = indexed.NotesInMeasures(5, 6)[1].timing;
            auto changed = indexed.Pattern();
            changed.timing.push_back({{1, {}}, chart::TimingDirectiveType::Bpm, 240});
            changed.timing.push_back({{2, {}}, chart::TimingDirectiveType::DelayMilliseconds, -100});
            indexed.Replace(changed, indexed.Effects());
            Require(indexed.NotesInMeasures(5, 6)[1].timing < oldTime && indexed.AudioSourceRevision() == audioRevision,
                    "BPM/delay edits must update time indices without invalidating unchanged audio sources.");
            const auto revisionBeforeFailure = indexed.Revision();
            bool rejectedEnd = false;
            try { indexed.AddNote({1, {}}, 11, chart::MusicalPosition{-1, {}}); }
            catch (const std::invalid_argument &) { rejectedEnd = true; }
            Require(rejectedEnd && indexed.Revision() == revisionBeforeFailure,
                    "Failed insertion must leave trees and revision unchanged.");
            indexed.DeleteNote(2);
            Require(indexed.Pattern().notes.size() == 1,
                    "Deleting a long tail must remove its paired head while preserving the normal note.");
            changed = indexed.Pattern();
            auto soundEffects = indexed.Effects();
            soundEffects.hitSounds.emplace("custom", "custom.wav");
            indexed.Replace(changed, soundEffects);
            Require(indexed.AudioSourceRevision() == audioRevision + 1,
                    "Changing a sound source must invalidate audio file resolution.");
            chart::EffectDocument effects;
            chart::EffectCommand speed;
            speed.type = chart::EffectCommandType::NoteSpeed;
            speed.beginValue = speed.endValue = .5;
            effects.commands.push_back(speed);
            indexed.Replace(indexed.Pattern(), effects);
            auto copied = indexed;
            indexed.Replace(indexed.Pattern(), {});
            Require(copied.EffectValueAt(chart::EffectCommandType::NoteSpeed, {3, {}}) == .5,
                    "Copied event trees must own independent effect lookup pointers.");
        }
        chart::PatternDocument p;
        p.baseBpm = 120;
        p.musicMetadataFile = "../Music/test.ymm";
        p.makers = {"테스트"};
        p.tags = {"editor"};
        chart::ChartEditor editor(p);
        editor.AddNote({0, {1, 4}}, 1);
        editor.AddNote({0, {3, 4}}, 11, chart::MusicalPosition{1, {3, 4}}, {"8"});
        editor.SetMeasureLength(0, {2, 4});
        Require(editor.Pattern().notes[0].position == chart::MusicalPosition{0, {1, 4}} &&
                    editor.Pattern().notes[1].position == chart::MusicalPosition{1, {1, 4}},
                "Only overflowing note positions must carry into later measures with exact "
                "remainders.");
        chart::ChartEditor collapsed(p);
        collapsed.AddNote({0, {3, 4}}, 11, chart::MusicalPosition{1, {1, 4}});
        bool rejected = false;
        try
        {
            collapsed.SetMeasureLength(0, {2, 4});
        }
        catch (const std::invalid_argument &)
        {
            rejected = true;
        }
        Require(rejected && collapsed.Timeline().MeasureLength(0) == chart::Rational{1, 1} &&
                    collapsed.Pattern().notes.front().position == chart::MusicalPosition{0, {3, 4}},
                "A collapsed long-note pair must reject the whole signature edit without altering "
                "the source.");
        editor.DeleteNote(1);
        Require(editor.Pattern().notes.size() == 1, "Deleting a long-note head must also remove its matching tail.");
        const auto &timeline = editor.Timeline();
        for (const chart::Rational beat : {chart::Rational{1, 3}, chart::Rational{1, 1}, chart::Rational{1000, 3}})
            Require(timeline.PositionToWholeNotes(timeline.PositionAtWholeNotes(beat)) == beat,
                    "Prefix sum inverse must remain exact, including extrapolated measures.");

        auto pattern = editor.Pattern();

        pattern.timing.push_back({{0, {1, 4}}, chart::TimingDirectiveType::Bpm, 240});
        chart::EffectDocument effects;
        effects.hitSounds["1"] = "Sounds/pop.wav";
        chart::EffectCommand speed;
        speed.type = chart::EffectCommandType::NoteSpeed;
        speed.beginValue = 1;
        speed.endValue = 2;
        speed.curve = chart::AutomationCurve::Linear;
        speed.endPosition = chart::MusicalPosition{1, {0, 1}};
        effects.commands.push_back(speed);
        effects.hitSoundChanges.push_back({{1, {1, 4}}, "1", 2});
        editor.Replace(pattern, effects);
        const auto revision = editor.Revision();
        editor.Replace(pattern, effects);
        Require(editor.Revision() == revision + 1, "Edits must invalidate dependent editor caches exactly once.");
        Require(std::abs(editor.Notes()[0].scrollMultiplier - (5.0 / 3.0)) < 1e-9,
                "Separate speed must interpolate by compiled elapsed time across a "
                "BPM change.");
        const auto beforeRevision = editor.Revision();
        const auto beforeDirty = editor.Dirty();
        const auto beforePattern = chart::ChartEditor::WritePattern(editor.Pattern());
        const auto beforeEffects = chart::ChartEditor::WriteEffects(editor.Effects());
        const auto beforeTime = editor.Notes().front().timing;
        auto invalidEffects = effects;
        invalidEffects.commands.front().beginValue = 0;
        bool invalidRejected = false;
        try
        {
            editor.Replace(pattern, invalidEffects);
        }
        catch (const std::invalid_argument &)
        {
            invalidRejected = true;
        }
        Require(invalidRejected && editor.Revision() == beforeRevision && editor.Dirty() == beforeDirty &&
                    chart::ChartEditor::WritePattern(editor.Pattern()) == beforePattern &&
                    chart::ChartEditor::WriteEffects(editor.Effects()) == beforeEffects &&
                    editor.Notes().front().timing == beforeTime && std::abs(editor.Notes().front().scrollMultiplier - (5.0 / 3.0)) < 1e-9,
                "Failed candidate validation must preserve document, revision, dirty state and compiled caches.");
        auto serialized = chart::ChartEditor::WriteEffects(effects);
        const auto parsedEffects = chart::ChartParser{}.ParseEffect(serialized);
        Require(parsedEffects.Succeeded() && parsedEffects.document.commands[0].endPosition == speed.endPosition &&
                    parsedEffects.document.hitSoundChanges.size() == 1,
                "YME regions and indexed hit sound changes must round-trip.");

        TemporaryDirectory directory("finger-drum-editor");
        pattern.sourcePath = directory.Path() / "edited.ymp";
        editor.Replace(pattern, effects);
        editor.Save();
        auto yme = pattern.sourcePath;
        yme.replace_extension(".yme");
        Require(std::filesystem::exists(yme) && !editor.Dirty(),
                "Save must produce adjacent YMP/YME and clear dirty state.");
        const auto parsed = chart::ChartParser{}.ParsePatternFile(pattern.sourcePath);
        Require(parsed.Succeeded() && parsed.document.notes.size() == pattern.notes.size() &&
                    parsed.document.makers == pattern.makers &&
                    chart::ChartParser{}.ParseEffectFile(yme).document.hitSounds == effects.hitSounds &&
                    chart::MusicalTimeline(parsed.document).CompileNotes(parsed.document)[0].timing ==
                        editor.Notes()[0].timing,
                "Saving must preserve Unicode metadata, hit sound table and exact compiled note "
                "timing.");
        editor.Save();
        Require(!std::filesystem::exists(pattern.sourcePath.string() + ".editor.bak"),
                "Successful save must clean its own backup.");
    }
} // namespace finger_drum::tests
