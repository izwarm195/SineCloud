// ==============================================================================
// DenoisePass.h
// Layer 5: Block-level noise reduction ¡ª operates after pixelate, before motion blur
// ------------------------------------------------------------------------------
// Samples neighbouring blocks at pixelSize stride using bilateral luminance
// weights. Preserves pixel-block edges because comparisons are block-to-block.
// ==============================================================================
#pragma once
#include "GLUtils.h"
#include "Shader.h"

namespace sc {

    class DenoisePass {
    public:
        explicit DenoisePass(juce::OpenGLContext& ctx)
            : context(ctx), shader(ctx) {
        }

        bool build() {
            const juce::String quadVS = R"(#version 330 core
layout(location = 0) in vec2 aPos;
out vec2 vUV;
void main() {
    vUV = aPos * 0.5 + 0.5;
    gl_Position = vec4(aPos, 0.0, 1.0);
})";

            const juce::String fs = R"(#version 330 core
in vec2 vUV;
uniform sampler2D uSrc;
uniform float uPixelSize;        // block stride (same as pixelate)
uniform float uDenoiseStrength;  // 0 = off, 1 = full
uniform float uColorSigma;       // luminance similarity sigma
out vec4 fragColor;

float luminance(vec3 c) {
    return dot(c, vec3(0.2126, 0.7152, 0.0722));
}

void main() {
    ivec2 texSize = textureSize(uSrc, 0);
    float gridX = float(texSize.x) / uPixelSize;
    float gridY = float(texSize.y) / uPixelSize;

    // ---- snap to block centre (identical to pixelate) ----
    vec2 blockUV = floor(vUV * vec2(gridX, gridY)) / vec2(gridX, gridY)
                 + vec2(0.5 / gridX, 0.5 / gridY);
    ivec2 blockCoord = ivec2(blockUV * vec2(texSize));

    vec3 centerCol = texelFetch(uSrc, blockCoord, 0).rgb;
    float centerLum = luminance(centerCol);

    int step = int(uPixelSize);

    // ---- 3¡Á3 block neighbourhood ----
    vec3 sum   = centerCol;
    float totalW = 1.0;

    for (int y = -1; y <= 1; ++y) {
        for (int x = -1; x <= 1; ++x) {
            if (x == 0 && y == 0) continue;

            ivec2 nCoord = clamp(
                blockCoord + ivec2(x, y) * step,
                ivec2(0), texSize - 1);
            vec3 nCol = texelFetch(uSrc, nCoord, 0).rgb;

            // bilateral weight on luminance distance
            float lumDist = abs(luminance(nCol) - centerLum);
            float w = exp(-lumDist * lumDist
                          / (2.0 * uColorSigma * uColorSigma));

            sum    += nCol * w;
            totalW += w;
        }
    }

    vec3 denoised = sum / totalW;
    vec3 result = mix(centerCol, denoised, uDenoiseStrength);
    fragColor = vec4(result, 1.0);
})";

            if (!shader.build(quadVS, fs)) {
                DBG("DenoisePass: shader build failed");
                return false;
            }
            return true;
        }

        void shutdown() {
            shader.raw().release();
        }

        void render(GLuint srcTex,
            GLuint fullscreenVAO,
            int viewW, int viewH,
            float pixelSize,
            float denoiseStrength,
            float colorSigma,
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
            shader.setFloat("uDenoiseStrength",
                juce::jlimit(0.0f, 1.0f, denoiseStrength));
            shader.setFloat("uColorSigma",
                juce::jmax(0.001f, colorSigma));

            glBindVertexArray(fullscreenVAO);
            glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
            glBindVertexArray(0);

            glDepthMask(GL_TRUE);
            glEnable(GL_DEPTH_TEST);
        }

    private:
        juce::OpenGLContext& context;
        Shader shader;
        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DenoisePass)
    };

} // namespace sc
