/*
==============================================================================
 CollisionTypes.cpp
==============================================================================
*/
#include "CollisionTypes.h"
#include <JuceHeader.h>
#include <cmath>

namespace sc
{

    bool CollisionShape::collidePlayerCylinder(
        const Vec3& playerPos, float playerRadius,
        const CollisionShape& obs, Vec3& pushOut)
    {
        const float dx = playerPos.x - obs.center.x;
        const float dy = playerPos.y - obs.center.y;
        const float dist2 = dx * dx + dy * dy;
        const float minDist = playerRadius + obs.radius;

        if (dist2 >= minDist * minDist)
            return false;

        float dist = std::sqrt(dist2);
        if (dist < 0.001f)
            dist = 0.001f;

        const float overlap = minDist - dist;
        pushOut.x = (dx / dist) * overlap;
        pushOut.y = (dy / dist) * overlap;
        pushOut.z = 0.0f;
        return true;
    }

    bool CollisionShape::collidePlayerBox(
        const Vec3& playerPos, float playerRadius,
        const CollisionShape& obs, Vec3& pushOut)
    {
        // 把 player 近似为点 + 半径，盒子向外扩展半径 → 点 vs 扩展盒
        const float ex = obs.radius + playerRadius;
        const float ey = obs.radius + playerRadius;

        // 最近点（钳制到扩展盒内）
        const float cx = juce::jlimit(obs.center.x - ex, obs.center.x + ex, playerPos.x);
        const float cy = juce::jlimit(obs.center.y - ey, obs.center.y + ey, playerPos.y);

        const float dx = playerPos.x - cx;
        const float dy = playerPos.y - cy;
        const float dist2 = dx * dx + dy * dy;

        if (dist2 >= playerRadius * playerRadius)
            return false;

        float dist = std::sqrt(dist2);
        if (dist < 0.001f)
        {
            pushOut = { playerRadius, 0.0f, 0.0f };
            return true;
        }

        const float overlap = playerRadius - dist;
        pushOut.x = (dx / dist) * overlap;
        pushOut.y = (dy / dist) * overlap;
        pushOut.z = 0.0f;
        return true;
    }

} // namespace sc
