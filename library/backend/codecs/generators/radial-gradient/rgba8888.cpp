#include <haio_codec.hpp>
#include <haio/codecs/generators/radial-gradient.hpp>

#include <algorithm>
#include <cmath>
#include <tuple>

namespace Haio::Codecs {

/**
 * a radial gradient: the first colour in the middle and the second at the edge, the
 * edge being wherever gradient:extent puts it -- by default the larger half of the
 * picture, measured between pixel centres as imagemagick measures it.
 */
template <>
Result<Image<Color::RGBA8888>> Generate<Brush::RadialGradient, Color::RGBA8888>(std::string_view words, const Settings& settings) {
    HAIO_TRY(size, Canvas::sizeOf(settings));
    HAIO_TRY(colours, Canvas::pairOf(words, 0xFFFFFFFFu, 0xFF000000u));

    const double halfWidth = (size.width - 1) / 2.0;
    const double halfHeight = (size.height - 1) / 2.0;

    double cx = halfWidth;
    double cy = halfHeight;
    if (const auto* center = settingNamed(settings, gradientCenter.name())) {
        const auto point = Canvas::pointOf(center->value);
        if (!point) return std::unexpected(Canvas::invalidDefine(*center));
        std::tie(cx, cy) = *point;
    }

    auto extent = GradientExtent::Circle;
    if (const auto* named = settingNamed(settings, gradientExtent.name())) extent = *Stages::enumNamed<GradientExtent>(named->value);

    double rx = 0;
    double ry = 0;
    switch (extent) {
        case GradientExtent::Circle:
        case GradientExtent::Maximum: rx = ry = std::max(halfWidth, halfHeight); break;
        case GradientExtent::Minimum: rx = ry = std::min(halfWidth, halfHeight); break;
        case GradientExtent::Diagonal: rx = ry = std::hypot(halfWidth, halfHeight); break;
        case GradientExtent::Ellipse: rx = halfWidth; ry = halfHeight; break;
    }
    if (const auto* radii = settingNamed(settings, gradientRadii.name())) {
        const auto point = Canvas::pointOf(radii->value);
        if (!point || point->first < 0 || point->second < 0) return std::unexpected(Canvas::invalidDefine(*radii));
        std::tie(rx, ry) = *point;
    }

    auto image = Canvas::blank(size);
    for (int y = 0; y < size.height; y++) {
        for (int x = 0; x < size.width; x++) {
            // a radius of nothing is a point: the middle is the first colour and the
            // rest is past the edge
            const double ex = rx > 0 ? (x - cx) / rx : (x == cx ? 0 : 1);
            const double ey = ry > 0 ? (y - cy) / ry : (y == cy ? 0 : 1);
            const double share = std::min(1.0, std::hypot(ex, ey));
            Canvas::put(image, x, y, Canvas::mix(colours.first, colours.second, share));
        }
    }
    return image;
}

}
