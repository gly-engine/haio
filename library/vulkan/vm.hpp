#pragma once
#include <haio_vulkan.hpp>
#include <volk.h>
#include <cstring>

struct Haio::Vulkan::Vm {
    VkInstance instance;
    VkPhysicalDevice physicalDevice;
    VkDevice device;
    VkQueue computeQueue;
    uint32_t computeFamilyIndex;
};
