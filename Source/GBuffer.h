/* ==============================================================================
   GBuffer.h
   Layer 3: Deferred Rendering
   MRT FBO：4 个颜色附件 + 1 个深度纹理。
   RT0 (RGBA8)  : albedo.rgb (linear) + roughness
   RT1 (RGBA16F): worldNormal.xyz + metallic
   RT2 (RGBA8)  : emissive.rgb (linear) + sss
   RT3 (RG16F)  : velocity.xy (NDC 差值, 为 Layer 5 占位)
   Depth         : DEPTH_COMPONENT24 纹理 (供光照 pass 重建世界坐标)
   ============================================================================== */
#pragma once
#include "GLUtils.h"
#include "Shader.h"

namespace sc {

    class GBuffer
    {
    public:
        explicit GBuffer(juce::OpenGLContext& ctx)
            : context(ctx), geomShader(ctx) {
        }

        // ----------------------------------------------------------------
        // 生命周期
        // ----------------------------------------------------------------
        bool build()
        {
            using namespace sc::gl;

            // ---- 几何 pass shader (写入 MRT) ----
            const juce::String vs = R"(#version 330 core
        layout(location = 0) in vec3 aPos;
        layout(location = 1) in vec3 aNormal;
        layout(location = 2) in vec2 aUV;


        uniform mat4 uModel;
        uniform mat4 uView;
        uniform mat4 uProj;
        uniform mat4 uPrevModelViewProj;

        out vec3 vNormalWS;
        out vec3 vPosWS;
        out vec2 vUV;
        out vec4 vCurrClip;
        out vec4 vPrevClip;

        void main()
        {
            vec4 wp = uModel * vec4(aPos, 1.0);
            vPosWS    = wp.xyz;
            vNormalWS = normalize(mat3(uModel) * aNormal);
            vUV       = aUV;
            vCurrClip = uProj * uView * wp;
            vPrevClip = uPrevModelViewProj * wp; 
            gl_Position = vCurrClip;
        })";

            const juce::String fs = R"(#version 330 core
        in vec3 vNormalWS;
        in vec3 vPosWS;
        in vec2 vUV;
        in vec4 vCurrClip;
        in vec4 vPrevClip;

        uniform vec3  uBaseColor;        // 线性空间
        uniform float uRoughness;
        uniform float uMetallic;
        uniform vec3  uEmissive;         // 线性空间
        uniform float uEmissiveStrength;
        uniform float uSSS;
        uniform int   uIsLine;

        layout(location = 0) out vec4 outAlbedoRough;  // RT0
        layout(location = 1) out vec4 outNormalMetal;   // RT1
        layout(location = 2) out vec4 outEmissiveSSS;   // RT2
        layout(location = 3) out vec2 outVelocity;      // RT3
        layout(location = 4) out vec3 outWorldPos;   // RT4


        void main()
        {
            // ---- 线模式：当作纯自发光写入 ----
            if (uIsLine == 1)
            {
                vec3 col = uBaseColor + uEmissive * uEmissiveStrength;
                outAlbedoRough  = vec4(col, 1.0);          // roughness=1 → 无高光
                outNormalMetal  = vec4(0.5, 0.5, 1.0, 0.0); // (0,0,1) 平面上法线, metallic=0
                outEmissiveSSS  = vec4(col, 0.0);
                outVelocity     = vec2(0.0);
                outWorldPos = vPosWS;

                return;
            }

            vec3 N = normalize(vNormalWS);

            outAlbedoRough  = vec4(uBaseColor, uRoughness);
            outNormalMetal  = vec4(N * 0.5 + 0.5, uMetallic);
            outEmissiveSSS  = vec4(uEmissive * uEmissiveStrength, uSSS);

            // 速度缓冲占位（Layer 5 正式使用）
            vec3 currNDC = vCurrClip.xyz / max(vCurrClip.w, 1e-6);
            vec3 prevNDC = vPrevClip.xyz / max(vPrevClip.w, 1e-6);
            outVelocity  = (currNDC.xy - prevNDC.xy) * 0.5;
            outWorldPos = vPosWS;

        })";

            if (!geomShader.build(vs, fs))
            {
                DBG("GBuffer: geometry shader build failed");
                return false;
            }

            return true;
        }

        void shutdown()
        {
            releaseTextures();
            geomShader.raw().release();
        }

        /** 在 viewport 变化时重建附件纹理。 */
        void ensureSize(int newW, int newH)
        {
            if (newW == w && newH == h) return;
            w = newW; h = newH;

            using namespace sc::gl;
            releaseTextures();

            glGenFramebuffers(1, &fbo);
            glBindFramebuffer(GL_FRAMEBUFFER, fbo);

            // RT0: albedo.rgb + roughness — 8-bit 够用且省带宽
            gAlbedoRough = makeTex2D(w, h, GL_RGBA8, GL_RGBA, GL_UNSIGNED_BYTE);
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, gAlbedoRough, 0);

            // RT1: normal.xyz + metallic — 需要精度，用 16F
            gNormalMetal = makeTex2D(w, h, GL_RGBA16F, GL_RGBA, GL_FLOAT);
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_TEXTURE_2D, gNormalMetal, 0);

            // RT2: emissive.rgb + sss
            gEmissiveSSS = makeTex2D(w, h, GL_RGBA8, GL_RGBA, GL_UNSIGNED_BYTE);
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT2, GL_TEXTURE_2D, gEmissiveSSS, 0);

            // RT3: velocity.xy — 16F signed
            gVelocity = makeTex2D(w, h, GL_RG16F, GL_RG, GL_FLOAT);
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT3, GL_TEXTURE_2D, gVelocity, 0);

            // RT4: worldPos.xyz  需要高精度
            gWorldPos = makeTex2D(w, h, GL_RGBA16F, GL_RGBA, GL_FLOAT);
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT4, GL_TEXTURE_2D, gWorldPos, 0);


            // Depth 纹理（必须用纹理以在 lighting pass 中采样）
            glGenTextures(1, &gDepth);
            glBindTexture(GL_TEXTURE_2D, gDepth);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT24, w, h, 0,
                GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, gDepth, 0);

            // 设置 MRT 绘制缓冲
            const GLenum bufs[5] = { GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1,
    GL_COLOR_ATTACHMENT2, GL_COLOR_ATTACHMENT3, GL_COLOR_ATTACHMENT4 };
            glDrawBuffers(5, bufs);


            checkComplete();
            glBindFramebuffer(GL_FRAMEBUFFER, 0);
        }

        // ----------------------------------------------------------------
        // 每帧操作
        // ----------------------------------------------------------------
        void bindForGeometry() noexcept
        {
            sc::gl::glBindFramebuffer(sc::gl::GL_FRAMEBUFFER, fbo);
        }

        void unbind() noexcept
        {
            sc::gl::glBindFramebuffer(sc::gl::GL_FRAMEBUFFER, 0);
        }

        /** 将 4 个颜色附件 + depth 绑定到纹理单元 0-4。*/
        void bindTexturesForLighting(Shader& shader) const noexcept
        {
            using namespace sc::gl;
            auto bindTex = [&](GLuint tex, int unit, const char* name) {
                glActiveTexture(GL_TEXTURE0 + unit);
                glBindTexture(GL_TEXTURE_2D, tex);
                const GLint loc = glGetUniformLocation(shader.raw().getProgramID(), name);
                if (loc >= 0) glUniform1i(loc, unit);
                };
            bindTex(gAlbedoRough, 0, "gAlbedoRough");
            bindTex(gNormalMetal, 1, "gNormalMetal");
            bindTex(gEmissiveSSS, 2, "gEmissiveSSS");
            bindTex(gVelocity, 3, "gVelocity");
            bindTex(gDepth, 4, "gDepth");
            bindTex(gWorldPos, 5, "gWorldPos");

        }

        Shader& getGeometryShader() noexcept { return geomShader; }
        int width()  const noexcept { return w; }
        int height() const noexcept { return h; }

        GLuint getVelocityTex() const noexcept { return gVelocity; }


    private:
        void releaseTextures()
        {
            if (fbo != 0) { sc::gl::glDeleteFramebuffers(1, &fbo);      fbo = 0; }
            if (gAlbedoRough != 0) { sc::gl::glDeleteTextures(1, &gAlbedoRough);  gAlbedoRough = 0; }
            if (gNormalMetal != 0) { sc::gl::glDeleteTextures(1, &gNormalMetal);  gNormalMetal = 0; }
            if (gEmissiveSSS != 0) { sc::gl::glDeleteTextures(1, &gEmissiveSSS);  gEmissiveSSS = 0; }
            if (gVelocity != 0) { sc::gl::glDeleteTextures(1, &gVelocity);     gVelocity = 0; }
            if (gDepth != 0) { sc::gl::glDeleteTextures(1, &gDepth);        gDepth = 0; }
            if (gWorldPos != 0) { sc::gl::glDeleteTextures(1, &gWorldPos); gWorldPos = 0; }

        }

        static GLuint makeTex2D(int w, int h, GLint internalFormat, GLenum format, GLenum type)
        {
            GLuint t = 0;
            sc::gl::glGenTextures(1, &t);
            sc::gl::glBindTexture(sc::gl::GL_TEXTURE_2D, t);
            sc::gl::glTexImage2D(sc::gl::GL_TEXTURE_2D, 0, internalFormat, w, h, 0, format, type, nullptr);
            sc::gl::glTexParameteri(sc::gl::GL_TEXTURE_2D, sc::gl::GL_TEXTURE_MIN_FILTER, sc::gl::GL_NEAREST);
            sc::gl::glTexParameteri(sc::gl::GL_TEXTURE_2D, sc::gl::GL_TEXTURE_MAG_FILTER, sc::gl::GL_NEAREST);
            sc::gl::glTexParameteri(sc::gl::GL_TEXTURE_2D, sc::gl::GL_TEXTURE_WRAP_S, sc::gl::GL_CLAMP_TO_EDGE);
            sc::gl::glTexParameteri(sc::gl::GL_TEXTURE_2D, sc::gl::GL_TEXTURE_WRAP_T, sc::gl::GL_CLAMP_TO_EDGE);
            return t;
        }

        void checkComplete() const
        {
#if JUCE_DEBUG
            const GLenum status = sc::gl::glCheckFramebufferStatus(sc::gl::GL_FRAMEBUFFER);
            if (status != sc::gl::GL_FRAMEBUFFER_COMPLETE)
                DBG("GBuffer FBO incomplete: 0x" << juce::String::toHexString((int)status));
#endif
        }

        juce::OpenGLContext& context;
        Shader geomShader;

        GLuint fbo = 0;
        GLuint gAlbedoRough = 0;
        GLuint gNormalMetal = 0;
        GLuint gEmissiveSSS = 0;
        GLuint gVelocity = 0;
        GLuint gDepth = 0;
        GLuint gWorldPos = 0;


        int w = 0, h = 0;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(GBuffer)
    };

} // namespace sc
