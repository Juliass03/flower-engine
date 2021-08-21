#pragma once
#include "../core/runtime_module.h"
#include "asset_common.h"
#include "../vk/vk_rhi.h"

namespace engine{ namespace asset_system{

class TextureManager;

class EngineAsset
{
private:
    bool bInit = false;
    static EngineAsset* s_asset;

public:
    Texture2DImage* iconError;
    Texture2DImage* iconInfo;
    Texture2DImage* iconNotify;
    Texture2DImage* iconOther;
    Texture2DImage* iconWarn;

    static EngineAsset* get(){ return s_asset; }

    void init();
    void release();
};

/**
* NOTE: AssetSystem����ά������Ŀ¼��ɨ�蹤��Ŀ¼�µ������ļ�����֧�ֵĸ�ʽ���������ר�ø�ʽ��
*       ����ά��һ��ȫ��Ѱַ�������·����ΪID��ͬʱ������й������ļ��Ƿ����仯���������仯�����¼��ز�ˢ�����á�
*       ά����Դ���á�
**/
class AssetSystem : public IRuntimeModule
{
public:
    AssetSystem(Ref<ModuleManager>);

public:
    virtual bool init() override;
    virtual void tick(float dt) override;
    virtual void release() override;

private:
    void scanProject();
};

}}