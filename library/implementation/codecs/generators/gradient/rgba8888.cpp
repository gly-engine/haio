#include <haio/generated/codecs/classes.hpp>
#include <haio/implementation/codecs.hpp>
#include <haio/implementation/codecs/generators/gradient.hpp>

#include <algorithm>
#include <charconv>
#include <cmath>
#include <numbers>
#include <optional>

namespace {

using namespace Haio;
using namespace Haio::Implementation;
using namespace Haio::Implementation::Codecs;

struct Vector {
    double x1 = 0, y1 = 0, x2 = 0, y2 = 0;
};

/**
 * the line a linear gradient runs along, from where the first colour is whole to
 * where the second is, the way imagemagick sets it up:
 *
 * a compass point runs corner to corner or edge to edge -- northeast from the bottom
 * left to the top right -- and an angle runs through the middle, as long as the
 * picture is deep in that direction. nothing at all is south, top to bottom.
 *
 * imagemagick 6 has two slips here that are not copied: -define gradient:direction=
 * south measures itself against the width rather than the height, and the pixel a
 * gradient starts from takes its left neighbour's colour when it sits in the last
 * column. both read as bugs, and a picture with them is the worse picture.
 */
Result<Vector> vectorOf(const Settings& settings, Size size) {
    const double w = size.width - 1;
    const double h = size.height - 1;

    const auto* angle = Util::settingNamed(settings, gradientAngle.name());
    const auto* direction = Util::settingNamed(settings, gradientDirection.name());
    if (angle && direction) {
        return std::unexpected(Error{ErrorCode::InvalidInput,
                                     "gradient:angle and gradient:direction both say which way it runs"});
    }

    if (angle) {
        const auto degrees = Util::getCanvasNumber(angle->value);
        if (!degrees) return std::unexpected(Util::getCanvasInvalidDefine(*angle));
        // whole right angles exactly: a stray 1e-16 across a row is a different byte
        const double turned = std::fmod(std::fmod(*degrees - 90, 360.0) + 360.0, 360.0);
        double sine = std::sin(turned * std::numbers::pi / 180);
        double cosine = std::cos(turned * std::numbers::pi / 180);
        if (std::fmod(turned, 90.0) == 0) {
            sine = std::round(sine);
            cosine = std::round(cosine);
        }
        const double distance = std::abs(w * cosine) + std::abs(h * sine);
        return Vector{(w - distance * cosine) / 2, (h - distance * sine) / 2,
                      (w + distance * cosine) / 2, (h + distance * sine) / 2};
    }

    switch (direction ? *Util::enumNamed<Generate<>::GradientDirection>(direction->value) : Generate<>::GradientDirection::South) {
        case Generate<>::GradientDirection::North: return Vector{0, h, 0, 0};
        case Generate<>::GradientDirection::NorthEast: return Vector{0, h, w, 0};
        case Generate<>::GradientDirection::East: return Vector{0, 0, w, 0};
        case Generate<>::GradientDirection::SouthEast: return Vector{0, 0, w, h};
        case Generate<>::GradientDirection::South: return Vector{0, 0, 0, h};
        case Generate<>::GradientDirection::SouthWest: return Vector{w, 0, 0, h};
        case Generate<>::GradientDirection::West: return Vector{w, 0, 0, 0};
        case Generate<>::GradientDirection::NorthWest: return Vector{w, h, 0, 0};
    }
    return Vector{0, 0, 0, h};
}

}

namespace Haio::Implementation::Codecs {

/**
 * a linear gradient, the way imagemagick draws it: every pixel projected onto the
 * line it runs along, and how far along that line it lands is how far from the
 * first colour to the second it is, held at either end past them.
 */
CodecRes<Color::RGBA8888> Generate<Brush::Gradient, Color::RGBA8888>::operator()(std::string_view words, const Settings& settings) const {
    HAIO_TRY(size, Util::getCanvasSize(settings));
    HAIO_TRY(colours, Util::getCanvasPair(words, 0xFFFFFFFFu, 0xFF000000u));
    HAIO_TRY(line, vectorOf(settings, size));

    /**
     * a line of no length -- one row running south -- is run corner to corner
     * instead, which is what imagemagick does with a 5x1 gradient: rather than one
     * colour throughout, it runs across.
     */
    if (line.x1 == line.x2 && line.y1 == line.y2) line = Vector{0, 0, size.width - 1.0, size.height - 1.0};

    const double dx = line.x2 - line.x1;
    const double dy = line.y2 - line.y1;
    const double length = dx * dx + dy * dy;

    auto image = Util::getCanvasBlank(size);
    for (int y = 0; y < size.height; y++) {
        for (int x = 0; x < size.width; x++) {
            const double along = length > 0 ? ((x - line.x1) * dx + (y - line.y1) * dy) / length : 0;
            Util::setCanvasPixel(image, x, y, Util::getCanvasMix(colours.first, colours.second, std::clamp(along, 0.0, 1.0)));
        }
    }
    return image;
}

}
