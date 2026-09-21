#include <haio_util.hpp>

namespace Haio::Util::GPU {

Result<size_t> payloadSize(Color color, Size size) {
    if (size.width <= 0 || size.height <= 0) {
        return std::unexpected(Error{ErrorCode::InvalidInput, "gpu container requires positive dimensions"});
    }

    const auto pixels = static_cast<size_t>(size.width) * static_cast<size_t>(size.height);
    switch (color) {
        case Color::ETC1: return etc1Size(size);
        case Color::RGB565: return pixels * 2;
        case Color::RGBA8888: return pixels * 4;
        default: break;
    }
    return std::unexpected(Error{ErrorCode::UnsupportedFormat, "unsupported gpu payload colour"});
}

Result<Profile> profileFor(Color color, Size size) {
    HAIO_TRY(bytes, payloadSize(color, size));
    switch (color) {
        case Color::ETC1:
            return Profile{0, 0, GL_ETC1_RGB8_OES, GL_RGB, VK_FORMAT_ETC2_R8G8B8_UNORM_BLOCK, 6, bytes};
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
    if (glType == 0 && glFormat == 0 && glInternal == GL_ETC1_RGB8_OES) return Color::ETC1;
    if (glType == GL_UNSIGNED_SHORT_5_6_5 && glFormat == GL_RGB && glInternal == GL_RGB565) return Color::RGB565;
    if (glType == GL_UNSIGNED_BYTE && glFormat == GL_RGBA && glInternal == GL_RGBA8) return Color::RGBA8888;
    return std::unexpected(Error{ErrorCode::UnsupportedFormat, "unsupported ktx payload colour"});
}

Result<Color> colorFromVK(uint32_t format) {
    if (format == VK_FORMAT_ETC2_R8G8B8_UNORM_BLOCK) return Color::ETC1;
    if (format == VK_FORMAT_R5G6B5_UNORM_PACK16) return Color::RGB565;
    if (format == VK_FORMAT_R8G8B8A8_UNORM) return Color::RGBA8888;
    return std::unexpected(Error{ErrorCode::UnsupportedFormat, "unsupported ktx2 payload colour"});
}

Result<Color> colorFromPVR(uint64_t format) {
    if (format == 6) return Color::ETC1;
    if (format == pvrRaw('r', 'g', 'b', 0, 5, 6, 5, 0)) return Color::RGB565;
    if (format == pvrRaw('r', 'g', 'b', 'a', 8, 8, 8, 8)) return Color::RGBA8888;
    return std::unexpected(Error{ErrorCode::UnsupportedFormat, "unsupported pvr payload colour"});
}

}
