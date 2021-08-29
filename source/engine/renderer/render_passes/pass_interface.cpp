#include "pass_interface.h"

using namespace engine;

void engine::GraphicsPass::init()
{
    // 1. 调用子类的初始化函数
	initInner();

    // 2. 注册回调函数
    m_renderScene->getSceneTextures().addBeforeSceneTextureRebuildCallback(m_passName, [&]()
    {
        beforeSceneTextureRecreate();
    });

    m_renderScene->getSceneTextures().addAfterSceneTextureRebuildCallback(m_passName,[&]()
    {
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
