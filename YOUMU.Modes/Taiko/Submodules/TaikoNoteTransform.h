#pragma once
#include "Mode/PlayOptions.h"
#include "TaikoNoteDefinition.h"
#include "TaikoNoteOptions.h"
#include <random>

// Called once per accepted logical note, after long-note pairing. Mirror is
// applied first; Random then attempts a uniform assignment within that group.
class TaikoNoteTransform final
{
  public:
    explicit TaikoNoteTransform(const PlayOptions &options) : options_(options), random_(options.randomSeed)
    {
    }
    void Apply(PatternNote &note)
    {
        const auto type = static_cast<TaikoNoteType>(note.keyType);
        const bool small = type == TaikoNoteType::Don || type == TaikoNoteType::Kat;
        const bool big = type == TaikoNoteType::BigDon || type == TaikoNoteType::BigKat;
        const bool buzz = type == TaikoNoteType::Buzz;
        if (!small && !big && !buzz)
            return;
        bool kat = type == TaikoNoteType::Kat || type == TaikoNoteType::BigKat;
        if (buzz)
        {
            const auto action = FindOption(note, Action);
            if (!action || (!EqualsInsensitive(*action, "Don") && !EqualsInsensitive(*action, "Kat")))
                return;
            kat = EqualsInsensitive(*action, "Kat");
        }
        if (options_.mirror)
            kat = !kat;
        if (options_.random)
        {
            // Defined draws avoid implementation-specific uniform distributions.
            const double draw = static_cast<double>(random_() >> 11) * 0x1.0p-53;
            if (draw < options_.randomAttemptProbability)
                kat = (random_() & 1) != 0;
        }
        if (buzz)
        {
            for (auto &field : note.extraData)
            {
                const auto equals = field.find('=');
                if (equals != std::string::npos &&
                    EqualsInsensitive(Trim(std::string_view(field).substr(0, equals)), Action))
                    field = kat ? "Action=Kat" : "Action=Don";
            }
        }
        else
            note.keyType = static_cast<int>(big ? (kat ? TaikoNoteType::BigKat : TaikoNoteType::BigDon)
                                                : (kat ? TaikoNoteType::Kat : TaikoNoteType::Don));
    }

  private:
    const PlayOptions &options_;
    std::mt19937_64 random_;
};
