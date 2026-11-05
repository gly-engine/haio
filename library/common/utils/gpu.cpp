#include <haio_util.hpp>

namespace Haio::Util::GPU {

Result<size_t> payloadSize(Color color, Size size) {
    if (size.width <= 0 || size.height <= 0) {
        return std::unexpected(Error{ErrorCode::InvalidInput, "gpu container requires positive dimensions"});
    }

    if (blockBytes(color) != 0) return blockedSize(color, size);

    const auto pixels = static_cast<size_t>(size.width) * static_cast<size_t>(size.height);
    switch (color) {
        case Color::RGB565: return pixels * 2;
        case Color::RGBA8888: return pixels * 4;
        default: break;
    }
    return std::unexpected(Error{ErrorCode::UnsupportedFormat, "unsupported gpu payload colour"});
}

Result<Profile> profileFor(Color color, Size size) {
    HAIO_TRY(bytes, payloadSize(color, size));
    switch (color) {
        /**
         * vulkan never named etc1, so the only format number that fits an etc1
         * payload is etc2's: a hardware decoder reads it the same way, and a ktx2
         * written here comes back as etc2 rather than as what was asked for.
         */
        case Color::ETC1:
            return Profile{0, 0, GL_ETC1_RGB8_OES, GL_RGB, VK_FORMAT_ETC2_R8G8B8_UNORM_BLOCK, PVR_ETC1, bytes};
        case Color::ETC2:
            return Profile{0, 0, GL_COMPRESSED_RGB8_ETC2, GL_RGB, VK_FORMAT_ETC2_R8G8B8_UNORM_BLOCK,
                           PVR_ETC2_RGB, bytes};
        case Color::ETC2A:
            return Profile{0, 0, GL_COMPRESSED_RGBA8_ETC2_EAC, GL_RGBA, VK_FORMAT_ETC2_R8G8B8A8_UNORM_BLOCK,
                           PVR_ETC2_RGBA, bytes};
        case Color::EAC_R11:
            return Profile{0, 0, GL_COMPRESSED_R11_EAC, GL_RED, VK_FORMAT_EAC_R11_UNORM_BLOCK,
                           PVR_EAC_R11, bytes};
        case Color::EAC_RG11:
            return Profile{0, 0, GL_COMPRESSED_RG11_EAC, GL_RG, VK_FORMAT_EAC_R11G11_UNORM_BLOCK,
                           PVR_EAC_RG11, bytes};
        case Color::RGB565:
            return Profile{GL_UNSIGNED_SHORT_5_6_5, GL_RGB, GL_RGB565, GL_RGB, VK_FORMAT_R5G6B5_UNORM_PACK16,
                           pvrRaw('r', 'g', 'b', 0, 5, 6, 5, 0), bytes};
        default:
            return Profile{GL_UNSIGNED_BYTE, GL_RGBA, GL_RGBA8, GL_RGBA, VK_FORMAT_R8G8B8A8_UNORM,
                           pvrRaw('r', 'g', 'b', 'a', 8, 8, 8, 8), bytes};
    }
}

uint32_t typeSize(const Profile& profile) {
    return profile.glType == GL_UNSIGNED_SHORT_5_6_5 ? 2u : 1u;
}

Result<Color> colorFromGL(uint32_t glType, uint32_t glFormat, uint32_t glInternal) {
    // a compressed payload has no type and no format: the internal format is the whole answer
    if (glType == 0 && glFormat == 0) {
        switch (glInternal) {
            case GL_ETC1_RGB8_OES: return Color::ETC1;
            case GL_COMPRESSED_RGB8_ETC2: return Color::ETC2;
            case GL_COMPRESSED_RGBA8_ETC2_EAC: return Color::ETC2A;
            case GL_COMPRESSED_R11_EAC: return Color::EAC_R11;
            case GL_COMPRESSED_RG11_EAC: return Color::EAC_RG11;
            default: break;
        }
    }
    if (glType == GL_UNSIGNED_SHORT_5_6_5 && glFormat == GL_RGB && glInternal == GL_RGB565) return Color::RGB565;
    if (glType == GL_UNSIGNED_BYTE && glFormat == GL_RGBA && glInternal == GL_RGBA8) return Color::RGBA8888;
    return std::unexpected(Error{ErrorCode::UnsupportedFormat, "unsupported ktx payload colour"});
}

Result<Color> colorFromVK(uint32_t format) {
    // etc2, not etc1: see profileFor. the bytes are read the same either way
    if (format == VK_FORMAT_ETC2_R8G8B8_UNORM_BLOCK) return Color::ETC2;
    if (format == VK_FORMAT_ETC2_R8G8B8A8_UNORM_BLOCK) return Color::ETC2A;
    if (format == VK_FORMAT_EAC_R11_UNORM_BLOCK) return Color::EAC_R11;
    if (format == VK_FORMAT_EAC_R11G11_UNORM_BLOCK) return Color::EAC_RG11;
    if (format == VK_FORMAT_R5G6B5_UNORM_PACK16) return Color::RGB565;
    if (format == VK_FORMAT_R8G8B8A8_UNORM) return Color::RGBA8888;
    return std::unexpected(Error{ErrorCode::UnsupportedFormat, "unsupported ktx2 payload colour"});
}

Result<Color> colorFromPVR(uint64_t format) {
    if (format == PVR_ETC1) return Color::ETC1;
    if (format == PVR_ETC2_RGB) return Color::ETC2;
    if (format == PVR_ETC2_RGBA) return Color::ETC2A;
    if (format == PVR_EAC_R11) return Color::EAC_R11;
    if (format == PVR_EAC_RG11) return Color::EAC_RG11;
    if (format == pvrRaw('r', 'g', 'b', 0, 5, 6, 5, 0)) return Color::RGB565;
    if (format == pvrRaw('r', 'g', 'b', 'a', 8, 8, 8, 8)) return Color::RGBA8888;
    return std::unexpected(Error{ErrorCode::UnsupportedFormat, "unsupported pvr payload colour"});
}

}
