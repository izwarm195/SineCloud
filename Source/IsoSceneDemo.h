#pragma once

#include <JuceHeader.h>
#include "SceneCamera.h"
#include "GroundKnob.h"
#include "PluginProcessor.h"

//==============================================================================
class IsoSceneDemo : public juce::Component,
    private juce::Timer
{
public:
    explicit IsoSceneDemo(SineCloudAudioProcessor& p)
        : processor(p)
    {
        setWantsKeyboardFocus(true);

        // ---- 球面相机参数 ----
        camera.setOrbitDistance(400.0f);
        camera.setPitch(juce::MathConstants<float>::pi * 0.4f);     // 72°
        camera.setMinPitch(juce::MathConstants<float>::pi * 0.25f); // 45° 最低
        camera.setMaxPitch(juce::MathConstants<float>::pi * 0.48f); // 86° 最高
        camera.setFocalLength(1200.0f);
        camera.setPivot({ 0, 0, 0 });

        // ---- 旋钮分布 ----
        struct KnobInit { const char* paramId; const char* label; Vec3 worldPos; };
        const KnobInit inits[] = {
            { SineCloudAudioProcessor::PARAM_PITCH,   "Pitch",   {  0.0f,    0.0f, 0.0f } },
            { SineCloudAudioProcessor::PARAM_DENSITY, "Density", {  150.0f,  100.0f, 0.0f } },
            { SineCloudAudioProcessor::PARAM_DREAM,   "Dream",   { -150.0f,  100.0f, 0.0f } },
            { SineCloudAudioProcessor::PARAM_FLOAT,   "Float",   {  150.0f, -100.0f, 0.0f } },
            { SineCloudAudioProcessor::PARAM_SHIMMER, "Shimmer", { -150.0f, -100.0f, 0.0f } },
            { SineCloudAudioProcessor::PARAM_GAIN,    "Gain",    {    0.0f,-200.0f, 0.0f } },
        };

        for (const auto& k : inits)
        {
            auto knob = std::make_unique<GroundKnob>();
            knob->setWorldRadius(35.0f);
            knob->setWorldPos(k.worldPos);
            knob->setKnobLabel(k.label);
            addAndMakeVisible(*knob);

            auto attach = std::make_unique<SA>(processor.apvts, k.paramId, *knob);
            knobs.push_back(std::move(knob));
            attachments.push_back(std::move(attach));
        }

        startTimerHz(60);
    }

    ~IsoSceneDemo() override = default;

    //==============================================================================
    void paint(juce::Graphics& g) override
    {
        g.fillAll(juce::Colour::fromRGB(20, 25, 30));

        // ---- 网格 ----
        g.setColour(juce::Colour::fromRGB(50, 60, 70));
        const int gridStep = 60;
        const int gridRange = 600;
        for (int wx = -gridRange; wx <= gridRange; wx += gridStep)
        {
            const auto a = camera.worldToScreen({ (float)wx, -(float)gridRange, 0.0f });
            const auto b = camera.worldToScreen({ (float)wx,  (float)gridRange, 0.0f });
            g.drawLine(a.x, a.y, b.x, b.y, 1.0f);
        }
        for (int wy = -gridRange; wy <= gridRange; wy += gridStep)
        {
            const auto a = camera.worldToScreen({ -(float)gridRange, (float)wy, 0.0f });
            const auto b = camera.worldToScreen({ (float)gridRange, (float)wy, 0.0f });
            g.drawLine(a.x, a.y, b.x, b.y, 1.0f);
        }

        // ---- 轴 ----
        const auto origin = camera.worldToScreen({ 0, 0, 0 });
        const auto xAxis = camera.worldToScreen({ 100, 0, 0 });
        const auto yAxis = camera.worldToScreen({ 0, 100, 0 });
        const auto zAxis = camera.worldToScreen({ 0, 0, 100 });
        g.setColour(juce::Colours::red);   g.drawLine(origin.x, origin.y, xAxis.x, xAxis.y, 2.0f);
        g.setColour(juce::Colours::green); g.drawLine(origin.x, origin.y, yAxis.x, yAxis.y, 2.0f);
        g.setColour(juce::Colours::blue);  g.drawLine(origin.x, origin.y, zAxis.x, zAxis.y, 2.0f);

        // ---- 玩家 ----
        const auto playerScreen = camera.worldToScreen(playerWorldPos);
        g.setColour(juce::Colours::red);
        g.fillEllipse(playerScreen.x - 8, playerScreen.y - 8, 16, 16);

        // 朝向线
        const auto fwd = camera.getForwardWorld();
        const auto front = camera.worldToScreen({ playerWorldPos.x + fwd.x * 30,
                                                  playerWorldPos.y + fwd.y * 30,
                                                  playerWorldPos.z });
        g.drawLine(playerScreen.x, playerScreen.y, front.x, front.y, 2.0f);

        // ---- 中心阈值圈（玩家不在此圈内不能旋转视角）----
        const float cx = getWidth() * 0.5f;
        const float cy = getHeight() * 0.5f;
        const float r = 30.0f;
        const float dxC = playerScreen.x - cx;
        const float dyC = playerScreen.y - cy;
        const bool centered = (dxC * dxC + dyC * dyC) < r * r;
        g.setColour(centered ? juce::Colours::yellow.withAlpha(0.4f)
            : juce::Colours::grey.withAlpha(0.3f));
        g.drawEllipse(cx - r, cy - r, r * 2, r * 2, 1.0f);// ---- 调试：屏幕中心对应的地面点 ----
        
        const auto centerGround = camera.worldToScreen(camera.getPivot());
        g.setColour(juce::Colours::cyan);
        g.drawLine(centerGround.x - 10, centerGround.y, centerGround.x + 10, centerGround.y, 2.0f);
        g.drawLine(centerGround.x, centerGround.y - 10, centerGround.x, centerGround.y + 10, 2.0f);

        // ---- HUD ----
        g.setColour(juce::Colours::white);
        g.setFont(13.0f);
        const float yawDeg = camera.getYaw() * 180.0f / juce::MathConstants<float>::pi;
        const float pitchDeg = camera.getPitch() * 180.0f / juce::MathConstants<float>::pi;
        const auto cp = camera.getCameraWorldPos();
        g.drawText("Player: (" + juce::String(playerWorldPos.x, 0) + "," + juce::String(playerWorldPos.y, 0) + ")  "
            "Cam: (" + juce::String(cp.x, 0) + "," + juce::String(cp.y, 0) + "," + juce::String(cp.z, 0) + ")  "
            "Yaw: " + juce::String(yawDeg, 0) + "  Pitch: " + juce::String(pitchDeg, 0)
            + (centered ? "   [Drag to rotate]" : "   [Wait centered]"),
            10, 10, 900, 20, juce::Justification::left);
    }

    void resized() override
    {
        camera.setScreenCenter({ getWidth() * 0.5f, getHeight() * 0.5f });
        for (auto& k : knobs)
            k->updateScreenPosition(camera);
    }

    //==============================================================================
    void mouseDown(const juce::MouseEvent& e) override
    {
        // 只有玩家回到屏幕中央阈值圈内才允许旋转
        const auto playerScreen = camera.worldToScreen(playerWorldPos);
        const float dx = playerScreen.x - getWidth() * 0.5f;
        const float dy = playerScreen.y - getHeight() * 0.5f;
        if (dx * dx + dy * dy > 30.0f * 30.0f)
        {
            isDragging = false;
            return;
        }

        dragStart = e.position;
        yawAtStart = camera.getYaw();
        pitchAtStart = camera.getPitch();
        isDragging = true;
    }

    void mouseDrag(const juce::MouseEvent& e) override
    {
        if (!isDragging) return;
        const float dx = e.position.x - dragStart.x;
        const float dy = e.position.y - dragStart.y;

        camera.setYaw(yawAtStart + dx * 0.005f);
        camera.setPitch(pitchAtStart - dy * 0.005f);   // 上拖 = 抬头

        for (auto& k : knobs) k->updateScreenPosition(camera);
        repaint();
    }

    void mouseUp(const juce::MouseEvent&) override { isDragging = false; }

private:
    //==============================================================================
    void timerCallback() override
    {
        // ---- 输入：箭头键沿世界轴（不跟视角）----
        Vec3 inputDir{ 0, 0, 0 };
        if (juce::KeyPress::isKeyCurrentlyDown(juce::KeyPress::upKey))    inputDir.y += 1.0f;
        if (juce::KeyPress::isKeyCurrentlyDown(juce::KeyPress::downKey))  inputDir.y -= 1.0f;
        if (juce::KeyPress::isKeyCurrentlyDown(juce::KeyPress::rightKey)) inputDir.x += 1.0f;
        if (juce::KeyPress::isKeyCurrentlyDown(juce::KeyPress::leftKey))  inputDir.x -= 1.0f;

        const float len = std::sqrt(inputDir.x * inputDir.x + inputDir.y * inputDir.y);
        if (len > 1e-4f) { inputDir.x /= len; inputDir.y /= len; }

        // ---- 玩家速度惯性 ----
        const float targetSpeed = 2.0f;
        const float accel = 0.18f;
        const float damping = 0.85f;

        const Vec3 targetVel{ inputDir.x * targetSpeed, inputDir.y * targetSpeed, 0 };
        const bool hasInput = (len > 1e-4f);
        if (hasInput)
        {
            playerVel.x += (targetVel.x - playerVel.x) * accel;
            playerVel.y += (targetVel.y - playerVel.y) * accel;
        }
        else
        {
            playerVel.x *= damping;
            playerVel.y *= damping;
            if (std::abs(playerVel.x) < 0.02f) playerVel.x = 0;
            if (std::abs(playerVel.y) < 0.02f) playerVel.y = 0;
        }

        // ---- 移动玩家 ----
        playerWorldPos.x += playerVel.x;
        playerWorldPos.y += playerVel.y;

        // ---- 相机软跟随：pivot 朝玩家位置插值 ----
                // ---- 相机跟随：移动中 lerp 慢一拍，停下后强制吸附 ----
        Vec3 pivot = camera.getPivot();
        const float pdx = playerWorldPos.x - pivot.x;
        const float pdy = playerWorldPos.y - pivot.y;

        const bool playerStill = (playerVel.x == 0 && playerVel.y == 0);

        if (playerStill)
        {
            // 停下后用更强的 lerp 快速收敛
            const float catchUp = 0.25f;
            pivot.x += pdx * catchUp;
            pivot.y += pdy * catchUp;

            // 距离够近就直接吸附
            if (std::abs(pdx) < 0.3f && std::abs(pdy) < 0.3f)
            {
                pivot.x = playerWorldPos.x;
                pivot.y = playerWorldPos.y;
            }
        }
        else
        {
            // 移动中：慢一拍 lerp，制造"角色先走、相机后跟"
            const float lerpFactor = 0.10f;
            pivot.x += pdx * lerpFactor;
            pivot.y += pdy * lerpFactor;
        }

        camera.setPivot(pivot);

        for (auto& k : knobs)
            k->updateScreenPosition(camera);

        repaint();
    }

    using SA = juce::AudioProcessorValueTreeState::SliderAttachment;

    SineCloudAudioProcessor& processor;
    SceneCamera camera;
    std::vector<std::unique_ptr<GroundKnob>> knobs;
    std::vector<std::unique_ptr<SA>>         attachments;

    Vec3 playerWorldPos{ 0, 0, 0 };
    Vec3 playerVel{ 0, 0, 0 };

    bool  isDragging{ false };
    juce::Point<float> dragStart;
    float yawAtStart{ 0.0f };
    float pitchAtStart{ 0.0f };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(IsoSceneDemo)
};
