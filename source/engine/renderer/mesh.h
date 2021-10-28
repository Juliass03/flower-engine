#pragma once
#include "../core/core.h"
#include "../vk/vk_rhi.h"
#include <unordered_set>
namespace engine{

namespace asset_system
{
    class AssetSystem;
}

struct Material;

struct RenderBounds
{
    glm::vec3 origin; // 中心
    glm::vec3 extents; // 角点

    float radius; // 包围球半径

    static void toExtents(const RenderBounds& in,float& zmin,float& zmax,float& ymin,float& ymax,float& xmin,float& xmax,float scale = 1.5f);
    static RenderBounds combine(const RenderBounds& b0,const RenderBounds& b1);
};

struct SubMesh
{
    RenderBounds renderBounds;

    uint32 indexStartPosition;
    uint32 indexCount;

    Ref<Material> cacheMaterial = nullptr;
    std::string materialInfoPath;
};

struct RenderSubMesh
{
    RenderBounds renderBounds;

    uint32 indexStartPosition;
    uint32 indexCount;

    Ref<Material> cacheMaterial = nullptr;
    bool bCullingResult = true;

    glm::mat4 modelMatrix;
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

inline uint32 getStandardMeshAttributesVertexCount()
{
    return 3 + 2 + 3 + 4;
}

enum class EPrimitiveMesh
{
    Min = 0,

    Box,
    Custom,

    Max,
};

inline std::string toString(EPrimitiveMesh type)
{
    switch(type)
    {
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

    uint32 vertexStartPosition;
    uint32 vertexCount;

    uint32 indexStartPosition;
    uint32 indexCount;
};

struct RenderMeshPack
{
    std::vector<RenderSubMesh> submesh;
};

class MeshLibrary
{
    // TODO: 缓存Memory Allocation + Placement New的方式增加内存连续性
    using MeshContainer = std::unordered_map<std::string,Mesh*>;
    friend asset_system::AssetSystem;

private:
    static MeshLibrary* s_meshLibrary;

    std::unordered_set<std::string> m_staticMeshList;
    MeshContainer m_meshContainer;

    // NOTE: 缓存了所有的网格顶点数据
    //       当前的策略为一旦加载就不再释放直到引擎明确退出。
    // TODO: 如果有需要时再换成流式加载
    std::vector<VertexIndexType> m_cacheIndicesData = {};
    std::vector<float> m_cacheVerticesData = {};

    VulkanIndexBuffer* m_indexBuffer = nullptr;
    VulkanVertexBuffer* m_vertexBuffer = nullptr;

private:
    void buildFromGameAsset(Mesh& inout,const std::string& gameName);

private: // upload gpu
    uint32 m_lastUploadVertexBufferPos = 0;
    uint32 m_lastUploadIndexBufferPos = 0;

    // 每帧Tick时在AssetSystem调用
    void uploadAppendBuffer();

public:
    Mesh& getUnitBox();

    Mesh& getMeshByName(const std::string& gameName);

    void init();
    void release();

    static MeshLibrary* get() { return s_meshLibrary; }
    void bindVertexBuffer(VkCommandBuffer cmd);
    void bindIndexBuffer(VkCommandBuffer cmd);

    const std::unordered_set<std::string>& getStaticMeshList() const;
    void emplaceStaticeMeshList(const std::string& name);

    bool MeshReady(const Mesh& in) const;

    // NOTE: 网格重新上传需要vkwaitQueueIdle
    //       这似乎会导致Graphics队列族的Memory所有权丢失掉，成为默认的内存状态
    //       该变量指示了下次异步队列获取所有权是否需要屏障
    // TODO: 使用一个全局的状态
    bool bMeshReload = false;
};

}