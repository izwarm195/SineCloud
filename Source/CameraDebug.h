/*
  ==============================================================================
    CameraDebugOverlay.h
    Layer 2: Debug Visualization
    屏幕实时显示 Camera 参数的调试面板，支持 F3 切换显示/隐藏
    ★ 在 OpenGL 中绘制（不是 Component）
  ==============================================================================
*/
#pragma once

#include <JuceHeader.h>
#include "Camera.h"
#include <sstream>
#include <iomanip>

namespace sc
{
    class CameraDebugOverlay
    {
    public:
        CameraDebugOverlay() = default;

        void setCamera(const Camera* cam) noexcept { camera = cam; }

        void setVisible(bool visible) noexcept { shouldDraw = visible; }
        void toggleVisible() noexcept { shouldDraw = !shouldDraw; }
        bool isVisible() const noexcept { return shouldDraw; }

        /** 在 renderOpenGL 中调用此方法，使用 JUCE Graphics 在屏幕上方绘制 */
        // CameraDebug.h 顶部新增
#include <cstdio>

// 替换原来的 std::ostringstream 写法
        void drawDebugInfo(juce::Graphics& g, int windowWidth, int windowHeight)
        {
            if (!shouldDraw || !camera) return;

            const int panelW = 320;
            const int panelH = 280;
            const int margin = 10;

            g.setColour(juce::Colour(0xFF1a1a1a).withAlpha(0.85f));
            g.fillRect(margin, margin, panelW, panelH);
            g.setColour(juce::Colour(0xFF44ff44).withAlpha(0.6f));
            g.drawRect(margin, margin, panelW, panelH, 1);

            g.setColour(juce::Colour(0xFF44ff44));
            g.setFont(juce::Font(juce::Font::getDefaultMonospacedFontName(),
                11.0f, juce::Font::plain));

            const int lineHeight = 16;
            int y = margin + 8;

            auto drawLine = [&](const juce::String& s)
                {
                    g.drawFittedText(s, margin + 5, y, panelW - 10,
                        lineHeight, juce::Justification::left, 1);
                    y += lineHeight;
                };

            // 标题（纯 ASCII，不要中文/全角）
            g.drawFittedText(juce::String("== CAMERA DEBUG =="),
                margin + 5, y, panelW - 10,
                lineHeight, juce::Justification::centred, 1);
            y += lineHeight + 4;

            const Vec3 eye = camera->getEyeWorld();
            const Vec3 pivot = camera->getPivot();
            const Vec3 fwd = camera->getForwardOnGround();
            const Vec3 rgt = camera->getRightOnGround();
            const float yaw = camera->getYaw();
            const float pitch = camera->getPitch();
            const float distance = camera->getDistance();
            const float yawDeg = juce::radiansToDegrees(yaw);
            const float pitchDeg = juce::radiansToDegrees(pitch);

            char buf[128];

            std::snprintf(buf, sizeof(buf), "Pivot: (%.2f, %.2f, %.2f)",
                pivot.x, pivot.y, pivot.z);
            drawLine(juce::String::fromUTF8(buf));

            std::snprintf(buf, sizeof(buf), "Eye:   (%.2f, %.2f, %.2f)",
                eye.x, eye.y, eye.z);
            drawLine(juce::String::fromUTF8(buf));

            std::snprintf(buf, sizeof(buf), "Yaw:   %.2f deg (%.3f rad)", yawDeg, yaw);
            drawLine(juce::String::fromUTF8(buf));

            std::snprintf(buf, sizeof(buf), "Pitch: %.2f deg (%.3f rad)", pitchDeg, pitch);
            drawLine(juce::String::fromUTF8(buf));

            std::snprintf(buf, sizeof(buf), "Distance: %.2f", distance);
            drawLine(juce::String::fromUTF8(buf));

            std::snprintf(buf, sizeof(buf), "Pitch Limits: [%.1f, %.1f]",
                juce::radiansToDegrees(camera->getMinPitch()),
                juce::radiansToDegrees(camera->getMaxPitch()));
            drawLine(juce::String::fromUTF8(buf));

            std::snprintf(buf, sizeof(buf), "Viewport: %dx%d",
                camera->getViewportWidth(), camera->getViewportHeight());
            drawLine(juce::String::fromUTF8(buf));

            std::snprintf(buf, sizeof(buf), "Aspect: %.3f", camera->getAspect());
            drawLine(juce::String::fromUTF8(buf));

            y += 4;
            g.drawLine((float)(margin + 5), (float)y,
                (float)(margin + panelW - 5), (float)y, 1.0f);
            y += 8;

            std::snprintf(buf, sizeof(buf), "Fwd:   (%.2f, %.2f, %.2f)",
                fwd.x, fwd.y, fwd.z);
            drawLine(juce::String::fromUTF8(buf));

            std::snprintf(buf, sizeof(buf), "Right: (%.2f, %.2f, %.2f)",
                rgt.x, rgt.y, rgt.z);
            drawLine(juce::String::fromUTF8(buf));

            y += 4;
            g.setColour(juce::Colour(0xFF888888).withAlpha(0.8f));
            g.setFont(juce::Font(juce::Font::getDefaultMonospacedFontName(),
                10.0f, juce::Font::plain));
            g.drawFittedText(juce::String("Press F3 to toggle"),
                margin + 5, y, panelW - 10, 12,
                juce::Justification::centred, 1);
        }


    private:
        const Camera* camera{ nullptr };
        bool shouldDraw{ false };
    };
}