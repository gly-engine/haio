#include <haio_palette.hpp>
#include <haio_util.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <string>

namespace {

struct Rgb {
    int r = 0;
    int g = 0;
    int b = 0;
};

Rgb unpack(uint32_t colour) {
    return Rgb{static_cast<int>((colour >> 16) & 0xFF),
               static_cast<int>((colour >> 8) & 0xFF),
               static_cast<int>(colour & 0xFF)};
}

/**
 * squared distance, weighted the way the eye weighs the channels.
 *
 * plain rgb distance calls a dark blue and a dark green neighbours, because it counts
 * every channel the same while the eye counts green about six times more than blue.
 */
int distance(const Rgb& a, const Rgb& b) {
    const auto dr = a.r - b.r;
    const auto dg = a.g - b.g;
    const auto db = a.b - b.b;
    return 2 * dr * dr + 4 * dg * dg + 3 * db * db;
}

size_t nearest(const std::vector<Rgb>& palette, const Rgb& wanted) {
    size_t best = 0;
    int closest = distance(palette[0], wanted);

    for (size_t at = 1; at < palette.size(); at++) {
        if (const auto here = distance(palette[at], wanted); here < closest) {
            closest = here;
            best = at;
        }
    }
    return best;
}

int clampByte(int value) { return value < 0 ? 0 : value > 255 ? 255 : value; }

}

namespace Haio {

std::optional<Dither> ditherNamed(std::string_view name) {
    if (name == "nearest") return Dither::Nearest;
    if (name == "bayer") return Dither::Bayer;
    if (name == "floyd") return Dither::Floyd;
    if (name == "error") return Dither::Strict;
    return std::nullopt;
}

std::optional<Limit> limitNamed(std::string_view name) {
    if (name == "sort") return Limit::Sort;
    if (name == "spread") return Limit::Spread;
    return std::nullopt;
}

Result<std::vector<uint32_t>> limitPalette(const Image<Color::RGBA8888>& image,
                                           std::vector<uint32_t> entries, size_t most, Limit how) {
    if (most == 0) {
        return std::unexpected(Error{ErrorCode::InvalidInput, "a limit of zero leaves no colours to draw with"});
    }
    if (entries.size() <= most) return entries;

    std::vector<Rgb> palette;
    palette.reserve(entries.size());
    for (const auto colour : entries) palette.push_back(unpack(colour));

    // how many pixels would land on each colour if the whole palette were available
    std::vector<size_t> votes(entries.size(), 0);
    for (size_t at = 0; at + 3 < image.data.size(); at += 4) {
        votes[nearest(palette, Rgb{image.data[at + 0], image.data[at + 1], image.data[at + 2]})]++;
    }

    std::vector<size_t> order;
    order.reserve(most);

    if (how == Limit::Sort) {
        std::vector<size_t> all(entries.size());
        for (size_t at = 0; at < all.size(); at++) all[at] = at;

        // most used first, and the original order breaks ties so the result is the
        // same every run rather than whatever the sort happened to do
        std::ranges::stable_sort(all, [&](size_t a, size_t b) { return votes[a] > votes[b]; });
        order.assign(all.begin(), all.begin() + most);
    } else {
        std::vector<bool> taken(entries.size(), false);

        // the first is simply the most used: there is nothing yet to be far from
        size_t first = 0;
        for (size_t at = 1; at < votes.size(); at++) {
            if (votes[at] > votes[first]) first = at;
        }
        order.push_back(first);
        taken[first] = true;

        while (order.size() < most) {
            size_t best = entries.size();
            double bestScore = -1;

            for (size_t at = 0; at < entries.size(); at++) {
                if (taken[at]) continue;

                // how far this colour is from the nearest one already kept
                int apart = std::numeric_limits<int>::max();
                for (const auto chosen : order) {
                    apart = std::min(apart, distance(palette[at], palette[chosen]));
                }

                // count times distance: rare and distinct beats common and duplicate
                const auto score = static_cast<double>(votes[at]) * static_cast<double>(apart);
                if (score > bestScore) {
                    bestScore = score;
                    best = at;
                }
            }

            // every remaining colour is unused and identical to something kept
            if (best == entries.size() || bestScore <= 0) break;
            order.push_back(best);
            taken[best] = true;
        }
    }

    // back into palette order, so a picture keeps the sequence its colours came in
    std::ranges::sort(order);

    std::vector<uint32_t> kept;
    kept.reserve(most);
    for (const auto at : order) kept.push_back(entries[at]);
    return kept;
}

Result<Image<Color::PALETTE>> toPalette(const Image<Color::RGBA8888>& image,
                                        std::vector<uint32_t> entries, Dither how) {
    if (entries.empty()) {
        return std::unexpected(Error{ErrorCode::InvalidInput, "a palette with no colours cannot hold a picture"});
    }
    if (entries.size() > 256) {
        return std::unexpected(Error{ErrorCode::InvalidInput, "a palette holds at most 256 colours"});
    }

    const auto width = static_cast<size_t>(image.width);
    const auto height = static_cast<size_t>(image.height);
    if (image.data.size() != width * height * 4) {
        return std::unexpected(Error{ErrorCode::InvalidInput, "invalid rgba8888 image for palette conversion"});
    }

    std::vector<Rgb> palette;
    palette.reserve(entries.size());
    for (const auto colour : entries) palette.push_back(unpack(colour));

    std::vector<uint8_t> indices(width * height);

    // floyd carries the error forward, so it needs somewhere to put it; the others
    // decide each pixel on its own and never look at this
    std::vector<Rgb> carried(how == Dither::Floyd ? width * height : 0);

    // how far a threshold may push a channel: the average gap between neighbouring
    // colours, so a dense palette dithers gently and a sparse one hard
    const int spread = 256 / static_cast<int>(std::max<size_t>(entries.size(), 2));

    // a sparse palette needs a coarse matrix and a dense one a fine matrix: with four
    // colours a 2x2 is already enough steps, while 256 wants all of an 8x8
    const int matrix = Util::Dither::bayerSideFor(static_cast<int>(entries.size()));

    for (size_t y = 0; y < height; y++) {
        for (size_t x = 0; x < width; x++) {
            const auto at = (y * width + x) * 4;
            Rgb wanted{image.data[at + 0], image.data[at + 1], image.data[at + 2]};

            if (how == Dither::Bayer) {
                // centred on zero, so the matrix nudges a colour both ways rather
                // than only ever making it brighter
                const auto half = matrix * matrix / 2;
                const auto threshold = Util::Dither::bayer(matrix, static_cast<int>(x % matrix),
                                                           static_cast<int>(y % matrix)) - half;
                wanted.r = clampByte(wanted.r + threshold * spread / half);
                wanted.g = clampByte(wanted.g + threshold * spread / half);
                wanted.b = clampByte(wanted.b + threshold * spread / half);
            } else if (how == Dither::Floyd) {
                const auto& error = carried[y * width + x];
                wanted.r = clampByte(wanted.r + error.r);
                wanted.g = clampByte(wanted.g + error.g);
                wanted.b = clampByte(wanted.b + error.b);
            }

            const auto chosen = nearest(palette, wanted);

            if (how == Dither::Strict && distance(palette[chosen], wanted) != 0) {
                return std::unexpected(Error{ErrorCode::InvalidInput,
                                             "the colour at " + std::to_string(x) + "," + std::to_string(y)
                                                 + " is not in the palette, and -filter error means every one must be"});
            }
            indices[y * width + x] = static_cast<uint8_t>(chosen);

            if (how != Dither::Floyd) continue;

            // the part that did not fit goes to the neighbours, in the proportions
            // floyd and steinberg landed on: 7 right, 3 below left, 5 below, 1 after
            const auto& got = palette[chosen];
            const Rgb left{wanted.r - got.r, wanted.g - got.g, wanted.b - got.b};

            const auto spill = [&](size_t nx, size_t ny, int part) {
                if (nx >= width || ny >= height) return;
                auto& into = carried[ny * width + nx];
                into.r += left.r * part / 16;
                into.g += left.g * part / 16;
                into.b += left.b * part / 16;
            };
            spill(x + 1, y, 7);
            if (x > 0) spill(x - 1, y + 1, 3);
            spill(x, y + 1, 5);
            spill(x + 1, y + 1, 1);
        }
    }

    return Image<Color::PALETTE>{image.width, image.height, std::move(indices), std::move(entries)};
}

}
