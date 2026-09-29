#include <haio/transforms/composite.hpp>

namespace Haio::Transforms {

/**
 * grayalpha88 with the shape's colour counting as its rec.709 grey, so a grey shape
 * tints the same whether or not a transform widened it to rgba8888.
 *
 * @startuml{math}
 * {: ("dst"_"y" = 0.2127 "dst"_"r" + 0.7152 "dst"_"g" + 0.0722 "dst"_"b"), ("res"_"rgb" = {("src"_"rgb", "res"_"a" > 0), (0, "res"_"a" = 0):}), ("res"_"a" = "src"_"a" "dst"_"a" (1 - "dst"_"y")) :}
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
            const double grey = 0.212656 * shape[0] + 0.715158 * shape[1] + 0.072186 * shape[2];
            shape[3] = timesOf(timesOf(colour[3], shape[3]), 255 - static_cast<int>(grey + 0.5));
            if (shape[3] != 0) std::copy_n(colour, 3, shape);
            else std::fill_n(shape, 3, 0);
        }
    }
    return dst;
}

/**
 * gray8 with an alpha: the shape's own alpha cuts src too.
 *
 * @startuml{math}
 * {: ("res"_"rgb" = {("src"_"rgb", "res"_"a" > 0), (0, "res"_"a" = 0):}), ("res"_"a" = "src"_"a" "dst"_"a" (1 - "dst"_"g")) :}
 * @enduml
 *
 * <center>
 * @haio{convert} assets/jucagato256x256.png -pix_fmt grayalpha88 assets/disk256x256color.png -compose tint -composite png:-
 * @haio{convert} assets/jucagato256x256.png -pix_fmt grayalpha88 assets/disk256x256gray.png -compose tint -composite png:-
 * </center>
 */
template <>
BlendRes Blend<Compose::Tint, Color::GRAYALPHA88>(BlendDst<Color::GRAYALPHA88> dst, BlendSrc src, int x, int y) {
    Image<Color::RGBA8888> out{dst.width, dst.height,
                               std::vector<uint8_t>(static_cast<size_t>(dst.width) * static_cast<size_t>(dst.height) * 4)};
    for (int by = 0; by < dst.height; by++) {
        for (int bx = 0; bx < dst.width; bx++) {
            const auto at = static_cast<size_t>(by) * static_cast<size_t>(dst.width) + static_cast<size_t>(bx);
            const auto* colour = colourAt(src, bx - x, by - y);
            auto* pixel = out.data.data() + at * 4;
            pixel[3] = timesOf(timesOf(colour[3], dst.data[at * 2 + 1]), 255 - dst.data[at * 2]);
            if (pixel[3] != 0) std::copy_n(colour, 3, pixel);
        }
    }
    return out;
}

/**
 * the shape's grey is how much of src shows, black being all of it and white none.
 * only the alpha is cut, so a half covered edge is src at half rather than darker.
 *
 * @startuml{math}
 * {: ("res"_"rgb" = {("src"_"rgb", "res"_"a" > 0), (0, "res"_"a" = 0):}), ("res"_"a" = "src"_"a" (1 - "dst"_"g")) :}
 * @enduml
 *
 * <center>
 * @haio{convert} assets/jucagato256x256.png -pix_fmt gray8 assets/disk256x256color.png -compose tint -composite png:-
 * @haio{convert} assets/jucagato256x256.png -pix_fmt gray8 assets/disk256x256gray.png -compose tint -composite png:-
 * </center>
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
