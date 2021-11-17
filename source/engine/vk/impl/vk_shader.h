#pragma once
#include "vk_common.h"
#include "vk_device.h"
#include "vk_descriptor.h"
#include <fstream>

namespace engine{

enum class EShaderStage
{
    Vert,
    Frag,
};

class VulkanShaderModule
{
private:
    VkDevice m_device;
    struct State
    {
        std::vector<uint32_t> opcodes = {};
        VkShaderModule shaderModule = VK_NULL_HANDLE;
        bool ok = false;
    };
    State s = {};

    VulkanShaderModule() = default;
public:
    ~VulkanShaderModule()
    {
        if(s.ok)
        {
            if (s.shaderModule != VK_NULL_HANDLE) 
            {
                vkDestroyShaderModule(m_device, s.shaderModule, nullptr);
                s.shaderModule = VK_NULL_HANDLE;
            }
            s.ok = false;
        }
    }
    VkDevice GetDevice() { return m_device; }
    operator VkShaderModule() { return s.shaderModule; }
    std::vector<uint32_t>& GetCodes() { return s.opcodes; }

    static VulkanShaderModule* create(const VkDevice device,const std::string& filename)
    {
        auto file = std::ifstream(filename,std::ios::binary);
        if(file.bad())
        {
            LOG_IO_FATAL("Open shader file£º{} failed.",filename);
            return nullptr;
        }

        file.seekg(0,std::ios::end);
        int length = (int)file.tellg();

        VulkanShaderModule* res = new VulkanShaderModule();

        res->s.opcodes.resize((size_t)(length/4));
        file.seekg(0,std::ios::beg);
        file.read((char*)res->s.opcodes.data(),res->s.opcodes.size()*4);

        VkShaderModuleCreateInfo ci{};
        ci.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        ci.codeSize = res->s.opcodes.size() * 4;
        ci.pCode = res->s.opcodes.data();

        if (vkCreateShaderModule(device, &ci, nullptr, &res->s.shaderModule) != VK_SUCCESS) 
        {
            LOG_GRAPHICS_ERROR("Create shader module {0} failed.",filename);
        }

        res->m_device = device;
        res->s.ok = true;
        return res;
    }

    bool Ok() const
    {
        return s.ok;
    }

    VkShaderModule GetModule() const
    {
        return s.shaderModule;
    }
};

class VulkanShaderCache
{
public:
    VulkanShaderModule* getShader(const std::string& path,bool reload)
    {
        auto it = m_moduleCache.find(path);
        if(it == m_moduleCache.end())
        {
            VulkanShaderModule* newShader = VulkanShaderModule::create(m_device,path);
            m_moduleCache[path] = newShader;
        }
        else if(reload)
        {
            delete m_moduleCache[path];

            VulkanShaderModule* newShader = VulkanShaderModule::create(m_device,path);
            m_moduleCache[path] = newShader;
        }
        return m_moduleCache[path];
    }

    void release()
    {
        for (auto shaders : m_moduleCache)
        {
            delete shaders.second;
        }
    }

    void init(VkDevice device)
    {
        m_device = device;
    };

private:
    VkDevice m_device{ VK_NULL_HANDLE };
    std::unordered_map<std::string,VulkanShaderModule*> m_moduleCache;
};

}
