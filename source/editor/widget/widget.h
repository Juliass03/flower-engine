#pragma once
#include "../../engine/engine.h"
#include "../../engine/renderer/renderer.h"
#include "../../imgui/imgui.h"
#include "../../imgui/imgui.h"
#include "../../engine/vk/vk_rhi.h"
#include "../../engine/vk/impl/vk_image.h"
#include "../../engine/asset_system/asset_system.h"
#include "../../engine/scene/scene.h"
#include "../../engine/scene/scene_node.h"

struct ImageInfo
{
private:
	engine::Ref<engine::VulkanImage> icon = nullptr;
	VkSampler sampler;
	std::vector<ImTextureID> cacheId { };

public:
	void setIcon(engine::Ref<engine::VulkanImage> i)
	{
		icon = i;
	}

	void setSampler(VkSampler s)
	{
		sampler = s;
	}

	ImTextureID getId(uint32_t i);
};

struct Drawer
{
	static void vector3(const std::string& label, glm::vec3& values, float resetValue = 0.0f, float columnWidth = 100.0f);
};

namespace engine
{
	class SceneManager;
	class SceneNode;
	class Scene;
}

struct SceneNodeWrapper
{
	std::weak_ptr<engine::SceneNode> node;
	size_t id;
};

class EditorScene
{
public:
	std::weak_ptr<engine::SceneNode> hoverNode; // 上一次选择的节点
	std::weak_ptr<engine::SceneNode> leftClickNode;
	std::weak_ptr<engine::SceneNode> rightClickNode; // 右键按钮，也是选中按钮
	SceneNodeWrapper dragNode; // 拖拽的节点

	bool bRename = false;
	std::weak_ptr<engine::SceneNode> copiedNode;

	static EditorScene& get()
	{
		static EditorScene instance;
		return instance;
	}

	std::weak_ptr<engine::SceneNode> getSelectNode() { return rightClickNode; }
};

class Widget
{
protected:
	engine::Ref<engine::Engine> m_engine;
	engine::Ref<engine::Renderer> m_renderer;
	static uint32_t s_widgetCount;
	// Common widget info.
	bool m_visible = true;
	std::string m_title = "Widget";
	int id;
public:
	std::string getTile() const
	{
		return m_title + std::to_string(s_widgetCount);
	}

	engine::Ref<engine::Renderer> getRenderer()
	{
		return m_renderer;
	}

public:
	virtual ~Widget() {  }
	Widget() = delete;
	Widget(engine::Ref<engine::Engine> engine);

	void tick(size_t);
	void release();

protected:
	// Tick functions
	virtual void onShow() {  }
	virtual void onHide() {  }
	virtual void onTick(size_t) {  }
	virtual void onVisibleTick(size_t) {  }
	virtual void onRelease() {  }

}; 