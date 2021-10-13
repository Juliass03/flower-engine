#pragma once
#include "../core/runtime_module.h"
#include "asset_common.h"
#include "../vk/vk_rhi.h"
#include <unordered_set>
#include <mutex>
#include <queue>

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
        Texture2DImage* icon = nullptr;
        VkSampler sampler = VK_NULL_HANDLE;

        void init(const std::string& path,bool flip = true);
        void release();

        // NOTE: EngineIcon的ID将永久驻留在引擎运行阶段
        void* getId();
        IconInfo(){}
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
        bool bEditorSnap = false; // 用于
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
    IconInfo* getIconInfo(const std::string& name);
private:
    std::unordered_map<std::string/*texture name*/,IconInfo/*cache*/> m_cacheIconInfo;
};

struct AssetCache
{
    std::unordered_set<std::string> m_staticMesh;
    std::unordered_set<std::string> m_texture;
    std::unordered_set<std::string> m_materials;

    static AssetCache* s_cache;
    void clear();
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
    void addLoadTextureTask(const std::string&);
    
private:
    void processProjectDirectory(bool firstInit = false);
    void addAsset(std::string path,EAssetFormat format);
    void loadEngineTextures();

    void prepareTextureLoad();
    bool loadTexture2DImage(CombineTexture& inout,const std::string& gameName);


private:
    std::unordered_set<std::string> m_texturesNameLoad;   // 扫描后的纹理名字将会填充到这里
    std::vector<std::string> m_perScanAdditionalTextures; // 每次扫描后多出来的纹理
    std::queue<std::string> m_loadingTextureTasks;
};

}}