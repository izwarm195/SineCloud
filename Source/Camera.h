/*
  ==============================================================================
    Camera.h
    Layer 2: Scene & Renderer
    球面轨道相机：以 pivot 为圆心，球坐标 (yaw, pitch, distance) 决定 eye 位置。
    Z-up 右手系：yaw 绕 +Z，pitch 抬头（pitch=0 平视，pi/2 完全俯视）。
  ==============================================================================
*/
#pragma once

#include <JuceHeader.h>
#include "Vec.h"
#include "Mat4.h"

namespace sc
{
    struct Ray
    {
        Vec3 origin{ 0, 0, 0 };
        Vec3 direction{ 0, 1, 0 };
    };

    class Camera
    {
    public:
        Camera() = default;

        //----------------------------------------------------------------------
        // 设置
        //----------------------------------------------------------------------
        void setPivot(const Vec3& p) noexcept { pivot = p; }
        void setOrbit(float yawRad, float pitchRad, float distance) noexcept
        {
            yaw = yawRad;
            pitch = juce::jlimit(minPitch, maxPitch, pitchRad);
            distance = juce::jmax(0.01f, distance);
        }
        void setYaw(float r) noexcept { yaw = r; }
        void setPitch(float r) noexcept { pitch = juce::jlimit(minPitch, maxPitch, r); }
        void setDistance(float d) noexcept { distance = juce::jmax(0.01f, d); }
        void setPitchLimits(float minR, float maxR) noexcept
        {
            minPitch = minR; maxPitch = maxR;
            pitch = juce::jlimit(minPitch, maxPitch, pitch);
        }

        void setPerspective(float fovYDeg, float zNear, float zFar) noexcept
        {
            fovYRad = juce::degreesToRadians(fovYDeg);
            this->zNear = zNear;
            this->zFar = zFar;
        }

        /** 由 SceneView 在 resized() 时调用。 */
        void setViewport(int widthPx, int heightPx) noexcept
        {
            vpW = juce::jmax(1, widthPx);
            vpH = juce::jmax(1, heightPx);
        }

        //----------------------------------------------------------------------
        // 读取
        //----------------------------------------------------------------------
        float getYaw()      const noexcept { return yaw; }
        float getPitch()    const noexcept { return pitch; }
        float getDistance() const noexcept { return distance; }
        Vec3  getPivot()    const noexcept { return pivot; }
        float getMinPitch() const noexcept { return minPitch; }
        float getMaxPitch() const noexcept { return maxPitch; }
        int   getViewportWidth()  const noexcept { return vpW; }
        int   getViewportHeight() const noexcept { return vpH; }
        float getAspect() const noexcept { return (float)vpW / (float)juce::jmax(1, vpH); }

        Vec3 getEyeWorld() const noexcept
        {
            const float cp = std::cos(pitch);
            const float sp = std::sin(pitch);
            const float cy = std::cos(yaw);
            const float sy = std::sin(yaw);
            // yaw=0 时 eye 在 pivot 的 -Y 方向（看向 +Y）
            return { pivot.x + distance * cp * (-sy),
                     pivot.y + distance * cp * (-cy),
                     pivot.z + distance * sp };
        }

        /** 角色 / 玩家行进所用的"水平面前向"（投影到 z=0 后的 forward）。 */
        Vec3 getForwardOnGround() const noexcept
        {
            return { -std::sin(yaw), -std::cos(yaw), 0.0f };
        }
        Vec3 getRightOnGround() const noexcept
        {
            return { std::cos(yaw), -std::sin(yaw), 0.0f };
        }

        //----------------------------------------------------------------------
        // 矩阵
        //----------------------------------------------------------------------
        Mat4 view() const noexcept
        {
            return lookAt(getEyeWorld(), pivot, { 0.0f, 0.0f, 1.0f });
        }
        Mat4 proj() const noexcept
        {
            return perspective(fovYRad, getAspect(), zNear, zFar);
        }

        //----------------------------------------------------------------------
        // 屏幕 → 世界 射线（从 eye 出发）
        //----------------------------------------------------------------------
        Ray screenToWorldRay(juce::Point<float> screenPx) const noexcept
        {
            // NDC: x∈[-1,1] 右正，y∈[-1,1] 上正
            const float ndcX = (2.0f * screenPx.x / (float)vpW) - 1.0f;
            const float ndcY = -((2.0f * screenPx.y / (float)vpH) - 1.0f);

            // 用 pinhole 模型在相机空间构造方向：
            // tan(fovY/2) * ndcY 是 y，aspect * tan(fovY/2) * ndcX 是 x，z = -1（相机看 -Z）
            const float tanHalf = std::tan(fovYRad * 0.5f);
            const Vec3 dirCam{ ndcX * tanHalf * getAspect(),
                               ndcY * tanHalf,
                              -1.0f };

            // 把 dirCam 旋转回世界：用相机 basis（与 view 矩阵反向一致）
            const Vec3 eye = getEyeWorld();
            const Vec3 fwd = normalize(pivot - eye);                 // 相机 -Z（世界）
            const Vec3 rgt = normalize(cross(fwd, { 0, 0, 1 }));     // 相机 +X
            const Vec3 upT = cross(rgt, fwd);                        // 相机 +Y

            // dirCam = (x, y, -1) 对应 x*right + y*up + (-1)*(-fwd) = x*right + y*up + fwd
            Vec3 dirWorld{
                rgt.x * dirCam.x + upT.x * dirCam.y + fwd.x * 1.0f,
                rgt.y * dirCam.x + upT.y * dirCam.y + fwd.y * 1.0f,
                rgt.z * dirCam.x + upT.z * dirCam.y + fwd.z * 1.0f
            };
            return { eye, normalize(dirWorld) };
        }

        /** 给定一条射线，求与 z=planeZ 平面的交点；若射线平行于平面返回 false。 */
        static bool intersectGroundPlane(const Ray& r, float planeZ, Vec3& out) noexcept
        {
            if (std::abs(r.direction.z) < 1.0e-6f) return false;
            const float t = (planeZ - r.origin.z) / r.direction.z;
            if (t < 0.0f) return false;
            out = { r.origin.x + r.direction.x * t,
                    r.origin.y + r.direction.y * t,
                    planeZ };
            return true;
        }

    private:
        // 球坐标
        float yaw{ 0.0f };
        float pitch{ juce::MathConstants<float>::pi * 0.35f };
        float distance{ 12.0f };

        // 限制
        float minPitch{ juce::MathConstants<float>::pi * 0.10f };
        float maxPitch{ juce::MathConstants<float>::pi * 0.48f };

        // 中心
        Vec3 pivot{ 0, 0, 0 };

        // 投影
        float fovYRad{ juce::degreesToRadians(55.0f) };
        float zNear{ 0.1f };
        float zFar{ 500.0f };

        // 视口（像素）
        int vpW{ 1 }, vpH{ 1 };
    };
}
