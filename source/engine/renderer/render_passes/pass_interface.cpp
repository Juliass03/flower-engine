#include "pass_interface.h"

using namespace engine;

void engine::GraphicsPass::init()
{
    // 1. ��������ĳ�ʼ������
	initInner();

    // 2. ע��ص�����
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
    // �Ƴ��ص�ע�ắ��
    m_renderScene->getSceneTextures().removeBeforeSceneTextureRebuildCallback(m_passName);
    m_renderScene->getSceneTextures().removeAfterSceneTextureRebuildCallback(m_passName);

    m_deletionQueue.flush(); 
}
