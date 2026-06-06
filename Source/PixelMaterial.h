/* ==============================================================================
   PixelMaterial.h
   Layer 2: Scene & Renderer
   像素风共享 material：方向光 toon + 简化 PBR（GGX + Fresnel） + 色阶量化。
   保留原有 Reinhard tonemap 与 gamma 校正。
   ============================================================================== */
#pragma once
#include <JuceHeader.h >
#include "Shader.h"
#include "Lighting.h"
#include "Material.h"

namespace sc
{
    class PixelMaterial
    {
    public:
        explicit PixelMaterial(juce::OpenGLContext& ctx) : shader(ctx) {}

        bool build()
        {
            // ==================== 顶点着色器 ====================
            const juce::String vs = R"(#version 330 core
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec2 aUV;

uniform mat4 uModel;
uniform mat4 uView;
uniform mat4 uProj;

out vec3 vNormalWS;
out vec3 vPosWS;
out vec2 vUV;

void main()
{
    vec4 wp = uModel * vec4(aPos, 1.0);
    vPosWS = wp.xyz;
    vNormalWS = normalize(mat3(uModel) * aNormal);
    vUV = aUV;
    gl_Position = uProj * uView * wp;
}
)";

            // ==================== 片段着色器 ====================
            const juce::String fs = R"(#version 330 core

in vec3 vNormalWS;
in vec3 vPosWS;
in vec2 vUV;

uniform vec3 uLightDir;
uniform vec3 uLightColor;
uniform vec3 uAmbient;
uniform vec3 uSkyColor;
uniform vec3 uGroundColor;
uniform float uLightIntensity;
uniform vec3 uPlayerPos;
uniform vec3 uCamPos;

uniform vec3 uBaseColor;
uniform float uRoughness;
uniform float uMetallic;
uniform vec3 uEmissive;
uniform float uEmissiveStrength;
uniform float uSSS;
uniform float uSpecTint;

uniform float uShadeLevels;
uniform int uIsLine;

const int MAX_POINT_LIGHTS = 16;
uniform int uNumPointLights;
uniform vec3 uPointLightPos[MAX_POINT_LIGHTS];
uniform vec3 uPointLightColor[MAX_POINT_LIGHTS];
uniform float uPointLightQuadratic[MAX_POINT_LIGHTS];
uniform float uPointLightRange[MAX_POINT_LIGHTS];

uniform vec3 uFogColor;
uniform float uFogDensity;
uniform float uFogHeightFalloff;
uniform float uFogStart;

out vec4 fragColor;

const float PI = 3.14159265359;

float D_GGX(float NoH, float a)
{
    float a2 = a * a;
    float d = (NoH * NoH) * (a2 - 1.0) + 1.0;
    return a2 / max(PI * d * d, 1e-5);
}

float G_Smith(float NoV, float NoL, float a)
{
    float k = (a + 1.0); k = (k * k) * 0.125;
    float gv = NoV / (NoV * (1.0 - k) + k);
    float gl = NoL / (NoL * (1.0 - k) + k);
    return gv * gl;
}

vec3 F_Schlick(float VoH, vec3 F0)
{
    float f = pow(1.0 - VoH, 5.0);
    return F0 + (1.0 - F0) * f;
}

vec3 evalBRDF(vec3 N, vec3 V, vec3 L, vec3 baseColor,
              float roughness, float metallic, vec3 F0,
              vec3 radiance)
{
    vec3 H = normalize(V + L);
    float NoL = max(dot(N, L), 0.0);
    if (NoL <= 0.0) return vec3(0.0);
    float NoV = max(dot(N, V), 1e-4);
    float NoH = max(dot(N, H), 0.0);
    float VoH = max(dot(V, H), 0.0);
    float a = max(roughness * roughness, 0.002);
    float D = D_GGX(NoH, a);
    float G = G_Smith(NoV, NoL, a);
    vec3 F = F_Schlick(VoH, F0);
    vec3 spec = (D * G * F) / max(4.0 * NoL * NoV, 1e-4);
    vec3 kd = (1.0 - F) * (1.0 - metallic);
    vec3 diffuse = kd * baseColor / PI;
    return (diffuse + spec) * radiance * NoL;
}

void main()
{
    // ---- 线模式 ----
    if (uIsLine == 1)
    {
        fragColor = vec4(uBaseColor + uEmissive * uEmissiveStrength, 1.0);
        return;
    }

    vec3 N = normalize(vNormalWS);
    vec3 V = normalize(uCamPos - vPosWS);
    vec3 F0 = mix(vec3(0.04), uBaseColor, uMetallic);
    float NoV = max(dot(N, V), 1e-4);

    // ---- 1. 主方向光 ----
    vec3 Ldir = -normalize(uLightDir);
    vec3 dirRad = uLightColor * uLightIntensity;
    vec3 direct = evalBRDF(N, V, Ldir, uBaseColor, uRoughness, uMetallic, F0, dirRad);

    // ---- SSS（仅主方向光） ----
    float backLight = max(dot(-N, Ldir), 0.0);
    vec3 sssTerm = uSSS * backLight * uBaseColor * uLightColor * uLightIntensity * 0.5;

    // ---- 2. 点光源 ----
    vec3 pointSum = vec3(0.0);
    for (int i = 0; i < uNumPointLights; ++i)
    {
        vec3 toLight = uPointLightPos[i] - vPosWS;
        float dist = length(toLight);
        if (dist > uPointLightRange[i]) continue;
        vec3 L = toLight / max(dist, 1e-4);

        float atten = 1.0 / (1.0 + uPointLightQuadratic[i] * dist * dist);
        float edge = 1.0 - smoothstep(uPointLightRange[i] * 0.85,
                                      uPointLightRange[i], dist);
        vec3 radiance = uPointLightColor[i] * atten * edge;

        pointSum += evalBRDF(N, V, L, uBaseColor, uRoughness, uMetallic, F0, radiance);
    }

    // ---- 3. 半球环境光 ----
    float upDot = clamp(N.z * 0.5 + 0.5, 0.0, 1.0);
    vec3 hemi = mix(uGroundColor, uSkyColor, upDot);
    vec3 envIrradiance = uAmbient + hemi;
    vec3 ambientDiffuse = (1.0 - uMetallic) * uBaseColor * envIrradiance;
    vec3 F_env = F0 + (max(vec3(1.0 - uRoughness), F0) - F0)
                       * pow(1.0 - NoV, 5.0);
    vec3 ambientSpec = F_env * envIrradiance;
    vec3 ambientTerm = ambientDiffuse + ambientSpec;

    // ---- 4. 合并 ----
    vec3 col = direct + pointSum + sssTerm + ambientTerm
             + uEmissive * uEmissiveStrength;

    // ---- 5. 雾 ----
    if (uFogDensity > 0.0)
    {
        float dPl  = length(uPlayerPos.xy - vPosWS.xy);
        float dFog = max(dPl - uFogStart, 0.0);
        float heightK = exp(-max(vPosWS.z - uPlayerPos.z, 0.0) * uFogHeightFalloff);
        float fogAmt  = 1.0 - exp(-dFog * uFogDensity * heightK);
        col = mix(col, uFogColor, clamp(fogAmt, 0.0, 1.0));
    }

    // ---- 6. Tonemap + Gamma ----
    col = col / (col + vec3(1.0));
    col = pow(col, vec3(1.0 / 2.2));

    // ---- 7. 色阶量化 ----
    if (uShadeLevels > 1.5)
        col = floor(col * uShadeLevels) / uShadeLevels;

    fragColor = vec4(col, 1.0);
}
)";   // ← ✅ raw string 在此正确关闭

            // ==================== 编译链接 ====================
            if (!shader.build(vs, fs))
                return false;

            return true;
        }




        void use() noexcept { shader.use(); }

        //----------------------------------------------------------------------
        // 每帧一次
        //----------------------------------------------------------------------
        void setCameraMatrices(const Mat4& view, const Mat4& proj) noexcept
        {
            shader.setMat4("uView", view);
            shader.setMat4("uProj", proj);
        }
        void setCameraPos(const Vec3& p) noexcept
        {
            shader.setVec3("uCamPos", p);
        }

        // 新增 ↓
        void setPlayerPos(const Vec3& p) noexcept
        {
            shader.setVec3("uPlayerPos", p);
        }


        void setLighting(const Lighting& l) noexcept
        {
            shader.setVec3("uLightDir", l.direction);
            shader.setVec3("uLightColor", l.color);    // 你原本就当线性用
            shader.setVec3("uAmbient", l.ambient);
            shader.setVec3("uSkyColor", l.skyColor);
            shader.setVec3("uGroundColor", l.groundColor);
            shader.setFloat("uLightIntensity", l.intensity);
        }

        /** 上传点光源数组（已折算 linear color * intensity）。 */
        void setPointLights(const std::vector<PointLight>& lights,
            const Vec3& camPos) noexcept
        {
            // sRGB->linear 帮助函数（与 Renderer 同口径）
            auto srgbToLin = [](const Vec3& c) {
                auto t = [](float v) {
                    return v <= 0.04045f ? v / 12.92f
                        : std::pow((v + 0.055f) / 1.055f, 2.4f);
                    };
                return Vec3{ t(c.x), t(c.y), t(c.z) };
                };

            // 收集 enabled 的灯，并按"距相机距离"升序，取前 MAX 个
            struct Tmp { const PointLight* p; float d2; };
            std::vector<Tmp> tmps;
            tmps.reserve(lights.size());
            for (const auto& pl : lights)
            {
                if (!pl.enabled) continue;
                const float dx = pl.position.x - camPos.x;
                const float dy = pl.position.y - camPos.y;
                const float dz = pl.position.z - camPos.z;
                tmps.push_back({ &pl, dx * dx + dy * dy + dz * dz });
            }
            std::sort(tmps.begin(), tmps.end(),
                [](const Tmp& a, const Tmp& b) { return a.d2 < b.d2; });

            const int n = juce::jmin((int)tmps.size(), MAX_POINT_LIGHTS);
            shader.setInt("uNumPointLights", n);

            for (int i = n; i < MAX_POINT_LIGHTS; ++i)
            {
                const juce::String si = juce::String(i);
                shader.setVec3((juce::String("uPointLightColor[") + si + "]").toRawUTF8(), Vec3{ 0,0,0 });
                shader.setFloat((juce::String("uPointLightRange[") + si + "]").toRawUTF8(), 0.0f);
                shader.setFloat((juce::String("uPointLightQuadratic[") + si + "]").toRawUTF8(), 1.0f);
            }


        }

        /** 上传雾参数（fogColor 由 sRGB 转线性）。 */
        void setFog(const Lighting& l) noexcept
        {
            auto srgbToLin = [](float v) {
                return v <= 0.04045f ? v / 12.92f
                    : std::pow((v + 0.055f) / 1.055f, 2.4f);
                };
            Vec3 fogLin{ srgbToLin(l.fogColorSRGB.x),
                         srgbToLin(l.fogColorSRGB.y),
                         srgbToLin(l.fogColorSRGB.z) };
            shader.setVec3("uFogColor", fogLin);
            shader.setFloat("uFogDensity", l.fogDensity);
            shader.setFloat("uFogHeightFalloff", l.fogHeightFalloff);
            shader.setFloat("uFogStart", l.fogStart);
        }


        void setShadeLevels(float levels) noexcept { shader.setFloat("uShadeLevels", levels); }

        //----------------------------------------------------------------------
        // 每个 draw call
        //----------------------------------------------------------------------
        void setModel(const Mat4& m) noexcept { shader.setMat4("uModel", m); }

        /** 新主路径：打入完整材质（线性颜色）。 */
        void setMaterial(const Material& mat, const Vec3& baseLin, const Vec3& emissiveLin) noexcept
        {
            shader.setVec3("uBaseColor", baseLin);
            shader.setFloat("uRoughness", mat.roughness);
            shader.setFloat("uMetallic", mat.metallic);
            shader.setVec3("uEmissive", emissiveLin);
            shader.setFloat("uEmissiveStrength", mat.emissiveStrength);
            shader.setFloat("uSSS", mat.sss);
            shader.setFloat("uSpecTint", mat.specTint);
        }

        /** 兼容旧路径：仅颜色 + 自发光（线性颜色）。 */
        void setBaseColorLegacy(const Vec3& baseLin, const Vec3& emissiveLin) noexcept
        {
            shader.setVec3("uBaseColor", baseLin);
            shader.setFloat("uRoughness", 0.9f);
            shader.setFloat("uMetallic", 0.0f);
            shader.setVec3("uEmissive", emissiveLin);
            shader.setFloat("uEmissiveStrength", 0.0f);   // ← 关掉
            shader.setFloat("uSSS", 0.0f);
            shader.setFloat("uSpecTint", 0.0f);
        }


        /** 切换“线模式”：网格线不参与光照。 */
        void setLineMode(bool on) noexcept { shader.setInt("uIsLine", on ? 1 : 0); }

    private:
        Shader shader;
    };
} // namespace sc
