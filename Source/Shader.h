/*
  ==============================================================================
    Shader.h
    Layer 2: Scene & Renderer
    juce::OpenGLShaderProgram µÄ±¡·â×° + ÀàÐÍ»¯ uniform setter¡£
  ==============================================================================
*/
#pragma once

#include <JuceHeader.h>
#include "GLUtils.h"
#include "Vec.h"
#include "Mat4.h"

namespace sc
{
    class Shader
    {
    public:
        explicit Shader(juce::OpenGLContext& ctx) : program(ctx) {}

        /** ±àÒë²¢Á´½Ó¡£Ê§°ÜÊ±·µ»Ø false£¬´íÎóÒÑÍ¨¹ý DBG ´òÓ¡¡£ */
        bool build(const juce::String& vsSource, const juce::String& fsSource)
        {
            program.release();

            if (!program.addVertexShader(vsSource))
            {
                DBG("VS error: " << program.getLastError());
                return false;
            }
            if (!program.addFragmentShader(fsSource))
            {
                DBG("FS error: " << program.getLastError());
                return false;
            }
            if (!program.link())
            {
                DBG("Link error: " << program.getLastError());
                return false;
            }
            return true;
        }

        void use() noexcept { program.use(); }

        juce::OpenGLShaderProgram& raw() noexcept { return program; }

        //----------------------------------------------------------------------
        // Uniform setters
        //----------------------------------------------------------------------
        void setMat3(const char* name, const float* m9) noexcept
        {
            const GLint loc = sc::gl::uniformLoc(program, name);
            if (loc >= 0)
                sc::gl::glUniformMatrix3fv(loc, 1, sc::gl::GL_FALSE, m9);
        }

        void setMat4(const char* name, const Mat4& m) noexcept
        {
            const GLint loc = sc::gl::uniformLoc(program, name);
            if (loc >= 0)
                sc::gl::glUniformMatrix4fv(loc, 1, sc::gl::GL_FALSE, m.mat);
        }

        void setVec3(const char* name, const Vec3& v) noexcept
        {
            const GLint loc = sc::gl::uniformLoc(program, name);
            if (loc >= 0)
                sc::gl::glUniform3f(loc, v.x, v.y, v.z);
        }

        void setFloat(const char* name, float f) noexcept
        {
            const GLint loc = sc::gl::uniformLoc(program, name);
            if (loc >= 0)
                sc::gl::glUniform1f(loc, f);
        }

        void setInt(const char* name, int i) noexcept
        {
            const GLint loc = sc::gl::uniformLoc(program, name);
            if (loc >= 0)
                sc::gl::glUniform1i(loc, i);
        }

    private:
        juce::OpenGLShaderProgram program;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(Shader)
    };
}
