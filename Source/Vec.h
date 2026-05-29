/*
  ==============================================================================
    Vec.h
    Layer 1: Math
    轻量 3D 向量。包装 juce::Vector3D<float>，补齐 normalize/cross/dot/scale 等
    后续场景与渲染层都需要的常用算子。这层零依赖。
  ==============================================================================
*/
#pragma once

#include <JuceHeader.h>
#include <cmath>

namespace sc
{
    using Vec3 = juce::Vector3D<float>;
    using Vec2 = juce::Point<float>;

    //--------------------------------------------------------------------------
    // 标量缩放（用具名函数，避免和 juce::Vector3D 自带的 operator*/ / 冲突）
    //--------------------------------------------------------------------------
    inline Vec3 scale(const Vec3& v, float s) noexcept
    {
        return { v.x * s, v.y * s, v.z * s };
    }

    inline Vec3 scale(const Vec3& v, const Vec3& s) noexcept
    {
        return { v.x * s.x, v.y * s.y, v.z * s.z };
    }

    //--------------------------------------------------------------------------
    // 基础算子
    //--------------------------------------------------------------------------
    inline float dot(const Vec3& a, const Vec3& b) noexcept
    {
        return a.x * b.x + a.y * b.y + a.z * b.z;
    }

    inline Vec3 cross(const Vec3& a, const Vec3& b) noexcept
    {
        // juce::Vector3D 已重载 ^ 为叉乘，这里给一个具名版本以便阅读。
        return a ^ b;
    }

    inline float lengthSquared(const Vec3& v) noexcept
    {
        return dot(v, v);
    }

    inline float length(const Vec3& v) noexcept
    {
        return std::sqrt(lengthSquared(v));
    }

    inline Vec3 normalize(const Vec3& v) noexcept
    {
        const float len = length(v);
        if (len < 1.0e-8f)
            return { 0.0f, 0.0f, 0.0f };
        return { v.x / len, v.y / len, v.z / len };
    }

    inline float distance(const Vec3& a, const Vec3& b) noexcept
    {
        return length(a - b);
    }

    inline float distanceSquared(const Vec3& a, const Vec3& b) noexcept
    {
        return lengthSquared(a - b);
    }

    inline Vec3 lerp(const Vec3& a, const Vec3& b, float t) noexcept
    {
        return { a.x + (b.x - a.x) * t,
                 a.y + (b.y - a.y) * t,
                 a.z + (b.z - a.z) * t };
    }

    //--------------------------------------------------------------------------
    // 标量工具
    //--------------------------------------------------------------------------
    inline float clamp01(float v) noexcept
    {
        return juce::jlimit(0.0f, 1.0f, v);
    }

    inline float lerp(float a, float b, float t) noexcept
    {
        return a + (b - a) * t;
    }

    inline float invLerp(float a, float b, float v) noexcept
    {
        return (b - a) != 0.0f ? (v - a) / (b - a) : 0.0f;
    }

    inline float remap(float inMin, float inMax,
        float outMin, float outMax,
        float v) noexcept
    {
        return lerp(outMin, outMax, invLerp(inMin, inMax, v));
    }
}
