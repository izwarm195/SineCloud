/*
  ==============================================================================
    Lighting.h
    Layer 2: Scene & Renderer
    ¼òµ¥µÄ·½Ïò¹â + »·¾³É«¡£Õû¸ö³¡¾°¹²ÏíÒ»·Ý¡£
  ==============================================================================
*/
#pragma once

#include "Vec.h"

namespace sc
{
    struct Lighting
    {
        Vec3  direction{ -0.4f, 0.6f, -0.7f };  // ´Ó¹âÔ´Ö¸ÏòÊÀ½çµÄ·½Ïò
        Vec3  color{ 1.0f, 0.96f, 0.88f };
        Vec3  ambient{ 0.28f, 0.30f, 0.34f };
        float intensity{ 2.0f };
    };
}
