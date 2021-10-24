#include "mesh.h"
#include "../asset_system/asset_system.h"
#include "../asset_system/asset_mesh.h"
#include "mesh/primitive.h"
#include "../core/file_system.h"
#include "material.h"
#include "../launch/launch_engine_loop.h"

using namespace engine;

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

void engine::Mesh::release()
{
    if(indexBuffer)
    {
        delete indexBuffer;
        indexBuffer = nullptr;
    }

    if(vertexBuffer)
    {
        delete vertexBuffer;
        vertexBuffer = nullptr;
    }
}

void engine::Mesh::buildFromGameAsset(const std::string& gameName)
{
    using namespace asset_system;
    // AssetSystem* assetSystem = g_engineLoop.getEngine()->getRuntimeModule<AssetSystem>();

    AssetFile asset {};
    loadBinFile(gameName.c_str(),asset);
    CHECK(asset.type[0] == 'M' && 
          asset.type[1] == 'E' && 
          asset.type[2] == 'S' && 
          asset.type[3] == 'H');
    
    std::vector<VertexIndexType> indicesData = {};
    std::vector<float> verticesData = {};

    MeshInfo info = asset_system::readMeshInfo(&asset);

    // 1. ���indicesData��verticesData
    unpackMesh(&info,asset.binBlob.data(),asset.binBlob.size(),verticesData,indicesData);

    layout = info.attributeLayout;
    subMeshes.resize(info.subMeshCount);

    uint32 index_startPoint = 0;
    uint32 vertex_startPoint = 0;
    for (uint32 i = 0; i < info.subMeshCount; i++)
    {
        const auto& subMeshInfo = info.subMeshInfos[i];
        auto& subMesh = subMeshes[i];

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

        // NOTE: �������ò��ʿ����һ�β���
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

    indexBuffer = VulkanRHI::get()->createIndexBuffer(indicesData);
    vertexBuffer = VulkanRHI::get()->createVertexBuffer(verticesData,layout);
}

Mesh& engine::MeshLibrary::getUnitQuad()
{
    auto quadName = toString(EPrimitiveMesh::Quad);
    if(m_meshContainer.find(quadName) != m_meshContainer.end())
    {
        return *m_meshContainer[quadName];
    }
    else
    {
        Mesh* newMesh = new Mesh();
        m_meshContainer[quadName] = newMesh;
        buildQuad(*newMesh);
        return *newMesh;
    }
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
        newMesh->buildFromGameAsset(s_engineMeshBox);
        return *newMesh;
    }
}

Mesh& engine::MeshLibrary::getMeshByName(const std::string& gameName)
{
    if(m_meshContainer.find(gameName) != m_meshContainer.end())
    {
        return *m_meshContainer[gameName];
    }
    else if(gameName==toString(EPrimitiveMesh::Quad))
    {
        return getUnitQuad();
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
        newMesh->buildFromGameAsset(gameName);

        return *newMesh;
    }
}

void engine::MeshLibrary::release()
{
    for (auto& meshPair : m_meshContainer)
    {
        meshPair.second->release();
        delete meshPair.second;
    }

    m_meshContainer.clear();
}

const std::unordered_set<std::string>& engine::MeshLibrary::getStaticMeshList() const
{ 
    return m_staticMeshList; 
}

void engine::MeshLibrary::emplaceStaticeMeshList(const std::string& name)
{
    m_staticMeshList.emplace(name); 
}
