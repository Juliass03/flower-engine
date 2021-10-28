#include "mesh.h"
#include "../asset_system/asset_system.h"
#include "../asset_system/asset_mesh.h"
#include "../core/file_system.h"
#include "material.h"
#include "../launch/launch_engine_loop.h"
#include <execution>
#include "../core/job_system.h"
#include "../vk/vk_rhi.h"

using namespace engine;

static AutoCVarInt32 cVarBaseVRamForVertexBuffer(
    "r.Vram.BaseVertexBuffer",
    "Persistent vram for vertex buffer(mb).",
    "Vram",
    128,
    CVarFlags::InitOnce | CVarFlags::ReadOnly
);

static AutoCVarInt32 cVarBaseVRamForIndexBuffer(
    "r.Vram.BaseIndexBuffer",
    "Persistent vram for index buffer(mb).",
    "Vram",
    128,
    CVarFlags::InitOnce | CVarFlags::ReadOnly
);

static AutoCVarInt32 cVarVertexBufferIncrementSize(
    "r.Vram.VertexBufferIncrementSize",
    "Increment vram size for vertex buffer(mb) when base vertex buffer no fit.",
    "Vram",
    16,
    CVarFlags::InitOnce | CVarFlags::ReadOnly
);

static AutoCVarInt32 cVarIndexBufferIncrementSize(
    "r.Vram.IndexBufferIncrementSize",
    "Increment vram size for index buffer(mb) when base index buffer no fit.",
    "Vram",
    16,
    CVarFlags::InitOnce | CVarFlags::ReadOnly
);

MeshLibrary* MeshLibrary::s_meshLibrary = new MeshLibrary();

void RenderBounds::toExtents(
    const RenderBounds& in,
    float& zmin,
    float& zmax,
    float& ymin,
    float& ymax,
    float& xmin,
    float& xmax,
    float scale)
{
    xmax = in.origin.x + in.extents.x * scale;
    xmin = in.origin.x - in.extents.x * scale;
    ymax = in.origin.y + in.extents.y * scale;
    ymin = in.origin.y - in.extents.y * scale;
    zmax = in.origin.z + in.extents.z * scale;
    zmin = in.origin.z - in.extents.z * scale;
}

RenderBounds RenderBounds::combine(const RenderBounds& b0,const RenderBounds& b1)
{
    float zmin0,zmin1,zmax0,zmax1,ymin0,ymin1,ymax0,ymax1,xmin0,xmin1,xmax0,xmax1;
    toExtents(b0,zmin0,zmax0,ymin0,ymax0,xmin0,xmax0);
    toExtents(b1,zmin1,zmax1,ymin1,ymax1,xmin1,xmax1);

    float xmax = std::max(xmax0,xmax1);
    float ymax = std::max(ymax0,ymax1);
    float zmax = std::max(zmax0,zmax1);
    float xmin = std::min(xmin0,xmin1);
    float ymin = std::min(ymin0,ymin1);
    float zmin = std::min(zmin0,zmin1);

    RenderBounds ret {};
    ret.origin.x = (xmax + xmin) * 0.5f;
    ret.origin.y = (ymax + ymin) * 0.5f;
    ret.origin.z = (zmax + zmin) * 0.5f;

    ret.extents.x = (xmax - xmin) * 0.5f;
    ret.extents.y = (ymax - ymin) * 0.5f;
    ret.extents.z = (zmax - zmin) * 0.5f;

    ret.radius = std::sqrt(ret.extents.x * ret.extents.x + ret.extents.y * ret.extents.y + ret.extents.z* ret.extents.z);
    return ret;
}

void engine::MeshLibrary::buildFromGameAsset(Mesh& inout,const std::string& gameName)
{
    using namespace asset_system;

    AssetFile asset {};
    loadBinFile(gameName.c_str(),asset);
    CHECK(asset.type[0] == 'M' && 
        asset.type[1] == 'E' && 
        asset.type[2] == 'S' && 
        asset.type[3] == 'H');

    std::vector<VertexIndexType> indicesData = {};
    std::vector<float> verticesData = {};

    MeshInfo info = asset_system::readMeshInfo(&asset);

    // 1. 填充indicesData和verticesData
    unpackMesh(&info,asset.binBlob.data(),asset.binBlob.size(),verticesData,indicesData);

    inout.layout = info.attributeLayout;
    inout.subMeshes.resize(info.subMeshCount);

    for (uint32 i = 0; i < info.subMeshCount; i++)
    {
        const auto& subMeshInfo = info.subMeshInfos[i];
        auto& subMesh = inout.subMeshes[i];

        subMesh.indexCount = subMeshInfo.indexCount;
        subMesh.indexStartPosition = subMeshInfo.indexStartPosition;
        subMesh.renderBounds.extents = glm::vec3(
            subMeshInfo.bounds.extents[0],
            subMeshInfo.bounds.extents[1],
            subMeshInfo.bounds.extents[2]
        );

        subMesh.renderBounds.origin = glm::vec3(
            subMeshInfo.bounds.origin[0],
            subMeshInfo.bounds.origin[1],
            subMeshInfo.bounds.origin[2]
        );

        subMesh.renderBounds.radius = subMeshInfo.bounds.radius;

        // NOTE: 这里先让材质库加载一次材质
        if(subMeshInfo.materialPath != "")
        {
            subMesh.cacheMaterial =  MaterialLibrary::get()->getMaterial(subMeshInfo.materialPath);
            subMesh.materialInfoPath = subMeshInfo.materialPath;
        }
        else
        {
            subMesh.cacheMaterial = &MaterialLibrary::get()->getCallbackMaterial();
            subMesh.materialInfoPath = "";
        }
    }

    // 2. 对顶点位置做处理
    uint32 lastMeshVerticesPos = (uint32)m_cacheVerticesData.size();
    uint32 lastMeshIndexPos = (uint32)m_cacheIndicesData.size();

    inout.vertexStartPosition = lastMeshVerticesPos;
    inout.vertexCount = (uint32)verticesData.size();

    inout.indexStartPosition = lastMeshIndexPos;
    inout.indexCount = (uint32)indicesData.size();

    uint32 lastMeshVertexIndex = lastMeshVerticesPos / getStandardMeshAttributesVertexCount();
    std::for_each(std::execution::par_unseq,indicesData.begin(),indicesData.end(), [lastMeshVertexIndex](auto&& index)
    {
        index = index + lastMeshVertexIndex;
    });

    // 3. 拼接到原本的网格上
    m_cacheVerticesData.insert(m_cacheVerticesData.end(),verticesData.begin(),verticesData.end());
    m_cacheIndicesData.insert(m_cacheIndicesData.end(),indicesData.begin(),indicesData.end());
}

Mesh& engine::MeshLibrary::getUnitBox()
{
    if(m_meshContainer.find(s_engineMeshBox) != m_meshContainer.end())
    {
        return *m_meshContainer[s_engineMeshBox];
    }
    else
    {
        Mesh* newMesh = new Mesh();
        m_meshContainer[s_engineMeshBox] = newMesh;
        buildFromGameAsset(*newMesh,s_engineMeshBox);
        return *newMesh;
    }
}

Mesh& engine::MeshLibrary::getMeshByName(const std::string& gameName)
{
    if(m_meshContainer.find(gameName) != m_meshContainer.end())
    {
        return *m_meshContainer[gameName];
    }
    else if(gameName=="" || gameName == toString(EPrimitiveMesh::Box))
    {
        return getUnitBox();
    }
    else
    {
        CHECK(FileSystem::endWith(gameName,".mesh"));

        Mesh* newMesh = new Mesh();
        m_meshContainer[gameName] = newMesh;
        buildFromGameAsset(*newMesh,gameName);

        return *newMesh;
    }
}

void engine::MeshLibrary::init()
{
    CHECK(m_indexBuffer == nullptr);
    CHECK(m_vertexBuffer == nullptr);

    VkDeviceSize baseVertexBufferSize = static_cast<VkDeviceSize>(cVarBaseVRamForVertexBuffer.get()) * 1024 * 1024;
    VkDeviceSize baseIndexBufferSize  = static_cast<VkDeviceSize>(cVarBaseVRamForIndexBuffer.get())  * 1024 * 1024;

    // NOTE: 实际上1024 * 1024 根本不够用
    m_cacheVerticesData.reserve(1024 * 1024);
    m_cacheIndicesData.reserve(1024 * 1024);

    m_indexBuffer = VulkanIndexBuffer::create(
        VulkanRHI::get()->getVulkanDevice(),
        baseIndexBufferSize,
        VulkanRHI::get()->getGraphicsCommandPool(),
        true,
        true
    );

    m_vertexBuffer = VulkanVertexBuffer::create(
        VulkanRHI::get()->getVulkanDevice(),
        VulkanRHI::get()->getGraphicsCommandPool(),
        baseVertexBufferSize,
		true,
		true
    );

    // NOTE: 先加载Box作为回滚网格
    getUnitBox();
}

void engine::MeshLibrary::release()
{
    for (auto& meshPair : m_meshContainer)
    {
        delete meshPair.second;
    }

    m_meshContainer.clear();

    if(m_indexBuffer)
    {
        delete m_indexBuffer;
        m_indexBuffer = nullptr;
    }
   
    if(m_vertexBuffer)
    {
        delete m_vertexBuffer;
        m_vertexBuffer = nullptr;
    }
}

void engine::MeshLibrary::bindVertexBuffer(VkCommandBuffer cmd)
{
    m_vertexBuffer->bind(cmd);
}

void engine::MeshLibrary::bindIndexBuffer(VkCommandBuffer cmd)
{
    m_indexBuffer->bind(cmd);
}

const std::unordered_set<std::string>& engine::MeshLibrary::getStaticMeshList() const
{ 
    return m_staticMeshList; 
}

void engine::MeshLibrary::emplaceStaticeMeshList(const std::string& name)
{
    m_staticMeshList.emplace(name); 
}

bool engine::MeshLibrary::MeshReady(const Mesh& in) const
{
    if(in.vertexCount <= 0) return false;
    if(in.indexCount <= 0) return false;

    if(in.vertexStartPosition < m_lastUploadVertexBufferPos &&
        in.indexStartPosition < m_lastUploadIndexBufferPos)
    {
        return true;
    }

    return false;
}

void engine::MeshLibrary::uploadAppendBuffer()
{
    VkDeviceSize incrementVertexBufferSize = static_cast<VkDeviceSize>(cVarVertexBufferIncrementSize.get()) * 1024 * 1024;
    VkDeviceSize incrementIndexBufferSize  = static_cast<VkDeviceSize>(cVarIndexBufferIncrementSize.get())  * 1024 * 1024;

    if(m_lastUploadVertexBufferPos != (uint32)m_cacheVerticesData.size())
    {
        uint32 upLoadStartPos = m_lastUploadVertexBufferPos;
        uint32 upLoadEndPos = (uint32)m_cacheVerticesData.size();
        uint64 perElemtSize = sizeof(m_cacheVerticesData[0]);
        m_lastUploadVertexBufferPos = upLoadEndPos;

        VkDeviceSize currentVertexDataDeviceSize = VkDeviceSize(perElemtSize * m_cacheVerticesData.size());
        VkDeviceSize lastDeviceSize = m_vertexBuffer->getVulkanBuffer()->getSize();

        // NOTE: 若超出了范围需要重新申请并刷新
        if(currentVertexDataDeviceSize >= lastDeviceSize)
        {
            VkDeviceSize newDeviceSize = lastDeviceSize + ((currentVertexDataDeviceSize / incrementVertexBufferSize) + 1) * incrementVertexBufferSize;


            LOG_INFO("Reallocate {0} mb local vram for vertex buffer!", newDeviceSize / (1024 * 1024));
           
            vkQueueWaitIdle(VulkanRHI::get()->getVulkanDevice()->graphicsQueue);
            delete m_vertexBuffer;

            m_vertexBuffer = VulkanVertexBuffer::create(
                VulkanRHI::get()->getVulkanDevice(),
                VulkanRHI::get()->getGraphicsCommandPool(),
                newDeviceSize,
                true,
                true // TODO: 未来我们将在ComputeShader中剔除每一个三角形！
            );

            upLoadStartPos = 0; // NOTE: 需要全部上传一次
        }

        CHECK(upLoadEndPos > upLoadStartPos);
        VkDeviceSize uploadDeviceSize = VkDeviceSize(perElemtSize * (upLoadEndPos - upLoadStartPos));
        VkDeviceSize offsetDeviceSize = VkDeviceSize(perElemtSize * upLoadStartPos);

        VulkanBuffer* stageBuffer = VulkanBuffer::create(
            VulkanRHI::get()->getVulkanDevice(),
            VulkanRHI::get()->getGraphicsCommandPool(),
            VK_BUFFER_USAGE_TRANSFER_SRC_BIT, 
            VMA_MEMORY_USAGE_CPU_TO_GPU,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
            uploadDeviceSize,
            (void *)(m_cacheVerticesData.data() + upLoadStartPos)
        );

        m_vertexBuffer->getVulkanBuffer()->stageCopyFrom(*stageBuffer, uploadDeviceSize,VulkanRHI::get()->getVulkanDevice()->graphicsQueue, 0, offsetDeviceSize);
        delete stageBuffer;
    }

    if(m_lastUploadIndexBufferPos != (uint32)m_cacheIndicesData.size())
    {
        uint32 upLoadStartPos = m_lastUploadIndexBufferPos;
        uint32 upLoadEndPos = (uint32)m_cacheIndicesData.size();
        m_lastUploadIndexBufferPos = upLoadEndPos;
        uint64 perElemtSize = sizeof(m_cacheIndicesData[0]);

        VkDeviceSize currentIndexDataDeviceSize = VkDeviceSize(perElemtSize * m_cacheIndicesData.size());
        VkDeviceSize lastDeviceSize = m_indexBuffer->getVulkanBuffer()->getSize();

        // NOTE: 若超出了范围需要重新申请并刷新
        if(currentIndexDataDeviceSize >= lastDeviceSize)
        {
            VkDeviceSize newDeviceSize = lastDeviceSize + ((currentIndexDataDeviceSize / incrementIndexBufferSize) + 1) * incrementIndexBufferSize;
            delete m_indexBuffer;

            LOG_INFO("Reallocate {0} mb local vram for index buffer!", newDeviceSize / (1024 * 1024));
            vkQueueWaitIdle(VulkanRHI::get()->getVulkanDevice()->graphicsQueue);
            m_indexBuffer = VulkanIndexBuffer::create(
                VulkanRHI::get()->getVulkanDevice(),
                newDeviceSize,
                VulkanRHI::get()->getGraphicsCommandPool(),
				true,
                true // TODO: 未来我们将在ComputeShader中剔除每一个三角形！
            );

            upLoadStartPos = 0; // NOTE: 需要全部上传一次
        }

        CHECK(upLoadEndPos > upLoadStartPos);
        VkDeviceSize uploadDeviceSize = VkDeviceSize(perElemtSize * (upLoadEndPos - upLoadStartPos));
        VkDeviceSize offsetDeviceSize = VkDeviceSize(perElemtSize * upLoadStartPos);

        VulkanBuffer* stageBuffer = VulkanBuffer::create(
            VulkanRHI::get()->getVulkanDevice(),
            VulkanRHI::get()->getGraphicsCommandPool(),
            VK_BUFFER_USAGE_TRANSFER_SRC_BIT, 
            VMA_MEMORY_USAGE_CPU_TO_GPU,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
            uploadDeviceSize,
            (void *)(m_cacheIndicesData.data() + upLoadStartPos)
        );

        m_indexBuffer->getVulkanBuffer()->stageCopyFrom(*stageBuffer, uploadDeviceSize,VulkanRHI::get()->getVulkanDevice()->graphicsQueue, 0, offsetDeviceSize);
        delete stageBuffer;
    }
}