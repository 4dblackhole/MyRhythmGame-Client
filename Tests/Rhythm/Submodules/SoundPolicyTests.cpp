#include "TestSupport.h"

namespace finger_drum::tests
{
    void TestIndexedHitSoundChanges()
    {
        using namespace finger_drum;
        chart::ChartParser parser;
        const auto parsed = parser.ParsePattern(R"(
Base BPM: 120
[HitSounds]
1: Sounds/pop.wav
2: Sounds/kat.wav
[Time Signature]
--
#measure 3/4
1/4, #bpm 180
)",
                                                "Songs/Pattern/test.ymp");
        Require(parsed.Succeeded(), "Indexed hit sound table must parse.");
        const auto effects = parser.ParseEffect(R"(
Version: 1
[HitSound Changes]
4, 2/4, 1, 2
5, 0/4, 2, 1
5, 0/4, 2, 2
)",
                                                "Songs/Pattern/test.yme");
        Require(effects.Succeeded() && effects.document.hitSoundChanges.size() == 3 &&
                    effects.document.hitSoundChanges.front().position.measure == 3,
                "Explicit YME measures must be one-based, with independently targeted key IDs.");
        mode::TaikoMode mode;
        auto loaded = mode.CreateSession(parsed.document, effects.document);
        Require(loaded.Succeeded(), "Valid indexed changes must load.");
        Require(loaded.session->HitSoundFiles().at("Chart.HitSound.1") ==
                    std::filesystem::path("Songs/Pattern/Sounds/pop.wav"),
                "Hit sound paths must be relative to the YMP, not the working directory.");
        chart::MusicalTimeline timeline(parsed.document);
        const auto first = timeline.Compile({3, {2, 4}});
        const auto second = timeline.Compile({4, {0, 4}});
        auto sound = [&](rhythm::PhysicalKey key, rhythm::RhythmTime time) {
            const auto result = loaded.session->ProcessInput(key, rhythm::InputEdge::Pressed, time);
            Require(result.audioCues.size() == 1, "One input must produce one cue.");
            return result.audioCues.front().sound;
        };
        Require(sound('D', first - rhythm::RhythmDuration{1}) == "Taiko.Kat.FreeInput",
                "Change must not apply a microsecond early.");
        Require(sound('D', first) == "Chart.HitSound.1" &&
                    sound('F', first) == "Taiko.Don.FreeInput",
                "Kat-only change must apply at the exact compiled BPM/measure boundary.");
        Require(sound('D', second) == "Chart.HitSound.2" &&
                    sound('F', second) == "Chart.HitSound.2",
                "Two changes at one position must change both actions independently.");
        Require(sound('D', second + rhythm::RhythmDuration{10'000'000}) == "Chart.HitSound.2",
                "The last change must persist.");
        loaded.session->Reset();
        Require(sound('D', first - rhythm::RhythmDuration{1}) == "Taiko.Kat.FreeInput",
                "Reset/backward seeking must restore earlier defaults without stale state.");

        for (const std::string row : {"0, 2/4, 1, 2", "4, 2/ 4, 1, 2", "4, 2/4, 1, 3", "4, 2/4, 1",
                                      "4, 2/4, , 2", "4, 2/0, 1, 2", "4, -1/4, 1, 2"})
        {
            Require(!parser.ParseEffect("[HitSound Changes]\n" + row).Succeeded(),
                    "Malformed indexed hit sound rows must be rejected: " + row);
        }
        const auto unknown = parser.ParseEffect("[HitSound Changes]\n4, 2/4, 999, 2");
        Require(!mode.CreateSession(parsed.document, unknown.document).Succeeded(),
                "Undefined table references must fail chart loading.");
        const auto outside = parser.ParseEffect("[HitSound Changes]\n4, 3/4, 1, 2");
        Require(!mode.CreateSession(parsed.document, outside.document).Succeeded(),
                "A change at or outside the measure end must be rejected.");
        Require(!parser.ParsePattern("[HitSounds]\n1: a.wav\n1: b.wav").Succeeded(),
                "Duplicate table entries must not be silently discarded.");

        // Explicit note assignments override timed Don/Kat defaults.
        auto pattern = parsed.document;
        pattern.notes.push_back({{3, {2, 4}}, 2, 0, "2"});
        auto explicitNote = mode.CreateSession(pattern, effects.document);
        const auto noteResult =
            explicitNote.session->ProcessInput('D', rhythm::InputEdge::Pressed, first);
        Require(noteResult.audioCues.size() == 1 &&
                    noteResult.audioCues.front().sound == "Chart.HitSound.2",
                "Explicit per-note hit sound must take precedence.");

        // Both big note first-hit sounds and Kat roll ticks follow Kat changes.
        for (int type : {4, 11, 12, 13, 14})
        {
            pattern = parsed.document;
            pattern.notes.push_back({{3, {2, 4}}, type, type == 4 ? 0 : 1});
            if (type != 4)
                pattern.notes.push_back({{4, {1, 4}}, type, 2});
            auto special = mode.CreateSession(pattern, effects.document);
            Require(special.Succeeded(), "Special-note sound test must load.");
            const auto hit = special.session->ProcessInput('D', rhythm::InputEdge::Pressed, first);
            Require(hit.audioCues.size() == 1 && hit.audioCues.front().sound == "Chart.HitSound.1",
                    "BigKat and Kat roll inputs must use the current Kat sound.");
        }
    }

    void TestSpecialNoteSoundOverridePriority()
    {
        for (const int type : {15, 16, 17})
        {
            chart::PatternDocument pattern;
            pattern.hitSounds["1"] = "explicit.wav";
            pattern.hitSounds["2"] = "timed.wav";
            pattern.notes.push_back(
                {{0, {}}, type, 1, "1", {"HitCount=8", "Action=Don", "TickDivision=16"}});
            pattern.notes.push_back({{0, {1, 2}}, type, 2});
            chart::EffectDocument effects;
            effects.hitSoundChanges.push_back({{0, {}}, "2", 1});
            auto loaded = mode::TaikoMode{}.CreateSession(pattern, effects);
            Require(loaded.Succeeded(), "Special sound priority chart must load.");
            const auto head =
                loaded.session->ProcessInput('F', rhythm::InputEdge::Pressed, rhythm::RhythmTime{});
            Require(head.audioCues.size() == 1 &&
                        head.audioCues.front().sound == "Chart.HitSound.1",
                    "Balloon, DengDeng and Buzz explicit sounds must override timed Don defaults.");
            if (type == 17)
            {
                const std::array<rhythm::PhysicalKey, 1> held{'F'};
                const auto tick = loaded.session->Update(rhythm::RhythmTime{125'000}, held);
                Require(tick.audioCues.size() == 1 &&
                            tick.audioCues.front().sound == "Chart.HitSound.1",
                        "Buzz body ticks must retain the explicit sound assignment.");
            }
        }
    }
} // namespace finger_drum::tests
