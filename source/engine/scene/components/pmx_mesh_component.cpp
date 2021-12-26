#include "pmx_mesh_component.h"
#include "../../renderer/pmx_mesh.h"
#include "../../renderer/render_passes/pmx_pass.h"
#include "../../renderer/render_passes/cascade_shadowdepth_pass.h"

void engine::PMXMeshComponent::preparePMX()
{
	if(bPMXMeshChange || (m_pmxPath != "" && m_pmxRef == nullptr))
	{
		// release first.
		m_pmxRef = nullptr;
		if(m_vertexBuffer != nullptr)
		{
			VulkanRHI::get()->waitIdle();
			delete m_vertexBuffer;
			m_vertexBuffer = nullptr;
		}
		if(m_stageBuffer != nullptr)
		{
			VulkanRHI::get()->waitIdle();
			delete m_stageBuffer;
			m_stageBuffer = nullptr;
		}

		auto* pmxMesh = PMXManager::get()->getPMX(m_pmxPath);
		if(pmxMesh)
		{
			// prepare vertex buffer
			m_vertexBuffer = VulkanVertexBuffer::create(
				VulkanRHI::get()->getVulkanDevice(),
				VulkanRHI::get()->getGraphicsCommandPool(),
				pmxMesh->getTotalVertexSize(),
				true,
				true 
			);

			m_stageBuffer = VulkanBuffer::create(
				VulkanRHI::get()->getVulkanDevice(),
				VulkanRHI::get()->getGraphicsCommandPool(),
				VK_BUFFER_USAGE_TRANSFER_SRC_BIT, 
				VMA_MEMORY_USAGE_CPU_TO_GPU,
				VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
				pmxMesh->getTotalVertexSize(),
				nullptr
			);

			// prepare reference.
			m_pmxRef = pmxMesh;
		}
		bPMXMeshChange = false;
	}
}

void engine::PMXMeshComponent::OnRenderCollect(VkCommandBuffer cmd,VkPipelineLayout pipelinelayout)
{
	if(auto node = m_node.lock())
	{
		if(m_pmxRef == nullptr)
		{
			// do nothing if no pmx init.
			return;
		}

		CHECK(m_vertexBuffer);
		CHECK(m_stageBuffer);

		auto modelMatrix = node->getTransform()->getWorldMatrix();

		// bind index buffer
		m_pmxRef->m_indexBuffer->bind(cmd);

		// bind vertex buffer.
		m_vertexBuffer->bind(cmd);

		// then draw every submesh.
		for(uint32 i = 0; i < m_pmxRef->m_subMeshes.size(); i++)
		{
			const auto& subMesh = m_pmxRef->m_subMeshes[i];
			const auto& material = m_pmxRef->m_materials[subMesh.materialId];

			PMXGpuPushConstants pushConstants {};

			pushConstants.modelMatrix = modelMatrix;
			pushConstants.basecolorTexId = material.baseColorTextureId;

			vkCmdPushConstants(cmd,
				pipelinelayout,
				VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
				0,
				sizeof(PMXGpuPushConstants),
				&pushConstants
			);


			vkCmdDrawIndexed(cmd, subMesh.vertexCount, 1, subMesh.beginIndex, 0, 0);
		}

	}
}

void engine::PMXMeshComponent::OnShadowRenderCollect(VkCommandBuffer cmd,VkPipelineLayout pipelinelayout, uint32_t cascadeIndex)
{
	if(auto node = m_node.lock())
	{
		if(m_pmxRef == nullptr)
		{
			// do nothing if no pmx init.
			return;
		}

		CHECK(m_vertexBuffer);
		CHECK(m_stageBuffer);
		auto modelMatrix = node->getTransform()->getWorldMatrix();

		// bind index buffer
		m_pmxRef->m_indexBuffer->bind(cmd);

		// bind vertex buffer.
		m_vertexBuffer->bind(cmd);

		// then draw every submesh.
		for(uint32 i = 0; i < m_pmxRef->m_subMeshes.size(); i++)
		{
			const auto& subMesh = m_pmxRef->m_subMeshes[i];
			const auto& material = m_pmxRef->m_materials[subMesh.materialId];

			PMXCascadePushConstants pushConstants {};

			pushConstants.modelMatrix = modelMatrix;
			pushConstants.cascadeIndex = cascadeIndex;

			vkCmdPushConstants(cmd,
				pipelinelayout,
				VK_SHADER_STAGE_VERTEX_BIT,
				0,
				sizeof(PMXCascadePushConstants),
				&pushConstants
			);

			vkCmdDrawIndexed(cmd, subMesh.vertexCount, 1, subMesh.beginIndex, 0, 0);
		}
	}
}

void engine::PMXMeshComponent::OnRenderTick(VkCommandBuffer cmd)
{
	if(auto node = m_node.lock())
	{
		if(m_pmxRef == nullptr)
		{
			// do nothing if no pmx init.
			return;
		}
		CHECK(m_vertexBuffer);
		CHECK(m_stageBuffer);

		// copy vertex buffer gpu. 
		VkDeviceSize bufferSize = m_pmxRef->getTotalVertexSize();
		auto readySize = getComponentVertexSize();
		CHECK(bufferSize == readySize);
		m_stageBuffer->map();
		m_stageBuffer->copyTo(m_readyVertexData.data(),readySize);
		m_stageBuffer->unmap();

		// copy to gpu
		VkBufferCopy copyRegion{};
		copyRegion.srcOffset = 0; 
		copyRegion.dstOffset = 0; 
		copyRegion.size = bufferSize;
		vkCmdCopyBuffer(cmd, m_stageBuffer->GetVkBuffer(), m_vertexBuffer->getBuffer(), 1, &copyRegion);
	}
}

void engine::PMXMeshComponent::OnSceneTick()
{
	if(auto node = m_node.lock())
	{
		preparePMX();
		if(m_pmxRef == nullptr)
		{
			// do nothing if no pmx init.
			return;
		}
		CHECK(m_vertexBuffer);
		CHECK(m_stageBuffer);

		prepareCurrentFrameVertexData();
	}
}

VkDeviceSize engine::PMXMeshComponent::getComponentVertexSize() const
{
	VkDeviceSize perDataDeviceSize = VkDeviceSize(sizeof(float));
	return perDataDeviceSize * m_readyVertexData.size();
}

void engine::PMXMeshComponent::prepareCurrentFrameVertexData()
{
	CHECK(m_pmxRef);

	size_t vertexCount = m_pmxRef->m_positions.size() / 3;

	size_t perVertexFloatCount = 0;
	auto attris = getPMXMeshAttributes();
	for(const auto& attri : attris)
	{
		perVertexFloatCount += getVertexAttributeElementCount(attri);
	}

	m_readyVertexData.resize(vertexCount * perVertexFloatCount);
	for(auto index = 0; index < vertexCount; index++)
	{
		size_t readyDataIndexBase = index * perVertexFloatCount;

		size_t normalIndexBase = index * 3;
		size_t posIndexBase = index * 3;
		size_t uvIndexBase = index * 2;
		

		m_readyVertexData[readyDataIndexBase + 0] = m_pmxRef->m_positions[posIndexBase + 0];
		m_readyVertexData[readyDataIndexBase + 1] = m_pmxRef->m_positions[posIndexBase + 1];
		m_readyVertexData[readyDataIndexBase + 2] = m_pmxRef->m_positions[posIndexBase + 2];

		m_readyVertexData[readyDataIndexBase + 3] = m_pmxRef->m_normals[normalIndexBase + 0];
		m_readyVertexData[readyDataIndexBase + 4] = m_pmxRef->m_normals[normalIndexBase + 1];
		m_readyVertexData[readyDataIndexBase + 5] = m_pmxRef->m_normals[normalIndexBase + 2];

		m_readyVertexData[readyDataIndexBase + 6] = m_pmxRef->m_uvs[uvIndexBase + 0];
		m_readyVertexData[readyDataIndexBase + 7] = m_pmxRef->m_uvs[uvIndexBase + 1];

		CHECK(perVertexFloatCount == 8);
	}
}

engine::PMXMeshComponent::PMXMeshComponent()
	: Component("PMXMeshComponent")
{

}

engine::PMXMeshComponent::~PMXMeshComponent()
{
	m_pmxRef = nullptr;

	if(m_vertexBuffer != nullptr)
	{
		delete m_vertexBuffer;
		m_vertexBuffer = nullptr;
	}

	if(m_stageBuffer!=nullptr)
	{
		delete m_stageBuffer;
		m_stageBuffer = nullptr;
	}
}

void engine::PMXMeshComponent::setNode(std::shared_ptr<SceneNode> node)
{
	m_node = node;
}

std::string engine::PMXMeshComponent::getPath() const
{
	return m_pmxPath;
}

void engine::PMXMeshComponent::setPath(std::string newPath)
{ 
	if(m_pmxPath != newPath)
	{
		m_pmxPath = newPath;
		bPMXMeshChange = true;
	}
}
