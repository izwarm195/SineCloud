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
        // 关键：用旋转模式，不是 LinearVertical
        setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
        setMouseDragSensitivity(400);
        setVelocityBasedMode(false);

        // 关键：禁用"点击位置即跳值"行为
        setSliderSnapsToMousePosition(false);
    }

    void setWorldRadius(float r) { worldRadius = r; }
    void setKnobLabel(const juce::String& s) { label = s; repaint(); }

    //--- SceneObject ---
    void updateScreenPosition(const SceneCamera& cam) override
    {
        const auto pCenter = cam.worldToScreen(worldPos);
        const auto pRight = cam.worldToScreen({ worldPos.x + worldRadius, worldPos.y, 0 });
        const auto pUp = cam.worldToScreen({ worldPos.x, worldPos.y + worldRadius, 0 });

        rightVec = { pRight.x - pCenter.x, pRight.y - pCenter.y };
        upVec = { pUp.x - pCenter.x, pUp.y - pCenter.y };

        // 关键修复 1：用固定大 box，避免边界值随椭圆形态跳变
        // box 边长基于最大可能尺寸（worldRadius 在最俯视时的投影上限）
        const int fixedBoxSize = (int)(worldRadius * 4.0f + 80.0f);

        // 关键修复 2：center 用 floor 而不是 round，保证亚像素位移单调
        // 同时把"亚像素余量"保存下来，paint 时补偿
        const int boxX = (int)std::floor(pCenter.x) - fixedBoxSize / 2;
        const int boxY = (int)std::floor(pCenter.y) - fixedBoxSize / 2;

        subPixelOffset.x = pCenter.x - std::floor(pCenter.x);
        subPixelOffset.y = pCenter.y - std::floor(pCenter.y);

        setBounds(boxX, boxY, fixedBoxSize, fixedBoxSize);
    }


    //--- 椭圆形命中测试：鼠标只在地面椭圆内才触发 ---
    bool hitTest(int x, int y) override
    {
        const float cx = getWidth() * 0.5f;
        const float cy = getHeight() * 0.5f;
        const float dx = (float)x - cx;
        const float dy = (float)y - cy;

        // 把鼠标点 (dx, dy) 反变换回单位圆空间
        // 设单位圆向量 a*right + b*up = (dx, dy)，求 (a, b)，判断 a²+b² < 1
        const float det = rightVec.x * upVec.y - rightVec.y * upVec.x;
        if (std::abs(det) < 1e-3f) return false;
        const float a = (upVec.y * dx - upVec.x * dy) / det;
        const float b = (-rightVec.y * dx + rightVec.x * dy) / det;

        return (a * a + b * b) <= 1.0f;
    }

    //--- 自绘 ---
    void paint(juce::Graphics& g) override
    {
        const float cx = getWidth() * 0.5f + subPixelOffset.x - 0.5f;
        const float cy = getHeight() * 0.5f + subPixelOffset.y - 0.5f;


        // 椭圆背景
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

        g.setColour(juce::Colour::fromRGB(40, 50, 60).withAlpha(0.85f));
        g.fillPath(ellipse);

        g.setColour(isMouseOverOrDragging() ? juce::Colours::white
            : juce::Colour::fromRGB(150, 160, 170));
        g.strokePath(ellipse, juce::PathStrokeType(2.0f));

        // 指针：值映射到 [-135°, 135°]
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

        // 标签
        if (label.isNotEmpty())
        {
            g.setColour(juce::Colours::white);
            g.setFont(11.0f);
            const float labelY = cy + std::abs(upVec.y) + 8.0f;
            g.drawText(label, (int)(cx - 50), (int)labelY, 100, 14,
                juce::Justification::centred);
        }
    }

private:
    juce::Point<float> rightVec{ 30, 0 };
    juce::Point<float> upVec{ 0, 30 };
    juce::Point<float> subPixelOffset{ 0, 0 };    // 新增
    float worldRadius{ 35.0f };
    juce::String label;



    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(GroundKnob)
};
