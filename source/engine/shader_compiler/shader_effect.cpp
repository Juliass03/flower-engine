#include "shader_effect.h"
#include <algorithm>
#include <SpvReflect/spirv_reflect.h>

using namespace engine;
using namespace engine::shaderCompiler;

void engine::shaderCompiler::ShaderEffect::release()
{
    if(builtLayout != VK_NULL_HANDLE)
    {
        vkDestroyPipelineLayout(VulkanRHI::get()->getDevice(),builtLayout,nullptr);
        builtLayout = VK_NULL_HANDLE;

        for (auto& setLayout : setLayouts)
        {
            if(setLayout!=VK_NULL_HANDLE)
            {
                vkDestroyDescriptorSetLayout(VulkanRHI::get()->getDevice(),setLayout,nullptr);
                setLayout = VK_NULL_HANDLE;
            }
        }
    }
}

void engine::shaderCompiler::ShaderEffect::addStage(VulkanShaderModule* shaderModule,VkShaderStageFlagBits stage)
{
    ShaderStage newStage = { shaderModule,stage};
    m_stages.push_back(newStage);
}

void engine::shaderCompiler::ShaderEffect::fillStages(std::vector<VkPipelineShaderStageCreateInfo>& pipelineStages)
{
    for(auto& s : m_stages)
    {
        VkPipelineShaderStageCreateInfo info{};
        info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        info.pNext = nullptr;
        info.stage = s.stage;
        info.module = s.shaderModule->GetModule();
        info.pName = "main";
        pipelineStages.push_back(info);
    }
}

// 单个描述符集中记录的信息
struct DescriptorSetLayoutData
{
    uint32 set_number;
    VkDescriptorSetLayoutCreateInfo create_info;
    std::vector<VkDescriptorSetLayoutBinding> bindings;
};

uint32 HashDescriptorLayoutInfo(VkDescriptorSetLayoutCreateInfo* info)
{
    std::stringstream ss;

    ss<<info->flags;
    ss<<info->bindingCount;

    for(auto i = 0u; i<info->bindingCount; i++)
    {
        const VkDescriptorSetLayoutBinding& binding = info->pBindings[i];

        ss << binding.binding;
        ss << binding.descriptorCount;
        ss << binding.descriptorType;
        ss << binding.stageFlags;
    }

    auto str = ss.str();

    return fnv1a32(str.c_str(),str.length());
}

void engine::shaderCompiler::ShaderEffect::reflectLayout(VkDevice device,ReflectionOverrides* overrides,int overrideCount)
{
    std::vector<DescriptorSetLayoutData> set_layouts;
    std::vector<VkPushConstantRange> constant_ranges;

    for(auto& s : m_stages)
    {
        // 1. 获取每个spv shader模块的反射信息。
        SpvReflectShaderModule spvmodule;
        SpvReflectResult result = spvReflectCreateShaderModule(
            s.shaderModule->GetCodes().size() * sizeof(uint32_t),
            s.shaderModule->GetCodes().data(),
            &spvmodule
        );

        // 2. 查询反射符集数目与详细信息并存到容器中
        uint32 count = 0;
        result = spvReflectEnumerateDescriptorSets(&spvmodule,&count,NULL); CHECK(result == SPV_REFLECT_RESULT_SUCCESS);
        std::vector<SpvReflectDescriptorSet*> sets(count);
        result = spvReflectEnumerateDescriptorSets(&spvmodule,&count,sets.data()); CHECK(result==SPV_REFLECT_RESULT_SUCCESS);

        // 3. 遍历每个查询到的反射符集
        for(size_t i_set = 0; i_set < sets.size(); ++i_set)
        {
            const SpvReflectDescriptorSet& refl_set = *(sets[i_set]);
            DescriptorSetLayoutData layout = {};
            layout.bindings.resize(refl_set.binding_count);

            // 3.1 遍历反射符集的每个binding
            for(uint32 i_binding = 0; i_binding < refl_set.binding_count; ++i_binding)
            {
                const SpvReflectDescriptorBinding& refl_binding = *(refl_set.bindings[i_binding]);

                // 3.1.1 填充VkDescriptorSetLayoutBinding信息
                VkDescriptorSetLayoutBinding& layout_binding = layout.bindings[i_binding];
                layout_binding.binding = refl_binding.binding;
                layout_binding.descriptorType = static_cast<VkDescriptorType>(refl_binding.descriptor_type);
                layout_binding.stageFlags = static_cast<VkShaderStageFlagBits>(spvmodule.shader_stage);

                for(int ov = 0; ov < overrideCount; ov++)
                {
                    if(strcmp(refl_binding.name,overrides[ov].name)==0)
                    {
                        // 使用指定的描述符类型覆盖反射出来的类型
                        layout_binding.descriptorType = overrides[ov].overridenType;
                    }
                }

                layout_binding.descriptorCount = 1;
                for(uint32 i_dim = 0; i_dim < refl_binding.array.dims_count; ++i_dim)
                {
                    layout_binding.descriptorCount *= refl_binding.array.dims[i_dim];
                }

                // 存储当前binding
                ReflectedBinding reflected { };
                reflected.binding = layout_binding.binding;
                reflected.set = refl_set.set;
                reflected.type = layout_binding.descriptorType;


                bindings[refl_binding.name] = reflected;
            }

            layout.set_number = refl_set.set;
            layout.create_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
            layout.create_info.bindingCount = refl_set.binding_count;
            layout.create_info.pBindings = layout.bindings.data();

            set_layouts.push_back(layout);
        }

        // 4. 推送常量反射信息读取
        result = spvReflectEnumeratePushConstantBlocks(&spvmodule,&count,NULL);
        CHECK(result == SPV_REFLECT_RESULT_SUCCESS);
        std::vector<SpvReflectBlockVariable*> pconstants(count);
        result = spvReflectEnumeratePushConstantBlocks(&spvmodule,&count,pconstants.data());
        CHECK(result == SPV_REFLECT_RESULT_SUCCESS);
        if(count > 0)
        {
            VkPushConstantRange pcs{};
            pcs.offset = pconstants[0]->offset;
            pcs.size = pconstants[0]->size;
            pcs.stageFlags = s.stage;
            constant_ranges.push_back(pcs);
        }
    }

    // 合并描述符集
    std::array<DescriptorSetLayoutData,4> merged_layouts;
    for(int i = 0; i < 4; i++)
    {
        DescriptorSetLayoutData& ly = merged_layouts[i];

        ly.set_number = i;
        ly.create_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        std::unordered_map<int,VkDescriptorSetLayoutBinding> binds;
        for(auto& s : set_layouts)
        {
            if(s.set_number == i)
            {
                for(auto& b:s.bindings)
                {
                    auto it = binds.find(b.binding);
                    if(it==binds.end())
                    {
                        binds[b.binding] = b;
                    }
                    else
                    {
                        binds[b.binding].stageFlags |= b.stageFlags;
                    }

                }
            }
        }
        for(auto [k,v] : binds)
        {
            ly.bindings.push_back(v);
        }

        std::sort(ly.bindings.begin(),ly.bindings.end(),[](VkDescriptorSetLayoutBinding& a,VkDescriptorSetLayoutBinding& b)
        {
            return a.binding < b.binding;
        });

        ly.create_info.bindingCount = (uint32)ly.bindings.size();
        ly.create_info.pBindings = ly.bindings.data();
        ly.create_info.flags = 0;
        ly.create_info.pNext = 0;

        if(ly.create_info.bindingCount > 0)
        {
            setHashes[i] = HashDescriptorLayoutInfo(&ly.create_info);
            vkCreateDescriptorSetLayout(device,&ly.create_info,nullptr,&setLayouts[i]);
        }
        else
        {
            setHashes[i] = 0;
            setLayouts[i] = VK_NULL_HANDLE;
        }
    }

    VkPipelineLayoutCreateInfo mesh_pipeline_layout_info{};
    mesh_pipeline_layout_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    mesh_pipeline_layout_info.pNext = nullptr;
    mesh_pipeline_layout_info.flags = 0;
    mesh_pipeline_layout_info.pPushConstantRanges = constant_ranges.data();
    mesh_pipeline_layout_info.pushConstantRangeCount = (uint32)constant_ranges.size();

    std::array<VkDescriptorSetLayout,4> compactedLayouts;
    int s = 0;
    for(int i = 0; i<4; i++)
    {
        if(setLayouts[i] != VK_NULL_HANDLE)
        {
            compactedLayouts[s] = setLayouts[i];
            s++;
        }
    }

    mesh_pipeline_layout_info.setLayoutCount = s;
    mesh_pipeline_layout_info.pSetLayouts = compactedLayouts.data();

    vkCreatePipelineLayout(device,&mesh_pipeline_layout_info,nullptr,&builtLayout);
}