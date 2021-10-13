#pragma once
#include "asset_common.h"
#include "asset_system.h"
#include "../renderer/mesh.h"

namespace engine{ namespace asset_system{
	
struct MeshInfo
{
    struct MeshBounds 
    {
        float origin[3];
        float radius;
        float extents[3];
    };

    struct SubMeshInfo
    {
        std::string materialPath;
        MeshBounds bounds = {};
        uint32 indexCount; 
        uint32 indexStartPosition;
        uint32 vertexCount;
    };

    uint32 subMeshCount = 0;
    std::vector<SubMeshInfo> subMeshInfos = {};
    uint32 vertCount;
    std::vector<EVertexAttribute> attributeLayout = {};

    ECompressMode compressMode;
    std::string originalFile;
};

extern int32 getSize(const std::vector<EVertexAttribute>& layouts);
extern int32 getCount(const std::vector<EVertexAttribute>& layouts);
extern MeshInfo readMeshInfo(AssetFile* file);
extern AssetFile packMesh(MeshInfo* info,char* vertexData,char* indexData,bool compress = true);
extern void unpackMesh(MeshInfo* info, const char* sourcebuffer,size_t sourceSize, std::vector<float>& vertexBufer, std::vector<VertexIndexType>& indexBuffer);

extern bool bakeAssimpMesh(const char* pathIn,const char* pathOut,bool compress = true);
}}