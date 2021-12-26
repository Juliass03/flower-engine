#include "pmx_mesh.h"
#include "../asset_system/asset_pmx.h"
#include "texture.h"
#include "../core/file_system.h"
#include "../asset_system/asset_texture.h"

namespace engine{
	

PMXManager* PMXManager::get()
{
	static PMXManager manager;
	return &manager;
}

PMXMesh* PMXManager::getPMX(const std::string& name)
{
	if(m_cache.find(name) == m_cache.end())
	{
		asset_system::PMXFile pmxFile;

		if(asset_system::ReadPMXFile(&pmxFile,name.c_str()))
		{
			// build pmx mesh
			PMXMesh* newMesh = new PMXMesh();

			// 0. prepare path.
			auto end_pos = name.find_last_of("/\\");
			std::string pmx_folder_path;
			if(end_pos!=std::string::npos)
			{
				pmx_folder_path = name.substr(0,end_pos);
			}
			pmx_folder_path += "/";

			// 1. prepare data.
			newMesh->m_materials.resize(pmxFile.m_materials.size());
			newMesh->m_subMeshes.resize(pmxFile.m_materials.size());

			newMesh->m_positions.resize(pmxFile.m_vertices.size() * 3);
			newMesh->m_normals.resize(  pmxFile.m_vertices.size() * 3);
			newMesh->m_uvs.resize(      pmxFile.m_vertices.size() * 2);

			// 2. pack raw vertices data.
			for(auto i_v = 0; i_v < pmxFile.m_vertices.size(); i_v ++)
			{
				auto pos_index    = 3 * i_v;
				auto normal_index = 3 * i_v;
				auto uv0_index    = 2 * i_v;

				newMesh->m_positions[pos_index]     = pmxFile.m_vertices[i_v].m_position.x;
				newMesh->m_positions[pos_index + 1] = pmxFile.m_vertices[i_v].m_position.y;
				newMesh->m_positions[pos_index + 2] = pmxFile.m_vertices[i_v].m_position.z;

				newMesh->m_normals[normal_index]     = pmxFile.m_vertices[i_v].m_normal.x;
				newMesh->m_normals[normal_index + 1] = pmxFile.m_vertices[i_v].m_normal.y;
				newMesh->m_normals[normal_index + 2] = pmxFile.m_vertices[i_v].m_normal.z;

				newMesh->m_uvs[uv0_index]     = pmxFile.m_vertices[i_v].m_uv.x;
				newMesh->m_uvs[uv0_index + 1] = pmxFile.m_vertices[i_v].m_uv.y;
			}

			// 3. prepare mmd textures.
			//  for mmd mesh asset, i want to keep best quality so don't do any compress.
			auto* textureLibrary = TextureLibrary::get();
			for(auto mat_i = 0; mat_i < pmxFile.m_materials.size(); mat_i++)
			{
				auto& set_mat = newMesh->m_materials[mat_i];
				auto& process_mat = pmxFile.m_materials[mat_i];

				CHECK(textureLibrary->m_textureContainer[s_defaultCheckboardTextureName].bReady);

				uint32_t fallbackId = textureLibrary->m_textureContainer[s_defaultCheckboardTextureName].bindingIndex;

				// base color
				if(process_mat.m_textureIndex >= 0)
				{
					auto base_color_path = pmx_folder_path + pmxFile.m_textures[process_mat.m_textureIndex].m_textureName;
					if(textureLibrary->m_textureContainer.find(base_color_path)==textureLibrary->m_textureContainer.end())
					{
						textureLibrary->m_textureContainer[base_color_path] = {};
						auto& texture = textureLibrary->m_textureContainer[base_color_path];
						texture.sampler = VulkanRHI::get()->getLinearRepeatSampler();
						texture.texture = asset_system::loadFromFile(base_color_path,VK_FORMAT_R8G8B8A8_SRGB,4,false,true);
						texture.bReady = true;
						TextureLibrary::get()->updateTextureToBindlessDescriptorSet(texture);
					}

					set_mat.baseColorTextureName = base_color_path;
					set_mat.baseColorTextureId = textureLibrary->m_textureContainer[base_color_path].bindingIndex;
				}
				else
				{
					set_mat.baseColorTextureName = s_defaultCheckboardTextureName;
					set_mat.baseColorTextureId = fallbackId;
				}

				// toon lut
				if(process_mat.m_toonTextureIndex >=0 )
				{
					auto toon_path = pmx_folder_path + pmxFile.m_textures[process_mat.m_toonTextureIndex].m_textureName;

					if(textureLibrary->m_textureContainer.find(toon_path)==textureLibrary->m_textureContainer.end())
					{
						textureLibrary->m_textureContainer[toon_path] = {};

						auto& texture = textureLibrary->m_textureContainer[toon_path];

						texture.sampler = VulkanRHI::get()->getLinearRepeatSampler();
						texture.texture = asset_system::loadFromFile(toon_path,VK_FORMAT_R8G8B8A8_SRGB,4,false,true);
						texture.bReady = true;
						TextureLibrary::get()->updateTextureToBindlessDescriptorSet(texture);
					}

					

					set_mat.toonTextureName = toon_path;
					set_mat.toonTextureId = textureLibrary->m_textureContainer[toon_path].bindingIndex;
				}
				else
				{
					set_mat.toonTextureId = fallbackId;
					set_mat.toonTextureName = s_defaultCheckboardTextureName;
				}


				// sphere texture
				if(process_mat.m_sphereTextureIndex >= 0)
				{
					auto sphere_tex_path = pmx_folder_path + pmxFile.m_textures[process_mat.m_sphereTextureIndex].m_textureName;

					if(textureLibrary->m_textureContainer.find(sphere_tex_path)==textureLibrary->m_textureContainer.end())
					{
						textureLibrary->m_textureContainer[sphere_tex_path] = {};

						auto& texture = textureLibrary->m_textureContainer[sphere_tex_path];

						texture.sampler = VulkanRHI::get()->getLinearRepeatSampler();
						texture.texture = asset_system::loadFromFile(sphere_tex_path,VK_FORMAT_R8G8B8A8_SRGB,4,false,true);
						texture.bReady = true;
						TextureLibrary::get()->updateTextureToBindlessDescriptorSet(texture);
					}

					

					set_mat.sphereTextureId = textureLibrary->m_textureContainer[sphere_tex_path].bindingIndex;
					set_mat.sphereTextureName = sphere_tex_path;
				}
				else
				{
					set_mat.sphereTextureId = fallbackId;
					set_mat.sphereTextureName = s_defaultCheckboardTextureName;
				}
			}

			// 4. prepare submesh
			uint32_t indicesCount = 0;
			for(auto& mat : pmxFile.m_materials)
			{
				indicesCount += mat.m_numFaceVertices;
			}
			newMesh->m_indices.resize(indicesCount);
			int32_t start_face_point = 0;
			int32_t material_id = 0;
			int32_t indexLoop = 0;
			for(auto& mat : pmxFile.m_materials)
			{
				auto& processing_submesh = newMesh->m_subMeshes[material_id];
				

				processing_submesh.beginIndex  = indexLoop;
				processing_submesh.vertexCount = mat.m_numFaceVertices;
				processing_submesh.materialId  = material_id;

				material_id ++;

				// pack all face data in mesh
				auto face_num = mat.m_numFaceVertices / 3;
				int32_t end_face_point = start_face_point + face_num;
				for(auto face_id = start_face_point; face_id < end_face_point; face_id++)
				{
					auto& face = pmxFile.m_faces[face_id];
					for(auto vertex : face.m_vertices)
					{
						newMesh->m_indices[indexLoop] = vertex;
						indexLoop ++;
					}
				}
				start_face_point = end_face_point;
			}

			// 5. create index buffer and vertex buffer.
			CHECK(newMesh->m_indexBuffer == nullptr);

			// init and upload indices data.
			newMesh->m_indexBuffer = VulkanIndexBuffer::create(
				VulkanRHI::get()->getVulkanDevice(),
				VulkanRHI::get()->getGraphicsCommandPool(),
				newMesh->m_indices
			);
			

			m_cache[name] = newMesh;
			return newMesh;
		}
		return nullptr;
	}
	else
	{
		return m_cache[name];
	}
}

void PMXManager::release()
{
	for(auto& pair : m_cache)
	{
		delete pair.second;
	}

	m_cache.clear();
}

VkDeviceSize PMXMesh::getTotalVertexSize() const
{
	VkDeviceSize perDataDeviceSize = VkDeviceSize(sizeof(float));

	return perDataDeviceSize * (
		m_positions.size() + 
		m_uvs.size() + 
		m_normals.size()
	);
}

PMXMesh::~PMXMesh()
{
	if(m_indexBuffer)
	{
		delete m_indexBuffer;
		m_indexBuffer = nullptr;
	}
}

}
