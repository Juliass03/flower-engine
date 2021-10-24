#include "asset_mesh.h"
#include <Nlohmann/json.hpp>
#include <lz4/lz4.h>
#include "../vk/vk_rhi.h"

#include "../core/file_system.h"
#include "../core/timer.h"
#include <algorithm>
#include <unordered_map>
#include <glm/glm.hpp>
#include <glm/gtx/hash.hpp>
#include "../renderer/material.h"

#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>


namespace engine::asset_system{ 

int32 getSize(const std::vector<EVertexAttribute>& layouts)
{
	int32 size = 0;
	for(const auto& layout:layouts)
	{
		int32 layoutSize = getVertexAttributeSize(layout);
		size += layoutSize;
	}

	return size;
}

int32 getCount(const std::vector<EVertexAttribute>& layouts)
{
	int32 size = 0;
	for(const auto& layout:layouts)
	{
		int32 layoutSize = getVertexAttributeElementCount(layout);
		size += layoutSize;
	}

	return size;
}

std::string toString(const std::vector<EVertexAttribute>& layouts)
{
	std::string keyStr = "";
	for(const auto& layout:layouts)
	{
		uint32 key = (uint32)layout;
		keyStr += std::to_string(key);
	}

	return keyStr;
}

std::vector<EVertexAttribute> toVertexAttributes(std::string keyStr)
{
	std::vector<EVertexAttribute> res = {};
	for(auto& charKey:keyStr)
	{
		uint32 key = uint32(charKey-'0');
		CHECK(key>(uint32)EVertexAttribute::none && key<(uint32)EVertexAttribute::count);
		EVertexAttribute attri = (EVertexAttribute)key;
		res.push_back(attri);
	}
	return res;
}

MeshInfo readMeshInfo(AssetFile* file)
{
	MeshInfo info{};
	nlohmann::json metadata = nlohmann::json::parse(file->json);

	info.vertCount = metadata["vertNum"];
	info.subMeshCount = metadata["subMeshCount"];
	info.originalFile = metadata["originalFile"];

	std::string compressionString = metadata["compression"];
	info.compressMode = toCompressMode(compressionString.c_str());

	info.attributeLayout = toVertexAttributes(metadata["vertexAttributes"]);

	info.subMeshInfos.resize(info.subMeshCount);
	for(uint32 i = 0; i<info.subMeshCount; i++)
	{
		auto& subMeshInfo = info.subMeshInfos[i];
		std::string subMeshPreStr = "subMesh";
		subMeshPreStr += std::to_string(i);

		subMeshInfo.materialPath = metadata[subMeshPreStr+"MaterialPath"];
		subMeshInfo.indexCount = metadata[subMeshPreStr+"IndexCount"];
        subMeshInfo.vertexCount = metadata[subMeshPreStr+"VertexCount"];
        subMeshInfo.indexStartPosition = metadata[subMeshPreStr+"IndexStartPosition"];

		std::vector<float> subBoundsData;
		subBoundsData.reserve(7);
		subBoundsData = metadata[subMeshPreStr+"Bounds"].get<std::vector<float>>();

		subMeshInfo.bounds.origin[0] = subBoundsData[0];
		subMeshInfo.bounds.origin[1] = subBoundsData[1];
		subMeshInfo.bounds.origin[2] = subBoundsData[2];
		subMeshInfo.bounds.radius = subBoundsData[3];
		subMeshInfo.bounds.extents[0] = subBoundsData[4];
		subMeshInfo.bounds.extents[1] = subBoundsData[5];
		subMeshInfo.bounds.extents[2] = subBoundsData[6];
	}

	return info;
}

AssetFile packMesh(MeshInfo* info,char* vertexData,char* indexData,bool compress)
{
	AssetFile file;

	// 填入Magic头
	file.type[0] = 'M';
	file.type[1] = 'E';
	file.type[2] = 'S';
	file.type[3] = 'H';
	file.version = 1;

	// meta data 填入
	nlohmann::json meshMetadata;
	meshMetadata["subMeshCount"] = info->subMeshCount;
	meshMetadata["vertexAttributes"] = toString(info->attributeLayout);

	// 填入 subMesh 数据
	uint32 vertNum = info->vertCount;
	meshMetadata["vertNum"] = vertNum;
	uint32 indicesNum = 0;
	for(uint32 i = 0; i<info->subMeshCount; i++)
	{
		auto& subMeshInfo = info->subMeshInfos[i];
		std::string subMeshPreStr = "subMesh";
		subMeshPreStr += std::to_string(i);

		meshMetadata[subMeshPreStr+"MaterialPath"] = subMeshInfo.materialPath;
		meshMetadata[subMeshPreStr+"IndexCount"] = subMeshInfo.indexCount;
        meshMetadata[subMeshPreStr+"VertexCount"] = subMeshInfo.vertexCount;
        meshMetadata[subMeshPreStr+"IndexStartPosition"] = subMeshInfo.indexStartPosition;
        
		indicesNum += subMeshInfo.indexCount;

		std::vector<float> subBoundsData;
		subBoundsData.resize(7);
		subBoundsData[0] = subMeshInfo.bounds.origin[0];
		subBoundsData[1] = subMeshInfo.bounds.origin[1];
		subBoundsData[2] = subMeshInfo.bounds.origin[2];
		subBoundsData[3] = subMeshInfo.bounds.radius;
		subBoundsData[4] = subMeshInfo.bounds.extents[0];
		subBoundsData[5] = subMeshInfo.bounds.extents[1];
		subBoundsData[6] = subMeshInfo.bounds.extents[2];

		meshMetadata[subMeshPreStr+"Bounds"] = subBoundsData;
	}

	meshMetadata["originalFile"] = info->originalFile;

	// 数据按照每个submesh打包
	uint32 vertBufferSize = vertNum*getSize(info->attributeLayout);
	uint32 indicesBufferSize = indicesNum*sizeof(VertexIndexType);
	uint32 fullSize = vertBufferSize+indicesBufferSize;
	std::vector<char> mergedBuffer;
	mergedBuffer.resize(fullSize);

	memcpy(mergedBuffer.data(),vertexData,vertBufferSize);
	memcpy(mergedBuffer.data()+vertBufferSize,indexData,indicesBufferSize);

	if(compress)
	{
		size_t compressStaging = LZ4_compressBound(static_cast<int>(fullSize));
		file.binBlob.resize(compressStaging);
		int compressedSize = LZ4_compress_default(
			mergedBuffer.data(),
			file.binBlob.data(),
			static_cast<int>(mergedBuffer.size()),
			static_cast<int>(compressStaging)
		);

		file.binBlob.resize(compressedSize);
		meshMetadata["compression"] = "LZ4";
	}
	else
	{
		file.binBlob.resize(fullSize);
		memcpy(file.binBlob.data(),(const char*)mergedBuffer.data(),fullSize);
		meshMetadata["compression"] = "None";
	}

	file.json = meshMetadata.dump();
	return file;
}

void unpackMesh(MeshInfo* info,const char* sourcebuffer,size_t sourceSize,std::vector<float>& vertexBufer,std::vector<VertexIndexType>& indexBuffer)
{
	uint32 indicesCount = 0;
	for(auto& subInfo:info->subMeshInfos)
	{
		indicesCount += subInfo.indexCount;
	}
	indexBuffer.resize(indicesCount);
	vertexBufer.resize(info->vertCount * getCount(info->attributeLayout));

	uint32 vertBufferSize = info->vertCount*getSize(info->attributeLayout);
	uint32 indicesBufferSize = indicesCount*sizeof(VertexIndexType);

	std::vector<char> packBuffer;
	packBuffer.resize(vertBufferSize+indicesBufferSize);

	if(info->compressMode==ECompressMode::LZ4)
	{
		LZ4_decompress_safe(
			sourcebuffer,
			packBuffer.data(),
			static_cast<int>(sourceSize),
			static_cast<int>(packBuffer.size())
		);
	}
	else
	{
		memcpy(packBuffer.data(),sourcebuffer,sourceSize);
	}

	memcpy((char*)vertexBufer.data(),packBuffer.data(),vertBufferSize);
	memcpy((char*)indexBuffer.data(),packBuffer.data()+vertBufferSize,indicesBufferSize);
}

struct AssimpModelProcess
{
    struct StandardVertex
    {
        glm::vec3 pos = glm::vec3(0.0f);
        glm::vec2 uv0 = glm::vec2(0.0f);
        glm::vec3 normal = glm::vec3(0.0f);
        glm::vec4 tangent = glm::vec4(0.0f);
    };

    std::vector<MeshInfo::SubMeshInfo> m_subMeshInfos{};
    std::vector<StandardVertex> m_vertices{};
    std::vector<VertexIndexType> m_indices{};

    // 填充材质信息
    std::vector<std::string> loadMaterialTexture(aiMaterial *mat, aiTextureType type)
    {
        std::vector<std::string> textures;
        for(unsigned int i = 0; i < mat->GetTextureCount(type); i++)
        {
            aiString str;
            mat->GetTexture(type,i,&str);
            textures.push_back(str.C_Str());
        }
        return textures;
    }

    MeshInfo::SubMeshInfo processMesh(aiMesh* mesh,const aiScene* scene,const std::string& materialFolderPath)
    {
        MeshInfo::SubMeshInfo subMeshInfo{};
        subMeshInfo.indexStartPosition = (uint32)m_indices.size(); // 先存储顶点开始的位置信息
        uint32 indexOffset = (uint32)m_vertices.size();

        std::vector<StandardVertex> vertices{};
        std::vector<VertexIndexType> indices{};

        for(unsigned int i = 0; i < mesh->mNumVertices; i++)
        {
            StandardVertex vertex;

            glm::vec3 vector {};
            vector.x = mesh->mVertices[i].x;
            vector.y = mesh->mVertices[i].y;
            vector.z = mesh->mVertices[i].z;
            vertex.pos = vector;

            vector.x = mesh->mNormals[i].x;
            vector.y = mesh->mNormals[i].y;
            vector.z = mesh->mNormals[i].z;
            vertex.normal = vector;

            if(mesh->mTextureCoords[0])
            {
                glm::vec2 vec {};
                vec.x = mesh->mTextureCoords[0][i].x;
                vec.y = mesh->mTextureCoords[0][i].y;
                vertex.uv0 = vec;
            }
            else
            {
                vertex.uv0 = glm::vec2(0.0f,0.0f);
            }

            glm::vec4 tangentVec {};
            tangentVec.x = mesh->mTangents[i].x;
            tangentVec.y = mesh->mTangents[i].y;
            tangentVec.z = mesh->mTangents[i].z;

            vector.x = mesh->mTangents[i].x;
            vector.y = mesh->mTangents[i].y;
            vector.z = mesh->mTangents[i].z;

            glm::vec3 bitangent{};
            bitangent.x = mesh->mBitangents[i].x;
            bitangent.y = mesh->mBitangents[i].y;
            bitangent.z = mesh->mBitangents[i].z;

            // NOTE: 判断UV是否镜像
            tangentVec.w = glm::sign(glm::dot(glm::normalize(bitangent),glm::normalize(glm::cross(vertex.normal,vector))));
            vertex.tangent = tangentVec;
            vertices.push_back(vertex);
        }

        // 处理索引
        for(unsigned int i = 0; i < mesh->mNumFaces; i++)
        {
            aiFace face = mesh->mFaces[i];
            for(unsigned int j = 0; j<face.mNumIndices; j++)
            {
                indices.push_back(indexOffset + face.mIndices[j]);
            }
        }
        
        // 填充信息到容器中
        m_vertices.insert(m_vertices.end(),vertices.begin(),vertices.end());
        m_indices.insert(m_indices.end(),indices.begin(),indices.end());

        // 填充submesh信息
        subMeshInfo.indexCount = (uint32)indices.size();
        subMeshInfo.vertexCount = (uint32)vertices.size();

        auto aabbMax = mesh->mAABB.mMax;
        auto aabbMin = mesh->mAABB.mMin;
        auto aabbExt    = (aabbMax - aabbMin) * 0.5f;
        auto aabbCenter = aabbExt + aabbMin;
        subMeshInfo.bounds.extents[0] = aabbExt.x;
        subMeshInfo.bounds.extents[1] = aabbExt.y;
        subMeshInfo.bounds.extents[2] = aabbExt.z;
        subMeshInfo.bounds.origin[0] = aabbCenter.x;
        subMeshInfo.bounds.origin[1] = aabbCenter.y;
        subMeshInfo.bounds.origin[2] = aabbCenter.z;
        subMeshInfo.bounds.radius = glm::distance(
            glm::vec3(aabbMax.x,aabbMax.y,aabbMax.z),
            glm::vec3(aabbCenter.x,aabbCenter.y,aabbCenter.z)
        );

        // NOTE: 处理材质
        std::vector<std::string> baseColorTextures{};
        std::vector<std::string> normalTextures{};
        std::vector<std::string> specularTextures{};
        std::vector<std::string> emissiveTextures{};
        if(mesh->mMaterialIndex >= 0)
        {
            aiMaterial* material = scene->mMaterials[mesh->mMaterialIndex];

            baseColorTextures = loadMaterialTexture(material,aiTextureType_DIFFUSE);
            CHECK(baseColorTextures.size() <= 1);

            normalTextures = loadMaterialTexture(material,aiTextureType_HEIGHT);
            CHECK(normalTextures.size() <= 1);

            specularTextures = loadMaterialTexture(material,aiTextureType_SPECULAR);
            CHECK(specularTextures.size() <= 1);

            emissiveTextures = loadMaterialTexture(material,aiTextureType_EMISSIVE);
            CHECK(emissiveTextures.size() <= 1);

            // 创建并序列化引擎材质
            std::string materialPath = materialFolderPath + material->GetName().C_Str() + ".material";

            // 填入到submesh info中
            subMeshInfo.materialPath = materialPath;

            

            Material newInfo{};

            // map_kd
            newInfo.baseColorTexture =  baseColorTextures.size() <= 0 ?
                s_defaultWhiteTextureName : 
                FileSystem::getFileRawName(baseColorTextures[0]) + ".texture";

            // map_bump
            newInfo.normalTexture  = normalTextures.size() <= 0 ?
                s_defaultNormalTextureName :
                FileSystem::getFileRawName(normalTextures[0])  + ".texture";

            // map_ks
            newInfo.specularTexture  = specularTextures.size() <= 0 ?
                s_defaultBlackTextureName :
                FileSystem::getFileRawName(specularTextures[0]) + ".texture";

            // map_ke
			newInfo.emissiveTexture = emissiveTextures.size() <= 0 ?
				s_defaultBlackTextureName :
				FileSystem::getFileRawName(emissiveTextures[0]) + ".texture";

            std::ofstream os(materialPath);
            cereal::JSONOutputArchive archive(os);
            archive(newInfo);
        }
        else // 没有材质的情况下回滚
        {
            subMeshInfo.materialPath = "";
        }

        return subMeshInfo;
    }

    void processNode(aiNode *node, const aiScene *scene,const std::string& materialFolderPath)
    {
        for(unsigned int i = 0; i < node->mNumMeshes; i++)
        {
            aiMesh* mesh = scene->mMeshes[node->mMeshes[i]];
            m_subMeshInfos.push_back(processMesh(mesh,scene,materialFolderPath));
        }

        for(unsigned int i = 0; i<node->mNumChildren; i++)
        {
            processNode(node->mChildren[i],scene,materialFolderPath);
        }
    }
};

bool bakeAssimpMesh(const char* pathIn,const char* pathOut,bool compress)
{
	Assimp::Importer importer;

    // NOTE: 计算法线、切线和翻转UV
	const aiScene* scene = importer.ReadFile(pathIn,aiProcessPreset_TargetRealtime_Fast |aiProcess_FlipUVs | aiProcess_GenBoundingBoxes);

	if(!scene||scene->mFlags&AI_SCENE_FLAGS_INCOMPLETE||!scene->mRootNode)
	{
        LOG_ERROR("ERROR::ASSIMP::{0}",importer.GetErrorString());
		return false;
	}

    std::string path = pathIn;
    std::string mtlSearchPath;
    std::string meshName;
    size_t pos = path.find_last_of("/\\");
    if(pos != std::string::npos)
    {
        mtlSearchPath = path.substr(0,pos);
        meshName = path.substr(pos);
    }

    mtlSearchPath += "/";
    std::string materialFolderPath = mtlSearchPath + "materials/";
    if(!std::filesystem::exists(materialFolderPath))
    {
        std::filesystem::create_directory(materialFolderPath);
    }

	MeshInfo info = {};

    // NOTE: 每个submesh仅用一个材质、但是一个材质可以给多个submesh使用
    //       我们不在这里严格区别，在模型绘制期间，根据绘制排序尽可能合并单材质多网格使用的情况
	info.subMeshCount = (uint32)scene->mNumMeshes;
	info.originalFile = pathIn;
	info.compressMode = compress ? ECompressMode::LZ4 : ECompressMode::None;

    // NOTE:使用标准的网格属性，缺少的属性将会填零
	info.attributeLayout = getStandardMeshAttributes(); 

    AssimpModelProcess processor{};
    processor.processNode(scene->mRootNode,scene,materialFolderPath);
    info.subMeshInfos = processor.m_subMeshInfos;
    info.vertCount = (uint32)processor.m_vertices.size();

    std::vector<float> vertices{};
    vertices.reserve(processor.m_vertices.size() * getCount(getStandardMeshAttributes()) );
    for(const auto& vertex : processor.m_vertices)
    {
        vertices.push_back(vertex.pos.x);
        vertices.push_back(vertex.pos.y);
        vertices.push_back(vertex.pos.z);
        vertices.push_back(vertex.uv0.x);
        vertices.push_back(vertex.uv0.y);
        vertices.push_back(vertex.normal.x);
        vertices.push_back(vertex.normal.y);
        vertices.push_back(vertex.normal.z);
        vertices.push_back(vertex.tangent.x);
        vertices.push_back(vertex.tangent.y);
        vertices.push_back(vertex.tangent.z);
        vertices.push_back(vertex.tangent.w);
    }
    asset_system::AssetFile newMesh = asset_system::packMesh(
        &info,
        (char*)vertices.data(),
        (char*)processor.m_indices.data(),
        compress
    );
    bool res = asset_system::saveBinFile(pathOut,newMesh);

    std::string pathMtl = engine::FileSystem::getFileRawName(pathIn) + ".mtl";
    remove(pathMtl.c_str());
	return true;
}
}