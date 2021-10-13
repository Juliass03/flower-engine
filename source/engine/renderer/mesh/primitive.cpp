#include "primitive.h"

using namespace engine;

void engine::buildQuad(Mesh& inout)
{
    std::vector<float> verticesData = {
        -0.5f, -0.5f, 0.0f,  0.0f, 0.0f,  0.0f, 0.0f, 1.0f, 0.0f,1.0f,0.0f,0.0f,
         0.5f, -0.5f, 0.0f,  1.0f, 0.0f,  0.0f, 0.0f, 1.0f, 0.0f,1.0f,0.0f,0.0f,
         0.5f,  0.5f, 0.0f,  1.0f, 1.0f,  0.0f, 0.0f, 1.0f, 0.0f,1.0f,0.0f,0.0f,
        -0.5f,  0.5f, 0.0f,  0.0f, 1.0f,  0.0f, 0.0f, 1.0f, 0.0f,1.0f,0.0f,0.0f
    };

    std::vector<VertexIndexType> indicesData = {
        0, 1, 2, 
        2, 3, 0
    };

    RenderBounds renderBounds = {};
    renderBounds.origin  = { 0.0f,0.0f,0.0f };
    renderBounds.radius  = 0.5f;
    renderBounds.extents = { 0.5f, 0.5f, 0.5f };
    inout.indexBuffer = VulkanRHI::get()->createIndexBuffer(indicesData);
    inout.vertexBuffer = VulkanRHI::get()->createVertexBuffer(verticesData,inout.layout);
    inout.layout = getStandardMeshAttributes();

    SubMesh unitCubeSubMesh{};
    
    unitCubeSubMesh.renderBounds = renderBounds;
    unitCubeSubMesh.cacheMaterial = nullptr; 
    unitCubeSubMesh.indexCount = (uint32)indicesData.size();
    unitCubeSubMesh.indexStartPosition = 0;
    inout.subMeshes.push_back(unitCubeSubMesh);
}
