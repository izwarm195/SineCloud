/*
  ==============================================================================
    GLUtils.h
    Layer 2: Scene & Renderer
    GL 函数空间 / 错误检查 / 小工具。所有渲染层文件都通过这里访问 juce::gl::*。
  ==============================================================================
*/
#pragma once

#include <JuceHeader.h>

namespace sc::gl
{
    using namespace juce::gl;

    /** Debug 用：检查并打印 GL 错误（非致命），仅在 JUCE_DEBUG 下开启。 */
    inline void checkError(const char* tag) noexcept
    {
#if JUCE_DEBUG
        const GLenum e = juce::gl::glGetError();
        if (e != juce::gl::GL_NO_ERROR)
            DBG("GL error [" << tag << "]: 0x" << juce::String::toHexString((int)e));
#else
        juce::ignoreUnused(tag);
#endif
    }

    /** 一行式获取 uniform location，找不到时返回 -1（GL 规范行为）。 */
    inline GLint uniformLoc(juce::OpenGLShaderProgram& sh, const char* name) noexcept
    {
        return juce::gl::glGetUniformLocation(sh.getProgramID(), name);
    }
}
