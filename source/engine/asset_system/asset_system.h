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
* NOTE: AssetSystem负责维护工程目录，扫描工程目录下的所有文件，将支持的格式打包成引擎专用格式。
*       并且维护一个全局寻址表，以相对路径作为ID，同时检测运行过程中文件是否发生变化，若发生变化则重新加载并刷新引用。
*       维护资源引用。
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