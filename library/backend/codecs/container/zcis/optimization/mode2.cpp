#include <haio/internal/codecs/zcis.hpp>

#include <haio/generated/codec.hpp>
#include <haio_codec.hpp>

#include <algorithm>
#include <optional>
#include <vector>

namespace Haio::Codecs::Zcis {

namespace {

/**
 * a bitmap of what is to be painted, written out and measured.
 *
 * it is weighed against what it actually costs rather than against a guess at it: a
 * mask over a thing with a clean outline packs to almost nothing and a mask over a
 * speckled one packs to almost its whole size, and the difference decides whether it
 * is worth sending at all.
 */
std::optional<Mask> written(const Rect& box, const Rect& at, const std::vector<uint8_t>& spared, size_t runs,
                            size_t worth, bool refine) {
    if (runs == 0 || at.width <= 0 || at.height <= 0) return std::nullopt;

    Image<Color::PALETTE> bits{at.width, at.height, {}};
    bits.entries = {0xFF000000u, 0xFFFFFFFFu};
    bits.data.resize(static_cast<size_t>(at.width) * static_cast<size_t>(at.height));
    for (int y = 0; y < at.height; y++) {
        for (int x = 0; x < at.width; x++) {
            const auto from = static_cast<size_t>(at.y + y - box.y) * static_cast<size_t>(box.width)
                            + static_cast<size_t>(at.x + x - box.x);
            bits.data[static_cast<size_t>(y) * static_cast<size_t>(at.width) + static_cast<size_t>(x)] =
                spared[from] ? 0 : 1;
        }
    }

    auto out = Encode<Format::PCX, Color::PALETTE>(std::move(bits), Settings{});
    if (!out) return std::nullopt;
    if (runs * bytesPerDraw <= worth + out->data.size()) return std::nullopt;
    return Mask{at, std::move(out->data), runs, refine};
}

/**
 * the masks of one box, once nothing moves any more.
 *
 * every run of spared pixels is exactly one draw that stops being made, whatever its
 * length and wherever it sits: a run at the edge of a row ends one call, and a run in
 * the middle of one splits a call in two and so still removes one.
 *
 * two masks come out of this and they are different in kind. the first says which
 * pixels are already the colour of the ground beneath them, and it goes down before
 * anything is painted, so it spares both layers. the second says which rows of the
 * sharp layer the stretched one already got right, and it goes down between them, so
 * it spares the sharp layer alone -- which is a third of all the drawing there is.
 */
void gather(Flat& flat, size_t worth, bool stretched, auto&& keyAt) {
    for (const auto& box : flat.boxes) {
        if (box.width <= 0 || box.height <= 0) continue;
        const auto area = static_cast<size_t>(box.width) * static_cast<size_t>(box.height);
        const auto plane = groundOf(box, flat.fills);

        std::vector<uint8_t> ground(area, 0);
        Rect at{box.x + box.width, box.y + box.height, 0, 0};
        int right = box.x - 1;
        int bottom = box.y - 1;
        size_t runs = 0;

        if (!plane.empty()) {
            for (int y = box.y; y < box.y + box.height; y++) {
                const auto paints = paintsOf(box, y, stretched);
                bool inside = false;
                for (int x = box.x; x < box.x + box.width; x++) {
                    const auto index = static_cast<size_t>(y - box.y) * static_cast<size_t>(box.width)
                                     + static_cast<size_t>(x - box.x);
                    if (keyAt(x, y) != plane[index]) {
                        inside = false;
                        continue;
                    }
                    ground[index] = 1;
                    if (!inside) runs += paints;
                    inside = true;
                    at.x = std::min(at.x, x);
                    at.y = std::min(at.y, y);
                    right = std::max(right, x);
                    bottom = std::max(bottom, y);
                }
            }
        }

        // only the part that has anything to spare is worth a bitmap: outside it
        // every pixel is painted anyway, and a mask says nothing by being absent
        at.width = right - at.x + 1;
        at.height = bottom - at.y + 1;
        if (auto one = written(box, at, ground, runs, worth, false)) flat.masks.push_back(*std::move(one));

        if (!stretched || box.height <= 1) continue;

        /**
         * and now the rows the stretched layer already settled.
         *
         * the base layer holds the odd rows of the box and is laid over both rows of
         * every pair, so the even row is already wearing its neighbour's colour when
         * the sharp layer arrives. where the two were the same to begin with, that is
         * the right colour and the sharp layer has nothing to add.
         *
         * the pixels the ground already spared are left out of the count, since they
         * were not going to be drawn twice anyway.
         */
        std::vector<uint8_t> twice(area, 0);
        Rect pairs{box.x + box.width, box.y + box.height, 0, 0};
        right = box.x - 1;
        bottom = box.y - 1;
        size_t again = 0;

        for (int y = box.y; y + 1 < box.y + box.height; y += 2) {
            bool inside = false;
            for (int x = box.x; x < box.x + box.width; x++) {
                const auto index = static_cast<size_t>(y - box.y) * static_cast<size_t>(box.width)
                                 + static_cast<size_t>(x - box.x);
                if (ground[index] || keyAt(x, y) != keyAt(x, y + 1)) {
                    inside = false;
                    continue;
                }
                twice[index] = 1;
                if (!inside) again++;
                inside = true;
                pairs.x = std::min(pairs.x, x);
                pairs.y = std::min(pairs.y, y);
                right = std::max(right, x);
                bottom = std::max(bottom, y);
            }
        }
        pairs.width = right - pairs.x + 1;
        pairs.height = bottom - pairs.y + 1;
        if (auto one = written(box, pairs, twice, again, worth, true)) flat.masks.push_back(*std::move(one));
    }
}

}

void maskAll(const Image<Color::RGBA8888>& img, Flat& flat, size_t worth, bool stretched) {
    gather(flat, worth, stretched, keysOf(img));
}

void maskAllYuv(const Image<Color::YUV420>& img, Flat& flat, size_t worth) {
    gather(flat, worth, false, keysOf(img));
}

}
