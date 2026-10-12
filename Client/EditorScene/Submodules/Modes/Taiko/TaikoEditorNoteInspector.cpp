#include "TaikoEditorNoteInspector.h"
#include "../../EditorSupport.h"
#include "Taiko/Submodules/TaikoNoteOptions.h"
#include "TaikoEditorPlacement.h"
#include "TaikoEditorTool.h"
#include "Texts/EditorScene/Taiko/TaikoEditorTexts.h"
#include <format>
using namespace editor_ui;
namespace
{
    namespace options = finger_drum::mode::taiko_options;

    void SetOption(chart::PatternNote &note, std::string_view name, std::string value, bool removeBare = false)
    {
        std::erase_if(
            note.extraData,
            [&](const std::string &field)
            {
                const auto equals = field.find('=');
                return equals != std::string::npos
                           ? options::EqualsInsensitive(options::Trim(std::string_view(field).substr(0, equals)), name)
                           : removeBare &&
                                 std::ranges::all_of(options::Trim(field), [](char c) { return c >= '0' && c <= '9'; });
            });
        if (!value.empty())
            note.extraData.push_back(std::string(name) + "=" + value);
    }
    void ValidateCount(const std::string &value, bool allowAuto)
    {
        if (value.empty() && allowAuto)
            return;
        const auto count = Integer(value);
        if (count < 1 || count > 1024)
            throw std::invalid_argument("Note count/division must be 1..1024.");
    }
} // namespace

void TaikoEditorNoteInspector::Select(const chart::IEditorDocument &document, std::size_t id)
{
    selected_ = document.FindNote(id);
    draftChanged_ = false;
    revision_ = 0;
    Refresh(document);
}
bool TaikoEditorNoteInspector::Clear() noexcept
{
    const bool hadSelection = selected_.has_value();
    selected_.reset();
    draftChanged_ = false;
    return hadSelection;
}
bool TaikoEditorNoteInspector::IsSelected(std::size_t id) const noexcept
{
    return selected_ &&
           (selected_->head.note.sourceOrder == id || (selected_->tail && selected_->tail->note.sourceOrder == id));
}
void TaikoEditorNoteInspector::Refresh(const chart::IEditorDocument &document)
{
    if (!selected_ || revision_ == document.Revision())
        return;
    selected_ = document.FindNote(selected_->head.note.sourceOrder);
    revision_ = document.Revision();
    if (!selected_ || draftChanged_)
        return;
    const auto &note = selected_->head.note;
    draft_.tool = note.keyType;
    if (note.keyType == 17)
    {
        std::vector<chart::Diagnostic> diagnostics;
        if (options::ReadBuzzAction(note, diagnostics) == finger_drum::mode::TaikoAction::Kat)
            draft_.tool = 18;
    }
    draft_.hitCount = std::string(options::FindOption(note, "HitCount").value_or(""));
    if (draft_.hitCount.empty())
        for (const auto &field : note.extraData)
            if (field.find('=') == std::string::npos)
            {
                draft_.hitCount = std::string(options::Trim(field));
                break;
            }
    draft_.tickDivision = std::string(options::FindOption(note, "TickDivision").value_or("16"));
    draft_.hitSound = note.hitSound;
}
void TaikoEditorNoteInspector::Field(IEditorModeCanvas &canvas, IEditorContext &state,
                                     finger_drum::texts::Language language, float y, std::wstring_view label,
                                     std::string &value)
{
    canvas.Text({1530, y, 330, 24}, std::wstring(label), 17);
    canvas.Button({1530, y + 26, 340, 34}, Wide(value),
                  [this, &state, language, label, &value]
                  {
                      if (const auto edited =
                              EditText(std::wstring(label), value, finger_drum::texts::Editor(language)))
                      {
                          value = *edited;
                          draftChanged_ = true;
                          state.RequestRebuild();
                      }
                  });
}
void TaikoEditorNoteInspector::Draw(IEditorModeCanvas &canvas, IEditorContext &state,
                                    finger_drum::texts::Language language)
{
    Refresh(state.Document());
    const auto &text = finger_drum::texts::TaikoEditor(language);
    canvas.Box({1510, 116, 386, 678}, Pale, 8);
    canvas.Text({1530, 136, 340, 28}, std::wstring(text.noteInfo), 22);
    if (!selected_)
    {
        canvas.Text({1530, 184, 340, 100}, std::wstring(text.selectNoteHelp), 18);
        return;
    }
    canvas.Button({1530, 182, 340, 38}, std::wstring(editor_tools::Name(draft_.tool, text.toolVariants)),
                  [this, &state]
                  {
                      const auto current = std::ranges::find(editor_tools::Tools, draft_.tool, &editor_tools::Tool::id);
                      draft_.tool = current == editor_tools::Tools.end() || current + 1 == editor_tools::Tools.end()
                                        ? editor_tools::Tools.front().id
                                        : (current + 1)->id;
                      draftChanged_ = true;
                      state.RequestRebuild();
                  });
    const auto &head = selected_->head;
    canvas.Text({1530, 235, 340, 32},
                std::wstring(text.startPosition) + L" " + std::to_wstring(head.note.position.measure + 1) + L", " +
                    Wide(Fraction(head.note.position.fraction)),
                17);
    canvas.Text({1530, 267, 340, 28}, std::format(L"{:.3f} ms", head.timing.count() / 1000.0), 16);
    if (selected_->tail)
        canvas.Text({1530, 302, 340, 32},
                    std::wstring(text.endPosition) + L" " +
                        std::to_wstring(selected_->tail->note.position.measure + 1) + L", " +
                        Wide(Fraction(selected_->tail->note.position.fraction)),
                    17);
    if (editor_tools::UsesHitCount(draft_.tool))
        Field(canvas, state, language, 368, text.hitCount, draft_.hitCount);
    if (editor_tools::UsesTickDivision(draft_.tool))
        Field(canvas, state, language, 368, text.tickDivision, draft_.tickDivision);
    Field(canvas, state, language, 446, text.hitSound, draft_.hitSound);
    if (selected_->tail && editor_tools::UsesTickDivision(head.note.keyType))
    {
        std::vector<chart::Diagnostic> diagnostics;
        const auto division = options::ReadPositiveOption(head.note, "TickDivision", 16, diagnostics);
        if (division)
        {
            auto count = state.Document().Timeline().CountSubdivisions(head.note.position,
                                                                       selected_->tail->note.position, *division);
            if (head.note.keyType == 17 && count != 0)
                --count;
            canvas.Text({1530, 524, 340, 30}, std::wstring(text.tickCount) + L" " + std::to_wstring(count), 17);
        }
    }
    canvas.Button({1530, 716, 160, 40}, std::wstring(text.apply), [this, &state] { Apply(state); }, true);
    canvas.Button({1710, 716, 160, 40}, std::wstring(text.resetDraft),
                  [this, &state]
                  {
                      Select(state.Document(), selected_->head.note.sourceOrder);
                      state.RequestRebuild();
                  });
}
void TaikoEditorNoteInspector::Apply(IEditorContext &state)
{
    Refresh(state.Document());
    if (!selected_)
        throw std::out_of_range("Selected note no longer exists.");
    const auto *tool = editor_tools::Find(draft_.tool);
    if (!tool)
        throw std::invalid_argument("Unknown Taiko note type.");
    auto head = selected_->head.note;
    head.keyType = static_cast<int>(tool->note);
    head.hitSound = draft_.hitSound;
    std::optional<chart::MusicalPosition> end;
    if (finger_drum::mode::FindTaikoNote(head.keyType)->longNote)
    {
        if (!selected_->tail)
            throw std::invalid_argument("A long note requires an end position.");
        end = selected_->tail->note.position;
    }
    if (editor_tools::UsesHitCount(draft_.tool))
    {
        ValidateCount(draft_.hitCount, true);
        SetOption(head, "HitCount", draft_.hitCount, true);
    }
    if (editor_tools::UsesTickDivision(draft_.tool))
    {
        ValidateCount(draft_.tickDivision, false);
        SetOption(head, "TickDivision", draft_.tickDivision);
    }
    if (head.keyType == 17)
        SetOption(head, "Action", draft_.tool == 18 ? "Kat" : "Don");
    if (editor_tools::UsesHitCount(draft_.tool))
    {
        if (!end)
            throw std::invalid_argument("A counted note requires an end position.");
        std::vector<chart::Diagnostic> diagnostics;
        if (!options::ReadHitCount(head, state.Document().Timeline().CountSubdivisions(head.position, *end, 12),
                                   diagnostics))
            throw std::invalid_argument(diagnostics.front().message);
    }
    taiko_editor::ValidatePlacement(state.Document(), head.position, head.keyType, end, head.sourceOrder);
    const auto id = head.sourceOrder;
    state.UpdateNote(id, std::move(head), end);
    draftChanged_ = false;
    revision_ = 0;
    Refresh(state.Document());
}
