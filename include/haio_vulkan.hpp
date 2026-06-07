#include <haio_common.hpp>

namespace Haio {
     class Vulkan {
        private:
        struct Vm; 
        Vm* vm;
        Vulkan();
        ~Vulkan();
        Vulkan(const Vulkan&) = delete;
        Vulkan& operator=(const Vulkan&) = delete;

        public:
        static Vulkan &getInstance();
        void transformRGBAtoRGB(const std::vector<uint8_t>& input, std::vector<uint8_t>& output);
    };
}
