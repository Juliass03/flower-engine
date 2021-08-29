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