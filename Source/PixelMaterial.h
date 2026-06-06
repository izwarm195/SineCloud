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
        // NOTE: assumes uniform scale; non-uniform scale needs normal matrix
        vNormalWS = mat3(uModel) * aNormal;
        vUV = aUV;
        gl_Position = uProj * uView * wp;
    }
)";

            const juce::String fs = R"(#version 330 core
    in vec3 vNormalWS;
    in vec3 vPosWS;
    in vec2 vUV;

    // Main directional light + hemisphere ambient
    uniform vec3  uLightDir;
    uniform vec3  uLightColor;
    uniform vec3  uAmbient;
    uniform vec3  uSkyColor;
    uniform vec3  uGroundColor;
    uniform float uLightIntensity;

    // Camera position (world space)
    uniform vec3  uCamPos;

    // Material properties (linear space)
    uniform vec3  uBaseColor;
    uniform float uRoughness;
    uniform float uMetallic;
    uniform vec3  uEmissive;
    uniform float uEmissiveStrength;
    uniform float uSSS;
    uniform float uSpecTint;

    // Stylization
    uniform float uShadeLevels; // <=1.5 disables banding
    uniform int   uIsLine;

    out vec4 fragColor;

    const float PI = 3.14159265359;

    // ---- Simplified PBR with GGX / Schlick ----
    float D_GGX(float NoH, float a)
    {
        float a2 = a * a;
        float d  = (NoH * NoH) * (a2 - 1.0) + 1.0;
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

    void main()
{
    if (uIsLine == 1)
    {
        fragColor = vec4(uBaseColor + uEmissive * uEmissiveStrength, 1.0);
        return;
    }

    vec3 N = normalize(vNormalWS);
    vec3 V = normalize(uCamPos - vPosWS);
    vec3 L = -normalize(uLightDir);   // direction = 光线传播方向
    vec3 H = normalize(V + L);

    float NoL = max(dot(N, L), 0.0);
    float NoV = max(dot(N, V), 1e-4);
    float NoH = max(dot(N, H), 0.0);
    float VoH = max(dot(V, H), 0.0);

    // ★ F0：介电用 0.04（白），金属用 baseColor
    vec3 F0_dielectric = vec3(0.04);
    vec3 F0 = mix(F0_dielectric, uBaseColor, uMetallic);

    float a = max(uRoughness * uRoughness, 0.002);
    float D = D_GGX(NoH, a);
    float G = G_Smith(NoV, NoL, a);
    vec3  F = F_Schlick(VoH, F0);

    // 高光项（GGX BRDF）
    vec3 spec = (D * G * F) / max(4.0 * NoL * NoV, 1e-4);

    // 漫反射：金属无漫反射；能量守恒用 (1-F) 控制；除以 PI
    vec3 kd = (1.0 - F) * (1.0 - uMetallic);
    vec3 diffuse = kd * uBaseColor / PI;

    // ★ 直接光：辐照度 = lightColor * intensity * NoL
    vec3 radiance = uLightColor * uLightIntensity * NoL;
    vec3 direct = (diffuse + spec) * radiance;

    // SSS：背光透出
    float backLight = max(dot(-N, L), 0.0);
    vec3 sssTerm = uSSS * backLight * uBaseColor * uLightColor * uLightIntensity * 0.5;

    // ★ 环境光：分成 diffuse + spec 两部分，各自能量守恒
    float upDot = clamp(N.z * 0.5 + 0.5, 0.0, 1.0);
    vec3 hemi = mix(uGroundColor, uSkyColor, upDot);
    vec3 envIrradiance = uAmbient + hemi;

    // 环境漫反射（被金属抑制）
    vec3 ambientDiffuse = (1.0 - uMetallic) * uBaseColor * envIrradiance;

    // ★ 环境高光：用 grazing-angle Fresnel + roughness 衰减给金属/光滑面一个"周围环境的亮度"
    vec3 F_env = F0 + (max(vec3(1.0 - uRoughness), F0) - F0) * pow(1.0 - NoV, 5.0);
    vec3 ambientSpec = F_env * envIrradiance;   // 简化：用同一个 envIrradiance 当反射环境

    vec3 ambientTerm = ambientDiffuse + ambientSpec;

    vec3 col = direct + sssTerm + ambientTerm + uEmissive * uEmissiveStrength;

    // ★ 提一点曝光，避免 Reinhard 过早压暗
    col *= 1.5;

    // Reinhard tonemap
    col = col / (col + vec3(1.0));
    // Linear to sRGB gamma
    col = pow(col, vec3(1.0 / 2.2));

    if (uShadeLevels > 1.5)
    {
        col = floor(col * uShadeLevels) / uShadeLevels;
    }

    fragColor = vec4(col, 1.0);
}



)";
            return shader.build(vs, fs);
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
        void setCameraPos(const Vec3& p) noexcept { shader.setVec3("uCamPos", p); }

        void setLighting(const Lighting& l) noexcept
        {
            shader.setVec3("uLightDir", l.direction);
            shader.setVec3("uLightColor", l.color);
            shader.setVec3("uAmbient", l.ambient);
            shader.setVec3("uSkyColor", l.skyColor);
            shader.setVec3("uGroundColor", l.groundColor);
            shader.setFloat("uLightIntensity", l.intensity);
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
            shader.setFloat("uEmissiveStrength", (emissiveLin.x + emissiveLin.y + emissiveLin.z) > 0 ? 1.0f : 0.0f);
            shader.setFloat("uSSS", 0.0f);
            shader.setFloat("uSpecTint", 0.0f);
        }

        /** 切换“线模式”：网格线不参与光照。 */
        void setLineMode(bool on) noexcept { shader.setInt("uIsLine", on ? 1 : 0); }

    private:
        Shader shader;
    };
} // namespace sc
