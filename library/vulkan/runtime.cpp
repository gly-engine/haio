#include "vm.hpp"
#include <haio.hpp>
#include <volk.h>

namespace Haio {

    Vulkan::Vulkan(): vm(new Vm{}) {
        if (volkInitialize() != VK_SUCCESS) {
            throw std::runtime_error("failed to load vulkan library");
        }

        VkApplicationInfo appInfo{};
        appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
        appInfo.pApplicationName = "Haio";
        appInfo.apiVersion = VK_API_VERSION_1_2;

        VkInstanceCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
        createInfo.pApplicationInfo = &appInfo;

        if (vkCreateInstance(&createInfo, nullptr, &vm->instance) != VK_SUCCESS) {
            throw std::runtime_error("failed to create instance");
        }
        volkLoadInstance(vm->instance);

        uint32_t deviceCount = 0;
        vkEnumeratePhysicalDevices(vm->instance, &deviceCount, nullptr);
        std::vector<VkPhysicalDevice> devices(deviceCount);
        vkEnumeratePhysicalDevices(vm->instance, &deviceCount, devices.data());

        /** @todo choose beter GPU */
        vm->physicalDevice = devices[0];

        uint32_t queueFamilyCount = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(vm->physicalDevice, &queueFamilyCount, nullptr);
        std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
        vkGetPhysicalDeviceQueueFamilyProperties(vm->physicalDevice, &queueFamilyCount, queueFamilies.data());

        int computeFamilyIndex = -1;
        for (uint32_t i = 0; i < queueFamilyCount; i++) {
            if (queueFamilies[i].queueFlags & VK_QUEUE_COMPUTE_BIT) {
                computeFamilyIndex = i;
                break;
            }
        }
        if (computeFamilyIndex == -1) {
            throw std::runtime_error("no compute queue found");
        }

        float queuePriority = 1.0f;
        VkDeviceQueueCreateInfo queueCreateInfo{};
        queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queueCreateInfo.queueFamilyIndex = computeFamilyIndex;
        queueCreateInfo.queueCount = 1;
        queueCreateInfo.pQueuePriorities = &queuePriority;

        VkPhysicalDeviceShaderFloat16Int8Features int8Features{};
        int8Features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_FLOAT16_INT8_FEATURES;
        int8Features.shaderInt8 = VK_TRUE;

        VkDeviceCreateInfo deviceCreateInfo{};
        deviceCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
        deviceCreateInfo.pNext = &int8Features;
        deviceCreateInfo.queueCreateInfoCount = 1;
        deviceCreateInfo.pQueueCreateInfos = &queueCreateInfo;

        if (vkCreateDevice(vm->physicalDevice, &deviceCreateInfo, nullptr, &vm->device) != VK_SUCCESS) {
            throw std::runtime_error("failed to create device");
        }
        volkLoadDevice(vm->device);

        vm->computeFamilyIndex = computeFamilyIndex;
        vkGetDeviceQueue(vm->device, computeFamilyIndex, 0, &vm->computeQueue);
    }

    Vulkan::~Vulkan(){
        if (vm->device) {
            vkDestroyDevice(vm->device, nullptr);
            vm->device = VK_NULL_HANDLE;
        }
        if (vm->instance) {
            vkDestroyInstance(vm->instance, nullptr);
            vm->instance = VK_NULL_HANDLE;
        }

        delete vm;
    }

    Vulkan& Vulkan::getInstance() {
        static Vulkan instance;
        return instance;
    }
}
