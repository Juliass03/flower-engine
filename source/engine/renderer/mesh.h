#pragma once
#include "../core/core.h"
#include "../vk/vk_rhi.h"

namespace engine{

struct RenderBounds
{
    glm::vec3 origin; // 中心
    glm::vec3 extents; // 角点

    float radius; // 包围球半径

    static void toExtents(const RenderBounds& in,float& zmin,float& zmax,float& ymin,float& ymax,float& xmin,float& xmax,float scale = 1.5f);
    static RenderBounds combine(const RenderBounds& b0,const RenderBounds& b1);
};

class Material;

struct SubMesh
{
    RenderBounds renderBounds;
    
    uint32 indexStartPosition;
    uint32 indexCount;
    Ref<Material> cacheMaterial = nullptr;
    std::string materialInfoPath;
};

inline std::vector<EVertexAttribute> getStandardMeshAttributes()
{
    return std::vector<EVertexAttribute>{
        EVertexAttribute::pos,
        EVertexAttribute::uv0,
        EVertexAttribute::normal,
        EVertexAttribute::tangent
    };
}

enum class EPrimitiveMesh
{
    Min = 0,

    Quad,
    Box,
    Custom,
    Max,
};

inline std::string toString(EPrimitiveMesh type)
{
    switch(type)
    {
    case engine::EPrimitiveMesh::Quad:
        return "Quad";
    case engine::EPrimitiveMesh::Box:
        return "Box";
    case engine::EPrimitiveMesh::Custom:
        return "Custom";
    }


    LOG_FATAL("Unknow type!");
    return "";
}

inline void loopPrimitiveType(std::function<void(std::string,EPrimitiveMesh)>&& lambda)
{
    size_t begin = size_t(EPrimitiveMesh::Min) + 1;
    size_t end = size_t(EPrimitiveMesh::Max);

    for(size_t i = begin; i<end; i++)
    {
        lambda(toString(EPrimitiveMesh(i)),EPrimitiveMesh(i));
    }
}

struct Mesh
{
    std::vector<SubMesh> subMeshes;
    std::vector<EVertexAttribute> layout;

    VulkanIndexBuffer* indexBuffer = nullptr;
    VulkanVertexBuffer* vertexBuffer = nullptr;

    void release();
    void buildFromGameAsset(const std::string& gameName);
};

struct RenderMesh
{
    Ref<Mesh> mesh;

    glm::mat4 modelMatrix;
};

// NOTE: MeshLibrary负责管理所有的静态网格加载。
//       当前的策略为一旦加载就不再释放直到引擎明确退出。
// TODO: 增加引用计数，在没有引用且显存占用达到一定阈值时销毁Mesh
class MeshLibrary
{
    using MeshContainer = std::unordered_map<std::string,Mesh*>;

public:
    Mesh& getUnitQuad();
    Mesh& getUnitBox();

    Mesh& getMeshByName(const std::string& gameName);
    void release();
    static MeshLibrary* get() { return s_meshLibrary; }

private:
    static MeshLibrary* s_meshLibrary;
    MeshContainer m_meshContainer;
};

}