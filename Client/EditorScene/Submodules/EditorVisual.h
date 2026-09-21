#pragma once
#include "MRG_Core.h"
#include <functional>
#include <vector>
namespace v = mrg::visual2d;

class EditorVisual final : public v::Visual2DComponent
{
  public:
    std::vector<v::DrawPacket> packets;
    void AppendDrawPackets(std::vector<v::DrawPacket> &output) const override
    {
        output.insert(output.end(), packets.begin(), packets.end());
    }
};
struct Control
{
    v::Rect rect;
    std::function<void()> click;
};
struct NoteHit
{
    v::Point point;
    std::size_t order;
};
