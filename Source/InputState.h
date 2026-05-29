/*
  ==============================================================================
    InputState.h
    Layer 3: Game / Interaction
    Ò»Ö¡ÊäÈë¿ìÕÕ¡£ÓÉ SceneView ÔÚÃ¿´Î World::update Ç°ÌîºÃ£¬World ÄÚµÄ Entity
    Ö»¶Á¡£Êó±ê°´ÏÂ/Ì§ÆðÒÔ"ÊÂ¼þ"ÐÎÊ½´æÔÚ£¨justPressed / justReleased£©£¬Î»ÖÃÊ¼
    ÖÕ¿ÉÓÃ£¬±ãÓÚ×öÊ°È¡ÓëÍÏ×§¡£
  ==============================================================================*/
#pragma once

#include <JuceHeader.h>

namespace sc
{
    struct InputState
    {
        // ---- ¼üÅÌ£¨³ÖÐø°´ÏÂ£© ----
        bool keyUp{ false };
        bool keyDown{ false };
        bool keyLeft{ false };
        bool keyRight{ false };
        bool keyAttack{ false };   // ÔÝÊ±Õ¼Î»£¬ÈÕºó¸ø BossEntity ÓÃ

        // ---- Êó±ê ----
        juce::Point<float> mousePos{ 0.0f, 0.0f };
        bool mouseDown{ false }; // ×ó¼üµ±Ç°ÊÇ·ñ°´ÏÂ
        bool mouseJustPressed{ false }; // ±¾Ö¡¸Õ°´ÏÂ
        bool mouseJustReleased{ false }; // ±¾Ö¡¸ÕÌ§Æð
        juce::Point<float> mouseDelta{ 0.0f, 0.0f }; // ×ÔÉÏÒ»Ö¡µÄÎ»ÒÆ

        // ---- ÊÓ¿Ú£¨ÏñËØ£© ----
        int viewportW{ 1 };
        int viewportH{ 1 };
    };
}
