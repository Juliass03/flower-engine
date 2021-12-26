#include <stdexcept>
#include <string>
#include <array>
#include "../core/file_system.h"
#include "unicode.h"
#include <iostream>
#include <fstream>
#include <string>
#include "asset_pmx.h"
#include "../core/file_system.h"

using namespace engine::asset_system;

constexpr bool print_debug = true;

void read_string(PMXFile* pmx, std::string* val,std::ifstream& file,std::u16string* string_16 = nullptr)
{
	uint32_t buf_size;
	// 1. 获取buffer大小
	file.read((char*)&buf_size,sizeof(uint32_t));

	if(buf_size>0)
	{
		if(pmx->m_header.m_encode==0)
		{
			// utf-16
			std::u16string utf16_str(buf_size/2,u'\0');
			file.read((char*)&utf16_str[0],sizeof(utf16_str[0])*utf16_str.size());

			if(string_16!=nullptr)
			{
				*string_16 = utf16_str;
			}

			engine::asset_system::unicode::ConvU16ToU8(utf16_str,*val);
		}
		else if(pmx->m_header.m_encode==1)
		{
			// utf-8
			std::string utf8_str(buf_size,'\0');
			file.read((char*)&utf8_str[0],sizeof(utf8_str[0])*buf_size);
			*val = utf8_str;
		}
		else
		{
			LOG_IO_ERROR("unknown PMX encode format：Encode = {0}",pmx->m_header.m_encode);
		}
	}
}

void read_index(int32_t* index,uint8_t index_size,std::ifstream& file)
{
	switch(index_size)
	{
	case 1:
	{
		uint8_t idx;
		file.read((char*)&idx,sizeof(uint8_t));
		if(idx!=0xFF)
		{
			*index = (int32_t)idx;
		}
		else
		{
			*index = -1;
		}
	}
	break;
	case 2:
	{
		uint16_t idx;
		file.read((char*)&idx,sizeof(uint16_t));
		if(idx!=0xFFFF)
		{
			*index = (int32_t)idx;
		}
		else
		{
			*index = -1;
		}
	}
	break;
	case 4:
	{
		uint32_t idx;
		file.read((char*)&idx,sizeof(uint32_t));
		*index = (int32_t)idx;
	}
	break;
	default:
		LOG_IO_ERROR("错误的PMX次序大小：{0}",index_size);
		return;
	}
}

void read_vertex(PMXFile* pmx,std::ifstream& file)
{
	int32_t vertex_count;
	file.read((char*)&vertex_count,sizeof(int32_t));
	if constexpr(print_debug)
	{
		LOG_IO_TRACE("vertex count: {0}",vertex_count);
	}

	auto& vertices = pmx->m_vertices;
	vertices.resize(vertex_count);

	for(auto& vertex:vertices)
	{
		file.read((char*)&vertex.m_position[0],sizeof(float)*3);
		file.read((char*)&vertex.m_normal[0],sizeof(float)*3);
		file.read((char*)&vertex.m_uv[0],sizeof(float)*2);

		for(uint8_t i = 0; i<pmx->m_header.m_addUVNum; i++)
		{
			file.read((char*)&vertex.m_addUV[i][0],sizeof(float)*4);
		}

		file.read((char*)&vertex.m_weightType,sizeof(uint8_t));

		switch(vertex.m_weightType)
		{
		case PMXVertexWeight::BDEF1:
			read_index(&vertex.m_boneIndices[0],pmx->m_header.m_boneIndexSize,file);
			break;
		case PMXVertexWeight::BDEF2:
			read_index(&vertex.m_boneIndices[0],pmx->m_header.m_boneIndexSize,file);
			read_index(&vertex.m_boneIndices[1],pmx->m_header.m_boneIndexSize,file);
			file.read((char*)&vertex.m_boneWeights[0],sizeof(float));
			break;
		case PMXVertexWeight::BDEF4:
			read_index(&vertex.m_boneIndices[0],pmx->m_header.m_boneIndexSize,file);
			read_index(&vertex.m_boneIndices[1],pmx->m_header.m_boneIndexSize,file);
			read_index(&vertex.m_boneIndices[2],pmx->m_header.m_boneIndexSize,file);
			read_index(&vertex.m_boneIndices[3],pmx->m_header.m_boneIndexSize,file);
			file.read((char*)&vertex.m_boneWeights[0],sizeof(float));
			file.read((char*)&vertex.m_boneWeights[1],sizeof(float));
			file.read((char*)&vertex.m_boneWeights[2],sizeof(float));
			file.read((char*)&vertex.m_boneWeights[3],sizeof(float));
			break;
		case PMXVertexWeight::SDEF:
			read_index(&vertex.m_boneIndices[0],pmx->m_header.m_boneIndexSize,file);
			read_index(&vertex.m_boneIndices[1],pmx->m_header.m_boneIndexSize,file);
			file.read((char*)&vertex.m_boneWeights[0],sizeof(float));
			file.read((char*)&vertex.m_sdefC[0],sizeof(float)*3);
			file.read((char*)&vertex.m_sdefR0[0],sizeof(float)*3);
			file.read((char*)&vertex.m_sdefR1[0],sizeof(float)*3);
			break;
		case PMXVertexWeight::QDEF:
			read_index(&vertex.m_boneIndices[0],pmx->m_header.m_boneIndexSize,file);
			read_index(&vertex.m_boneIndices[1],pmx->m_header.m_boneIndexSize,file);
			read_index(&vertex.m_boneIndices[2],pmx->m_header.m_boneIndexSize,file);
			read_index(&vertex.m_boneIndices[3],pmx->m_header.m_boneIndexSize,file);
			file.read((char*)&vertex.m_boneWeights[0],sizeof(float));
			file.read((char*)&vertex.m_boneWeights[1],sizeof(float));
			file.read((char*)&vertex.m_boneWeights[2],sizeof(float));
			file.read((char*)&vertex.m_boneWeights[3],sizeof(float));
			break;
		default:
			LOG_IO_ERROR("未知的PMX顶点格式：{0}.",vertex.m_weightType);
			return;
			break;
		}
		file.read((char*)&vertex.m_edgeMag,sizeof(float));
	}
}

void read_face(PMXFile* pmx,std::ifstream& file)
{
	int32_t face_count;
	file.read((char*)&face_count,sizeof(int32_t));
	face_count /= 3;
	if constexpr(print_debug)
	{
		LOG_IO_TRACE("face count: {0}",face_count);
	}

	pmx->m_faces.resize(face_count);
	switch(pmx->m_header.m_vertexIndexSize)
	{
	case 1:
	{
		std::vector<uint8_t> vertices(face_count*3);
		file.read((char*)vertices.data(),sizeof(uint8_t)*vertices.size());
		for(int32_t faceIdx = 0; faceIdx<face_count; faceIdx++)
		{
			pmx->m_faces[faceIdx].m_vertices[0] = vertices[faceIdx*3+0];
			pmx->m_faces[faceIdx].m_vertices[1] = vertices[faceIdx*3+1];
			pmx->m_faces[faceIdx].m_vertices[2] = vertices[faceIdx*3+2];
		}
	}
	break;
	case 2:
	{
		std::vector<uint16_t> vertices(face_count*3);
		file.read((char*)vertices.data(),sizeof(uint16_t)*vertices.size());
		for(int32_t faceIdx = 0; faceIdx<face_count; faceIdx++)
		{
			pmx->m_faces[faceIdx].m_vertices[0] = vertices[faceIdx*3+0];
			pmx->m_faces[faceIdx].m_vertices[1] = vertices[faceIdx*3+1];
			pmx->m_faces[faceIdx].m_vertices[2] = vertices[faceIdx*3+2];
		}
	}
	break;
	case 4:
	{
		std::vector<uint32_t> vertices(face_count*3);
		file.read((char*)vertices.data(),sizeof(uint32_t)*vertices.size());
		for(int32_t faceIdx = 0; faceIdx<face_count; faceIdx++)
		{
			pmx->m_faces[faceIdx].m_vertices[0] = vertices[faceIdx*3+0];
			pmx->m_faces[faceIdx].m_vertices[1] = vertices[faceIdx*3+1];
			pmx->m_faces[faceIdx].m_vertices[2] = vertices[faceIdx*3+2];
		}
	}
	break;
	default:
		LOG_IO_ERROR("未知的PMX顶点次序类型: {0}",pmx->m_header.m_vertexIndexSize);
		return;
	}
}

void read_texture(PMXFile* pmx,std::ifstream& file)
{
	int32_t texture_count;
	file.read((char*)&texture_count,sizeof(int32_t));
	if constexpr(print_debug)
	{
		LOG_IO_TRACE("texture count: {0}",texture_count);
	}

	pmx->m_textures.resize(texture_count);

	for(auto& tex:pmx->m_textures)
	{
		read_string(pmx,&tex.m_textureName,file);
		if constexpr(print_debug)
		{
			LOG_IO_TRACE("texture name: {0}",tex.m_textureName);
		}
	}
}

void read_material(PMXFile* pmx,std::ifstream& file)
{
	int32_t mat_count;
	file.read((char*)&mat_count,sizeof(int32_t));
	if constexpr(print_debug)
	{
		LOG_IO_TRACE("material count: {0}",mat_count);
	}

	pmx->m_materials.resize(mat_count);
	for(auto& mat:pmx->m_materials)
	{
		read_string(pmx,&mat.m_name,file);
		read_string(pmx,&mat.m_englishName,file);
		if constexpr(print_debug)
		{
			LOG_IO_TRACE("material name: {0}",mat.m_name);
			LOG_IO_TRACE("material english name: {0}",mat.m_englishName);
		}

		file.read((char*)&mat.m_diffuse[0],sizeof(float)*4);
		file.read((char*)&mat.m_specular[0],sizeof(float)*3);
		file.read((char*)&mat.m_specularPower,sizeof(float));
		file.read((char*)&mat.m_ambient[0],sizeof(float)*3);
		file.read((char*)&mat.m_drawMode,sizeof(uint8_t));
		file.read((char*)&mat.m_edgeColor[0],sizeof(float)*4);
		file.read((char*)&mat.m_edgeSize,sizeof(float));

		read_index(&mat.m_textureIndex,pmx->m_header.m_textureIndexSize,file);
		read_index(&mat.m_sphereTextureIndex,pmx->m_header.m_textureIndexSize,file);

		file.read((char*)&mat.m_sphereMode,sizeof(uint8_t));
		file.read((char*)&mat.m_toonMode,sizeof(uint8_t));

		if(mat.m_toonMode==PMXToonMode::Separate)
		{
			read_index(&mat.m_toonTextureIndex,pmx->m_header.m_textureIndexSize,file);
		}
		else if(mat.m_toonMode==PMXToonMode::Common)
		{
			uint8_t toonIndex;
			file.read((char*)&toonIndex,sizeof(uint8_t));
			mat.m_toonTextureIndex = (int32_t)toonIndex;
		}
		else
		{
			LOG_IO_ERROR("未知的PMX Toon Mode:{0}",mat.m_toonMode);
			return;
		}

		read_string(pmx,&mat.m_memo,file);
		file.read((char*)&mat.m_numFaceVertices,sizeof(int32_t));
	}
}

void read_bone(PMXFile* pmx,std::ifstream& file)
{
	int32_t boneCount;
	file.read((char*)&boneCount,sizeof(int32_t));
	if constexpr(print_debug)
	{
		LOG_IO_TRACE("bone count: {0}",boneCount);
	}
	pmx->m_bones.resize(boneCount);

	for(auto& bone:pmx->m_bones)
	{
		read_string(pmx,&bone.m_name,file,&bone.m_u16name);

		// bone.m_u16name = unicode::ConvertSjisToU16String(bone.m_name.c_str());
		read_string(pmx,&bone.m_englishName,file);

		file.read((char*)&bone.m_position[0],sizeof(float)*3);
		read_index(&bone.m_parentBoneIndex,pmx->m_header.m_boneIndexSize,file);

		file.read((char*)&bone.m_deformDepth,sizeof(int32_t));
		file.read((char*)&bone.m_boneFlag,sizeof(uint16_t));

		if(((uint16_t)bone.m_boneFlag&(uint16_t)PMXBoneFlags::TargetShowMode)==0)
		{
			file.read((char*)&bone.m_positionOffset[0],sizeof(float)*3);
		}
		else
		{
			read_index(&bone.m_linkBoneIndex,pmx->m_header.m_boneIndexSize,file);
		}

		if(((uint16_t)bone.m_boneFlag&(uint16_t)PMXBoneFlags::AppendRotate)||
			((uint16_t)bone.m_boneFlag&(uint16_t)PMXBoneFlags::AppendTranslate))
		{
			read_index(&bone.m_appendBoneIndex,pmx->m_header.m_boneIndexSize,file);

			file.read((char*)&bone.m_appendWeight,sizeof(float));
		}

		if((uint16_t)bone.m_boneFlag&(uint16_t)PMXBoneFlags::FixedAxis)
		{
			file.read((char*)&bone.m_fixedAxis[0],sizeof(float)*3);
		}

		if((uint16_t)bone.m_boneFlag&(uint16_t)PMXBoneFlags::LocalAxis)
		{
			file.read((char*)&bone.m_localXAxis[0],sizeof(float)*3);
			file.read((char*)&bone.m_localZAxis[0],sizeof(float)*3);
		}

		if((uint16_t)bone.m_boneFlag&(uint16_t)PMXBoneFlags::DeformOuterParent)
		{
			file.read((char*)&bone.m_keyValue,sizeof(int32_t));
		}

		if((uint16_t)bone.m_boneFlag&(uint16_t)PMXBoneFlags::IK)
		{
			read_index(&bone.m_ikTargetBoneIndex,pmx->m_header.m_boneIndexSize,file);
			file.read((char*)&bone.m_ikIterationCount,sizeof(int32_t));
			file.read((char*)&bone.m_ikLimit,sizeof(float));

			int32_t linkCount;
			file.read((char*)&linkCount,sizeof(int32_t));
			bone.m_ikLinks.resize(linkCount);
			for(auto& ikLink:bone.m_ikLinks)
			{
				read_index(&ikLink.m_ikBoneIndex,pmx->m_header.m_boneIndexSize,file);
				file.read((char*)&ikLink.m_enableLimit,sizeof(unsigned char));

				if(ikLink.m_enableLimit!=0)
				{
					file.read((char*)&ikLink.m_limitMin,sizeof(float)*3);
					file.read((char*)&ikLink.m_limitMax,sizeof(float)*3);
				}
			}
		}
	}
}

void read_morph(PMXFile* pmx,std::ifstream& file)
{
	int32_t morphCount;
	file.read((char*)&morphCount,sizeof(int32_t));
	if constexpr(print_debug)
	{
		LOG_IO_TRACE("morph count: {0}",morphCount);
	}
	pmx->m_morphs.resize(morphCount);

	for(auto& morph:pmx->m_morphs)
	{
		read_string(pmx,&morph.m_name,file);
		read_string(pmx,&morph.m_englishName,file);

		file.read((char*)&morph.m_controlPanel,sizeof(uint8_t));
		file.read((char*)&morph.m_morphType,sizeof(uint8_t));

		int32_t dataCount;
		file.read((char*)&dataCount,sizeof(int32_t));

		if(morph.m_morphType==PMXMorphType::Position)
		{
			morph.m_positionMorph.resize(dataCount);
			for(auto& data:morph.m_positionMorph)
			{
				read_index(&data.m_vertexIndex,pmx->m_header.m_vertexIndexSize,file);
				file.read((char*)&data.m_position[0],sizeof(float)*3);
			}
		}
		else if(morph.m_morphType==PMXMorphType::UV||
			morph.m_morphType==PMXMorphType::AddUV1||
			morph.m_morphType==PMXMorphType::AddUV2||
			morph.m_morphType==PMXMorphType::AddUV3||
			morph.m_morphType==PMXMorphType::AddUV4
			)
		{
			morph.m_uvMorph.resize(dataCount);
			for(auto& data:morph.m_uvMorph)
			{
				read_index(&data.m_vertexIndex,pmx->m_header.m_vertexIndexSize,file);
				file.read((char*)&data.m_uv[0],sizeof(float)*4);
			}
		}
		else if(morph.m_morphType==PMXMorphType::Bone)
		{
			morph.m_boneMorph.resize(dataCount);
			for(auto& data:morph.m_boneMorph)
			{
				read_index(&data.m_boneIndex,pmx->m_header.m_boneIndexSize,file);
				file.read((char*)&data.m_position[0],sizeof(float)*3);
				file.read((char*)&data.m_quaternion[0],sizeof(float)*4);
			}
		}
		else if(morph.m_morphType==PMXMorphType::Material)
		{
			morph.m_materialMorph.resize(dataCount);
			for(auto& data:morph.m_materialMorph)
			{
				read_index(&data.m_materialIndex,pmx->m_header.m_materialIndexSize,file);

				file.read((char*)&data.m_opType,sizeof(uint8_t));
				file.read((char*)&data.m_diffuse,sizeof(float)*4);
				file.read((char*)&data.m_specular,sizeof(float)*3);
				file.read((char*)&data.m_specularPower,sizeof(float));
				file.read((char*)&data.m_ambient,sizeof(float)*3);
				file.read((char*)&data.m_edgeColor,sizeof(float)*4);
				file.read((char*)&data.m_edgeSize,sizeof(float));
				file.read((char*)&data.m_textureFactor,sizeof(float)*4);
				file.read((char*)&data.m_sphereTextureFactor,sizeof(float)*4);
				file.read((char*)&data.m_toonTextureFactor,sizeof(float)*4);
			}
		}
		else if(morph.m_morphType==PMXMorphType::Group)
		{
			morph.m_groupMorph.resize(dataCount);
			for(auto& data:morph.m_groupMorph)
			{
				read_index(&data.m_morphIndex,pmx->m_header.m_morphIndexSize,file);
				file.read((char*)&data.m_weight,sizeof(float));
			}
		}
		else if(morph.m_morphType==PMXMorphType::Flip)
		{
			morph.m_flipMorph.resize(dataCount);
			for(auto& data:morph.m_flipMorph)
			{
				read_index(&data.m_morphIndex,pmx->m_header.m_morphIndexSize,file);
				file.read((char*)&data.m_weight,sizeof(float));
			}
		}
		else if(morph.m_morphType==PMXMorphType::Impluse)
		{
			morph.m_impulseMorph.resize(dataCount);
			for(auto& data:morph.m_impulseMorph)
			{
				read_index(&data.m_rigidbodyIndex,pmx->m_header.m_rigidbodyIndexSize,file);
				file.read((char*)&data.m_localFlag,sizeof(uint8_t));
				file.read((char*)&data.m_translateVelocity,sizeof(float)*3);
				file.read((char*)&data.m_rotateTorque,sizeof(float)*3);
			}
		}
		else
		{
			LOG_IO_ERROR("不支持的PMX Morph类型:[{0}]",(int)morph.m_morphType);
			return;
		}
	}
}

void read_display_frame(PMXFile* pmx,std::ifstream& file)
{
	int32_t displayFrameCount;
	file.read((char*)&displayFrameCount,sizeof(int32_t));
	pmx->m_displayFrames.resize(displayFrameCount);
	for(auto& displayFrame:pmx->m_displayFrames)
	{
		read_string(pmx,&displayFrame.m_name,file);
		read_string(pmx,&displayFrame.m_englishName,file);
		file.read((char*)&displayFrame.m_flag,sizeof(uint8_t));

		int32_t targetCount;
		file.read((char*)&targetCount,sizeof(int32_t));
		displayFrame.m_targets.resize(targetCount);

		for(auto& target:displayFrame.m_targets)
		{
			file.read((char*)&target.m_type,sizeof(uint8_t));

			if(target.m_type==PMXDispalyFrame::TargetType::BoneIndex)
			{
				read_index(&target.m_index,pmx->m_header.m_boneIndexSize,file);
			}
			else if(target.m_type==PMXDispalyFrame::TargetType::MorphIndex)
			{
				read_index(&target.m_index,pmx->m_header.m_morphIndexSize,file);
			}
			else
			{
				LOG_IO_ERROR("未知的PMX帧类型:{0}",(int32_t)target.m_type);
				return;
			}
		}
	}
}

void read_rigidbody(PMXFile* pmx,std::ifstream& file)
{
	int32_t rbCount;
	file.read((char*)&rbCount,sizeof(int32_t));
	pmx->m_rigidbodies.resize(rbCount);
	for(auto& rb:pmx->m_rigidbodies)
	{
		read_string(pmx,&rb.m_name,file);
		read_string(pmx,&rb.m_englishName,file);
		read_index(&rb.m_boneIndex,pmx->m_header.m_boneIndexSize,file);

		file.read((char*)&rb.m_group,sizeof(uint8_t));
		file.read((char*)&rb.m_collisionGroup,sizeof(uint16_t));
		file.read((char*)&rb.m_shape,sizeof(uint8_t));
		file.read((char*)&rb.m_shapeSize[0],sizeof(float)*3);
		file.read((char*)&rb.m_translate[0],sizeof(float)*3);
		file.read((char*)&rb.m_rotate[0],sizeof(float)*3);
		file.read((char*)&rb.m_mass,sizeof(float));
		file.read((char*)&rb.m_translateDimmer,sizeof(float));
		file.read((char*)&rb.m_rotateDimmer,sizeof(float));
		file.read((char*)&rb.m_repulsion,sizeof(float));
		file.read((char*)&rb.m_friction,sizeof(float));
		file.read((char*)&rb.m_op,sizeof(uint8_t));
	}
}

void read_joint(PMXFile* pmx,std::ifstream& file)
{
	int32_t jointCount;
	file.read((char*)&jointCount,sizeof(int32_t));
	pmx->m_joints.resize(jointCount);
	for(auto& joint:pmx->m_joints)
	{
		read_string(pmx,&joint.m_name,file);
		read_string(pmx,&joint.m_englishName,file);

		file.read((char*)&joint.m_type,sizeof(uint8_t));
		read_index(&joint.m_rigidbodyAIndex,pmx->m_header.m_rigidbodyIndexSize,file);
		read_index(&joint.m_rigidbodyBIndex,pmx->m_header.m_rigidbodyIndexSize,file);

		file.read((char*)&joint.m_translate[0],sizeof(float)*3);
		file.read((char*)&joint.m_rotate[0],sizeof(float)*3);

		file.read((char*)&joint.m_translateLowerLimit[0],sizeof(float)*3);
		file.read((char*)&joint.m_translateUpperLimit[0],sizeof(float)*3);
		file.read((char*)&joint.m_rotateLowerLimit[0],sizeof(float)*3);
		file.read((char*)&joint.m_rotateUpperLimit[0],sizeof(float)*3);

		file.read((char*)&joint.m_springTranslateFactor[0],sizeof(float)*3);
		file.read((char*)&joint.m_springRotateFactor[0],sizeof(float)*3);
	}
}

void read_soft_body(PMXFile* pmx,std::ifstream& file)
{
	int32_t sbCount;
	file.read((char*)&sbCount,sizeof(int32_t));
	// if(sbCount<=0)
	{
		// todo: fix me.

		// pmx 2.1 adapt.
		return;
	}
	pmx->m_softbodies.resize(sbCount);
	for(auto& sb:pmx->m_softbodies)
	{
		read_string(pmx,&sb.m_name,file);
		read_string(pmx,&sb.m_englishName,file);

		file.read((char*)&sb.m_type,sizeof(uint8_t));

		read_index(&sb.m_materialIndex,pmx->m_header.m_materialIndexSize,file);

		file.read((char*)&sb.m_group,sizeof(uint8_t));
		file.read((char*)&sb.m_collisionGroup,sizeof(uint16_t));

		file.read((char*)&sb.m_flag,sizeof(uint8_t));

		file.read((char*)&sb.m_BLinkLength,sizeof(int32_t));
		file.read((char*)&sb.m_numClusters,sizeof(int32_t));

		file.read((char*)&sb.m_totalMass,sizeof(float));
		file.read((char*)&sb.m_collisionMargin,sizeof(float));

		file.read((char*)&sb.m_aeroModel,sizeof(int32_t));

		file.read((char*)&sb.m_VCF,sizeof(float));
		file.read((char*)&sb.m_DP,sizeof(float));
		file.read((char*)&sb.m_DG,sizeof(float));
		file.read((char*)&sb.m_LF,sizeof(float));
		file.read((char*)&sb.m_PR,sizeof(float));
		file.read((char*)&sb.m_VC,sizeof(float));
		file.read((char*)&sb.m_DF,sizeof(float));
		file.read((char*)&sb.m_MT,sizeof(float));
		file.read((char*)&sb.m_CHR,sizeof(float));
		file.read((char*)&sb.m_KHR,sizeof(float));
		file.read((char*)&sb.m_SHR,sizeof(float));
		file.read((char*)&sb.m_AHR,sizeof(float));

		file.read((char*)&sb.m_SRHR_CL,sizeof(float));
		file.read((char*)&sb.m_SKHR_CL,sizeof(float));
		file.read((char*)&sb.m_SSHR_CL,sizeof(float));
		file.read((char*)&sb.m_SR_SPLT_CL,sizeof(float));
		file.read((char*)&sb.m_SK_SPLT_CL,sizeof(float));
		file.read((char*)&sb.m_SS_SPLT_CL,sizeof(float));

		file.read((char*)&sb.m_V_IT,sizeof(int32_t));
		file.read((char*)&sb.m_P_IT,sizeof(int32_t));
		file.read((char*)&sb.m_D_IT,sizeof(int32_t));
		file.read((char*)&sb.m_C_IT,sizeof(int32_t));

		file.read((char*)&sb.m_LST,sizeof(float));
		file.read((char*)&sb.m_AST,sizeof(float));
		file.read((char*)&sb.m_VST,sizeof(float));

		int32_t arCount;
		file.read((char*)&arCount,sizeof(int32_t));
		sb.m_anchorRigidbodies.resize(arCount);

		for(auto& ar:sb.m_anchorRigidbodies)
		{
			read_index(&ar.m_rigidBodyIndex,pmx->m_header.m_rigidbodyIndexSize,file);
			read_index(&ar.m_vertexIndex,pmx->m_header.m_vertexIndexSize,file);
			file.read((char*)&ar.m_nearMode,sizeof(uint8_t));
		}

		int32_t pvCount;
		file.read((char*)&pvCount,sizeof(int32_t));
		sb.m_pinVertexIndices.resize(pvCount);
		for(auto& pv:sb.m_pinVertexIndices)
		{
			read_index(&pv,pmx->m_header.m_vertexIndexSize,file);
		}
	}
}

bool engine::asset_system::ReadPMXFile(PMXFile* pmx_file,const char* filename)
{
	std::ifstream infile;
	infile.open(filename,std::ios::binary);

	if(!infile.is_open())
	{
		LOG_IO_FATAL("文件{0}打开失败！",filename);
		return false;
	}

	infile.seekg(0);
	LOG_IO_INFO("Reading Pmx file {0}.",filename);

	// 0.读取header
	auto& header = pmx_file->m_header;
	infile.read(header.m_magic,4);
	infile.read((char*)&header.m_version,sizeof(float));
	infile.read((char*)&header.m_dataSize,sizeof(uint8_t));
	infile.read((char*)&header.m_encode,sizeof(uint8_t));
	infile.read((char*)&header.m_addUVNum,sizeof(uint8_t));
	infile.read((char*)&header.m_vertexIndexSize,sizeof(uint8_t));
	infile.read((char*)&header.m_textureIndexSize,sizeof(uint8_t));
	infile.read((char*)&header.m_materialIndexSize,sizeof(uint8_t));
	infile.read((char*)&header.m_boneIndexSize,sizeof(uint8_t));
	infile.read((char*)&header.m_morphIndexSize,sizeof(uint8_t));
	infile.read((char*)&header.m_rigidbodyIndexSize,sizeof(uint8_t));
	if constexpr(print_debug)
	{
		LOG_IO_TRACE("Header Info {0}",header.get_header());
	}

	// 1. 读取info
	auto& info = pmx_file->m_info;
	read_string(pmx_file,&info.m_modelName,infile);
	read_string(pmx_file,&info.m_englishModelName,infile);
	read_string(pmx_file,&info.m_comment,infile);
	read_string(pmx_file,&info.m_englishComment,infile);
	if constexpr(print_debug)
	{
		LOG_IO_TRACE("Model Name: {0}",info.m_modelName);
		LOG_IO_TRACE("Model English Name: {0}",info.m_englishModelName);
		LOG_IO_TRACE("Model Comment: {0}",info.m_comment);
		LOG_IO_TRACE("Model English Comment: {0}",info.m_englishComment);
	}

	// 2. 读取 vertex
	read_vertex(pmx_file,infile);

	// 3. 读取 face
	read_face(pmx_file,infile);

	// 4. 读取 pmx 纹理
	read_texture(pmx_file,infile);

	// 5. 读取材质
	read_material(pmx_file,infile);

	// 6. 读取骨骼
	read_bone(pmx_file,infile);

	// 7. 读取蒙皮信息
	read_morph(pmx_file,infile);

	// 8. 读取Frame信息
	read_display_frame(pmx_file,infile);

	// 9. 读取刚体
	read_rigidbody(pmx_file,infile);

	// 10. 读取关节
	read_joint(pmx_file,infile);

	// 11. 读取软体（PMX2.1）
	if(infile.good())
	{
		read_soft_body(pmx_file,infile);
	}

	infile.close();
	return true;
}