#include "Taiko/TaikoMode.h"

#include <algorithm>
#include <charconv>
#include <cctype>
#include <map>
#include <memory>
#include <optional>
#include <string_view>
#include <utility>

namespace finger_drum::mode
{
    namespace
    {
        [[nodiscard]] rhythm::NoteAction ActionValue(
            const TaikoAction action) noexcept
        {
            return static_cast<rhythm::NoteAction>(action);
        }

        [[nodiscard]] rhythm::AudioCueRequest Cue(
            std::string sound,
            std::string bus = "HitSound")
        {
            rhythm::AudioCueRequest result;
            result.sound = std::move(sound);
            result.bus = std::move(bus);
            return result;
        }

        [[nodiscard]] bool IsDonType(const TaikoNoteType type) noexcept
        {
            return type == TaikoNoteType::Don ||
                type == TaikoNoteType::BigDon;
        }

        [[nodiscard]] std::string_view Trim(
            std::string_view value) noexcept
        {
            while (!value.empty() &&
                std::isspace(static_cast<unsigned char>(value.front())) != 0)
            {
                value.remove_prefix(1);
            }
            while (!value.empty() &&
                std::isspace(static_cast<unsigned char>(value.back())) != 0)
            {
                value.remove_suffix(1);
            }
            return value;
        }

        [[nodiscard]] bool EqualsInsensitive(
            const std::string_view left,
            const std::string_view right) noexcept
        {
            return left.size() == right.size() &&
                std::ranges::equal(
                    left,
                    right,
                    [](const char a, const char b)
                    {
                        return std::tolower(static_cast<unsigned char>(a)) ==
                            std::tolower(static_cast<unsigned char>(b));
                    });
        }

        [[nodiscard]] std::optional<std::string_view> FindOption(
            const chart::PatternNote& note,
            const std::string_view name) noexcept
        {
            for (const std::string& field : note.extraData)
            {
                const std::string_view view = field;
                const std::size_t equals = view.find('=');
                if (equals != std::string_view::npos &&
                    EqualsInsensitive(Trim(view.substr(0, equals)), name))
                {
                    return Trim(view.substr(equals + 1));
                }
            }
            return std::nullopt;
        }

        [[nodiscard]] bool ParsePositiveSize(
            const std::string_view value,
            std::size_t& output) noexcept
        {
            const char* const begin = value.data();
            const char* const end = begin + value.size();
            const auto parsed = std::from_chars(begin, end, output);
            return parsed.ec == std::errc{} && parsed.ptr == end &&
                output > 0;
        }

        [[nodiscard]] std::optional<std::size_t> ReadPositiveOption(
            const chart::PatternNote& note,
            const std::string_view name,
            const std::optional<std::size_t> fallback,
            std::vector<chart::Diagnostic>& diagnostics)
        {
            const std::optional<std::string_view> value =
                FindOption(note, name);
            if (!value.has_value())
            {
                if (fallback.has_value())
                {
                    return fallback;
                }
                diagnostics.push_back({
                    chart::DiagnosticSeverity::Error,
                    note.source,
                    std::string(name) +
                        " is required for this long note."});
                return std::nullopt;
            }

            std::size_t parsed{};
            if (!ParsePositiveSize(*value, parsed) || parsed > 1024)
            {
                diagnostics.push_back({
                    chart::DiagnosticSeverity::Error,
                    note.source,
                    std::string(name) +
                        " must be an integer from 1 through 1024."});
                return std::nullopt;
            }
            return parsed;
        }

        [[nodiscard]] std::optional<TaikoAction> ReadBuzzAction(
            const chart::PatternNote& note,
            std::vector<chart::Diagnostic>& diagnostics)
        {
            const std::optional<std::string_view> value =
                FindOption(note, "Action");
            if (value.has_value() && EqualsInsensitive(*value, "Don"))
            {
                return TaikoAction::Don;
            }
            if (value.has_value() && EqualsInsensitive(*value, "Kat"))
            {
                return TaikoAction::Kat;
            }
            diagnostics.push_back({
                chart::DiagnosticSeverity::Error,
                note.source,
                "Buzz requires Action=Don or Action=Kat."});
            return std::nullopt;
        }

        [[nodiscard]] std::string_view LongNoteVisualId(
            const TaikoNoteType type,
            const std::optional<TaikoAction> buzzAction = std::nullopt) noexcept
        {
            if (type == TaikoNoteType::BigRoll ||
                type == TaikoNoteType::BigTickRoll)
            {
                return "Taiko.BigRoll";
            }
            if (type == TaikoNoteType::Balloon)
            {
                return "Taiko.Balloon";
            }
            if (type == TaikoNoteType::DengDeng)
            {
                return "Taiko.DengDeng";
            }
            if (type == TaikoNoteType::Buzz)
            {
                return buzzAction == TaikoAction::Kat
                    ? "Taiko.Buzz.Kat"
                    : "Taiko.Buzz.Don";
            }
            return "Taiko.Roll";
        }

        [[nodiscard]] std::string_view TapVisualId(
            const TaikoNoteType type) noexcept
        {
            switch (type)
            {
            case TaikoNoteType::Don: return "Taiko.Don";
            case TaikoNoteType::Kat: return "Taiko.Kat";
            case TaikoNoteType::BigDon: return "Taiko.BigDon";
            case TaikoNoteType::BigKat: return "Taiko.BigKat";
            default: return "Taiko.Unknown";
            }
        }
    }

    std::string_view TaikoMode::Id() const noexcept
    {
        return "Taiko";
    }

    ModeLoadResult TaikoMode::LoadSession(
        const std::filesystem::path& patternPath,
        const std::optional<std::filesystem::path>& effectPath) const
    {
        chart::ChartParser parser;
        chart::ParseResult<chart::PatternDocument> pattern =
            parser.ParsePatternFile(patternPath);
        chart::ParseResult<chart::EffectDocument> effect;
        if (effectPath.has_value())
        {
            effect = parser.ParseEffectFile(*effectPath);
        }

        ModeLoadResult result;
        result.diagnostics = std::move(pattern.diagnostics);
        result.diagnostics.insert(
            result.diagnostics.end(),
            std::make_move_iterator(effect.diagnostics.begin()),
            std::make_move_iterator(effect.diagnostics.end()));
        if (std::ranges::any_of(
                result.diagnostics,
                [](const chart::Diagnostic& diagnostic)
                {
                    return diagnostic.severity ==
                        chart::DiagnosticSeverity::Error;
                }))
        {
            return result;
        }

        ModeLoadResult created = CreateSession(
            pattern.document,
            effect.document);
        result.session = std::move(created.session);
        result.diagnostics.insert(
            result.diagnostics.end(),
            std::make_move_iterator(created.diagnostics.begin()),
            std::make_move_iterator(created.diagnostics.end()));
        return result;
    }

    ModeLoadResult TaikoMode::CreateSession(
        const chart::PatternDocument& pattern,
        const chart::EffectDocument& effects) const
    {
        ModeLoadResult result;
        auto session = std::make_unique<PlaySession>();
        rhythm::Lane& lane = session->Gear().CreateLane();
        auto profile = std::make_shared<rhythm::JudgementProfile>(
            "Taiko.Default",
            pattern.judgementLevel);

        chart::MusicalTimeline timeline(pattern);
        const std::vector<chart::CompiledPatternNote> compiled =
            timeline.CompileNotes(pattern);
        // The legacy RPG Lane treated a long note as one active head/tail
        // transaction per lane. Keeping that rule prevents a stray second
        // LNStart from replacing the real head before its matching tail.
        std::optional<chart::CompiledPatternNote> longNoteHead;
        rhythm::NoteId nextId = 1;

        for (const chart::CompiledPatternNote& compiledNote : compiled)
        {
            const chart::PatternNote& source = compiledNote.note;
            const TaikoNoteType type =
                static_cast<TaikoNoteType>(source.keyType);
            const TaikoPatternAction action =
                static_cast<TaikoPatternAction>(source.actionType);

            if (action == TaikoPatternAction::LongNoteStart)
            {
                if (!longNoteHead.has_value())
                {
                    longNoteHead = compiledNote;
                }
                continue;
            }
            if (action == TaikoPatternAction::LongNoteEnd)
            {
                if (!longNoteHead.has_value() ||
                    longNoteHead->note.keyType != source.keyType)
                {
                    continue;
                }
                AddLongNote(
                    *longNoteHead,
                    compiledNote,
                    timeline,
                    profile,
                    lane,
                    *session,
                    nextId,
                    result.diagnostics);
                longNoteHead.reset();
                continue;
            }
            if (action != TaikoPatternAction::Down)
            {
                continue;
            }
            // Match the original RPG Lane transaction: while a long-note
            // head is waiting for its tail, ordinary Down events in that same
            // lane do not create a second focus target inside the roll.
            if (longNoteHead.has_value())
            {
                continue;
            }

            const bool don = IsDonType(type);
            const rhythm::NoteAction requiredAction = ActionValue(
                don ? TaikoAction::Don : TaikoAction::Kat);
            const bool big = type == TaikoNoteType::BigDon ||
                type == TaikoNoteType::BigKat;
            std::unique_ptr<rhythm::INoteRule> rule = big
                ? std::unique_ptr<rhythm::INoteRule>(
                    std::make_unique<rhythm::CountedHitInputRule>(
                        requiredAction,
                        2,
                        rhythm::JudgementGrade::Good))
                : std::unique_ptr<rhythm::INoteRule>(
                    std::make_unique<rhythm::TapInputRule>(requiredAction));
            std::string soundId = source.hitSound;
            if (soundId.empty())
            {
                soundId = big
                    ? (don ? "Taiko.BigDon.FirstHit" : "Taiko.BigKat.FirstHit")
                    : (don ? "Taiko.Don.Hit" : "Taiko.Kat.Hit");
            }
            const rhythm::NoteId noteId = nextId++;
            lane.AddNote(std::make_unique<rhythm::RuleBasedNote>(
                noteId,
                compiledNote.timing,
                profile,
                std::move(rule),
                big
                    ? MakeBigSoundPolicy(std::move(soundId))
                    : MakeTapSoundPolicy(std::move(soundId))));
            session->SetNotePresentation(noteId, {
                std::string(TapVisualId(type)),
                {},
                false});
        }

        session->SetInputMapping({
            {static_cast<rhythm::PhysicalKey>('F'), ActionValue(TaikoAction::Don)},
            {static_cast<rhythm::PhysicalKey>('J'), ActionValue(TaikoAction::Don)},
            {static_cast<rhythm::PhysicalKey>('D'), ActionValue(TaikoAction::Kat)},
            {static_cast<rhythm::PhysicalKey>('K'), ActionValue(TaikoAction::Kat)},
        });
        session->SetFreeInputCue(
            ActionValue(TaikoAction::Don),
            Cue("Taiko.Don.FreeInput", "UserInputFeedback"));
        session->SetFreeInputCue(
            ActionValue(TaikoAction::Kat),
            Cue("Taiko.Kat.FreeInput", "UserInputFeedback"));
        session->SetEffects(timeline.CompileEffects(effects));
        session->Gear().Finalize();
        result.session = std::move(session);
        return result;
    }

    void TaikoMode::AddLongNote(
        const chart::CompiledPatternNote& compiledHead,
        const chart::CompiledPatternNote& compiledTail,
        const chart::MusicalTimeline& timeline,
        const std::shared_ptr<const rhythm::JudgementProfile>& profile,
        rhythm::Lane& lane,
        PlaySession& session,
        rhythm::NoteId& nextId,
        std::vector<chart::Diagnostic>& diagnostics)
    {
        const chart::PatternNote& head = compiledHead.note;
        const chart::PatternNote& tail = compiledTail.note;
        const rhythm::RhythmTime begin = compiledHead.timing;
        const rhythm::RhythmTime end = compiledTail.timing;
        if (end <= begin)
        {
            diagnostics.push_back({
                chart::DiagnosticSeverity::Error,
                tail.source,
                "A long-note end must be later than its start."});
            return;
        }

        // Translate only the Taiko-specific options here. The resulting rule
        // and sound policy remain backend-neutral rhythm objects.
        const TaikoNoteType type = static_cast<TaikoNoteType>(head.keyType);
        std::unique_ptr<rhythm::INoteRule> rule;
        std::shared_ptr<const rhythm::INoteSoundPolicy> soundPolicy;
        std::optional<TaikoAction> buzzAction;
        switch (type)
        {
        case TaikoNoteType::Roll:
        case TaikoNoteType::BigRoll:
            rule = std::make_unique<rhythm::DrumRollInputRule>(
                std::vector<rhythm::NoteAction>{
                    ActionValue(TaikoAction::Don),
                    ActionValue(TaikoAction::Kat)},
                end);
            soundPolicy = MakeTickSoundPolicy(
                head.hitSound.empty()
                    ? "Taiko.LongNote.Tick"
                    : head.hitSound);
            break;
        case TaikoNoteType::TickRoll:
        case TaikoNoteType::BigTickRoll:
            if (const auto division = ReadPositiveOption(
                    head,
                    "TickDivision",
                    16,
                    diagnostics))
            {
                rule = std::make_unique<rhythm::TickRollInputRule>(
                    std::vector<rhythm::NoteAction>{
                        ActionValue(TaikoAction::Don),
                        ActionValue(TaikoAction::Kat)},
                    end,
                    timeline.CompileSubdivisions(
                        head.position,
                        tail.position,
                        *division));
                soundPolicy = MakeTickSoundPolicy(
                    head.hitSound.empty()
                        ? "Taiko.LongNote.Tick"
                        : head.hitSound);
            }
            break;
        case TaikoNoteType::Balloon:
            if (const auto hitCount = ReadPositiveOption(
                    head,
                    "HitCount",
                    std::nullopt,
                    diagnostics))
            {
                rule = std::make_unique<rhythm::TimedSequenceInputRule>(
                    std::vector<rhythm::NoteAction>{
                        ActionValue(TaikoAction::Don)},
                    *hitCount,
                    end);
                soundPolicy = MakeBalloonSoundPolicy();
            }
            break;
        case TaikoNoteType::DengDeng:
            if (const auto hitCount = ReadPositiveOption(
                    head,
                    "HitCount",
                    std::nullopt,
                    diagnostics))
            {
                rule = std::make_unique<rhythm::TimedSequenceInputRule>(
                    std::vector<rhythm::NoteAction>{
                        ActionValue(TaikoAction::Don),
                        ActionValue(TaikoAction::Kat)},
                    *hitCount,
                    end);
                soundPolicy = MakeAlternatingSoundPolicy(*hitCount);
            }
            break;
        case TaikoNoteType::Buzz:
            buzzAction = ReadBuzzAction(head, diagnostics);
            if (const auto division = ReadPositiveOption(
                    head,
                    "TickDivision",
                    16,
                    diagnostics);
                buzzAction.has_value() && division.has_value())
            {
                rule = std::make_unique<rhythm::HoldInputRule>(
                    ActionValue(*buzzAction),
                    end,
                    timeline.CompileSubdivisions(
                        head.position,
                        tail.position,
                        *division));
                soundPolicy = MakeTickSoundPolicy(
                    head.hitSound.empty()
                        ? (*buzzAction == TaikoAction::Don
                            ? "Taiko.Don.Hit"
                            : "Taiko.Kat.Hit")
                        : head.hitSound);
            }
            break;
        default:
            diagnostics.push_back({
                chart::DiagnosticSeverity::Error,
                head.source,
                "The long-note key type is not supported by Taiko mode."});
            return;
        }
        if (rule == nullptr || soundPolicy == nullptr)
        {
            return;
        }

        // One rule-based note owns the complete interval; presentation keeps
        // the tail time separate from its input and scoring rule.
        const rhythm::NoteId noteId = nextId++;
        lane.AddNote(std::make_unique<rhythm::RuleBasedNote>(
            noteId,
            begin,
            profile,
            std::move(rule),
            std::move(soundPolicy)));
        session.SetNotePresentation(noteId, {
            std::string(LongNoteVisualId(type, buzzAction)),
            end,
            true});
    }

    std::shared_ptr<const rhythm::INoteSoundPolicy>
    TaikoMode::MakeTapSoundPolicy(std::string soundId)
    {
        auto policy = std::make_shared<rhythm::MappedNoteSoundPolicy>();
        policy->Bind(rhythm::SoundBinding{
            .eventType = rhythm::NoteEventType::HitAccepted,
            .cue = Cue(std::move(soundId))});
        return policy;
    }

    std::shared_ptr<const rhythm::INoteSoundPolicy>
    TaikoMode::MakeBigSoundPolicy(std::string soundId)
    {
        auto policy = std::make_shared<rhythm::MappedNoteSoundPolicy>();
        policy->Bind(rhythm::SoundBinding{
            .eventType = rhythm::NoteEventType::HitAccepted,
            .maximumGrade = rhythm::JudgementGrade::Good,
            .hitIndex = 0,
            .cue = Cue(std::move(soundId)),
            .stopAfterMatch = true});
        return policy;
    }

    std::shared_ptr<const rhythm::INoteSoundPolicy>
    TaikoMode::MakeTickSoundPolicy(std::string soundId)
    {
        auto policy = std::make_shared<rhythm::MappedNoteSoundPolicy>();
        policy->Bind(rhythm::SoundBinding{
            .eventType = rhythm::NoteEventType::TickAccepted,
            .cue = Cue(std::move(soundId), "TickSound")});
        return policy;
    }

    std::shared_ptr<const rhythm::INoteSoundPolicy>
    TaikoMode::MakeBalloonSoundPolicy()
    {
        auto policy = std::make_shared<rhythm::MappedNoteSoundPolicy>();
        policy->Bind(rhythm::SoundBinding{
            .eventType = rhythm::NoteEventType::HitAccepted,
            .cue = Cue("Taiko.Don.Hit", "TickSound")});
        policy->Bind(rhythm::SoundBinding{
            .eventType = rhythm::NoteEventType::Completed,
            .cue = Cue("Taiko.Balloon.Pop")});
        return policy;
    }

    std::shared_ptr<const rhythm::INoteSoundPolicy>
    TaikoMode::MakeAlternatingSoundPolicy(const std::size_t hitCount)
    {
        auto policy = std::make_shared<rhythm::MappedNoteSoundPolicy>();
        for (std::size_t index = 0; index < hitCount; ++index)
        {
            policy->Bind(rhythm::SoundBinding{
                .eventType = rhythm::NoteEventType::HitAccepted,
                .hitIndex = index,
                .cue = Cue(
                    index % 2 == 0
                        ? "Taiko.Don.Hit"
                        : "Taiko.Kat.Hit",
                    "TickSound"),
                .stopAfterMatch = true});
        }
        return policy;
    }
}
