#include <haio/transforms/composite.hpp>

namespace Haio::Transforms {

/**
 * like imagemagick, what src does not reach is cleared too, as a "src"_"a" of 0.
 *
 * @startuml{math}
 * {: ("result"_"rgb" = "dst"_"rgb"), ("result"_"a" = "dst"_"a" "src"_"a") :}
 * @enduml
 */
template <>
Result<Image<Color::RGBA8888>> Blend<Compose::DstIn>(Image<Color::RGBA8888> base, const Image<Color::RGBA8888>& layer, int x, int y) {
    for (int by = 0; by < base.height; by++) {
        for (int bx = 0; bx < base.width; bx++) {
            auto* under = base.data.data() + (static_cast<size_t>(by) * static_cast<size_t>(base.width) + static_cast<size_t>(bx)) * 4;
            const bool inside = bx >= x && bx < x + layer.width && by >= y && by < y + layer.height;
            const int la = inside ? layer.data[(static_cast<size_t>(by - y) * static_cast<size_t>(layer.width) + static_cast<size_t>(bx - x)) * 4 + 3] : 0;
            if (la == 255) continue;
            if (la == 0) {
                std::fill_n(under, 4, 0);
                continue;
            }
            const double alpha = under[3] / 255.0 * (la / 255.0);
            under[3] = static_cast<uint8_t>(255 - static_cast<uint32_t>((1 - alpha) * 65535.0 + 0.5) / 257);
            if (under[3] == 0) std::fill_n(under, 3, 0);
        }
    }
    return base;
}

}
