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
#include <functional>
#include <memory>
#include <random>

namespace sc {

    struct GrassBlade
    {
        Vec3  root;
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
        Vec3  grassColorSRGB{ 0.28f, 0.62f, 0.28f };
        float windIntensity{ 0.0f };
        float currentWind{ 0.0f };
        float totalTime{ 0.0f };
        float bladeBaseHeight{ 0.65f };
        float bladeBaseWidth{ 0.12f };

        std::vector<GrassBlade> blades;

        // GPU 资源
        std::unique_ptr<Shader> grassShader;   // ★ 延迟构建，避免默认构造
        GLuint bladeSSBO{ 0 };
        GLuint dummyVAO{ 0 };

        Mat4 prevViewProj;                    // 用于 velocity

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
            b.colorTint = dist01(rng);
            blades.push_back(b);
        }

        DBG("GrassComponent: " << (int)blades.size() << " blades, spacing=" << spacing);
    }

    inline void GrassComponent::buildFromGrid(
        float minX, float maxX, float minY, float maxY,
        float cellSize, float bladeHeight, float bladeWidth,
        std::function<float(float, float)> heightFunc)
    {
        std::vector<MeshVertex> verts;
        for (float x = minX; x <= maxX; x += cellSize)
            for (float y = minY; y <= maxY; y += cellSize)
            {
                MeshVertex v;
                v.px = x;
                v.py = y;
                v.pz = heightFunc(x, y);
                verts.push_back(v);
            }

        buildFromMeshPoints(verts, cellSize, bladeHeight, bladeWidth, heightFunc);
    }

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
        ext.glBindVertexArray(dummyVAO);
        // shader 全用 gl_VertexID + gl_InstanceID，不需要任何 vertex attribute
        // core profile 要求 VAO 绑了即可，无需 enable 空 attribute
        ext.glBindVertexArray(0);



        // ---- Shader ----
        grassShader = std::make_unique<Shader>(*glCtx);
        tryBuildShader();
    }

    inline void GrassComponent::tryBuildShader()
    {
        const juce::String vs = R"(#version 430 core

// SSBO: blade 元数据
layout(std430, binding = 0) readonly buffer BladeBlock {
    float data[]; // 每 blade 8 个 float
};

uniform mat4 uView;
uniform mat4 uProj;
uniform mat4 uPrevViewProj;
uniform float uTime;
uniform float uWind;
uniform float uBladeBaseHeight;
uniform float uBladeBaseWidth;
uniform vec3 uGrassColor;

out vec3 vWorldPos;
out vec3 vWorldPosPrev;
out vec3 vNormal;
out vec3 vNdcNow;
out vec3 vNdcPrev;

out vec2 vUV;
flat out vec3 vBucketTint;

const int BLADE_STRIDE = 8;

float bladeNoise(float x, float y, float t, float freq) {
    return sin(x * freq + t * 2.3)
         + cos(y * freq * 1.7 + t * 1.1)
         + sin((x + y) * freq * 0.6 + t * 1.8);
}

void main() {
    int bladeIdx = gl_InstanceID;
    int off = bladeIdx * BLADE_STRIDE;

    vec3 root       = vec3(data[off],   data[off+1], data[off+2]);
    float phase     = data[off+3];
    float hScale    = data[off+4];
    float bWidth    = data[off+5];
    float tint      = data[off+6];

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
    
    vec3 leftB = root + right * halfW;
    vec3 rightB = root - right * halfW;

    
    vec3 flatNormal = normalize(cross(rightB - leftB, vec3(0,0,1)));
    vec3 upBlend = vec3(0, 0, 1);
    vec3 normal = normalize(mix(flatNormal, upBlend, 0.4));  // 40% 朝上


    // === 选顶点 ===
    vec3 pos;
    vec2 uv;
    switch (gl_VertexID) {
        default:
        case 0: pos = leftB; uv = vec2(0.0, 0.0); break;
        case 1: pos = rightB; uv = vec2(1.0, 0.0); break;
        case 2: pos = tip; uv = vec2(0.5, 1.0); break;
    }


   // === 输出 ===
    vec4 worldPos = vec4(pos, 1.0);
    vec4 clipNow  = uProj * uView * worldPos;
    vec4 clipPrev = uPrevViewProj * worldPos;
    gl_Position = clipNow;

    vWorldPos   = pos;
    vNdcNow     = clipNow.xyz / max(clipNow.w, 1e-6);
    vNdcPrev    = clipPrev.xyz / max(clipPrev.w, 1e-6);

    vNormal       = normal;
    vUV           = uv;
    // === 每草叶 tint bucket（替代 CPU 端三次绘制） ===
    const vec3 tints[3] = vec3[3](
        vec3(0.25, 0.00, 0.08),
        vec3(0.28, 0.01, 0.02),
        vec3(0.24, 0.01, 0.06)
    );
    vBucketTint = tints[bladeIdx % 3] ;
}
)";


const juce::String fs = R"(#version 430 core
in vec3 vWorldPos;
in vec3 vNormal;
in vec3 vNdcNow;
in vec3 vNdcPrev;
in vec2 vUV;
flat in vec3 vBucketTint;

uniform vec3 uEmissive;

layout(location = 0) out vec4 outAlbedoRough;
layout(location = 1) out vec4 outNormalMetal;
layout(location = 2) out vec4 outEmissiveSSS;
layout(location = 3) out vec2 outVelocity;
layout(location = 4) out vec3 outWorldPos;

void main() {
    float tipBright = vUV.y * 0.25;
    vec3 albedo = vBucketTint + vec3(tipBright);
    outAlbedoRough = vec4(albedo, 0.95);
    outNormalMetal = vec4(vNormal * 0.5 + 0.5, 0.0);
    outEmissiveSSS = vec4(uEmissive, 0.0);
    outVelocity = (vNdcNow.xy - vNdcPrev.xy) * 0.5;
    outWorldPos = vWorldPos;
}
)";


        if (!grassShader->build(vs, fs))
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

        grassShader->use();
        grassShader->setMat4("uView", camera.view());
        grassShader->setMat4("uProj", camera.proj());
        grassShader->setMat4("uPrevViewProj", prevViewProj);
        grassShader->setFloat("uTime", totalTime);
        grassShader->setFloat("uWind", currentWind);
        grassShader->setFloat("uBladeBaseHeight", bladeBaseHeight);
        grassShader->setFloat("uBladeBaseWidth", bladeBaseWidth);
        grassShader->setVec3("uEmissive", Vec3{ 0, 0, 0 });
        grassShader->setVec3("uGrassColor", grassColorSRGB);  // ← 新增

        juce::gl::glBindBufferBase(juce::gl::GL_SHADER_STORAGE_BUFFER, 0, bladeSSBO);

        ext.glBindVertexArray(dummyVAO);

        int totalN = (int)blades.size();
        juce::gl::glDrawArraysInstanced(juce::gl::GL_TRIANGLES, 0, 3, totalN);


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

        if (grassShader)
        {
            grassShader->raw().release();
            grassShader.reset();
        }
        shaderBuilt = false;
    }

} // namespace sc
