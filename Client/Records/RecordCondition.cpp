#include "RecordCondition.h"
#include <algorithm>
#include <cctype>
#include <charconv>
#include <cmath>
#include <cstdint>
#include <stdexcept>
#include <unordered_map>

namespace
{
    class Parser
    {
      public:
        explicit Parser(std::string_view source) : source_(source)
        {
            Next();
        }
        RecordCondition Run()
        {
            if (token_.empty())
                return {"1", {}};
            output_.sql = Expression(0);
            if (!token_.empty())
                Fail();
            return std::move(output_);
        }

      private:
        [[noreturn]] static void Fail()
        {
            throw std::invalid_argument(
                "Invalid record query. Use option comparisons with AND, OR, NOT and parentheses.");
        }
        void Next()
        {
            while (!source_.empty() && std::isspace(static_cast<unsigned char>(source_.front())))
                source_.remove_prefix(1);
            token_.clear();
            if (source_.empty())
                return;
            const char first = source_.front();
            if (std::isalnum(static_cast<unsigned char>(first)) || first == '.' || first == '-' || first == '+')
            {
                while (!source_.empty())
                {
                    const auto ch = static_cast<unsigned char>(source_.front());
                    if (!std::isalnum(ch) && ch != '.' && ch != '_' && ch != '-' && ch != '+')
                        break;
                    token_ += static_cast<char>(std::tolower(ch));
                    source_.remove_prefix(1);
                }
            }
            else
            {
                token_ += first;
                source_.remove_prefix(1);
                if (!source_.empty() && (first == '<' || first == '>' || first == '=' || first == '!') &&
                    (source_.front() == '=' || (first == '<' && source_.front() == '>')))
                {
                    token_ += source_.front();
                    source_.remove_prefix(1);
                }
            }
        }
        std::string Expression(unsigned depth)
        {
            auto left = Conjunction(depth);
            while (token_ == "or")
            {
                Next();
                left = "(" + left + " OR " + Conjunction(depth) + ")";
            }
            return left;
        }
        std::string Conjunction(unsigned depth)
        {
            auto left = Term(depth);
            while (token_ == "and")
            {
                Next();
                left = "(" + left + " AND " + Term(depth) + ")";
            }
            return left;
        }
        std::string Term(unsigned depth)
        {
            if (depth >= 32 || ++terms_ > 128)
                Fail();
            if (token_ == "not")
            {
                Next();
                return "(NOT " + Term(depth + 1) + ")";
            }
            if (token_ == "(")
            {
                Next();
                auto value = Expression(depth + 1);
                if (token_ != ")")
                    Fail();
                Next();
                return "(" + value + ")";
            }
            // Fixed identifiers are the only SQL text produced from a name.
            static const std::unordered_map<std::string, std::pair<std::string, char>> fields{
                {"judgelevel", {"JudgeLevel", 'n'}},
                {"random", {"Random", 'b'}},
                {"mirror", {"Mirror", 'b'}},
                {"metronome", {"Metronome", 'b'}},
                {"accelerate", {"Accelerate", 'n'}},
                {"scrollspeed", {"ScrollSpeed", 'n'}},
                {"scrollunit", {"ScrollUnit", 'n'}},
                {"scrollbpm", {"CASE WHEN ScrollUnit=1 THEN ScrollSpeed ELSE NULL END", 'n'}},
                {"ignorescrolleffects", {"IgnoreScrollEffects", 'b'}},
                {"randomprobability", {"RandomProbability", 'n'}},
                {"randomseed", {"RandomSeed", 's'}},
                {"preservemusicpitch", {"PreserveMusicPitch", 'b'}},
                {"preservehitsoundpitch", {"PreserveHitSoundPitch", 'b'}},
                {"judgeoverride", {"JudgeOverride", 'b'}},
                {"completed", {"Completed", 'b'}},
                {"cleared", {"Cleared", 'b'}},
                {"pausemenuused", {"PauseMenuUsed", 'b'}}};
            const auto field = fields.find(token_);
            if (field == fields.end())
                throw std::invalid_argument("Unknown record query field: " + token_);
            const auto name = field->second.first;
            const auto kind = field->second.second;
            Next();
            const auto op = token_;
            if (op != "=" && op != "==" && op != "!=" && op != "<>" && op != ">" && op != ">=" && op != "<" &&
                op != "<=")
                Fail();
            if (kind != 'n' && op != "=" && op != "==" && op != "!=" && op != "<>")
                Fail();
            Next();
            std::string value = token_;
            if (kind == 'b')
            {
                if (value == "true")
                    value = "1";
                else if (value == "false")
                    value = "0";
                else if (value != "0" && value != "1")
                    Fail();
            }
            else if (kind == 's')
            {
                std::uint64_t seed{};
                const auto parsed = std::from_chars(value.data(), value.data() + value.size(), seed);
                if (parsed.ec != std::errc{} || parsed.ptr != value.data() + value.size())
                    Fail();
                value = std::to_string(seed);
            }
            else
            {
                double number{};
                const auto parsed = std::from_chars(value.data(), value.data() + value.size(), number);
                if (parsed.ec != std::errc{} || parsed.ptr != value.data() + value.size() || !std::isfinite(number))
                    Fail();
            }
            output_.values.push_back(std::move(value));
            Next();
            return "(" + name + " " + op + (kind == 's' ? " ?" : " CAST(? AS REAL)") + ")";
        }
        std::string_view source_;
        std::string token_;
        unsigned terms_{};
        RecordCondition output_;
    };
} // namespace
RecordCondition RecordCondition::Parse(std::string_view condition)
{
    if (condition.size() > 2048)
        throw std::invalid_argument("Record query is too long (maximum 2048 characters).");
    return Parser(condition).Run();
}
