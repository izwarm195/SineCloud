/*
  ==============================================================================
    SceneView.h
    Layer 2: Scene & Renderer
    GL 视口组件：拥有 OpenGLContext / Camera / Renderer，60Hz 主循环。
    World（Layer 3）尚未引入时，自带一个调试场景：地面网格 + 原点立方体 +
    一个旋转的小盒子，鼠标拖拽旋转视角，滚轮缩放。
  ==============================================================================
*/
#pragma once

#include <JuceHeader.h>
#include "Camera.h"
#include "Renderer.h"
#include "Mesh.h"
#include "Lighting.h"

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
            context.setComponentPaintingEnabled(false); // 纯 GL，不让 JUCE 在上面再画一层
            context.setContinuousRepainting(false);     // 我们用 Timer 驱动重绘
            context.attachTo(*this);

            startTimerHz(60);
        }

        ~SceneView() override
        {
            stopTimer();
            context.detach();
        }

        //----------------------------------------------------------------------
        // 给 Layer 3 用：World 接进来后会拿 camera 跟随玩家
        //----------------------------------------------------------------------
        Camera& getCamera()   noexcept { return camera; }
        Lighting& getLighting() noexcept { return lighting; }

        //======================================================================
        // juce::Component
        //======================================================================
        void paint(juce::Graphics&) override {}     // 全部交给 GL

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

            // ---- 调试场景资源（World 接入后会被替换） ----
            gridMesh = Mesh::createGrid(20.0f, 1.0f);
            gridMesh->uploadToGPU(context);

            cubeMesh = Mesh::createBox(2.0f, 2.0f, 2.0f);
            cubeMesh->uploadToGPU(context);

            spinnerMesh = Mesh::createBox(0.6f, 0.6f, 0.6f);
            spinnerMesh->uploadToGPU(context);

            cylinderMesh = Mesh::createCylinder(0.6f, 0.4f, 24);
            cylinderMesh->uploadToGPU(context);

            // ---- 默认相机 ----
            camera.setPivot({ 0, 0, 0 });
            camera.setOrbit(juce::degreesToRadians(35.0f),
                juce::degreesToRadians(55.0f),
                14.0f);
            camera.setPerspective(50.0f, 0.1f, 200.0f);
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

            // ---- 调试动画 ----
            spinnerYaw += dt * juce::degreesToRadians(45.0f);

            // ---- 渲染 ----
            renderer->beginFrame(camera, lighting);

            // 地面网格（GL_LINES，不参与光照）
            if (gridMesh)
                renderer->drawLines(*gridMesh, identity(),
                    { 0.25f, 0.30f, 0.35f });

            // 中心立方体：抬高半个身位，以便在网格上方
            if (cubeMesh)
                renderer->drawMesh(*cubeMesh,
                    translation({ 0.0f, 0.0f, 1.0f }),
                    { 0.55f, 0.50f, 0.45f });

            // 旋钮形圆柱（占位，演示 cylinder factory）
            if (cylinderMesh)
                renderer->drawMesh(*cylinderMesh,
                    translation({ 4.0f, 0.0f, 0.0f }),
                    { 0.30f, 0.55f, 0.70f });

            // 自旋小盒子，演示 model 矩阵动画
            if (spinnerMesh)
            {
                const Mat4 model = translation({ -4.0f, 0.0f, 0.6f })
                    * rotationZ(spinnerYaw);
                renderer->drawMesh(*spinnerMesh, model,
                    { 0.85f, 0.30f, 0.30f });
            }

            renderer->endFrame();
        }

        void openGLContextClosing() override
        {
            if (gridMesh)     gridMesh->releaseGPU(context);
            if (cubeMesh)     cubeMesh->releaseGPU(context);
            if (spinnerMesh)  spinnerMesh->releaseGPU(context);
            if (cylinderMesh) cylinderMesh->releaseGPU(context);
            gridMesh.reset();
            cubeMesh.reset();
            spinnerMesh.reset();
            cylinderMesh.reset();

            if (renderer) renderer->shutdown();
            renderer.reset();
        }

        //======================================================================
        // 输入：调试期直接驱动相机；World 接入后改为转发 InputState
        //======================================================================
        void mouseDown(const juce::MouseEvent& e) override
        {
            dragging = true;
            dragStart = e.position;
            yawAtStart = camera.getYaw();
            pitchAtStart = camera.getPitch();
        }

        void mouseDrag(const juce::MouseEvent& e) override
        {
            if (!dragging) return;
            const float dx = e.position.x - dragStart.x;
            const float dy = e.position.y - dragStart.y;
            // 横向拖 → yaw；纵向拖 → pitch（向下拖抬头）
            camera.setYaw(yawAtStart + dx * 0.008f);
            camera.setPitch(pitchAtStart - dy * 0.008f);
        }

        void mouseUp(const juce::MouseEvent&) override { dragging = false; }

        void mouseWheelMove(const juce::MouseEvent&,
            const juce::MouseWheelDetails& w) override
        {
            // 滚轮缩放：deltaY > 0 拉近，< 0 推远
            const float factor = std::pow(0.9f, w.deltaY * 4.0f);
            camera.setDistance(camera.getDistance() * factor);
        }

    private:
        //======================================================================
        // Timer：60Hz 触发 GL 重绘
        //======================================================================
        void timerCallback() override
        {
            context.triggerRepaint();
        }

        //======================================================================
        // 数据成员
        //======================================================================
        juce::OpenGLContext context;

        Camera   camera;
        Lighting lighting;

        std::unique_ptr<Renderer> renderer;

        // 调试 mesh（World 接入后会从 SceneView 移走）
        std::unique_ptr<Mesh> gridMesh;
        std::unique_ptr<Mesh> cubeMesh;
        std::unique_ptr<Mesh> spinnerMesh;
        std::unique_ptr<Mesh> cylinderMesh;

        // 调试动画
        double lastRenderSec{ 0.0 };
        float  spinnerYaw{ 0.0f };

        // 调试输入
        bool   dragging{ false };
        juce::Point<float> dragStart;
        float  yawAtStart{ 0.0f };
        float  pitchAtStart{ 0.0f };

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SceneView)
    };
}
