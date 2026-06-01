/*
  ==============================================================================
    Camera.h
    Layer 2: Scene & Renderer
    轨道相机，以 pivot 为圆心，(yaw, pitch, distance) 描述 eye 位置。
    Z-up 坐标系；yaw 绕 +Z；pitch 抬头，pitch=0 平视，pi/2 正俯视。
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

        // ★ 参数名改为 dist，消除对成员变量 distance 的遮蔽
        void setOrbit(float yawRad, float pitchRad, float dist) noexcept
        {
            setYaw(yawRad);
            pitch = juce::jlimit(minPitch, maxPitch, pitchRad);
            distance = juce::jmax(0.01f, dist);
        }

       
        // 归一化setYaw
        void setYaw(float r) noexcept
        {
            yaw = r;
            constexpr float twoPi = juce::MathConstants<float>::twoPi;
            constexpr float pi = juce::MathConstants<float>::pi;
            yaw = std::fmod(yaw, twoPi);
            if (yaw > pi) yaw -= twoPi;
            if (yaw < -pi) yaw += twoPi;
        }

        void setPitch(float r) noexcept { pitch = juce::jlimit(minPitch, maxPitch, r); }
        void setDistance(float d) noexcept { distance = juce::jmax(0.01f, d); }

        void setPitchLimits(float minR, float maxR) noexcept
        {
            minPitch = minR;
            maxPitch = maxR;
            pitch = juce::jlimit(minPitch, maxPitch, pitch);
        }

        void setPerspective(float fovYDeg, float zNear_, float zFar_) noexcept
        {
            fovYRad = juce::degreesToRadians(fovYDeg);
            zNear = zNear_;
            zFar = zFar_;
        }

        /** 由 SceneView 在 resized() 和 newOpenGLContextCreated() 末尾调用。 */
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
        float getAspect() const noexcept
        {
            return (float)vpW / (float)juce::jmax(1, vpH);
        }

        Vec3 getEyeWorld() const noexcept
        {
            const float cp = std::cos(pitch);
            const float sp = std::sin(pitch);
            const float cy = std::cos(yaw);
            const float sy = std::sin(yaw);
            return { pivot.x + distance * cp * (-sy),
                     pivot.y + distance * cp * (-cy),
                     pivot.z + distance * sp };
        }

        /** 水平面前向，yaw=0 时指向 +Y（屏幕上方）。 */
        Vec3 getForwardOnGround() const noexcept
        {
            return { +std::sin(yaw), +std::cos(yaw), 0.0f };
        }

        /** 水平面右向，yaw=0 时指向 +X（屏幕右方）。★修正方向 */
        Vec3 getRightOnGround() const noexcept
        {
            return { +std::cos(yaw), -std::sin(yaw), 0.0f };
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
        // 屏幕坐标 → 世界射线
        //----------------------------------------------------------------------
        Ray screenToWorldRay(juce::Point<float> screenPx) const noexcept
        {
            const float ndcX = (2.0f * screenPx.x / (float)vpW) - 1.0f;
            const float ndcY = -((2.0f * screenPx.y / (float)vpH) - 1.0f);

            const float tanHalf = std::tan(fovYRad * 0.5f);
            const Vec3 dirCam
            {
                ndcX * tanHalf * getAspect(),
                ndcY * tanHalf,
                -1.0f
            };

            const Vec3 eye = getEyeWorld();
            const Vec3 fwd = normalize(pivot - eye);
            const Vec3 rgt = normalize(cross(fwd, { 0, 0, 1 }));
            const Vec3 upT = cross(rgt, fwd);

            Vec3 dirWorld
            {
                rgt.x * dirCam.x + upT.x * dirCam.y + fwd.x,
                rgt.y * dirCam.x + upT.y * dirCam.y + fwd.y,
                rgt.z * dirCam.x + upT.z * dirCam.y + fwd.z
            };
            return { eye, normalize(dirWorld) };
        }

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
        float yaw{ 0.0f };
        float pitch{ juce::MathConstants<float>::pi * 0.35f };
        float distance{ 12.0f };

        float minPitch{ juce::MathConstants<float>::pi * 0.05f };
        float maxPitch{ juce::MathConstants<float>::pi * 0.48f };

        Vec3  pivot{ 0, 0, 0 };

        float fovYRad{ juce::degreesToRadians(55.0f) };
        float zNear{ 0.1f };
        float zFar{ 500.0f };

        int vpW{ 1 }, vpH{ 1 };
    };
}
