/* ==============================================================================
   Lighting.h
   Layer 2: Scene & Renderer
   主方向光 + 半球环境光 + 动态点光源数组 + 距离雾。
   World 每帧维护 pointLights（可任意 push/erase），Renderer 提交时
   只上传前 MAX_POINT_LIGHTS 个使能的灯。
   ============================================================================== */
#pragma once
#include <vector>
#include "Vec.h"
#include "PointLight.h"

namespace sc
{
    struct Lighting
    {
        // ---- 主方向光 ----
        Vec3  direction{ -0.577f, 0.577f, -0.577f };
        Vec3  color{ 1.00f, 0.94f, 0.82f };
        Vec3  ambient{ 0.12f, 0.14f, 0.18f };

        // ---- 半球环境光 ----
        Vec3  skyColor{ 0.18f, 0.20f, 0.26f };
        Vec3  groundColor{ 0.06f, 0.05f, 0.04f };
        float intensity{ 1.2f };

        // ---- 距离雾（俯视专用） ----
        Vec3  fogColorSRGB{ 0.06f, 0.07f, 0.09f };  // 默认与 clearColor 同色
        float fogDensity{ 0.018f };               // 越大越浓；0 = 关闭
        float fogHeightFalloff{ 0.08f };            // Z 越高雾越稀；0 = 均匀雾
        float fogStart{ 4.0f };                 // 这个距离前不算雾

        // ---- 动态点光源 ----
        std::vector<PointLight> pointLights;
    };

    inline constexpr int MAX_POINT_LIGHTS = 16;
} // namespace sc
