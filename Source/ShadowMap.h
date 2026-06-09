/* ==============================================================================
   ShadowMap.h
   Layer 2: Scene & Renderer
   方向光 2D Shadow Map + 点光源 Cube Shadow Map。
   在 G-Buffer 几何阶段之前执行深度预渲染。
   ============================================================================== */
#pragma once

#include "GLUtils.h"
#include "Shader.h"
#include "Mat4.h"
#include "Vec.h"
#include "Camera.h"

namespace sc {

    class ShadowMap
    {
    public:
        explicit ShadowMap(juce::OpenGLContext& ctx)
            : context(ctx), depthShader(ctx) {
        }

        // ----------------------------------------------------------------
        // 生命周期
        // ----------------------------------------------------------------
        bool build()
        {
            using namespace sc::gl;

            // ---- 深度写入 shader（顶点变换到 light clip space，片元为空）----
            const juce::String depthVS = R"(#version 330 core
layout(location = 0) in vec3 aPos;
uniform mat4 uLightViewProj;
uniform mat4 uModel;

void main()
{
    gl_Position = uLightViewProj * uModel * vec4(aPos, 1.0);
}
)";
            const juce::String depthFS = R"(#version 330 core
// depth-only: 片元着色器为空，深度自动写入
void main() {}
)";

            if (!depthShader.build(depthVS, depthFS))
            {
                DBG("ShadowMap: depth shader build failed");
                return false;
            }

            // ---- 方向光 Shadow Map (2D) ----
            dirRes = 4096;
            glGenFramebuffers(1, &dirFBO);
            glGenTextures(1, &dirShadowMap);
            glBindTexture(GL_TEXTURE_2D, dirShadowMap);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT24,
                dirRes, dirRes, 0, GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
            const float border[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
            glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, border);

            glBindFramebuffer(GL_FRAMEBUFFER, dirFBO);
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
                GL_TEXTURE_2D, dirShadowMap, 0);
            glDrawBuffer(GL_NONE);   // 无颜色输出
            glReadBuffer(GL_NONE);
            checkComplete("DirectionalShadow");
            glBindFramebuffer(GL_FRAMEBUFFER, 0);

            // ---- 点光源 Cube Shadow Map ----
            cubeRes = 512;
            glGenFramebuffers(1, &cubeFBO);
            glGenTextures(1, &cubeShadowMap);
            glBindTexture(GL_TEXTURE_CUBE_MAP, cubeShadowMap);
            for (int i = 0; i < 6; ++i)
            {
                glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0,
                    GL_DEPTH_COMPONENT24,
                    cubeRes, cubeRes, 0,
                    GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
            }
            glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

            glBindFramebuffer(GL_FRAMEBUFFER, cubeFBO);
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
                GL_TEXTURE_CUBE_MAP_POSITIVE_X, cubeShadowMap, 0);
            glDrawBuffer(GL_NONE);
            glReadBuffer(GL_NONE);
            checkComplete("CubeShadow");
            glBindFramebuffer(GL_FRAMEBUFFER, 0);

            checkError("ShadowMap::build");
            return true;
        }

        void shutdown()
        {
            if (dirFBO) { sc::gl::glDeleteFramebuffers(1, &dirFBO);       dirFBO = 0; }
            if (dirShadowMap) { sc::gl::glDeleteTextures(1, &dirShadowMap);      dirShadowMap = 0; }
            if (cubeFBO) { sc::gl::glDeleteFramebuffers(1, &cubeFBO);       cubeFBO = 0; }
            if (cubeShadowMap) { sc::gl::glDeleteTextures(1, &cubeShadowMap);     cubeShadowMap = 0; }
            depthShader.raw().release();
        }

        // ----------------------------------------------------------------
        // 方向光 Shadow Pass
        // ----------------------------------------------------------------
        void beginDirectionalPass(const Vec3& lightDir, const Vec3& pivot, float sceneRadius)
        {
            using namespace sc::gl;

            // 光源"相机"：沿光线反方向远离 pivot
            const Vec3 lightPos = pivot - normalize(lightDir) * (sceneRadius * 2.0f);
            const Vec3 lightTarget = pivot;
            const Vec3 up = (std::abs(dot(normalize(lightDir), Vec3{ 0,0,1 })) > 0.999f)
                ? Vec3{ 0, 1, 0 } : Vec3{ 0, 0, 1 };

            const Mat4 lightView = lookAt(lightPos, lightTarget, up);
            const float halfSz = sceneRadius;
            const Mat4 lightProj = ortho(-halfSz, halfSz, -halfSz, halfSz,
                0.02f, sceneRadius * 2.5f);
            lightViewProj = lightProj * lightView;

            // 保存当前 viewport
            GLint prevVP[4];
            glGetIntegerv(GL_VIEWPORT, prevVP);

            glBindFramebuffer(GL_FRAMEBUFFER, dirFBO);
            glViewport(0, 0, dirRes, dirRes);
            glEnable(GL_DEPTH_TEST);
            glDepthFunc(GL_LESS);
            glDepthMask(GL_TRUE);
            glClear(GL_DEPTH_BUFFER_BIT);

            // 存储以便 restore
            prevVPW = prevVP[2];
            prevVPH = prevVP[3];

            depthShader.use();
            depthShader.setMat4("uLightViewProj", lightViewProj);
        }

        void endDirectionalPass()
        {
            using namespace sc::gl;
            glBindFramebuffer(GL_FRAMEBUFFER, 0);
            glViewport(0, 0, prevVPW, prevVPH);
            glDepthMask(GL_TRUE);
        }

        // 设置 model 矩阵并绘制（在 begin/end 之间调用）
        void setModelForDepth(const Mat4& model)
        {
            depthShader.setMat4("uModel", model);
        }

        // ----------------------------------------------------------------
        // 点光源 Cube Shadow Pass（单面）
        // ----------------------------------------------------------------
        /** face ∈ [0,5] 映射到 +X, -X, +Y, -Y, +Z, -Z */
        void beginCubeFace(int face, const Vec3& lightPos, float farPlane)
        {
            using namespace sc::gl;

            static const Vec3 targets[6] = {
                { 1, 0, 0}, {-1, 0, 0}, {0,  1, 0},
                {0,-1, 0}, { 0, 0, 1}, {0, 0,-1}
            };
            static const Vec3 ups[6] = {
                {0,-1, 0}, {0,-1, 0}, {0, 0, 1},
                {0, 0,-1}, {0,-1, 0}, {0,-1, 0}
            };

            ptLightShadowPos = lightPos;
            ptLightShadowRange = farPlane;

            Mat4 cubeView = lookAt(lightPos, lightPos + targets[face], ups[face]);
            Mat4 cubeProj = perspective(1.57079632679f /*90°*/, 1.0f, 0.05f, farPlane);
            cubeLightViewProj = cubeProj * cubeView;

            GLint prevVP[4];
            glGetIntegerv(GL_VIEWPORT, prevVP);
            prevVPW = prevVP[2]; prevVPH = prevVP[3];

            glBindFramebuffer(GL_FRAMEBUFFER, cubeFBO);
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
                GL_TEXTURE_CUBE_MAP_POSITIVE_X + face,
                cubeShadowMap, 0);
            glViewport(0, 0, cubeRes, cubeRes);
            glEnable(GL_DEPTH_TEST);
            glDepthFunc(GL_LESS);
            glDepthMask(GL_TRUE);
            glClear(GL_DEPTH_BUFFER_BIT);

            depthShader.use();
            depthShader.setMat4("uLightViewProj", cubeLightViewProj);
        }

        void endCubeFace()
        {
            using namespace sc::gl;
            glBindFramebuffer(GL_FRAMEBUFFER, 0);
            glViewport(0, 0, prevVPW, prevVPH);
        }

        // ----------------------------------------------------------------
        // 绑定纹理到 PostPipeline shader
        // ----------------------------------------------------------------
        void bindForLighting(Shader& shader, int startUnit) const
        {
            using namespace sc::gl;

            // 方向光 Shadow Map → texture unit startUnit
            glActiveTexture(GL_TEXTURE0 + startUnit);
            glBindTexture(GL_TEXTURE_2D, dirShadowMap);
            GLint loc = glGetUniformLocation(shader.raw().getProgramID(), "uDirShadowMap");
            if (loc >= 0) glUniform1i(loc, startUnit);

            // Cube Shadow Map → texture unit startUnit+1
            glActiveTexture(GL_TEXTURE0 + startUnit + 1);
            glBindTexture(GL_TEXTURE_CUBE_MAP, cubeShadowMap);
            loc = glGetUniformLocation(shader.raw().getProgramID(), "uCubeShadowMap");
            if (loc >= 0) glUniform1i(loc, startUnit + 1);

            // Light ViewProj
            loc = glGetUniformLocation(shader.raw().getProgramID(), "uLightViewProj");
            if (loc >= 0) glUniformMatrix4fv(loc, 1, GL_FALSE, lightViewProj.mat);

            // Point light shadow params
            shader.setVec3("uPtShadowPos", ptLightShadowPos);
            shader.setFloat("uPtShadowRange", ptLightShadowRange);
            shader.setFloat("uShadowBias", 0.0015f);
            shader.setFloat("uShadowStrength", 0.85f);
        }

        // 获取深度 shader（供 Renderer 绑定 model 矩阵后 draw）
        Shader& getDepthShader() noexcept { return depthShader; }

        GLuint getDirShadowMap() const noexcept { return dirShadowMap; }
        GLuint getCubeShadowMap() const noexcept { return cubeShadowMap; }

    private:
        void checkComplete(const char* tag) const
        {
#if JUCE_DEBUG
            const GLenum s = sc::gl::glCheckFramebufferStatus(sc::gl::GL_FRAMEBUFFER);
            if (s != sc::gl::GL_FRAMEBUFFER_COMPLETE)
                DBG("ShadowMap FBO incomplete [" << tag << "]: 0x"
                    << juce::String::toHexString((int)s));
#endif
        }

        juce::OpenGLContext& context;
        Shader depthShader;

        // 方向光
        GLuint dirFBO = 0;
        GLuint dirShadowMap = 0;
        int    dirRes = 2048;

        // 点光源 Cube
        GLuint cubeFBO = 0;
        GLuint cubeShadowMap = 0;
        int    cubeRes = 512;

        // 临时状态恢复
        int prevVPW = 1, prevVPH = 1;

    public:
        Mat4  lightViewProj;         // 方向光 VP（公开以便外部读取）
        Vec3  ptLightShadowPos;
        float ptLightShadowRange = 10.0f;

    private:
        Mat4  cubeLightViewProj;     // Cube 面 VP（临时）

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ShadowMap)
    };

} // namespace sc
