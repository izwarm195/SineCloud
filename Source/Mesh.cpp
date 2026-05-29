/*
  ==============================================================================

    Mesh.cpp
    Created: 29 May 2026 12:48:03pm
    Author:  wzm

  ==============================================================================
*/

#include "Mesh.h"

// tinyobjloader£ºÔÚÕâÒ»¸ö .cpp Àï define IMPLEMENTATION
#define TINYOBJLOADER_IMPLEMENTATION
#include "tiny_obj_loader.h"

#include <sstream>
#include <unordered_map>

//==============================================================================
// ÓÃ pos/normal/uv ÈýÔª×éµÄ×Ö·û´®×÷ key£¬È¥ÖØÉú³Éµ¥Ò» index buffer
// ÕâÊÇ .obj ×ª OpenGL ¶¥µã¸ñÊ½µÄ±ê×¼×ö·¨
static inline std::string makeKey(int p, int n, int t)
{
    return std::to_string(p) + "/" + std::to_string(n) + "/" + std::to_string(t);
}

bool Mesh::loadFromObjMemory(const char* objText, size_t length)
{
    vertices.clear();
    indices.clear();

    tinyobj::attrib_t attrib;
    std::vector<tinyobj::shape_t>    shapes;
    std::vector<tinyobj::material_t> materials;
    std::string warn, err;

    // tinyobj Ã»ÓÐ"´ÓÄÚ´æ¼ÓÔØ"µÄ¹Ù·½±ãÀû API£¬ÓÃ stringstream °üÒ»²ã
    std::string text(objText, length);
    std::istringstream iss(text);

    // MaterialReader ¸ø nullptr£¬ÒòÎªÎÒÃÇ²»¶Á .mtl
    bool ok = tinyobj::LoadObj(&attrib, &shapes, &materials,
        &warn, &err, &iss, nullptr,
        /*triangulate=*/ true);

    if (!warn.empty()) DBG("tinyobj warn: " << warn);
    if (!err.empty()) DBG("tinyobj err : " << err);
    if (!ok)           return false;

    std::unordered_map<std::string, unsigned int> uniqueMap;

    for (const auto& shape : shapes)
    {
        for (const auto& idx : shape.mesh.indices)
        {
            const auto key = makeKey(idx.vertex_index, idx.normal_index, idx.texcoord_index);

            auto it = uniqueMap.find(key);
            if (it != uniqueMap.end())
            {
                indices.push_back(it->second);
                continue;
            }

            MeshVertex v{};

            const int pi = idx.vertex_index;
            v.px = attrib.vertices[3 * pi + 0];
            v.py = attrib.vertices[3 * pi + 1];
            v.pz = attrib.vertices[3 * pi + 2];

            if (idx.normal_index >= 0)
            {
                const int ni = idx.normal_index;
                v.nx = attrib.normals[3 * ni + 0];
                v.ny = attrib.normals[3 * ni + 1];
                v.nz = attrib.normals[3 * ni + 2];
            }
            else
            {
                v.nx = 0.0f; v.ny = 0.0f; v.nz = 1.0f;
            }

            if (idx.texcoord_index >= 0)
            {
                const int ti = idx.texcoord_index;
                v.u = attrib.texcoords[2 * ti + 0];
                v.v = attrib.texcoords[2 * ti + 1];
            }
            else
            {
                v.u = 0.0f; v.v = 0.0f;
            }

            const auto newIndex = (unsigned int)vertices.size();
            vertices.push_back(v);
            uniqueMap[key] = newIndex;
            indices.push_back(newIndex);
        }
    }

    DBG("Mesh loaded: " << (int)vertices.size() << " vertices, "
        << (int)indices.size() << " indices");
    return true;
}

//==============================================================================
void Mesh::uploadToGPU(juce::OpenGLContext& ctx)
{
    auto& gl = ctx.extensions;

    gl.glGenVertexArrays(1, &vao);
    gl.glBindVertexArray(vao);

    gl.glGenBuffers(1, &vbo);
    gl.glBindBuffer(juce::gl::GL_ARRAY_BUFFER, vbo);
    gl.glBufferData(juce::gl::GL_ARRAY_BUFFER,
        (GLsizeiptr)(vertices.size() * sizeof(MeshVertex)),
        vertices.data(),
        juce::gl::GL_STATIC_DRAW);

    gl.glGenBuffers(1, &ebo);
    gl.glBindBuffer(juce::gl::GL_ELEMENT_ARRAY_BUFFER, ebo);
    gl.glBufferData(juce::gl::GL_ELEMENT_ARRAY_BUFFER,
        (GLsizeiptr)(indices.size() * sizeof(unsigned int)),
        indices.data(),
        juce::gl::GL_STATIC_DRAW);

    // layout(location=0) vec3 aPos
    gl.glEnableVertexAttribArray(0);
    gl.glVertexAttribPointer(0, 3, juce::gl::GL_FLOAT, juce::gl::GL_FALSE,
        sizeof(MeshVertex),
        (void*)offsetof(MeshVertex, px));

    // layout(location=1) vec3 aNormal
    gl.glEnableVertexAttribArray(1);
    gl.glVertexAttribPointer(1, 3, juce::gl::GL_FLOAT, juce::gl::GL_FALSE,
        sizeof(MeshVertex),
        (void*)offsetof(MeshVertex, nx));

    // layout(location=2) vec2 aUV
    gl.glEnableVertexAttribArray(2);
    gl.glVertexAttribPointer(2, 2, juce::gl::GL_FLOAT, juce::gl::GL_FALSE,
        sizeof(MeshVertex),
        (void*)offsetof(MeshVertex, u));

    gl.glBindVertexArray(0);
}

void Mesh::releaseGPU(juce::OpenGLContext& ctx)
{
    auto& gl = ctx.extensions;
    if (ebo) { gl.glDeleteBuffers(1, &ebo); ebo = 0; }
    if (vbo) { gl.glDeleteBuffers(1, &vbo); vbo = 0; }
    if (vao) { gl.glDeleteVertexArrays(1, &vao); vao = 0; }
}

void Mesh::draw(juce::OpenGLContext& ctx)
{
    if (!isUploaded()) return;
    auto& gl = ctx.extensions;

    gl.glBindVertexArray(vao);
    juce::gl::glDrawElements(juce::gl::GL_TRIANGLES,
        (GLsizei)indices.size(),
        juce::gl::GL_UNSIGNED_INT,
        nullptr);
    gl.glBindVertexArray(0);
}
