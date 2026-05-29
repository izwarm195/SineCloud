#pragma once
#include <JuceHeader.h>
#include "SceneObject.h"
#include "InertialSlider.h"

//==============================================================================
class GroundKnob : public InertialSlider,
    public SceneObject
{
public:
    GroundKnob()
    {
        setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
        setMouseDragSensitivity(400);
        setVelocityBasedMode(false);
        setSliderSnapsToMousePosition(false);
    }

    void setWorldRadius(float r) { worldRadius = r; }
    float getWorldRadius() const { return worldRadius; }

    // [NEW] 柱体基础高度：旋钮值 0 -> base*0.9，1 -> base*1.1
    void  setWorldHeight(float h) { baseHeight = h; }
    float getWorldHeight() const { return baseHeight; }

    // [NEW] 当前实际高度（受旋钮值调制）
    float getCurrentHeight() 
    {
        const float t = (float)valueToProportionOfLength(getValue()); // 0..1
        return baseHeight * (0.9f + 0.2f * t);
    }

    void setKnobLabel(const juce::String& s) { label = s; repaint(); }

    //--- SceneObject ---
    void updateScreenPosition(const SceneCamera& cam) override
    {
        // [FIX] 旋钮盘面贴在柱体顶端，而不是地面
        const float topZ = getCurrentHeight();
        const Vec3  topCenterWorld{ worldPos.x, worldPos.y, topZ };

        const auto pCenter = cam.worldToScreen(topCenterWorld);
        const auto pRight = cam.worldToScreen({ worldPos.x + worldRadius, worldPos.y, topZ });
        const auto pUp = cam.worldToScreen({ worldPos.x, worldPos.y + worldRadius, topZ });

        rightVec = { pRight.x - pCenter.x, pRight.y - pCenter.y };
        upVec = { pUp.x - pCenter.x, pUp.y - pCenter.y };

        // [FIX] box 紧贴椭圆 AABB，不再用过大的固定 box，避免覆盖到后排柱体
        const float halfW = std::abs(rightVec.x) + std::abs(upVec.x) + 6.0f;
        const float halfH = std::abs(rightVec.y) + std::abs(upVec.y) + 6.0f;

        // 留出标签空间（向下扩 18px）
        const float labelPad = 18.0f;

        const int boxX = (int)std::floor(pCenter.x - halfW);
        const int boxY = (int)std::floor(pCenter.y - halfH);
        const int boxW = (int)std::ceil(halfW * 2.0f);
        const int boxH = (int)std::ceil(halfH * 2.0f + labelPad);

        subPixelOffset.x = pCenter.x - std::floor(pCenter.x);
        subPixelOffset.y = pCenter.y - std::floor(pCenter.y);

        // 缓存中心在 box 内的局部坐标，paint/hitTest 都用它
        localCenter = { pCenter.x - (float)boxX, pCenter.y - (float)boxY };

        setBounds(boxX, boxY, boxW, boxH);
    }

    //--- 椭圆形命中测试：只在柱顶椭圆内才触发 ---
    bool hitTest(int x, int y) override
    {
        const float dx = (float)x - localCenter.x;
        const float dy = (float)y - localCenter.y;

        const float det = rightVec.x * upVec.y - rightVec.y * upVec.x;
        if (std::abs(det) < 1e-3f) return false;

        const float a = (upVec.y * dx - upVec.x * dy) / det;
        const float b = (-rightVec.y * dx + rightVec.x * dy) / det;
        return (a * a + b * b) <= 1.0f;
    }

    //--- 自绘 ---
    void paint(juce::Graphics& g) override
    {
        const float cx = localCenter.x;
        const float cy = localCenter.y;

        // 顶面椭圆背景
        juce::Path ellipse;
        const int segs = 48;
        for (int i = 0; i <= segs; ++i)
        {
            const float a = juce::MathConstants<float>::twoPi * (float)i / (float)segs;
            const float ca = std::cos(a);
            const float sa = std::sin(a);
            const float x = cx + rightVec.x * ca + upVec.x * sa;
            const float y = cy + rightVec.y * ca + upVec.y * sa;
            if (i == 0) ellipse.startNewSubPath(x, y);
            else        ellipse.lineTo(x, y);
        }
        ellipse.closeSubPath();

        g.setColour(juce::Colour::fromRGB(40, 50, 60).withAlpha(0.95f));
        g.fillPath(ellipse);

        g.setColour(isMouseOverOrDragging() ? juce::Colours::white
            : juce::Colour::fromRGB(150, 160, 170));
        g.strokePath(ellipse, juce::PathStrokeType(2.0f));

        // 指针
        const float val = (float)valueToProportionOfLength(getValue());
        const float startA = -juce::MathConstants<float>::pi * 0.75f;
        const float endA = juce::MathConstants<float>::pi * 0.75f;
        const float ang = startA + (endA - startA) * val;
        const float ca = std::sin(ang);
        const float sa = std::cos(ang);
        const float ox = cx + (rightVec.x * ca + upVec.x * sa) * 0.85f;
        const float oy = cy + (rightVec.y * ca + upVec.y * sa) * 0.85f;

        g.setColour(juce::Colours::white);
        g.drawLine(cx, cy, ox, oy, 3.0f);
        g.fillEllipse(cx - 3, cy - 3, 6, 6);

        if (label.isNotEmpty())
        {
            g.setColour(juce::Colours::white);
            g.setFont(11.0f);
            const float labelY = cy + std::abs(upVec.y) + 6.0f;
            g.drawText(label, (int)(cx - 50), (int)labelY, 100, 14,
                juce::Justification::centred);
        }
    }

private:
    juce::Point<float> rightVec{ 30, 0 };
    juce::Point<float> upVec{ 0, 30 };
    juce::Point<float> subPixelOffset{ 0, 0 };
    juce::Point<float> localCenter{ 0, 0 };  // [NEW]

    float       worldRadius{ 35.0f };
    float       baseHeight{ 80.0f }; // [NEW]
    juce::String label;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(GroundKnob)
};
