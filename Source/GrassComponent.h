// ==============================================================================
// GrassComponent.h — corrected (no ctx arg in draw)
// ==============================================================================
#pragma once
#include "Mesh.h"
#include "Vec.h"
#include "Mat4.h"
#include "Renderer.h"
#include "World.h"       // for HeightField (forward declare or include)
#include <vector>
#include <memory>
#include <random>
#include <unordered_map>

namespace sc {

    // =============================================================================
    // GrassBlade
    // =============================================================================
    struct GrassBlade
    {
        Vec3   root;
        float  phaseOffset;
        float  heightScale;
        float  bladeWidth;
    };

    // =============================================================================
    // GrassComponent
    // =============================================================================
    class GrassComponent
    {
    public:
        GrassComponent() = default;

        /** 在 World::uploadMeshes() 中调用一次即可 */
        void setGLContext(juce::OpenGLContext* c) noexcept { glCtx = c; }

        // -------- 建造 --------
        void buildFromMeshPoints(
            const std::vector<MeshVertex>& worldVertices,
            float spacing,
            float bladeHeight,
            float bladeWidth,
            std::function<float(float, float)> worldHeightFunc);

        void buildFromGrid(
            float minX, float maxX,
            float minY, float maxY,
            float cellSize,
            float bladeHeight,
            float bladeWidth,
            const HeightField& hf);

        void releaseGPU();

        // -------- 参数 --------
        void setColor(const Vec3& srgb) noexcept { grassColorSRGB = srgb; }
        Vec3 getColor() const noexcept { return grassColorSRGB; }

        void setWindIntensity(float w) noexcept { windIntensity = w; }
        float getWindIntensity() const noexcept { return windIntensity; }

        void tick(float dt) noexcept
        {
            const float smooth = 1.0f - std::exp(-10.0f * dt);
            currentWind = currentWind + (windIntensity - currentWind) * smooth;
            totalTime += dt;
        }

        // -------- 渲染：只需 Renderer，不需要 ctx / cam --------
        void draw(Renderer& r);

        int getBladeCount() const noexcept { return (int)blades.size(); }

    private:
        // ---- 参数 ----
        Vec3   grassColorSRGB{ 0.28f, 0.62f, 0.28f };
        float  windIntensity{ 0.0f };
        float  currentWind{ 0.0f };
        float  totalTime{ 0.0f };
        float  bladeBaseHeight{ 0.65f };
        float  bladeBaseWidth{ 0.12f };

        // ---- 数据 ----
        std::vector<GrassBlade> blades;
        Mesh grassMesh;
        juce::OpenGLContext* glCtx{ nullptr };

        // ---- 内部 ----
        void computeBladeVertices(
            const GrassBlade& blade, float time, float wind,
            Vec3& tipPos, Vec3& leftBase, Vec3& rightBase) const noexcept;

        static float bladeNoise(float x, float y, float t, float freq) noexcept
        {
            return std::sin(x * freq + t * 2.3f)
                + std::cos(y * freq * 1.7f + t * 1.1f)
                + std::sin((x + y) * freq * 0.6f + t * 1.8f);
        }
    };

    // ========================= 实现 =========================

    inline void GrassComponent::buildFromMeshPoints(
        const std::vector<MeshVertex>& worldVertices,
        float spacing, float bladeHeight, float bladeWidth,
        std::function<float(float, float)> worldHeightFunc)
    {
        bladeBaseHeight = bladeHeight;
        bladeBaseWidth = bladeWidth;
        blades.clear();

        std::mt19937 rng(42);
        std::uniform_real_distribution<float> dist01(0.0f, 1.0f);
        const float cell = juce::jmax(0.01f, spacing);
        std::unordered_map<int64_t, bool> usedCells;

        for (const auto& v : worldVertices)
        {
            if (std::abs(v.px) > 50.0f || std::abs(v.py) > 50.0f) continue;

            const int cx = (int)std::floor(v.px / cell);
            const int cy = (int)std::floor(v.py / cell);
            const int64_t key = (static_cast<int64_t>(cx) << 32)
                | static_cast<int64_t>(static_cast<uint32_t>(cy));
            if (usedCells.count(key)) continue;
            usedCells[key] = true;

            GrassBlade b;
            b.root.x = v.px;
            b.root.y = v.py;
            b.root.z = worldHeightFunc ? worldHeightFunc(v.px, v.py) : v.pz;
            b.phaseOffset = dist01(rng) * juce::MathConstants<float>::twoPi;
            b.heightScale = 0.65f + dist01(rng) * 0.7f;
            b.bladeWidth = bladeWidth * (0.7f + dist01(rng) * 0.6f);
            blades.push_back(b);
        }
        DBG("GrassComponent: " << (int)blades.size() << " blades, spacing=" << spacing);
    }

    inline void GrassComponent::buildFromGrid(
        float minX, float maxX, float minY, float maxY,
        float cellSize, float bladeHeight, float bladeWidth, const HeightField& hf)
    {
        std::vector<MeshVertex> verts;
        for (float x = minX; x <= maxX; x += cellSize)
            for (float y = minY; y <= maxY; y += cellSize)
            {
                MeshVertex v;
                v.px = x; v.py = y; v.pz = hf.sampleHeight(x, y);
                verts.push_back(v);
            }
        auto hFunc = [&hf](float x, float y) { return hf.sampleHeight(x, y); };
        buildFromMeshPoints(verts, cellSize, bladeHeight, bladeWidth, hFunc);
    }

    inline void GrassComponent::releaseGPU()
    {
        if (glCtx)
            grassMesh.releaseGPU(*glCtx);
    }

    inline void GrassComponent::computeBladeVertices(
        const GrassBlade& blade, float time, float wind,
        Vec3& tipPos, Vec3& leftBase, Vec3& rightBase) const noexcept
    {
        const float h = bladeBaseHeight * blade.heightScale;
        const float halfW = blade.bladeWidth * 0.5f;

        // 三层正弦叠加
        const float freq0 = 0.4f, freq1 = 0.9f, freq2 = 1.7f;
        const float bigWave = std::sin(blade.root.x * freq0 + time * 0.8f)
            + std::cos(blade.root.y * freq0 * 0.7f + time * 0.65f);
        const float midNoise = bladeNoise(blade.root.x, blade.root.y, time + blade.phaseOffset, freq1);
        const float micro = std::sin(time * 5.5f + blade.phaseOffset) * 0.15f;

        const float swingAmplitude = wind * h * 0.8f;
        const float swingX = (bigWave * 0.6f + midNoise * 0.3f + micro) * swingAmplitude;
        const float swingY = (std::cos(blade.root.x * freq2 + time * 0.9f) * 0.5f
            + bladeNoise(blade.root.x, blade.root.y, time * 1.3f + blade.phaseOffset, 2.1f) * 0.4f)
            * swingAmplitude * 0.7f;

        tipPos = blade.root + Vec3{ swingX, swingY, h * 0.95f };
        const float compress = 1.0f - wind * 0.08f;
        tipPos.z = blade.root.z + h * 0.95f * compress;

        const Vec3 upRef{ 0, 0, 1 };
        const Vec3 toTip = normalize(tipPos - blade.root);
        const Vec3 right = normalize(cross(upRef, toTip));
        leftBase = blade.root + right * halfW;
        rightBase = blade.root - right * halfW;
    }

    inline void GrassComponent::draw(Renderer& r)
    {
        if (blades.empty() || glCtx == nullptr) return;

        const int N = (int)blades.size();
        const float time = totalTime;
        const float wind = currentWind;

        // ---- 重建顶点 & 索引 ----
        std::vector<MeshVertex> verts;
        std::vector<uint32_t> indices;
        verts.reserve(N * 4);
        indices.reserve(N * 6);

        for (int i = 0; i < N; ++i)
        {
            Vec3 tipPos, leftBase, rightBase;
            computeBladeVertices(blades[i], time, wind, tipPos, leftBase, rightBase);
            const Vec3 midBase = (leftBase + rightBase) * 0.5f;
            const Vec3 normal = normalize(cross(rightBase - leftBase, Vec3{ 0, 0, 1 }));

            const uint32_t baseIdx = static_cast<uint32_t>(verts.size());
            auto addVert = [&](const Vec3& pos, float u, float v) {
                MeshVertex mv;
                mv.px = pos.x; mv.py = pos.y; mv.pz = pos.z;
                mv.nx = normal.x; mv.ny = normal.y; mv.nz = normal.z;
                mv.u = u; mv.v = v;
                verts.push_back(mv);
                };
            addVert(leftBase, 0.0f, 0.0f);
            addVert(midBase, 0.5f, 0.0f);
            addVert(rightBase, 1.0f, 0.0f);
            addVert(tipPos, 0.5f, 1.0f);

            // 三角形 0: leftBase → midBase → tip
            indices.push_back(baseIdx + 0);
            indices.push_back(baseIdx + 1);
            indices.push_back(baseIdx + 3);
            // 三角形 1: midBase → rightBase → tip
            indices.push_back(baseIdx + 1);
            indices.push_back(baseIdx + 2);
            indices.push_back(baseIdx + 3);
        }

        // ---- 上传 → 绘制 → 释放 ----
        grassMesh.releaseGPU(*glCtx);
        grassMesh.vertices = std::move(verts);
        grassMesh.indices = std::move(indices);
        grassMesh.uploadToGPU(*glCtx);

        r.drawMesh(grassMesh, identity(),
            grassColorSRGB,
            Vec3{ 0.05f, 0.10f, 0.02f });

        grassMesh.releaseGPU(*glCtx);
    }

} // namespace sc
