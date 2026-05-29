#pragma once

#include <JuceHeader.h>
#include "SceneObject.h"
#include "InertialSlider.h"

//==============================================================================
class IsoKnob : public InertialSlider,
    public SceneObject
{
public:
    IsoKnob()
    {
        setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
        setMouseDragSensitivity(400);
        setVelocityBasedMode(false);
    }

    void setKnobSize(int diameter) { screenDiameter = diameter; }
    int  getKnobSize() const { return screenDiameter; }

    void updateScreenPosition(const SceneCamera& cam) override
    {
        // ---- 1. ÖÐÐÄµã + µØÃæÔ²ÉÏµÄ 4 ¸ö²ÉÑùµã ----
        const float r = (float)screenDiameter * 0.5f / 2.0f;  // ÓÃÊÀ½ç°ë¾¶£¬µ¥Î»ÊÇÊÀ½ç¾àÀë
        // ×¢£ºscreenDiameter Ö®Ç°ÊÇÆÁÄ»ÏñËØ£¬ÏÖÔÚÓÃ×÷"ÊÀ½çµ¥Î»Ö±¾¶"»á¸üÖ±¹Û£»
        // ÕâÀïÈ¡Ò»°ëÔÙ³ý2×÷ÎªÊÀ½ç°ë¾¶£¬ÈÃÐýÅ¥¿´ÆðÀ´±ÈÀýºÏÊÊ
        const float worldR = 30.0f;  // ÐýÅ¥ÔÚÊÀ½çÖÐµÄ°ë¾¶£¨¹Ì¶¨Öµ£¬²»ÔÙÓÃ screenDiameter£©

        const auto pCenter = cam.worldToScreen(worldPos);
        const auto pRight = cam.worldToScreen({ worldPos.x + worldR, worldPos.y, 0 });
        const auto pUp = cam.worldToScreen({ worldPos.x, worldPos.y + worldR, 0 });

        // ---- 2. ÆÁÄ»ÉÏÍÖÔ²µÄÓÒÏòÁ¿¡¢ÉÏÏòÁ¿ ----
        const float rxScr = pRight.x - pCenter.x;
        const float ryScr = pRight.y - pCenter.y;
        const float uxScr = pUp.x - pCenter.x;
        const float uyScr = pUp.y - pCenter.y;

        // ---- 3. °üÎ§ºÐ³ß´ç ----
        // ÍÖÔ²ÔÚÆÁÄ»ÉÏË®Æ½/´¹Ö±Í¶Ó°×î´óÖµ£ºmax(|rx|+|ux|, |ry|+|uy|)
        const float halfW = std::abs(rxScr) + std::abs(uxScr);
        const float halfH = std::abs(ryScr) + std::abs(uyScr);
        const int boxW = (int)(halfW * 2.0f + 16.0f);  // Áôµã margin
        const int boxH = (int)(halfH * 2.0f + 16.0f);

        setBounds((int)std::round(pCenter.x) - boxW / 2,
            (int)std::round(pCenter.y) - boxH / 2,
            boxW, boxH);

        // ---- 4. ·ÂÉä±ä»»£º°Ñ component ÄÚµÄ"µ¥Î»Ô²"±ä»»³ÉÆÁÄ»ÍÖÔ² ----
        // component ÖÐÐÄ = pCenter£¬Ë®Æ½ÏòÁ¿ = (rxScr, ryScr)£¬´¹Ö±ÏòÁ¿ = (uxScr, uyScr)
        // JUCE Slider Ä¬ÈÏÔÚ component ¾ØÐÎÄÚ»­Ô²£¬°ë¾¶ = min(w,h)/2
        // ÎÒÃÇÒªÈÃÄÇ¸öÔ²±ä³ÉÆÁÄ»ÉÏµÄÍÖÔ²
        const float compR = (float)juce::jmin(boxW, boxH) * 0.5f;
        if (compR < 1.0f) { setTransform({}); return; }

        // µ¥Î»ÏòÁ¿
        const float scaleR = std::sqrt(rxScr * rxScr + ryScr * ryScr) / compR;
        const float scaleU = std::sqrt(uxScr * uxScr + uyScr * uyScr) / compR;

        // ÓÃ AffineTransform Ö±½Ó¹¹Ôì£º´Ó component ¾Ö²¿ (ÈÆÖÐÐÄ) Ó³Éäµ½ÆÁÄ»ÍÖÔ²
        // ÕâÀï¼ò»¯£ºÏÈ°´ R ·½ÏòËõ·Å£¬ÔÙ¼ôÇÐµ½ U ·½Ïò
        const float angleR = std::atan2(ryScr, rxScr);

        const float cx = boxW * 0.5f;
        const float cy = boxH * 0.5f;

        // ¸´ºÏ±ä»»£ºÐý×ª angleR ÈÃ X Öá¶ÔÆëÓÒÏòÁ¿£¬°´ (scaleR, scaleU) Ëõ·Å
        // ×¢Òâ JUCE AffineTransform ÊÇ×ó³Ë
        auto t = juce::AffineTransform()
            .translated(-cx, -cy)
            .scaled(scaleR, scaleU)
            .rotated(angleR)
            .translated(cx, cy);

        setTransform(t);
    }


//void applyIsoTransform(const SceneCamera& cam)
//    {
//        // ÐýÅ¥Ñ¹±âÏµÊý = sin(pitch) * yScale
//        // pitch=90¡ã ÍêÈ«¸©ÊÓ ¡ú squash = yScale£¬y ·½Ïò×îÃ÷ÏÔ
//        // pitch=30¡ã Ä¬ÈÏ       ¡ú squash ¡Ö 0.5 * 0.7 = 0.35£¬±â
//        const float squash = std::max(0.3f, std::sin(cam.getPitch()) * cam.getYAxisScale() + 0.3f);
//        const float shearX = cam.getYAxisScale() * std::cos(cam.getPitch()) * 0.5f;
//
//        const float cx = getWidth() * 0.5f;
//        const float cy = getHeight() * 0.5f;
//
//        auto t = juce::AffineTransform()
//            .translated(-cx, -cy)
//            .sheared(shearX, 0.0f)
//            .scaled(1.0f, squash)
//            .translated(cx, cy);
//
//        setTransform(t);
//    }



private:
    int screenDiameter{ 64 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(IsoKnob)
};
