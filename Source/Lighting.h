#pragma once

#include "Vec.h"

namespace sc
{

    struct Lighting
    {
        // 归一化的光照方向（从光源指向世界），偏暖的倾斜顶光
        Vec3 direction{ -0.420f, 0.525f, -0.741f };  // normalized: (-0.42, 0.525, -0.741)
        Vec3 color{ 1.00f, 0.96f, 0.88f };       // 暖白主光
        Vec3 ambient{ 0.24f, 0.26f, 0.30f };       // 提升环境光，暗面不至于全黑
        float intensity{ 2.2f };                      // 微调强度
    };

} // namespace sc
