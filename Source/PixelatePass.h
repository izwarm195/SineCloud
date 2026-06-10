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

            // ★ Red Giraffe 风格：UV snap + 可选的 full color quantization
            const juce::String fs = R"(#version 330 core

in vec2 vUV;

uniform sampler2D uSrc;
uniform float   uPixelSize;       // 像素块大小（屏幕像素单位，如 2.0 = 2×2 pixel art block）
uniform float   uColorLevels;     // RGB 独立色阶数（0 = 不做色阶量化）
uniform bool    uUseColorQuant;   // true: RGB 独立量化 / false: 沿用 luminance posterize

out vec4 fragColor;

void main()
{
    // ---- 1) 采样分辨率 ----
    ivec2 texSize = textureSize(uSrc, 0);

    // ---- 2) Red Giraffe UV snap：锁到像素块几何中心 ----
    //      floor(vUV * grid) / grid    →    块左上角
    //      + 0.5 / grid                →    块正中心（避免子像素偏差）
    float gridX = float(texSize.x) / uPixelSize;
    float gridY = float(texSize.y) / uPixelSize;
    vec2  pixelUV = floor(vUV * vec2(gridX, gridY)) / vec2(gridX, gridY)
                  + vec2(0.5 / gridX, 0.5 / gridY);

    vec3 col = texture(uSrc, pixelUV).rgb;

    // ---- 3) 色阶量化 ----
    if (uColorLevels > 1.5)
    {
        if (uUseColorQuant)
        {
            // RGB 三个通道独立量化 — 更接近像素艺术调色板限制
            col.r = (floor(col.r * uColorLevels) + 0.5) / uColorLevels;
            col.g = (floor(col.g * uColorLevels) + 0.5) / uColorLevels;
            col.b = (floor(col.b * uColorLevels) + 0.5) / uColorLevels;
        }
        else
        {
            // 基于亮度的量化（与现有 shadeLevels 逻辑一致，避免 hue shift）
            float Y = max(dot(col, vec3(0.2126, 0.7152, 0.0722)), 1e-5);
            float Yq = (floor(Y * uColorLevels) + 0.5) / uColorLevels;
            col = col * (Yq / Y);
        }
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
        void render(GLuint   srcTex,        // 输入纹理（已 composite 好的 sRGB）
            GLuint   fullscreenVAO, // PostPipeline 的 quad VAO
            int      viewW,
            int      viewH,
            float    pixelSize,     // e.g. 2.0 ~ 8.0
            float    colorLevels,   // e.g. 16.0
            bool     useColorQuant, // true: RGB独立 / false: 亮度posterize
            GLuint   targetFBO = 0) noexcept
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
            shader.setFloat("uColorLevels", juce::jmax(1.0f, colorLevels));
            shader.setInt("uUseColorQuant", useColorQuant ? 1 : 0);

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
