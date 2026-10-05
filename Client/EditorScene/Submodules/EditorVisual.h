#pragma once
#include "MRG_Core.h"
#include <functional>
#include <vector>
namespace v = mrg::visual2d;

class EditorVisual final : public v::Visual2DComponent
{
  public:
    void Clear() noexcept
    {
        pendingRectangles_.reset();
        packets_.clear();
    }
    void AddRectangle(v::Rect bounds, v::Color color, float radius)
    {
        if (!pendingRectangles_)
        {
            pendingRectangles_ = std::make_shared<std::vector<v::RectanglePrimitive>>();
            v::DrawPacket batch;
            batch.type = v::DrawPacketType::RectangleBatch;
            batch.rectangles = pendingRectangles_;
            packets_.push_back(std::move(batch));
        }
        pendingRectangles_->push_back({bounds, color, radius});
    }
    void AddText(v::DrawPacket packet)
    {
        pendingRectangles_.reset();
        packets_.push_back(std::move(packet));
    }
    void Finish() noexcept { pendingRectangles_.reset(); }
    void AppendDrawPackets(std::vector<v::DrawPacket> &output) const override
    {
        output.insert(output.end(), packets_.begin(), packets_.end());
    }
  private:
    std::vector<v::DrawPacket> packets_;
    std::shared_ptr<std::vector<v::RectanglePrimitive>> pendingRectangles_;
};
struct Control
{
    v::Rect rect;
    std::function<void()> click;
};
