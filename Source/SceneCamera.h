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
