/*
  ==============================================================================
    Lighting.h
    Layer 2: Scene & Renderer
    简单的方向光 + 环境色。整个场景共享一份。
  ==============================================================================
*/
#pragma once

#include "Vec.h"

namespace sc
{
    struct Lighting
    {
        Vec3  direction{ -0.4f, 0.6f, -0.7f };  // 从光源指向世界的方向
        Vec3  color{ 1.0f, 0.96f, 0.88f };
        Vec3  ambient{ 0.18f, 0.20f, 0.24f };
        float intensity{ 1.0f };
    };
}
