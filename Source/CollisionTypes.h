/*
==============================================================================
 CollisionTypes.h
 Layer 3: Game / Interaction

 障碍物碰撞体定义 + 碰撞检测函数。
==============================================================================
*/
#pragma once

#include "Vec.h"

namespace sc
{

    struct CollisionShape
    {
        enum Type { Cylinder, Box };

        Type   type;
        Vec3   center;
        float  radius;      // 圆柱半径 / 盒子 X 和 Y 半边长
        float  halfHeight;  // 半高（Z 方向）

        /** Player vs 圆柱碰撞。命中时 pushOut 为推开向量。 */
        static bool collidePlayerCylinder(
            const Vec3& playerPos, float playerRadius,
            const CollisionShape& obs, Vec3& pushOut);

        /** Player vs 盒子碰撞。 */
        static bool collidePlayerBox(
            const Vec3& playerPos, float playerRadius,
            const CollisionShape& obs, Vec3& pushOut);
        // CollisionTypes.h 新增声明
        static bool collidePlayerTriEdge(
            const Vec3& playerPos, float playerRadius,
            const Vec3& edgeA, const Vec3& edgeB,
            Vec3& pushOut);
    };
    struct CollisionTri {
        Vec3 a, b, c;    // 三角形三个顶点（世界坐标）    
        Vec3 normal;     // 面法线
    };
    


} // namespace sc
