#include "TestSupport.h"

namespace finger_drum::tests
{
    void TestChartEditingAndSave()
    {
        using namespace finger_drum;
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
        Require(editor.Pattern().notes.size() == 1,
                "Deleting a long-note head must also remove its matching tail.");
        const auto &timeline = editor.Timeline();
        for (const chart::Rational beat :
             {chart::Rational{1, 3}, chart::Rational{1, 1}, chart::Rational{1000, 3}})
            Require(timeline.PositionToWholeNotes(timeline.PositionAtWholeNotes(beat)) == beat,
                    "Prefix sum inverse must remain exact, including extrapolated measures.");

        auto pattern = editor.Pattern();
        pattern.hitSounds["1"] = "Sounds/pop.wav";
        pattern.timing.push_back({{0, {1, 4}}, chart::TimingDirectiveType::Bpm, 240});
        chart::EffectDocument effects;
        chart::EffectCommand speed;
        speed.type = chart::EffectCommandType::ScrollSpeed;
        speed.beginValue = 1;
        speed.endValue = 2;
        speed.curve = chart::AutomationCurve::Linear;
        speed.endPosition = chart::MusicalPosition{1, {0, 1}};
        effects.commands.push_back(speed);
        effects.hitSoundChanges.push_back({{1, {1, 4}}, "1", 2});
        editor.Replace(pattern, effects);
        const auto revision = editor.Revision();
        editor.Replace(pattern, effects);
        Require(editor.Revision() == revision + 1,
                "Edits must invalidate dependent editor caches exactly once.");
        Require(std::abs(editor.Notes()[0].scrollMultiplier - 1.5) < 1e-9,
                "Region speed must interpolate by rational beat, not by elapsed seconds across a "
                "BPM change.");
        auto serialized = chart::ChartEditor::WriteEffects(effects);
        const auto parsedEffects = chart::ChartParser{}.ParseEffect(serialized);
        Require(parsedEffects.Succeeded() &&
                    parsedEffects.document.commands[0].endPosition == speed.endPosition &&
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
        Require(
            parsed.Succeeded() && parsed.document.notes.size() == pattern.notes.size() &&
                parsed.document.makers == pattern.makers &&
                parsed.document.hitSounds == pattern.hitSounds &&
                chart::MusicalTimeline(parsed.document).CompileNotes(parsed.document)[0].timing ==
                    editor.Notes()[0].timing,
            "Saving must preserve Unicode metadata, hit sound table and exact compiled note "
            "timing.");
        editor.Save();
        Require(!std::filesystem::exists(pattern.sourcePath.string() + ".editor.bak"),
                "Successful save must clean its own backup.");
    }
} // namespace finger_drum::tests
