/*
  ==============================================================================

    Mesh,h.h
    Created: 29 May 2026 12:47:33pm
    Author:  wzm

  ==============================================================================
*/

#pragma once
#include <JuceHeader.h>
#include <vector>
#include <cstring>

// °Ñ tinyobjloader µÄ IMPLEMENTATION ·ÅÔÚ .cpp Àï£¬ÕâÀïÖ»ÉùÃ÷
// µ«ÒòÎª tinyobjloader ÊÇ single-header£¬ÎÒÃÇÒ»»á¶ùÔÚ Mesh.cpp Àï #define Ëü
struct tinyobj_dummy_forward {};

//==============================================================================
struct MeshVertex
{
    float px, py, pz;
    float nx, ny, nz;
    float u, v;
};

//==============================================================================
// Ò»¸ö¾²Ì¬ mesh£º´Ó .obj ¼ÓÔØ¶¥µãÊý¾Ý£¬ÉÏ´«µ½ GPU£¬°´Ðè»æÖÆ
//==============================================================================
class Mesh
{
public:
    Mesh() = default;
    ~Mesh() = default;

    // ´ÓÄÚ´æÖÐµÄ obj ÎÄ±¾¼ÓÔØ£¨ÓÃÓÚ BinaryData£©
    bool loadFromObjMemory(const char* objText, size_t length);

    // ÔÚ GL ÉÏÏÂÎÄÀïÉÏ´« / ÊÍ·Å / »æÖÆ
    void uploadToGPU(juce::OpenGLContext& ctx);
    void releaseGPU(juce::OpenGLContext& ctx);
    void draw(juce::OpenGLContext& ctx);

    int getVertexCount() const noexcept { return (int)vertices.size(); }
    int getIndexCount()  const noexcept { return (int)indices.size(); }
    bool isUploaded()    const noexcept { return vao != 0; }

private:
    std::vector<MeshVertex>     vertices;
    std::vector<unsigned int>   indices;

    GLuint vao{ 0 };
    GLuint vbo{ 0 };
    GLuint ebo{ 0 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(Mesh)
};
