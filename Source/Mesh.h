/*
==============================================================================
 Mesh.h
 Layer 2: Scene & Renderer
==============================================================================
*/
#pragma once

#include <JuceHeader.h>
#include <vector>
#include <memory>
#include "Vec.h"

namespace sc
{

    struct MeshVertex
    {
        float px, py, pz;
        float nx, ny, nz;
        float u, v;
    };

    class Mesh
    {
    public:
        Mesh() = default;
        ~Mesh() = default;

        //----------------------------------------------------------------------
        // 加载 / 程序化构造
        //----------------------------------------------------------------------
        bool loadFromObjMemory(const char* objText, size_t length);

        /** 从磁盘 .obj 文件加载。 */
        bool loadFromObjFile(const juce::File& file);

        /** 从 JUCE BinaryData / 内嵌资源加载。 */
        bool loadFromBinaryData(const void* data, size_t size);

        static std::unique_ptr<Mesh> createPlane(float sizeX, float sizeY);
        static std::unique_ptr<Mesh> createBox(float sizeX, float sizeY, float sizeZ);
        static std::unique_ptr<Mesh> createGrid(float halfRange, float step);
        static std::unique_ptr<Mesh> createCylinder(float radius, float height, int segments);

        //----------------------------------------------------------------------
        // GL
        //----------------------------------------------------------------------
        void uploadToGPU(juce::OpenGLContext& ctx);
        void releaseGPU(juce::OpenGLContext& ctx);
        void draw(juce::OpenGLContext& ctx);
        void drawRange(juce::OpenGLContext& ctx,
            GLsizei indexCount,
            GLintptr indexByteOffset);

        int  getVertexCount() const noexcept { return (int)vertices.size(); }
        int  getIndexCount()  const noexcept { return (int)indices.size(); }
        bool isUploaded()     const noexcept { return vao != 0; }

        /** GL_TRIANGLES（默认）或 GL_LINES（grid 用）。 */
        void   setPrimitive(GLenum prim) noexcept { primitive = prim; }
        GLenum getPrimitive() const noexcept { return primitive; }

        std::vector<MeshVertex>     vertices;
        std::vector<unsigned int>   indices;

    private:
        GLuint vao{ 0 };
        GLuint vbo{ 0 };
        GLuint ebo{ 0 };
        GLenum primitive{ juce::gl::GL_TRIANGLES };

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(Mesh)
    };

} // namespace sc
