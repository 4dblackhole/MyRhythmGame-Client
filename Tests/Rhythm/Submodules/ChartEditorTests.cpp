#include "TestSupport.h"

namespace tests
{
    namespace
    {
        void CheckSavedMetadataHistory()
        {
            TemporaryDirectory saved("youmu-metadata-history-save");
            PatternDocument pattern;
            pattern.sourcePath = saved.Path() / "metadata.ymp";
            pattern.name = "Before";
            ChartEditor document(pattern);
            pattern.name = "After";
            document.Replace(pattern, document.Effects());
            document.Save();
            const auto savedText = ChartEditor::WritePattern(document.Pattern());
            document.Undo();
            Require(document.Dirty() && document.Pattern().name == "Before" &&
                        document.Pattern().effectFile == "metadata.yme",
                    "Metadata undo must preserve the YME reference established by the first save.");
            document.Redo();
            Require(!document.Dirty() && ChartEditor::WritePattern(document.Pattern()) == savedText,
                    "Redo to a saved state must restore exactly the saved metadata, including automatic YME paths.");
        }

        void CheckEditorHistory()
        {
            CheckSavedMetadataHistory();
            ChartEditor bounded(PatternDocument{});
            for (int i = 0; i < 40; ++i)
                bounded.AddNote({0, {i, 64}}, 1);
            for (int i = 0; i < 32; ++i)
                bounded.Undo();
            Require(!bounded.CanUndo() && bounded.CanRedo() && bounded.Notes().size() == 8,
                    "History must retain exactly the latest 32 actions.");
            for (int i = 0; i < 32; ++i)
                bounded.Redo();
            Require(!bounded.CanRedo() && bounded.Notes().size() == 40, "Redo must restore all retained actions.");

            ChartEditor notes(PatternDocument{});
            notes.AddNote({0, {1, 4}}, 1);
            const auto id = notes.Notes().front().note.sourceOrder;
            notes.Undo();
            Require(!notes.Dirty() && notes.Notes().empty(), "Undo to the initial state restores clean status.");
            notes.Redo();
            auto changed = notes.Pattern();
            changed.notes.front().keyType = 2;
            notes.Replace(changed, {});
            notes.Undo();
            Require(notes.Notes().front().note.keyType == 1 && notes.FindNote(id).has_value(),
                    "Overwrite undo must restore the original Don and stable ID, not erase its slot.");
            notes.Redo();
            Require(notes.Notes().front().note.keyType == 2, "Overwrite redo must restore Kat.");
            notes.Undo();
            notes.AddNote({0, {1, 2}}, 2);
            Require(!notes.CanRedo(), "A new edit after Undo must clear the abandoned redo branch.");

            auto timing = notes.Timing();
            timing.push_back({{}, TimingDirectiveType::Bpm, 240});
            notes.ReplaceTiming(timing);
            EffectDocument effects;
            EffectCommand speed;
            speed.type = EffectCommandType::NoteSpeed;
            speed.beginValue = speed.endValue = 2;
            effects.commands.push_back(speed);
            notes.ReplaceEffects(effects);
            const auto compiled = notes.Notes().front().timing;
            notes.Undo();
            Require(notes.Notes().front().scrollMultiplier == 1 && notes.Notes().front().timing == compiled,
                    "Effects undo must restore compiled scroll values without changing timing.");
            notes.Undo();
            Require(notes.Notes().front().timing.count() == 500'000, "Timing undo must restore note times.");
            const auto revision = notes.Revision();
            bool failed = false;
            try
            {
                notes.ReplaceTiming({{{}, TimingDirectiveType::Bpm, -1}});
            }
            catch (const std::invalid_argument &)
            {
                failed = true;
            }
            Require(failed && notes.Revision() == revision && notes.CanRedo(),
                    "A rejected edit must preserve document and redo history.");
            notes.Redo();
            notes.Redo();
            Require(notes.Notes().front().scrollMultiplier == 2 && notes.Notes().front().timing == compiled,
                    "Timing/effects redo must recompile the original values.");

            ChartEditor longNotes(PatternDocument{});
            longNotes.AddNote({0, {}}, 11, MusicalPosition{1, {}});
            const auto head = longNotes.Notes().front().note.sourceOrder;
            longNotes.DeleteNote(head);
            longNotes.Undo();
            const auto restored = longNotes.FindNote(head);
            Require(restored && restored->tail.has_value() &&
                        longNotes.NotesInTimeRange(RhythmTime{500'000}, RhythmTime{600'000}).size() ==
                            2,
                    "Long-note undo must restore both endpoints and interval indices.");
            longNotes.Undo();
            Require(longNotes.Notes().empty(), "Undoing long-note insertion removes its entire pair.");
            longNotes.Redo();
            longNotes.Redo();
            Require(longNotes.Notes().empty(), "Long-note deletion redo must remove the restored pair.");

            TemporaryDirectory saved("youmu-history-save");
            PatternDocument pattern;
            pattern.sourcePath = saved.Path() / "history.ymp";
            ChartEditor document(pattern);
            document.AddNote({0, {}}, 1);
            document.Save();
            document.AddNote({0, {1, 4}}, 2);
            document.Undo();
            Require(!document.Dirty(), "Undo must recognize the most recently saved state.");
            document.Redo();
            Require(document.Dirty(), "Redo beyond the save point must mark the chart dirty.");
        }
    } // namespace
    void TestChartEditorIndices()
    {
        CheckEditorHistory();
        {
            ChartParser parser;
            const auto parsed = parser.ParsePattern("[Metadata]\nBG: abcd/bg1.png\noffset: 25.5, -40.25\n");
            const auto roundtrip = parser.ParsePattern(ChartEditor::WritePattern(parsed.document));
            Require(parsed.Succeeded() && roundtrip.Succeeded() &&
                        roundtrip.document.backgroundFile == "abcd/bg1.png" &&
                        roundtrip.document.backgroundOffsetX == 25.5 && roundtrip.document.backgroundOffsetY == -40.25,
                    "BG and engine-space offsets must round-trip independently of the YMP folder.");
            for (const auto text :
                 {"BG: ../outside.png", "BG: C:/outside.png", "offset: nan, 0", "offset: 1", "offset: 0, 1e300"})
                Require(!parser.ParsePattern(std::string("[Metadata]\n") + text).Succeeded(),
                        "Invalid BG paths/offsets must be rejected.");
        }

        {
            PatternDocument pattern;
            ChartEditor document(pattern);
            const auto revision = document.Revision();
            for (const double level : {0.0, 101.0, 50.001})
            {
                bool rejected = false;
                try
                {
                    pattern.judgementLevel = level;
                    document.Replace(pattern, {});
                }
                catch (const std::invalid_argument &)
                {
                    rejected = true;
                }
                Require(rejected && document.Revision() == revision && document.Pattern().judgementLevel == 50,
                        "Invalid JudgeLevel edits must preserve the complete document.");
            }
            pattern.judgementLevel = 50.25;
            document.Replace(pattern, {});
            Require(document.Pattern().judgementLevel == 50.25 && document.Pattern().judgementLevel.Format() == "50.25",
                    "Fractional JudgeLevel must be editable.");
        }
        {
            PatternDocument pattern;
            pattern.mode = "Taiko";
            pattern.musicMetadataFile = "fixture.ymm";
            pattern.notes = {{{0, {}}, 11, 1, {}, {"HitCount=3", "Custom=keep"}, {}, 10},
                             {{0, {1, 2}}, 11, 2, {}, {}, {}, 11},
                             {{2, {}}, 1, 0, {}, {}, {}, 12}};
            ChartEditor document(pattern);
            auto selected = document.FindNote(11);
            Require(selected && selected->head.note.sourceOrder == 10 && selected->tail->note.sourceOrder == 11,
                    "Tail selection must resolve a stable head/tail pair.");
            auto head = selected->head.note;
            head.keyType = 17;
            head.position = {1, {1, 4}};
            head.extraData = {"Action=Kat", "TickDivision=64", "Custom=keep"};
            document.UpdateNote(11, head, MusicalPosition{1, {3, 4}});
            selected = document.FindNote(10);
            Require(selected->head.note.sourceOrder == 10 && selected->tail->note.sourceOrder == 11 &&
                        selected->tail->note.keyType == 17 && document.FindNote(12)->head.note.position.measure == 2 &&
                        document.NotesInMeasures(0, 1).empty() && document.NotesInMeasures(1, 2).size() == 2,
                    "Property edits must preserve IDs/unrelated notes and update both endpoints and range indices.");
            const auto revision = document.Revision();
            auto invalid = selected->head.note;
            invalid.hitSound = "undefined";
            bool rejected = false;
            try
            {
                document.UpdateNote(10, invalid, selected->tail->note.position);
            }
            catch (const std::invalid_argument &)
            {
                rejected = true;
            }
            Require(rejected && document.Revision() == revision && document.FindNote(10)->head.note.hitSound.empty(),
                    "An invalid property edit must preserve the document and revision.");
            rejected = false;
            try
            {
                document.UpdateNote(10, selected->head.note, selected->head.note.position);
            }
            catch (const std::invalid_argument &)
            {
                rejected = true;
            }
            Require(rejected && document.Revision() == revision, "A zero-duration property edit must be atomic.");
            const auto reparsed =
                ChartParser{}.ParsePattern(ChartEditor::WritePattern(document.Pattern()));
            Require(reparsed.Succeeded() && reparsed.document.notes[0].extraData == head.extraData,
                    "Updated note options must survive serialization.");
            head.keyType = 2;
            document.UpdateNote(10, head, {});
            Require(!document.FindNote(11) && !document.FindNote(10)->tail && document.FindNote(12),
                    "Converting a long note to a tap must remove only its tail.");
            head.keyType = 11;
            document.UpdateNote(10, head, MusicalPosition{1, {3, 4}});
            Require(document.FindNote(10)->tail->note.sourceOrder > 12,
                    "Converting a tap back to a long note must not reuse the old tail ID.");
        }

        {
            PatternDocument source;
            source.baseBpm = 120;
            source.notes = {{{0, {}}, 11, 1, {}, {}, {}, 90}, {{10, {}}, 11, 2, {}, {}, {}, 2}};
            ChartEditor indexed(source);
            Require(
                indexed.NotesInMeasures(4, 5).size() == 2 &&
                    indexed.NotesInTimeRange(RhythmTime{8'000'000}, RhythmTime{9'000'000}).size() == 2,
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
            changed.timing.push_back({{1, {}}, TimingDirectiveType::Bpm, 240});
            changed.timing.push_back({{2, {}}, TimingDirectiveType::DelayMilliseconds, -100});
            indexed.Replace(changed, indexed.Effects());
            Require(indexed.NotesInMeasures(5, 6)[1].timing < oldTime && indexed.AudioSourceRevision() == audioRevision,
                    "BPM/delay edits must update time indices without invalidating unchanged audio sources.");
            const auto revisionBeforeFailure = indexed.Revision();
            bool rejectedEnd = false;
            try
            {
                indexed.AddNote({1, {}}, 11, MusicalPosition{-1, {}});
            }
            catch (const std::invalid_argument &)
            {
                rejectedEnd = true;
            }
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
            EffectDocument effects;
            EffectCommand speed;
            speed.type = EffectCommandType::NoteSpeed;
            speed.beginValue = speed.endValue = .5;
            effects.commands.push_back(speed);
            indexed.Replace(indexed.Pattern(), effects);
            auto copied = indexed;
            indexed.Replace(indexed.Pattern(), {});
            Require(copied.EffectValueAt(EffectCommandType::NoteSpeed, {3, {}}) == .5,
                    "Copied event trees must own independent effect lookup pointers.");
        }
    }

    void TestChartEditingAndSave()
    {

        PatternDocument p;
        p.baseBpm = 120;
        p.musicMetadataFile = "../Music/test.ymm";
        p.makers = {"테스트"};
        p.tags = {"editor"};
        ChartEditor editor(p);
        editor.AddNote({0, {1, 4}}, 1);
        editor.AddNote({0, {3, 4}}, 11, MusicalPosition{1, {3, 4}}, {"8"});
        editor.SetMeasureLength(0, {2, 4});
        Require(editor.Pattern().notes[0].position == MusicalPosition{0, {1, 4}} &&
                    editor.Pattern().notes[1].position == MusicalPosition{1, {1, 4}},
                "Only overflowing note positions must carry into later measures with exact "
                "remainders.");
        ChartEditor collapsed(p);
        collapsed.AddNote({0, {3, 4}}, 11, MusicalPosition{1, {1, 4}});
        bool rejected = false;
        try
        {
            collapsed.SetMeasureLength(0, {2, 4});
        }
        catch (const std::invalid_argument &)
        {
            rejected = true;
        }
        Require(rejected && collapsed.Timeline().MeasureLength(0) == Rational{1, 1} &&
                    collapsed.Pattern().notes.front().position == MusicalPosition{0, {3, 4}},
                "A collapsed long-note pair must reject the whole signature edit without altering "
                "the source.");
        editor.DeleteNote(1);
        Require(editor.Pattern().notes.size() == 1, "Deleting a long-note head must also remove its matching tail.");
        const auto &timeline = editor.Timeline();
        for (const Rational beat : {Rational{1, 3}, Rational{1, 1}, Rational{1000, 3}})
            Require(timeline.PositionToWholeNotes(timeline.PositionAtWholeNotes(beat)) == beat,
                    "Prefix sum inverse must remain exact, including extrapolated measures.");

        {
            PatternDocument variable;
            variable.timing = {{{1, {}}, TimingDirectiveType::MeasureLength, 0, {3, 4}},
                               {{1, {1, 4}}, TimingDirectiveType::Bpm, 240},
                               {{2, {1, 4}}, TimingDirectiveType::DelayMilliseconds, 250}};
            const MusicalTimeline indexed(variable);
            const MusicalPosition first{0, {3, 4}}, last{2, {1, 4}};
            const auto distance = indexed.WholeNotesBetween(first, last);
            Require(distance == Rational{5, 4} && indexed.AddWholeNotes(first, distance) == last &&
                        indexed.AddWholeNotes(last, -distance) == first,
                    "Musical addition/subtraction must cross changing measures exactly without BPM/delay distortion.");
            Require(indexed.Compile(indexed.AddWholeNotes({1, {1, 4}}, {1, 4})).count() -
                            indexed.Compile({1, {1, 4}}).count() ==
                        250'000,
                    "Only the final microsecond conversion applies the indexed BPM.");
            bool beforeStart = false;
            try
            {
                static_cast<void>(indexed.AddWholeNotes({}, {-1, 4}));
            }
            catch (const std::invalid_argument &)
            {
                beforeStart = true;
            }
            Require(beforeStart, "Musical subtraction before the first measure must be rejected.");
        }

        auto pattern = editor.Pattern();

        pattern.timing.push_back({{0, {1, 4}}, TimingDirectiveType::Bpm, 240});
        EffectDocument effects;
        effects.hitSounds["1"] = "Sounds/pop.wav";
        EffectCommand speed;
        speed.type = EffectCommandType::NoteSpeed;
        speed.beginValue = 1;
        speed.endValue = 2;
        speed.curve = AutomationCurve::Linear;
        speed.endPosition = MusicalPosition{1, {0, 1}};
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
        const auto beforePattern = ChartEditor::WritePattern(editor.Pattern());
        const auto beforeEffects = ChartEditor::WriteEffects(editor.Effects());
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
                    ChartEditor::WritePattern(editor.Pattern()) == beforePattern &&
                    ChartEditor::WriteEffects(editor.Effects()) == beforeEffects &&
                    editor.Notes().front().timing == beforeTime &&
                    std::abs(editor.Notes().front().scrollMultiplier - (5.0 / 3.0)) < 1e-9,
                "Failed candidate validation must preserve document, revision, dirty state and compiled caches.");
        auto serialized = ChartEditor::WriteEffects(effects);
        const auto parsedEffects = ChartParser{}.ParseEffect(serialized);
        Require(parsedEffects.Succeeded() && parsedEffects.document.commands[0].endPosition == speed.endPosition &&
                    parsedEffects.document.hitSoundChanges.size() == 1,
                "YME regions and indexed hit sound changes must round-trip.");

        TemporaryDirectory directory("youmu-editor");
        pattern.sourcePath = directory.Path() / "edited.ymp";
        editor.Replace(pattern, effects);
        editor.Save();
        auto yme = pattern.sourcePath;
        yme.replace_extension(".yme");
        Require(std::filesystem::exists(yme) && !editor.Dirty(),
                "Save must produce adjacent YMP/YME and clear dirty state.");
        const auto parsed = ChartParser{}.ParsePatternFile(pattern.sourcePath);
        Require(parsed.Succeeded() && parsed.document.notes.size() == pattern.notes.size() &&
                    parsed.document.makers == pattern.makers &&
                    ChartParser{}.ParseEffectFile(yme).document.hitSounds == effects.hitSounds &&
                    MusicalTimeline(parsed.document).CompileNotes(parsed.document)[0].timing ==
                        editor.Notes()[0].timing,
                "Saving must preserve Unicode metadata, hit sound table and exact compiled note "
                "timing.");
        editor.Save();
        auto backup = pattern.sourcePath;
        backup += L".editor.bak";
        Require(!std::filesystem::exists(backup), "Successful save must clean its own backup.");
    }
} // namespace tests
