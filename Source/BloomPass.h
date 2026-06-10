// ==============================================================================
//   BloomPass.h
//   Layer 4: Bloom + Tonemap
//   6 级降采样 → 逐级上采样叠加 → ACES + Gamma composite 到默认 FBO
// ==============================================================================
#pragma once

#include "GLUtils.h"
#include "Shader.h"

namespace sc {

    class BloomPass
    {
    public:
        static constexpr int MIP_LEVELS = 6;

        explicit BloomPass(juce::OpenGLContext& ctx)
            : context(ctx), downShader(ctx), upShader(ctx), compositeShader(ctx) {
        }

        // ----------------------------------------------------------------
        // 构建
        // ----------------------------------------------------------------
        bool build()
        {
            // 全屏 quad 顶点着色器（与 PostPipeline 一致）
            const juce::String quadVS = R"(#version 330 core
layout(location = 0) in vec2 aPos;
out vec2 vUV;
void main() {
    vUV = aPos * 0.5 + 0.5;
    gl_Position = vec4(aPos, 0.0, 1.0);
})";

            // ---- 降采样 + Bright Extract (13-tap) ----
            const juce::String downFS = R"(#version 330 core
in vec2 vUV;
uniform sampler2D uSrc;
uniform vec2 uTexel;
uniform int  uIsFirstPass;
uniform float uThreshold;
uniform float uSoftKnee;
out vec3 fragColor;

vec3 prefilter(vec3 c) {
    float br = max(c.r, max(c.g, c.b));
    float knee = uThreshold * uSoftKnee + 1e-5;
    float soft = clamp(br - uThreshold + knee, 0.0, 2.0 * knee);
    soft = soft * soft / (4.0 * knee + 1e-5);
    float contrib = max(soft, br - uThreshold) / max(br, 1e-5);
    return c * contrib;
}

void main() {
    vec2 t = uTexel;
    vec3 a = texture(uSrc, vUV + t * vec2(-2,-2)).rgb;
    vec3 b = texture(uSrc, vUV + t * vec2( 0,-2)).rgb;
    vec3 c = texture(uSrc, vUV + t * vec2( 2,-2)).rgb;
    vec3 d = texture(uSrc, vUV + t * vec2(-2, 0)).rgb;
    vec3 e = texture(uSrc, vUV                  ).rgb;
    vec3 f = texture(uSrc, vUV + t * vec2( 2, 0)).rgb;
    vec3 g = texture(uSrc, vUV + t * vec2(-2, 2)).rgb;
    vec3 h = texture(uSrc, vUV + t * vec2( 0, 2)).rgb;
    vec3 i = texture(uSrc, vUV + t * vec2( 2, 2)).rgb;
    vec3 j = texture(uSrc, vUV + t * vec2(-1,-1)).rgb;
    vec3 k = texture(uSrc, vUV + t * vec2( 1,-1)).rgb;
    vec3 l = texture(uSrc, vUV + t * vec2(-1, 1)).rgb;
    vec3 m = texture(uSrc, vUV + t * vec2( 1, 1)).rgb;
    vec3 col = e * 0.125
             + (a + c + g + i) * 0.03125
             + (b + d + f + h) * 0.0625
             + (j + k + l + m) * 0.125;
    if (uIsFirstPass == 1) col = prefilter(col);
    fragColor = max(col, vec3(0.0));
})";

            // ---- 上采样 (9-tap tent) ----
            const juce::String upFS = R"(#version 330 core
in vec2 vUV;
uniform sampler2D uSrc;
uniform vec2 uTexel;
uniform float uFilterRadius;
out vec3 fragColor;

void main() {
    float x = uFilterRadius * uTexel.x;
    float y = uFilterRadius * uTexel.y;
    vec3 a = texture(uSrc, vUV + vec2(-x,-y)).rgb;
    vec3 b = texture(uSrc, vUV + vec2( 0,-y)).rgb;
    vec3 c = texture(uSrc, vUV + vec2( x,-y)).rgb;
    vec3 d = texture(uSrc, vUV + vec2(-x, 0)).rgb;
    vec3 e = texture(uSrc, vUV               ).rgb;
    vec3 f = texture(uSrc, vUV + vec2( x, 0)).rgb;
    vec3 g = texture(uSrc, vUV + vec2(-x, y)).rgb;
    vec3 h = texture(uSrc, vUV + vec2( 0, y)).rgb;
    vec3 i = texture(uSrc, vUV + vec2( x, y)).rgb;
    fragColor = (e * 4.0 + (b + d + f + h) * 2.0 + (a + c + g + i)) * (1.0 / 16.0);
})";

            // ---- Composite: ACES + Gamma + Posterize ----
            const juce::String compositeFS = R"(#version 330 core
in vec2 vUV;
uniform sampler2D uScene;
uniform sampler2D uBloom;
uniform float uBloomStrength;
uniform float uShadeLevels;
out vec4 fragColor;

vec3 aces(vec3 x) {
    const float a = 2.51, b = 0.03, c = 2.43, d = 0.59, e = 0.14;
    return clamp((x*(a*x+b)) / (x*(c*x+d)+e), 0.0, 1.0);
}

void main() {
    vec3 scene = texture(uScene, vUV).rgb;
    vec3 bloom = texture(uBloom, vUV).rgb;
    vec3 col = scene + bloom * uBloomStrength;
    col = aces(col);
    col = pow(col, vec3(1.0 / 2.2));
    if (uShadeLevels > 1.5) {
        float Y = max(dot(col, vec3(0.2126, 0.7152, 0.0722)), 1e-5);
        float Yq = (floor(Y * uShadeLevels) + 0.5) / uShadeLevels;
        col = col * (Yq / Y);
    }
    fragColor = vec4(col, 1.0);
})";

            if (!downShader.build(quadVS, downFS)) { DBG("BloomPass: downShader build failed"); return false; }
            if (!upShader.build(quadVS, upFS)) { DBG("BloomPass: upShader build failed"); return false; }
            if (!compositeShader.build(quadVS, compositeFS)) { DBG("BloomPass: compositeShader build failed"); return false; }
            return true;
        }

        void shutdown()
        {
            releaseTextures();
            downShader.raw().release();
            upShader.raw().release();
            compositeShader.raw().release();
        }

        void ensureSize(int newW, int newH)
        {
            if (newW == w && newH == h) return;
            w = newW; h = newH;
            releaseTextures();
            for (int i = 0; i < MIP_LEVELS; ++i)
            {
                mw[i] = juce::jmax(1, w >> (i + 1));
                mh[i] = juce::jmax(1, h >> (i + 1));
                mipTex[i] = makeTex2D(mw[i], mh[i], sc::gl::GL_RGBA16F,
                    sc::gl::GL_RGBA, sc::gl::GL_FLOAT);
                mipFBO[i] = makeFBO(mipTex[i]);
            }
        }

        // ----------------------------------------------------------------
        // 每帧渲染
        // ----------------------------------------------------------------
        void render(GLuint sceneHDRTex, GLuint fullscreenVAO,
            float threshold, float softKnee, float strength,
            float filterRadius, float shadeLevels,
            GLuint targetFBO = 0) noexcept
        {
            using namespace sc::gl;

            glDisable(GL_DEPTH_TEST);
            glDepthMask(GL_FALSE);
            glDisable(GL_BLEND);
            glBindFramebuffer(GL_FRAMEBUFFER, targetFBO);
            glBindVertexArray(fullscreenVAO);


            // ---- 6 级降采样 ----
            downShader.use();
            downShader.setFloat("uThreshold", threshold);
            downShader.setFloat("uSoftKnee", softKnee);
            for (int i = 0; i < MIP_LEVELS; ++i)
            {
                glBindFramebuffer(GL_FRAMEBUFFER, mipFBO[i]);
                glViewport(0, 0, mw[i], mh[i]);

                GLuint srcTex = (i == 0) ? sceneHDRTex : mipTex[i - 1];
                int    srcW = (i == 0) ? w : mw[i - 1];
                int    srcH = (i == 0) ? h : mh[i - 1];

                glActiveTexture(GL_TEXTURE0);
                glBindTexture(GL_TEXTURE_2D, srcTex);
                downShader.setInt("uSrc", 0);
                downShader.setVec2("uTexel", 1.0f / (float)srcW, 1.0f / (float)srcH);
                downShader.setInt("uIsFirstPass", (i == 0) ? 1 : 0);

                glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
            }

            // ---- 逐级上采样 + additive blend ----
            glEnable(GL_BLEND);
            glBlendFunc(GL_ONE, GL_ONE);
            upShader.use();
            upShader.setFloat("uFilterRadius", filterRadius);
            for (int i = MIP_LEVELS - 1; i > 0; --i)
            {
                glBindFramebuffer(GL_FRAMEBUFFER, mipFBO[i - 1]);
                glViewport(0, 0, mw[i - 1], mh[i - 1]);

                glActiveTexture(GL_TEXTURE0);
                glBindTexture(GL_TEXTURE_2D, mipTex[i]);
                upShader.setInt("uSrc", 0);
                upShader.setVec2("uTexel", 1.0f / (float)mw[i], 1.0f / (float)mh[i]);

                glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
            }
            glDisable(GL_BLEND);

            // ---- Composite 到默认 framebuffer ----
            glBindFramebuffer(GL_FRAMEBUFFER, targetFBO);
            glViewport(0, 0, w, h);

            compositeShader.use();
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, sceneHDRTex);
            compositeShader.setInt("uScene", 0);
            glActiveTexture(GL_TEXTURE1);
            glBindTexture(GL_TEXTURE_2D, mipTex[0]);  // mip0 已累积所有上采样
            compositeShader.setInt("uBloom", 1);
            compositeShader.setFloat("uBloomStrength", strength);
            compositeShader.setFloat("uShadeLevels", shadeLevels);

            glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

            glBindVertexArray(0);
            glDepthMask(GL_TRUE);
            glEnable(GL_DEPTH_TEST);
        }

    private:
        // ---- 复用 GBuffer 的贴图创建模式 ----
        static GLuint makeTex2D(int w, int h, GLint internalFmt, GLenum fmt, GLenum type)
        {
            GLuint t = 0;
            sc::gl::glGenTextures(1, &t);
            sc::gl::glBindTexture(sc::gl::GL_TEXTURE_2D, t);
            sc::gl::glTexImage2D(sc::gl::GL_TEXTURE_2D, 0, internalFmt, w, h, 0, fmt, type, nullptr);
            sc::gl::glTexParameteri(sc::gl::GL_TEXTURE_2D, sc::gl::GL_TEXTURE_MIN_FILTER, sc::gl::GL_LINEAR);
            sc::gl::glTexParameteri(sc::gl::GL_TEXTURE_2D, sc::gl::GL_TEXTURE_MAG_FILTER, sc::gl::GL_LINEAR);
            sc::gl::glTexParameteri(sc::gl::GL_TEXTURE_2D, sc::gl::GL_TEXTURE_WRAP_S, sc::gl::GL_CLAMP_TO_EDGE);
            sc::gl::glTexParameteri(sc::gl::GL_TEXTURE_2D, sc::gl::GL_TEXTURE_WRAP_T, sc::gl::GL_CLAMP_TO_EDGE);
            return t;
        }

        static GLuint makeFBO(GLuint colorTex)
        {
            GLuint fbo = 0;
            sc::gl::glGenFramebuffers(1, &fbo);
            sc::gl::glBindFramebuffer(sc::gl::GL_FRAMEBUFFER, fbo);
            sc::gl::glFramebufferTexture2D(sc::gl::GL_FRAMEBUFFER, sc::gl::GL_COLOR_ATTACHMENT0,
                sc::gl::GL_TEXTURE_2D, colorTex, 0);
            return fbo;
        }

        void releaseTextures()
        {
            for (int i = 0; i < MIP_LEVELS; ++i)
            {
                if (mipFBO[i]) { sc::gl::glDeleteFramebuffers(1, &mipFBO[i]); mipFBO[i] = 0; }
                if (mipTex[i]) { sc::gl::glDeleteTextures(1, &mipTex[i]);     mipTex[i] = 0; }
            }
        }

        juce::OpenGLContext& context;
        Shader downShader, upShader, compositeShader;

        GLuint mipFBO[MIP_LEVELS]{};
        GLuint mipTex[MIP_LEVELS]{};
        int    mw[MIP_LEVELS]{}, mh[MIP_LEVELS]{};
        int    w = 0, h = 0;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(BloomPass)
    };

} // namespace sc
