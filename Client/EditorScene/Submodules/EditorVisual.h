#pragma once
#include "PhantomCore.h"
#include <functional>
#include <vector>

class EditorVisual final : public Visual2DComponent
{
  public:
    void Clear() noexcept
    {
        pendingRectangles_.reset();
        packets_.clear();
    }
    void AddRectangle(Rect bounds, Color color, float radius)
    {
        if (!pendingRectangles_)
        {
            pendingRectangles_ = std::make_shared<std::vector<RectanglePrimitive>>();
            DrawPacket batch;
            batch.type = DrawPacketType::RectangleBatch;
            batch.rectangles = pendingRectangles_;
            packets_.push_back(std::move(batch));
        }
        pendingRectangles_->push_back({bounds, color, radius});
    }
    void AddPacket(DrawPacket packet)
    {
        pendingRectangles_.reset();
        packets_.push_back(std::move(packet));
    }
    void Finish() noexcept { pendingRectangles_.reset(); }
    void AppendDrawPackets(std::vector<DrawPacket> &output) const override
    {
        output.insert(output.end(), packets_.begin(), packets_.end());
    }
  private:
    std::vector<DrawPacket> packets_;
    std::shared_ptr<std::vector<RectanglePrimitive>> pendingRectangles_;
};
struct Control
{
    Rect rect;
    std::function<void()> click;
};
