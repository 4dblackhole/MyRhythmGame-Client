#include "MusicSelectView.h"
#include "Presentation/ValueDialog.h"
#include "SongSelectLayout.h"
#include "SongSelectionText.h"
#include <chrono>
#include <format>
using namespace song_select;

void MusicSelectView::LayoutRecordControls()
{
    if (!recordRows_)
        return;
    const float width = recordPanel_->NodeSize().width / DesignToCanvasScale;
    const float height = recordPanel_->NodeSize().height / DesignToCanvasScale;
    recordQueryButton_->SetBounds(ScaleTopLeftBounds({22, 78, width - 44, 40}, recordPanel_->NodeSize().height));
    recordRows_->SetBounds(ScaleTopLeftBounds({22, 130, width - 44, height - 220}, recordPanel_->NodeSize().height));
    recordRows_->SetClipRect({0, 0, recordRows_->NodeSize().width, recordRows_->NodeSize().height});
    recordPreviousButton_->SetBounds(
        ScaleTopLeftBounds({22, height - 65, (width - 56) / 2, 40}, recordPanel_->NodeSize().height));
    recordNextButton_->SetBounds(ScaleTopLeftBounds({34 + (width - 56) / 2, height - 65, (width - 56) / 2, 40},
                                                    recordPanel_->NodeSize().height));
}
void MusicSelectView::EditRecordQuery()
{
    std::vector<DialogValue> fields{
        {std::wstring(texts_.Get("Records.condition")), std::wstring(recordQuery_.begin(), recordQuery_.end())}};
    if (ShowValueDialog(std::wstring(texts_.Get("Records.query")), fields, std::wstring(texts_.Get("PlayOptions.ok")),
                        std::wstring(texts_.Get("PlayOptions.cancel")),
                        [&]
                        {
                            std::string query;
                            for (auto ch : fields[0].value)
                            {
                                if (ch > 127)
                                    throw std::invalid_argument("Query field names and operators use ASCII.");
                                query.push_back(static_cast<char>(ch));
                            }
                            PlayRecordStore::ValidateQuery(query);
                        }))
    {
        recordQuery_.clear();
        for (auto ch : fields[0].value)
            recordQuery_.push_back(static_cast<char>(ch));
        recordOffset_ = 0;
        ++recordRevision_;
    }
    inputRouter_.Reset(*canvas_);
}
void MusicSelectView::UpdateRecords()
{
    if (!recordRows_)
        return;
    std::filesystem::path pattern;
    std::string mode;
    if (selection_.focusedSongPosition_ < selection_.visibleSongIndices_.size())
    {
        const auto &song = selection_.catalog_.songs[selection_.visibleSongIndices_[selection_.focusedSongPosition_]];
        if (selection_.selectedPatternIndex_ < song.patterns.size())
        {
            pattern = song.patterns[selection_.selectedPatternIndex_].patternPath;
            mode = song.patterns[selection_.selectedPatternIndex_].pattern.mode;
        }
    }
    if (pattern != recordPattern_ || mode != recordMode_)
    {
        recordPattern_ = pattern;
        recordMode_ = mode;
        recordOffset_ = 0;
        ++recordRevision_;
    }
    if (recordDisplayRevision_ != recordRevision_)
    {
        recordDisplayRevision_ = recordRevision_;
        recordNext_ = false;
        recordPreviousButton_->SetEnabled(recordOffset_ > 0);
        recordNextButton_->SetEnabled(false);
        std::vector<NodeId> ids;
        for (const auto &child : recordRows_->Children())
            ids.push_back(child->Id());
        for (auto id : ids)
            static_cast<void>(recordRows_->RemoveChild(id));
        emptyRecordMessage_->SetVisible(true);
        RequireComponent<TextVisualComponent>(*emptyRecordMessage_)
            .SetText(std::wstring(texts_.Get(pattern.empty() ? "MusicSelect.noRecords" : "Records.loading")));
    }
    if (recordJob_.valid() && recordJob_.wait_for(std::chrono::seconds{0}) == std::future_status::ready)
    {
        try
        {
            auto rows = recordJob_.get();
            if (recordJobRevision_ == recordRevision_)
                ShowRecords(rows);
        }
        catch (const std::exception &error)
        {
            if (recordJobRevision_ == recordRevision_)
            {
                emptyRecordMessage_->SetVisible(true);
                RequireComponent<TextVisualComponent>(*emptyRecordMessage_)
                    .SetText(std::wstring(texts_.Get("Records.loadError")) + DecodeDisplayText(error.what()));
            }
        }
    }
    if (!recordJob_.valid() && recordJobRevision_ != recordRevision_)
    {
        recordJobRevision_ = recordRevision_;
        if (pattern.empty())
            return;
        const auto database = recordDatabase_;
        recordJob_ = std::async(std::launch::async,
                                [database, pattern, mode, condition = recordQuery_, offset = recordOffset_]
                                {
                                    return PlayRecordStore(database.empty() ? PlayRecordStore::DefaultPath() : database)
                                        .Query(pattern, mode, condition, offset, 7);
                                });
    }
}
void MusicSelectView::ShowRecords(const std::vector<PlayRecord> &records)
{
    emptyRecordMessage_->SetVisible(records.empty());
    RequireComponent<TextVisualComponent>(*emptyRecordMessage_)
        .SetText(std::wstring(texts_.Get("MusicSelect.noRecords")));
    recordNext_ = records.size() > 6;
    recordNextButton_->SetEnabled(recordNext_);
    const float width = recordRows_->NodeSize().width / DesignToCanvasScale;
    const float step = (recordRows_->NodeSize().height / DesignToCanvasScale) / 6;
    for (std::size_t i = 0; i < std::min(records.size(), std::size_t{6}); ++i)
    {
        const auto &r = records[i];
        const auto &o = r.options;
        const auto timestamp =
            std::chrono::sys_time<std::chrono::milliseconds>{std::chrono::milliseconds{r.playedAtMilliseconds}};
        std::wstring description = std::format(L"{:%Y-%m-%d %H:%M} UTC  {:.6f}%", timestamp, r.accuracy.Percent());
        description +=
            L"\n" + std::wstring(texts_.Get(r.cleared ? "Records.cleared"
                                                      : (r.completed ? "Records.practice" : "Records.interrupted")));
        description += std::format(L"  JL {:.2f}  {:.2f}x", r.judgeLevel.Value(), o.accelerate);
        description += std::format(L"\nScroll {:g}{}{}{}{}{}", o.scrollSpeed,
                                   o.scrollUnit == PlayOptions::ScrollUnit::Bpm ? L" BPM" : L"x",
                                   o.ignoreScrollEffects ? L" FIX" : L"", o.metronome ? L" MET" : L"",
                                   o.mirror ? L" MIR" : L"", o.random ? L" RND" : L"");
        if (o.random)
            description += std::format(L"\n{:g}%  #{}", o.randomAttemptProbability * 100, o.randomSeed);
        auto &row = AddLabel(*recordRows_, {0, static_cast<float>(i) * step, width, step - 3}, std::move(description),
                             13, DeepBlue, "PlayRecord");
        row.SetClipRect({0, 0, row.NodeSize().width, row.NodeSize().height});
    }
}
