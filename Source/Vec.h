/*
  ==============================================================================
    Vec.h
    Layer 1: Math
    ÇáÁ¿ 3D ÏòÁ¿¡£°ü×° juce::Vector3D<float>£¬²¹Æë normalize/cross/dot/scale µÈ
    ºóÐø³¡¾°ÓëäÖÈ¾²ã¶¼ÐèÒªµÄ³£ÓÃËã×Ó¡£Õâ²ãÁãÒÀÀµ¡£
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
    // ±êÁ¿Ëõ·Å£¨ÓÃ¾ßÃûº¯Êý£¬±ÜÃâºÍ juce::Vector3D ×Ô´øµÄ operator*/ / ³åÍ»£©
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
    // »ù´¡Ëã×Ó
    //--------------------------------------------------------------------------
    inline float dot(const Vec3& a, const Vec3& b) noexcept
    {
        return a.x * b.x + a.y * b.y + a.z * b.z;
    }

    inline Vec3 cross(const Vec3& a, const Vec3& b) noexcept
    {
        // juce::Vector3D ÒÑÖØÔØ ^ Îª²æ³Ë£¬ÕâÀï¸øÒ»¸ö¾ßÃû°æ±¾ÒÔ±ãÔÄ¶Á¡£
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
    // ±êÁ¿¹¤¾ß
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
