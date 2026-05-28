#pragma once

#include <cmath>

//==============================================================================
// ¼«¼ò 3D ÏòÁ¿£¬ÓÃÓÚ³¡¾°ÊÀ½ç×ø±ê
//==============================================================================
struct Vec3
{
    float x{ 0.0f };
    float y{ 0.0f };
    float z{ 0.0f };

    Vec3() = default;
    Vec3(float xx, float yy, float zz) : x(xx), y(yy), z(zz) {}

    Vec3 operator+(const Vec3& o) const { return { x + o.x, y + o.y, z + o.z }; }
    Vec3 operator-(const Vec3& o) const { return { x - o.x, y - o.y, z - o.z }; }
    Vec3 operator*(float s)       const { return { x * s,   y * s,   z * s }; }

    float lengthSquared() const { return x * x + y * y + z * z; }
    float length()        const { return std::sqrt(lengthSquared()); }
};
