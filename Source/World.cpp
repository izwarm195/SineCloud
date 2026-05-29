/*
  ==============================================================================
    World.cpp
  ==============================================================================*/
#include "World.h"

namespace sc
{
    World::World(SineCloudAudioProcessor& p)
        : processor(p)
    {
        player = std::make_unique<Player>();
        buildKnobs();
    }

    //==========================================================================
    // 旋钮布局：沿用旧 IsoSceneDemo 的 6 个核心参数，按"圆形分布"重排
    //==========================================================================
    void World::buildKnobs()
    {
        struct Init {
            const char* paramId;
            const char* label;
            Vec3 worldPos;
        };
        const Init inits[] = {
            { SineCloudAudioProcessor::PARAM_PITCH,   "Pitch",   {  0.0f,  3.5f, 0.0f } },
            { SineCloudAudioProcessor::PARAM_DENSITY, "Density", {  3.0f,  1.5f, 0.0f } },
            { SineCloudAudioProcessor::PARAM_DREAM,   "Dream",   { -3.0f,  1.5f, 0.0f } },
            { SineCloudAudioProcessor::PARAM_FLOAT,   "Float",   {  3.0f, -1.5f, 0.0f } },
            { SineCloudAudioProcessor::PARAM_SHIMMER, "Shimmer", { -3.0f, -1.5f, 0.0f } },
            { SineCloudAudioProcessor::PARAM_GAIN,    "Gain",    {  0.0f, -3.5f, 0.0f } }
        };

        for (const auto& k : inits)
        {
            auto knob = std::make_unique<KnobEntity>(processor.apvts, k.paramId, k.label);
            knob->worldPos = k.worldPos;
            knob->setRadius(0.55f);
            knob->setHeight(0.35f);
            knobs.push_back(std::move(knob));
        }
    }

    //==========================================================================
    // GL 资源
    //==========================================================================
    void World::uploadMeshes(juce::OpenGLContext& ctx)
    {
        groundMesh = Mesh::createGrid(20.0f, 1.0f);
        boxMesh = Mesh::createBox(1.0f, 1.0f, 1.0f);
        cylMesh = Mesh::createCylinder(1.0f, 1.0f, 32);
        ptrMesh = Mesh::createBox(1.0f, 1.0f, 1.0f);

        groundMesh->uploadToGPU(ctx);
        boxMesh->uploadToGPU(ctx);
        cylMesh->uploadToGPU(ctx);
        ptrMesh->uploadToGPU(ctx);

        // 把共享 mesh 注入 entity
        player->setMesh(boxMesh.get());
        for (auto& k : knobs)
            k->setMeshes(cylMesh.get(), ptrMesh.get());
    }

    void World::releaseMeshes(juce::OpenGLContext& ctx)
    {
        if (groundMesh) groundMesh->releaseGPU(ctx);
        if (boxMesh)    boxMesh->releaseGPU(ctx);
        if (cylMesh)    cylMesh->releaseGPU(ctx);
        if (ptrMesh)    ptrMesh->releaseGPU(ctx);
        groundMesh.reset();
        boxMesh.reset();
        cylMesh.reset();
        ptrMesh.reset();
    }

    //==========================================================================
    // 主循环
    //==========================================================================
    void World::update(float dt, const InputState& in, Camera& cam)
    {
        // 1. 注入相机基向量
        player->setBasis(cam.getForwardOnGround(), cam.getRightOnGround());

        // 2. update 所有 entity
        player->update(dt, in);
        for (auto& k : knobs)
            k->update(dt, in);

        // 3. 计算 Focused：玩家是否在某个旋钮的 reach 之内
        //    选最近且在 reach 内的那个，其它全部 false。
        KnobEntity* nearest = nullptr;
        float bestDist2 = interactReach * interactReach;
        for (auto& k : knobs)
        {
            const float dx = player->worldPos.x - k->worldPos.x;
            const float dy = player->worldPos.y - k->worldPos.y;
            const float d2 = dx * dx + dy * dy;
            if (d2 <= bestDist2)
            {
                bestDist2 = d2;
                nearest = k.get();
            }
        }
        for (auto& k : knobs)
            k->setFocused(k.get() == nearest);

        // 4. 相机 pivot 软跟随玩家（XY 平面，不动 Z）
        Vec3 pivot = cam.getPivot();
        pivot.x = easing::damp(pivot.x, player->worldPos.x, pivotFollowRate, dt);
        pivot.y = easing::damp(pivot.y, player->worldPos.y, pivotFollowRate, dt);
        cam.setPivot(pivot);
    }

    void World::draw(Renderer& r, const Camera& cam)
    {
        // 地面网格
        if (groundMesh)
            r.drawLines(*groundMesh, identity(), { 0.22f, 0.27f, 0.32f });

        // 实体
        for (auto& k : knobs) k->draw(r, cam);
        player->draw(r, cam);
    }

    //==========================================================================
    // 鼠标
    //==========================================================================

    bool World::onMousePress(const Ray& worldRay)
    {
        KnobEntity* hit = nullptr;
        float bestT = std::numeric_limits<float>::max();
        for (auto& k : knobs)
        {
            if (!k->intersectsRay(worldRay)) continue;
            const Vec3 d = k->worldPos - worldRay.origin;
            const float dist2 = d.x * d.x + d.y * d.y + d.z * d.z;
            if (dist2 < bestT) { bestT = dist2; hit = k.get(); }
        }
        if (hit == nullptr) return false;
        draggingKnob = hit;
        draggingKnob->beginMouseDrag();
        return true;
    }


    void World::onMouseDragDelta(juce::Point<float> deltaPx)
    {
        if (draggingKnob != nullptr)
            draggingKnob->onMouseDragDelta(deltaPx);
    }

    void World::onMouseRelease()
    {
        if (draggingKnob != nullptr)
        {
            draggingKnob->endMouseDrag();
            draggingKnob = nullptr;
        }
    }
}
