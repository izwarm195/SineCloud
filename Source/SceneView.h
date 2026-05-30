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
#include "CameraDebug.h"
#include "PauseOverlay.h"

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
            context.setComponentPaintingEnabled(true);   // ★ 关键：true，允许 GL 之后再叠 2D
            context.setContinuousRepainting(false);
            context.attachTo(*this);

            debugOverlay.setCamera(&camera);
            startTimerHz(60);

            // ---- Pause ----
            addAndMakeVisible(pauseOverlay);
            pauseOverlay.setVisible(false);
            pauseOverlay.onResume = [this]()
                {
                    setPaused(false);
                };

            // 启动时进入鼠标跟随模式（隐藏光标）
            setMouseCursor(juce::MouseCursor::NoCursor);
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
        void paint(juce::Graphics& g) override
        {
            // GL 已经画完场景，这里只在最上层叠加 debug 面板
            debugOverlay.drawDebugInfo(g, getWidth(), getHeight());
        }


        void resized() override
        {
            camera.setViewport(getWidth(), getHeight());
            pauseOverlay.setBounds(getLocalBounds());   // ← 新增
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


            //debug
            debugOverlay.setVisible(false);
        }

        void renderOpenGL() override
        {
            if (renderer == nullptr) return;

            // ---- 帧时间 ----
            const double now = juce::Time::getMillisecondCounterHiRes() * 0.001;
            const float dt = (lastRenderSec > 0.0)
                ? (float)juce::jlimit(0.001, 0.1, now - lastRenderSec)
                : 1.0f / 60.0f;
            lastRenderSec = now;

            InputState in = pollInput();
            in.viewportW = camera.getViewportWidth();
            in.viewportH = camera.getViewportHeight();

            // ★ 暂停时不更新世界
            if (world != nullptr && !paused)
            {
                world->update(dt, in, camera);
            }

            // ---- 渲染（暂停时场景仍会绘制，只是冻结不动）----
            renderer->beginFrame(camera, lighting);

            if (world != nullptr)
                world->draw(*renderer, camera);

            renderer->endFrame();

            mouseJustPressedFlag = false;
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
        void mouseMove(const juce::MouseEvent& e) override
        {
            if (paused) return;

            if (!mouseLookInit)
            {
                // 第一帧不旋转，只记录位置，防止跳变
                lastMouseLookPos = e.position;
                mouseLookInit = true;
                return;
            }

            const float dx = e.position.x - lastMouseLookPos.x;
            const float dy = e.position.y - lastMouseLookPos.y;

            // 灵敏度略低于原来的 0.008，因为每帧都触发，不需要太高
            camera.setYaw(camera.getYaw() + dx * 0.005f);
            camera.setPitch(camera.getPitch() - dy * 0.005f);

            lastMouseLookPos = e.position;
        }

        void mouseDown(const juce::MouseEvent& e) override
        {
            // 有 World 时转发点击做拾取，否则忽略
            if (world != nullptr)
            {
                const auto ray = camera.screenToWorldRay(e.position);
                if (world->onMousePress(ray))
                    return;
            }
        }

        void mouseDrag(const juce::MouseEvent& e) override
        {
            // 只转发给 World 做旋钮拖拽，不再旋转视角
            if (world != nullptr)
                world->onMouseDragDelta(e.position - lastMouseLookPos);
        }

        void mouseUp(const juce::MouseEvent&) override
        {
            if (world != nullptr)
                world->onMouseRelease();
        }


        
        void mouseWheelMove(const juce::MouseEvent&,const juce::MouseWheelDetails& w) override
        {
            if (paused) return;                        // ← 新增
            const float factor = std::pow(0.9f, w.deltaY * 4.0f);
            camera.setDistance(camera.getDistance() * factor);
        }


        bool keyPressed(const juce::KeyPress& key) override
        {
            // ESC：切换暂停
            if (key.getKeyCode() == juce::KeyPress::escapeKey)
            {
                setPaused(!paused);
                return true;
            }

            // F3（保留你现有的逻辑）
            if (key.getKeyCode() == juce::KeyPress::F3Key)
            {
                debugOverlay.toggleVisible();
                repaint();
                return true;
            }

            return false;
        }


    private:
        void timerCallback() override
        {
            context.triggerRepaint();
            if (debugOverlay.isVisible())
                repaint();           // ★ 面板可见时同时刷 2D 层
        }


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

            //s.keyF3 = juce::KeyPress::isKeyCurrentlyDown(juce::KeyPress::F3Key);
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

        CameraDebugOverlay debugOverlay;


        // ---- 暂停 ----
        void setPaused(bool p)
        {
            if (paused == p) return;
            paused = p;

            if (paused)
            {
                pauseOverlay.setVisible(true);
                setMouseCursor(juce::MouseCursor::NormalCursor);
                mouseLookInit = false;      // 恢复时重置，防止跳变
            }
            else
            {
                pauseOverlay.setVisible(false);
                setMouseCursor(juce::MouseCursor::NoCursor);
                mouseLookInit = false;      // 下次 mouseMove 第一帧只记录位置
                grabKeyboardFocus();        // 确保 WASD 继续工作
            }
        }

        // ---- 暂停状态 ----
        bool paused = false;
        PauseOverlay pauseOverlay;

        // ---- 鼠标跟随 ----
        juce::Point<float> lastMouseLookPos;
        bool mouseLookInit = false;         // 第一帧标记，防止跳视角


        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SceneView)
    };
}
