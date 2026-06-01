/*
==============================================================================
 World.h
 Layer 3: Game / Interaction

 场景容器：拥有共享 mesh、Player、若干 KnobEntity。
==============================================================================
*/
#pragma once

#include <JuceHeader.h>
#include <vector>
#include <memory>
#include <unordered_map>
#include "Entity.h"
#include "Player.h"
#include "KnobEntity.h"
#include "InputState.h"
#include "Easing.h"
#include "PluginProcessor.h"
#include "CollisionTypes.h"


namespace sc
{

    //==========================================================================
    // 地面高度采样表（从 ground_collision.obj 顶点构建）
    //==========================================================================
    struct HeightField
    {
        std::vector<Vec3> vertices;

        float sampleHeight(float x, float y) const;
        Vec3  sampleNormal(float x, float y) const;
        void  buildFromMesh(const class Mesh& mesh);

    private:
        float cellSize{ 1.0f };
        std::unordered_map<int64_t, std::vector<int>> spatialMap;
        int64_t cellKey(int cx, int cy) const noexcept;
    };

    //==========================================================================
    // World
    //==========================================================================
    class World
    {
    public:
        explicit World(SineCloudAudioProcessor& p);

        // GL 资源生命周期
        void uploadMeshes(juce::OpenGLContext& ctx);
        void releaseMeshes(juce::OpenGLContext& ctx);

        // 主循环
        void update(float dt, const InputState& in, Camera& cam);
        void draw(Renderer& r, const Camera& cam);

        // 交互
        void onMouseWheel(float deltaY);

        KnobEntity* getFocusedKnob() const noexcept { return focusedKnob; }

        // 调试 / 信息
        Player& getPlayer() noexcept { return *player; }
        const std::vector<std::unique_ptr<KnobEntity>>& getKnobs() const noexcept { return knobs; }

        
    private:
        void buildKnobs();
        void loadCollidersFromJSON(const juce::File& jsonFile);
        void resolvePlayerCollisions();

        SineCloudAudioProcessor& processor;

        // 共享 mesh
        std::unique_ptr<Mesh> boxMesh;
        std::unique_ptr<Mesh> cylMesh;
        std::unique_ptr<Mesh> ptrMesh;

        // 场景 OBJ mesh
        std::unique_ptr<Mesh> groundVisMesh;
        std::unique_ptr<Mesh> propsVisMesh;
        std::unique_ptr<Mesh> groundColMesh;

        // 实体
        std::unique_ptr<Player>                  player;
        std::vector<std::unique_ptr<KnobEntity>> knobs;

        // 物理
        HeightField                 heightField;
        std::vector<CollisionShape> colliders;

        // 调参
        float interactReach{ 1.6f };
        float pivotFollowRate{ 0.9999f };
        float playerRadius{ 0.4f };
        float maxSlopeCos{ 0.5f };
        float groundFollowRate{ 0.85f }; // player.z 贴地形的平滑系数

        // 交互状态
        KnobEntity* focusedKnob{ nullptr };

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(World)
    };

} // namespace sc
