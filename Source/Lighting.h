#pragma once

#include "Vec.h"

namespace sc
{

    struct Lighting
    {
            
            // 主方向光：更侧、更高，产生更好的立体感
            Vec3 direction{ -0.577f, 0.577f, -0.577f }; // normalized: (-0.577, 0.577, -0.577)
        Vec3 color{ 1.00f, 0.94f, 0.82f }; // 暖黄昏光
        
        Vec3 ambient{ 0.12f, 0.14f, 0.18f }; // 暗面基础环境光
        
                // 半球环境光：顶上偏暖的天空散射，底下偏暗的地面反弹
            Vec3 skyColor{ 0.18f, 0.20f, 0.26f };  // 朝上的面微蓝
        Vec3 groundColor{ 0.06f, 0.05f, 0.04f }; // 朝下的面微暗暖
        
            float intensity{ 2.2f };
    };


} // namespace sc
