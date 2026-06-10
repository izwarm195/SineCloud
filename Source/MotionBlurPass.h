// ==============================================================================
// MotionBlurPass.h
// Layer 5: Motion Blur
// 沿 GBuffer RT3 velocity 方向做对称 N-tap 采样，适配俯视像素风
// ==============================================================================
#pragma once

#include "GLUtils.h"
#include "Shader.h"

namespace sc {

    class MotionBlurPass
    {
    public:
        static constexpr int MAX_SAMPLES = 32;

        explicit MotionBlurPass(juce::OpenGLContext& ctx)
            : context(ctx), shader(ctx) {
        }

        // ----------------------------------------------------------------
        // 构建
        // ----------------------------------------------------------------
        bool build()
        {
            const juce::String quadVS = R"(#version 330 core
layout(location = 0) in vec2 aPos;
out vec2 vUV;
void main() {
    vUV = aPos * 0.5 + 0.5;
    gl_Position = vec4(aPos, 0.0, 1.0);
})";

            const juce::String fs = R"(#version 330 core
in vec2 vUV;
uniform sampler2D uScene;        // HDR 场景（bloom composite 后）
uniform sampler2D uVelocity;     // GBuffer RT3: velocity.xy
uniform int   uSamples;          // 采样数 (2 ~ 32)
uniform float uIntensity;        // 混合强度 [0, 1]
out vec4 fragColor;

void main() {
    vec3 scene = texture(uScene, vUV).rgb;
    vec2 vel  = texture(uVelocity, vUV).rg;

    vec3 accum      = scene;
    float totalW    = 1.0;
    int   samples   = max(uSamples, 2);

    // 沿速度方向对称采样
    for (int i = 1; i <= samples / 2; ++i)
    {
        float t     = float(i) / float(samples / 2);
        vec2  off   = vel * t * uIntensity;

        accum += texture(uScene, vUV + off).rgb;
        accum += texture(uScene, vUV - off).rgb;
        totalW += 2.0;
    }

    vec3 blurred = accum / totalW;

    // 根据速度大小自适应混合：静止 → 原场景，快速移动 → 模糊结果
    float blend  = clamp(length(vel) * uIntensity * 40.0, 0.0, 1.0);
    vec3  result = mix(scene, blurred, blend);

    fragColor = vec4(result, 1.0);
})";

            if (!shader.build(quadVS, fs))
            {
                DBG("MotionBlurPass: shader build failed");
                return false;
            }
            return true;
        }

        void shutdown()
        {
            shader.raw().release();
        }

        // ----------------------------------------------------------------
        // 每帧渲染
        // ----------------------------------------------------------------
        void render(GLuint sceneTex,
            GLuint velocityTex,
            GLuint fullscreenVAO,
            int    w, int h,
            float  intensity = 0.5f,
            int    samples = 16) noexcept
        {
            using namespace sc::gl;

            glDisable(GL_DEPTH_TEST);
            glDepthMask(GL_FALSE);
            glDisable(GL_BLEND);

            glBindVertexArray(fullscreenVAO);
            glViewport(0, 0, w, h);

            shader.use();

            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, sceneTex);
            shader.setInt("uScene", 0);

            glActiveTexture(GL_TEXTURE1);
            glBindTexture(GL_TEXTURE_2D, velocityTex);
            shader.setInt("uVelocity", 1);

            shader.setInt("uSamples", juce::jmin(samples, MAX_SAMPLES));
            shader.setFloat("uIntensity", juce::jlimit(0.0f, 1.0f, intensity));

            glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

            glBindVertexArray(0);
            glDepthMask(GL_TRUE);
            glEnable(GL_DEPTH_TEST);
        }

    private:
        juce::OpenGLContext& context;
        Shader shader;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MotionBlurPass)
    };

} // namespace sc
