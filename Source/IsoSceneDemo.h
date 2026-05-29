#pragma once

#include <JuceHeader.h>
#include "SceneCamera.h"
#include "GroundKnob.h"
#include "PluginProcessor.h"
#include "Mesh.h"

//==============================================================================
// [FIX] 柱体不再保存 height/radius/pos 的拷贝，而是指向对应的 GroundKnob
//      这样旋钮值变化、位置变化都会同步反映到碰撞与绘制
//==============================================================================
struct Pillar
{
    GroundKnob* knob{ nullptr };

    Vec3  getWorldPos() const { return knob->getWorldPos(); }
    float getRadius()   const { return knob->getWorldRadius() + 5.0f; }   // 比旋钮略大
    float getHeight()   const { return knob->getCurrentHeight(); }        // 实时高度

    bool checkCollision(float px, float py, float playerRadius) const
    {
        const auto p = getWorldPos();
        const float dx = px - p.x;
        const float dy = py - p.y;
        const float r = getRadius() + playerRadius;
        return (dx * dx + dy * dy) <= (r * r);
    }
};


//==============================================================================
class IsoSceneDemo : public juce::Component,
    public juce::OpenGLRenderer,   // [NEW]
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
        struct KnobInit { const char* paramId; const char* label; Vec3 worldPos; float height; };
        const KnobInit inits[] = {
            // ---- 左五边形：DREAM 区（5 个外环 + 1 个中心）----
            { P::PARAM_DREAM,    "Dream",    { -345.0f,    0.0f, 0.0f },   90.0f },  // 五边形中心：最高
            { P::PARAM_PITCH,    "Pitch",    { -305.92f,  135.70f, 0.0f }, 80.0f }, // 右上
            { P::PARAM_FLOAT,    "Float",    { -465.42f,   83.86f, 0.0f }, 80.0f }, // 左上
            { P::PARAM_DENSITY,  "Density",  { -465.42f,  -83.86f, 0.0f }, 80.0f }, // 左下
            { P::PARAM_GAIN,     "Gain",     { -305.92f, -135.70f, 0.0f }, 80.0f }, // 右下
            { P::PARAM_SHIMMER,  "Shimmer",  { -207.33f,    0.0f,  0.0f }, 75.0f }, // 右顶点

            // ---- 中四边形：ADSR ----
            { P::PARAM_SUSTAIN,  "S",        {    0.0f,  100.0f, 0.0f },   50.0f },
            { P::PARAM_ATTACK,   "A",        { -100.0f,    0.0f, 0.0f },   50.0f },
            { P::PARAM_DECAY,    "D",        {  100.0f,    0.0f, 0.0f },   50.0f },
            { P::PARAM_RELEASE,  "R",        {    0.0f, -100.0f, 0.0f },   50.0f },

            // ---- 右五边形：SPACE 区 ----
            { P::PARAM_DLY_TIME, "Dly Time", { 305.92f,  135.70f, 0.0f }, 85.0f }, // 左上
            { P::PARAM_DLY_FB,   "Dly Fb",   { 465.42f,   83.86f, 0.0f }, 85.0f }, // 右上
            { P::PARAM_DLY_MIX,  "Dly Mix",  { 465.42f,  -83.86f, 0.0f }, 85.0f }, // 右下
            { P::PARAM_REV_MIX,  "Rev Mix",  { 305.92f, -135.70f, 0.0f }, 85.0f }, // 左下
            { P::PARAM_REV_SIZE, "Rev Size", { 207.33f,    0.0f,  0.0f }, 100.0f }, // 左顶点：最高
        };

        for (const auto& k : inits)
        {
            auto knob = std::make_unique<GroundKnob>();

            // Dream 主旋钮单独放大
            const float radius = (juce::String(k.paramId) == P::PARAM_DREAM) ? 65.0f : 35.0f;
            knob->setWorldRadius(radius);
            knob->setWorldHeight(k.height);

            knob->setWorldPos(k.worldPos);
            knob->setKnobLabel(k.label);
            addAndMakeVisible(*knob);

            auto attach = std::make_unique<SA>(processor.apvts, k.paramId, *knob);
            knobs.push_back(std::move(knob));
            attachments.push_back(std::move(attach));
        }

        // 构建柱体碰撞数据
        buildPillars();
        // [NEW] OpenGL 渲染上下文
        glContext.setRenderer(this);
        glContext.setComponentPaintingEnabled(true); // 让 paint() 和 child component 叠在 GL 之上
        glContext.setContinuousRepainting(true);
        glContext.attachTo(*this);

        startTimerHz(60);
    }

    ~IsoSceneDemo() override
    {
        glContext.detach();
    }

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

        // ---- 绘制柱体（在玩家和旋钮之前，实现正确的深度排序） ----
        // [FIX] 先按相机深度排序：远的先画，近的后画 —— 解决遮挡顺序
        std::vector<const Pillar*> sorted;
        sorted.reserve(pillars.size());
        for (const auto& p : pillars) sorted.push_back(&p);

        const Vec3 camPos = camera.getCameraWorldPos();
        std::sort(sorted.begin(), sorted.end(),
            [&](const Pillar* a, const Pillar* b)
            {
                const auto pa = a->getWorldPos();
                const auto pb = b->getWorldPos();
                const float da = (pa.x - camPos.x) * (pa.x - camPos.x)
                    + (pa.y - camPos.y) * (pa.y - camPos.y);
                const float db = (pb.x - camPos.x) * (pb.x - camPos.x)
                    + (pb.y - camPos.y) * (pb.y - camPos.y);
                return da > db; // 远的在前
            });

        for (auto* pp : sorted) drawPillar(g, *pp);


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
        g.drawEllipse(cx - r, cy - r, r * 2, r * 2, 1.0f);

        // ---- 调试：屏幕中心对应的地面点 ----
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

    //==============================================================================
// OpenGLRenderer
//==============================================================================
    void newOpenGLContextCreated() override
    {
        // 1) 加载 cube
        int dataSize = 0;
        const char* data = BinaryData::getNamedResource("Cube_obj", dataSize);
        if (data == nullptr || dataSize == 0)
        {
            DBG("Cube_obj not found in BinaryData");
            return;
        }
        if (!cubeMesh.loadFromObjMemory(data, (size_t)dataSize))
        {
            DBG("Failed to parse cube.obj");
            return;
        }
        cubeMesh.uploadToGPU(glContext);

        // 2) 编译 shader
        shader.reset(new juce::OpenGLShaderProgram(glContext));

        const char* vs = R"(#version 330 core
        layout(location=0) in vec3 aPos;
        layout(location=1) in vec3 aNormal;
        layout(location=2) in vec2 aUV;

        uniform mat4 uModel;
        uniform mat4 uView;
        uniform mat4 uProj;

        out vec3 vNormalWS;

        void main() {
            vec4 wp = uModel * vec4(aPos, 1.0);
            vNormalWS = mat3(uModel) * aNormal;
            gl_Position = uProj * uView * wp;
        }
    )";

        const char* fs = R"(#version 330 core
        in vec3 vNormalWS;

        uniform vec3 uLightDir;
        uniform vec3 uBaseColor;

        out vec4 fragColor;

        void main() {
            vec3 N = normalize(vNormalWS);
            float lambert = max(dot(N, -normalize(uLightDir)), 0.0);
            float ambient = 0.25;
            vec3 col = uBaseColor * (ambient + (1.0 - ambient) * lambert);
            fragColor = vec4(col, 1.0);
        }
    )";

        if (!shader->addVertexShader(vs))   DBG("VS err: " << shader->getLastError());
        if (!shader->addFragmentShader(fs)) DBG("FS err: " << shader->getLastError());
        if (!shader->link())                DBG("Link err: " << shader->getLastError());
    }

    void renderOpenGL() override
    {
        using namespace juce::gl;

        const float scale = (float)glContext.getRenderingScale();
        const int   pw = (int)(getWidth() * scale);
        const int   ph = (int)(getHeight() * scale);
        glViewport(0, 0, pw, ph);

        glEnable(GL_DEPTH_TEST);
        glClearColor(0.08f, 0.10f, 0.12f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        if (shader == nullptr || !cubeMesh.isUploaded()) return;

        shader->use();

        // ---- 矩阵 ----
        const float aspect = (float)pw / juce::jmax(1, ph);
        const auto view = camera.getViewMatrix();
        const auto proj = camera.getProjMatrix(aspect);

        // cube 放原点，单位边长 = 50（场景里"大约 35 半径"那种尺度）
        juce::Matrix3D<float> model;   // identity
        // 缩放 50 倍，让 cube 在场景里看得见（你的 obj 是 ±1 → 边长 2，乘 50 → 边长 100）
        {
            float s[16] = {
                50.0f, 0,    0,    0,
                0,    50.0f, 0,    0,
                0,    0,    50.0f, 0,
                0,    0,    0,    1.0f
            };
            model = juce::Matrix3D<float>(s);
        }

        setMat4(*shader, "uModel", model);
        setMat4(*shader, "uView", view);
        setMat4(*shader, "uProj", proj);

        const GLint locLight = glGetUniformLocation(shader->getProgramID(), "uLightDir");
        const GLint locColor = glGetUniformLocation(shader->getProgramID(), "uBaseColor");
        glUniform3f(locLight, -0.4f, 0.6f, -0.7f);
        glUniform3f(locColor, 0.65f, 0.62f, 0.58f);

        cubeMesh.draw(glContext);
    }

    void openGLContextClosing() override
    {
        cubeMesh.releaseGPU(glContext);
        shader = nullptr;
    }


    bool keyPressed(const juce::KeyPress& key) override
    {
        if (key == juce::KeyPress::spaceKey)
        {
            // [NEW] 拖拽期间按 space：先结束拖拽，避免下一次 mouseDrag 把相机覆盖回去
            if (isDragging)
            {
                isDragging = false;
            }
            toggleViewMode();
            return true;
        }
        return false;
    }


private:
    //==============================================================================
    void buildPillars()
    {
        pillars.clear();
        pillars.reserve(knobs.size());
        for (auto& knob : knobs)
        {
            Pillar p;
            p.knob = knob.get();
            pillars.push_back(p);
        }
    }


    //==============================================================================
    // [FIX] 把柱体画成"底椭圆 + 顶椭圆 + 两条侧切线封闭的柱身"
    //==============================================================================
    void drawPillar(juce::Graphics& g, const Pillar& p)
    {
        const Vec3  base = p.getWorldPos();
        const float h = p.getHeight();
        const float r = p.knob->getWorldRadius() + 5.0f; // 与命中半径一致

        // 在世界空间生成底/顶圆周采样点
        constexpr int N = 36;
        juce::Point<float> bottom[N];
        juce::Point<float> top[N];
        for (int i = 0; i < N; ++i)
        {
            const float a = juce::MathConstants<float>::twoPi * (float)i / (float)N;
            const float wx = base.x + r * std::cos(a);
            const float wy = base.y + r * std::sin(a);
            bottom[i] = camera.worldToScreen({ wx, wy, 0.0f });
            top[i] = camera.worldToScreen({ wx, wy, h });
        }

        // ---- 找出柱身侧面的两条"轮廓切线" ----
        // 在屏幕上，顶/底椭圆的最左、最右切线对应同一个 i（左/右极值点）
        int leftIdx = 0, rightIdx = 0;
        for (int i = 1; i < N; ++i)
        {
            if (top[i].x < top[leftIdx].x)  leftIdx = i;
            if (top[i].x > top[rightIdx].x) rightIdx = i;
        }

        // ---- 柱身：连接两条切线之间靠"前面"那一半的弧（屏幕 y 较大的一侧）----
        // 我们走的弧应该是顶椭圆"靠近相机"那半圈，简单用屏幕 y 判断
        juce::Path body;
        auto walkArc = [&](int from, int to, juce::Point<float>* ring, bool started)
            {
                // 选择走 from -> to 经过"y 更大"的方向
                // 比较两条候选路径在中点的 y
                int midA = (from + (to - from + N) % N / 2) % N;
                int midB = (from + (to - from - N) % N / 2 + N) % N;
                bool useA = ring[midA].y > ring[midB].y;
                int step = useA ? +1 : -1;
                int i = from;
                while (true)
                {
                    if (!started) { body.startNewSubPath(ring[i]); started = true; }
                    else            body.lineTo(ring[i]);
                    if (i == to) break;
                    i = (i + step + N) % N;
                }
            };

        walkArc(leftIdx, rightIdx, top, false);   // 顶弧（前半）
        // 沿右切线下到底
        body.lineTo(bottom[rightIdx]);
        // 底弧反向走回左切线
        {
            // 重用 walkArc 但起点已经放好，这里手动连
            int i = rightIdx;
            // 决定方向：和 top 同样取 y 更大那条
            int midA = (rightIdx + (leftIdx - rightIdx + N) % N / 2) % N;
            int midB = (rightIdx + (leftIdx - rightIdx - N) % N / 2 + N) % N;
            bool useA = bottom[midA].y > bottom[midB].y;
            int step = useA ? +1 : -1;
            while (i != leftIdx)
            {
                i = (i + step + N) % N;
                body.lineTo(bottom[i]);
            }
        }
        body.closeSubPath();

        // ---- 颜色：高度越高越亮，并加一点立体感 ----
        const float t = juce::jlimit(0.0f, 1.0f, h / 120.0f);
        const auto colSide = juce::Colour::fromRGB(
            (juce::uint8)(40 + 30 * t),
            (juce::uint8)(55 + 35 * t),
            (juce::uint8)(70 + 40 * t)).withAlpha(0.92f);

        g.setColour(colSide);
        g.fillPath(body);

        g.setColour(juce::Colour::fromRGB(150, 170, 190).withAlpha(0.9f));
        g.strokePath(body, juce::PathStrokeType(1.2f));

        // ---- 顶面椭圆（独立填充，让顶端略亮）----
        juce::Path topCap;
        topCap.startNewSubPath(top[0]);
        for (int i = 1; i < N; ++i) topCap.lineTo(top[i]);
        topCap.closeSubPath();

        g.setColour(juce::Colour::fromRGB(70, 90, 110).withAlpha(0.95f));
        g.fillPath(topCap);
        g.setColour(juce::Colour::fromRGB(200, 220, 240).withAlpha(0.9f));
        g.strokePath(topCap, juce::PathStrokeType(1.2f));
    }


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
            toPitch = juce::MathConstants<float>::pi * 0.499f;  // 89.8°
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

        // [FIX] 碰撞解算：sliding（沿切线滑动），不再 break、不再清零整条速度向量
//       多 pass 解多柱体夹角，单帧最多 3 次以收敛
        const float playerRadius = 10.0f;
        for (int pass = 0; pass < 3; ++pass)
        {
            bool anyHit = false;
            for (const auto& pillar : pillars)
            {
                const auto pp = pillar.getWorldPos();
                const float r = pillar.getRadius() + playerRadius;
                const float dx = playerWorldPos.x - pp.x;
                const float dy = playerWorldPos.y - pp.y;
                const float d2 = dx * dx + dy * dy;

                if (d2 < r * r && d2 > 1e-6f)
                {
                    anyHit = true;
                    const float dist = std::sqrt(d2);
                    // 法向（指向玩家外侧）
                    const float nx = dx / dist;
                    const float ny = dy / dist;

                    // 1) 位置投影到圆周外侧（带极小偏移防止持续接触）
                    const float push = (r - dist) + 0.01f;
                    playerWorldPos.x += nx * push;
                    playerWorldPos.y += ny * push;

                    // 2) 速度只去掉法向分量，保留切向分量（sliding）
                    const float vn = playerVel.x * nx + playerVel.y * ny;
                    if (vn < 0.0f) // 只在朝柱心的速度上扣
                    {
                        playerVel.x -= vn * nx;
                        playerVel.y -= vn * ny;
                    }
                }
            }
            if (!anyHit) break;
        }


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

        // [FIX] Z-sort：让靠近相机的旋钮压在远处旋钮之上，避免远处旋钮被前排 box 截获鼠标
        std::vector<GroundKnob*> sortedKnobs;
        sortedKnobs.reserve(knobs.size());
        for (auto& k : knobs) sortedKnobs.push_back(k.get());
        const Vec3 cp = camera.getCameraWorldPos();
        std::sort(sortedKnobs.begin(), sortedKnobs.end(),
            [&](GroundKnob* a, GroundKnob* b)
            {
                const auto pa = a->getWorldPos();
                const auto pb = b->getWorldPos();
                const float da = (pa.x - cp.x) * (pa.x - cp.x) + (pa.y - cp.y) * (pa.y - cp.y);
                const float db = (pb.x - cp.x) * (pb.x - cp.x) + (pb.y - cp.y) * (pb.y - cp.y);
                return da > db;
            });
        for (auto* k : sortedKnobs) k->toFront(false); // 远的先 toFront，最后近的在最上层

    }

    //---- GL ----
    juce::OpenGLContext glContext;
    Mesh cubeMesh;
    std::unique_ptr<juce::OpenGLShaderProgram> shader;

    static void setMat4(juce::OpenGLShaderProgram& sh, const char* name,
        const juce::Matrix3D<float>& m)
    {
        const GLint loc = juce::gl::glGetUniformLocation(sh.getProgramID(), name);
        if (loc >= 0)
            juce::gl::glUniformMatrix4fv(loc, 1, juce::gl::GL_FALSE, m.mat);
    }
    //-------------

    using SA = juce::AudioProcessorValueTreeState::SliderAttachment;

    SineCloudAudioProcessor& processor;
    SceneCamera camera;
    std::vector<std::unique_ptr<GroundKnob>> knobs;
    std::vector<std::unique_ptr<SA>>         attachments;
    std::vector<Pillar> pillars;  // 柱体碰撞数据

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