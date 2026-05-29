/*
  ==============================================================================
    World.h
    Layer 3: Game / Interaction
    场景容器：拥有共享 mesh、Player、若干 KnobEntity。
    每帧：
      1. 把相机水平面 basis 注入 Player；
      2. update 所有 entity；
      3. 计算 Player 与各 Knob 的距离 -> Focused 状态；
      4. 把 pivot 软跟随到 Player；
      5. draw 所有 entity。
    鼠标交互：
      - mousePress(ray) -> 对 KnobEntity 做拾取，若命中且 Focused，开始拖拽
      - mouseDrag(deltaPx) -> 推 InertialValue
      - mouseRelease() -> 结束拖拽
  ==============================================================================*/
#pragma once

#include <JuceHeader.h>
#include <vector>
#include <memory>

#include "Entity.h"
#include "Player.h"
#include "KnobEntity.h"
#include "InputState.h"
#include "Easing.h"
#include "PluginProcessor.h"

namespace sc
{
    class World
    {
    public:
        explicit World(SineCloudAudioProcessor& p);

        // GL 资源生命周期（与 SceneView 的 newOpenGLContextCreated/Closing 对齐）
        void uploadMeshes(juce::OpenGLContext& ctx);
        void releaseMeshes(juce::OpenGLContext& ctx);

        // 主循环
        void update(float dt, const InputState& in, Camera& cam);
        void draw(Renderer& r, const Camera& cam);

        // 鼠标
        bool onMousePress(const Ray& worldRay);
        void onMouseDragDelta(juce::Point<float> deltaPx);
        void onMouseRelease();

        // 调试 / 信息
        Player& getPlayer() noexcept { return *player; }
        const std::vector<std::unique_ptr<KnobEntity>>& getKnobs() const noexcept { return knobs; }

    private:
        void buildKnobs();

        SineCloudAudioProcessor& processor;

        // 共享 mesh
        std::unique_ptr<Mesh> groundMesh;   // GL_LINES
        std::unique_ptr<Mesh> boxMesh;      // 1x1x1
        std::unique_ptr<Mesh> cylMesh;      // 半径 1, 高 1
        std::unique_ptr<Mesh> ptrMesh;      // 1x1x1（用作指针条，靠 scale 拉成长条）

        // 实体
        std::unique_ptr<Player> player;
        std::vector<std::unique_ptr<KnobEntity>> knobs;

        // 拖拽中的旋钮
        KnobEntity* draggingKnob{ nullptr };

        // 调参
        float interactReach{ 1.6f }; // 玩家到旋钮中心的"focus"距离
        float pivotFollowRate{ 0.92f };

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(World)
    };
}
