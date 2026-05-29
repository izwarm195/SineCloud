/*
  ==============================================================================
    Renderer.h
    Layer 2: Scene & Renderer
    Ã¿Ö¡äÖÈ¾Èë¿Ú¡£³ÖÓÐ PixelMaterial£¬¶ÔÍâ±©Â¶ beginFrame / drawMesh / endFrame¡£
    ÉÏ²ãÖ»Ðè´« camera + Ò»×é (mesh, model, color)£¬²»ÓÃ¹ØÐÄ GL ×´Ì¬¡£
  ==============================================================================
*/
#pragma once

#include <JuceHeader.h>
#include "GLUtils.h"
#include "Mesh.h"
#include "Camera.h"
#include "Lighting.h"
#include "PixelMaterial.h"

namespace sc
{
    class Renderer
    {
    public:
        explicit Renderer(juce::OpenGLContext& ctx)
            : context(ctx), material(ctx) {
        }

        //----------------------------------------------------------------------
        // ÉúÃüÖÜÆÚ£ºÓë OpenGLRenderer µÄ newOpenGLContextCreated/openGLContextClosing ¶ÔÆë
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
        // Ö¡
        //----------------------------------------------------------------------
        void beginFrame(const Camera& camera, const Lighting& light,
            const Vec3& clearColor = { 0.06f, 0.07f, 0.09f }) noexcept
        {
            using namespace sc::gl;

            const float scale = (float)context.getRenderingScale();
            const int wPx = (int)(camera.getViewportWidth() * scale);
            const int hPx = (int)(camera.getViewportHeight() * scale);
            glViewport(0, 0, wPx, hPx);

            glEnable(GL_DEPTH_TEST);
            glDepthFunc(GL_LESS);
            glDisable(GL_CULL_FACE); // ÏñËØ·çÔÝ²»ÌÞ±³Ãæ£¬±ãÓÚµ÷ÊÔ
            glClearColor(clearColor.x, clearColor.y, clearColor.z, 1.0f);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

            material.use();
            material.setCameraMatrices(camera.view(), camera.proj());
            material.setLighting(light);
            material.setShadeLevels(shadeLevels);
            material.setLineMode(false);
        }

        /** ±ê×¼ÊµÐÄ mesh »æÖÆ¡£ */
        void drawMesh(Mesh& mesh, const Mat4& model,
            const Vec3& baseColor,
            const Vec3& emissive = { 0, 0, 0 }) noexcept
        {
            material.setLineMode(false);
            material.setModel(model);
            material.setBaseColor(baseColor);
            material.setEmissive(emissive);
            mesh.draw(context);
        }

        /** Íø¸ñÏß£¨GL_LINES£©»æÖÆ£ºÌø¹ý¹âÕÕ¡£ */
        void drawLines(Mesh& mesh, const Mat4& model, const Vec3& color) noexcept
        {
            material.setLineMode(true);
            material.setModel(model);
            material.setBaseColor(color);
            material.setEmissive({ 0, 0, 0 });
            mesh.draw(context);
        }

        void endFrame() noexcept
        {
            sc::gl::checkError("endFrame");
        }

        //----------------------------------------------------------------------
        // ·ç¸ñ¿ØÖÆ
        //----------------------------------------------------------------------
        /** É«½×Á¿»¯µµÊý¡£1 = ¹Ø±Õ£¬3~6 ÏñËØ¸Ð×îÇ¿¡£ */
        void setShadeLevels(float levels) noexcept { shadeLevels = juce::jmax(1.0f, levels); }

    private:
        juce::OpenGLContext& context;
        PixelMaterial material;
        float shadeLevels{ 4.0f };

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(Renderer)
    };
}
