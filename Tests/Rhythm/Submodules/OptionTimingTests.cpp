#include "Mode/PlayGameModeFactory.h"
#include "TestSupport.h"

namespace finger_drum::tests
{
    void TestModeInputOverrides()
    {
        mode::PlaySessionOptions options;
        options.inputKeys["LeftDon"] = {'S', 'A'};
        auto pattern = MakeLongPattern(mode::TaikoNoteType::Buzz, {"TickDivision=64", "Action=Don"});
        auto loaded = mode::CreatePlayGameMode("Taiko", options)->CreateSession(pattern);
        Require(loaded.Succeeded(),
                "Remapped mode must load: " +
                    (loaded.diagnostics.empty() ? std::string{} : loaded.diagnostics.front().message));
        auto &session = *loaded.session;
        Require(!session.InputMapping().contains('F') && !session.InputMapping().contains('V') &&
                    !session.InputMapping().contains(0) && session.InputMapping().at('S') == 1 &&
                    session.InputLayout().bindings[1].keys == std::vector<rhythm::PhysicalKey>{'S', 'A'},
                "Session input and presentation share the resolved primary/auxiliary keys.");
        Require(
            !HasEvent(session.ProcessInput('F', rhythm::InputEdge::Pressed, {}), rhythm::NoteEventType::HitAccepted),
            "Old key must no longer judge the note.");
        Require(HasEvent(session.ProcessInput('S', rhythm::InputEdge::Pressed, {}), rhythm::NoteEventType::HitAccepted),
                "Remapped primary judges the Buzz head.");
        const std::array<rhythm::PhysicalKey, 1> held{'A'};
        Require(HasEvent(session.Update(rhythm::RhythmTime{125'000}, held), rhythm::NoteEventType::TickAccepted),
                "Remapped auxiliary held state processes scheduled Buzz ticks.");
        auto result = session.TakeResult(mode::PlayEndReason::Interrupted, rhythm::RhythmTime{125'000});
        Require(result->Summary().inputMapping.at('S') == 1 && result->Summary().inputs.front().physicalKey == 'S',
                "Input trace records actual physical keys and their mapping.");
        options.inputKeys["LeftDon"] = {'K', 'A'};
        Require(!mode::TaikoMode(options).CreateSession(pattern).Succeeded(),
                "Conflicting bindings must fail before play.");
        // A different mode supplies different actions/key counts through the same contract.
        const auto other = mode::ResolveInputLayout({"SevenKeys", {{"Lane1", "Lane 1", 7, {'S'}}}}, {{"Lane1", {'A'}}});
        Require(other.bindings.front().action == 7 && other.bindings.front().keys.front() == 'A',
                "Binding resolution does not assume Taiko actions or four rows.");
    }

    void TestRuntimeGlobalOffset()
    {
        auto pattern = MakeLongPattern(mode::TaikoNoteType::TickRoll, {"TickDivision=16"});
        pattern.patternOffsetMilliseconds = 125;
        pattern.timing = {{{0, {1, 4}}, chart::TimingDirectiveType::Bpm, 240},
                          {{0, {1, 4}}, chart::TimingDirectiveType::DelayMilliseconds, 100},
                          {{1, {}}, chart::TimingDirectiveType::MeasureLength, 0, {3, 4}}};
        auto parsed = chart::ChartParser{}.ParseEffect(
            "[Speed]\n1,0/1,Area,1,1/2,#ScrollSpeed Whole,From=1,To=2,Curve=Linear\n"
            "[Sounds]\n1,0/1,Area,1,1/2,#Volume HitSound,From=0.2,To=0.8,Curve=Linear\n"
            "[Zone]\n1,1/4,#Kiai ON\n",
            "offset.yme");
        Require(parsed.Succeeded(), "Offset effect fixture parses.");
        const chart::MusicalTimeline original(pattern);
        const auto before = original.CompileEffects(parsed.document);
        const auto ticks = original.CompileSubdivisions(pattern.notes[0].position, pattern.notes[1].position, 16);
        for (const std::int64_t us : {-100'000, 100'000})
        {
            const rhythm::RhythmDuration shift{us};
            chart::ChartEditor editor(pattern, parsed.document, shift);
            const auto &timeline = editor.Timeline();
            Require(!editor.Dirty() && editor.Pattern().patternOffsetMilliseconds == 125 &&
                        editor.Pattern().notes[0].position == pattern.notes[0].position,
                    "Runtime offset does not edit or dirty the authored chart.");
            for (const chart::MusicalPosition &position :
                 {chart::MusicalPosition{}, {{0}, {1, 4}}, {{0}, {1, 2}}, {{1}, {}}})
                Require(timeline.Compile(position) == original.Compile(position) + shift &&
                            std::abs(timeline.WholeNotesAtTime(timeline.Compile(position)) -
                                     original.WholeNotesAtTime(original.Compile(position))) < 1e-12,
                        "Notes/BPM/delay/inverse seeking share one translation.");
            const auto shifted = timeline.CompileSubdivisions(pattern.notes[0].position, pattern.notes[1].position, 16);
            Require(shifted.size() == ticks.size(), "Tick count stays unchanged.");
            for (std::size_t i = 0; i < ticks.size(); ++i)
                Require(shifted[i] == ticks[i] + shift, "Every tick shifts once.");
            const auto effects = timeline.CompileEffects(parsed.document);
            for (std::size_t i = 0; i < effects.size(); ++i)
                Require(effects[i].timing == before[i].timing + shift && effects[i].duration == before[i].duration,
                        "YME points/endpoints shift, Area durations do not change.");
            for (std::size_t i = 0; i < original.MeasureSpans().size(); ++i)
                Require(timeline.MeasureSpans()[i].begin == original.MeasureSpans()[i].begin + shift,
                        "Measure lines share the same offset.");
            mode::PlaySessionOptions options;
            options.globalOffset = shift;
            auto loaded = mode::TaikoMode(options).CreateSession(pattern, parsed.document);
            Require(loaded.Succeeded() &&
                        loaded.session->Gear().Lanes()[0]->Notes()[0]->Timing() == editor.Notes()[0].timing,
                    "Editor and gameplay compile the same head time.");
            const auto result = loaded.session->TakeResult(mode::PlayEndReason::Interrupted, {});
            Require(result->Summary().globalOffset == shift, "Replay summary retains the offset used by this attempt.");
            auto copy = editor;
            copy.ReplaceTiming(pattern.timing);
            copy.Replace(pattern, parsed.document);
            Require(copy.Notes()[0].timing == editor.Notes()[0].timing, "Copy and edits preserve runtime offset.");
            Require(chart::ChartEditor::WritePattern(copy.Pattern()) == chart::ChartEditor::WritePattern(pattern),
                    "Saved YMP never contains global offset.");
        }
    }
} // namespace finger_drum::tests
