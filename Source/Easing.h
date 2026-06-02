/*
  ==============================================================================
    Easing.h
    Layer 1: Math
    ³£ÓÃ»º¶¯º¯Êý + Ö¡ÂÊÎÞ¹Ø²åÖµ¡£
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

    inline float damp(float current, float target, float rate, float dt) noexcept
    {
        const float k = 1.0f - std::exp(-rate * 10 * dt);
        return current + (target - current) * k;
    }
}
