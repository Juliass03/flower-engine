#pragma once
#include "../component.h"
#include "../scene.h"

namespace engine{

constexpr glm::vec3 CAMERA_FORWARD = glm::vec3(0.0f,0.0f,1.0f);
constexpr glm::vec3 CAMERA_UP = glm::vec3(0.0f,1.0f,0.0f);



class SceneViewCamera : public Component
{
private:
	std::string m_cameraName = "SceneViewCamera";

	glm::mat4 m_viewMatrix {1.0f};
	glm::mat4 m_projectMatrix {1.0f};
	glm::vec3 m_position {0.0f};

	float m_zNear;
	float m_fovY;
	float m_zFar;
	class SceneViewCameraImpl* m_cameraImpl = nullptr;

private:
	friend class cereal::access;

	template <class Archive>
	void serialize(Archive& ar)
	{
		ar( cereal::base_class<Component>(this),
			m_cameraName,
			m_viewMatrix,
			m_projectMatrix,
			m_zNear,
			m_zFar,
			m_fovY
		);
	}

public:
	void tick(float dt,uint32 viewWidth,uint32 viewHeight);

	SceneViewCamera();
	virtual ~SceneViewCamera();
	virtual size_t getType() override { return EComponentType::SceneViewCamera; }

	float getFoVy() const { return m_fovY; }
	float getZNear() const { return m_zNear; }
	float getZFar() const  { return m_zFar; }
	void setZNear(float in) { m_zNear = in; }
	void setZFar(float in) { m_zFar = in; }

	glm::mat4 getProjection() { return m_projectMatrix; }
	glm::mat4 getView() { return m_viewMatrix; }
	glm::vec3 getPosition() { return m_position; }

private:
	bool bActiveViewport = false;
	bool bActiveViewportLastframe = false;
	bool bHideMouseCursor = false;

	glm::vec2 m_twiceDiffer = glm::vec2(0.0f);
};

template<>
inline size_t getTypeId<SceneViewCamera>()
{
	return EComponentType::SceneViewCamera;
}

}

CEREAL_REGISTER_TYPE_WITH_NAME(engine::SceneViewCamera, "SceneViewCamera");
CEREAL_REGISTER_POLYMORPHIC_RELATION(engine::Component, engine::SceneViewCamera)