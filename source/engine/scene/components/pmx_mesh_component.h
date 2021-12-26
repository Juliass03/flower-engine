#pragma once
#include "../component.h"
#include "../scene.h"
#include "../../vk/vk_rhi.h"

namespace engine{

class PMXMesh;

class PMXMeshComponent : public Component
{
private:
	std::string m_pmxPath = "";
	std::weak_ptr<SceneNode> m_node;

private:
	bool bPMXMeshChange = false;
	VulkanVertexBuffer* m_vertexBuffer = nullptr;
	VulkanBuffer* m_stageBuffer = nullptr;
	Ref<PMXMesh> m_pmxRef = nullptr;

	std::vector<float> m_readyVertexData {};
	VkDeviceSize getComponentVertexSize() const;

	void prepareCurrentFrameVertexData();

private:
	friend class cereal::access;

	template <class Archive>
	void serialize(Archive& ar)
	{
		ar(cereal::base_class<Component>(this),
			m_pmxPath,
			m_node
		);
	}

	void preparePMX();

public:
	PMXMeshComponent();
	~PMXMeshComponent();

	virtual size_t getType() override { return EComponentType::PMXMeshComponent; }
	void setNode(std::shared_ptr<SceneNode> node);
	std::string getPath() const;

	void setPath(std::string path);

	void OnRenderCollect(VkCommandBuffer cmd,VkPipelineLayout pipelinelayout);
	void OnShadowRenderCollect(VkCommandBuffer cmd,VkPipelineLayout pipelinelayout, uint32_t cascadeIndex);
	void OnRenderTick(VkCommandBuffer cmd);

	void OnSceneTick();
};

template<>
inline size_t getTypeId<PMXMeshComponent>()
{
	return EComponentType::PMXMeshComponent;
}

template<>
inline void Scene::addComponent<PMXMeshComponent>(std::shared_ptr<PMXMeshComponent> component,std::shared_ptr<SceneNode> node)
{
	if(component&&!node->hasComponent(getTypeId<PMXMeshComponent>()))
	{
		node->setComponent(component);
		m_components[component->getType()].push_back(component);
		component->setNode(node);
	}
}
}


CEREAL_REGISTER_TYPE_WITH_NAME(engine::PMXMeshComponent, "PMXMeshComponent");
CEREAL_REGISTER_POLYMORPHIC_RELATION(engine::Component, engine::PMXMeshComponent)