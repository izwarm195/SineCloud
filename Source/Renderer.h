// ==============================================================================
//   Renderer.h
//   Layer 3 + 4: Deferred Rendering + Bloom
// ==============================================================================
#pragma once

#include "GLUtils.h"
#include "Mesh.h"
#include "Camera.h"
#include "Lighting.h"
#include "Material.h"
#include "GBuffer.h"
#include "PostPipeline.h"
#include "ShadowMap.h"
#include "BloomPass.h" 
#include "MotionBlurPass.h" 
#include "PixelatePass.h" 

namespace sc {

    class Renderer
    {
    public:
        explicit Renderer(juce::OpenGLContext& ctx)
            : context(ctx), gbuffer(ctx), postPipeline(ctx), shadowMap(ctx), bloom(ctx), motionBlur(ctx),pixelate(ctx) {
        }  // ★ bloom

// ----------------------------------------------------------------
// 生命周期
// ----------------------------------------------------------------
        bool initialise()
        {
            if (!gbuffer.build()) { DBG("Renderer: GBuffer build failed");      return false; }
            if (!postPipeline.build()) { DBG("Renderer: PostPipeline build failed"); return false; }
            if (!shadowMap.build()) { DBG("Renderer: ShadowMap build failed");    return false; }
            if (!bloom.build()) { DBG("Renderer: BloomPass build failed");    return false; }  // ★
            if (!motionBlur.build()) { DBG("Renderer: MotionBlurPass build failed"); return false; }
            if (!pixelate.build()) { DBG("Renderer: PixelatePass build failed"); return false; }

            return true;
        }

        void shutdown()
        {
            releaseSceneHDR();          // ★
            gbuffer.shutdown();
            postPipeline.shutdown();
            shadowMap.shutdown();
            bloom.shutdown();           // ★
            motionBlur.shutdown();
            pixelate.shutdown();
            releaseMBInput();

        }

        // ----------------------------------------------------------------
        // Phase 1: 绑 G-Buffer，清屏，准备几何写入
        // ----------------------------------------------------------------
        void beginFrame(const Camera& camera,
            const Lighting& light,
            const Vec3& playerPos,
            const Vec3& clearColor = { 0.06f, 0.07f, 0.09f }) noexcept
        {
            using namespace sc::gl;

            if (juce::gl::glDebugMessageControl != nullptr)
                juce::gl::glDebugMessageControl(
                    GL_DONT_CARE, GL_DONT_CARE,
                    GL_DEBUG_SEVERITY_NOTIFICATION, 0, nullptr, GL_FALSE);

            const float scale = (float)context.getRenderingScale();
            const int wPx = (int)(camera.getViewportWidth() * scale);
            const int hPx = (int)(camera.getViewportHeight() * scale);

            gbuffer.ensureSize(wPx, hPx);

            gbuffer.bindForGeometry();

            glViewport(0, 0, wPx, hPx);
            glEnable(GL_DEPTH_TEST);
            glDepthFunc(GL_LESS);
            glDisable(GL_BLEND);
            glDepthMask(GL_TRUE);


            // RT2 (emissive + sss): 必须清零,否则背景凭空发光
            glClearColor(clearColor.x, clearColor.y, clearColor.z, 1.0f);

            const GLfloat clrAlbedo[4] = { clearColor.x, clearColor.y, clearColor.z, 1.0f };
            const GLfloat clrNormal[4] = { 0.5f, 0.5f, 1.0f, 0.0f };
            const GLfloat clrEmiss[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
            const GLfloat clrVel[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
            glClearBufferfv(GL_COLOR, 0, clrAlbedo);
            glClearBufferfv(GL_COLOR, 1, clrNormal);
            glClearBufferfv(GL_COLOR, 2, clrEmiss);
            glClearBufferfv(GL_COLOR, 3, clrVel);
            glClear(GL_DEPTH_BUFFER_BIT);
            // ★ 加这两行
            const GLfloat clrWorldPos[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
            glClearBufferfv(GL_COLOR, 4, clrWorldPos);

            Shader& geom = gbuffer.getGeometryShader();

            geom.use();
            geom.setMat4("uView", camera.view());
            geom.setMat4("uProj", camera.proj());
            geom.setMat4("uPrevModelViewProj", prevViewProj);
            geom.setInt("uIsLine", 0);
        }

        // ----------------------------------------------------------------
        // sRGB -> linear
        // ----------------------------------------------------------------
        static Vec3 srgbToLinear(const Vec3& c) noexcept
        {
            auto toLin = [](float v) {
                return v <= 0.04045f ? v / 12.92f
                    : std::pow((v + 0.055f) / 1.055f, 2.4f);
                };
            return { toLin(c.x), toLin(c.y), toLin(c.z) };
        }

        // ----------------------------------------------------------------
        // 绘制接口 (Phase 1: 写入 G-Buffer)
        // ----------------------------------------------------------------
        void drawMesh(Mesh& mesh, const Mat4& model, const Material& mat) noexcept
        {
            Shader& geom = gbuffer.getGeometryShader();
            geom.setInt("uIsLine", 0);
            geom.setMat4("uModel", model);

            const Vec3 baseLin = srgbToLinear(mat.baseColorSRGB);
            const Vec3 emissiveLin = srgbToLinear(mat.emissiveSRGB);

            geom.setVec3("uBaseColor", baseLin);
            geom.setFloat("uRoughness", mat.roughness);
            geom.setFloat("uMetallic", mat.metallic);
            geom.setVec3("uEmissive", emissiveLin);
            geom.setFloat("uEmissiveStrength", mat.emissiveStrength);
            geom.setFloat("uSSS", mat.sss);

            mesh.draw(context);
        }

        // 兼容旧调用: 颜色 + 自发光 (sRGB)
        void drawMesh(Mesh& mesh, const Mat4& model,
            const Vec3& baseColorSRGB,
            const Vec3& emissiveSRGB = { 0,0,0 }) noexcept
        {
            Shader& geom = gbuffer.getGeometryShader();
            geom.setInt("uIsLine", 0);
            geom.setMat4("uModel", model);

            const Vec3 baseLin = srgbToLinear(baseColorSRGB);
            const Vec3 emissiveLin = srgbToLinear(emissiveSRGB);

            geom.setVec3("uBaseColor", baseLin);
            geom.setFloat("uRoughness", 0.9f);
            geom.setFloat("uMetallic", 0.0f);
            geom.setVec3("uEmissive", emissiveLin);
            geom.setFloat("uEmissiveStrength", 0.0f);
            geom.setFloat("uSSS", 0.0f);

            mesh.draw(context);
        }

        // 网格线: 线模式写入 G-Buffer
        void drawLines(Mesh& mesh, const Mat4& model, const Vec3& color) noexcept
        {
            Shader& geom = gbuffer.getGeometryShader();
            geom.setInt("uIsLine", 1);
            geom.setMat4("uModel", model);

            const Vec3 colLin = srgbToLinear(color);
            geom.setVec3("uBaseColor", colLin);
            geom.setFloat("uRoughness", 1.0f);
            geom.setFloat("uMetallic", 0.0f);
            geom.setVec3("uEmissive", { 0,0,0 });
            geom.setFloat("uEmissiveStrength", 0.0f);
            geom.setFloat("uSSS", 0.0f);

            mesh.draw(context);

            geom.setInt("uIsLine", 0);
        }

        // ----------------------------------------------------------------
        // Phase 2: 延迟光照 → HDR → Bloom → Composite
        // ----------------------------------------------------------------
        void endFrame(const Camera& camera, const Lighting& light, const Vec3& playerPos) noexcept
        {
            using namespace sc::gl;
            gbuffer.unbind();

            const float scale = (float)context.getRenderingScale();
            const int wPx = (int)(camera.getViewportWidth() * scale);
            const int hPx = (int)(camera.getViewportHeight() * scale);

            // ★ 确保 HDR 中间缓冲 + Bloom mip 尺寸一致
            ensureSceneHDR(wPx, hPx);
            ensureMBInput(wPx, hPx);
            bloom.ensureSize(wPx, hPx);

            // ★ Shadow Map 绑定（和原来逻辑一致）
            {
                Shader& shader = postPipeline.getDeferredShader();
                shader.use();
                glActiveTexture(GL_TEXTURE6);
                glBindTexture(GL_TEXTURE_2D, shadowMap.getDirShadowMap());
                GLint loc = glGetUniformLocation(shader.raw().getProgramID(), "uDirShadowMap");
                if (loc >= 0) glUniform1i(loc, 6);
                glActiveTexture(GL_TEXTURE7);
                glBindTexture(GL_TEXTURE_CUBE_MAP, shadowMap.getCubeShadowMap());
                loc = glGetUniformLocation(shader.raw().getProgramID(), "uCubeShadowMap");
                if (loc >= 0) glUniform1i(loc, 7);
                loc = glGetUniformLocation(shader.raw().getProgramID(), "uLightViewProj");
                if (loc >= 0) glUniformMatrix4fv(loc, 1, GL_FALSE, shadowMap.lightViewProj.mat);
            }

            // ★ 1) 延迟光照 → sceneHDRTex
            glBindFramebuffer(GL_FRAMEBUFFER, sceneHDRFBO);
            glViewport(0, 0, wPx, hPx);
            postPipeline.doLighting(gbuffer, camera, light, playerPos, 1.0f);

            // ★ 2) Bloom + ACES + Gamma + Posterize → mbInputFBO（中间缓冲）
            bloom.render(sceneHDRTex,
                postPipeline.getFullscreenVAO(),
                light.bloomThreshold,
                light.bloomSoftKnee,
                light.bloomStrength,
                light.bloomFilterRadius,
                shadeLevels,
                mbInputFBO); 
            // ★ 输出到中间 FBO
            
            // ★ 像素化 pass：读 mbInputTex，写回 mbInputFBO（或新开 FBO）
            pixelate.render(mbInputTex,                   // src = bloom composite 输出
                postPipeline.getFullscreenVAO(),
                wPx, hPx,
                pixelSize,
                colorLevels > 0.0f ? colorLevels : shadeLevels, // fallback
                false,                         // 默认用亮度 posterize 模式
                mbInputFBO);                   // 写回同一个中间 FBO

            // ★ 3) Motion Blur → 默认 FBO
            glBindFramebuffer(GL_FRAMEBUFFER, 0);
            glViewport(0, 0, wPx, hPx);
            motionBlur.render(mbInputTex,
                gbuffer.getVelocityTex(),        // ★ GBuffer 需暴露 gVelocity getter
                postPipeline.getFullscreenVAO(),
                wPx, hPx,
                0.5f,   // intensity
                16);    // samples

            prevViewProj = camera.proj() * camera.view();
            checkError("endFrame");

        }

        void setShadeLevels(float levels) noexcept
        {
            shadeLevels = juce::jmax(1.0f, levels);
        }

        ShadowMap& getShadowMap() noexcept { return shadowMap; }

    private:
        juce::OpenGLContext& context;
        GBuffer      gbuffer;
        PostPipeline postPipeline;
        ShadowMap    shadowMap;
        BloomPass    bloom;
        MotionBlurPass motionBlur;


        float shadeLevels{ 64.0f };
        Mat4  prevViewProj;

        // ★ sceneHDR 中间缓冲（成员变量定义在这里，ensureSceneHDR / releaseSceneHDR 使用它们）
        GLuint sceneHDRFBO = 0;
        GLuint sceneHDRTex = 0;
        // ★ 新增中间 FBO 成员（sceneHDR 区域后）
        GLuint mbInputFBO = 0;        // ★ L5: bloom composite → 这个 FBO
        GLuint mbInputTex = 0;        // ★ L5: 对应的纹理
        int    mbInputW = 0, mbInputH = 0;
        int    sceneHDRW = 0;
        int    sceneHDRH = 0;

        PixelatePass pixelate;   // ★ 新成员
        float pixelSize = 4.0f; // 默认 4×4 像素块
        float colorLevels = 0.0f; // 0 = 不做色阶量化，沿用 shadeLevels 即可

        // ★ 私有辅助方法
        void ensureSceneHDR(int newW, int newH)
        {
            if (newW == sceneHDRW && newH == sceneHDRH) return;
            releaseSceneHDR();
            sceneHDRW = newW;
            sceneHDRH = newH;

            using namespace sc::gl;

            glGenTextures(1, &sceneHDRTex);
            glBindTexture(GL_TEXTURE_2D, sceneHDRTex);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, newW, newH, 0, GL_RGBA, GL_FLOAT, nullptr);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

            glGenFramebuffers(1, &sceneHDRFBO);
            glBindFramebuffer(GL_FRAMEBUFFER, sceneHDRFBO);
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, sceneHDRTex, 0);

            glBindFramebuffer(GL_FRAMEBUFFER, 0);
        }

        void releaseSceneHDR()
        {
            if (sceneHDRFBO) { sc::gl::glDeleteFramebuffers(1, &sceneHDRFBO); sceneHDRFBO = 0; }
            if (sceneHDRTex) { sc::gl::glDeleteTextures(1, &sceneHDRTex);     sceneHDRTex = 0; }
            sceneHDRW = sceneHDRH = 0;
        }

        void ensureMBInput(int newW, int newH)
        {
            if (newW == mbInputW && newH == mbInputH) return;
            releaseMBInput();
            mbInputW = newW; mbInputH = newH;
            using namespace sc::gl;
            glGenTextures(1, &mbInputTex);
            glBindTexture(GL_TEXTURE_2D, mbInputTex);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, newW, newH, 0, GL_RGBA, GL_FLOAT, nullptr);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            glGenFramebuffers(1, &mbInputFBO);
            glBindFramebuffer(GL_FRAMEBUFFER, mbInputFBO);
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, mbInputTex, 0);
            glBindFramebuffer(GL_FRAMEBUFFER, 0);
        }

        void releaseMBInput()
        {
            if (mbInputFBO) { sc::gl::glDeleteFramebuffers(1, &mbInputFBO); mbInputFBO = 0; }
            if (mbInputTex) { sc::gl::glDeleteTextures(1, &mbInputTex); mbInputTex = 0; }
            mbInputW = mbInputH = 0;
        }


        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(Renderer)
    };

} // namespace sc
