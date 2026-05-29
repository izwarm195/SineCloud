/*
  ==============================================================================
    PixelMaterial.h
    Layer 2: Scene & Renderer
    像素风共享 material：方向光 toon + 色阶量化。
    所有 3D 物体（地砖、玩家、旋钮、Boss）都用这一份 shader。
  ==============================================================================
*/
#pragma once

#include <JuceHeader.h>
#include "Shader.h"
#include "Lighting.h"

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

                void main() {
                    vec4 wp = uModel * vec4(aPos, 1.0);
                    vPosWS = wp.xyz;
                    // 假设 model 没有非均匀缩放（box/cylinder factory 都是均匀缩放）
                    vNormalWS = mat3(uModel) * aNormal;
                    vUV = aUV;
                    gl_Position = uProj * uView * wp;
                }
            )";

            const juce::String fs = R"(#version 330 core
                in vec3 vNormalWS;
                in vec3 vPosWS;
                in vec2 vUV;

                uniform vec3  uLightDir;
                uniform vec3  uLightColor;
                uniform vec3  uAmbient;
                uniform float uLightIntensity;

                uniform vec3  uBaseColor;
                uniform vec3  uEmissive;
                uniform float uShadeLevels; // 量化档数；<=1 时关闭量化

                uniform int   uIsLine;      // 网格线模式：忽略光照

                out vec4 fragColor;

                void main() {
                    if (uIsLine == 1) {
                        fragColor = vec4(uBaseColor + uEmissive, 1.0);
                        return;
                    }

                    vec3 N = normalize(vNormalWS);
                    vec3 L = -normalize(uLightDir);
                    float lambert = max(dot(N, L), 0.0);

                    if (uShadeLevels > 1.5) {
                        lambert = floor(lambert * uShadeLevels) / uShadeLevels;
                    }

                    vec3 lit = uAmbient + uLightColor * uLightIntensity * lambert;
                    vec3 col = uBaseColor * lit + uEmissive;

                    if (uShadeLevels > 1.5) {
                        // 颜色色阶量化（更"像素"）
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

        void setLighting(const Lighting& l) noexcept
        {
            shader.setVec3("uLightDir", l.direction);
            shader.setVec3("uLightColor", l.color);
            shader.setVec3("uAmbient", l.ambient);
            shader.setFloat("uLightIntensity", l.intensity);
        }

        void setShadeLevels(float levels) noexcept { shader.setFloat("uShadeLevels", levels); }

        //----------------------------------------------------------------------
        // 每个 draw call
        //----------------------------------------------------------------------
        void setModel(const Mat4& m) noexcept { shader.setMat4("uModel", m); }

        void setBaseColor(const Vec3& c) noexcept { shader.setVec3("uBaseColor", c); }
        void setEmissive(const Vec3& c) noexcept { shader.setVec3("uEmissive", c); }

        /** 切换"线模式"：网格线不参与光照。 */
        void setLineMode(bool on) noexcept { shader.setInt("uIsLine", on ? 1 : 0); }

    private:
        Shader shader;
    };
}
