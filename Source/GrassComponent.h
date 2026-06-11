// ==============================================================================
// GrassComponent.h — GPU instanced grass
// ==============================================================================
#pragma once
#include "Mesh.h"
#include "Vec.h"
#include "Mat4.h"
#include "Renderer.h"
#include "GLUtils.h"
#include "Shader.h"
#include <vector>
#include <random>
#include <functional>
#include <unordered_map>

namespace sc {

    struct GrassBlade
    {
        Vec3 root;
        float phaseOffset{ 0.0f };
        float heightScale{ 0.0f };
        float bladeWidth{ 0.0f };
        float colorTint{ 0.0f };
    };

    // 寄到 SSBO 的结构体（std430 布局）
    struct BladeGPUData
    {
        float rootX, rootY, rootZ;
        float phaseOffset;
        float heightScale;
        float bladeWidth;
        float colorTint;
        float _pad; // std430 vec3 + 4 floats = 7 floats，补齐到 8
    };

    class GrassComponent
    {
    public:
        GrassComponent() = default;

        void setGLContext(juce::OpenGLContext* c) noexcept { glCtx = c; }

        void buildFromMeshPoints(
            const std::vector<MeshVertex>& worldVertices,
            float spacing, float bladeHeight, float bladeWidth,
            std::function<float(float, float)> worldHeightFunc);

        void buildFromGrid(
            float minX, float maxX, float minY, float maxY,
            float cellSize, float bladeHeight, float bladeWidth,
            std::function<float(float, float)> heightFunc);

        void buildGPUResources();   // ★ 编译 shader + 上传 SSBO
        void releaseGPU();

        void setColor(const Vec3& srgb) noexcept { grassColorSRGB = srgb; }
        Vec3 getColor() const noexcept { return grassColorSRGB; }

        void setWindIntensity(float w) noexcept { windIntensity = w; }
        float getWindIntensity() const noexcept { return windIntensity; }

        void tick(float dt) noexcept;
        void draw(Renderer& r, const Camera& camera);  // ★ 需要 camera 传 view/proj

        int getBladeCount() const noexcept { return (int)blades.size(); }

    private:
        Vec3 grassColorSRGB{ 0.28f, 0.62f, 0.28f };
        float windIntensity{ 0.0f };
        float currentWind{ 0.0f };
        float totalTime{ 0.0f };
        float bladeBaseHeight{ 0.65f };
        float bladeBaseWidth{ 0.12f };

        std::vector<GrassBlade> blades;

        // GPU 资源
        Shader grassShader;
        GLuint bladeSSBO{ 0 };
        GLuint dummyVAO{ 0 }; // instanced draw 不需要顶点数据
        Mat4 prevViewProj;     // 用于 velocity
        juce::OpenGLContext* glCtx{ nullptr };
        bool shaderBuilt{ false };

        void tryBuildShader();
    };

    // ======================= 实现 =======================

    inline void GrassComponent::tick(float dt) noexcept
    {
        const float smooth = 1.0f - std::exp(-10.0f * dt);
        currentWind = currentWind + (windIntensity - currentWind) * smooth;
        totalTime += dt;
    }

    // buildFromMeshPoints / buildFromGrid 不变（略，同上个版本）
    // ...

    inline void GrassComponent::buildGPUResources()
    {
        if (!glCtx || blades.empty()) return;

        auto& ext = glCtx->extensions;

        // ---- SSBO ----
        std::vector<BladeGPUData> gpuData(blades.size());
        for (size_t i = 0; i < blades.size(); ++i)
        {
            auto& d = gpuData[i];
            d.rootX = blades[i].root.x;
            d.rootY = blades[i].root.y;
            d.rootZ = blades[i].root.z;
            d.phaseOffset = blades[i].phaseOffset;
            d.heightScale = blades[i].heightScale;
            d.bladeWidth = blades[i].bladeWidth;
            d.colorTint = blades[i].colorTint;
            d._pad = 0.0f;
        }

        ext.glGenBuffers(1, &bladeSSBO);
        ext.glBindBuffer(juce::gl::GL_SHADER_STORAGE_BUFFER, bladeSSBO);
        ext.glBufferData(juce::gl::GL_SHADER_STORAGE_BUFFER,
            (GLsizeiptr)(gpuData.size() * sizeof(BladeGPUData)),
            gpuData.data(), juce::gl::GL_STATIC_DRAW);
        ext.glBindBuffer(juce::gl::GL_SHADER_STORAGE_BUFFER, 0);

        // ---- Dummy VAO（instancing 需要） ----
        ext.glGenVertexArrays(1, &dummyVAO);

        // ---- Shader ----
        tryBuildShader();
    }

    inline void GrassComponent::tryBuildShader()
    {
        const juce::String vs = R"(#version 330 core

// SSBO: blade 元数据
layout(std430, binding = 0) readonly buffer BladeBlock {
    float data[];   // 每 blade 8 个 float
};

uniform mat4 uView;
uniform mat4 uProj;
uniform mat4 uPrevViewProj;
uniform float uTime;
uniform float uWind;
uniform float uBladeBaseHeight;
uniform float uBladeBaseWidth;

out vec3 vWorldPos;
out vec3 vWorldPosPrev;
out vec3 vNormal;
out vec2 vUV;
flat out float vColorTint;

const int BLADE_STRIDE = 8;

float bladeNoise(float x, float y, float t, float freq) {
    return sin(x * freq + t * 2.3)
         + cos(y * freq * 1.7 + t * 1.1)
         + sin((x + y) * freq * 0.6 + t * 1.8);
}

void main() {
    int bladeIdx = gl_InstanceID;
    int off = bladeIdx * BLADE_STRIDE;

    vec3 root    = vec3(data[off], data[off+1], data[off+2]);
    float phase  = data[off+3];
    float hScale = data[off+4];
    float bWidth = data[off+5];
    float tint   = data[off+6];

    float h     = uBladeBaseHeight * hScale;
    float halfW = bWidth * 0.5;

    // === 风场计算（与 CPU 版完全一致） ===
    float freq0 = 0.4, freq1 = 0.9, freq2 = 1.7;
    float bigWave = sin(root.x * freq0 + uTime * 0.8)
                  + cos(root.y * freq0 * 0.7 + uTime * 0.65);
    float midNoise = bladeNoise(root.x, root.y, uTime + phase, freq1);
    float micro = sin(uTime * 5.5 + phase) * 0.15;
    float swingAmp = uWind * h * 0.8;
    float swingX = (bigWave * 0.6 + midNoise * 0.3 + micro) * swingAmp;
    float swingY = (cos(root.x * freq2 + uTime * 0.9) * 0.5
                 + bladeNoise(root.x, root.y, uTime * 1.3 + phase, 2.1) * 0.4)
                 * swingAmp * 0.7;

    vec3 tip = root + vec3(swingX, swingY, h * 0.95);
    float compress = 1.0 - uWind * 0.08;
    tip.z = root.z + h * 0.95 * compress;

    vec3 toTip = normalize(tip - root);
    vec3 upRef = vec3(0, 0, 1);
    vec3 right = cross(upRef, toTip);
    if (length(right) < 1e-8) right = vec3(1, 0, 0);
    else right = normalize(right);

    vec3 leftB  = root + right * halfW;
    vec3 rightB = root - right * halfW;
    vec3 midB   = (leftB + rightB) * 0.5;

    vec3 normal = normalize(cross(rightB - leftB, vec3(0,0,1)));

    // === 选顶点 ===
    vec3 pos;
    vec2 uv;
    switch (gl_VertexID) {
        default:
        case 0: pos = leftB;  uv = vec2(0.0, 0.0); break;
        case 1: pos = midB;   uv = vec2(0.5, 0.0); break;
        case 2: pos = rightB; uv = vec2(1.0, 0.0); break;
        case 3: pos = tip;    uv = vec2(0.5, 1.0); break;
    }

    // === 输出 ===
    vec4 worldPos = vec4(pos, 1.0);
    gl_Position = uProj * uView * worldPos;

    vec4 worldPosPrev = uPrevViewProj * worldPos;
    vWorldPos = pos;
    vWorldPosPrev = worldPosPrev.xyz / max(worldPosPrev.w, 1e-6);
    vNormal = normal;
    vUV = uv;
    vColorTint = tint;
}
)";

        const juce::String fs = R"(#version 330 core

in vec3 vWorldPos;
in vec3 vWorldPosPrev;
in vec3 vNormal;
in vec2 vUV;
flat in float vColorTint;

uniform vec3 uTintColor;  // 当前 bucket 的颜色
uniform vec3 uEmissive;

layout(location = 0) out vec4 outAlbedoRough;
layout(location = 1) out vec4 outNormalMetal;
layout(location = 2) out vec4 outEmissiveSSS;
layout(location = 3) out vec2 outVelocity;

void main() {
    // 叶子纹理：越往上越亮
    float tipBright = vUV.y * 0.25;
    vec3 albedo = uTintColor + vec3(tipBright);

    outAlbedoRough   = vec4(albedo, 0.95);   // roughness
    outNormalMetal   = vec4(vNormal * 0.5 + 0.5, 0.0); // metallic=0
    outEmissiveSSS   = vec4(uEmissive, 0.0);
    outVelocity      = vWorldPosPrev.xy - vWorldPos.xy;
}
)";

        if (!grassShader.build(vs, fs))
        {
            DBG("GrassComponent: shader build failed");
            return;
        }
        shaderBuilt = true;
    }

    inline void GrassComponent::draw(Renderer& /*unused*/, const Camera& camera)
    {
        if (!shaderBuilt || blades.empty() || glCtx == nullptr) return;

        auto& ext = glCtx->extensions;

        static const Vec3 kTintColors[3] = {
            { 0.22f, 0.56f, 0.18f },
            { 0.28f, 0.58f, 0.20f },
            { 0.34f, 0.56f, 0.16f },
        };

        grassShader.use();
        grassShader.setMat4("uView", camera.view());
        grassShader.setMat4("uProj", camera.proj());
        grassShader.setMat4("uPrevViewProj", prevViewProj);
        grassShader.setFloat("uTime", totalTime);
        grassShader.setFloat("uWind", currentWind);
        grassShader.setFloat("uBladeBaseHeight", bladeBaseHeight);
        grassShader.setFloat("uBladeBaseWidth", bladeBaseWidth);
        grassShader.setVec3("uEmissive", Vec3{ 0, 0, 0 });

        juce::gl::glBindBufferBase(juce::gl::GL_SHADER_STORAGE_BUFFER, 0, bladeSSBO);
        ext.glBindVertexArray(dummyVAO);

        // 3 次 instanced draw，按 tint 分桶
        int totalN = (int)blades.size();
        for (int bucket = 0; bucket < 3; ++bucket) {
            grassShader.setVec3("uTintColor", kTintColors[bucket]);
            grassShader.setInt("uBucket", bucket);
            juce::gl::glDrawArraysInstanced(juce::gl::GL_TRIANGLE_STRIP, 0, 4, totalN);
        }


        ext.glBindVertexArray(0);
        juce::gl::glBindBufferBase(juce::gl::GL_SHADER_STORAGE_BUFFER, 0, 0);


        prevViewProj = camera.proj() * camera.view();
    }

    inline void GrassComponent::releaseGPU()
    {
        if (!glCtx) return;
        auto& ext = glCtx->extensions;
        if (bladeSSBO) { ext.glDeleteBuffers(1, &bladeSSBO); bladeSSBO = 0; }
        if (dummyVAO) { ext.glDeleteVertexArrays(1, &dummyVAO); dummyVAO = 0; }
        grassShader.raw().release();
        shaderBuilt = false;
    }

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
            b.colorTint = dist01(rng);  // ✅ 新增
            blades.push_back(b);

        }

        DBG("GrassComponent: " << (int)blades.size() << " blades, spacing=" << spacing);
    }

    inline void GrassComponent::buildFromGrid(
        float minX, float maxX, float minY, float maxY,
        float cellSize, float bladeHeight, float bladeWidth,
        std::function<float(float, float)> heightFunc)  // ✅
    {
        std::vector<MeshVertex> verts;
        for (float x = minX; x <= maxX; x += cellSize)
            for (float y = minY; y <= maxY; y += cellSize)
            {
                MeshVertex v;
                v.px = x; v.py = y;
                v.pz = heightFunc(x, y);  // ✅ 回调，不需要 HeightField 完整类型
                verts.push_back(v);
            }
        buildFromMeshPoints(verts, cellSize, bladeHeight, bladeWidth, heightFunc);
    }

    //inline void GrassComponent::releaseGPU()
    //{
    //    if (glCtx)
    //        grassMesh.releaseGPU(*glCtx);
    //}

    //inline void GrassComponent::computeBladeVertices(
    //    const GrassBlade& blade, float time, float wind,
    //    Vec3& tipPos, Vec3& leftBase, Vec3& rightBase) const noexcept
    //{
    //    const float h = bladeBaseHeight * blade.heightScale;
    //    const float halfW = blade.bladeWidth * 0.5f;

    //    // 三层正弦叠加
    //    const float freq0 = 0.4f, freq1 = 0.9f, freq2 = 1.7f;
    //    const float bigWave = std::sin(blade.root.x * freq0 + time * 0.8f)
    //        + std::cos(blade.root.y * freq0 * 0.7f + time * 0.65f);
    //    const float midNoise = bladeNoise(blade.root.x, blade.root.y, time + blade.phaseOffset, freq1);
    //    const float micro = std::sin(time * 5.5f + blade.phaseOffset) * 0.15f;

    //    const float swingAmplitude = wind * h * 0.8f;
    //    const float swingX = (bigWave * 0.6f + midNoise * 0.3f + micro) * swingAmplitude;
    //    const float swingY = (std::cos(blade.root.x * freq2 + time * 0.9f) * 0.5f
    //        + bladeNoise(blade.root.x, blade.root.y, time * 1.3f + blade.phaseOffset, 2.1f) * 0.4f)
    //        * swingAmplitude * 0.7f;

    //    tipPos = blade.root + Vec3{ swingX, swingY, h * 0.95f };
    //    const float compress = 1.0f - wind * 0.08f;
    //    tipPos.z = blade.root.z + h * 0.95f * compress;

    //    const Vec3 upRef{ 0, 0, 1 };
    //    const Vec3 toTip = normalize(tipPos - blade.root);

    //    // ★ 修复：cross 退化检测（无风时 toTip == upRef → 零向量）
    //    Vec3 right = cross(upRef, toTip);
    //    if (lengthSquared(right) < 1e-8f)
    //        right = { 1.0f, 0.0f, 0.0f };
    //    else
    //        right = normalize(right);

    //    leftBase = blade.root + right * halfW;
    //    rightBase = blade.root - right * halfW;
    //}

    //inline void GrassComponent::draw(Renderer& r)
    //{
    //    if (blades.empty() || glCtx == nullptr) return;
    //    const int N = (int)blades.size();
    //    const float time = totalTime;
    //    const float wind = currentWind;

    //    static const Vec3 kTintColors[3] = {
    //        { 0.22f, 0.56f, 0.18f },
    //        { 0.28f, 0.58f, 0.20f },
    //        { 0.34f, 0.56f, 0.16f },
    //    };

    //    // ★ 一次遍历，全量收集到同一个 buffer
    //    std::vector<MeshVertex> allVerts;
    //    std::vector<uint32_t>      allIndices;
    //    allVerts.reserve(N * 4 + 16);
    //    allIndices.reserve(N * 6 + 32);

    //    // 记录每个 bucket 在 index buffer 中的范围
    //    int bucketStartByte[3] = {};
    //    int bucketCount[3] = {};

    //    for (int bucket = 0; bucket < 3; ++bucket)
    //    {
    //        bucketStartByte[bucket] = (int)(allIndices.size() * sizeof(uint32_t));

    //        for (int i = 0; i < N; ++i)
    //        {
    //            int b = (int)(blades[i].colorTint * 3.0f);
    //            if (b >= 3) b = 2;
    //            if (b != bucket) continue;

    //            Vec3 tipPos, leftBase, rightBase;
    //            computeBladeVertices(blades[i], time, wind,
    //                tipPos, leftBase, rightBase);

    //            const Vec3 midBase = (leftBase + rightBase) * 0.5f;
    //            const Vec3 normal = normalize(
    //                cross(rightBase - leftBase, Vec3{ 0, 0, 1 }));

    //            const uint32_t baseIdx = static_cast<uint32_t>(allVerts.size());
    //            auto addVert = [&](const Vec3& pos, float u, float v) {
    //                MeshVertex mv;
    //                mv.px = pos.x; mv.py = pos.y; mv.pz = pos.z;
    //                mv.nx = normal.x; mv.ny = normal.y; mv.nz = normal.z;
    //                mv.u = u; mv.v = v;
    //                allVerts.push_back(mv);
    //                };
    //            addVert(leftBase, 0.0f, 0.0f);
    //            addVert(midBase, 0.5f, 0.0f);
    //            addVert(rightBase, 1.0f, 0.0f);
    //            addVert(tipPos, 0.5f, 1.0f);

    //            allIndices.push_back(baseIdx + 0);
    //            allIndices.push_back(baseIdx + 1);
    //            allIndices.push_back(baseIdx + 3);
    //            allIndices.push_back(baseIdx + 1);
    //            allIndices.push_back(baseIdx + 2);
    //            allIndices.push_back(baseIdx + 3);
    //        }
    //        bucketCount[bucket] =
    //            (int)allIndices.size()
    //            - bucketStartByte[bucket] / (int)sizeof(uint32_t);
    //    }

    //    if (allVerts.empty()) return;

    //    // ★ 只上传一次
    //    grassMesh.vertices = std::move(allVerts);
    //    grassMesh.indices = std::move(allIndices);
    //    grassMesh.uploadToGPU(*glCtx);

    //    // ★ 三次绘制，共享同一个 VBO，仅 index 范围不同
    //    for (int bucket = 0; bucket < 3; ++bucket)
    //    {
    //        if (bucketCount[bucket] == 0) continue;
    //        r.drawMesh(grassMesh, identity(),
    //            kTintColors[bucket],
    //            Vec3{ 0.03f, 0.06f, 0.01f },
    //            bucketCount[bucket],
    //            bucketStartByte[bucket]);
    //    }
    //}



} // namespace sc
