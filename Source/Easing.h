/*
  ==============================================================================
    Easing.h
    Layer 1: Math
    常用缓动函数 + 帧率无关插值。
  ==============================================================================
*/
#pragma once

#include <cmath>
#include <algorithm>

namespace sc::easing
{
    inline float linear(float t) noexcept { return t; }

    inline float smoothstep(float t) noexcept
    {
        t = std::clamp(t, 0.0f, 1.0f);
        return t * t * (3.0f - 2.0f * t);
    }

    inline float easeOutCubic(float t) noexcept
    {
        const float u = 1.0f - std::clamp(t, 0.0f, 1.0f);
        return 1.0f - u * u * u;
    }

    inline float easeInOutCubic(float t) noexcept
    {
        t = std::clamp(t, 0.0f, 1.0f);
        return t < 0.5f
            ? 4.0f * t * t * t
            : 1.0f - std::pow(-2.0f * t + 2.0f, 3.0f) * 0.5f;
    }

    /** 帧率无关的指数趋近：每秒衰减到 (1 - rate)。
        例：rate=0.9 表示每秒覆盖目标值的 90%。 */
    inline float damp(float current, float target,
        float rate, float dt) noexcept
    {
        rate = std::clamp(rate, 0.0f, 0.9999f);
        const float k = 1.0f - std::pow(1.0f - rate, dt);
        return current + (target - current) * k;
    }
}
