/*
  ==============================================================================
    Mat4.h
    Layer 1: Math
    ÁÐÖ÷Ðò 4x4 ¾ØÕó£¨Óë GL / juce::Matrix3D Ò»ÖÂ£©¡£
    ËùÓÐ¼¸ºÎ±ä»»ÔÚÕâÀï¼¯ÖÐÊµÏÖ£¬äÖÈ¾²ãºÍ³¡¾°²ã¹²ÓÃ¡£
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
    // »ù´¡¾ØÕó
    //--------------------------------------------------------------------------
    inline Mat4 identity() noexcept
    {
        return Mat4(); // juce Ä¬ÈÏ¾ÍÊÇ identity
    }

    inline Mat4 translation(const Vec3& t) noexcept
    {
        // ÁÐÖ÷Ðò£ºÆ½ÒÆ·ÖÁ¿·ÅÔÚµÚ 12/13/14 ¸öÔªËØ
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

    /** ÈÆÈÎÒâµ¥Î»ÖáÐý×ª angle »¡¶È£¨ÓÒÊÖÏµ£¬Óë GL Ò»ÖÂ£©¡£ */
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
    // ÊÓÍ¼ / Í¶Ó°
    //--------------------------------------------------------------------------
    /** ÓÒÊÖÏµ lookAt£¬up ÈÎÒâ£¬»á×Ô¶¯Õý½»»¯¡£ */
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

    /** ±ê×¼Í¸ÊÓÍ¶Ó°£¬ÓÒÊÖÏµ£¬clip space z ¡Ê [-1, 1]£¨GL Ä¬ÈÏ£©¡£ */
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

    /** Õý½»Í¶Ó°£¬µ÷ÊÔ / 2D overlay ±¸ÓÃ¡£ */
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
    // ÊµÓÃ£º°Ñ Mat4 Ð´µ½ GL uniform
    //--------------------------------------------------------------------------
    inline void setMatrixUniform(juce::OpenGLShaderProgram& sh,
        const char* name,
        const Mat4& m) noexcept
    {
        const GLint loc = juce::gl::glGetUniformLocation(sh.getProgramID(), name);
        if (loc >= 0)
            juce::gl::glUniformMatrix4fv(loc, 1, juce::gl::GL_FALSE, m.mat);
    }

    //--------------------------------------------------------------------------
// 矩阵求逆（伴随矩阵 / 行列式法）
// 适用于摄像机 VP 矩阵等典型非奇异矩阵
//--------------------------------------------------------------------------
    inline Mat4 inverse(const Mat4& m) noexcept
    {
        const float* a = m.mat; // juce::Matrix3D 列主序 float[16]

        // 3x3 子式辅助
        auto det3 = [](float a00, float a01, float a02,
            float a10, float a11, float a12,
            float a20, float a21, float a22) -> float
            {
                return a00 * (a11 * a22 - a12 * a21)
                    - a01 * (a10 * a22 - a12 * a20)
                    + a02 * (a10 * a21 - a11 * a20);
            };

        // 余子式矩阵（代数余子式 = cofactor，符号考虑在内）
        float cof[16];

        // 第 0 列 (col 0)
        cof[0] = det3(a[5], a[6], a[7], a[9], a[10], a[11], a[13], a[14], a[15]);
        cof[1] = -det3(a[1], a[2], a[3], a[9], a[10], a[11], a[13], a[14], a[15]);
        cof[2] = det3(a[1], a[2], a[3], a[5], a[6], a[7], a[13], a[14], a[15]);
        cof[3] = -det3(a[1], a[2], a[3], a[5], a[6], a[7], a[9], a[10], a[11]);

        // 第 1 列 (col 1)
        cof[4] = -det3(a[4], a[6], a[7], a[8], a[10], a[11], a[12], a[14], a[15]);
        cof[5] = det3(a[0], a[2], a[3], a[8], a[10], a[11], a[12], a[14], a[15]);
        cof[6] = -det3(a[0], a[2], a[3], a[4], a[6], a[7], a[12], a[14], a[15]);
        cof[7] = det3(a[0], a[2], a[3], a[4], a[6], a[7], a[8], a[10], a[11]);

        // 第 2 列 (col 2)
        cof[8] = det3(a[4], a[5], a[7], a[8], a[9], a[11], a[12], a[13], a[15]);
        cof[9] = -det3(a[0], a[1], a[3], a[8], a[9], a[11], a[12], a[13], a[15]);
        cof[10] = det3(a[0], a[1], a[3], a[4], a[5], a[7], a[12], a[13], a[15]);
        cof[11] = -det3(a[0], a[1], a[3], a[4], a[5], a[7], a[8], a[9], a[11]);

        // 第 3 列 (col 3)
        cof[12] = -det3(a[4], a[5], a[6], a[8], a[9], a[10], a[12], a[13], a[14]);
        cof[13] = det3(a[0], a[1], a[2], a[8], a[9], a[10], a[12], a[13], a[14]);
        cof[14] = -det3(a[0], a[1], a[2], a[4], a[5], a[6], a[12], a[13], a[14]);
        cof[15] = det3(a[0], a[1], a[2], a[4], a[5], a[6], a[8], a[9], a[10]);

        // 行列式 = 第 0 行点乘第 0 列余子式
        const float det = a[0] * cof[0] + a[4] * cof[4] + a[8] * cof[8] + a[12] * cof[12];

        // 退化检查
        if (std::abs(det) < 1e-12f)
        {
            // 返回单位矩阵兜底
            return Mat4();
        }

        const float invDet = 1.0f / det;

        // 伴随矩阵转置 / 行列式 → 逆矩阵，列主序输出
        float inv[16];
        for (int i = 0; i < 16; ++i)
            inv[i] = cof[i] * invDet;

        return Mat4(inv);
    }

}
