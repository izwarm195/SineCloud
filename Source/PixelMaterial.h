/* ==============================================================================
   PixelMaterial.h
   Layer 3: Deferred Rendering
   精简为纯前向 shader，仅用于线框/调试等不需要走 G-Buffer 的场景。
   主体渲染已迁移至 GBuffer + PostPipeline。
   ============================================================================== */
#pragma once
#include "Shader.h"
#include "Lighting.h"
#include "Material.h"

namespace sc {

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
            vPosWS    = wp.xyz;
            vNormalWS = normalize(mat3(uModel) * aNormal);
            vUV       = aUV;
            gl_Position = uProj * uView * wp;
        })";

            const juce::String fs = R"(#version 330 core
        in vec3 vNormalWS;
        in vec3 vPosWS;
        in vec2 vUV;

        uniform vec3  uBaseColor;
        uniform vec3  uEmissive;
        uniform float uEmissiveStrength;
        uniform int   uIsLine;

        out vec4 fragColor;

        void main()
        {
            vec3 col = uBaseColor + uEmissive * uEmissiveStrength;
            // 简易 gamma
            col = pow(col, vec3(1.0 / 2.2));
            fragColor = vec4(col, 1.0);
        })";

            return shader.build(vs, fs);
        }

        void use() noexcept { shader.use(); }

        void setCameraMatrices(const Mat4& view, const Mat4& proj) noexcept
        {
            shader.setMat4("uView", view);
            shader.setMat4("uProj", proj);
        }

        void setModel(const Mat4& m) noexcept { shader.setMat4("uModel", m); }

        void setBaseColorLegacy(const Vec3& baseLin, const Vec3& emissiveLin) noexcept
        {
            shader.setVec3("uBaseColor", baseLin);
            shader.setVec3("uEmissive", emissiveLin);
            shader.setFloat("uEmissiveStrength", 1.0f);
        }

        void setLineMode(bool on) noexcept { shader.setInt("uIsLine", on ? 1 : 0); }

    private:
        Shader shader;
    };

} // namespace sc
