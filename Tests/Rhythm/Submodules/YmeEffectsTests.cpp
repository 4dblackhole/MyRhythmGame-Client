#include "Automation/InterpolationExpression.h"
#include "Automation/ScrollAutomation.h"
#include "TestSupport.h"

namespace finger_drum::tests
{
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
                                   "[Interpolation]\nBad: From / (u-u)\n[Sounds]\n1,0/4,Area,2,0/4,#Volume "
                                   "HitSound,From=1,To=2,Curve=Bad",
                                   "[Interpolation]\nBad: "
                                   "From+(To-From)*Bezier(u,-1,0,1,1)\n[Speed]\n1,0/4,Area,2,0/"
                                   "4,#ScrollSpeed "
                                   "Whole,From=1,To=2,Curve=Bad",
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
    const auto path = songsRoot / "Pattern/angeldream/angeldream [effects test].ymp";
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
    Require(std::abs(session.ScrollDistance(time(4), time(3)) - (time(4) - time(3)).count() * 1.5L) < .01L,
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
