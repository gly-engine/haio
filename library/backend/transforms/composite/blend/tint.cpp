#include <haio/transforms/composite.hpp>

namespace Haio::Transforms {

/**
 * where src does not reach, it is opaque black, which tints nothing.
 *
 * @startuml{math}
 * {: ("res"_"rgb" = 1 - (1 - "dst"_"rgb")(1 - "src"_"rgb")), ("res"_"a" = "dst"_"a" "src"_"a") :}
 * @enduml
 *
 * <center>
 * @haio{convert} assets/jucagato256x256.png assets/disk256x256color.png -compose tint -composite png:-
 * @haio{convert} assets/jucagato256x256.png assets/disk256x256gray.png -compose tint -composite png:-
 * </center>
 */
template <>
BlendRes Blend<Compose::Tint>(BlendDst<> dst, BlendSrc src, int x, int y) {
    for (int by = 0; by < dst.height; by++) {
        for (int bx = 0; bx < dst.width; bx++) {
            auto* shape = dst.data.data() + (static_cast<size_t>(by) * static_cast<size_t>(dst.width) + static_cast<size_t>(bx)) * 4;
            const auto* colour = colourAt(src, bx - x, by - y);
            for (int c = 0; c < 3; c++) shape[c] = screenOf(shape[c], colour[c]);
            shape[3] = timesOf(shape[3], colour[3]);
        }
    }
    return dst;
}

/**
 * the same as rgba8888, the grey standing for all three channels.
 *
 * @startuml{math}
 * {: ("res"_"rgb" = 1 - (1 - "dst"_"g")(1 - "src"_"rgb")), ("res"_"a" = "dst"_"a" "src"_"a") :}
 * @enduml
 */
template <>
BlendRes Blend<Compose::Tint, Color::GRAYALPHA88>(BlendDst<Color::GRAYALPHA88> dst, BlendSrc src, int x, int y) {
    Image<Color::RGBA8888> out{dst.width, dst.height,
                               std::vector<uint8_t>(static_cast<size_t>(dst.width) * static_cast<size_t>(dst.height) * 4)};
    for (int by = 0; by < dst.height; by++) {
        for (int bx = 0; bx < dst.width; bx++) {
            const auto at = static_cast<size_t>(by) * static_cast<size_t>(dst.width) + static_cast<size_t>(bx);
            const int grey = dst.data[at * 2];
            const int alpha = dst.data[at * 2 + 1];
            const auto* colour = colourAt(src, bx - x, by - y);
            auto* pixel = out.data.data() + at * 4;
            for (int c = 0; c < 3; c++) pixel[c] = screenOf(grey, colour[c]);
            pixel[3] = timesOf(alpha, colour[3]);
        }
    }
    return out;
}

/**
 * a grey alone is how much of src shows, dark being all of it; only the alpha
 * is cut, so a half covered edge is the colour at half rather than a darker one.
 *
 * @startuml{math}
 * {: ("res"_"rgb" = "src"_"rgb"), ("res"_"a" = "src"_"a" (1 - "dst"_"g")) :}
 * @enduml
 */
template <>
BlendRes Blend<Compose::Tint, Color::GRAY8>(BlendDst<Color::GRAY8> dst, BlendSrc src, int x, int y) {
    Image<Color::RGBA8888> out{dst.width, dst.height,
                               std::vector<uint8_t>(static_cast<size_t>(dst.width) * static_cast<size_t>(dst.height) * 4)};
    for (int by = 0; by < dst.height; by++) {
        for (int bx = 0; bx < dst.width; bx++) {
            const auto at = static_cast<size_t>(by) * static_cast<size_t>(dst.width) + static_cast<size_t>(bx);
            const auto* colour = colourAt(src, bx - x, by - y);
            auto* pixel = out.data.data() + at * 4;
            pixel[3] = timesOf(colour[3], 255 - dst.data[at]);
            if (pixel[3] != 0) std::copy_n(colour, 3, pixel);
        }
    }
    return out;
}

}
