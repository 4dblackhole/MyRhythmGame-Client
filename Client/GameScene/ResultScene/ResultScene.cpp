#include "ResultScene.h"
#include "GameFlow/YOUMUSceneIds.h"
#include "Submodules/ResultViewFactory.h"
#include <stdexcept>
#include "Records/PlayRecordStore.h"
#include "Presentation/UiColors.h"
ResultScene::ResultScene(std::unique_ptr<IPlayResult> result,
                         ScreenVisual2DManager &visuals, TextCatalog &texts,
                         ResultViewContext context, RetryHandler retry)
    : result_(std::move(result)), retry_(std::move(retry)), visuals_(visuals), texts_(texts)
{
    if (!result_)
        throw std::invalid_argument("Result Scene requires a play result.");
    if (!context.recordDatabase.empty())
        recordWrite_ = std::async(std::launch::async, [path = context.recordDatabase, record = PlayRecord::From(*result_)] {
            try { PlayRecordStore(path).Save(record); return std::string{}; }
            catch (const std::exception &error) { return std::string(error.what()); }
        });
    context.canRetry = context.canRetry && static_cast<bool>(retry_);
    view_ = CreateResultView(*result_, visuals, texts, std::move(context));
}
ResultScene::~ResultScene() = default;
void ResultScene::Initialize(const EngineServices &services)
{
    view_->Initialize(services);
    if (recordWrite_.valid())
    {
        recordStatusCanvas_ = visuals_.CreateOwnedCanvas({{1280,720}, CanvasScaleMode::FixedHeight});
        auto *canvas = recordStatusCanvas_.Get();
        if (!canvas) throw std::runtime_error("Failed to create the record status canvas.");
        recordStatus_ = &CreateLabel(canvas->AnchorNode(Anchor::BottomRight), {-700, 18, 680, 26},
            std::wstring(texts_.Get("Records.saving")), "Result.RecordStatus");
        auto &text = *recordStatus_->GetComponent<TextVisualComponent>();
        texts_.ApplyFont(text); text.SetFontSize(14); text.SetTextColor(ui_colors::result::Ink);
        text.SetHorizontalAlignment(TextAlignment::Trailing);
    }
}
void ResultScene::BeginScene()
{
    view_->SetVisible(true);
    static_cast<void>(recordStatusCanvas_.SetVisible(true));
}
void ResultScene::EndScene() noexcept
{
    view_->SetVisible(false);
    static_cast<void>(recordStatusCanvas_.SetVisible(false));
}
void ResultScene::Update(const UpdateContext &context, SceneManager &scenes)
{
    if (recordWrite_.valid() && recordWrite_.wait_for(std::chrono::seconds{0}) == std::future_status::ready)
        recordError_ = recordWrite_.get();
    if (recordStatus_)
    {
        auto &text = *recordStatus_->GetComponent<TextVisualComponent>();
        text.SetText(recordWrite_.valid() ? std::wstring(texts_.Get("Records.saving")) :
            (recordError_.empty() ? std::wstring(texts_.Get("Records.saved")) :
             std::wstring(texts_.Get("Records.saveError")) + std::wstring(recordError_.begin(), recordError_.end())));
    }
    const auto action = view_->Update(context.input);
    if (action == ResultAction::Retry && (!retry_ || !retry_(scenes)))
        throw std::runtime_error("Failed to retry the completed pattern.");
    if (action == ResultAction::Back && !scenes.ChangeScene(LobbySceneId))
        throw std::runtime_error("Failed to return from Result to song select.");
}
void ResultScene::Render(const RenderContext &)
{
}
void ResultScene::OnResize(std::uint32_t width, std::uint32_t height)
{
    view_->OnResize(width, height);
}
void ResultScene::Shutdown() noexcept
{
    recordStatus_ = nullptr;
    recordStatusCanvas_.Reset();
    view_->Shutdown();
}
