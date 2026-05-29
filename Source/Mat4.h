/*
  ==============================================================================
    Mat4.h
    Layer 1: Math
    列主序 4x4 矩阵（与 GL / juce::Matrix3D 一致）。
    所有几何变换在这里集中实现，渲染层和场景层共用。
  ==============================================================================
*/
#pragma once

#include <JuceHeader.h>
#include "Vec.h"
#include <cmath>

namespace sc
{
    using Mat4 = juce::Matrix3D<float>;

    //--------------------------------------------------------------------------
    // 基础矩阵
    //--------------------------------------------------------------------------
    inline Mat4 identity() noexcept
    {
        return Mat4(); // juce 默认就是 identity
    }

    inline Mat4 translation(const Vec3& t) noexcept
    {
        // 列主序：平移分量放在第 12/13/14 个元素
        const float m[16] = {
            1.0f, 0.0f, 0.0f, 0.0f,
            0.0f, 1.0f, 0.0f, 0.0f,
            0.0f, 0.0f, 1.0f, 0.0f,
            t.x,  t.y,  t.z,  1.0f
        };
        return Mat4(m);
    }

    inline Mat4 scaling(const Vec3& s) noexcept
    {
        const float m[16] = {
            s.x,  0.0f, 0.0f, 0.0f,
            0.0f, s.y,  0.0f, 0.0f,
            0.0f, 0.0f, s.z,  0.0f,
            0.0f, 0.0f, 0.0f, 1.0f
        };
        return Mat4(m);
    }

    inline Mat4 scaling(float uniform) noexcept
    {
        return scaling({ uniform, uniform, uniform });
    }

    /** 绕任意单位轴旋转 angle 弧度（右手系，与 GL 一致）。 */
    inline Mat4 rotationAxis(const Vec3& axisIn, float angleRad) noexcept
    {
        const Vec3 a = normalize(axisIn);
        const float c = std::cos(angleRad);
        const float s = std::sin(angleRad);
        const float ic = 1.0f - c;

        const float x = a.x, y = a.y, z = a.z;

        const float m[16] = {
            c + x * x * ic,    y * x * ic + z * s,  z * x * ic - y * s,  0.0f,
            x * y * ic - z * s,  c + y * y * ic,    z * y * ic + x * s,  0.0f,
            x * z * ic + y * s,  y * z * ic - x * s,  c + z * z * ic,    0.0f,
            0.0f,          0.0f,          0.0f,          1.0f
        };
        return Mat4(m);
    }

    inline Mat4 rotationX(float r) noexcept { return rotationAxis({ 1, 0, 0 }, r); }
    inline Mat4 rotationY(float r) noexcept { return rotationAxis({ 0, 1, 0 }, r); }
    inline Mat4 rotationZ(float r) noexcept { return rotationAxis({ 0, 0, 1 }, r); }

    //--------------------------------------------------------------------------
    // 视图 / 投影
    //--------------------------------------------------------------------------
    /** 右手系 lookAt，up 任意，会自动正交化。 */
    inline Mat4 lookAt(const Vec3& eye, const Vec3& target, const Vec3& upHint) noexcept
    {
        const Vec3 f = normalize(target - eye);   // forward
        const Vec3 s = normalize(cross(f, upHint)); // right
        const Vec3 u = cross(s, f);               // true up

        const float m[16] = {
             s.x,         u.x,        -f.x,        0.0f,
             s.y,         u.y,        -f.y,        0.0f,
             s.z,         u.z,        -f.z,        0.0f,
            -dot(s, eye), -dot(u, eye), dot(f, eye), 1.0f
        };
        return Mat4(m);
    }

    /** 标准透视投影，右手系，clip space z ∈ [-1, 1]（GL 默认）。 */
    inline Mat4 perspective(float fovYRadians, float aspect,
        float zNear, float zFar) noexcept
    {
        const float f = 1.0f / std::tan(fovYRadians * 0.5f);
        const float nf = 1.0f / (zNear - zFar);

        const float m[16] = {
            f / aspect, 0.0f, 0.0f,                       0.0f,
            0.0f,       f,    0.0f,                       0.0f,
            0.0f,       0.0f, (zFar + zNear) * nf,        -1.0f,
            0.0f,       0.0f, (2.0f * zFar * zNear) * nf,  0.0f
        };
        return Mat4(m);
    }

    /** 正交投影，调试 / 2D overlay 备用。 */
    inline Mat4 ortho(float left, float right,
        float bottom, float top,
        float zNear, float zFar) noexcept
    {
        const float rl = 1.0f / (right - left);
        const float tb = 1.0f / (top - bottom);
        const float fn = 1.0f / (zFar - zNear);

        const float m[16] = {
            2.0f * rl, 0.0f,      0.0f,       0.0f,
            0.0f,      2.0f * tb, 0.0f,       0.0f,
            0.0f,      0.0f,      -2.0f * fn, 0.0f,
            -(right + left) * rl,
            -(top + bottom) * tb,
            -(zFar + zNear) * fn,
             1.0f
        };
        return Mat4(m);
    }

    //--------------------------------------------------------------------------
    // 实用：把 Mat4 写到 GL uniform
    //--------------------------------------------------------------------------
    inline void setMatrixUniform(juce::OpenGLShaderProgram& sh,
        const char* name,
        const Mat4& m) noexcept
    {
        const GLint loc = juce::gl::glGetUniformLocation(sh.getProgramID(), name);
        if (loc >= 0)
            juce::gl::glUniformMatrix4fv(loc, 1, juce::gl::GL_FALSE, m.mat);
    }
}
