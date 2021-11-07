#include "sceneview_camera.h"
#include "../../core/input.h"
#include <glm/gtx/matrix_decompose.hpp>
#include "../../launch/launch_engine_loop.h"
#include "../../renderer/renderer.h"

namespace engine
{
    // NOTE: �����Ϊ����Ԥ����������߱�����Ϸ��ʵ������һЩ���ܡ�
    namespace CameraUtils
    {
        const float yawDefault = -90.0f;
        const float pitchDefault = 0.0f;
        const float speedDefault = 10.0f;
        const float sensitivityDefault = 0.1f;

        // NOTE: �����fov���׵�����3dѣ��
        const float zoomDefault = 45.0f;

        enum class MoveType
        {
            Forward,
            Backward,
            Left,
            Right,
        };
    }

    class SceneViewCameraImpl
    {
    public:
        SceneViewCameraImpl(
            glm::vec3 position = glm::vec3(0.0f,0.0f,0.0f),
            glm::vec3 up = glm::vec3(0.0f,1.0f,0.0f),
            float yaw = CameraUtils::yawDefault,
            float pitch = CameraUtils::pitchDefault):
            Front(glm::vec3(0.0f,0.0f,1.0f)),
            MovementSpeed(CameraUtils::speedDefault),
            MouseSensitivity(CameraUtils::sensitivityDefault),
            Zoom(CameraUtils::zoomDefault)
        {
            Position = position;
            WorldUp = up;
            Yaw = yaw;
            Pitch = pitch;
            UpdateCameraVectors();
        }

        SceneViewCameraImpl(float posX,float posY,float posZ,float upX,float upY,float upZ,float yaw,float pitch):
            Front(glm::vec3(0.0f,0.0f,1.0f)),
            MovementSpeed(CameraUtils::speedDefault),
            MouseSensitivity(CameraUtils::sensitivityDefault),
            Zoom(CameraUtils::zoomDefault)
        {
            Position = glm::vec3(posX,posY,posZ);
            WorldUp = glm::vec3(upX,upY,upZ);
            Yaw = yaw;
            Pitch = pitch;
            UpdateCameraVectors();
        }

        auto GetPosition()
        {
            return Position;
        }

        // ����glm::lookat����ͨ������ŷ���Ƿ�ʽ����View Matrix
        glm::mat4 GetViewMatrix()
        {
            return glm::lookAt(Position,Position+Front,Up);
        }

        // ��������¼�
        void ProcessKeyboard(CameraUtils::MoveType direction,float deltaTime)
        {
            float velocity = MovementSpeed*deltaTime;
            if(direction==CameraUtils::MoveType::Forward)
                Position += Front*velocity;
            if(direction==CameraUtils::MoveType::Backward)
                Position -= Front*velocity;
            if(direction==CameraUtils::MoveType::Left)
                Position -= Right*velocity;
            if(direction==CameraUtils::MoveType::Right)
                Position += Right*velocity;
        }

        // ��������ƶ�
        void ProcessMouseMovement(float xoffset,float yoffset,bool constrainPitch = true)
        {
            xoffset *= MouseSensitivity;
            yoffset *= MouseSensitivity;

            Yaw += xoffset;
            Pitch += yoffset;

            // ��ֹ��ת
            if(constrainPitch)
            {
                if(Pitch>89.0f)
                    Pitch = 89.0f;
                if(Pitch<-89.0f)
                    Pitch = -89.0f;
            }

            UpdateCameraVectors();
        }

        // ��������¼�
        void ProcessMouseScroll(float yoffset)
        {
            MovementSpeed += (float)yoffset;

            MovementSpeed = glm::clamp(MovementSpeed,1.0f,20.0f);
        }

        auto GetProjectMatrix(uint32_t app_width,uint32_t app_height)
        {
            float tmpZnear = zNear;
            float tmpZFar = zFar;
            if(engine::reverseZOpen())
            {
                tmpZnear = zFar;
                tmpZFar = zNear;
            }

            auto ret = glm::perspective(glm::radians(Zoom),(float)app_width/(float)app_height,tmpZnear,tmpZFar);
            
            //ret[1][1] *= -1.0f; // now flip y at tonemapper pass
            return ret;
        }

        float GetFovY() const
        {
            return glm::radians(Zoom);
        }

    public:
        // �������
        glm::vec3 Position;
        glm::vec3 Front;
        glm::vec3 Up;
        glm::vec3 Right;
        glm::vec3 WorldUp;

        // ŷ����
        float Yaw;
        float Pitch;

        float zNear = 0.1f;
        float zFar = 200.0f;

        // ����
        float MovementSpeed;
        float MouseSensitivity;
        float Zoom;

        // ��һ����������¼�
        bool firstMouse = true;

        float lastX;
        float lastY;

    private:
        void UpdateCameraVectors()
        {
            glm::vec3 front;
            front.x = cos(glm::radians(Yaw))*cos(glm::radians(Pitch));
            front.y = sin(glm::radians(Pitch));
            front.z = sin(glm::radians(Yaw))*cos(glm::radians(Pitch));
            Front = glm::normalize(front);
            Right = glm::normalize(glm::cross(Front,WorldUp));
            Up = glm::normalize(glm::cross(Right,Front));
        }
    };
}

void engine::SceneViewCamera::tick(float dt,uint32 viewWidth,uint32 viewHeight)
{
    if(m_cameraImpl==nullptr)
    {
        m_cameraImpl = new SceneViewCameraImpl();
    }

    if(m_cameraImpl->firstMouse)
    {
        m_cameraImpl->lastX = Input::getMouseX();
        m_cameraImpl->lastY = Input::getMouseY();
        m_cameraImpl->firstMouse = false;
    }

    if(Input::isMouseInViewport() && Input::isMouseButtonPressed(Mouse::ButtonRight))
    {
        bActiveViewport = true;
    }
    else
    {
        bActiveViewport = false;
    }

    if(bActiveViewport && !bHideMouseCursor)
    {
        bHideMouseCursor = true;
        glfwSetInputMode(g_windowData.window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
    }

    if(!bActiveViewport && bHideMouseCursor)
    {
        bHideMouseCursor = false;
        glfwSetInputMode(g_windowData.window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
    }

    if(bActiveViewportLastframe && !bActiveViewport) // ȡ�������һ˲��
    {
        m_cameraImpl->lastX = Input::getMouseX();
        m_cameraImpl->lastY = Input::getMouseY();
    }
    
    float xoffset = 0.0f;
    float yoffset = 0.0f;
    if(bActiveViewportLastframe && bActiveViewport) // �Ǽ���˲��
    {
        xoffset = Input::getMouseX() - m_cameraImpl->lastX;
        yoffset = m_cameraImpl->lastY-Input::getMouseY();
    }

    bActiveViewportLastframe = bActiveViewport;

    if(bActiveViewport)
    {
        m_cameraImpl->lastX = Input::getMouseX();
        m_cameraImpl->lastY = Input::getMouseY();

        m_cameraImpl->ProcessMouseMovement(xoffset,yoffset);
        m_cameraImpl->ProcessMouseScroll(Input::getScrollOffset().y);

        if(Input::isKeyPressed(Key::W))
            m_cameraImpl->ProcessKeyboard(CameraUtils::MoveType::Forward,dt);
        if(Input::isKeyPressed(Key::S))
            m_cameraImpl->ProcessKeyboard(CameraUtils::MoveType::Backward,dt);
        if(Input::isKeyPressed(Key::A))
            m_cameraImpl->ProcessKeyboard(CameraUtils::MoveType::Left,dt);
        if(Input::isKeyPressed(Key::D))
            m_cameraImpl->ProcessKeyboard(CameraUtils::MoveType::Right,dt);
    }

    m_viewMatrix = m_cameraImpl->GetViewMatrix();
    m_projectMatrix = m_cameraImpl->GetProjectMatrix(viewWidth,viewHeight);
    m_position = m_cameraImpl->GetPosition();
    setZNear(m_cameraImpl->zNear);
    setZFar(m_cameraImpl->zFar);

    m_fovY = m_cameraImpl->GetFovY();
}

engine::SceneViewCamera::SceneViewCamera()
	: Component("CameraComponent")
{

}

engine::SceneViewCamera::~SceneViewCamera()
{
    if(m_cameraImpl!=nullptr)
    {
        delete m_cameraImpl;
        m_cameraImpl = nullptr;
    }
}