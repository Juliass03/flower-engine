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
    glm::vec3 origin; // ����
    glm::vec3 extents; // �ǵ�

    float radius; // ��Χ��뾶

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
    // TODO: ����Memory Allocation + Placement New�ķ�ʽ�����ڴ�������
    using MeshContainer = std::unordered_map<std::string,Mesh*>;
    friend asset_system::AssetSystem;

private:
    static MeshLibrary* s_meshLibrary;

    std::unordered_set<std::string> m_staticMeshList;
    MeshContainer m_meshContainer;

    // NOTE: ���������е����񶥵�����
    //       ��ǰ�Ĳ���Ϊһ�����ؾͲ����ͷ�ֱ��������ȷ�˳���
    // TODO: �������Ҫʱ�ٻ�����ʽ����
    std::vector<VertexIndexType> m_cacheIndicesData = {};
    std::vector<float> m_cacheVerticesData = {};

    VulkanIndexBuffer* m_indexBuffer = nullptr;
    VulkanVertexBuffer* m_vertexBuffer = nullptr;

private:
    void buildFromGameAsset(Mesh& inout,const std::string& gameName);

private: // upload gpu
    uint32 m_lastUploadVertexBufferPos = 0;
    uint32 m_lastUploadIndexBufferPos = 0;

    // ÿ֡Tickʱ��AssetSystem����
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

    // NOTE: ���������ϴ���ҪvkwaitQueueIdle
    //       ���ƺ��ᵼ��Graphics�������Memory����Ȩ��ʧ������ΪĬ�ϵ��ڴ�״̬
    //       �ñ���ָʾ���´��첽���л�ȡ����Ȩ�Ƿ���Ҫ����
    // TODO: ʹ��һ��ȫ�ֵ�״̬
    bool bMeshReload = false;
};

}