/*
  ==============================================================================
    SceneView.h
    Layer 2 + 3 接合：World 接入版。
    若 setWorld(world) 已注入，渲染交给 world.draw + world.update；
    否则退化到自带的调试场景（地面 + 立方体 + 旋转盒）。
  ==============================================================================*/
#pragma once

#include <JuceHeader.h>
#include "Camera.h"
#include "Renderer.h"
#include "Mesh.h"
#include "Lighting.h"
#include "World.h"
#include "InputState.h"

namespace sc
{
    class SceneView : public juce::Component,
        public juce::OpenGLRenderer,
        private juce::Timer
    {
    public:
        SceneView()
        {
            setOpaque(true);
            setWantsKeyboardFocus(true);

            context.setRenderer(this);
            context.setComponentPaintingEnabled(false);
            context.setContinuousRepainting(true);  // ← 关键！
            context.attachTo(*this);

            // startTimerHz(60);
        }

        ~SceneView() override
        {
            stopTimer();
            context.detach();
        }

        Camera& getCamera()   noexcept { return camera; }
        Lighting& getLighting() noexcept { return lighting; }

        /** 由 PluginEditor 注入；World 必须比 SceneView 长寿，或在析构前 setWorld(nullptr)。 */
        void setWorld(World* w) noexcept { world = w; }

        //----------------------------------------------------------------------
        // juce::Component
        //----------------------------------------------------------------------
        void paint(juce::Graphics&) override {}

        void resized() override
        {
            // ★ 确保在主线程中运行
            jassert(juce::MessageManager::getInstance()->isThisTheMessageThread());

            pendingVpW = getWidth();
            pendingVpH = getHeight();
            viewportDirty.store(true, std::memory_order_release);

            context.triggerRepaint();
        }

        //----------------------------------------------------------------------
        // juce::OpenGLRenderer
        //----------------------------------------------------------------------
        void newOpenGLContextCreated() override
        {
            renderer = std::make_unique<Renderer>(context);
            if (!renderer->initialise()) { renderer.reset(); return; }

            // 调试 mesh（仅当没有 world 时使用）
            debugGrid = Mesh::createGrid(20.0f, 1.0f); debugGrid->uploadToGPU(context);
            debugCube = Mesh::createBox(2.0f, 2.0f, 2.0f); debugCube->uploadToGPU(context);

            // World 共享 mesh
            if (world) world->uploadMeshes(context);

            camera.setPivot({ 0, 0, 0 });
            camera.setOrbit(juce::degreesToRadians(35.0f),
                juce::degreesToRadians(55.0f),
                14.0f);
            camera.setPerspective(50.0f, 0.1f, 200.0f);

            //解决 standalone/VST3 初始黑屏
            camera.setViewport(getWidth(), getHeight());

        }

        void renderOpenGL() override
        {
            if (renderer == nullptr) return;

            // [新增] 在 GL 线程中安全更新 viewport
            if (viewportDirty.load(std::memory_order_acquire))
            {
                camera.setViewport(pendingVpW, pendingVpH);
                viewportDirty.store(false, std::memory_order_release);
            }

            // ---- dt ----
            const double now = juce::Time::getMillisecondCounterHiRes() * 0.001;
            const float dt = (lastRenderSec > 0.0)
                ? (float)juce::jlimit(0.001, 0.1, now - lastRenderSec)
                : 1.0f / 60.0f;
            lastRenderSec = now;

            // ---- 输入快照 ----
            InputState in = pollInput();
            in.viewportW = camera.getViewportWidth();
            in.viewportH = camera.getViewportHeight();

            // ---- update ----
            if (world)
                world->update(dt, in, camera);

            // ---- draw ----
            renderer->beginFrame(camera, lighting);

            if (world)
            {
                world->draw(*renderer, camera);
            }
            else if (debugGrid && debugCube)
            {
                renderer->drawLines(*debugGrid, identity(), { 0.25f, 0.30f, 0.35f });
                renderer->drawMesh(*debugCube, translation({ 0, 0, 1.0f }),
                    { 0.55f, 0.50f, 0.45f });
            }

            renderer->endFrame();

            // 帧末重置一次性事件
            mouseJustPressedFlag = false;
            mouseJustReleasedFlag = false;
            lastMousePos = currentMousePos;
        }

        void openGLContextClosing() override
        {
            if (world) world->releaseMeshes(context);

            if (debugGrid) debugGrid->releaseGPU(context);
            if (debugCube) debugCube->releaseGPU(context);
            debugGrid.reset();
            debugCube.reset();

            renderer.reset();
        }

        //----------------------------------------------------------------------
        // 输入：键盘走 isKeyCurrentlyDown，鼠标走事件
        //----------------------------------------------------------------------
        void mouseDown(const juce::MouseEvent& e) override
        {
            currentMousePos = e.position;
            lastMousePos = e.position;
            mouseDownFlag = true;
            mouseJustPressedFlag = true;

            if (world)
            {
                const auto ray = camera.screenToWorldRay(e.position);
                if (world->onMousePress(ray))
                    return;            // 命中旋钮 -> 不旋转相机
            }
            camDragActive = true;
            camDragStart = e.position;
            yawAtStart = camera.getYaw();
            pitchAtStart = camera.getPitch();

        }

        void mouseDrag(const juce::MouseEvent& e) override
        {
            const auto delta = e.position - currentMousePos;
            currentMousePos = e.position;

            if (world)
            {
                world->onMouseDragDelta(delta);
            }

            if (camDragActive)
            {
                const float dx = e.position.x - camDragStart.x;
                const float dy = e.position.y - camDragStart.y;
                camera.setYaw(yawAtStart + dx * 0.008f);
                camera.setPitch(pitchAtStart + dy * 0.008f);
            }
        }

        void mouseUp(const juce::MouseEvent&) override
        {
            mouseDownFlag = false;
            mouseJustReleasedFlag = true;
            camDragActive = false;
            if (world) world->onMouseRelease();
        }

        void mouseMove(const juce::MouseEvent& e) override
        {
            currentMousePos = e.position;
        }

        void mouseWheelMove(const juce::MouseEvent&,
            const juce::MouseWheelDetails& w) override
        {
            const float factor = std::pow(0.9f, w.deltaY * 4.0f);
            camera.setDistance(camera.getDistance() * factor);
        }

    private:
        //原子锁
        std::atomic<bool> viewportDirty{ false };
        int pendingVpW{ 0 }, pendingVpH{ 0 };

        void timerCallback() override { context.triggerRepaint(); }

        InputState pollInput() const noexcept
        {
            InputState s;
            s.keyUp = juce::KeyPress::isKeyCurrentlyDown(juce::KeyPress::upKey)
                || juce::KeyPress::isKeyCurrentlyDown('W')
                || juce::KeyPress::isKeyCurrentlyDown('w');
            s.keyDown = juce::KeyPress::isKeyCurrentlyDown(juce::KeyPress::downKey)
                || juce::KeyPress::isKeyCurrentlyDown('S')
                || juce::KeyPress::isKeyCurrentlyDown('s');
            s.keyLeft = juce::KeyPress::isKeyCurrentlyDown(juce::KeyPress::leftKey)
                || juce::KeyPress::isKeyCurrentlyDown('A')
                || juce::KeyPress::isKeyCurrentlyDown('a');
            s.keyRight = juce::KeyPress::isKeyCurrentlyDown(juce::KeyPress::rightKey)
                || juce::KeyPress::isKeyCurrentlyDown('D')
                || juce::KeyPress::isKeyCurrentlyDown('d');
            s.keyAttack = juce::KeyPress::isKeyCurrentlyDown(juce::KeyPress::spaceKey);

            s.mousePos = currentMousePos;
            s.mouseDown = mouseDownFlag;
            s.mouseJustPressed = mouseJustPressedFlag;
            s.mouseJustReleased = mouseJustReleasedFlag;
            s.mouseDelta = currentMousePos - lastMousePos;
            return s;
        }

        // GL
        juce::OpenGLContext context;
        Camera   camera;
        Lighting lighting;
        std::unique_ptr<Renderer> renderer;

        // 调试 mesh（无 World 时使用）
        std::unique_ptr<Mesh> debugGrid;
        std::unique_ptr<Mesh> debugCube;

        // 注入
        World* world{ nullptr };

        // 时序
        double lastRenderSec{ 0.0 };

        // 鼠标状态
        juce::Point<float> currentMousePos{ 0, 0 };
        juce::Point<float> lastMousePos{ 0, 0 };
        bool mouseDownFlag{ false };
        bool mouseJustPressedFlag{ false };
        bool mouseJustReleasedFlag{ false };

        // 相机轨道拖拽（鼠标没命中旋钮时）
        bool   camDragActive{ false };
        juce::Point<float> camDragStart;
        float  yawAtStart{ 0.0f };
        float  pitchAtStart{ 0.0f };

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SceneView)
    };
}
