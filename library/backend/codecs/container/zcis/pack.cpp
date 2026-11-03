#include <haio_codec.hpp>
#include <haio/generated/codec.hpp>
#include <haio/internal/codecs/zcis.hpp>
#include <haio_atlas.hpp>

#include <algorithm>
#include <vector>

namespace {

void blit(Haio::Image<Haio::Color::RGBA8888>& canvas, const Haio::Image<Haio::Color::RGBA8888>& picture,
          const Haio::Rect& at) {
    const auto row = static_cast<size_t>(picture.width) * 4;
    for (int y = 0; y < picture.height; y++) {
        const auto from = static_cast<size_t>(y) * row;
        const auto to = (static_cast<size_t>(at.y + y) * static_cast<size_t>(canvas.width)
                         + static_cast<size_t>(at.x)) * 4;
        std::copy_n(picture.data.data() + from, row, canvas.data.data() + to);
    }
}

}

namespace Haio::Codecs {

/**
 * an atlas and the index that cuts it up: the pictures are laid out on one canvas,
 * which is then written as any single zcis canvas is, and the header says where each
 * of them starts and ends.
 *
 * what is between them stays transparent, so a cut never picks up its neighbour.
 */
template <>
Result<Blob> Pack<Format::ZCIS, Color::RGBA8888>(std::vector<Image<Color::RGBA8888>> images) {
    if (images.empty()) HAIO_FAIL(InvalidInput, "nothing to pack");

    std::vector<Size> sizes;
    sizes.reserve(images.size());
    for (const auto& image : images) {
        const auto expected = static_cast<size_t>(image.width) * static_cast<size_t>(image.height) * 4;
        if (image.width <= 0 || image.height <= 0 || image.data.size() != expected) {
            HAIO_FAIL(InvalidInput, "invalid rgba8888 image for zcis pack");
        }
        sizes.push_back(Size{image.width, image.height});
    }

    HAIO_TRY(layout, Atlas::shelf(sizes));

    Image<Color::RGBA8888> canvas{
        layout.canvas.width,
        layout.canvas.height,
        std::vector<uint8_t>(static_cast<size_t>(layout.canvas.width) * static_cast<size_t>(layout.canvas.height) * 4),
    };
    for (size_t at = 0; at < images.size(); at++) {
        blit(canvas, images[at], layout.rects[at]);
        images[at].data = std::vector<uint8_t>{};
    }

    return Zcis::writeCanvas(canvas, layout.rects);
}

}
