/* ==============================================================================
   Material.h
   Layer 2: Scene & Renderer
   表面材质属性：简化 PBR（baseColor + roughness + metallic） + 自发光 + SSS。
   颜色值均为 sRGB（设计色），在 Renderer 内部转为线性。
   预设静态工厂方便 World 直接用。
   ============================================================================== */
#pragma once
#include "Vec.h"

namespace sc
{
    struct Material
    {
        // 设计色（sRGB 空间，0-1）
        Vec3  baseColorSRGB{ 0.8f, 0.8f, 0.8f };
        float roughness{ 0.85f };   // 0 = 镜面，1 = 完全漫反射
        float metallic{ 0.0f };    // 0 = 介电，1 = 金属
        Vec3  emissiveSRGB{ 0.0f, 0.0f, 0.0f };
        float emissiveStrength{ 0.0f };    // 乘数，>1 会进入后续 Bloom 提取
        float sss{ 0.0f };    // 简化次表面散射：背光面透光强度
        float specTint{ 0.0f };    // 介电高光是否染色（金属强制 1）

        // ---------------- 预设 ----------------
        static Material stone(Vec3 c = { 0.55f, 0.52f, 0.50f }) noexcept
        {
            Material m; m.baseColorSRGB = c; m.roughness = 0.92f; m.metallic = 0.0f; return m;
        }
        static Material metal(Vec3 c = { 0.78f, 0.80f, 0.82f }, float r = 0.35f) noexcept
        {
            Material m; m.baseColorSRGB = c; m.roughness = r; m.metallic = 1.0f; return m;
        }
        static Material water(Vec3 c = { 0.10f, 0.30f, 0.45f }) noexcept
        {
            Material m; m.baseColorSRGB = c; m.roughness = 0.08f; m.metallic = 0.0f;
            m.specTint = 0.2f; return m;
        }
        static Material wood(Vec3 c = { 0.45f, 0.30f, 0.18f }) noexcept
        {
            Material m; m.baseColorSRGB = c; m.roughness = 0.78f; m.sss = 0.05f; return m;
        }
        static Material lamp(Vec3 c, float strength = 4.0f) noexcept
        {
            Material m; m.baseColorSRGB = c; m.emissiveSRGB = c;
            m.emissiveStrength = strength; m.roughness = 1.0f; return m;
        }
        static Material foliage(Vec3 c = { 0.25f, 0.45f, 0.22f }) noexcept
        {
            Material m; m.baseColorSRGB = c; m.roughness = 0.85f; m.sss = 0.35f; return m;
        }
    };
} // namespace sc
