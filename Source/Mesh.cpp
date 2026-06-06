/*
==============================================================================
 Mesh.cpp
 Layer 2: Scene & Renderer
==============================================================================
*/
#include "Mesh.h"
#include "GLUtils.h"

#define TINYOBJLOADER_IMPLEMENTATION
#include "tiny_obj_loader.h"

#include <unordered_map>
#include <sstream>
#include <cmath>

namespace sc
{

    //==========================================================================
    // OBJ 加载（行为与旧版本一致，仅迁入命名空间）
    //==========================================================================

    static inline std::string makeKey(int p, int n, int t)
    {
        return std::to_string(p) + "/" + std::to_string(n) + "/" + std::to_string(t);
    }

    bool Mesh::loadFromObjMemory(const char* objText, size_t length)
    {
        vertices.clear();
        indices.clear();

        tinyobj::attrib_t                attrib;
        std::vector<tinyobj::shape_t>    shapes;
        std::vector<tinyobj::material_t> materials;
        std::string warn, err;

        std::string        text(objText, length);
        std::istringstream iss(text);

        bool ok = tinyobj::LoadObj(&attrib, &shapes, &materials,
            &warn, &err, &iss, nullptr,
            /*triangulate=*/ true);

        if (!warn.empty()) DBG("tinyobj warn: " << warn);
        if (!err.empty())  DBG("tinyobj err : " << err);
        if (!ok) return false;

        std::unordered_map<std::string, unsigned int> uniqueMap;

        for (const auto& shape : shapes)
        {
            for (const auto& idx : shape.mesh.indices)
            {
                const auto key = makeKey(idx.vertex_index, idx.normal_index, idx.texcoord_index);
                auto it = uniqueMap.find(key);
                if (it != uniqueMap.end()) { indices.push_back(it->second); continue; }

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
                else { v.nx = 0.0f; v.ny = 0.0f; v.nz = 1.0f; }

                if (idx.texcoord_index >= 0)
                {
                    const int ti = idx.texcoord_index;
                    v.u = attrib.texcoords[2 * ti + 0];
                    v.v = attrib.texcoords[2 * ti + 1];
                }
                else { v.u = 0.0f; v.v = 0.0f; }

                const auto newIndex = (unsigned int)vertices.size();
                vertices.push_back(v);
                uniqueMap[key] = newIndex;
                indices.push_back(newIndex);
            }
        }
        // ============== 强制按面重算法线 ==============
// 先清零累加器
        for (auto& v : vertices)
        {
            v.nx = v.ny = v.nz = 0.0f;
        }

        // 累加面法线到每个顶点
        for (size_t i = 0; i + 2 < indices.size(); i += 3)
        {
            auto& a = vertices[indices[i + 0]];
            auto& b = vertices[indices[i + 1]];
            auto& c = vertices[indices[i + 2]];

            const float ex = b.px - a.px, ey = b.py - a.py, ez = b.pz - a.pz;
            const float fx = c.px - a.px, fy = c.py - a.py, fz = c.pz - a.pz;
            const float nx = ey * fz - ez * fy;
            const float ny = ez * fx - ex * fz;
            const float nz = ex * fy - ey * fx;

            a.nx += nx; a.ny += ny; a.nz += nz;
            b.nx += nx; b.ny += ny; b.nz += nz;
            c.nx += nx; c.ny += ny; c.nz += nz;
        }

        // 归一化
        for (auto& v : vertices)
        {
            const float l2 = v.nx * v.nx + v.ny * v.ny + v.nz * v.nz;
            if (l2 > 1e-12f)
            {
                const float inv = 1.0f / std::sqrt(l2);
                v.nx *= inv; v.ny *= inv; v.nz *= inv;
            }
            else
            {
                v.nx = 0.0f; v.ny = 0.0f; v.nz = 1.0f;
            }
        }


        return true;
    }

    //==========================================================================
    // 磁盘 / BinaryData 加载（新增）
    //==========================================================================

    bool Mesh::loadFromObjFile(const juce::File& file)
    {
        juce::MemoryBlock mb;
        if (!file.loadFileAsData(mb))
        {
            DBG("Mesh: failed to read file: " << file.getFullPathName());
            return false;
        }
        return loadFromObjMemory(
            static_cast<const char*>(mb.getData()),
            mb.getSize());

    }

    bool Mesh::loadFromBinaryData(const void* data, size_t size)
    {
        return loadFromObjMemory(
            static_cast<const char*>(data),
            size);
    }

    //==========================================================================
    // 程序化几何
    //==========================================================================

    std::unique_ptr<Mesh> Mesh::createPlane(float sizeX, float sizeY)
    {
        auto m = std::make_unique<Mesh>();
        const float hx = sizeX * 0.5f;
        const float hy = sizeY * 0.5f;

        m->vertices = {
            { -hx, -hy, 0, 0, 0, 1, 0, 0 },
            {  hx, -hy, 0, 0, 0, 1, 1, 0 },
            {  hx,  hy, 0, 0, 0, 1, 1, 1 },
            { -hx,  hy, 0, 0, 0, 1, 0, 1 }
        };
        m->indices = { 0, 1, 2, 0, 2, 3 };
        return m;
    }

    std::unique_ptr<Mesh> Mesh::createBox(float sx, float sy, float sz)
    {
        auto m = std::make_unique<Mesh>();
        const float hx = sx * 0.5f;
        const float hy = sy * 0.5f;
        const float hz = sz * 0.5f;

        struct Face { Vec3 n; Vec3 p[4]; };
        const Face faces[6] = {
            { { 1, 0, 0 }, { { hx, -hy, -hz }, { hx,  hy, -hz }, { hx,  hy,  hz }, { hx, -hy,  hz } } },
            { {-1, 0, 0 }, { {-hx,  hy, -hz }, {-hx, -hy, -hz }, {-hx, -hy,  hz }, {-hx,  hy,  hz } } },
            { { 0, 1, 0 }, { { hx,  hy, -hz }, {-hx,  hy, -hz }, {-hx,  hy,  hz }, { hx,  hy,  hz } } },
            { { 0,-1, 0 }, { {-hx, -hy, -hz }, { hx, -hy, -hz }, { hx, -hy,  hz }, {-hx, -hy,  hz } } },
            { { 0, 0, 1 }, { {-hx, -hy,  hz }, { hx, -hy,  hz }, { hx,  hy,  hz }, {-hx,  hy,  hz } } },
            { { 0, 0,-1 }, { {-hx,  hy, -hz }, { hx,  hy, -hz }, { hx, -hy, -hz }, {-hx, -hy, -hz } } }
        };

        const float uvs[4][2] = { {0,0}, {1,0}, {1,1}, {0,1} };

        for (int f = 0; f < 6; ++f)
        {
            const auto base = (unsigned int)m->vertices.size();
            for (int k = 0; k < 4; ++k)
            {
                MeshVertex v{};
                v.px = faces[f].p[k].x; v.py = faces[f].p[k].y; v.pz = faces[f].p[k].z;
                v.nx = faces[f].n.x;    v.ny = faces[f].n.y;    v.nz = faces[f].n.z;
                v.u = uvs[k][0];       v.v = uvs[k][1];
                m->vertices.push_back(v);
            }
            m->indices.insert(m->indices.end(), { base, base + 1, base + 2, base, base + 2, base + 3 });
        }
        return m;
    }

    std::unique_ptr<Mesh> Mesh::createGrid(float halfRange, float step)
    {
        auto m = std::make_unique<Mesh>();
        m->setPrimitive(juce::gl::GL_LINES);

        auto pushLineVertex = [&](float x, float y)
            {
                MeshVertex v{};
                v.px = x; v.py = y; v.pz = 0.0f;
                v.nx = 0.0f; v.ny = 0.0f; v.nz = 1.0f;
                v.u = 0.0f; v.v = 0.0f;
                m->vertices.push_back(v);
            };

        for (float x = -halfRange; x <= halfRange + 0.5f * step; x += step)
        {
            const auto base = (unsigned int)m->vertices.size();
            pushLineVertex(x, -halfRange);
            pushLineVertex(x, halfRange);
            m->indices.push_back(base);
            m->indices.push_back(base + 1);
        }

        for (float y = -halfRange; y <= halfRange + 0.5f * step; y += step)
        {
            const auto base = (unsigned int)m->vertices.size();
            pushLineVertex(-halfRange, y);
            pushLineVertex(halfRange, y);
            m->indices.push_back(base);
            m->indices.push_back(base + 1);
        }

        return m;
    }

    std::unique_ptr<Mesh> Mesh::createCylinder(float radius, float height, int segments)
    {
        segments = juce::jmax(3, segments);
        auto m = std::make_unique<Mesh>();
        const float twoPi = juce::MathConstants<float>::twoPi;

        for (int i = 0; i <= segments; ++i)
        {
            const float t = (float)i / (float)segments;
            const float a = t * twoPi;
            const float ca = std::cos(a);
            const float sa = std::sin(a);

            MeshVertex vb{}; vb.px = radius * ca; vb.py = radius * sa; vb.pz = 0.0f;
            vb.nx = ca; vb.ny = sa; vb.nz = 0.0f; vb.u = t; vb.v = 0.0f;
            m->vertices.push_back(vb);

            MeshVertex vt{}; vt.px = radius * ca; vt.py = radius * sa; vt.pz = height;
            vt.nx = ca; vt.ny = sa; vt.nz = 0.0f; vt.u = t; vt.v = 1.0f;
            m->vertices.push_back(vt);
        }

        for (int i = 0; i < segments; ++i)
        {
            const unsigned int b0 = (unsigned int)(i * 2);
            const unsigned int t0 = b0 + 1;
            const unsigned int b1 = b0 + 2;
            const unsigned int t1 = b0 + 3;
            m->indices.insert(m->indices.end(), { b0, b1, t1, b0, t1, t0 });
        }

        const auto sideCount = (unsigned int)m->vertices.size();

        MeshVertex cb{}; cb.px = 0; cb.py = 0; cb.pz = 0; cb.nx = 0; cb.ny = 0; cb.nz = -1;
        m->vertices.push_back(cb);
        MeshVertex ct{}; ct.px = 0; ct.py = 0; ct.pz = height; ct.nx = 0; ct.ny = 0; ct.nz = 1;
        m->vertices.push_back(ct);

        const unsigned int bottomCenter = sideCount;
        const unsigned int topCenter = sideCount + 1;

        for (int i = 0; i < segments; ++i)
        {
            const float a0 = (float)i / (float)segments * twoPi;
            const float a1 = (float)(i + 1) / (float)segments * twoPi;

            MeshVertex b0{}; b0.px = radius * std::cos(a0); b0.py = radius * std::sin(a0); b0.pz = 0;
            b0.nx = 0; b0.ny = 0; b0.nz = -1;
            MeshVertex b1{}; b1.px = radius * std::cos(a1); b1.py = radius * std::sin(a1); b1.pz = 0;
            b1.nx = 0; b1.ny = 0; b1.nz = -1;
            const auto bi0 = (unsigned int)m->vertices.size(); m->vertices.push_back(b0);
            const auto bi1 = (unsigned int)m->vertices.size(); m->vertices.push_back(b1);
            m->indices.insert(m->indices.end(), { bottomCenter, bi1, bi0 });

            MeshVertex t0v{}; t0v.px = radius * std::cos(a0); t0v.py = radius * std::sin(a0); t0v.pz = height;
            t0v.nx = 0; t0v.ny = 0; t0v.nz = 1;
            MeshVertex t1v{}; t1v.px = radius * std::cos(a1); t1v.py = radius * std::sin(a1); t1v.pz = height;
            t1v.nx = 0; t1v.ny = 0; t1v.nz = 1;
            const auto ti0 = (unsigned int)m->vertices.size(); m->vertices.push_back(t0v);
            const auto ti1 = (unsigned int)m->vertices.size(); m->vertices.push_back(t1v);
            m->indices.insert(m->indices.end(), { topCenter, ti0, ti1 });
        }

        return m;
    }

    //==========================================================================
    // GL 资源
    //==========================================================================

    void Mesh::uploadToGPU(juce::OpenGLContext& ctx)
    {
        auto& ext = ctx.extensions;
        ext.glGenVertexArrays(1, &vao);
        ext.glBindVertexArray(vao);

        ext.glGenBuffers(1, &vbo);
        ext.glBindBuffer(juce::gl::GL_ARRAY_BUFFER, vbo);
        ext.glBufferData(juce::gl::GL_ARRAY_BUFFER,
            (GLsizeiptr)(vertices.size() * sizeof(MeshVertex)),
            vertices.data(),
            juce::gl::GL_STATIC_DRAW);

        ext.glGenBuffers(1, &ebo);
        ext.glBindBuffer(juce::gl::GL_ELEMENT_ARRAY_BUFFER, ebo);
        ext.glBufferData(juce::gl::GL_ELEMENT_ARRAY_BUFFER,
            (GLsizeiptr)(indices.size() * sizeof(unsigned int)),
            indices.data(),
            juce::gl::GL_STATIC_DRAW);

        ext.glEnableVertexAttribArray(0);
        ext.glVertexAttribPointer(0, 3, juce::gl::GL_FLOAT, juce::gl::GL_FALSE,
            sizeof(MeshVertex), (void*)offsetof(MeshVertex, px));
        ext.glEnableVertexAttribArray(1);
        ext.glVertexAttribPointer(1, 3, juce::gl::GL_FLOAT, juce::gl::GL_FALSE,
            sizeof(MeshVertex), (void*)offsetof(MeshVertex, nx));
        ext.glEnableVertexAttribArray(2);
        ext.glVertexAttribPointer(2, 2, juce::gl::GL_FLOAT, juce::gl::GL_FALSE,
            sizeof(MeshVertex), (void*)offsetof(MeshVertex, u));

        ext.glBindVertexArray(0);
    }

    void Mesh::releaseGPU(juce::OpenGLContext& ctx)
    {
        auto& ext = ctx.extensions;
        if (ebo) { ext.glDeleteBuffers(1, &ebo); ebo = 0; }
        if (vbo) { ext.glDeleteBuffers(1, &vbo); vbo = 0; }
        if (vao) { ext.glDeleteVertexArrays(1, &vao); vao = 0; }
    }

    void Mesh::draw(juce::OpenGLContext& ctx)
    {
        if (!isUploaded()) return;
        auto& ext = ctx.extensions;
        ext.glBindVertexArray(vao);
        juce::gl::glDrawElements(primitive,
            (GLsizei)indices.size(),
            juce::gl::GL_UNSIGNED_INT, nullptr);
        ext.glBindVertexArray(0);
    }

} // namespace sc
