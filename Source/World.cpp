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
// HeightField  (三角面 + 重心插值版)
//==========================================================================

    int64_t HeightField::cellKey(int cx, int cy) const noexcept
    {
        // 固定语义：高32位=cx，低32位=cy（均按补码截断）。build/query 一致即可唯一。
        return (static_cast<int64_t>(cx) << 32)
            | (static_cast<int64_t>(static_cast<uint32_t>(cy)));
    }

    void HeightField::buildFromMesh(const Mesh& mesh)
    {
        tris.clear();
        spatialMap.clear();

        const auto& V = mesh.vertices;
        const auto& I = mesh.indices;
        if (I.size() < 3) return;

        // ---- 1) 自适应 cellSize：用三角形平均边长，避免与 mesh 密度脱钩 ----
        double edgeSum = 0.0; int edgeCnt = 0;
        for (size_t i = 0; i + 2 < I.size(); i += 3)
        {
            const auto& p0 = V[I[i]];   const auto& p1 = V[I[i + 1]];
            const float e = std::hypot(p1.px - p0.px, p1.py - p0.py);
            edgeSum += e; ++edgeCnt;
        }
        if (edgeCnt > 0)
            cellSize = juce::jmax(0.1f, (float)(edgeSum / edgeCnt));

        // ---- 2) 建三角形 + 预算法线，并把三角形索引塞进它 AABB 覆盖的所有格子 ----
        tris.reserve(I.size() / 3);
        double zAccum = 0.0;

        for (size_t i = 0; i + 2 < I.size(); i += 3)
        {
            const auto& v0 = V[I[i]];
            const auto& v1 = V[I[i + 1]];
            const auto& v2 = V[I[i + 2]];

            Tri t;
            t.a = { v0.px, v0.py, v0.pz };
            t.b = { v1.px, v1.py, v1.pz };
            t.c = { v2.px, v2.py, v2.pz };

            Vec3 n = cross(t.b - t.a, t.c - t.a);
            const float l2 = n.x * n.x + n.y * n.y + n.z * n.z;
            n = (l2 < 1.0e-12f) ? Vec3{ 0,0,1 } : n * (1.0f / std::sqrt(l2));
            if (n.z < 0.0f) n = n * -1.0f;        // 法线统一朝上
            t.normal = n;

            const int idx = (int)tris.size();
            tris.push_back(t);
            zAccum += (t.a.z + t.b.z + t.c.z) / 3.0;

            // 三角形在 XY 上的 AABB -> 覆盖的格子范围
            const float minX = std::min({ t.a.x, t.b.x, t.c.x });
            const float maxX = std::max({ t.a.x, t.b.x, t.c.x });
            const float minY = std::min({ t.a.y, t.b.y, t.c.y });
            const float maxY = std::max({ t.a.y, t.b.y, t.c.y });

            const int cx0 = (int)std::floor(minX / cellSize);
            const int cx1 = (int)std::floor(maxX / cellSize);
            const int cy0 = (int)std::floor(minY / cellSize);
            const int cy1 = (int)std::floor(maxY / cellSize);

            for (int cx = cx0; cx <= cx1; ++cx)
                for (int cy = cy0; cy <= cy1; ++cy)
                    spatialMap[cellKey(cx, cy)].push_back(idx);
        }

        fallbackZ = tris.empty() ? 0.0f : (float)(zAccum / (double)tris.size());
    }

    bool HeightField::sampleTri(const Tri& t, float x, float y,
        float& outZ, Vec3& outN) const noexcept
    {
        // 在 XY 平面用重心坐标判断 (x,y) 是否落在三角形内
        const float x1 = t.a.x, y1 = t.a.y;
        const float x2 = t.b.x, y2 = t.b.y;
        const float x3 = t.c.x, y3 = t.c.y;

        const float det = (y2 - y3) * (x1 - x3) + (x3 - x2) * (y1 - y3);
        if (std::abs(det) < 1.0e-12f) return false; // 退化三角形（XY 投影成线）

        const float inv = 1.0f / det;
        const float l1 = ((y2 - y3) * (x - x3) + (x3 - x2) * (y - y3)) * inv;
        const float l2 = ((y3 - y1) * (x - x3) + (x1 - x3) * (y - y3)) * inv;
        const float l3 = 1.0f - l1 - l2;

        const float e = -1.0e-4f; // 小容差，避免边界缝隙漏采
        if (l1 < e || l2 < e || l3 < e) return false;

        outZ = l1 * t.a.z + l2 * t.b.z + l3 * t.c.z; // 面上精确插值
        outN = t.normal;
        return true;
    }

    float HeightField::sampleHeight(float x, float y) const
    {
        if (tris.empty()) return fallbackZ;

        const int cx = (int)std::floor(x / cellSize);
        const int cy = (int)std::floor(y / cellSize);

        const auto it = spatialMap.find(cellKey(cx, cy));
        if (it != spatialMap.end())
        {
            // 多三角覆盖（如悬崖/重叠面）时取最高，避免穿模
            float best = -std::numeric_limits<float>::max();
            bool hit = false;
            for (int idx : it->second)
            {
                float z; Vec3 n;
                if (sampleTri(tris[(size_t)idx], x, y, z, n) && z > best)
                {
                    best = z; hit = true;
                }
            }
            if (hit) return best;
        }
        return fallbackZ; // 落在网格空洞外：返回平均高度而非 0
    }

    Vec3 HeightField::sampleNormal(float x, float y) const
    {
        if (tris.empty()) return { 0.0f, 0.0f, 1.0f };

        const int cx = (int)std::floor(x / cellSize);
        const int cy = (int)std::floor(y / cellSize);

        const auto it = spatialMap.find(cellKey(cx, cy));
        if (it != spatialMap.end())
        {
            float best = -std::numeric_limits<float>::max();
            Vec3 bestN{ 0, 0, 1 }; bool hit = false;
            for (int idx : it->second)
            {
                float z; Vec3 n;
                if (sampleTri(tris[(size_t)idx], x, y, z, n) && z > best)
                {
                    best = z; bestN = n; hit = true;
                }
            }
            if (hit) return bestN; // 直接取所在三角形的几何法线，无需有限差分
        }
        return { 0.0f, 0.0f, 1.0f };
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
        
        boxMesh = Mesh::createBox(1.0f, 1.0f, 1.0f);
        cylMesh = Mesh::createCylinder(1.0f, 1.0f, 32);
        ptrMesh = Mesh::createBox(1.0f, 1.0f, 1.0f);

        
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
        
        if (boxMesh)    boxMesh->releaseGPU(ctx);
        if (cylMesh)    cylMesh->releaseGPU(ctx);
        if (ptrMesh)    ptrMesh->releaseGPU(ctx);
        
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

        //playerz
        const float gz = heightField.sampleHeight(player->worldPos.x,
            player->worldPos.y);
        // 直接贴地；如需平滑可改用 easing::damp(player->worldPos.z, gz, rate, dt)
        player->worldPos.z = easing::damp(player->worldPos.z, gz, groundFollowRate, dt);


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
