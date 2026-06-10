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
            context.setComponentPaintingEnabled(true);
            context.setContinuousRepainting(false);
            context.attachTo(*this);
            context.setSwapInterval(0);               // ★ 不等待 vsync，允许 120fps 自由跑

            debugOverlay.setCamera(&camera);
            startTimerHz(120);
            context.setContinuousRepainting(true);    // ★ 与定时器双驱动确保不掉帧


            // ---- Pause ----
            addAndMakeVisible(pauseOverlay);
            pauseOverlay.setVisible(false);
            pauseOverlay.onResume = [this]()
                {
                    setPaused(false);
                };

            // 启动时隐藏光标，等鼠标首次进入组件时居中锁定
            setMouseCursor(juce::MouseCursor::NoCursor);
        }

        ~SceneView() override
        {
            stopTimer();
            context.detach();
        }

        Camera& getCamera()   noexcept { return camera; }
        Lighting& getLighting() noexcept { return lighting; }

        void setWorld(World* w) noexcept { world = w; }

        //======================================================================
        // juce::Component
        //======================================================================
        void paint(juce::Graphics& g) override
        {
            debugOverlay.drawDebugInfo(g, getWidth(), getHeight());
        }

        void resized() override
        {
            camera.setViewport(getWidth(), getHeight());
            pauseOverlay.setBounds(getLocalBounds());
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

           /* debugGrid = Mesh::createGrid(20.0f, 1.0f);
            debugGrid->uploadToGPU(context);*/
            debugCube = Mesh::createBox(2.0f, 2.0f, 2.0f);
            debugCube->uploadToGPU(context);

            if (world != nullptr)
                world->uploadMeshes(context);
            //----
            camera.setPivot({ 0.0f, 0.0f, 0.0f });
            camera.setOrbit(juce::degreesToRadians(35.0f),
                juce::degreesToRadians(55.0f),
                14.0f);
            camera.setPerspective(20.0f, 0.1f, 200.0f);
            camera.setViewport(getWidth(), getHeight());
            //----
            debugOverlay.setVisible(false);
            grabKeyboardFocus();

            //lighting.fogDensity = 0.125f;
            //lighting.fogHeightFalloff = 0.10f;
            //lighting.fogStart = 5.0f;
            //lighting.fogColorSRGB = { 0.1f, 0.1f, 0.1f };  // 与 clearColor 同色

        }

        void renderOpenGL() override
        {
            if (renderer == nullptr) return;

            const double now = juce::Time::getMillisecondCounterHiRes() * 0.001;
            const float dt = (lastRenderSec > 0.0)
                ? (float)juce::jlimit(0.001, 0.1, now - lastRenderSec)
                : 1.0f / 60.0f;
            lastDt = dt;
            lastRenderSec = now;

            InputState in = pollInput();
            in.viewportW = camera.getViewportWidth();
            in.viewportH = camera.getViewportHeight();

            if (world != nullptr && !paused)
                world->update(dt, in, camera);

            camera.tick(dt);

            // ====== Shadow Passes ======
            if (world != nullptr)
            {
                auto& sm = renderer->getShadowMap();
                const Vec3 playerPos = world->getPlayer().worldPos;

                // 方向光
                sm.beginDirectionalPass(lighting.direction, playerPos, lighting.sceneRadius);
                world->drawShadowDepth(sm, context);
                sm.endDirectionalPass();

                // 点光源 Cube (Player Light)
                if (!lighting.pointLights.empty())
                {
                    const auto& pl = lighting.pointLights[0];
                    for (int face = 0; face < 6; ++face)
                    {
                        sm.beginCubeFace(face, pl.position, pl.range);
                        world->drawShadowDepth(sm, context);
                        sm.endCubeFace();
                    }
                }
            }

            // ====== G-Buffer 几何 + 延迟光照 ======
            if (world != nullptr)
            {
                renderer->beginFrame(camera, lighting, world->getPlayer().worldPos);
                world->draw(*renderer, camera);
                renderer->endFrame(camera, lighting, world->getPlayer().worldPos);
            }
        }


        void openGLContextClosing() override
        {
            if (world != nullptr)
                world->releaseMeshes(context);

            /*if (debugGrid) { debugGrid->releaseGPU(context); debugGrid.reset(); }*/
            if (debugCube) { debugCube->releaseGPU(context); debugCube.reset(); }

            if (renderer) renderer->shutdown();
            renderer.reset();
        }

        //======================================================================
        // 输入
        //======================================================================
        void mouseEnter(const juce::MouseEvent&) override
        {
            grabKeyboardFocus();

            // ★ 启动时把鼠标移到组件正中央并锁定
            if (!mouseLookInit && !paused)
            {
                const auto sb = getScreenBounds();
                warpTarget = juce::Point<float>(
                    (float)sb.getCentreX(), (float)sb.getCentreY());

                juce::Desktop::getInstance().getMainMouseSource()
                    .setScreenPosition(warpTarget);

                lastMouseLookScreenPos = warpTarget;
                mouseLookInit = true;
            }
        }

        void mouseMove(const juce::MouseEvent& e) override
        {
            if (paused) return;

            if (ignoreNextMouseMove)
            {
                ignoreNextMouseMove = false;
                return;
            }

            const auto sp = e.getScreenPosition();
            const juce::Point<float> screenPos((float)sp.x, (float)sp.y);

            if (!mouseLookInit)
            {
                lastMouseLookScreenPos = screenPos;
                warpTarget = screenPos;
                mouseLookInit = true;
                return;
            }

            const float dx = screenPos.x - lastMouseLookScreenPos.x;
            const float dy = screenPos.y - lastMouseLookScreenPos.y;

            camera.setYaw(camera.getYaw() + dx * 0.005f, lastDt);
            camera.setPitch(camera.getPitch() + dy * 0.005f, lastDt);

            ignoreNextMouseMove = true;
            juce::Desktop::getInstance().getMainMouseSource()
                .setScreenPosition(warpTarget);
            lastMouseLookScreenPos = warpTarget;   // ★ 不管 OS 有没有弹回去，都当弹回去了
        }

        void mouseDown(const juce::MouseEvent&) override
        {
            grabKeyboardFocus();
        }

        void mouseUp(const juce::MouseEvent&) override
        {
            // 松手时重置 mouseLookInit，防止 setScreenPosition 被 OS 拦截期间积累的偏移在下一帧跳变
            mouseLookInit = false;
        }

        void mouseDrag(const juce::MouseEvent& e) override
        {
            // 和 mouseMove 完全一样的逻辑，因为 JUCE 按住按键时走 mouseDrag 而非 mouseMove
            if (paused) return;

            if (ignoreNextMouseMove)
            {
                ignoreNextMouseMove = false;
                return;
            }

            const auto sp = e.getScreenPosition();
            const juce::Point<float> screenPos((float)sp.x, (float)sp.y);

            if (!mouseLookInit)
            {
                lastMouseLookScreenPos = screenPos;
                warpTarget = screenPos;
                mouseLookInit = true;
                return;
            }

            const float dx = screenPos.x - lastMouseLookScreenPos.x;
            const float dy = screenPos.y - lastMouseLookScreenPos.y;

            camera.setYaw(camera.getYaw() + dx * 0.005f, lastDt);
            camera.setPitch(camera.getPitch() + dy * 0.005f, lastDt);

            ignoreNextMouseMove = true;
            juce::Desktop::getInstance().getMainMouseSource()
                .setScreenPosition(warpTarget);
            lastMouseLookScreenPos = warpTarget;
        }



        void mouseWheelMove(const juce::MouseEvent&, const juce::MouseWheelDetails& w) override {
            if (paused) return;
            if (world != nullptr && world->getFocusedKnob() != nullptr) {
                world->onMouseWheel(w.deltaY);
                return;
            }
            const float factor = std::pow(0.9f, w.deltaY * 4.0f);
            camera.setDistance(camera.getDistance() * factor);   // ★ 加 lastDt
        }



        bool keyPressed(const juce::KeyPress& key) override
        {
            if (key.getKeyCode() == juce::KeyPress::escapeKey)
            {
                setPaused(!paused);
                return true;
            }

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
                repaint();
        }

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

            // 鼠标字段暂不维护（鼠标被锁定做视角控制），填默认值
            s.mousePos = { 0.0f, 0.0f };
            s.mouseDown = false;
            s.mouseJustPressed = false;
            s.mouseJustReleased = false;
            s.mouseDelta = { 0.0f, 0.0f };

            return s;
        }

        // ---- 渲染 ----
        juce::OpenGLContext context;
        Camera camera;
        Lighting lighting;
        std::unique_ptr<Renderer> renderer;
        /*std::unique_ptr<Mesh> debugGrid;*/
        std::unique_ptr<Mesh> debugCube;
        World* world{ nullptr };
        double lastRenderSec{ 0.0 };
        float lastDt{ 1.0f / 60.0f };

        // ---- 调试 ----
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
                mouseLookInit = false;
            
            }
            else
            {
                pauseOverlay.setVisible(false);
                setMouseCursor(juce::MouseCursor::NoCursor);
                mouseLookInit = false;
            
                grabKeyboardFocus();
            }
        }


        bool paused = false;
        PauseOverlay pauseOverlay;

        // ---- 鼠标跟随 ----
        juce::Point<float> lastMouseLookScreenPos;
        juce::Point<float> warpTarget;
        bool mouseLookInit = false;
        bool ignoreNextMouseMove = false;
        

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SceneView)
    };
}
