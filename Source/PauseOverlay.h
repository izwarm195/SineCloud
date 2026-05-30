/*
==============================================================================
PauseOverlay.h
Layer 2: Debug & UI
ESC 暂停菜单 —— Minecraft 风格按钮、半透明遮罩、Resume 按钮居中。
==============================================================================
*/
#pragma once
#include <JuceHeader.h>

namespace sc
{

    class PauseOverlay : public juce::Component
    {
    public:
        PauseOverlay()
        {
            
        }

        std::function<void()> onResume;

        void paint(juce::Graphics& g) override
        {
            auto b = getLocalBounds();

            // 半透明黑色遮罩 —— 模拟场景冻结的视觉效果
            g.setColour(juce::Colour(0xCC000000));
            g.fillAll();

            drawMinecraftButton(g, resumeBtn, "Resume", resumeHovered, resumePressed);
        }

        void resized() override
        {
            const int btnW = 200;
            const int btnH = 24;
            resumeBtn = juce::Rectangle<int>(
                (getWidth() - btnW) / 2,
                (getHeight() - btnH) / 2,
                btnW, btnH);
        }

        void mouseMove(const juce::MouseEvent& e) override
        {
            bool was = resumeHovered;
            resumeHovered = resumeBtn.contains(e.position.toInt());
            if (was != resumeHovered) repaint();
        }

        void mouseDown(const juce::MouseEvent& e) override
        {
            if (resumeBtn.contains(e.position.toInt()))
            {
                resumePressed = true;
                repaint();
            }
        }

        void mouseUp(const juce::MouseEvent& e) override
        {
            bool was = resumePressed;
            resumePressed = false;
            if (was && resumeBtn.contains(e.position.toInt()) && onResume)
                onResume();
            repaint();
        }

        void mouseExit(const juce::MouseEvent&) override
        {
            resumeHovered = false;
            resumePressed = false;
            repaint();
        }

    private:
        // ================================================================
        // Minecraft 按钮绘制逻辑
        //
        // 三层构造：
        //   1. 底色填充（hover 时变亮，按下时变暗）
        //   2. 2px 边框 —— 上/左亮色（高光），下/右暗色（阴影）
        //      按下时边框颜色翻转，模拟"陷入"的触感
        //   3. 文字 + 1px 下阴影；按下时文字整体偏移 (1,1)
        // ================================================================
        void drawMinecraftButton(juce::Graphics& g,
            juce::Rectangle<int> bounds,
            const juce::String& text,
            bool hovered,
            bool pressed)
        {
            constexpr int border = 2;

            // 底色
            juce::Colour bg;
            if (pressed)
                bg = juce::Colour(0xFF757575);
            else if (hovered)
                bg = juce::Colour(0xFF9E9E9E);
            else
                bg = juce::Colour(0xFF898989);

            g.setColour(bg);
            g.fillRect(bounds);

            // 边框颜色（按下时翻转）
            juce::Colour light(0xFFB4B4B4);
            juce::Colour dark(0xFF4C4C4C);
            if (pressed) std::swap(light, dark);

            // 上边框 + 左边框
            g.setColour(light);
            g.fillRect(bounds.getX(), bounds.getY(), bounds.getWidth(), border);
            g.fillRect(bounds.getX(), bounds.getY(), border, bounds.getHeight());

            // 下边框 + 右边框
            g.setColour(dark);
            g.fillRect(bounds.getX(), bounds.getBottom() - border, bounds.getWidth(), border);
            g.fillRect(bounds.getRight() - border, bounds.getY(), border, bounds.getHeight());

            // 文字区域
            auto textArea = bounds.reduced(border);
            if (pressed)
                textArea.translate(1, 1);

            g.setFont(juce::Font("Courier New", 12.0f, juce::Font::plain));

            // 文字阴影
            g.setColour(juce::Colour(0xFF3F3F3F));
            g.drawFittedText(text, textArea.translated(1, 1),
                juce::Justification::centred, 1);
            // 文字本体
            g.setColour(juce::Colour(0xFFE0E0E0));
            g.drawFittedText(text, textArea,
                juce::Justification::centred, 1);
        }

        juce::Rectangle<int> resumeBtn;
        bool resumeHovered = false;
        bool resumePressed = false;
    };

} // namespace sc
