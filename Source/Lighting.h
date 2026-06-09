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
    Vec3  direction { -0.577f, 0.277f, -0.577f };
    Vec3  color     { 1.00f, 1.00f, 1.00f };
    Vec3  ambient   { 0.12f, 0.12f, 0.12f };

    // ---- 半球环境光 ----
    Vec3  skyColor  { 1.00f, 1.00f, 1.00f };
    Vec3  groundColor { 0.06f, 0.05f, 0.04f };
    float intensity { 1.0f };

    // ---- 距离雾（俯视专用） ----
    Vec3 fogColorSRGB{ 0.70f, 0.70f, 0.73f }; // 默认与 clearColor 同色
    float fogDensity   { 0.3f };               // 越大越浓；0 = 关闭
    float fogHeightFalloff { 0.1f };            // Z 越高雾越稀；0 = 均匀雾
    float fogStart     { 4.0f };                 // 这个距离前不算雾

    // ---- 动态点光源 ----
    std::vector<PointLight> pointLights;

    // ★★★ 云层 / 体积光参数 ★★★
    float cloudScale{ 0.4f };  // Perlin 噪声缩放
    float cloudThreshold{ 0.4f };   // 云/晴 阈值
    float cloudSpeed{ 1.25f };   // 云移动速度
    float cloudPlaneHeight{ 30.0f };   // 虚拟云层高度
    float cloudBandLevels{ 3.0f };    // 像素风 banding 级数
    float volumetricSteps{ 16.0f };    // Ray March 步数
    float volumetricIntensity{ 0.2f }; // 体积光强度
    float cloudTime{ 0.0f };    // 每帧由外部更新

    // ★★★ 阴影参数 ★★★
    float shadowBias{ 0.0025f };
    float shadowStrength{ 1.0f };
    float sceneRadius{ 7.0f };
    bool  shadowEnabled{ true };

    // ★★★ Bloom 参数 ★★★
    float bloomThreshold{ 1.0f };   // HDR 亮度阈值
    float bloomSoftKnee{ 0.5f };   // 软过渡
    float bloomStrength{ 0.06f };  // 混合权重
    float bloomFilterRadius{ 1.0f };   // 上采样模糊半径



};

inline constexpr int MAX_POINT_LIGHTS = 32;
} // namespace sc
