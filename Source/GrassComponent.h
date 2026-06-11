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

        void setWindDirection(const Vec2& d) noexcept { windDir = d; }

    private:
        Vec3  grassColorSRGB{ 0.28f, 0.62f, 0.28f };
        float windIntensity{ 2.0f };
        float currentWind{ 0.0f };
        float totalTime{ 0.0f };
        float bladeBaseHeight{ 0.65f };
        float bladeBaseWidth{ 0.12f };
        Vec2 windDir{ 0.5f, 0.866f };

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

layout(std430, binding = 0) readonly buffer BladeBlock {
    float data[];
};

uniform mat4 uView;
uniform mat4 uProj;
uniform mat4 uPrevViewProj;
uniform float uTime;
uniform float uWind;
uniform float uBladeBaseHeight;
uniform float uBladeBaseWidth;
uniform vec3 uGrassColor;
uniform vec2 uWindDir;

out vec3 vWorldPos;
out vec3 vWorldPosPrev;
out vec3 vNormal;
out vec3 vNdcNow;
out vec3 vNdcPrev;
out vec2 vUV;
flat out vec3 vBucketTint;

const int BLADE_STRIDE = 8;

// =============================================
// Hash & 连续噪声函数（用于 gust 大尺度场）
// =============================================
float hash12(vec2 p) {
    float h = dot(p, vec2(127.1, 311.7));
    return fract(sin(h) * 43758.5453123);
}

// 2D value noise（C2 连续，无周期）
float noise2D(vec2 p) {
    vec2 i = floor(p);
    vec2 f = fract(p);
    f = f * f * (3.0 - 2.0 * f); // smoothstep
    return mix(
        mix(hash12(i),              hash12(i + vec2(1.0, 0.0)), f.x),
        mix(hash12(i + vec2(0.0, 1.0)), hash12(i + vec2(1.0, 1.0)), f.x),
        f.y
    );
}

// 2 阶 FBM（2 octaves 足够低频用途）
float fbm2(vec2 p) {
    return noise2D(p) * 0.6 + noise2D(p * 2.1) * 0.4;
}

void main() {
    int bladeIdx = gl_InstanceID;
    int off = bladeIdx * BLADE_STRIDE;

    vec3 root   = vec3(data[off],   data[off+1], data[off+2]);
    float phase = data[off+3];
    float hScale = data[off+4];
    float bWidth = data[off+5];

    float h     = uBladeBaseHeight * hScale;
    float halfW = bWidth * 0.5;

    vec2 windDir = normalize(uWindDir);

    // =============================================
    // ★ 阵风场（Gust Field）— 核心新增
    // =============================================
    // 空间尺度：除以 18 意味着 18 单位才完成一次噪声周期的大致过渡
    // → 相邻 5~10 单位内的 blade 会得到相似的 gust 值（相干弯曲）
    vec2 gustCoord = root.xy / 18.0;
    // gustSample 随时间极慢移动，约 0.05 单位/秒 → 20 秒才平移 1 个噪声格
    float gustVal = fbm2(gustCoord + uTime * 0.05);
    // gustVal ∈ [0,1]，重映射到 [0.15, 1.0] 作为振幅调制系数
    float gustAmp = 0.15 + gustVal * 0.85;

    // =============================================
    // 风场分量（保留结构，各分量的振幅由 gustAmp 调制）
    // =============================================

    // -- 全局传播波（沿风方向） --
    float waveCoord = dot(root.xy, windDir);
    float waveFreq  = 0.3;                  // 原 1.6 → 波长 ≈ 21 单位，真正大尺度
    float waveSpeed = 0.4 + uWind * 0.15;   // 慢速传播
    float wave1 = sin(waveCoord * waveFreq - uTime * waveSpeed + 0.7);
    float wave2 = sin(waveCoord * waveFreq * 1.6 - uTime * waveSpeed * 1.3 + 2.1) * 0.35;

    // -- blade 独立随机波（降速） --
    float rf1 = 3.0 + fract(phase * 7.239) * 4.0;
    float rf2 = 5.0 + fract(phase * 3.847) * 6.0;
    float rs1 = 0.15 + fract(phase * 5.13) * 0.3;
    float rs2 = 0.2  + fract(phase * 9.651) * 0.4;

    float local1 = sin(root.x * rf1 + uTime * rs1 + phase);
    float local2 = cos(root.y * rf2 - uTime * rs2 + phase * 2.1);
    float local3 = sin((root.x + root.y) * 3.5 - uTime * 0.8 + phase * 0.7);

    // -- 高频微颤（保留但降速） --
    float micro1 = sin(root.x * 7.0 + uTime * 0.4 + phase * 3.1) * 0.22
                 + cos(root.y * 6.3 - uTime * 0.35 + phase * 1.7) * 0.18;

    // =============================================
    // 合成：所有分量 × gustAmp
    // =============================================
    float combined =
        (wave1  * 0.45 +   // 大尺度波权重提升到主导
         wave2  * 0.15 +
         local1 * 0.14 +
         local2 * 0.10 +
         local3 * 0.08 +
         micro1
        ) * gustAmp;        // ★ 阵风调制：整片区域振幅同步变化

    float swingAmp = uWind * h * 1.8;
    float swingX = combined * swingAmp * windDir.x;
    float swingY = combined * swingAmp * windDir.y;

    // ===== blade 几何体 =====
    vec3 tip = root + vec3(swingX, swingY, h * 0.95);
    float compress = 1.0 - uWind * 0.06;
    tip.z = root.z + h * 0.95 * compress;

    vec3 toTip = normalize(tip - root);
    vec3 upRef = vec3(0.0, 0.0, 1.0);

    vec3 rightBase = cross(upRef, toTip);
    if (length(rightBase) < 1e-8) rightBase = vec3(1.0, 0.0, 0.0);
    else rightBase = normalize(rightBase);

    // 随机朝向
    float rotAngle = fract(phase * 1.618 + bladeIdx * 0.276) * 6.283185307;
    float cosA = cos(rotAngle);
    float sinA = sin(rotAngle);
    vec3 right = rightBase * cosA + cross(toTip, rightBase) * sinA;

    vec3 leftB  = root + right * halfW;
    vec3 rightB = root - right * halfW;

    vec3 flatNormal = normalize(cross(rightB - leftB, vec3(0.0, 0.0, 1.0)));
    vec3 upBlend    = vec3(0.0, 0.0, 1.0);
    vec3 normal     = normalize(mix(flatNormal, upBlend, 0.4));

    vec3 pos;
    vec2 uv;
    switch (gl_VertexID) {
        default:
        case 0: pos = leftB;  uv = vec2(0.0, 0.0); break;
        case 1: pos = rightB; uv = vec2(1.0, 0.0); break;
        case 2: pos = tip;    uv = vec2(0.5, 1.0); break;
    }

    vec4 worldPos = vec4(pos, 1.0);
    vec4 clipNow  = uProj * uView * worldPos;
    vec4 clipPrev = uPrevViewProj * worldPos;

    gl_Position   = clipNow;
    vWorldPos     = pos;
    vNdcNow       = clipNow.xyz  / max(clipNow.w, 1e-6);
    vNdcPrev      = clipPrev.xyz / max(clipPrev.w, 1e-6);
    vNormal       = normal;
    vUV           = uv;

    const vec3 tints[3] = vec3[3](
        vec3(0.25, 0.20, 0.08),
        vec3(0.24, 0.22, 0.06),
        vec3(0.28, 0.20, 0.07)
    );
    vBucketTint = tints[bladeIdx % 3];
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
uniform vec3 uGrassColor;    // ← 现在真正参与计算

layout(location = 0) out vec4 outAlbedoRough;
layout(location = 1) out vec4 outNormalMetal;
layout(location = 2) out vec4 outEmissiveSSS;
layout(location = 3) out vec2 outVelocity;
layout(location = 4) out vec3 outWorldPos;

void main() {
    // -- 叶尖微亮 (降低系数: 0.25 → 0.12) --
    float tipBright = vUV.y * 0.12;

    // -- 混合：tint 作为品种色，uGrassColor 作为全局调色 --
    // lerp(tint, uGrassColor, 0.6) 使得可调范围足够大
    vec3 base = mix(vBucketTint, uGrassColor, 0.6);

    // -- 最终 albedo：底部深、叶尖略亮 --
    vec3 albedo = base + vec3(tipBright * 0.8, tipBright, tipBright * 0.6);

    outAlbedoRough  = vec4(albedo, 0.95);
    outNormalMetal  = vec4(vNormal * 0.5 + 0.5, 0.0);
    outEmissiveSSS  = vec4(uEmissive, 0.0);
    outVelocity      = (vNdcNow.xy - vNdcPrev.xy) * 0.5;
    outWorldPos      = vWorldPos;
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
        grassShader->setVec2("uWindDir", windDir.x, windDir.y);


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