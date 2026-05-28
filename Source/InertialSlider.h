#pragma once

#include <JuceHeader.h>

//==============================================================================
// ´ø¹ßÐÔ»¬ÐÐµÄ Slider£ºËÉÊÖºó»ùÓÚÊó±êËÙ¶È¼ÌÐø»¬¶¯£¬ËÙ¶È°´Ä¦²ÁÏµÊýË¥¼õ
//==============================================================================
class InertialSlider : public juce::Slider,
    private juce::Timer
{
public:
    InertialSlider() = default;

    /** Ä¦²ÁÏµÊý£¬Ã¿Ö¡ËÙ¶È³ËÒÔÕâ¸öÖµ¡£0.92 ~ 0.97 ±È½Ï×ÔÈ»¡£
        ÖµÔ½½Ó½ü 1 »¬µÃÔ½Ô¶£¬Ô½½Ó½ü 0 Ô½¿ìÍ£¡£ */
    void setFriction(float f) { friction = juce::jlimit(0.5f, 0.999f, f); }

    /** ¹ßÐÔÁéÃô¶È¡£³Ëµ½ËÉÊÖËÙ¶ÈÉÏµÄ·Å´óÏµÊý¡£ */
    void setInertiaGain(float g) { inertiaGain = juce::jmax(0.0f, g); }

    /** ËÙ¶ÈµÍÓÚ´ËãÐÖµÊ±Í£Ö¹»¬ÐÐ£¨±ÜÃâÎÞÏÞ¶¶¶¯£©¡£ */
    void setStopThreshold(float t) { stopThreshold = juce::jmax(0.0f, t); }

    void mouseDown(const juce::MouseEvent& e) override
    {
        stopTimer();
        velocity = 0.0f;
        currentDragVelocity = 0.0f;       // ← 新增
        lastY = (float)e.position.y;
        lastX = (float)e.position.x;
        lastTimeMs = (float)juce::Time::getMillisecondCounterHiRes();
        Slider::mouseDown(e);
    }


    void mouseDrag(const juce::MouseEvent& e) override
    {
        const float now = (float)juce::Time::getMillisecondCounterHiRes();
        const float dt = juce::jmax(1.0f, now - lastTimeMs);

        const float dy = (float)e.position.y - lastY;
        const float dx = (float)e.position.x - lastX;

        // 当前帧瞬时速度（与 RotaryHorizontalVerticalDrag 方向一致）
        const float instantVel = (-dy + dx) / dt;

        // 用低通滤波累积，避免被"最后一帧抖动"主导
        // alpha 越小越平滑，越大越敏感（这里 0.3 是中等响应）
        const float alpha = 0.05f;
        currentDragVelocity = currentDragVelocity * (1.0f - alpha) + instantVel * alpha;

        lastY = (float)e.position.y;
        lastX = (float)e.position.x;
        lastTimeMs = now;
        Slider::mouseDrag(e);
    }


    void mouseUp(const juce::MouseEvent& e) override
    {
        Slider::mouseUp(e);

        const auto range = getMaximum() - getMinimum();
        if (range <= 0.0)
            return;

        // 关键修复：松手时如果距离上次实际移动超过 60ms，认为玩家"停下再松手"，不施加惯性
        const float now = (float)juce::Time::getMillisecondCounterHiRes();
        const float idleMs = now - lastTimeMs;
        if (idleMs > 60.0f)
        {
            currentDragVelocity = 0.0f;
            return;
        }

        if (std::abs(currentDragVelocity) < 0.05f)
            return;

        // px/ms → param/frame
        const float sens = (float)getMouseDragSensitivity();
        const float pxPerMs = currentDragVelocity;
        const float frameMs = 16.0f;
        const float pxPerFrame = pxPerMs * frameMs;
        const float valuePerFrame = (pxPerFrame / sens) * (float)range;

        velocity = valuePerFrame * inertiaGain;
        startTimerHz(60);
    }


private:
    void timerCallback() override
    {
        if (std::abs(velocity) < stopThreshold)
        {
            velocity = 0.0f;
            stopTimer();
            return;
        }

        double v = getValue() + velocity;
        v = juce::jlimit((double)getMinimum(), (double)getMaximum(), v);
        setValue(v, juce::sendNotificationSync);

        // ´¥½çÍ£ÏÂ
        if (v <= getMinimum() || v >= getMaximum())
        {
            velocity = 0.0f;
            stopTimer();
            return;
        }

        velocity *= friction;
    }

    float friction{ 0.9f };
    float inertiaGain{ 0.6f };
    float stopThreshold{ 0.0005f };

    float currentDragVelocity{ 0.0f };
    float lastX{ 0.0f }, lastY{ 0.0f };
    float lastTimeMs{ 0.0f };
    float velocity{ 0.0f };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(InertialSlider)
};
