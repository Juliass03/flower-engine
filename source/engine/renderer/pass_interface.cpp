#include "pass_interface.h"

using namespace engine;

void engine::GraphicsPass::init()
{
    createCommandBuffersAndSemaphore();

    m_deletionQueue.push([&](){
        releaseCommandBuffersAndSemaphore();
    });

    // 1. 调用子类的初始化函数
	initInner();

    // 2. 注册回调函数
    m_renderScene->getSceneTextures().addBeforeSceneTextureRebuildCallback(m_passName, [&]()
    {
        beforeSceneTextureRecreate();
        releaseCommandBuffersAndSemaphore();
    });

    m_renderScene->getSceneTextures().addAfterSceneTextureRebuildCallback(m_passName,[&]()
    {
        createCommandBuffersAndSemaphore();
        afterSceneTextureRecreate();
    });
}

void engine::GraphicsPass::release()
{
    // 移除回调注册函数
    m_renderScene->getSceneTextures().removeBeforeSceneTextureRebuildCallback(m_passName);
    m_renderScene->getSceneTextures().removeAfterSceneTextureRebuildCallback(m_passName);

    m_deletionQueue.flush(); 
}

void engine::ComputePass::init()
{
    createCommandBuffersAndSemaphore();

    m_deletionQueue.push([&](){
        releaseCommandBuffersAndSemaphore();
    });

    // 1. 调用子类的初始化函数
    initInner();

    // 2. 注册回调函数
    m_renderScene->getSceneTextures().addBeforeSceneTextureRebuildCallback(m_passName, [&]()
    {
        beforeSceneTextureRecreate();
        releaseCommandBuffersAndSemaphore();
    });

    m_renderScene->getSceneTextures().addAfterSceneTextureRebuildCallback(m_passName,[&]()
    {
        createCommandBuffersAndSemaphore();
        afterSceneTextureRecreate();
    });
}

void engine::ComputePass::release()
{
    // 移除回调注册函数
    m_renderScene->getSceneTextures().removeBeforeSceneTextureRebuildCallback(m_passName);
    m_renderScene->getSceneTextures().removeAfterSceneTextureRebuildCallback(m_passName);

    m_deletionQueue.flush(); 
}

VkSemaphore engine::PassCommon::getSemaphore(uint32 i)
{
    return m_semaphore[i];
}

VulkanCommandBuffer* engine::PassCommon::getCommandBuf(uint32 i)
{
    return m_commandbufs[i];
}

void engine::PassCommon::createCommandBuffersAndSemaphore()
{
    m_commandbufs.resize(VulkanRHI::get()->getSwapchainImageViews().size());
    m_semaphore.resize(m_commandbufs.size());

    VkSemaphoreCreateInfo si {};
    si.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    for(size_t i = 0; i<m_commandbufs.size(); i++)
    {
        m_commandbufs[i] = VulkanRHI::get()->createGraphicsCommandBuffer();
        vkCheck(vkCreateSemaphore(VulkanRHI::get()->getDevice(),&si,nullptr,&m_semaphore[i]));
    }
}

void engine::PassCommon::releaseCommandBuffersAndSemaphore()
{
    for(size_t i = 0; i < m_commandbufs.size(); i++)
    {
        delete m_commandbufs[i];
        m_commandbufs[i] = nullptr;

        vkDestroySemaphore(VulkanRHI::get()->getDevice(),m_semaphore[i],nullptr);
    }

    m_commandbufs.clear();
    m_semaphore.clear();
}

void engine::PassCommon::commandBufBegin(uint32 index)
{
    vkCheck(vkResetCommandBuffer(m_commandbufs[index]->getInstance(),0));
    VkCommandBufferBeginInfo cmdBeginInfo = vkCommandbufferBeginInfo(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
    vkCheck(vkBeginCommandBuffer(m_commandbufs[index]->getInstance(),&cmdBeginInfo));
}

void engine::PassCommon::commandBufEnd(uint32 index)
{
    vkCheck(vkEndCommandBuffer(m_commandbufs[index]->getInstance()));
}