#pragma once
#include "../core/runtime_module.h"
#include "asset_common.h"
#include "../vk/vk_rhi.h"

namespace engine
{
    struct CombineTexture;
    struct Mesh;
}

namespace engine{ namespace asset_system{

extern bool g_assetFolderDirty;

class EngineAsset
{
private:
    bool bInit = false;
    static EngineAsset* s_asset;

public:
    struct IconInfo
    {
        Texture2DImage* icon;
        VkSampler sampler;

        void init(const std::string& path,bool flip = true);
        void release();

        // NOTE: EngineIcon的ID将永久驻留在引擎运行阶段
        void* getId();

        IconInfo(const std::string& path,bool flip = true)
        {
            init(path,flip);
        }

        ~IconInfo()
        {
            release();
        }

    private:
        void* cacheId = nullptr;
    };

    
    IconInfo* iconMaterial = nullptr;
    IconInfo* iconMesh = nullptr;
    IconInfo* iconFolder = nullptr;
    IconInfo* iconFile = nullptr;
    IconInfo* iconBack = nullptr;
    IconInfo* iconHome = nullptr;
    IconInfo* iconFlash = nullptr;
    IconInfo* iconProject = nullptr;
    IconInfo* iconTexture = nullptr;

    static EngineAsset* get(){ return s_asset; }

    void init();
    void release();
};

// NOTE: AssetSystem为异步加载系统
//       各种AssetLibrary如MeshLibrary、TextureLibrary产生加载命令
//       调用一次std::asyc，并传入一个lambda作为加载完毕的命令
class AssetSystem : public IRuntimeModule
{
public:
    AssetSystem(Ref<ModuleManager>);

public:
    virtual bool init() override;
    virtual void tick(float dt) override;
    virtual void release() override;

    bool loadTexture2DImage(CombineTexture& inout,const std::string& gameName,std::function<void(CombineTexture)>&& loadedCallback);
    bool loadMesh(Mesh& inout,const std::string& gameName,std::function<void(Mesh*)>&& loadedCallback);
private:
    void processProjectDirectory();

    void addAsset(std::string path,EAssetFormat format);
    void loadEngineTextures();
};

}}