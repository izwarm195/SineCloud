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
        void drawDebugInfo(juce::Graphics& g, int windowWidth, int windowHeight)
        {
            if (!shouldDraw || !camera) return;

            // 背景半透明面板
            const int panelW = 320;
            const int panelH = 280;
            const int margin = 10;

            g.setColour(juce::Colour(0xFF1a1a1a).withAlpha(0.85f));
            g.fillRect(margin, margin, panelW, panelH);

            // 边框
            g.setColour(juce::Colour(0xFF44ff44).withAlpha(0.6f));
            g.drawRect(margin, margin, panelW, panelH, 1);

            // 文字
            g.setColour(juce::Colour(0xFF44ff44));
            g.setFont(juce::Font(juce::Font::getDefaultMonospacedFontName(), 11.0f, juce::Font::plain));

            const int lineHeight = 16;
            int y = margin + 8;

            // 标题
            g.drawFittedText("═ CAMERA DEBUG ═", margin + 5, y, panelW - 10, lineHeight,
                juce::Justification::centred, 1);
            y += lineHeight + 4;

            // 获取 Camera 参数
            const Vec3 eye = camera->getEyeWorld();
            const Vec3 pivot = camera->getPivot();
            const Vec3 fwd = camera->getForwardOnGround();
            const Vec3 rgt = camera->getRightOnGround();

            float yaw = camera->getYaw();
            float pitch = camera->getPitch();
            float distance = camera->getDistance();

            // 转换为度数显示
            float yawDeg = juce::radiansToDegrees(yaw);
            float pitchDeg = juce::radiansToDegrees(pitch);

            // 格式化输出
            std::ostringstream oss;
            oss << std::fixed << std::setprecision(2);

            oss << "Pivot: (" << pivot.x << ", " << pivot.y << ", " << pivot.z << ")";
            g.drawFittedText(oss.str(), margin + 5, y, panelW - 10, lineHeight,
                juce::Justification::left, 1);
            y += lineHeight;

            oss.str(""); oss.clear();
            oss << "Eye: (" << eye.x << ", " << eye.y << ", " << eye.z << ")";
            g.drawFittedText(oss.str(), margin + 5, y, panelW - 10, lineHeight,
                juce::Justification::left, 1);
            y += lineHeight;

            oss.str(""); oss.clear();
            oss << "Yaw: " << yawDeg << "° (" << yaw << " rad)";
            g.drawFittedText(oss.str(), margin + 5, y, panelW - 10, lineHeight,
                juce::Justification::left, 1);
            y += lineHeight;

            oss.str(""); oss.clear();
            oss << "Pitch: " << pitchDeg << "° (" << pitch << " rad)";
            g.drawFittedText(oss.str(), margin + 5, y, panelW - 10, lineHeight,
                juce::Justification::left, 1);
            y += lineHeight;

            oss.str(""); oss.clear();
            oss << "Distance: " << distance;
            g.drawFittedText(oss.str(), margin + 5, y, panelW - 10, lineHeight,
                juce::Justification::left, 1);
            y += lineHeight;

            oss.str(""); oss.clear();
            oss << "Pitch Limits: [" << juce::radiansToDegrees(camera->getMinPitch())
                << "°, " << juce::radiansToDegrees(camera->getMaxPitch()) << "°]";
            g.drawFittedText(oss.str(), margin + 5, y, panelW - 10, lineHeight,
                juce::Justification::left, 1);
            y += lineHeight;

            oss.str(""); oss.clear();
            oss << "Viewport: " << camera->getViewportWidth() << "x" << camera->getViewportHeight();
            g.drawFittedText(oss.str(), margin + 5, y, panelW - 10, lineHeight,
                juce::Justification::left, 1);
            y += lineHeight;

            oss.str(""); oss.clear();
            oss << "Aspect: " << camera->getAspect();
            g.drawFittedText(oss.str(), margin + 5, y, panelW - 10, lineHeight,
                juce::Justification::left, 1);
            y += lineHeight + 4;

            // 分割线
            g.drawLine(margin + 5, y, margin + panelW - 5, y, 1.0f);
            y += 8;

            // Forward & Right 向量
            oss.str(""); oss.clear();
            oss << "Fwd: (" << fwd.x << ", " << fwd.y << ", " << fwd.z << ")";
            g.drawFittedText(oss.str(), margin + 5, y, panelW - 10, lineHeight,
                juce::Justification::left, 1);
            y += lineHeight;

            oss.str(""); oss.clear();
            oss << "Right: (" << rgt.x << ", " << rgt.y << ", " << rgt.z << ")";
            g.drawFittedText(oss.str(), margin + 5, y, panelW - 10, lineHeight,
                juce::Justification::left, 1);
            y += lineHeight + 4;

            // 快捷键提示
            g.setColour(juce::Colour(0xFF888888).withAlpha(0.8f));
            g.setFont(juce::Font(juce::Font::getDefaultMonospacedFontName(), 10.0f, juce::Font::plain));
            g.drawFittedText("Press F3 to toggle", margin + 5, y, panelW - 10, 12,
                juce::Justification::centred, 1);
        }

    private:
        const Camera* camera{ nullptr };
        bool shouldDraw{ false };
    };
}