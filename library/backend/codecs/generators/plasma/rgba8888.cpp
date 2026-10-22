#include <haio_codec.hpp>
#include <haio/codecs/generators/plasma.hpp>

#include <algorithm>
#include <random>

namespace {

using namespace Haio;
namespace Canvas = Haio::Codecs::Canvas;

/**
 * midpoint displacement over a rectangle: each edge's middle is the average of its
 * ends and each rectangle's the average of its corners, all nudged by noise that
 * halves at every level down. it is the same idea as imagemagick's plasma, drawn
 * with haio's own noise, so the pictures are alike in kind and not pixel for pixel.
 */
class Plasma {
public:
    Plasma(Image<Color::RGBA8888>& image, uint32_t seed)
        : image_(image), set_(static_cast<size_t>(image.width) * static_cast<size_t>(image.height)), random_(seed) {}

    void corner(int x, int y, uint32_t argb, double amplitude) { place(x, y, jitter(argb, amplitude)); }

    void fill(int x0, int y0, int x1, int y1, double amplitude) {
        const bool wide = x1 - x0 >= 2;
        const bool tall = y1 - y0 >= 2;
        if (!wide && !tall) return;

        const int xm = (x0 + x1) / 2;
        const int ym = (y0 + y1) / 2;
        if (wide) {
            between(xm, y0, x0, y0, x1, y0, amplitude);
            between(xm, y1, x0, y1, x1, y1, amplitude);
        }
        if (tall) {
            between(x0, ym, x0, y0, x0, y1, amplitude);
            between(x1, ym, x1, y0, x1, y1, amplitude);
        }
        if (wide && tall && !isSet(xm, ym)) {
            const auto top = average(at(x0, y0), at(x1, y0));
            const auto bottom = average(at(x0, y1), at(x1, y1));
            place(xm, ym, jitter(average(top, bottom), amplitude));
        }

        const double finer = amplitude / 2;
        const int xs[] = {x0, wide ? xm : x1, x1};
        const int ys[] = {y0, tall ? ym : y1, y1};
        for (int j = 0; j < (tall ? 2 : 1); j++) {
            for (int i = 0; i < (wide ? 2 : 1); i++) fill(xs[i], ys[j], xs[i + 1], ys[j + 1], finer);
        }
    }

    uint32_t randomColour() {
        std::uniform_int_distribution<uint32_t> any(0, 0xFFFFFF);
        return 0xFF000000u | any(random_);
    }

private:
    Image<Color::RGBA8888>& image_;
    std::vector<bool> set_;
    std::mt19937 random_;

    size_t indexOf(int x, int y) const { return static_cast<size_t>(y) * static_cast<size_t>(image_.width) + static_cast<size_t>(x); }
    bool isSet(int x, int y) const { return set_[indexOf(x, y)]; }

    uint32_t at(int x, int y) const {
        const auto* p = image_.data.data() + indexOf(x, y) * 4;
        return 0xFF000000u | (uint32_t{p[0]} << 16) | (uint32_t{p[1]} << 8) | p[2];
    }

    void place(int x, int y, uint32_t argb) {
        Canvas::put(image_, x, y, argb);
        set_[indexOf(x, y)] = true;
    }

    void between(int x, int y, int ax, int ay, int bx, int by, double amplitude) {
        if (!isSet(x, y)) place(x, y, jitter(average(at(ax, ay), at(bx, by)), amplitude));
    }

    static uint32_t average(uint32_t a, uint32_t b) { return Canvas::mix(a, b, 0.5); }

    /** every colour channel moved by up to half the amplitude either way; alpha stays whole */
    uint32_t jitter(uint32_t argb, double amplitude) {
        std::uniform_real_distribution<double> noise(-amplitude / 2, amplitude / 2);
        uint32_t out = 0xFF000000u;
        for (int shift = 0; shift < 24; shift += 8) {
            const double channel = ((argb >> shift) & 0xFF) + noise(random_);
            out |= static_cast<uint32_t>(std::clamp(channel, 0.0, 255.0)) << shift;
        }
        return out;
    }
};

}

namespace Haio::Codecs {

/**
 * plasma:red-blue starts from that gradient and breaks it up; plasma: alone is
 * plasma:white-black, as it is for imagemagick, and plasma:fractal starts from four
 * random corners instead. fractal: is plasma: under another name.
 */
template <>
Result<Image<Color::RGBA8888>> Generate<Brush::Plasma, Color::RGBA8888>(std::string_view words, const Settings& settings) {
    HAIO_TRY(size, Canvas::sizeOf(settings));

    uint32_t seed = std::random_device{}();
    if (settingNamed(settings, plasmaSeed.name())) {
        HAIO_TRY(chosen, settingInt(settings, plasmaSeed));
        seed = static_cast<uint32_t>(chosen);
    }

    auto image = Canvas::blank(size);
    Plasma plasma(image, seed);
    const int right = size.width - 1;
    const int bottom = size.height - 1;
    constexpr double amplitude = 255;

    if (words == "fractal") {
        for (const auto [x, y] : {std::pair{0, 0}, std::pair{right, 0}, std::pair{0, bottom}, std::pair{right, bottom}}) {
            plasma.corner(x, y, plasma.randomColour(), amplitude);
        }
    } else {
        HAIO_TRY(colours, Canvas::pairOf(words, 0xFFFFFFFFu, 0xFF000000u));
        plasma.corner(0, 0, colours.first, amplitude);
        plasma.corner(right, 0, colours.first, amplitude);
        plasma.corner(0, bottom, colours.second, amplitude);
        plasma.corner(right, bottom, colours.second, amplitude);
    }
    plasma.fill(0, 0, right, bottom, amplitude / 2);
    return image;
}

}
