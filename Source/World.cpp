/*
==============================================================================
 World.cpp
==============================================================================
*/
#include "World.h"
#include "Renderer.h"
#include "CollisionTypes.h"
#include <cmath>
#include <limits>

namespace sc
{

    //==========================================================================
    // HeightField
    //==========================================================================

    int64_t HeightField::cellKey(int cx, int cy) const noexcept
    {
        return (static_cast<int64_t>(cx) << 32) | static_cast<int64_t>(cy & 0xFFFFFFFF);
    }

    void HeightField::buildFromMesh(const Mesh& mesh)
    {
        vertices.clear();
        spatialMap.clear();

        for (const auto& v : mesh.vertices)
        {
            vertices.push_back({ v.px, v.py, v.pz });

            const int cx = static_cast<int>(std::floor(v.px / cellSize));
            const int cy = static_cast<int>(std::floor(v.py / cellSize));
            spatialMap[cellKey(cx, cy)].push_back(static_cast<int>(vertices.size()) - 1);
        }
    }

    float HeightField::sampleHeight(float x, float y) const
    {
        if (vertices.empty()) return 0.0f;

        const int cx = static_cast<int>(std::floor(x / cellSize));
        const int cy = static_cast<int>(std::floor(y / cellSize));

        float bestDist2 = std::numeric_limits<float>::max();
        float bestZ = 0.0f;

        for (int dx = -1; dx <= 1; ++dx)
        {
            for (int dy = -1; dy <= 1; ++dy)
            {
                const auto it = spatialMap.find(cellKey(cx + dx, cy + dy));
                if (it == spatialMap.end()) continue;

                for (int idx : it->second)
                {
                    const auto& v = vertices[static_cast<size_t>(idx)];
                    const float d2 = (v.x - x) * (v.x - x) + (v.y - y) * (v.y - y);
                    if (d2 < bestDist2)
                    {
                        bestDist2 = d2;
                        bestZ = v.z;
                    }
                }
            }
        }

        return bestZ;
    }

    Vec3 HeightField::sampleNormal(float x, float y) const
    {
        const float eps = 0.3f;
        const float zC = sampleHeight(x, y);
        const float zR = sampleHeight(x + eps, y);
        const float zF = sampleHeight(x, y + eps);

        const Vec3 dzdx = { eps, 0.0f, zR - zC };
        const Vec3 dzdy = { 0.0f, eps, zF - zC };

        Vec3 n = cross(dzdy, dzdx);
        const float len2 = n.x * n.x + n.y * n.y + n.z * n.z;
        if (len2 < 1.0e-8f) return { 0.0f, 0.0f, 1.0f };
        return n * (1.0f / std::sqrt(len2));
    }

    //==========================================================================
    // CollisionShape
    //==========================================================================

    bool CollisionShape::collidePlayerCylinder(
        const Vec3& playerPos, float playerRadius,
        const CollisionShape& obs, Vec3& pushOut)
    {
        const float dx = playerPos.x - obs.center.x;
        const float dy = playerPos.y - obs.center.y;
        const float dist2 = dx * dx + dy * dy;
        const float minDist = playerRadius + obs.radius;

        if (dist2 >= minDist * minDist) return false;

        float dist = std::sqrt(dist2);
        if (dist < 0.001f) dist = 0.001f;

        const float overlap = minDist - dist;
        pushOut.x = (dx / dist) * overlap;
        pushOut.y = (dy / dist) * overlap;
        pushOut.z = 0.0f;
        return true;
    }

    bool CollisionShape::collidePlayerBox(
        const Vec3& playerPos, float playerRadius,
        const CollisionShape& obs, Vec3& pushOut)
    {
        const float ex = obs.radius + playerRadius;
        const float ey = obs.radius + playerRadius;

        const float cx = juce::jlimit(obs.center.x - ex, obs.center.x + ex, playerPos.x);
        const float cy = juce::jlimit(obs.center.y - ey, obs.center.y + ey, playerPos.y);

        const float dx = playerPos.x - cx;
        const float dy = playerPos.y - cy;
        const float dist2 = dx * dx + dy * dy;

        if (dist2 >= playerRadius * playerRadius) return false;

        float dist = std::sqrt(dist2);
        if (dist < 0.001f)
        {
            pushOut = { playerRadius, 0.0f, 0.0f };
            return true;
        }

        const float overlap = playerRadius - dist;
        pushOut.x = (dx / dist) * overlap;
        pushOut.y = (dy / dist) * overlap;
        pushOut.z = 0.0f;
        return true;
    }

    //==========================================================================
    // World
    //==========================================================================

    World::World(SineCloudAudioProcessor& p)
        : processor(p)
    {
        player = std::make_unique<Player>();
        buildKnobs();
    }

    void World::buildKnobs()
    {
        struct Init {
            const char* paramId;
            const char* label;
            Vec3        worldPos;
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

        player->setMesh(boxMesh.get());
        for (auto& k : knobs)
            k->setMeshes(cylMesh.get(), ptrMesh.get());

        // ---- 加载外部 OBJ 场景模型 ----
        const auto assetsDir = juce::File::getSpecialLocation(
            juce::File::currentApplicationFile)
            .getParentDirectory()
            .getChildFile("assets");

        groundVisMesh = std::make_unique<Mesh>();
        if (groundVisMesh->loadFromObjFile(assetsDir.getChildFile("ground_visual.obj")))
            groundVisMesh->uploadToGPU(ctx);

        propsVisMesh = std::make_unique<Mesh>();
        if (propsVisMesh->loadFromObjFile(assetsDir.getChildFile("props_visual.obj")))
            propsVisMesh->uploadToGPU(ctx);

        groundColMesh = std::make_unique<Mesh>();
        if (groundColMesh->loadFromObjFile(assetsDir.getChildFile("ground_collision.obj")))
            heightField.buildFromMesh(*groundColMesh);

        loadCollidersFromJSON(assetsDir.getChildFile("colliders.json"));
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

        if (groundVisMesh) groundVisMesh->releaseGPU(ctx);
        if (propsVisMesh)  propsVisMesh->releaseGPU(ctx);
        groundVisMesh.reset();
        propsVisMesh.reset();
        groundColMesh.reset();
        colliders.clear();
    }

    void World::loadCollidersFromJSON(const juce::File& jsonFile)
    {
        colliders.clear();

        if (!jsonFile.existsAsFile()) return;

        juce::var root = juce::JSON::parse(jsonFile);
        if (!root.isObject()) return;

        const auto* arr = root["colliders"].getArray();
        if (arr == nullptr) return;

        for (const auto& item : *arr)
        {
            if (!item.isObject()) continue;

            CollisionShape cs;
            const auto& obj = item.getDynamicObject();

            const auto typeStr = obj->getProperty("type").toString();
            if (typeStr == "cylinder")
                cs.type = CollisionShape::Cylinder;
            else if (typeStr == "box")
                cs.type = CollisionShape::Box;
            else
                continue;

            cs.center.x = static_cast<float>(obj->getProperty("x"));
            cs.center.y = static_cast<float>(obj->getProperty("y"));
            cs.center.z = static_cast<float>(obj->getProperty("z"));
            cs.radius = static_cast<float>(obj->getProperty("radius"));
            cs.halfHeight = static_cast<float>(obj->getProperty("halfHeight"));

            colliders.push_back(cs);
        }
    }

    void World::update(float dt, const InputState& in, Camera& cam)
    {
        // 1. 注入相机基向量
        player->setBasis(cam.getForwardOnGround(), cam.getRightOnGround());

        // 2. update entity
        player->update(dt, in);
        for (auto& k : knobs)
            k->update(dt, in);

        // 3. 地面高度钳制
        player->worldPos.z = heightField.sampleHeight(
            player->worldPos.x, player->worldPos.y);

        // 4. 斜坡约束
        const Vec3 groundNormal = heightField.sampleNormal(
            player->worldPos.x, player->worldPos.y);
        // 如果太陡，此处可加回退逻辑（暂时留空）

        // 5. 障碍物碰撞
        resolvePlayerCollisions();

        // 6. 计算 Focused
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
        focusedKnob = nearest;

        // 7. 相机 pivot 跟随
        Vec3 pivot = cam.getPivot();
        pivot.x = easing::damp(pivot.x, player->worldPos.x, pivotFollowRate, dt);
        pivot.y = easing::damp(pivot.y, player->worldPos.y, pivotFollowRate, dt);
        cam.setPivot(pivot);
    }

    void World::resolvePlayerCollisions()
    {
        for (const auto& obs : colliders)
        {
            Vec3 push;
            bool hit = false;

            if (obs.type == CollisionShape::Cylinder)
                hit = CollisionShape::collidePlayerCylinder(
                    player->worldPos, playerRadius, obs, push);
            else if (obs.type == CollisionShape::Box)
                hit = CollisionShape::collidePlayerBox(
                    player->worldPos, playerRadius, obs, push);

            if (hit)
            {
                player->worldPos.x += push.x;
                player->worldPos.y += push.y;
            }
        }

        player->worldPos.z = heightField.sampleHeight(
            player->worldPos.x, player->worldPos.y);
    }

    void World::draw(Renderer& r, const Camera& cam)
    {
        // 场景 OBJ 模型
        if (groundVisMesh && groundVisMesh->isUploaded())
        {
            r.drawMesh(*groundVisMesh, identity(),
                { 0.25f, 0.30f, 0.28f },
                { 0.02f, 0.02f, 0.02f });
        }

        if (propsVisMesh && propsVisMesh->isUploaded())
        {
            r.drawMesh(*propsVisMesh, identity(),
                { 0.40f, 0.38f, 0.34f },
                { 0.01f, 0.01f, 0.01f });
        }

        // 调试网格线
        if (groundMesh)
            r.drawLines(*groundMesh, identity(), { 0.22f, 0.27f, 0.32f });

        // 实体
        for (auto& k : knobs) k->draw(r, cam);
        player->draw(r, cam);
    }

    void World::onMouseWheel(float deltaY)
    {
        if (focusedKnob != nullptr)
            focusedKnob->onMouseWheel(deltaY);
    }

} // namespace sc
