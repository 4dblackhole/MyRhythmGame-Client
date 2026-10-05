#include "EditorWorkspace.h"
#include "EditorTime.h"
#include "EditorValueParsing.h"
#include "Modes/EditorModeFactory.h"
using namespace editor_values;

void EditorWorkspace::Initialize()
{
    if (!mode_)
        mode_ = CreateEditorMode(request_.mode);
    if (!editor_)
        editor_ = mode_->OpenDocument(request_);
    if (!editor_)
        throw std::runtime_error("Editor mode did not provide a document.");
    audioSourceRevision_ = editor_->AudioSourceRevision();
}
void EditorWorkspace::UpdateAnalysis()
{
    if (analysis_.Update(Document(), *mode_, request_, status_))
        RequestRebuild();
}
void EditorWorkspace::DocumentChanged()
{
    // Resolved path comparison prevents unnecessary worker restarts. Marker
    // invalidation follows document identity/revision, regardless of active tab.
    if (audioSourceRevision_ != Document().AudioSourceRevision())
    {
        audioSourceRevision_ = Document().AudioSourceRevision();
        analysis_.Invalidate();
    }
    RequestRebuild();
}
void EditorWorkspace::Replace(chart::PatternDocument pattern, chart::EffectDocument effects)
{
    editor_->Replace(std::move(pattern), std::move(effects));
    DocumentChanged();
}
void EditorWorkspace::AddNote(chart::MusicalPosition position, int keyType, std::optional<chart::MusicalPosition> end,
                              std::vector<std::string> extra)
{
    editor_->AddNote(position, keyType, end, std::move(extra));
    DocumentChanged();
}
void EditorWorkspace::DeleteNote(std::size_t order)
{
    editor_->DeleteNote(order);
    DocumentChanged();
}
void EditorWorkspace::SetMeasureLength(std::int64_t measure, chart::Rational length)
{
    editor_->SetMeasureLength(measure, length);
    DocumentChanged();
}
void EditorWorkspace::Seek(double milliseconds)
{
    // Validate before publishing; preserve grid seeking beyond music and negative pre-roll.
    static_cast<void>(editor_time::MeasureNearTime(Document().Timeline(), milliseconds));
    timeMs_ = milliseconds;
    RequestRebuild();
}
void EditorWorkspace::SetFirstMeasure(std::int64_t measure)
{
    if (measure < 0)
        throw std::invalid_argument("First measure must be non-negative.");
    score_.firstMeasure = measure;
    RequestRebuild();
}
void EditorWorkspace::SetDivision(std::int64_t division)
{
    if (division < 1 || division > 1024)
        throw std::invalid_argument("Division must be 1..1024.");
    score_.division = static_cast<int>(division);
    RequestRebuild();
}
void EditorWorkspace::SetRealtime(bool realtime)
{
    score_.realtime = realtime;
    RequestRebuild();
}
void EditorWorkspace::SelectTab(EditorTab value)
{
    tab_ = value;
    listOffset = 0;
    mode_->CloseToolMenu();
    RequestRebuild();
}
void EditorWorkspace::SelectTimingPosition(chart::MusicalPosition p)
{
    timingForm.measure = std::to_string(p.measure + 1);
    timingForm.fraction = Fraction(p.fraction);
    SelectTab(EditorTab::Timing);
}
void EditorWorkspace::RemoveTiming(std::size_t row)
{
    auto pattern = Document().Pattern();
    if (row >= pattern.timing.size())
        throw std::out_of_range("Timing row no longer exists.");
    pattern.timing.erase(pattern.timing.begin() + row);
    Replace(std::move(pattern), Document().Effects());
}
void EditorWorkspace::ApplyBpm()
{
    auto pattern = Document().Pattern();
    const auto position = Position(timingForm.measure, timingForm.fraction);
    const auto bpm = Number(timingForm.bpm);
    if (bpm <= 0)
        throw std::invalid_argument("BPM must be positive.");
    std::erase_if(pattern.timing, [position](const auto &d) {
        return d.position == position && d.type == chart::TimingDirectiveType::Bpm;
    });
    pattern.timing.push_back({position, chart::TimingDirectiveType::Bpm, bpm});
    Replace(std::move(pattern), Document().Effects());
}
void EditorWorkspace::ApplyMeasureLength()
{
    SetMeasureLength(Integer(timingForm.measure) - 1, Position("1", timingForm.measureLength).fraction);
}
void EditorWorkspace::RemoveEffect(std::size_t row)
{
    auto effects = Document().Effects();
    if (row < effects.commands.size())
        effects.commands.erase(effects.commands.begin() + row);
    else
    {
        row -= effects.commands.size();
        if (row >= effects.hitSoundChanges.size())
            throw std::out_of_range("Effect row no longer exists.");
        effects.hitSoundChanges.erase(effects.hitSoundChanges.begin() + row);
    }
    Replace(Document().Pattern(), std::move(effects));
}
void EditorWorkspace::ApplyEffect()
{
    Replace(Document().Pattern(), effectForm.Apply(Document().Effects()));
}
std::pair<double, double> EditorWorkspace::TimelineRangeMilliseconds() const
{
    double begin = 0, end = 1;
    const auto [first, last] = Document().NoteTimeRange();
    begin = std::min(begin, first.count() / 1000.0);
    end = std::max(end, last.count() / 1000.0);
    if (const auto music = analysis_.Data().sounds.find("Music"); music != analysis_.Data().sounds.end())
        end = std::max(end, music->second.durationSeconds * 1000.0);
    return {begin, end};
}
void EditorWorkspace::SelectTool(int value)
{
    mode_->SelectTool(value);
    RequestRebuild();
}
void EditorWorkspace::Save()
{
    editor_->Save();
    const auto &effectPath = Document().Effects().sourcePath;
    request_.effectPath = effectPath.empty() ? std::nullopt : std::optional{effectPath};
    const auto path = Document().SourcePath().u8string();
    SetStatus("Saved: " + std::string(reinterpret_cast<const char *>(path.data()), path.size()));
    RequestRebuild();
}
void EditorWorkspace::PlaceNote(chart::MusicalPosition position)
{
    mode_->PlaceNote(*this, position);
}
