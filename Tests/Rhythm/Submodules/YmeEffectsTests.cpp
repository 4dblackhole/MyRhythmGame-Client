#include "Automation/InterpolationExpression.h"
#include "Automation/ScrollAutomation.h"
#include "Taiko/Submodules/TaikoAudioPreview.h"
#include "Taiko/Submodules/TaikoChartNotes.h"
#include "TestSupport.h"
#include <limits>

namespace finger_drum::tests
{
    namespace
    {
        chart::EffectDocument ParseRegressionEffects(std::string_view text)
        {
            const auto parsed = chart::ChartParser{}.ParseEffect(text, "quality-regression.yme");
            Require(parsed.Succeeded(), "The regression fixture must pass load-time validation.");
            return parsed.document;
        }
        chart::PatternDocument RegressionPattern()
        {
            chart::PatternDocument pattern;
            pattern.mode = "Taiko";
            pattern.baseBpm = 120;
            return pattern;
        }
    } // namespace

    void TestMeasureLineScrollEffects()
    {
        auto pattern = RegressionPattern();
        pattern.notes = {{{4, {}}, 1}};
        pattern.timing = {{{0, {1, 2}}, chart::TimingDirectiveType::DelayMilliseconds, 500}};
        const auto effects = ParseRegressionEffects(R"([Speed]
1,0/1,#ScrollSpeed Whole,Value=2
1,0/1,Area,3,0/1,#ScrollSpeed Separate,From=1,To=0.25,Curve=Linear
5,0/1,#ScrollSpeed Separate,Value=1
)");
        auto loaded = mode::TaikoMode{}.CreateSession(pattern, effects);
        Require(loaded.Succeeded(), "Measure-line scroll fixture must load.");
        auto &session = *loaded.session;
        const auto &lines = session.MeasureLines();
        const auto originalSize = lines.size();
        Require(lines.size() >= 5 && lines[0].timing == rhythm::RhythmTime{} &&
                    lines[1].timing == rhythm::RhythmTime{2'500'000} &&
                    std::abs(lines[1].scrollMultiplier - (1 - .75 * 2.5 / 4.5)) < 1e-12 &&
                    lines[2].scrollMultiplier == .25 && session.FindNotePresentation(1)->scrollMultiplier == 1,
                "Measure lines must fix Separate at their compiled start, including delayed Area interpolation.");
        Require(session.MinimumScrollMultiplier() <= .25 &&
                    std::abs(session.ScrollDistance(lines[1].timing, {}) - 5'000'000) < 1e-9,
                "Slow measure lines must extend the visible horizon and share Whole integration with notes.");
        bool rejected = false;
        try
        {
            session.SetMeasureLines({{rhythm::RhythmTime{}, std::numeric_limits<double>::quiet_NaN()}});
        }
        catch (const std::invalid_argument &)
        {
            rejected = true;
        }
        Require(rejected && session.MeasureLines().size() == originalSize &&
                    session.MeasureLines()[2].scrollMultiplier == .25,
                "Invalid bar multipliers must leave the previous compiled lines intact.");

        pattern.timing = {{{1, {1, 4}}, chart::TimingDirectiveType::DelayMilliseconds, -1500}};
        pattern.notes = {{{1, {1, 4}}, 1}, {{2, {}}, 1}};
        const auto clipped = ParseRegressionEffects("[Speed]\n2,1/4,#ScrollSpeed Separate,Value=3\n");
        loaded = mode::TaikoMode{}.CreateSession(pattern, clipped);
        Require(loaded.Succeeded(), "Clipped measure-line fixture must load.");
        const auto &line = loaded.session->MeasureLines()[1];
        Require(line.timing == rhythm::RhythmTime{1'000'000} && line.scrollMultiplier == 3 &&
                    loaded.session->FindNotePresentation(1)->scrollMultiplier == line.scrollMultiplier,
                "A clipped bar and note at its surviving source start must use identical Separate speed.");
    }

    void TestDelayEffectTimeConsistency()
    {
        auto pattern = RegressionPattern();
        pattern.patternOffsetMilliseconds = 125;
        pattern.timing = {{{0, {1, 2}}, chart::TimingDirectiveType::Bpm, 240},
                          {{0, {1, 2}}, chart::TimingDirectiveType::DelayMilliseconds, 300},
                          {{0, {1, 2}}, chart::TimingDirectiveType::DelayMilliseconds, 50},
                          {{0, {3, 4}}, chart::TimingDirectiveType::DelayMilliseconds, -100},
                          {{1, {}}, chart::TimingDirectiveType::Bpm, 180},
                          {{1, {}}, chart::TimingDirectiveType::MeasureLength, 0, {3, 4}}};
        pattern.notes = {{{0, {1, 4}}, 1}, {{0, {1, 2}}, 2}, {{0, {3, 4}}, 1}, {{2, {}}, 1}};
        const auto effects = ParseRegressionEffects(R"([Interpolation]
Ramp: From+(To-From)*u
[HitSounds]
1: hit.wav
[Speed]
1,1/4,Area,2,1/4,#ScrollSpeed Separate,From=1,To=3,Curve=Ramp
[Sounds]
1,1/4,Area,2,1/4,#Volume HitSound,From=0.2,To=0.8,Curve=Ramp
1,3/4,#HitSound Don 1
[Zone]
1,3/4,#Kiai ON
)");
        chart::ChartEditor editor(pattern, effects);
        auto loaded = mode::TaikoMode{}.CreateSession(pattern, effects);
        Require(loaded.Succeeded(), "The mixed BPM/signature/delay effect fixture must load.");
        const auto &timeline = editor.Timeline();
        Require(timeline.Compile({0, {1, 2}}) == rhythm::RhythmTime{1'475'000} &&
                    timeline.Compile({0, {3, 4}}) == rhythm::RhythmTime{1'625'000} &&
                    timeline.Compile({1, {}}) == rhythm::RhythmTime{1'875'000} &&
                    timeline.Compile({2, {}}) == rhythm::RhythmTime{2'875'000},
                "Every BPM prefix and measure time must include prior delays exactly once.");
        const auto compiled = timeline.CompileEffects(effects);
        Require(compiled[0].timing == rhythm::RhythmTime{625'000} &&
                    compiled[0].duration == rhythm::RhythmDuration{1'583'333},
                "YME Area endpoints must use delayed real time while retaining their musical source.");
        const double u = 850'000.0 / 1'583'333.0;
        Require(
            std::abs(editor.Notes()[1].scrollMultiplier - (1 + 2 * u)) < 1e-12 &&
                std::abs(timeline.EffectValueAt(effects, chart::EffectCommandType::BusVolume, {0, {1, 2}}) -
                         (.2 + .6 * u)) < 1e-12,
            "Separate speed and authored interpolation must evaluate actual elapsed microseconds, not beat ratios.");
        const auto &values = loaded.session->EvaluateAutomation(rhythm::RhythmTime{1'475'000});
        const auto volume = std::ranges::find(values, std::string("HitSound"), &mode::AutomationValue::target);
        Require(volume != values.end() && std::abs(volume->value - (.2 + .6 * u)) < 1e-12 &&
                    !loaded.session->IsKiaiActive(rhythm::RhythmTime{1'624'999}) &&
                    loaded.session->IsKiaiActive(rhythm::RhythmTime{1'625'000}),
                "Runtime volume and zone changes must follow the same delayed endpoint times as the editor.");
        editor.ReplaceTiming({});
        Require(std::abs(editor.EffectValueAt(chart::EffectCommandType::NoteSpeed, {0, {1, 2}}) - 1.5) < 1e-12,
                "Timing replacement must rebuild the compiled effect lookup together with note times.");

        // A valid negative delay may cross an earlier effect, without crossing a source note.
        pattern = RegressionPattern();
        pattern.timing = {{{0, {3, 4}}, chart::TimingDirectiveType::DelayMilliseconds, -750}};
        pattern.notes = {{{0, {}}, 1}, {{0, {3, 4}}, 1}};
        const auto reordered = ParseRegressionEffects(R"([Speed]
1,1/2,#ScrollSpeed Separate,Value=2
[Zone]
1,1/2,#Syncopation ON
)");
        chart::ChartEditor moved(pattern, reordered);
        loaded = mode::TaikoMode{}.CreateSession(pattern, reordered);
        Require(loaded.Succeeded() && moved.Notes().back().timing == rhythm::RhythmTime{750'000} &&
                    moved.Notes().back().scrollMultiplier == 1 && loaded.session->MinimumScrollMultiplier() == 1 &&
                    moved.EffectValueAt(chart::EffectCommandType::NoteSpeed, {0, {3, 4}}) == 1 &&
                    !moved.Timeline().FindEffectAt(reordered, chart::EffectCommandType::SyncopationZone, {0, {3, 4}}),
                "An effect whose real start is still in the future must not apply merely because its beat is earlier.");
        moved.ReplaceTiming({});
        Require(moved.Notes().back().scrollMultiplier == 2,
                "Removing delay must restore the effect's real-time ordering.");
        moved.ReplaceEffects(reordered);
        const chart::ChartEditor copied(moved);
        Require(copied.Notes().back().scrollMultiplier == 2 &&
                    copied.EffectValueAt(chart::EffectCommandType::NoteSpeed, {0, {3, 4}}) == 2,
                "Effect replacement/copy must preserve ownership of the compiled-time lookup pointers.");
    }

    void TestNegativeDelayLongsAndAreas()
    {
        auto pattern = RegressionPattern();
        pattern.timing = {{{1, {1, 4}}, chart::TimingDirectiveType::DelayMilliseconds, -1500}};
        const std::array<std::pair<chart::MusicalPosition, chart::MusicalPosition>, 5> endpoints{{
            {{0, {3, 4}}, {1, {3, 4}}}, // masked head, surviving tail
            {{0, {1, 4}}, {0, {3, 4}}}, // masked tail
            {{0, {3, 4}}, {1, {}}},     // both masked
            {{0, {1, 2}}, {1, {1, 4}}}, // equal compiled endpoints
            {{0, {3, 4}}, {1, {1, 4}}}  // reversed compiled endpoints
        }};
        for (int type : {11, 12, 13, 14, 15, 16, 17})
            for (std::size_t i = 0; i < endpoints.size(); ++i)
            {
                const auto [begin, end] = endpoints[i];
                pattern.notes = {{begin, type, 1, {}, {"HitCount=2", "TickDivision=16", "Action=Kat"}, {}, 10},
                                 {end, type, 2, {}, {}, {}, 11}};
                chart::ChartEditor editor(pattern);
                const auto playable = mode::taiko_notes::PlayableInSourceOrder(editor.Notes());
                auto loaded = mode::TaikoMode{}.CreateSession(pattern);
                Require(loaded.Succeeded(), "All Taiko long types must support negative-delay endpoint rules.");
                const auto &notes = loaded.session->Gear().Lanes().front()->Notes();
                const auto preview = mode::BuildTaikoAudioPreview(pattern, {}, editor.Timeline(), editor.Notes());
                if (i == 4)
                {
                    Require(playable.empty() && notes.empty() && preview.empty() && editor.Pattern().notes.size() == 2,
                            "Only reversed compiled longs disappear, while both authored endpoints remain saved.");
                    continue;
                }
                const auto oldHeadTime = editor.Timeline().Compile(begin), oldTailTime = editor.Timeline().Compile(end);
                Require(playable.size() == 2 && notes.size() == 1 && notes.front()->Timing() == oldHeadTime &&
                            loaded.session->FindNotePresentation(notes.front()->Id())->endTime == oldTailTime &&
                            std::ranges::any_of(preview, [&](const auto &cue) { return cue.time == oldHeadTime; }),
                        "Masked/equal long endpoints must keep their historical time in play, editor and preview.");
            }
        // Auto-count longs also remain valid when their entire source body is cut.
        pattern.notes = {{{0, {1, 2}}, 11, 1}, {{1, {1, 4}}, 11, 2}};
        Require(mode::TaikoMode{}.CreateSession(pattern).Succeeded(),
                "A zero-length retained auto-count roll must keep a valid minimum hit requirement.");

        pattern.notes = {{{1, {3, 4}}, 1}};
        const auto effects = ParseRegressionEffects(R"([HitSounds]
1: old.wav
2: overwritten.wav
3: new.wav
[Speed]
1,3/4,Area,2,3/4,#ScrollSpeed Separate,From=1,To=3,Curve=Linear
1,3/4,Area,2,1/4,#ScrollSpeed Whole,From=1,To=2,Curve=Linear
1,3/4,#ScrollSpeed Separate,Value=4
[Sounds]
1,1/2,Area,2,1/4,#Volume HitSound,From=0.2,To=0.8,Curve=Linear
1,1/4,Area,1,3/4,#Volume TickSound,From=0.3,To=0.9,Curve=Linear
1,1/4,#HitSound Don 1
1,3/4,#HitSound Don 2
2,1/4,#HitSound Don 3
[Zone]
1,3/4,#Kiai ON
1,3/4,#Syncopation ON
)");
        chart::ChartEditor editor(pattern, effects);
        const auto compiled = editor.Timeline().CompileEffects(effects);
        auto loaded = mode::TaikoMode{}.CreateSession(pattern, effects);
        Require(loaded.Succeeded() && compiled.size() == 3 && loaded.session->Effects().size() == compiled.size(),
                "Areas retain masked endpoints, reversed Areas disappear, and masked point commands disappear.");
        const auto equal = std::ranges::find(
            compiled, chart::EffectCommandType::BusVolume, [](const auto &effect)
            { return effect.command.target == "HitSound" ? effect.command.type : chart::EffectCommandType::Custom; });
        Require(equal != compiled.end() && equal->duration == rhythm::RhythmDuration{},
                "An Area whose compiled endpoints are equal must remain an instantaneous change.");
        Require(editor.EffectValueAt(chart::EffectCommandType::NoteSpeed, {1, {3, 4}}) == 3 &&
                    loaded.session->FindNotePresentation(1)->scrollMultiplier == 3 &&
                    !loaded.session->IsKiaiActive(rhythm::RhythmTime{3'000'000}),
                "Retained Areas and excluded point state must agree between editor and gameplay.");
        const auto hit = loaded.session->ProcessInput('F', rhythm::InputEdge::Pressed, rhythm::RhythmTime{2'000'000});
        const auto preview = mode::BuildTaikoAudioPreview(pattern, effects, editor.Timeline(), editor.Notes());
        Require(!hit.audioCues.empty() && hit.audioCues.front().sound == "Chart.HitSound.3" && preview.size() == 1 &&
                    preview.front().sound == hit.audioCues.front().sound,
                "A masked hit-sound change must not override the surviving point in play or audio preview.");
        const chart::ChartEditor copy(editor);
        Require(copy.EffectValueAt(chart::EffectCommandType::NoteSpeed, {1, {3, 4}}) == 3,
                "Copying must preserve ownership of the retained Area's indexed command.");
        editor.ReplaceTiming({});
        Require(editor.Timeline().CompileEffects(editor.Effects()).size() == effects.commands.size(),
                "Removing the cut must restore all authored Areas and point commands.");
        editor.ReplaceTiming(pattern.timing);
        editor.ReplaceEffects(effects);
        Require(editor.Timeline().CompileEffects(editor.Effects()).size() == 3 &&
                    editor.EffectValueAt(chart::EffectCommandType::NoteSpeed, {1, {3, 4}}) == 3,
                "Timing/effect replacement must recompute Area validity and point visibility atomically.");
    }

    void TestRuntimeAutomationRecovery()
    {
        const auto effects = ParseRegressionEffects(R"([Interpolation]
Singular: 1/(u-0.1)^2
[Sounds]
1,0/1,Area,2,0/1,#Volume HitSound,From=1,To=1,Curve=Singular
1,0/1,Area,2,0/1,#Volume TickSound,From=0,To=1,Curve=Linear
2,0/1,Area,3,0/1,#Volume HitSound,From=2,To=2,Curve=Singular
)");
        auto loaded = mode::TaikoMode{}.CreateSession(RegressionPattern(), effects);
        Require(loaded.Succeeded(), "An unsampled singularity must exercise runtime recovery.");
        auto &session = *loaded.session;
        const auto volume = [&session](long long time, std::string_view target)
        {
            for (const auto &value : session.EvaluateAutomation(rhythm::RhythmTime{time}))
                if (value.target == target)
                    return value.value;
            throw std::runtime_error("Missing effect track.");
        };
        Require(std::abs(volume(100'000, "HitSound") - 400) < 1e-8, "Capture the last valid value.");
        Require(std::abs(volume(200'000, "HitSound") - 400) < 1e-8 &&
                    std::abs(volume(200'000, "TickSound") - .1) < 1e-12,
                "A failing track must retain its value while another track continues.");
        Require(session.AutomationError().find("quality-regression.yme:4") != std::string_view::npos,
                "The visible error must identify the authored command.");
        Require(!session.IsKiaiActive(rhythm::RhythmTime{200'000}),
                "Other consumers must reuse the safe cached result.");
        Require(std::abs(volume(400'000, "HitSound") - 100) < 1e-8 && !session.AutomationError().empty(),
                "Valid samples must resume while retaining the diagnostic until reset.");
        Require(std::abs(volume(2'200'000, "HitSound") - 100) < 1e-8,
                "A replacement command on the same track must retain the track's prior normal value on failure.");
        session.Reset();
        Require(session.AutomationError().empty() && volume(200'000, "HitSound") == 1,
                "Reset clears history; first-sample failure uses the validated From anchor.");
        session.SetEffects({});
        Require(session.AutomationError().empty() && session.EvaluateAutomation({}).empty(),
                "Replacing tracks must discard all previous history and errors.");

        const auto negative = ParseRegressionEffects(R"([Interpolation]
NarrowDip: (u-0.1)^2-0.000001
[Sounds]
1,0/1,Area,2,0/1,#Volume HitSound,From=1,To=1,Curve=NarrowDip
)");
        loaded = mode::TaikoMode{}.CreateSession(RegressionPattern(), negative);
        Require(loaded.Succeeded(), "A narrow negative dip must pass sampling and be guarded at runtime.");
        const auto good = loaded.session->EvaluateAutomation(rhythm::RhythmTime{100'000}).front().value;
        Require(loaded.session->EvaluateAutomation(rhythm::RhythmTime{200'000}).front().value == good &&
                    !loaded.session->AutomationError().empty(),
                "Finite but semantically invalid values must also retain the last valid value.");
    }

    void TestEditorCompiledSpeedBounds()
    {
        const auto effects = ParseRegressionEffects(R"([Interpolation]
Dip: 1-3*u*(1-u)
[Speed]
1,0/1,Area,9,0/1,#ScrollSpeed Separate,From=1,To=1,Curve=Dip
)");
        auto pattern = RegressionPattern();
        pattern.notes.push_back({{4, {}}, 1});
        chart::ChartEditor editor(pattern, effects);
        auto loaded = mode::TaikoMode{}.CreateSession(pattern, effects);
        Require(loaded.Succeeded() && editor.MinimumScrollMultiplier() == .25 &&
                    loaded.session->MinimumScrollMultiplier() == .25,
                "A custom curve minimum at an actual note must match gameplay, not its endpoints.");
    const auto end = rhythm::RhythmTime{
        static_cast<long long>((1848 - 230 + 110) / (.0004896 * editor.MinimumScrollMultiplier()))};
        Require(editor.NotesInTimeRange({}, end).size() == 1,
                "The slow note at 8s must be retained in the visible realtime query.");
        chart::ChartEditor copied(editor);
        Require(copied.MinimumScrollMultiplier() == .25, "Copy must rebuild the speed index.");
        const auto id = editor.Notes().front().note.sourceOrder;
        editor.DeleteNote(id);
        Require(editor.MinimumScrollMultiplier() == 1, "Deleting the minimum-speed note must update the index.");
        editor.AddNote({4, {}}, 1);
        Require(editor.MinimumScrollMultiplier() == .25, "Insertion must index the compiled speed.");
        auto changed = effects;
        changed.interpolations.at("Dip") = "1";
        editor.ReplaceEffects(changed);
        Require(editor.MinimumScrollMultiplier() == 1, "Replacing formulas must replace cached note speeds.");
        editor.ReplaceEffects(effects);
        editor.ReplaceTiming({{{2, {}}, chart::TimingDirectiveType::Bpm, 240}});
        Require(editor.MinimumScrollMultiplier() == editor.Notes().front().scrollMultiplier &&
                    editor.MinimumScrollMultiplier() != .25,
                "Tempo edits must rebuild time-dependent note speed bounds transactionally.");
    }

    void TestEffectValidationConsistency()
    {
        const auto pattern = RegressionPattern();
        const auto base = ParseRegressionEffects("[Sounds]\n1,0/1,#Volume HitSound,Value=1\n");
        std::vector<chart::EffectDocument> invalid;
        invalid.push_back(base);
        invalid.back().commands.front().beginValue = invalid.back().commands.front().endValue = -1;
        invalid.push_back(base);
        invalid.back().commands.front().target = "Music";
        invalid.push_back(base);
        invalid.back().commands.front().position.fraction = {1, 1};
        invalid.push_back(base);
        invalid.back().commands.front().durationMilliseconds = -1;
        invalid.push_back(base);
        invalid.back().commands.front().durationMilliseconds = std::numeric_limits<double>::max();
        invalid.push_back(base);
        invalid.back().commands.front().endValue = std::numeric_limits<double>::quiet_NaN();
        for (const auto &effects : invalid)
        {
            bool rejected = false;
            try
            {
                chart::ChartEditor editor(pattern, effects);
            }
            catch (const std::invalid_argument &)
            {
                rejected = true;
            }
            Require(rejected && !mode::TaikoMode{}.CreateSession(pattern, effects).Succeeded(),
                    "Editor and direct gameplay construction must reject the same invalid effect data.");
        }
        auto effects = ParseRegressionEffects(R"([Interpolation]
Ramp: From+(To-From)*u
[Sounds]
1,0/1,Area,2,0/1,#Volume HitSound,From=0,To=1,Curve=Ramp
)");
        effects.interpolations.at("Ramp") = "From+(To-From)*u*u";
        chart::ChartEditor editor(pattern, effects);
        auto loaded = mode::TaikoMode{}.CreateSession(pattern, effects);
        Require(loaded.Succeeded() && editor.EffectValueAt(chart::EffectCommandType::BusVolume, {0, {1, 2}}) == .25 &&
                    loaded.session->EvaluateAutomation(rhythm::RhythmTime{1'000'000}).front().value == .25,
                "Editor and gameplay must recompile edited formulas from the same document.");
    }

void TestYmeSyntax()
{
    chart::ChartParser parser;
    const auto parse = [&parser](std::string_view body) {
        return parser.ParseEffect("Version: 1\n" + std::string(body), "syntax.yme");
    };
    const auto valid = parse(R"(
[Speed]
1, 0/4, #ScrollSpeed Whole, Value=2
2, 0/4, Area, 3, 0/4, #ScrollSpeed Separate, From=1, To=2, Curve=Soft
[Interpolation]
Soft: From + (To - From) * Bezier(u, 0.25, 0.1, 0.25, 1)
[HitSounds]
1: Sounds/don.wav
[Sounds]
1, 0/4, #HitSound Don 1
[Zone]
1, 0/4, #Syncopation ON, Exclude=1|2|4
2, 0/4, #Syncopation OFF
1, 0/4, #Kiai ON
2, 0/4, #Kiai OFF
)");
    Require(valid.Succeeded() && valid.document.commands.size() == 6 && valid.document.hitSoundChanges.size() == 1,
            "The five-section YME must parse points, Areas, forward curve "
            "references and zone flags.");
    const auto saved = chart::ChartEditor::WriteEffects(valid.document);
    const auto roundtrip = parser.ParseEffect(saved);
    Require(roundtrip.Succeeded() && chart::ChartEditor::WriteEffects(roundtrip.document) == saved,
            "YME tables, expressions, flags and excluded beats must round-trip "
            "without loss.");
    chart::ChartEditor edited({}, valid.document);
    auto changed = edited.Effects();
    changed.interpolations.at("Soft") = "From + (To - From) * u * u";
    edited.Replace(edited.Pattern(), changed);
    Require(std::abs(edited.EffectValueAt(chart::EffectCommandType::NoteSpeed, {1, chart::Rational{1, 2}}) - 1.25) <
                1e-9,
            "Editing a registered formula must recompile the shared curve "
            "instead of retaining the old program.");
    Require(parse("[Sounds]\n1,0/4,#Volume TickSound,Value=0.5\n1,0/4,#Volume "
                  "UserInputFeedback,Value=0.5")
                .Succeeded(),
            "Volume may target long-note ticks and empty-input hit sounds.");
    for (const std::string body : {"[Effects]\n0/4,#ScrollSpeed,,1,2,0,Linear",
                                   "[Speed]\n1,0/4,2,0/4,#ScrollSpeed Whole,From=1,To=2,Curve=Linear",
                                   "[Speed]\n1,0/4,Area,1,0/4,#ScrollSpeed Whole,From=1,To=2,Curve=Linear",
                                   "[Speed]\n1,0/4,#ScrollSpeed BMS,Value=1",
                                   "[Speed]\n1,0/4,#ScrollSpeed Whole,Value=0",
                                   "[Sounds]\n1,0/4,#ScrollSpeed Whole,Value=1",
                                   "[Sounds]\n1,0/4,#Volume HitSound,Value=nan",
                                   "[Sounds]\n1,0/4,#Volume Music,Value=0.5",
                                   "[Sounds]\n1,0/4,#Volume UI,Value=0.5",
                                   "[Sounds]\n1,0/4,#Volume HitSound,Value=-0.5",
                                   "[Sounds]\n1,0/4,Area,2,0/4,#Volume HitSound,From=1,To=0,Curve=Harmonic",
                                   "[Sounds]\n1,0/4,Area,2,0/4,#Volume HitSound,From=1,To=2,Curve=Unknown",
                                   "[Interpolation]\nBad: From + unknown(u)",
                                   "[Interpolation]\nLinear: From",
                                   R"([Interpolation]
Bad: From / (u-u)
[Sounds]
1,0/4,Area,2,0/4,#Volume HitSound,From=1,To=2,Curve=Bad)",
                                   R"([Interpolation]
Bad: From+(To-From)*Bezier(u,-1,0,1,1)
[Speed]
1,0/4,Area,2,0/4,#ScrollSpeed Whole,From=1,To=2,Curve=Bad)",
                                   "[Zone]\n1,0/4,#Syncopation ON,Exclude=0",
                                   "[Zone]\n1,0/4,Area,2,0/4,#Kiai ON",
                                   "[Speed]\n1,0/4,#ScrollSpeed Whole,Value=1,Value=2",
                                   "[HitSounds]\n1: a.wav\n1: b.wav",
                                   "[Sounds]\n1,0/4,#HitSound Kat missing",
                                   "[Speed]\n--"})
        Require(!parse(body).Succeeded(), "Invalid YME must report an error: " + body);
    Require(!parser.ParsePattern("[HitSounds]\n1: a.wav").Succeeded(), "YMP must not own a hit-sound table.");
    chart::EffectCommand c;
    c.beginValue = 1;
    c.endValue = 4;
    c.curve = chart::AutomationCurve::Exponential;
    Require(std::abs(chart::EvaluateInterpolation(c, .5) - 2) < 1e-12,
            "Exponential must interpolate values geometrically.");
    c.curve = chart::AutomationCurve::Harmonic;
    Require(std::abs(chart::EvaluateInterpolation(c, .5) - 1.6) < 1e-12,
            "Harmonic must match reciprocal RPG interpolation.");
    chart::InterpolationExpression expression("From+(To-From)*Bezier(u,0.25,0.1,0.25,1)");
    Require(std::abs(expression.Evaluate(.5, 0, 1) - .8024033876) < 1e-9,
            "Bezier must solve X(time) before evaluating Y, rather than using u "
            "as its parameter.");
    rhythm::AccuracyRange normal("level100", 100);
    rhythm::AccuracyRange sync("sync100", 100, rhythm::AccuracyRange::DefaultLevel50Bands(),
                               rhythm::RhythmDuration{10'000});
    for (std::size_t i = 0; i < normal.Bands().size(); ++i)
        Require(sync.Bands()[i].halfWindow - normal.Bands()[i].halfWindow == rhythm::RhythmDuration{10'000},
                "Syncopation padding must remain +10ms after JudgeLevel scaling.");
}

void TestYmeEffects(const std::filesystem::path &songsRoot)
{
    const auto path = songsRoot / "Patterns/angeldream/YME Effects Verification/angeldream [effects test].ymp";
    chart::ChartParser parser;
    const auto pattern = parser.ParsePatternFile(path);
    const auto effectPath = path.parent_path() / pattern.document.effectFile;
    const auto effects = parser.ParseEffectFile(effectPath);
    Require(pattern.Succeeded() && effects.Succeeded(), "The actual YMP/YME effect fixture must parse.");
    auto loaded = mode::TaikoMode{}.LoadSession(path);
    Require(loaded.Succeeded(), "YMP Effect file must resolve without an explicit caller override.");
    auto &session = *loaded.session;
    const auto &notes = session.Gear().Lanes().front()->Notes();
    Require(notes.size() == 15, "The effect fixture must contain all 15 logical notes.");
    Require(session.HitSoundFiles().at("Chart.HitSound.1") ==
                (effectPath.parent_path() / "Sounds/don.wav").lexically_normal(),
            "Sound paths must use the YME directory, which differs from the YMP "
            "directory.");
    const auto &normal = notes[0]->Profile();
    const auto &sync = notes[1]->Profile();
    Require(&normal != &sync && sync.Id() == "Taiko.Syncopation",
            "Eligible notes must reference the dedicated AccuracyRange object.");
    for (const std::size_t index : {1u, 3u, 5u, 7u})
        Require(&notes[index]->Profile() == &sync, "Syncopated taps and Buzz must share one range, including the "
                                                   "long-note head.");
    for (const std::size_t index : {0u, 2u, 4u, 6u, 8u, 14u})
        Require(&notes[index]->Profile() == &normal, "Quarter/eighth/sixteenth ticks and OFF-region notes must retain "
                                                     "the default range.");
    for (const std::size_t index : {0u, 1u})
    {
        auto isolated = pattern.document;
        isolated.notes = {pattern.document.notes[index]};
        auto trial = mode::TaikoMode{}.CreateSession(isolated, effects.document);
        const auto &note = trial.session->Gear().Lanes().front()->Notes().front();
        const auto late =
            note->Timing() + normal.HalfWindow(rhythm::JudgementGrade::Good) + rhythm::RhythmDuration{9'000};
        const auto hit = trial.session->ProcessInput(index == 0 ? 'F' : 'D', rhythm::InputEdge::Pressed, late);
        Require(!hit.events.empty() && hit.events.back().judgement.grade ==
                                           (index == 0 ? rhythm::JudgementGrade::Bad : rhythm::JudgementGrade::Good),
                "Real rule input must use the expanded range only on non-excluded "
                "notes.");
    }
    for (std::size_t i = 0; i < normal.Bands().size(); ++i)
    {
        const auto &band = sync.Bands()[i];
        Require(band.halfWindow == normal.Bands()[i].halfWindow + rhythm::RhythmDuration{10'000},
                "Every window must expand by +10ms per side.");
        for (const long long sign : {-1LL, 1LL})
        {
            const auto boundary = rhythm::RhythmTime{sign * band.halfWindow.count()};
            Require(sync.Evaluate({}, boundary).grade == band.grade &&
                        sync.Evaluate({}, boundary + rhythm::RhythmDuration{sign}).grade != band.grade,
                    "Expanded grade boundaries must be inclusive, and reject the "
                    "following microsecond.");
        }
    }
    const auto &timeline = session.Timeline();
    const auto time = [&timeline](long long measure) { return timeline.Compile({measure - 1, {}}); };
    Require(!session.IsKiaiActive(time(3) - rhythm::RhythmDuration{1}) && session.IsKiaiActive(time(3)) &&
                session.IsKiaiActive(time(7) - rhythm::RhythmDuration{1}) && !session.IsKiaiActive(time(7)) &&
                session.IsKiaiActive(time(4)),
            "Kiai must honor ON/OFF boundaries and backward queries without "
            "adding display/score behavior.");
    const auto middle = time(3) + (time(4) - time(3)) / 2;
    Require(std::abs(session.ScrollDistance(time(4), time(3)) - static_cast<long double>((time(4) - time(3)).count()) * 1.5L) < .01L,
            "Whole Linear ramp must integrate velocity to preserve continuous "
            "note positions.");
    const auto separate = session.FindNotePresentation(notes[1]->Id())->scrollMultiplier;
    Require(std::abs(separate - (1 + .5 / 12)) < 1e-6 &&
                session.FindNotePresentation(notes[7]->Id())->scrollMultiplier == 2,
            "Separate must assign note/head multipliers without baking Whole "
            "into them.");
    Require(std::abs(session.ScrollDistance(time(6), middle - rhythm::RhythmDuration{1}) -
                     session.ScrollDistance(time(6), middle + rhythm::RhythmDuration{1}) - 3) < .01,
            "Scroll coordinates must remain continuous as time advances through a "
            "ramp.");
    const auto volumeAt = [&session](rhythm::RhythmTime at, std::string_view target) {
        for (const auto &v : session.EvaluateAutomation(at))
            if (v.type == chart::EffectCommandType::BusVolume && v.target == target)
                return v.value;
        throw std::runtime_error("Missing volume automation.");
    };
    Require(std::abs(volumeAt(time(5) + (time(7) - time(5)) / 2, "HitSound") - .5) < 1e-6 &&
                volumeAt(time(10), "HitSound") == 1,
            "Hit-sound Volume Exponential must use elapsed time and retain To.");
    Require(std::abs(volumeAt(time(7) + (time(8) - time(7)) / 2, "HitSound") - 2.0 / 3) < 1e-6,
            "Volume Harmonic must use reciprocal interpolation.");
    auto cues = session.ProcessInput('F', rhythm::InputEdge::Pressed, time(2));
    Require(cues.audioCues.front().sound == "Chart.HitSound.2", "Timed Don change must route to the YME sound index.");
    session.Reset();
    cues = session.ProcessInput('F', rhythm::InputEdge::Pressed, notes[8]->Timing());
    Require(cues.audioCues.back().sound == "Chart.HitSound.1",
            "Explicit note sound must retain precedence over timed defaults.");
    chart::ChartEditor editor(pattern.document, effects.document);
    Require(std::abs(editor.ScrollDistance(time(8), time(4)) - session.ScrollDistance(time(8), time(4))) < .01L,
            "Editor realtime coordinates must share the gameplay Whole-scroll "
            "calculation.");
    std::cout << "YME effects verified: 15 notes; shared +10ms "
                 "ranges/exclusions; Whole/Separate; all curves; "
                 "sound/volume; Kiai.\n";
}
} // namespace finger_drum::tests
