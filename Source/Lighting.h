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
    Vec3  color     { 0.80f, 0.70f, 0.60f };
    Vec3  ambient   { 0.22f, 0.17f, 0.12f };

    // ---- 半球环境光 ----
    Vec3  skyColor  { 0.80f, 0.80f, 0.60f };
    Vec3  groundColor { 0.06f, 0.05f, 0.04f };
    float intensity { 1.0f };

    // ---- 距离雾（俯视专用） ----
    Vec3 fogColorSRGB{ 0.85f, 0.80f, 0.73f }; // 默认与 clearColor 同色
    float fogDensity   { 0.12f };               // 越大越浓；0 = 关闭
    float fogHeightFalloff { 0.2f };            // Z 越高雾越稀；0 = 均匀雾
    float fogStart     { 5.0f };                 // 这个距离前不算雾

    // ---- 动态点光源 ----
    std::vector<PointLight> pointLights;

    // ★★★ 云层 / 体积光参数 ★★★
    float cloudScale{ 0.3f };  // Perlin 噪声缩放
    float cloudThreshold{ 0.327f };   // 云/晴 阈值
    float cloudSpeed{ 1.0f };   // 云移动速度
    float cloudPlaneHeight{ 90.0f };   // 虚拟云层高度
    float cloudBandLevels{ 3.0f };    // 像素风 banding 级数
    float volumetricSteps{ 16.0f };    // Ray March 步数
    float volumetricIntensity{ 0.8f }; // 体积光强度
    float cloudTime{ 0.0f };    // 每帧由外部更新

    // ★★★ 阴影参数 ★★★
    float shadowBias{ 0.0025f };
    float shadowStrength{ 1.0f };
    float sceneRadius{ 7.0f };
    bool  shadowEnabled{ true };

    // ★★★ Bloom 参数 ★★★
    float bloomThreshold{ 1.0f };   // HDR 亮度阈值
    float bloomSoftKnee{ 0.5f };   // 软过渡
    float bloomStrength{ 0.2f };  // 混合权重
    float bloomFilterRadius{ 2.0f };   // 上采样模糊半径

    // ★★★ Motion Blur 参数 ★★★
    float motionBlurIntensity{ 0.5f };   // 混合强度 [0, 1]
    int   motionBlurSamples{ 16 };        // 采样数 (2 ~ 32)

    // ★★★ Pixelate 参数 ★★★
    float pixelSize{ 4.0f };       // 像素块大小（屏幕像素单位，1=关闭）
    float colorLevels{ 64.0f };     // 0=不量化; >1=色阶数
    bool  useColorQuant{ true };   // true=RGB独立量化（更硬边缘）; false=亮度posterize
    float edgeBoost{ 0.05f };

    // ★★★ Denoise 参数 ★★★
    float denoiseStrength{ 0.0f };   // 降噪混合强度 [0, 1]，0 = 不降噪
    float denoiseColorSigma{ 0.99f }; // 亮度相似度 sigma，越大越激进

};

inline constexpr int MAX_POINT_LIGHTS = 32;
} // namespace sc
