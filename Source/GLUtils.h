/*
  ==============================================================================
    GLUtils.h
    Layer 2: Scene & Renderer
    GL º¯Êý¿Õ¼ä / ´íÎó¼ì²é / Ð¡¹¤¾ß¡£ËùÓÐäÖÈ¾²ãÎÄ¼þ¶¼Í¨¹ýÕâÀï·ÃÎÊ juce::gl::*¡£
  ==============================================================================
*/
#pragma once

#include <JuceHeader.h>

namespace sc::gl
{
    using namespace juce::gl;

    /** Debug ÓÃ£º¼ì²é²¢´òÓ¡ GL ´íÎó£¨·ÇÖÂÃü£©£¬½öÔÚ JUCE_DEBUG ÏÂ¿ªÆô¡£ */
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

    /** Ò»ÐÐÊ½»ñÈ¡ uniform location£¬ÕÒ²»µ½Ê±·µ»Ø -1£¨GL ¹æ·¶ÐÐÎª£©¡£ */
    inline GLint uniformLoc(juce::OpenGLShaderProgram& sh, const char* name) noexcept
    {
        return juce::gl::glGetUniformLocation(sh.getProgramID(), name);
    }
}
