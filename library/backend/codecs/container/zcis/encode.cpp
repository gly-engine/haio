#include <haio/internal/codecs/yuv.hpp>
#include <haio/codecs/pcx.hpp>
#include <haio/codecs/zcis.hpp>
#include <haio/internal/codecs/zcis.hpp>
#include <haio_codec.hpp>
#include <haio/generated/codec.hpp>
#include <haio_convert.hpp>
#include <haio_util.hpp>

#include <algorithm>
#include <array>
#include <cstdio>
#include <iterator>
#include <optional>
#include <string>
#include <string_view>

namespace {

constexpr std::string_view base62 = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";
constexpr int base62PairMax = 62 * 62 - 1;

/**
 * zcis splits a picture into two layers of alternating rows, so that each layer
 * stays a decodable image on its own and both together rebuild the original
 * without losing a pixel. a command in capitals is a commit, which shows what was
 * painted; the same letter in lower case is a blit, which paints and waits.
 */
constexpr char baseCommand = 'A';
constexpr char refineCommand = 'C';
constexpr char fillCommand = 'F';
constexpr char maskCommand = 'E';

/**
 * what a fill has to save to be worth sending: its own member is sixty bytes of ar
 * header plus the colour and the padding, so anything under that makes the file
 * bigger to save nothing.
 */
constexpr size_t fillWorth = 80;

char commandOf(char commit, bool shown) {
    return shown ? commit : static_cast<char>(commit - 'A' + 'a');
}

constexpr std::string_view indexMemberName = "0000000000.txt";

std::string base62Pair(int value) {
    return {base62[static_cast<size_t>(value) / 62], base62[static_cast<size_t>(value) % 62]};
}

/** ar members sit on even offsets, padded with a newline. */
std::optional<Haio::Error> appendMember(std::vector<uint8_t>& out, const std::string& name, const std::vector<uint8_t>& body) {
    std::array<char, 61> header{};
    const int written = std::snprintf(
        header.data(),
        header.size(),
        "%-16s%-12u%-6u%-6u%-8o%-10zu`\n",
        name.c_str(),
        0u,
        0u,
        0u,
        0644u,
        body.size()
    );
    if (written != 60) return Haio::Error{Haio::ErrorCode::InvalidInput, "zcis member header overflow: " + name};

    out.insert(out.end(), header.begin(), header.begin() + 60);
    out.insert(out.end(), body.begin(), body.end());
    if (body.size() % 2 != 0) out.push_back('\n');
    return std::nullopt;
}

void appendBytes(std::vector<uint8_t>& out, std::string_view text) {
    out.insert(out.end(), text.begin(), text.end());
}

Haio::Image<Haio::Color::RGBA8888> selectRows(const Haio::Image<Haio::Color::RGBA8888>& image, int parity) {
    const int height = (image.height - parity + 1) / 2;
    const auto stride = static_cast<size_t>(image.width) * 4;

    std::vector<uint8_t> data(stride * static_cast<size_t>(height));
    for (int y = 0; y < height; y++) {
        const auto src = static_cast<size_t>(2 * y + parity) * stride;
        std::copy_n(image.data.data() + src, stride, data.data() + static_cast<size_t>(y) * stride);
    }
    return Haio::Image<Haio::Color::RGBA8888>{image.width, height, std::move(data)};
}

std::optional<Haio::Error> checkCanvas(int width, int height) {
    if (width > base62PairMax || height > base62PairMax) {
        return Haio::Error{Haio::ErrorCode::InvalidInput, "zcis dimensions exceed base62 limit"};
    }
    return std::nullopt;
}

/**
 * the index: a line for every picture laid on the canvas, saying where to cut it back
 * out, or a single one covering the canvas when nothing was laid on it. the names the
 * cuts carry stay behind, since a line has no room for them.
 */
std::optional<Haio::Error> openArchive(std::vector<uint8_t>& out, const Haio::Metadata& meta, int width, int height) {
    appendBytes(out, "!<arch>\n");

    const auto line = [](const Haio::Rect& rect) {
        return std::to_string(rect.x) + " " + std::to_string(rect.y) + " " + std::to_string(rect.width) + " "
             + std::to_string(rect.height) + "\r\n";
    };
    std::string index;
    for (const auto& cut : meta.cuts) index += line(cut.rect);
    if (meta.cuts.empty()) index = line(Haio::Rect{0, 0, width, height});
    return appendMember(out, std::string(indexMemberName), {index.begin(), index.end()});
}

/** the cuts as the index wrote them down, which is what the flattening walks */
std::vector<Haio::Rect> cutsOf(const Haio::Metadata& meta, int width, int height) {
    std::vector<Haio::Rect> rects;
    rects.reserve(meta.cuts.size());
    for (const auto& cut : meta.cuts) rects.push_back(cut.rect);
    if (rects.empty()) rects.push_back(Haio::Rect{0, 0, width, height});
    return rects;
}

/** the box a cut shrank to, by the rectangle it was */
const Haio::Rect* boxFor(const Haio::Codecs::Zcis::Flat& flat, const std::vector<Haio::Rect>& cuts,
                         const Haio::Rect& cut) {
    for (size_t at = 0; at < cuts.size() && at < flat.boxes.size(); at++) {
        const auto& one = cuts[at];
        if (one.x == cut.x && one.y == cut.y && one.width == cut.width && one.height == cut.height) {
            return &flat.boxes[at];
        }
    }
    return nullptr;
}

std::string target(int x, int y, int width, int height) {
    return base62Pair(x) + base62Pair(y) + base62Pair(width) + base62Pair(height);
}

/** the ORDER character, which only sorts the members and so may wrap around */
char orderOf(int order) {
    return base62[static_cast<size_t>(order) % base62.size()];
}

/**
 * the mask as the standard wants it: a one bit pcx where a set bit is a pixel to
 * paint and a clear one is a pixel to leave as the ground painted it.
 *
 * it is always a blit. a mask shows nothing by itself, and a commit on it would put
 * a half drawn page on the screen.
 */
std::optional<Haio::Error> appendMask(std::vector<uint8_t>& out, int& order, const Haio::Codecs::Zcis::Mask& mask) {
    const auto name = std::string(1, orderOf(order++)) + commandOf(maskCommand, false)
                    + target(mask.at.x, mask.at.y, mask.at.width, mask.at.height) + ".pcx";
    return appendMember(out, name, mask.pcx);
}

/**
 * the cuts in rows, top to bottom, and every row left to right: cuts of different
 * heights that share a line are one row. a canvas nothing was laid on is one cut.
 */
std::vector<std::vector<Haio::Rect>> rowsOf(const Haio::Metadata& meta, int width, int height) {
    std::vector<Haio::Rect> rects;
    rects.reserve(meta.cuts.size());
    for (const auto& cut : meta.cuts) rects.push_back(cut.rect);
    if (rects.empty()) rects.push_back(Haio::Rect{0, 0, width, height});
    std::ranges::stable_sort(rects, {}, &Haio::Rect::y);

    std::vector<std::vector<Haio::Rect>> rows;
    int bottom = 0;
    for (const auto& rect : rects) {
        if (rows.empty() || rect.y >= bottom) rows.emplace_back();
        rows.back().push_back(rect);
        bottom = std::max(bottom, rect.y + rect.height);
    }
    for (auto& row : rows) std::ranges::stable_sort(row, {}, &Haio::Rect::x);
    return rows;
}

Haio::Image<Haio::Color::RGBA8888> piece(const Haio::Image<Haio::Color::RGBA8888>& image, const Haio::Rect& rect) {
    const auto stride = static_cast<size_t>(image.width) * 4;
    const auto row = static_cast<size_t>(rect.width) * 4;
    std::vector<uint8_t> data(row * static_cast<size_t>(rect.height));
    for (int y = 0; y < rect.height; y++) {
        const auto from = static_cast<size_t>(rect.y + y) * stride + static_cast<size_t>(rect.x) * 4;
        std::copy_n(image.data.data() + from, row, data.data() + static_cast<size_t>(y) * row);
    }
    return Haio::Image<Haio::Color::RGBA8888>{rect.width, rect.height, std::move(data)};
}

/**
 * a cut grown outwards to even edges, which is the least a 4:2:0 frame can hold.
 *
 * it clamps to the planes rather than to the picture, so a cut that reaches the right
 * edge of an odd sided canvas grows into the spare column instead of staying odd and
 * losing its last chroma sample. the index keeps the real rectangle either way.
 */
Haio::Rect even(const Haio::Rect& rect, const Haio::Codecs::Yuv420::Planes& planes) {
    const auto left = rect.x / 2 * 2;
    const auto top = rect.y / 2 * 2;
    const auto right = std::min(planes.lumaWidth, (rect.x + rect.width + 1) / 2 * 2);
    const auto bottom = std::min(planes.lumaHeight, (rect.y + rect.height + 1) / 2 * 2);
    return Haio::Rect{left, top, right - left, bottom - top};
}

/** the same rectangle of each plane of a yuv420 frame, whose edges are all even */
std::vector<uint8_t> piece(const Haio::Image<Haio::Color::YUV420>& image, const Haio::Rect& rect,
                           const Haio::Codecs::Yuv420::Planes& planes) {
    std::vector<uint8_t> out;
    const auto copy = [&](size_t plane, int stride, int x, int y, int w, int h) {
        for (int row = 0; row < h; row++) {
            const auto from = plane + static_cast<size_t>(y + row) * static_cast<size_t>(stride) + static_cast<size_t>(x);
            out.insert(out.end(), image.data.begin() + static_cast<std::ptrdiff_t>(from),
                       image.data.begin() + static_cast<std::ptrdiff_t>(from + static_cast<size_t>(w)));
        }
    };
    copy(0, planes.lumaWidth, rect.x, rect.y, rect.width, rect.height);
    copy(planes.luma, planes.chromaWidth, rect.x / 2, rect.y / 2, rect.width / 2, rect.height / 2);
    copy(planes.luma + planes.chroma, planes.chromaWidth,
         rect.x / 2, rect.y / 2, rect.width / 2, rect.height / 2);
    return out;
}

}

namespace Haio::Codecs {

/**
 * @addtogroup encode
 * @{
 */
/**
 * the cuts a row at a time, top to bottom: every cut of the row blitted at low
 * resolution and the row shown at once, then the rows that were skipped blitted in
 * and the row shown again, sharp, before the next one starts.
 */
template <>
Result<Blob> Encode<Format::ZCIS, Color::RGBA8888>(Image<Color::RGBA8888> img, const Metadata& meta, const Settings& settings) {
    HAIO_TRY(level, settingInt(settings, zcisOptimization));
    HAIO_TRY(quality, settingInt(settings, zcisQuality));
    auto lazy = ZcisLazy::Whole;
    if (const auto* named = settingNamed(settings, zcisLazy.name())) {
        if (const auto chosen = Stages::enumNamed<ZcisLazy>(named->value)) lazy = *chosen;
    }
    const auto stretched = lazy == ZcisLazy::Whole;
    const auto expected = static_cast<size_t>(img.width) * static_cast<size_t>(img.height) * 4;
    if (img.width <= 0 || img.height <= 0 || img.data.size() != expected) {
        HAIO_FAIL(InvalidInput, "invalid rgba8888 image for zcis encode");
    }
    HAIO_CHECK(checkCanvas(img.width, img.height));

    // the cuts have to be known before the colour is chosen, since the flat ground
    // the choice counts on is found a cut at a time
    const auto cuts = cutsOf(meta, img.width, img.height);
    const auto slack = Zcis::minify(img, cuts, fillWorth, level, 100 - quality);
    auto plan = Zcis::planOf(img, cuts, fillWorth, level, slack, stretched);
    img = std::move(plan.img);
    const auto& flat = plan.flat;

    std::vector<uint8_t> out;
    HAIO_CHECK(openArchive(out, meta, img.width, img.height));

    int order = 1;
    const auto appendLayer = [&](char command, const Rect& at, const Image<Color::RGBA8888>& layer) -> std::optional<Error> {
        auto payload = Encode<Format::PPM, Color::RGBA8888>(Image<Color::RGBA8888>{layer});
        if (!payload) return payload.error();
        const auto name = std::string(1, orderOf(order++)) + command + target(at.x, at.y, at.width, at.height) + ".ppm";
        return appendMember(out, name, payload->data);
    };

    const auto appendFill = [&](const Zcis::Fill& fill, bool shown) -> std::optional<Error> {
        const auto text = Zcis::spell(fill.rgba);
        const auto name = std::string(1, orderOf(order++)) + commandOf(fillCommand, shown)
                        + target(fill.at.x, fill.at.y, fill.at.width, fill.at.height) + ".txt";
        return appendMember(out, name, std::vector<uint8_t>(text.begin(), text.end()));
    };

    /**
     * the ground first and all of it, because a fill merged across rows has to be
     * down before any of the rows it spans paints, and because a mask written before
     * a fill would erase the fill rather than what stands on it. none of them shows
     * anything by itself: the first picture to commit is what puts the page up.
     */
    const bool nothingPaints = std::ranges::all_of(flat.boxes, [](const Rect& box) { return box.width == 0; });
    for (size_t at = 0; at < flat.fills.size(); at++) {
        HAIO_CHECK(appendFill(flat.fills[at], nothingPaints && at + 1 == flat.fills.size()));
    }
    for (const auto& mask : flat.masks) {
        if (!mask.refine) HAIO_CHECK(appendMask(out, order, mask));
    }

    /**
     * a row is blitted a cut at a time and shown by the last of them, so it appears
     * whole rather than piece by piece, and only the cuts travel: nothing between
     * them is sent. its low resolution layer goes first and the rows it skipped after.
     */
    for (const auto& row : rowsOf(meta, img.width, img.height)) {
        // a cut whose ground swallowed it has nothing left to send, so the commit
        // its row was waiting for belongs to the last one that does
        std::vector<Rect> sending;
        for (const auto& rect : row) {
            const auto* box = level >= 1 ? boxFor(flat, cuts, rect) : nullptr;
            const auto at = box ? *box : rect;
            if (at.width > 0 && at.height > 0) sending.push_back(at);
        }

        std::vector<Rect> taller;
        std::ranges::copy_if(sending, std::back_inserter(taller), [](const Rect& rect) { return rect.height > 1; });

        /**
         * a half preview means neither layer is stretched: each one is interlaced
         * and lands on half the rows, the second a pixel further down than the first.
         *
         * that is the whole of it. the stretched layer paints every row of the box
         * and the sharp one then paints half of them again, so a third of all the
         * drawing there is goes on rows that were about to be painted anyway. two
         * interlaced layers paint each row once and the picture that comes out is
         * the same to the bit.
         *
         * what is given up is the first sight of it: a stretched layer is a whole
         * picture at half the height, and an interlaced one is half a picture.
         */
        if (!stretched) {
            for (size_t at = 0; at < sending.size(); at++) {
                HAIO_CHECK(appendLayer(commandOf(refineCommand, at + 1 == sending.size()), sending[at],
                                       selectRows(piece(img, sending[at]), 0)));
            }
            for (size_t at = 0; at < taller.size(); at++) {
                const auto& rect = taller[at];
                const Rect lower{rect.x, rect.y + 1, rect.width, rect.height - 1};
                HAIO_CHECK(appendLayer(commandOf(refineCommand, at + 1 == taller.size()), lower,
                                       selectRows(piece(img, rect), 1)));
            }
            continue;
        }

        for (size_t at = 0; at < sending.size(); at++) {
            const auto cut = piece(img, sending[at]);
            HAIO_CHECK(appendLayer(commandOf(baseCommand, at + 1 == sending.size()), sending[at],
                                   selectRows(cut, cut.height == 1 ? 0 : 1)));
        }

        // between the two layers, so that it spares the sharp one and not the
        // stretched one that has already been read
        for (const auto& mask : flat.masks) {
            if (!mask.refine) continue;
            const auto inside = std::ranges::any_of(taller, [&](const Rect& rect) {
                return mask.at.x >= rect.x && mask.at.y >= rect.y
                    && mask.at.x + mask.at.width <= rect.x + rect.width
                    && mask.at.y + mask.at.height <= rect.y + rect.height;
            });
            if (inside) HAIO_CHECK(appendMask(out, order, mask));
        }

        for (size_t at = 0; at < taller.size(); at++) {
            HAIO_CHECK(appendLayer(commandOf(refineCommand, at + 1 == taller.size()), taller[at], selectRows(piece(img, taller[at]), 0)));
        }
    }

    return Blob{Format::ZCIS, Color::RGBA8888, "image/x-zcis", {}, std::move(out)};
}

template <>
Result<Blob> Encode<Format::ZCIS, Color::RGBA8888>(Image<Color::RGBA8888> img) {
    return Encode<Format::ZCIS, Color::RGBA8888>(std::move(img), Metadata{}, Settings{});
}

/** the cuts a row at a time, top to bottom, every row blitted a cut at a time and shown by its last */
template <>
Result<Blob> Encode<Format::ZCIS, Color::YUV420>(Image<Color::YUV420> img, const Metadata& meta, const Settings& settings) {
    HAIO_TRY(level, settingInt(settings, zcisOptimization));
    HAIO_TRY(quality, settingInt(settings, zcisQuality));
    if (img.width <= 0 || img.height <= 0) HAIO_FAIL(InvalidInput, "invalid yuv420 image for zcis encode");
    HAIO_TRY(expected, sizeOf(Color::YUV420, Size{img.width, img.height}));
    if (img.data.size() != expected) HAIO_FAIL(InvalidInput, "invalid yuv420 image for zcis encode");
    HAIO_CHECK(checkCanvas(img.width, img.height));

    const auto cuts = cutsOf(meta, img.width, img.height);
    const auto slack = Zcis::minifyYuv(img, cuts, fillWorth, true, level, 100 - quality);
    auto plan = Zcis::planOfYuv(img, cuts, fillWorth, true, level, slack);
    img = std::move(plan.img);
    const auto& flat = plan.flat;

    std::vector<uint8_t> out;
    HAIO_CHECK(openArchive(out, meta, img.width, img.height));

    const auto planes = Yuv420::planesOf(Size{img.width, img.height});

    int order = 1;
    const bool nothingPaints = std::ranges::all_of(flat.boxes, [](const Rect& box) { return box.width == 0; });
    for (size_t at = 0; at < flat.fills.size(); at++) {
        const auto text = Zcis::spell(flat.fills[at].rgba);
        const auto& where = flat.fills[at].at;
        const auto name = std::string(1, orderOf(order++))
                        + commandOf(fillCommand, nothingPaints && at + 1 == flat.fills.size())
                        + target(where.x, where.y, where.width, where.height) + ".txt";
        HAIO_CHECK(appendMember(out, name, std::vector<uint8_t>(text.begin(), text.end())));
    }
    for (const auto& mask : flat.masks) HAIO_CHECK(appendMask(out, order, mask));

    for (const auto& row : rowsOf(meta, img.width, img.height)) {
        std::vector<Rect> sending;
        for (const auto& one : row) {
            const auto* box = level >= 1 ? boxFor(flat, cuts, one) : nullptr;
            const auto at = box ? *box : one;
            if (at.width > 0 && at.height > 0) sending.push_back(at);
        }

        for (size_t at = 0; at < sending.size(); at++) {
            const auto rect = even(sending[at], planes);
            std::vector<uint8_t> payload;
            appendBytes(payload, "YUV4MPEG2 W" + std::to_string(rect.width) + " H" + std::to_string(rect.height)
                                     + " F1:1 Ip A1:1 C420jpeg XCOLORRANGE=FULL\nFRAME\n");
            const auto cut = piece(img, rect, planes);
            payload.insert(payload.end(), cut.begin(), cut.end());

            const auto name = std::string(1, orderOf(order++)) + commandOf(baseCommand, at + 1 == sending.size())
                            + target(rect.x, rect.y, rect.width, rect.height) + ".y4m";
            HAIO_CHECK(appendMember(out, name, payload));
        }
    }

    return Blob{Format::ZCIS, Color::YUV420, "image/x-zcis", {}, std::move(out)};
}

template <>
Result<Blob> Encode<Format::ZCIS, Color::YUV420>(Image<Color::YUV420> img) {
    return Encode<Format::ZCIS, Color::YUV420>(std::move(img), Metadata{}, Settings{});
}
/** @} */

}
