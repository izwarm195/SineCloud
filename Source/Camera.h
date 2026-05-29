/*
  ==============================================================================
    Camera.h
    Layer 2: Scene & Renderer
    ÇòÃæ¹ìµÀÏà»ú£ºÒÔ pivot ÎªÔ²ÐÄ£¬Çò×ø±ê (yaw, pitch, distance) ¾ö¶¨ eye Î»ÖÃ¡£
    Z-up ÓÒÊÖÏµ£ºyaw ÈÆ +Z£¬pitch Ì§Í·£¨pitch=0 Æ½ÊÓ£¬pi/2 ÍêÈ«¸©ÊÓ£©¡£
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
        // ÉèÖÃ
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

        /** ÓÉ SceneView ÔÚ resized() Ê±µ÷ÓÃ¡£ */
        void setViewport(int widthPx, int heightPx) noexcept
        {
            vpW = juce::jmax(1, widthPx);
            vpH = juce::jmax(1, heightPx);
        }

        //----------------------------------------------------------------------
        // ¶ÁÈ¡
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
            // yaw=0 Ê± eye ÔÚ pivot µÄ -Y ·½Ïò£¨¿´Ïò +Y£©
            return { pivot.x + distance * cp * (-sy),
                     pivot.y + distance * cp * (-cy),
                     pivot.z + distance * sp };
        }

        /** ½ÇÉ« / Íæ¼ÒÐÐ½øËùÓÃµÄ"Ë®Æ½ÃæÇ°Ïò"£¨Í¶Ó°µ½ z=0 ºóµÄ forward£©¡£ */
        Vec3 getForwardOnGround() const noexcept
        {
            return { -std::sin(yaw), -std::cos(yaw), 0.0f };
        }
        Vec3 getRightOnGround() const noexcept
        {
            return { std::cos(yaw), -std::sin(yaw), 0.0f };
        }

        //----------------------------------------------------------------------
        // ¾ØÕó
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
        // ÆÁÄ» ¡ú ÊÀ½ç ÉäÏß£¨´Ó eye ³ö·¢£©
        //----------------------------------------------------------------------
        Ray screenToWorldRay(juce::Point<float> screenPx) const noexcept
        {
            // NDC: x¡Ê[-1,1] ÓÒÕý£¬y¡Ê[-1,1] ÉÏÕý
            const float ndcX = (2.0f * screenPx.x / (float)vpW) - 1.0f;
            const float ndcY = -((2.0f * screenPx.y / (float)vpH) - 1.0f);

            // ÓÃ pinhole Ä£ÐÍÔÚÏà»ú¿Õ¼ä¹¹Ôì·½Ïò£º
            // tan(fovY/2) * ndcY ÊÇ y£¬aspect * tan(fovY/2) * ndcX ÊÇ x£¬z = -1£¨Ïà»ú¿´ -Z£©
            const float tanHalf = std::tan(fovYRad * 0.5f);
            const Vec3 dirCam{ ndcX * tanHalf * getAspect(),
                               ndcY * tanHalf,
                              -1.0f };

            // °Ñ dirCam Ðý×ª»ØÊÀ½ç£ºÓÃÏà»ú basis£¨Óë view ¾ØÕó·´ÏòÒ»ÖÂ£©
            const Vec3 eye = getEyeWorld();
            const Vec3 fwd = normalize(pivot - eye);                 // Ïà»ú -Z£¨ÊÀ½ç£©
            const Vec3 rgt = normalize(cross(fwd, { 0, 0, 1 }));     // Ïà»ú +X
            const Vec3 upT = cross(rgt, fwd);                        // Ïà»ú +Y

            // dirCam = (x, y, -1) ¶ÔÓ¦ x*right + y*up + (-1)*(-fwd) = x*right + y*up + fwd
            Vec3 dirWorld{
                rgt.x * dirCam.x + upT.x * dirCam.y + fwd.x * 1.0f,
                rgt.y * dirCam.x + upT.y * dirCam.y + fwd.y * 1.0f,
                rgt.z * dirCam.x + upT.z * dirCam.y + fwd.z * 1.0f
            };
            return { eye, normalize(dirWorld) };
        }

        /** ¸ø¶¨Ò»ÌõÉäÏß£¬ÇóÓë z=planeZ Æ½ÃæµÄ½»µã£»ÈôÉäÏßÆ½ÐÐÓÚÆ½Ãæ·µ»Ø false¡£ */
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
        // Çò×ø±ê
        float yaw{ 0.0f };
        float pitch{ juce::MathConstants<float>::pi * 0.35f };
        float distance{ 12.0f };

        // ÏÞÖÆ
        float minPitch{ juce::MathConstants<float>::pi * 0.10f };
        float maxPitch{ juce::MathConstants<float>::pi * 0.48f };

        // ÖÐÐÄ
        Vec3 pivot{ 0, 0, 0 };

        // Í¶Ó°
        float fovYRad{ juce::degreesToRadians(55.0f) };
        float zNear{ 0.1f };
        float zFar{ 500.0f };

        // ÊÓ¿Ú£¨ÏñËØ£©
        int vpW{ 1 }, vpH{ 1 };
    };
}
