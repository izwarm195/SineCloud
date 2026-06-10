// ==============================================================================
// PixelatePass.h
// Layer 4: Post-composite spatial pixelation (Red Giraffe style)
// ------------------------------------------------------------------------------
// 在全分辨率帧缓冲上做 UV snap → 产生 crisp blocky pixels
// 配合已有的 shadeLevels 亮度 posterization，形成完整的像素艺术后处理
// ==============================================================================

#pragma once

#include "GLUtils.h"
#include "Shader.h"

namespace sc {

    class PixelatePass
    {
    public:
        explicit PixelatePass(juce::OpenGLContext& ctx)
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

            //UV snap + 可选的 full color quantization

            const juce::String fs = R"(#version 330 core

in vec2 vUV;

uniform sampler2D uSrc;
uniform float   uPixelSize;
uniform float   uEdgeBoost;  // ★ 边缘对比度强度 [0, 1]，0=关闭

out vec4 fragColor;

void main()
{
    ivec2 texSize = textureSize(uSrc, 0);
    float gridX = float(texSize.x) / uPixelSize;
    float gridY = float(texSize.y) / uPixelSize;

    // ---- 1) UV snap 到像素块几何中心 ----
    vec2 blockUV = floor(vUV * vec2(gridX, gridY)) / vec2(gridX, gridY)
                 + vec2(0.5 / gridX, 0.5 / gridY);

    // ---- 2) ★ texelFetch 硬采样：绕过所有 texture filter ----
    ivec2 coord = ivec2(blockUV * vec2(texSize));
    vec3 col = texelFetch(uSrc, coord, 0).rgb;

    // ---- 3) ★ 边缘局部对比度增强（不改色阶，只调对比度） ----
    if (uEdgeBoost > 0.0)
    {
        int step = int(uPixelSize);           // 步长 = 像素块大小
        vec3 n  = texelFetch(uSrc, coord + ivec2(0, -step), 0).rgb;
        vec3 s  = texelFetch(uSrc, coord + ivec2(0,  step), 0).rgb;
        vec3 e  = texelFetch(uSrc, coord + ivec2( step, 0), 0).rgb;
        vec3 w  = texelFetch(uSrc, coord + ivec2(-step, 0), 0).rgb;

        // 4 邻域平均
        vec3 avg = (n + s + e + w) * 0.25;

        // 当前块与邻域的差异 → 边缘强度
        float edge = length(col - avg);

        // 往远离邻域均值的方向推 → 边缘更跳
        // boost ∈ [1.0, 1.0+uEdgeBoost]，edge 越大 boost 越强
        float boost = 1.0 + uEdgeBoost * edge * 3.0;
        col = clamp(mix(col, col * boost, edge), 0.0, 1.0);
    }

    fragColor = vec4(col, 1.0);
})";



            if (!shader.build(quadVS, fs))
            {
                DBG("PixelatePass: shader build failed");
                return false;
            }
            return true;
        }

        void shutdown()
        {
            shader.raw().release();
        }

        // ----------------------------------------------------------------
        // 每帧调用
        // ----------------------------------------------------------------
        void render(GLuint srcTex,
            GLuint fullscreenVAO,
            int viewW,
            int viewH,
            float pixelSize,
            float edgeBoost,      // ★ 替代原来的 colorLevels + useColorQuant
            GLuint targetFBO = 0) noexcept

        {
            using namespace sc::gl;

            glDisable(GL_DEPTH_TEST);
            glDepthMask(GL_FALSE);
            glDisable(GL_BLEND);

            glBindFramebuffer(GL_FRAMEBUFFER, targetFBO);
            glViewport(0, 0, viewW, viewH);

            shader.use();

            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, srcTex);
            shader.setInt("uSrc", 0);

            shader.setFloat("uPixelSize", juce::jmax(1.0f, pixelSize));
            shader.setFloat("uEdgeBoost", juce::jmax(0.0f, juce::jmin(1.0f, edgeBoost)));
            // 删除原有 uColorLevels / uUseColorQuant 的 setFloat/setInt


            glBindVertexArray(fullscreenVAO);
            glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
            glBindVertexArray(0);

            glDepthMask(GL_TRUE);
            glEnable(GL_DEPTH_TEST);
        }

    private:
        juce::OpenGLContext& context;
        Shader shader;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PixelatePass)
    };

} // namespace sc
