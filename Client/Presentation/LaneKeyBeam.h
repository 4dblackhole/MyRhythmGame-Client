#pragma once

#include "MRG_Core.h"
#include "Note/Note.h"

namespace finger_drum::presentation
{
    // Owns presentation state; the Lane/Canvas owns the sprite node.
    class LaneKeyBeam final
    {
    public:
        void Initialize(mrg::visual2d::Visual2DNode& lane,
            mrg::visual2d::ImageHandle image, mrg::visual2d::Size imageSize);
        void SetLayout(float laneWidth, float laneLength);
        void OnKeyPressed(const rhythm::NoteProcessResult& result);
        void Update(double deltaSeconds);
        void Reset() noexcept;
        void Shutdown() noexcept;

        [[nodiscard]] static mrg::visual2d::Color InputColor(
            const rhythm::NoteProcessResult& result) noexcept;

    private:
        mrg::visual2d::Visual2DNode* node_{};
        mrg::visual2d::Color color_{1, 1, 1, 1};
        float aspectRatio_{1};
        double elapsedSeconds_{0.2};
    };
}
