/*
  ==============================================================================
    InertialValue.h
    Layer 1: Math
    ÍÑÀë juce::Slider / juce::Component µÄ¹ßÐÔÊýÖµ¿ØÖÆÆ÷£º
      - ÍÏ¶¯£º°ÑÊó±ê dx/dy/dt Î¹½øÀ´£¬ÄÚ²¿¼ÇÂ¼Ë²Ê±ËÙ¶È
      - Ì§ÊÖ£ºÒÔË²Ê±ËÙ¶È»¬ÐÐ£¬°´"Ã¿Ãë±£ÁôÂÊ"Ë¥¼õÖ±µ½Í£Ö¹
      - ×Ô¶¯»¯£ºËÞÖ÷¶Ë¸Ä±ä²ÎÊýÊ±µ÷ÓÃ setValueFromHost(...)£¬²»»á´¥·¢»Øµ÷

    ³ÖÓÐ·½£¨KnobEntity / ParamBridge£©ÐèÒªÃ¿Ö¡µ÷ÓÃ tick(dt)£¬²¢°Ñ onValueChanged
    »Øµ÷Ö¸Ïò ParamBridge::write()¡£
  ==============================================================================
*/
#pragma once

#include <JuceHeader.h>
#include <functional>
#include <cmath>
#include <algorithm>

namespace sc
{
    class InertialValue
    {
    public:
        InertialValue() = default;

        //----------------------------------------------------------------------
        // ÅäÖÃ
        //----------------------------------------------------------------------
        /** ÕæÊµÖµ·¶Î§£¨Óë APVTS ²ÎÊý min/max ¶ÔÆë£©¡£ */
        void setRange(float minValue, float maxValue) noexcept
        {
            jassert(maxValue > minValue);
            minV = minValue;
            maxV = maxValue;
            value = juce::jlimit(minV, maxV, value);
        }

        /** ÍÏ¶¯ÁéÃô¶È£ºÍÏÂú dragRangePixels ÏñËØ = ×ßÍêÕû¸öÖµÓò£¨min¡úmax£©¡£
            ¾É InertialSlider ÓÃ setMouseDragSensitivity(400)£¬ÕâÀïÄ¬ÈÏ¾Í 400¡£ */
        void setDragRangePixels(float pixels) noexcept
        {
            dragRangePx = juce::jmax(1.0f, pixels);
        }

        /** Ä¦²Á£ºÃ¿Ãë±£Áô¶àÉÙËÙ¶È¡£0.05 = 1 ÃëºóÖ»Ê£ 5% ËÙ¶È£¨ºÜÉ¬£©£¬
            0.5 = 1 ÃëºóÊ£ 50% ËÙ¶È£¨ºÜ»¬£©¡£¾ÉÖµ friction=0.9£¨Ã¿Ö¡£©¡Ö 0.0024£¨Ã¿Ãë£©
            ¹ýÉ¬£¬ÕâÀïÄ¬ÈÏÉè³É 0.05£¬¸ü½Ó½ü"¹ßÐÔÊÖ¸Ð"¡£ */
        void setFrictionPerSecond(float keepRatePerSecond) noexcept
        {
            frictionPerSec = juce::jlimit(1.0e-4f, 0.999f, keepRatePerSecond);
        }

        /** Ì§ÊÖÊ±°ÑË²Ê±ËÙ¶È³ËÒÔÕâ¸ö±¶Êý£¬×÷Îª³õÊ¼»¬ÐÐËÙ¶È¡£ */
        void setInertiaGain(float g) noexcept
        {
            inertiaGain = juce::jmax(0.0f, g);
        }

        /** ËÙ¶ÈµÍÓÚ´ËÖµ£¨µ¥Î»£ºÖµ/Ãë£©Ê±Í£Ö¹»¬ÐÐ¡£ */
        void setStopThreshold(float t) noexcept
        {
            stopThreshold = juce::jmax(0.0f, t);
        }

        //----------------------------------------------------------------------
        // ÊýÖµ·ÃÎÊ
        //----------------------------------------------------------------------
        float getValue() const noexcept { return value; }
        float getMin()   const noexcept { return minV; }
        float getMax()   const noexcept { return maxV; }
        float getNormalised() const noexcept
        {
            return (maxV > minV) ? (value - minV) / (maxV - minV) : 0.0f;
        }
        bool  isDragging() const noexcept { return dragging; }
        bool  isCoasting() const noexcept { return std::abs(velocity) > stopThreshold; }

        /** ÓÃ»§Ö÷¶¯ÉèÖÃ£¨»á´¥·¢ onValueChanged£©¡£ */
        void setValue(float newValue, bool notify = true) noexcept
        {
            const float clamped = juce::jlimit(minV, maxV, newValue);
            if (!juce::approximatelyEqual(clamped, value))
            {
                value = clamped;
                if (notify && onValueChanged)
                    onValueChanged(value);
            }
        }

        /** ËÞÖ÷×Ô¶¯»¯ / preset »Ö¸´»Ø¹à£¨²»»á»Øµ÷£¬±ÜÃâ·´ÏòÐ´ÈëËÀÑ­»·£©¡£ */
        void setValueFromHost(float newValue) noexcept
        {
            value = juce::jlimit(minV, maxV, newValue);
            // À´×ÔËÞÖ÷µÄ±ä»¯Ó¦¸ÃÇå³ý¹ßÐÔ£¬·ñÔòÊÖ¸ÐºÍ×Ô¶¯»¯»á´ò¼Ü
            velocity = 0.0f;
        }

        //----------------------------------------------------------------------
        // Êó±ê½»»¥
        //----------------------------------------------------------------------
        void beginDrag() noexcept
        {
            dragging = true;
            velocity = 0.0f;
            instantVelocity = 0.0f;
            lastDragTimeMs = juce::Time::getMillisecondCounterHiRes();
        }

        /** Î¹Èë±¾Ö¡Êó±êÎ»ÒÆ£¨ÏñËØ£©¡£Ô¼¶¨£ºdy ÏòÏÂÎªÕý£¬ÏòÉÏÍÏÔö´óÖµ£¨ÓëÐýÅ¥Ö±¾õÒ»ÖÂ£©¡£ */
        void onDragDelta(float dxPixels, float dyPixels) noexcept
        {
            if (!dragging) return;

            const double now = juce::Time::getMillisecondCounterHiRes();
            const float dtMs = (float)juce::jmax(1.0, now - lastDragTimeMs);
            lastDragTimeMs = now;

            // ÏñËØ ¡ú Öµ£ºÍÏÂú dragRangePx ÏñËØ = ×ßÍêÒ»¸öÖµÓò¿ç¶È
            const float range = maxV - minV;
            const float pxToVal = range / dragRangePx;

            // ÏòÉÏÍÏ£¨dy<0£©+ ÏòÓÒÍÏ£¨dx>0£©= Ôö´óÖµ
            const float deltaPx = (-dyPixels) + dxPixels;
            const float deltaVal = deltaPx * pxToVal;

            const float newValue = juce::jlimit(minV, maxV, value + deltaVal);
            const float actualDelta = newValue - value;
            value = newValue;

            // ¼ÇÂ¼Ë²Ê±ËÙ¶È£¨Öµ/Ãë£©£¬ÓÃÓÚ endDrag Ê±Æô¶¯»¬ÐÐ
            instantVelocity = actualDelta / juce::jmax(1.0e-3f, dtMs * 0.001f);

            if (onValueChanged)
                onValueChanged(value);
        }

        void endDrag() noexcept
        {
            dragging = false;
            velocity = instantVelocity * inertiaGain;
            // ·ÀÖ¹¶¶¶¯£ºÌ«Ð¡Ö±½Ó¹éÁã
            if (std::abs(velocity) < stopThreshold)
                velocity = 0.0f;
        }

        //----------------------------------------------------------------------
        // Ã¿Ö¡ÍÆ½ø£¨ÓÉ SceneView / World µ÷ÓÃ£©
        //----------------------------------------------------------------------
        void tick(float dtSec) noexcept
        {
            if (dragging || velocity == 0.0f)
                return;

            // Ö¡ÂÊÎÞ¹ØË¥¼õ£ºv *= frictionPerSec ^ dt
            velocity *= std::pow(frictionPerSec, juce::jmax(1.0e-4f, dtSec));

            const float newValue = juce::jlimit(minV, maxV, value + velocity * dtSec);

            // ×²µ½±ß½ç ¡ú Á¢¼´Í£
            if (newValue <= minV || newValue >= maxV)
                velocity = 0.0f;

            if (!juce::approximatelyEqual(newValue, value))
            {
                value = newValue;
                if (onValueChanged)
                    onValueChanged(value);
            }

            if (std::abs(velocity) < stopThreshold)
                velocity = 0.0f;
        }

        //----------------------------------------------------------------------
        // »Øµ÷£¨KnobEntity ÔÚ¹¹ÔìÊ±Á¬µ½ ParamBridge::write£©
        //----------------------------------------------------------------------
        std::function<void(float /*newRealValue*/)> onValueChanged;

    private:
        // ×´Ì¬
        float value{ 0.0f };
        float velocity{ 0.0f };          // Öµ/Ãë
        float instantVelocity{ 0.0f };   // ÍÏ¶¯×îºóÒ»Ö¡µÄË²Ê±ËÙ¶È£¨Öµ/Ãë£©
        bool  dragging{ false };
        double lastDragTimeMs{ 0.0 };

        // ·¶Î§
        float minV{ 0.0f };
        float maxV{ 1.0f };

        // µ÷²Î
        float dragRangePx{ 400.0f };
        float frictionPerSec{ 0.05f };   // 1 ÃëË¥¼õµ½ 5%
        float inertiaGain{ 0.8f };
        float stopThreshold{ 1.0e-4f };  // Öµ/Ãë
    };
}
