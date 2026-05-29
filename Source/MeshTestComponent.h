/*
  ==============================================================================

    MeshTestComponent.h
    Created: 29 May 2026 12:49:52pm
    Author:  wzm

  ==============================================================================
*/

#pragma once
#include <JuceHeader.h>
#include "Mesh.h"
#include "BinaryData.h"

//==============================================================================
// ×îÐ¡¿ÉÐÐµÄ GL äÖÈ¾²âÊÔ£º°Ñ BinaryData::cube_obj äÖÈ¾µ½Ô­µã
// Ïà»ú¹Ì¶¨ÔÚ (4, -6, 4) ¿´ÏòÔ­µã£¬Z-up
//==============================================================================
class MeshTestComponent : public juce::Component,
    public juce::OpenGLRenderer
{
public:
    MeshTestComponent()
    {
        glContext.setRenderer(this);
        glContext.setComponentPaintingEnabled(true);
        glContext.setContinuousRepainting(true);
        glContext.attachTo(*this);
    }

    ~MeshTestComponent() override
    {
        glContext.detach();
    }

    //--- OpenGLRenderer ---
    void newOpenGLContextCreated() override
    {
        // 1) ¼ÓÔØ cube.obj
        if (!cubeMesh.loadFromObjMemory(BinaryData::Cube_obj,
            (size_t)BinaryData::Cube_objSize))
        {
            DBG("Failed to load cube.obj");
            return;
        }
        cubeMesh.uploadToGPU(glContext);

        // 2) ±àÒë shader
        shader.reset(new juce::OpenGLShaderProgram(glContext));

        const char* vs = R"(#version 330 core
            layout(location=0) in vec3 aPos;
            layout(location=1) in vec3 aNormal;
            layout(location=2) in vec2 aUV;

            uniform mat4 uModel;
            uniform mat4 uView;
            uniform mat4 uProj;

            out vec3 vNormalWS;
            out vec3 vPosWS;

            void main() {
                vec4 wp = uModel * vec4(aPos, 1.0);
                vPosWS = wp.xyz;
                vNormalWS = mat3(uModel) * aNormal;
                gl_Position = uProj * uView * wp;
            }
        )";

        const char* fs = R"(#version 330 core
            in vec3 vNormalWS;
            in vec3 vPosWS;

            uniform vec3 uLightDir;
            uniform vec3 uBaseColor;

            out vec4 fragColor;

            void main() {
                vec3 N = normalize(vNormalWS);
                float lambert = max(dot(N, -normalize(uLightDir)), 0.0);
                float ambient = 0.25;
                vec3 col = uBaseColor * (ambient + (1.0 - ambient) * lambert);
                fragColor = vec4(col, 1.0);
            }
        )";

        if (!shader->addVertexShader(vs)) DBG("VS err: " << shader->getLastError());
        if (!shader->addFragmentShader(fs)) DBG("FS err: " << shader->getLastError());
        if (!shader->link())                  DBG("Link err: " << shader->getLastError());
    }

    void renderOpenGL() override
    {
        using namespace juce::gl;

        const float scale = (float)glContext.getRenderingScale();
        const int   w = (int)(getWidth() * scale);
        const int   h = (int)(getHeight() * scale);
        glViewport(0, 0, w, h);

        glEnable(GL_DEPTH_TEST);
        glClearColor(0.08f, 0.10f, 0.12f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        if (shader == nullptr || !cubeMesh.isUploaded()) return;

        shader->use();

        // ---- ¾ØÕó£ºZ-up ----
        // Ä£ÐÍ£ºÔ­µã£¬µ¥Î»±ä»»
        const auto model = juce::Matrix3D<float>(); // identity

        // ÊÓÍ¼£ºÏà»ú (4, -6, 4) ¿´ÏòÔ­µã£¬up = +Z
        const juce::Vector3D<float> eye{ 4.0f, -6.0f, 4.0f };
        const juce::Vector3D<float> target{ 0.0f,  0.0f, 0.0f };
        const juce::Vector3D<float> up{ 0.0f,  0.0f, 1.0f };
        const auto view = lookAt(eye, target, up);

        // Í¶Ó°£º60¡ã fov
        const float aspect = (float)w / juce::jmax(1, h);
        const auto  proj = perspective(juce::degreesToRadians(60.0f),
            aspect, 0.1f, 100.0f);

        setMatrixUniform(*shader, "uModel", model);
        setMatrixUniform(*shader, "uView", view);
        setMatrixUniform(*shader, "uProj", proj);

        const GLint locLight = glGetUniformLocation(shader->getProgramID(), "uLightDir");
        const GLint locColor = glGetUniformLocation(shader->getProgramID(), "uBaseColor");
        glUniform3f(locLight, -0.4f, 0.6f, -0.7f);   // ·½Ïò¹â
        glUniform3f(locColor, 0.65f, 0.62f, 0.58f); // Ê¯Í·»Ò

        cubeMesh.draw(glContext);
    }

    void openGLContextClosing() override
    {
        cubeMesh.releaseGPU(glContext);
        shader = nullptr;
    }

    void paint(juce::Graphics& g) override
    {
        // ÕâÀï»­µÄÄÚÈÝ»áµþÔÚ GL Ö®ÉÏ
        g.setColour(juce::Colours::white);
        g.setFont(14.0f);
        g.drawText("Mesh test: cube at origin (Z-up)", 10, 10, 400, 20,
            juce::Justification::left);
    }

private:
    //--- ¾ØÕó¹¤¾ß£ºJUCE Ã»ÓÐÄÚÖÃ lookAt / perspective£¬×Ô¼ºÐ´ ---
    static juce::Matrix3D<float> lookAt(juce::Vector3D<float> eye,
        juce::Vector3D<float> target,
        juce::Vector3D<float> up)
    {
        auto f = (target - eye); f = f / f.length();           // forward
        auto s = f ^ up;          s = s / s.length();           // right
        auto u = s ^ f;                                          // true up

        // ÁÐÖ÷Ðò£ºJUCE Matrix3D µÄ mat[16] Ò²ÊÇÁÐÖ÷Ðò£¨Óë GL Ò»ÖÂ£©
        float m[16] = {
             s.x,  u.x, -f.x, 0.0f,
             s.y,  u.y, -f.y, 0.0f,
             s.z,  u.z, -f.z, 0.0f,
            -(s * eye), -(u * eye), (f * eye), 1.0f
        };
        return juce::Matrix3D<float>(m);
    }

    static juce::Matrix3D<float> perspective(float fovYRadians, float aspect,
        float zNear, float zFar)
    {
        const float f = 1.0f / std::tan(fovYRadians * 0.5f);
        float m[16] = {
            f / aspect, 0.0f, 0.0f, 0.0f,
            0.0f,       f,    0.0f, 0.0f,
            0.0f, 0.0f, (zFar + zNear) / (zNear - zFar), -1.0f,
            0.0f, 0.0f, (2.0f * zFar * zNear) / (zNear - zFar), 0.0f
        };
        return juce::Matrix3D<float>(m);
    }

    static void setMatrixUniform(juce::OpenGLShaderProgram& sh,
        const char* name,
        const juce::Matrix3D<float>& m)
    {
        const GLint loc = juce::gl::glGetUniformLocation(sh.getProgramID(), name);
        if (loc >= 0)
            juce::gl::glUniformMatrix4fv(loc, 1, juce::gl::GL_FALSE, m.mat);
    }

    juce::OpenGLContext glContext;
    Mesh cubeMesh;
    std::unique_ptr<juce::OpenGLShaderProgram> shader;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MeshTestComponent)
};
