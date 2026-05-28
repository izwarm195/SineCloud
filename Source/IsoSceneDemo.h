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
    enum class ViewMode { Game, Top };

    explicit IsoSceneDemo(SineCloudAudioProcessor& p)
        : processor(p)
    {
        setWantsKeyboardFocus(true);

        // ---- 球面相机参数 ----
        camera.setOrbitDistance(800.0f);
        camera.setPitch(juce::MathConstants<float>::pi * 0.1f);     // 72°
        camera.setMinPitch(juce::MathConstants<float>::pi * 0.1f); // 45° 最低
        camera.setMaxPitch(juce::MathConstants<float>::pi * 0.5f); // 86° 最高
        camera.setFocalLength(1800.0f);
        camera.setPivot({ 0, 0, 0 });
        // 保存初始 focal
        savedFocal = camera.getFocalLength();

        // ---- 旋钮分布：左五边形(DREAM) + 中四边形(ADSR) + 右五边形(SPACE) + 中央 Dream ----
        using P = SineCloudAudioProcessor;
        struct KnobInit { const char* paramId; const char* label; Vec3 worldPos; };
        const KnobInit inits[] = {
            // ---- 左五边形：DREAM 区（5 个外环 + 1 个中心）----
            { P::PARAM_DREAM,    "Dream",    { -345.0f,    0.0f, 0.0f } },  // 五边形中心
            { P::PARAM_PITCH,    "Pitch",    { -305.92f,  135.70f, 0.0f } }, // 右上
            { P::PARAM_FLOAT,    "Float",    { -465.42f,   83.86f, 0.0f } }, // 左上
            { P::PARAM_DENSITY,  "Density",  { -465.42f,  -83.86f, 0.0f } }, // 左下
            { P::PARAM_GAIN,     "Gain",     { -305.92f, -135.70f, 0.0f } }, // 右下
            { P::PARAM_SHIMMER,  "Shimmer",  { -207.33f,    0.0f,  0.0f } }, // 右顶点

            // ---- 中四边形：ADSR ----
            { P::PARAM_SUSTAIN,  "S",        {    0.0f,  100.0f, 0.0f } },
            { P::PARAM_ATTACK,   "A",        { -100.0f,    0.0f, 0.0f } },
            { P::PARAM_DECAY,    "D",        {  100.0f,    0.0f, 0.0f } },
            { P::PARAM_RELEASE,  "R",        {    0.0f, -100.0f, 0.0f } },

            // ---- 右五边形：SPACE 区 ----
            { P::PARAM_DLY_TIME, "Dly Time", { 305.92f,  135.70f, 0.0f } }, // 左上
            { P::PARAM_DLY_FB,   "Dly Fb",   { 465.42f,   83.86f, 0.0f } }, // 右上
            { P::PARAM_DLY_MIX,  "Dly Mix",  { 465.42f,  -83.86f, 0.0f } }, // 右下
            { P::PARAM_REV_MIX,  "Rev Mix",  { 305.92f, -135.70f, 0.0f } }, // 左下
            { P::PARAM_REV_SIZE, "Rev Size", { 207.33f,    0.0f,  0.0f } }, // 左顶点
        };

        for (const auto& k : inits)
        {
            auto knob = std::make_unique<GroundKnob>();

            // Dream 主旋钮单独放大
            const float radius = (juce::String(k.paramId) == P::PARAM_DREAM) ? 65.0f : 35.0f;
            knob->setWorldRadius(radius);

            knob->setWorldPos(k.worldPos);
            knob->setKnobLabel(k.label);
            addAndMakeVisible(*knob);

            auto attach = std::make_unique<SA>(processor.apvts, k.paramId, *knob);
            knobs.push_back(std::move(knob));
            attachments.push_back(std::move(attach));
        }



        /*for (const auto& k : inits)
        {
            auto knob = std::make_unique<GroundKnob>();
            knob->setWorldRadius(35.0f);
            knob->setWorldPos(k.worldPos);
            knob->setKnobLabel(k.label);
            addAndMakeVisible(*knob);

            auto attach = std::make_unique<SA>(processor.apvts, k.paramId, *knob);
            knobs.push_back(std::move(knob));
            attachments.push_back(std::move(attach));
        }*/

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

        // 朝向线：跟随玩家实际移动方向（停下时保留最后朝向）
        if (std::abs(playerVel.x) > 0.01f || std::abs(playerVel.y) > 0.01f)
        {
            const float vlen = std::sqrt(playerVel.x * playerVel.x + playerVel.y * playerVel.y);
            lastFacing.x = playerVel.x / vlen;
            lastFacing.y = playerVel.y / vlen;
        }
        const auto front = camera.worldToScreen({ playerWorldPos.x + lastFacing.x * 30.0f,
                                                  playerWorldPos.y + lastFacing.y * 30.0f,
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
        g.drawText(viewMode == ViewMode::Top ? "VIEW: TOP-DOWN [Space to return]"
            : "VIEW: GAME [Space for top-down]",
            10, 30, 600, 20, juce::Justification::left);

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
        // 俯视模式不允许拖动旋转视角
        if (viewMode == ViewMode::Top || inTransition)
        {
            isDragging = false;
            return;
        }

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
        camera.setPitch(pitchAtStart + dy * 0.005f);   // 上拖 = 抬头

        for (auto& k : knobs) k->updateScreenPosition(camera);
        repaint();
    }

    void mouseUp(const juce::MouseEvent&) override { isDragging = false; }

    bool keyPressed(const juce::KeyPress& key) override
    {
        if (key == juce::KeyPress::spaceKey)
        {
            toggleViewMode();
            return true;
        }
        return false;
    }

private:
    //==============================================================================
    void toggleViewMode()
    {
        if (inTransition) return;  // 切换中不响应

        fromYaw = camera.getYaw();
        fromPitch = camera.getPitch();
        fromOrbit = camera.getOrbitDistance();
        fromPivot = camera.getPivot();
        fromFocal = camera.getFocalLength(); // 保存过渡起点 focal

        if (viewMode == ViewMode::Game)
        {
            // 进入俯视：保存当前游戏视角状态
            savedYaw = fromYaw;
            savedPitch = fromPitch;
            savedOrbit = fromOrbit;
            savedFocal = fromFocal; // 保存游戏视角 focal

            // 俯视目标：相机在 (0,0,0) 正上方，pitch 接近 90°
            toYaw = 0.0f;
            toPitch = juce::MathConstants<float>::pi * 0.499f;  // 89.8°（最大 pitch 是 0.48π=86.4°，需要先放宽 max）
            toOrbit = 1200.0f;
            toPivot = { 0.0f, 0.0f, 0.0f };
            toFocal = 1200.0f; // 俯视目标 focal
            viewMode = ViewMode::Top;
        }
        else
        {
            // 回到游戏视角：恢复保存的状态
            toYaw = savedYaw;
            toPitch = savedPitch;
            toOrbit = savedOrbit;
            toPivot = playerWorldPos;
            toFocal = savedFocal; // 恢复为游戏视角 focal
            viewMode = ViewMode::Game;
        }

        // 临时放宽 pitch 上限（俯视需要近 90°）
        camera.setMaxPitch(juce::MathConstants<float>::pi * 0.499f);

        inTransition = true;
        transitionT = 0.0f;
    }

    
    void timerCallback() override
    {
        // ---- 视角过渡插值 ----
        if (inTransition)
        {
            transitionT += (1.0f / 60.0f) / transitionDuration;
            if (transitionT >= 1.0f)
            {
                transitionT = 1.0f;
                inTransition = false;
            }
            // 缓动函数（smoothstep）
            const float t = transitionT;
            const float k = t * t * (3.0f - 2.0f * t);

            camera.setYaw(fromYaw + (toYaw - fromYaw) * k);
            camera.setPitch(fromPitch + (toPitch - fromPitch) * k);
            camera.setOrbitDistance(fromOrbit + (toOrbit - fromOrbit) * k);
            // 插值 focal
            camera.setFocalLength(fromFocal + (toFocal - fromFocal) * k);

            Vec3 piv{
                fromPivot.x + (toPivot.x - fromPivot.x) * k,
                fromPivot.y + (toPivot.y - fromPivot.y) * k,
                fromPivot.z + (toPivot.z - fromPivot.z) * k
            };
            camera.setPivot(piv);

            for (auto& k2 : knobs) k2->updateScreenPosition(camera);
            repaint();
            return;  // 过渡期间不处理移动和跟随
        }

        // ---- 俯视模式：相机锁死，玩家不能动 ----
        if (viewMode == ViewMode::Top)
        {
            repaint();
            return;
        }

        // ---- 游戏模式：原有逻辑 ----

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

        playerWorldPos.x += playerVel.x;
        playerWorldPos.y += playerVel.y;

        // ---- 相机跟随 ----
        Vec3 pivot = camera.getPivot();
        const float pdx = playerWorldPos.x - pivot.x;
        const float pdy = playerWorldPos.y - pivot.y;
        const bool playerStill = (playerVel.x == 0 && playerVel.y == 0);

        if (playerStill)
        {
            const float catchUp = 0.25f;
            pivot.x += pdx * catchUp;
            pivot.y += pdy * catchUp;
            if (std::abs(pdx) < 0.3f && std::abs(pdy) < 0.3f)
            {
                pivot.x = playerWorldPos.x;
                pivot.y = playerWorldPos.y;
            }
        }
        else
        {
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
    Vec3 lastFacing{ 0.0f, 1.0f, 0.0f };   // 最后一次移动朝向（默认朝 +Y）


    bool  isDragging{ false };
    juce::Point<float> dragStart;
    float yawAtStart{ 0.0f };
    float pitchAtStart{ 0.0f };

    // ---- 视角切换 ----
    ViewMode viewMode{ ViewMode::Game };
    bool inTransition{ false };
    float transitionT{ 0.0f };           // 0 → 1
    float transitionDuration{ 0.25f };   // 秒
    // 过渡起点
    float fromYaw{ 0 }, fromPitch{ 0 }, fromOrbit{ 0 };
    Vec3 fromPivot{ 0, 0, 0 };
    float fromFocal{ 0.0f };
    // 过渡终点
    float toYaw{ 0 }, toPitch{ 0 }, toOrbit{ 0 };
    Vec3 toPivot{ 0, 0, 0 };
    float toFocal{ 0.0f };
    // 游戏视角的"上次状态"，从俯视切回时恢复
    float savedYaw{ 0.0f };
    float savedPitch{ juce::MathConstants<float>::pi * 0.4f };
    float savedOrbit{ 400.0f };
    float savedFocal{ 1800.0f };


    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(IsoSceneDemo)
};
