/* ==============================================================================
   Renderer.h
   Layer 2: Scene & Renderer
   每帧渲染入口。持有 PixelMaterial，对外暴露 beginFrame / drawMesh / endFrame。
   上层只需传 camera + 一组 (mesh, model, color)，不用关心 GL 状态。
   ============================================================================== */
#pragma once
#include <JuceHeader.h >
#include "GLUtils.h"
#include "Mesh.h"
#include "Camera.h"
#include "Lighting.h"
#include "PixelMaterial.h"
#include "Material.h"

namespace sc
{
    class Renderer
    {
    public:
        explicit Renderer(juce::OpenGLContext& ctx)
            : context(ctx), material(ctx) {
        }

        //----------------------------------------------------------------------
        // 生命周期：与 OpenGLRenderer 的 newOpenGLContextCreated/openGLContextClosing 对齐
        //----------------------------------------------------------------------
        bool initialise()
        {
            if (!material.build())
            {
                DBG("Renderer: PixelMaterial build failed");
                return false;
            }
            return true;
        }

        void shutdown() {}

        //----------------------------------------------------------------------
        // 帧
        //----------------------------------------------------------------------
        void beginFrame(const Camera& camera, const Lighting& light, const Vec3& playerPos,
            const Vec3& clearColor = { 0.06f, 0.07f, 0.09f }) noexcept
        {
            using namespace sc::gl;
            if (juce::gl::glDebugMessageControl != nullptr)
                juce::gl::glDebugMessageControl(GL_DONT_CARE, GL_DONT_CARE,
                    GL_DEBUG_SEVERITY_NOTIFICATION,
                    0, nullptr, GL_FALSE);

            const float scale = (float)context.getRenderingScale();
            const int wPx = (int)(camera.getViewportWidth() * scale);
            const int hPx = (int)(camera.getViewportHeight() * scale);
            
            glViewport(0, 0, wPx, hPx);
            glEnable(GL_DEPTH_TEST);
            glDepthFunc(GL_LESS);
            glDisable(GL_CULL_FACE); // 像素风暂不剔除背面，便于调试
            glClearColor(clearColor.x, clearColor.y, clearColor.z, 1.0f);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

            material.use();
            material.setCameraMatrices(camera.view(), camera.proj());
            const Vec3 eye = camera.getEyeWorld();
            material.setCameraPos(eye);
            material.setPlayerPos(playerPos);
            material.setLighting(light);
            material.setPointLights(light.pointLights, eye);   // ★ 新增
            material.setFog(light);                            // ★ 新增
            material.setShadeLevels(shadeLevels);
            material.setLineMode(false);


        }
       


        /** 帮助函数：把设计色的 sRGB 值转换为线性值。 */
        static Vec3 srgbToLinear(const Vec3& c) noexcept
        {
            auto toLin = [](float v) {
                return v <= 0.04045f ? v / 12.92f
                    : std::pow((v + 0.055f) / 1.055f, 2.4f);
                };
            return { toLin(c.x), toLin(c.y), toLin(c.z) };
        }

        // ==================== 绘制接口 ====================

        /** 新主路径：使用完整材质（颜色为 sRGB 设计色）。 */
        void drawMesh(Mesh& mesh, const Mat4& model,
            const Material& mat) noexcept
        {
            const Vec3 baseLin = srgbToLinear(mat.baseColorSRGB);
            const Vec3 emissiveLin = srgbToLinear(mat.emissiveSRGB);
            material.setLineMode(false);
            material.setModel(model);
            material.setMaterial(mat, baseLin, emissiveLin);
            mesh.draw(context);
        }

        /** 兼容旧调用：颜色 + 自发光（sRGB 设计色），内部转为默认 stone 材质属性。 */
        void drawMesh(Mesh& mesh, const Mat4& model,
            const Vec3& baseColorSRGB,
            const Vec3& emissiveSRGB = { 0,0,0 }) noexcept
        {
            const Vec3 baseLin = srgbToLinear(baseColorSRGB);
            const Vec3 emissiveLin = srgbToLinear(emissiveSRGB);
            material.setLineMode(false);
            material.setModel(model);
            material.setBaseColorLegacy(baseLin, emissiveLin);
            mesh.draw(context);
        }

        /** 网格线（GL_LINES）绘制：跳过光照。 */
        void drawLines(Mesh& mesh, const Mat4& model, const Vec3& color) noexcept
        {
            material.setLineMode(true);
            material.setModel(model);
            material.setBaseColorLegacy(color, { 0, 0, 0 });
            mesh.draw(context);
        }

        void endFrame() noexcept
        {
            sc::gl::checkError("endFrame");
        }

        //----------------------------------------------------------------------
        // 风格控制
        //----------------------------------------------------------------------
        /** 色阶量化档数。1 = 关闭，3~6 像素感最强。 */
        void setShadeLevels(float levels) noexcept { shadeLevels = juce::jmax(1.0f, levels); }

    private:
        juce::OpenGLContext& context;
        PixelMaterial material;
        float shadeLevels{ 4.0f };
        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(Renderer)
    };
}
