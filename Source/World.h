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
#include "Lighting.h"
#include "GrassComponent.h"


namespace sc
{

    //==========================================================================
    // 地面高度采样表（从 ground_collision.obj 顶点构建）
    //==========================================================================
    struct HeightField
    {
        float sampleHeight(float x, float y) const;
        Vec3  sampleNormal(float x, float y) const;
        void  buildFromMesh(const class Mesh& mesh);
        bool  isEmpty() const noexcept { return tris.empty(); }

    private:
        struct Tri { Vec3 a, b, c; Vec3 normal; }; // 预存几何法线

        // 在三角形 t 的 XY 投影内做重心插值；命中则写 outZ/outN 返回 true
        bool sampleTri(const Tri& t, float x, float y,
            float& outZ, Vec3& outN) const noexcept;

        std::vector<Tri> tris;
        std::unordered_map<int64_t, std::vector<int>> spatialMap; // cell -> 三角形索引
        float cellSize{ 1.0f };
        float fallbackZ{ 0.0f }; // 没命中任何三角形时的兜底高度

        int64_t cellKey(int cx, int cy) const noexcept;
    };


    //==========================================================================
    // World
    //==========================================================================
    class World
    {
    public:
        explicit World(SineCloudAudioProcessor& p, Lighting& l);


        // GL 资源生命周期
        void uploadMeshes(juce::OpenGLContext& ctx);
        void releaseMeshes(juce::OpenGLContext& ctx);

        // 主循环
        void update(float dt, const InputState& in, Camera& cam);
        void draw(Renderer& r, const Camera& cam);
        void drawShadowDepth(class ShadowMap& sm, juce::OpenGLContext& ctx);

        // 交互
        void onMouseWheel(float deltaY);

        KnobEntity* getFocusedKnob() const noexcept { return focusedKnob; }

        // 调试 / 信息
        Player& getPlayer() noexcept { return *player; }
        const std::vector<std::unique_ptr<KnobEntity>>& getKnobs() const noexcept { return knobs; }

        
    private:
        void buildKnobs();
        void loadCollidersFromJSON(const juce::File& jsonFile);
        void buildCollidersFromObjFile(const juce::File& objFile);
        void resolvePlayerCollisions();

        juce::OpenGLContext* glCtx = nullptr;

        SineCloudAudioProcessor& processor;

        Lighting& lighting;

        // 共享 mesh
        std::unique_ptr<Mesh> boxMesh;
        std::unique_ptr<Mesh> cylMesh;
        std::unique_ptr<Mesh> ptrMesh;
        std::unique_ptr<Mesh> propsMesh;

        // 场景 OBJ mesh
        std::unique_ptr<Mesh> groundVisMesh;
        std::unique_ptr<Mesh> propRockMesh;
        std::unique_ptr<Mesh> propBouquetMesh;
        std::unique_ptr<Mesh> propBouquetPillarMesh;
        std::unique_ptr<Mesh> propKnobPillarMesh;
        std::unique_ptr<Mesh> propMainPillarMesh;
        std::unique_ptr<Mesh> propSurroundPillarMesh;
        std::unique_ptr<Mesh> groundColMesh;

        // 实体
        std::unique_ptr<Player>                  player;
        std::vector<std::unique_ptr<KnobEntity>> knobs;
        std::vector<CollisionShape> propColliders;
        std::vector<CollisionTri> collisionTris;

        // 物理
        HeightField                 heightField;
        std::vector<CollisionShape> colliders;

        //草
        std::unique_ptr<GrassComponent> grass;

        // 调参
        float interactReach{ 1.6f };
        float playerRadius{ 0.35f };
        float maxSlopeCos{ 0.5f };

        // 交互状态
        KnobEntity* focusedKnob{ nullptr };

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(World)
    };

} // namespace sc
