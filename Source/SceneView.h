/*
  ==============================================================================
    SceneView.h
    Layer 2+3 接合版：World 注入后渲染/更新全交给 World；
    未注入时退化到调试场景（地面网格 + 静止立方体），确保随时可编译运行。
  ==============================================================================
*/
#pragma once

#include <JuceHeader.h>
#include "Camera.h"
#include "Renderer.h"
#include "Mesh.h"
#include "Lighting.h"
#include "InputState.h"
#include "World.h"

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
            context.setContinuousRepainting(false);
            context.attachTo(*this);

            startTimerHz(60);
        }

        ~SceneView() override
        {
            stopTimer();
            context.detach();
        }

        Camera&   getCamera()   noexcept { return camera; }
        Lighting& getLighting() noexcept { return lighting; }

        void setWorld(World* w) noexcept { world = w; }

        //======================================================================
        // juce::Component
        //======================================================================
        void paint(juce::Graphics&) override {}

        void resized() override
        {
            camera.setViewport(getWidth(), getHeight());
        }

        //======================================================================
        // juce::OpenGLRenderer（GL 线程）
        //======================================================================
        void newOpenGLContextCreated() override
        {
            renderer = std::make_unique<Renderer>(context);
            if (!renderer->initialise())
            {
                DBG("SceneView: Renderer init failed");
                renderer.reset();
                return;
            }

            // 调试 mesh（World 注入后不会用到）
            debugGrid = Mesh::createGrid(20.0f, 1.0f);
            debugGrid->uploadToGPU(context);
            debugCube = Mesh::createBox(2.0f, 2.0f, 2.0f);
            debugCube->uploadToGPU(context);

            if (world != nullptr)
                world->uploadMeshes(context);

            camera.setPivot({ 0.0f, 0.0f, 0.0f });
            camera.setOrbit(juce::degreesToRadians(35.0f),
                            juce::degreesToRadians(55.0f),
                            14.0f);
            camera.setPerspective(50.0f, 0.1f, 200.0f);

            // ★ 修复黑屏：GL context 创建时立即同步 viewport
            //   GL 线程此时拿到的 getWidth()/getHeight() 已是组件真实尺寸
            camera.setViewport(getWidth(), getHeight());
        }

        void renderOpenGL() override
        {
            if (renderer == nullptr) return;

            const double now = juce::Time::getMillisecondCounterHiRes() * 0.001;
            const float dt = (lastRenderSec > 0.0)
                               ? (float)juce::jlimit(0.001, 0.1, now - lastRenderSec)
                               : 1.0f / 60.0f;
            lastRenderSec = now;

            InputState in = pollInput();
            in.viewportW  = camera.getViewportWidth();
            in.viewportH  = camera.getViewportHeight();

            if (world != nullptr)
                world->update(dt, in, camera);

            renderer->beginFrame(camera, lighting);

            if (world != nullptr)
            {
                world->draw(*renderer, camera);
            }
            else if (debugGrid && debugCube)
            {
                renderer->drawLines(*debugGrid, identity(),
                                    { 0.25f, 0.30f, 0.35f });
                renderer->drawMesh (*debugCube,
                                    translation({ 0.0f, 0.0f, 1.0f }),
                                    { 0.55f, 0.50f, 0.45f });
            }

            renderer->endFrame();

            mouseJustPressedFlag  = false;
            mouseJustReleasedFlag = false;
            lastMousePos = currentMousePos;
        }

        void openGLContextClosing() override
        {
            if (world != nullptr)
                world->releaseMeshes(context);

            if (debugGrid) { debugGrid->releaseGPU(context); debugGrid.reset(); }
            if (debugCube) { debugCube->releaseGPU(context); debugCube.reset(); }

            if (renderer) renderer->shutdown();
            renderer.reset();
        }

        //======================================================================
        // 输入
        //======================================================================
        void mouseDown(const juce::MouseEvent& e) override
        {
            currentMousePos      = e.position;
            lastMousePos         = e.position;
            mouseDownFlag        = true;
            mouseJustPressedFlag = true;

            if (world != nullptr)
            {
                const auto ray = camera.screenToWorldRay(e.position);
                if (world->onMousePress(ray))
                    return;
            }

            camDragActive = true;
            camDragStart  = e.position;
            yawAtStart    = camera.getYaw();
            pitchAtStart  = camera.getPitch();
        }

        void mouseDrag(const juce::MouseEvent& e) override
        {
            const juce::Point<float> delta = e.position - currentMousePos;
            currentMousePos = e.position;

            if (world != nullptr)
                world->onMouseDragDelta(delta);

            if (camDragActive)
            {
                const float dx = e.position.x - camDragStart.x;
                const float dy = e.position.y - camDragStart.y;
                camera.setYaw  (yawAtStart   + dx * 0.008f);
                camera.setPitch(pitchAtStart + dy * 0.008f); // 向上拖 dy<0 → pitch 减小 → 抬头
            }
        }

        void mouseUp(const juce::MouseEvent&) override
        {
            mouseDownFlag         = false;
            mouseJustReleasedFlag = true;
            camDragActive         = false;
            if (world != nullptr)
                world->onMouseRelease();
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
        void timerCallback() override { context.triggerRepaint(); }

        InputState pollInput() const noexcept
        {
            InputState s;
            // W/上键 → 往屏幕上方（+Y），映射到 keyUp
            s.keyUp    = juce::KeyPress::isKeyCurrentlyDown(juce::KeyPress::upKey)
                      || juce::KeyPress::isKeyCurrentlyDown('W')
                      || juce::KeyPress::isKeyCurrentlyDown('w');
            s.keyDown  = juce::KeyPress::isKeyCurrentlyDown(juce::KeyPress::downKey)
                      || juce::KeyPress::isKeyCurrentlyDown('S')
                      || juce::KeyPress::isKeyCurrentlyDown('s');
            s.keyLeft  = juce::KeyPress::isKeyCurrentlyDown(juce::KeyPress::leftKey)
                      || juce::KeyPress::isKeyCurrentlyDown('A')
                      || juce::KeyPress::isKeyCurrentlyDown('a');
            s.keyRight = juce::KeyPress::isKeyCurrentlyDown(juce::KeyPress::rightKey)
                      || juce::KeyPress::isKeyCurrentlyDown('D')
                      || juce::KeyPress::isKeyCurrentlyDown('d');
            s.keyAttack = juce::KeyPress::isKeyCurrentlyDown(juce::KeyPress::spaceKey);

            s.mousePos          = currentMousePos;
            s.mouseDown         = mouseDownFlag;
            s.mouseJustPressed  = mouseJustPressedFlag;
            s.mouseJustReleased = mouseJustReleasedFlag;
            s.mouseDelta        = currentMousePos - lastMousePos;
            return s;
        }

        juce::OpenGLContext context;
        Camera   camera;
        Lighting lighting;
        std::unique_ptr<Renderer> renderer;
        std::unique_ptr<Mesh> debugGrid;
        std::unique_ptr<Mesh> debugCube;
        World* world { nullptr };

        double lastRenderSec { 0.0 };

        juce::Point<float> currentMousePos { 0.0f, 0.0f };
        juce::Point<float> lastMousePos    { 0.0f, 0.0f };
        bool mouseDownFlag         { false };
        bool mouseJustPressedFlag  { false };
        bool mouseJustReleasedFlag { false };

        bool camDragActive { false };
        juce::Point<float> camDragStart { 0.0f, 0.0f };
        float yawAtStart   { 0.0f };
        float pitchAtStart { 0.0f };

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SceneView)
    };
}
