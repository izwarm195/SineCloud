#pragma once

#include <JuceHeader.h>
#include <cmath>
#include "Vec3.h"

//==============================================================================
// ÇòÃæÏà»ú£ºÒÔ pivot£¨Íæ¼Ò£©ÎªÖÐÐÄ£¬ÈÆÇòÃæÔË¶¯
//
// ÇòÃæ²ÎÊý»¯£º
//   yaw   = ÈÆ z Öá½Ç¶È£¬0 = camera ÔÚ pivot µÄ -y ·½Ïò£¨¿´Ïò +y£©
//   pitch = Ñö½Ç£¬0 = Æ½ÊÓ£¬¦Ð/2 = ÍêÈ«¸©ÊÓ
//   orbitDistance = Ïà»úµ½ pivot µÄ¾àÀë
//
// Ïà»úÎ»ÖÃ£¨ÊÀ½ç£©£º
//   camPos.x = pivot.x + orbitDistance * cos(pitch) * (-sin(yaw))
//   camPos.y = pivot.y + orbitDistance * cos(pitch) * (-cos(yaw))
//   camPos.z = pivot.z + orbitDistance * sin(pitch)
//
// pitch ±» clamp µ½ [minPitch, maxPitch] ±£Ö¤ camera ÔÚ×îµÍ¸ß¶ÈÒÔÉÏ
//==============================================================================
class SceneCamera
{
public:
    SceneCamera() { rebuild(); }

    //--- ÊÓ½Ç ---

    void  setYaw(float radians) { yaw = radians; rebuild();}
    void  setPitch(float radians) { pitch = juce::jlimit(minPitch, maxPitch, radians); rebuild();
    }
    float getYaw() const { return yaw; }
    float getPitch() const { return pitch; }

    void  setMinPitch(float r) { minPitch = r; setPitch(pitch); }
    void  setMaxPitch(float r) { maxPitch = r; setPitch(pitch); }
    float getMinPitch() const { return minPitch; }
    float getMaxPitch() const { return maxPitch; }

    //--- Çò°ë¾¶ ---
    void  setOrbitDistance(float d) { orbitDistance = std::max(50.0f, d); rebuild();
    }
    float getOrbitDistance() const { return orbitDistance; }

    //--- ÇòÐÄ£¨Íæ¼ÒÎ»ÖÃ£©---
    void  setPivot(Vec3 p) { pivot = p; rebuild();
    }
    Vec3  getPivot() const { return pivot; }

    //--- Í¸ÊÓ²ÎÊý ---
    void  setFocalLength(float f) { focal = std::max(50.0f, f); }
    float getFocalLength() const { return focal; }

    //--- ÆÁÄ»ÖÐÐÄ ---
    void  setScreenCenter(juce::Point<float> c) { screenCenter = c; }

    //--- È¡Ïà»úÊµ¼ÊÊÀ½çÎ»ÖÃ£¨¼ÆËãÊôÐÔ£©---
    Vec3 getCameraWorldPos() const { return camPos; }

    //--- ½ÇÉ«"Ç°/ÓÒ"·½Ïò£¨ÑØ yaw ³¯Ïò£¬Ë®Æ½ÃæÍ¶Ó°£©---
    Vec3 getForwardWorld() const { return { -sinYaw, cosYaw, 0.0f }; }
    Vec3 getRightWorld()   const { return { cosYaw, sinYaw, 0.0f }; }

    //--- ÆÁÄ»ÖÐÐÄµØÃæµã£¨Ðý×ªÖ§µãÓÃ£©---
    Vec3 getGroundCenterWorld() const { return pivot; }  // ¼ò»¯£ºÊ¼ÖÕÊÇ pivot

   
    //--- Í¶Ó° ---
    juce::Point<float> worldToScreen(Vec3 w) const
    {
        
        const float rx = w.x - camPos.x;
        const float ry = w.y - camPos.y;
        const float rz = w.z - camPos.z;

        
        const float xr = rx * cosYaw - ry * sinYaw;
        const float yr = rx * sinYaw + ry * cosYaw;

       
        const float camX = xr;
        const float camY = -rz * sinPitch + yr * cosPitch;   // 深度
        const float camZ = rz * cosPitch + yr * sinPitch;   // 上方


       
        const float depth = std::max(focal * 0.1f, camY);
        const float invD = focal / depth;

        const float px = camX * invD;
        const float py = -camZ * invD;
        return { screenCenter.x + px, screenCenter.y + py };


    }
    //--- GL 矩阵：与 worldToScreen 用同样的 yaw/pitch/orbit/pivot/focal ---
    juce::Matrix3D<float> getViewMatrix() const
    {
        // eye = camPos, target = pivot, up = +Z
        const juce::Vector3D<float> eye{ camPos.x, camPos.y, camPos.z };
        const juce::Vector3D<float> target{ pivot.x,  pivot.y,  pivot.z };
        const juce::Vector3D<float> upV{ 0.0f, 0.0f, 1.0f };

        auto fwd = target - eye; fwd = fwd / fwd.length();
        auto rgt = fwd ^ upV;    rgt = rgt / rgt.length();
        auto upT = rgt ^ fwd;

        auto dot = [](juce::Vector3D<float> a, juce::Vector3D<float> b)
            { return a.x * b.x + a.y * b.y + a.z * b.z; };

        float m[16] = {
             rgt.x,  upT.x, -fwd.x, 0.0f,
             rgt.y,  upT.y, -fwd.y, 0.0f,
             rgt.z,  upT.z, -fwd.z, 0.0f,
            -dot(rgt, eye), -dot(upT, eye), dot(fwd, eye), 1.0f
        };
        return juce::Matrix3D<float>(m);
    }

    juce::Matrix3D<float> getProjMatrix(float aspect, float zNear = 1.0f, float zFar = 5000.0f) const
    {
        // 用现有 focal 反推 fov：focal 越大 fov 越小（你现在 focal=1800 大致是 30 度多）
        // 这里用屏幕高度作为参考：fov_y = 2 * atan(screenH / (2 * focal))
        // 但 screenH 这里没存，简化用一个近似：focal=1200 → 约 53°，focal=1800 → 约 37°
        const float refH = 600.0f; // 参考屏高
        const float fovY = 2.0f * std::atan(refH / (2.0f * focal));

        const float f = 1.0f / std::tan(fovY * 0.5f);
        float m[16] = {
            f / aspect, 0.0f, 0.0f,                              0.0f,
            0.0f,       f,    0.0f,                              0.0f,
            0.0f,       0.0f, (zFar + zNear) / (zNear - zFar),  -1.0f,
            0.0f,       0.0f, (2.0f * zFar * zNear) / (zNear - zFar), 0.0f
        };
        return juce::Matrix3D<float>(m);
    }




private:
    void rebuild()
    {
        cosYaw = std::cos(yaw);
        sinYaw = std::sin(yaw);
        cosPitch = std::cos(pitch);
        sinPitch = std::sin(pitch);

        camPos.x = pivot.x + orbitDistance * cosPitch * (-sinYaw);
        camPos.y = pivot.y + orbitDistance * cosPitch * (-cosYaw);
        camPos.z = pivot.z + orbitDistance * sinPitch;
    }

    float yaw{ 0.0f };
    float pitch{ juce::MathConstants<float>::pi / 3.0f };  // 60¡ã
    float minPitch{ juce::MathConstants<float>::pi * 0.20f }; // 36¡ã ×îµÍ
    float maxPitch{ juce::MathConstants<float>::pi * 0.4f }; // 86¡ã ×î¸ß£¨²»µ½ 90 ±ÜÃâÆæµã£©

    float orbitDistance{ 150.0f };
    float focal{ 800.0f };

    float cosYaw{ 1.0f }, sinYaw{ 0.0f };
    float cosPitch{ 0.5f }, sinPitch{ 0.866f };

    Vec3 pivot{ 0, 0, 0 };
    Vec3 camPos{ 0, 0, 0 };
    juce::Point<float> screenCenter{ 0, 0 };
};
