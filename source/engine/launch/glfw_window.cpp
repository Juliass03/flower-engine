#include "launch_engine_loop.h"
#include "../core/cvar.h"
#include "../core/core.h"
#include <sstream>
#include "../engine.h"
#include "../vk/vk_rhi.h"
#include "../asset_system/asset_system.h"
#include <glfw/glfw3.h>
#include <stb/stb_image.h>
#include "../core/file_system.h"
#include "../renderer/mesh.h"
#include "../renderer/pmx_mesh.h"
#include "../renderer/texture.h"

namespace engine{

EngineLoop g_engineLoop;

static AutoCVarInt32 cVarWindowDefaultShowMode(
	"r.Window.ShowMode",
	"Window display mode. 0 is full screen without tile, 1 is full screen with tile, 2 is custom size by r.Window.Width & .Height",
	"Window",
	1,
	CVarFlags::ReadOnly | CVarFlags::InitOnce
);

static AutoCVarInt32 cVarWindowDefaultWidth(
    "r.Window.Width",
    "Window default width which only work when r.Window.ShowMode equal 2.",
    "Window",
    1024,
    CVarFlags::ReadOnly | CVarFlags::InitOnce
);

static AutoCVarInt32 cVarWindowDefaultHeight(
    "r.Window.Height",
    "Window default height which only work when r.Window.ShowMode equal 2.",
    "Window",
    960,
    CVarFlags::ReadOnly | CVarFlags::InitOnce
);

static AutoCVarString cVarTileName(
    "r.Window.TileName",
    "Window tile name.",
    "Window",
    "flower engine - ver 0.0.1 - vulkan 1.2",
    CVarFlags::ReadAndWrite
);

static AutoCVarInt32 cVarFpsMode(
    "r.Window.FpsMode",
    "Window fps mode,0 is free,1 is 30,2 is 60,3 is 120,4 is 240.",
    "Window",
    0,
    CVarFlags::ReadAndWrite
);

enum class EFpsMode : uint32
{
    Free = 0,
    FixFPS30 = 1,
    FixFPS60 = 2,
    FixFPS120 = 3,
    FixFPS240 = 4,
};

float getFps()
{
    auto mode = (EFpsMode)cVarFpsMode.get();
    switch(mode)
    {
    case engine::EFpsMode::Free:
        return -1.0f;
    case engine::EFpsMode::FixFPS30:
        return 30.0f;
    case engine::EFpsMode::FixFPS60:
        return 60.0f;
    case engine::EFpsMode::FixFPS120:
        return 120.0f;
    case engine::EFpsMode::FixFPS240:
        return 240.0f;
    default:
        return -1.0f;
    }
    return -1.0f;
}

enum class EWindowShowMode : int8
{
    Min = -1,
	FullScreenWithoutTile = 0,
	FullScreenWithTile = 1,
	Free = 2,

    Max,
};

EWindowShowMode getWindowShowMode()
{
    int32 Type = glm::clamp(
        cVarWindowDefaultShowMode.get(), 
        (int32)EWindowShowMode::Min + 1,
        (int32)EWindowShowMode::Max - 1
    );

    return EWindowShowMode(Type);
}

static void ResizeCallBack(GLFWwindow* window, int width, int height)
{
    auto app = reinterpret_cast<GLFWWindowData*>(glfwGetWindowUserPointer(window));
    app->CallbackOnResize(width,height);
}

static void MouseMoveCallback(GLFWwindow* window, double xpos, double ypos)
{
    auto app = reinterpret_cast<GLFWWindowData*>(glfwGetWindowUserPointer(window));
    app->CallbackOnMouseMove(xpos,ypos);
}

static void MouseButtonCallback(GLFWwindow* window, int button, int action, int mods)
{
    auto app = reinterpret_cast<GLFWWindowData*>(glfwGetWindowUserPointer(window));
    app->CallbackOnMouseButton(button,action,mods);
}

static void ScrollCallback(GLFWwindow* window,double xoffset,double yoffset)
{
    auto app = reinterpret_cast<GLFWWindowData*>(glfwGetWindowUserPointer(window));
    app->CallbackOnScroll(xoffset,yoffset);
}

static void WindowFocusCallBack(GLFWwindow* window, int focused)
{
    auto app = reinterpret_cast<GLFWWindowData*>(glfwGetWindowUserPointer(window));
    app->bFoucus = (bool)focused;
}

void GLFWInit()
{
    GLFWWindowData& windowData = g_windowData;

    glfwInit();
    glfwWindowHint(GLFW_CLIENT_API,GLFW_NO_API);
    EWindowShowMode showMode = getWindowShowMode();

    uint32 width;
    uint32 height;
    if(showMode == EWindowShowMode::FullScreenWithoutTile)
    {
        const GLFWvidmode* mode = glfwGetVideoMode(glfwGetPrimaryMonitor());

        width  = mode->width;
        height = mode->height;

        glfwWindowHint(GLFW_RED_BITS, mode->redBits);
        glfwWindowHint(GLFW_GREEN_BITS, mode->greenBits);
        glfwWindowHint(GLFW_BLUE_BITS, mode->blueBits);
        glfwWindowHint(GLFW_REFRESH_RATE, mode->refreshRate);

        windowData.window = glfwCreateWindow(width, height,cVarTileName.get().c_str(),glfwGetPrimaryMonitor(),nullptr);


        glfwSetWindowPos(windowData.window,(mode->width - width)/2, (mode->height - height)/2);
    }
    else if(showMode == EWindowShowMode::FullScreenWithTile)
    {
        const GLFWvidmode* mode = glfwGetVideoMode(glfwGetPrimaryMonitor());
        width = mode->width;
        height = mode->height;
        glfwWindowHint(GLFW_MAXIMIZED, GL_TRUE);
        windowData.window = glfwCreateWindow(width,height,cVarTileName.get().c_str(),nullptr,nullptr);
    }
    else if(showMode == EWindowShowMode::Free)
    {
        width  = std::max(10,cVarWindowDefaultWidth.get());
        height = std::max(10,cVarWindowDefaultHeight.get());
        const GLFWvidmode* mode = glfwGetVideoMode(glfwGetPrimaryMonitor());

        windowData.window = glfwCreateWindow(width,height,cVarTileName.get().c_str(),nullptr,nullptr);
        glfwSetWindowPos(windowData.window,(mode->width - width)/2, (mode->height - height)/2);
    }
    else
    {
        LOG_FATAL("Unknown windowShowMode: {0}.",(int32)showMode);
    }
    LOG_INFO("Create window size: ({0},{1}).",width,height);

    // 注册回调传输指针
    glfwSetWindowUserPointer(windowData.window,&windowData);
    glfwSetFramebufferSizeCallback(windowData.window, ResizeCallBack);
    glfwSetMouseButtonCallback(windowData.window, MouseButtonCallback);
    glfwSetCursorPosCallback(windowData.window, MouseMoveCallback);
    glfwSetScrollCallback(windowData.window, ScrollCallback);
    glfwSetWindowFocusCallback(windowData.window, WindowFocusCallBack);

    GLFWimage images[1]; 
    images[0].pixels = stbi_load(s_iconPath, &images[0].width, &images[0].height, 0, 4);
    glfwSetWindowIcon(windowData.window, 1, images); 
    stbi_image_free(images[0].pixels);
}

void GLFWRelease()
{
    glfwDestroyWindow(g_windowData.window);
    glfwTerminate();
}

bool EngineLoop::init()
{
    GLFWInit();
    VulkanRHI::get()->init(g_windowData.window);
    asset_system::EngineAsset::get()->init();

    MeshLibrary::get()->init();
    TextureLibrary::get()->init();

    // 引擎初始化
    m_engine.init();

    // Timer初始化
    g_timer.globalTimeInit();

	return true;
}

float computeSmoothDt(float oldDt,float dt)
{
    constexpr double frames_to_accumulate = 5;
    constexpr double delta_feedback       = 1.0 / frames_to_accumulate;

    constexpr double fps_min = 30.0;
    constexpr double delta_max = 1.0f / fps_min;

    const double delta_clamped  = dt > delta_max ? delta_max : dt;
    return (float)(oldDt * (1.0 - delta_feedback) + delta_clamped * delta_feedback);
}

void EngineLoop::guardedMain()
{
    // 程序运行的帧计时
    g_timer.frameTimeInit();
    
    float dt = 0.0166f;
    ETickResult result = ETickResult::Continue;
    while(ETickResult::Continue == result && !glfwWindowShouldClose(g_windowData.window) && m_run)
    {
        glfwPollEvents();

        float requestFps = getFps();
        g_timer.frameTick();

        if(!g_windowData.bFoucus)
        {
            // NOTE: 间隔10s刷新一次防止Vulkan设备丢失
            if(g_timer.frameDeltaTime() < 10.0f)
            {
                continue;
            }
        }

        if(requestFps > 0.0f && g_timer.frameDeltaTime() > 1.0f / requestFps)
        {
            g_timer.frameReset();
            dt = m_frameCount == 0 ? 1.0f / requestFps : g_timer.frameDeltaTime();

            m_smooth_dt = computeSmoothDt(m_smooth_dt,dt);
            result = m_engine.tick(dt,m_smooth_dt);
            g_timer.frameCountAdd();
            m_frameCount ++;

            g_timer.m_currentFps = requestFps;
            g_timer.m_currentSmoothFps = requestFps;
        }
        else if((EFpsMode)cVarFpsMode.get() == EFpsMode::Free)
        {
            static float passTime = 0.0f;
            static int32_t passFrame = 0;

            g_timer.frameReset();
            dt = m_frameCount == 0 ? dt : g_timer.frameDeltaTime();
            m_smooth_dt = computeSmoothDt(m_smooth_dt,dt);
            result = m_engine.tick(dt,m_smooth_dt);
            g_timer.frameCountAdd();

            m_frameCount ++;
            passFrame++;
            passTime += dt;
            if(passTime >= 1.0f)
            {
                requestFps = passFrame / passTime;
                passTime = 0;
                passFrame = 0;
                g_timer.m_currentSmoothFps = requestFps;
            }

            g_timer.m_currentFps = 1.0f / dt;
            
        }
    }

    VulkanRHI::get()->waitIdle();
}

void EngineLoop::release()
{
    m_engine.release();

    PMXManager::get()->release();
    MeshLibrary::get()->release();
    asset_system::EngineAsset::get()->release();
    TextureLibrary::get()->release();
    VulkanRHI::get()->release();
    GLFWRelease();
}

}