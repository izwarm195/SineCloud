/* ==============================================================================
   PointLight.h
   Layer 2: Scene & Renderer
   动态点光源：玩家辉光、萤火虫、旋钮发光等。颜色为 sRGB 设计色，
   range 是物理截断半径（米），衰减系数自动从 range 推导。
   ============================================================================== */
#pragma once
#include "Vec.h"

namespace sc
{
    struct PointLight
    {
        Vec3  position{ 0.0f, 0.0f, 0.0f };
        Vec3  colorSRGB{ 1.0f, 0.85f, 0.55f };  // 设计色
        float intensity{ 4.0f };                // 总强度（>1 才会进 bloom）
        float range{ 6.0f };                // 影响半径（米）
        bool  enabled{ true };

        /** 物理衰减用的二次项；线性项简化为 0 即可（quadratic 主导）。
            给 GPU 用：1 / (1 + 0*d + quadratic*d?) */
        float quadratic() const noexcept
        {
            // 让 d=range 处衰减约为 0.01（可接受截断点）
            const float r = juce::jmax(0.001f, range);
            return 99.0f / (r * r);
        }

        // 便捷构造
        static PointLight warm(Vec3 pos, float intensity = 4.0f, float range = 6.0f) noexcept
        {
            PointLight p; p.position = pos;
            p.colorSRGB = { 1.0f, 0.78f, 0.42f };
            p.intensity = intensity; p.range = range; return p;
        }
        static PointLight cool(Vec3 pos, float intensity = 3.0f, float range = 5.0f) noexcept
        {
            PointLight p; p.position = pos;
            p.colorSRGB = { 0.55f, 0.70f, 1.0f };
            p.intensity = intensity; p.range = range; return p;
        }
        static PointLight firefly(Vec3 pos) noexcept
        {
            PointLight p; p.position = pos;
            p.colorSRGB = { 0.85f, 1.0f, 0.55f };
            p.intensity = 2.5f; p.range = 3.5f; return p;
        }
    };
} // namespace sc
