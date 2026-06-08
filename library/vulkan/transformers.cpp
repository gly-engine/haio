#include "vm.hpp"
#include <haio.hpp>
#include <shaders/rgba8888_to_rgb888.h>

namespace Haio {

static uint32_t findMemoryType(VkPhysicalDevice physDev, uint32_t filter, VkMemoryPropertyFlags props) {
    VkPhysicalDeviceMemoryProperties memProps;
    vkGetPhysicalDeviceMemoryProperties(physDev, &memProps);
    for (uint32_t i = 0; i < memProps.memoryTypeCount; i++) {
        if ((filter & (1u << i)) && (memProps.memoryTypes[i].propertyFlags & props) == props)
            return i;
    }
    throw std::runtime_error("no suitable memory type");
}

static void makeBuffer(VkDevice device, VkPhysicalDevice physDev, VkDeviceSize size,
                       VkBuffer& buf, VkDeviceMemory& mem) {
    VkBufferCreateInfo info{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
    info.size = size;
    info.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
    info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    vkCreateBuffer(device, &info, nullptr, &buf);

    VkMemoryRequirements req;
    vkGetBufferMemoryRequirements(device, buf, &req);

    VkMemoryAllocateInfo allocInfo{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
    allocInfo.allocationSize = req.size;
    allocInfo.memoryTypeIndex = findMemoryType(physDev, req.memoryTypeBits,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    vkAllocateMemory(device, &allocInfo, nullptr, &mem);
    vkBindBufferMemory(device, buf, mem, 0);
}

void Vulkan::transformRGBAtoRGB(const std::vector<uint8_t>& input, std::vector<uint8_t>& output) {
    const size_t pixelCount = input.size() / 4;
    const VkDeviceSize inSize  = pixelCount * 4;
    const VkDeviceSize outSize = pixelCount * 3;

    VkBuffer inBuf, outBuf;
    VkDeviceMemory inMem, outMem;
    makeBuffer(vm->device, vm->physicalDevice, inSize,  inBuf,  inMem);
    makeBuffer(vm->device, vm->physicalDevice, outSize, outBuf, outMem);

    void* mapped;
    vkMapMemory(vm->device, inMem, 0, inSize, 0, &mapped);
    std::memcpy(mapped, input.data(), inSize);
    vkUnmapMemory(vm->device, inMem);

    VkShaderModuleCreateInfo shaderInfo{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
    shaderInfo.codeSize = rgba8888_to_rgb888_len;
    shaderInfo.pCode    = reinterpret_cast<const uint32_t*>(rgba8888_to_rgb888);
    VkShaderModule shaderMod;
    vkCreateShaderModule(vm->device, &shaderInfo, nullptr, &shaderMod);

    VkDescriptorSetLayoutBinding bindings[2]{};
    for (int b = 0; b < 2; b++) {
        bindings[b].binding         = b;
        bindings[b].descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        bindings[b].descriptorCount = 1;
        bindings[b].stageFlags      = VK_SHADER_STAGE_COMPUTE_BIT;
    }
    VkDescriptorSetLayout descLayout;
    VkDescriptorSetLayoutCreateInfo descLayoutInfo{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
    descLayoutInfo.bindingCount = 2;
    descLayoutInfo.pBindings    = bindings;
    vkCreateDescriptorSetLayout(vm->device, &descLayoutInfo, nullptr, &descLayout);

    VkPipelineLayout pipeLayout;
    VkPipelineLayoutCreateInfo pipeLayoutInfo{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
    pipeLayoutInfo.setLayoutCount = 1;
    pipeLayoutInfo.pSetLayouts    = &descLayout;
    vkCreatePipelineLayout(vm->device, &pipeLayoutInfo, nullptr, &pipeLayout);

    VkComputePipelineCreateInfo pipeInfo{VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO};
    pipeInfo.stage.sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    pipeInfo.stage.stage  = VK_SHADER_STAGE_COMPUTE_BIT;
    pipeInfo.stage.module = shaderMod;
    pipeInfo.stage.pName  = "main";
    pipeInfo.layout       = pipeLayout;
    VkPipeline pipeline;
    vkCreateComputePipelines(vm->device, VK_NULL_HANDLE, 1, &pipeInfo, nullptr, &pipeline);

    VkDescriptorPoolSize poolSize{VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 2};
    VkDescriptorPool descPool;
    VkDescriptorPoolCreateInfo poolInfo{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
    poolInfo.maxSets       = 1;
    poolInfo.poolSizeCount = 1;
    poolInfo.pPoolSizes    = &poolSize;
    vkCreateDescriptorPool(vm->device, &poolInfo, nullptr, &descPool);

    VkDescriptorSet descSet;
    VkDescriptorSetAllocateInfo descAllocInfo{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
    descAllocInfo.descriptorPool     = descPool;
    descAllocInfo.descriptorSetCount = 1;
    descAllocInfo.pSetLayouts        = &descLayout;
    vkAllocateDescriptorSets(vm->device, &descAllocInfo, &descSet);

    VkDescriptorBufferInfo bufInfos[2] = {{inBuf, 0, inSize}, {outBuf, 0, outSize}};
    VkWriteDescriptorSet writes[2]{};
    for (int b = 0; b < 2; b++) {
        writes[b].sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writes[b].dstSet          = descSet;
        writes[b].dstBinding      = b;
        writes[b].descriptorCount = 1;
        writes[b].descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        writes[b].pBufferInfo     = &bufInfos[b];
    }
    vkUpdateDescriptorSets(vm->device, 2, writes, 0, nullptr);

    VkCommandPool cmdPool;
    VkCommandPoolCreateInfo cmdPoolInfo{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
    cmdPoolInfo.queueFamilyIndex = vm->computeFamilyIndex;
    vkCreateCommandPool(vm->device, &cmdPoolInfo, nullptr, &cmdPool);

    VkCommandBuffer cmd;
    VkCommandBufferAllocateInfo cmdAllocInfo{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
    cmdAllocInfo.commandPool        = cmdPool;
    cmdAllocInfo.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    cmdAllocInfo.commandBufferCount = 1;
    vkAllocateCommandBuffers(vm->device, &cmdAllocInfo, &cmd);

    VkCommandBufferBeginInfo beginInfo{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkBeginCommandBuffer(cmd, &beginInfo);
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline);
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pipeLayout, 0, 1, &descSet, 0, nullptr);
    vkCmdDispatch(cmd, static_cast<uint32_t>((pixelCount + 31) / 32), 1, 1);
    vkEndCommandBuffer(cmd);

    VkFence fence;
    VkFenceCreateInfo fenceInfo{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
    vkCreateFence(vm->device, &fenceInfo, nullptr, &fence);

    VkSubmitInfo submitInfo{VK_STRUCTURE_TYPE_SUBMIT_INFO};
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers    = &cmd;
    vkQueueSubmit(vm->computeQueue, 1, &submitInfo, fence);
    vkWaitForFences(vm->device, 1, &fence, VK_TRUE, UINT64_MAX);

    const size_t outOffset = output.size();
    output.resize(outOffset + outSize);
    vkMapMemory(vm->device, outMem, 0, outSize, 0, &mapped);
    std::memcpy(output.data() + outOffset, mapped, outSize);
    vkUnmapMemory(vm->device, outMem);

    vkDestroyFence(vm->device, fence, nullptr);
    vkDestroyCommandPool(vm->device, cmdPool, nullptr);
    vkDestroyDescriptorPool(vm->device, descPool, nullptr);
    vkDestroyPipeline(vm->device, pipeline, nullptr);
    vkDestroyPipelineLayout(vm->device, pipeLayout, nullptr);
    vkDestroyDescriptorSetLayout(vm->device, descLayout, nullptr);
    vkDestroyShaderModule(vm->device, shaderMod, nullptr);
    vkFreeMemory(vm->device, outMem, nullptr);
    vkDestroyBuffer(vm->device, outBuf, nullptr);
    vkFreeMemory(vm->device, inMem, nullptr);
    vkDestroyBuffer(vm->device, inBuf, nullptr);
}

}
