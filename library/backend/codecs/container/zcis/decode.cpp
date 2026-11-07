#include <haio/internal/codecs/yuv.hpp>
#include <haio_codec.hpp>
#include <haio/generated/codec.hpp>
#include <haio_convert.hpp>
#include <haio_registry.hpp>

#include <algorithm>
#include <array>
#include <charconv>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace {

using Haio::Color;
using Haio::Image;
using Haio::Rect;
using Haio::Result;

constexpr std::string_view base62 = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";
constexpr std::string_view indexMemberName = "0000000000.txt";

struct Member {
    std::string name;
    Haio::Bytes body;
};

std::string_view trimmed(std::string_view field) {
    while (!field.empty() && field.back() == ' ') field.remove_suffix(1);
    return field;
}

/** every member in the order it was written, which is the order the commands run in */
Result<std::vector<Member>> membersOf(Haio::Bytes data) {
    if (data.size() < 8 || std::string_view(reinterpret_cast<const char*>(data.data()), 8) != "!<arch>\n") {
        HAIO_FAIL(InvalidInput, "not a zcis archive");
    }

    std::vector<Member> out;
    size_t offset = 8;
    while (offset < data.size()) {
        if (offset + 60 > data.size()) HAIO_FAIL(InvalidInput, "truncated zcis member header");
        const std::string_view header(reinterpret_cast<const char*>(data.data() + offset), 60);
        if (header.substr(58) != "`\n") HAIO_FAIL(InvalidInput, "malformed zcis member header");

        auto name = std::string(trimmed(header.substr(0, 16)));
        if (!name.empty() && name.back() == '/') name.pop_back();

        const auto sizeField = trimmed(header.substr(48, 10));
        size_t size = 0;
        const auto parsed = std::from_chars(sizeField.data(), sizeField.data() + sizeField.size(), size);
        if (parsed.ec != std::errc{} || parsed.ptr != sizeField.data() + sizeField.size()) {
            HAIO_FAIL(InvalidInput, "malformed zcis member size: " + name);
        }

        offset += 60;
        if (size > data.size() - offset) HAIO_FAIL(InvalidInput, "truncated zcis member: " + name);
        out.push_back(Member{std::move(name), data.subspan(offset, size)});
        offset += size + size % 2;
    }
    return out;
}

std::optional<int> base62Pair(std::string_view text) {
    const auto high = base62.find(text[0]);
    const auto low = base62.find(text[1]);
    if (high == std::string_view::npos || low == std::string_view::npos) return std::nullopt;
    return static_cast<int>(high * 62 + low);
}

/** the RRGGBBAA a fill member is named by */
std::optional<std::array<uint8_t, 4>> colourOf(std::string_view hex) {
    std::array<uint8_t, 4> out{};
    for (size_t at = 0; at < 4; at++) {
        const auto parsed = std::from_chars(hex.data() + at * 2, hex.data() + at * 2 + 2, out[at], 16);
        if (parsed.ec != std::errc{} || parsed.ptr != hex.data() + at * 2 + 2) return std::nullopt;
    }
    return out;
}

/** the decimal numbers of a text member, whatever spacing sits between them */
std::vector<long> numbersOf(Haio::Bytes body) {
    std::vector<long> out;
    const auto* at = reinterpret_cast<const char*>(body.data());
    const auto* end = at + body.size();
    while (at < end) {
        if (*at < '0' || *at > '9') {
            at++;
            continue;
        }
        long value = 0;
        at = std::from_chars(at, end, value).ptr;
        out.push_back(value);
    }
    return out;
}

/**
 * the canvas the cuts were laid on, which is as far as the index reaches. a chunk is
 * free to stick out of it -- a 4:2:0 one grows to even edges -- and what sticks out
 * is simply not painted.
 */
Result<Haio::Size> canvasOf(const Member& index) {
    const auto numbers = numbersOf(index.body);
    if (numbers.empty() || numbers.size() % 4 != 0) HAIO_FAIL(InvalidInput, "malformed zcis index");

    Haio::Size size{0, 0};
    for (size_t at = 0; at < numbers.size(); at += 4) {
        size.width = std::max(size.width, static_cast<int>(numbers[at] + numbers[at + 2]));
        size.height = std::max(size.height, static_cast<int>(numbers[at + 1] + numbers[at + 3]));
    }
    if (size.width <= 0 || size.height <= 0 || size.width > 62 * 62 || size.height > 62 * 62) {
        HAIO_FAIL(InvalidInput, "zcis index names an empty or oversized canvas");
    }
    return size;
}

/** the 4:2:0 frame a chunk carries, as the encoder writes it: one frame, full range */
Result<Image<Color::RGBA8888>> decodeY4m(Haio::Bytes body) {
    const std::string_view text(reinterpret_cast<const char*>(body.data()), body.size());
    const auto headerEnd = text.find('\n');
    if (!text.starts_with("YUV4MPEG2 ") || headerEnd == std::string_view::npos) HAIO_FAIL(InvalidInput, "not a y4m");

    int width = 0;
    int height = 0;
    auto rest = text.substr(10, headerEnd - 10);
    while (!rest.empty()) {
        const auto space = rest.find(' ');
        const auto token = rest.substr(0, space);
        const auto number = [&](int& into) { std::from_chars(token.data() + 1, token.data() + token.size(), into); };
        if (token.starts_with('W')) number(width);
        else if (token.starts_with('H')) number(height);
        else if (token.starts_with('C') && !token.starts_with("C420")) HAIO_FAIL(InvalidInput, "y4m chroma is not 4:2:0");
        rest = space == std::string_view::npos ? std::string_view{} : rest.substr(space + 1);
    }
    if (width <= 0 || height <= 0) HAIO_FAIL(InvalidInput, "y4m without a size");

    const auto frame = text.find('\n', headerEnd + 1);
    if (frame == std::string_view::npos || !text.substr(headerEnd + 1).starts_with("FRAME")) {
        HAIO_FAIL(InvalidInput, "y4m without a frame");
    }

    HAIO_TRY(expected, Haio::sizeOf(Color::YUV420, Haio::Size{width, height}));
    const auto planes = body.subspan(frame + 1);
    if (planes.size() < expected) HAIO_FAIL(InvalidInput, "truncated y4m frame");

    Image<Color::YUV420> yuv{width, height, {planes.begin(), planes.begin() + static_cast<std::ptrdiff_t>(expected)}};
    return Haio::Codecs::Convert<Color::YUV420, Color::RGBA8888>(std::move(yuv));
}

Result<Image<Color::RGBA8888>> decodeChunk(const Member& member) {
    if (member.name.ends_with(".y4m")) return decodeY4m(member.body);
    return Haio::Decode(Haio::Blob{.data = {member.body.begin(), member.body.end()}});
}

/**
 * the picture as the television would leave it once the last member is read.
 *
 * a mask is a stencil and it stays: a clear bit keeps every later command off that
 * pixel, so that what the ground painted under it survives. the masks add up, which
 * is what lets the one laid between the layers spare the sharp one without undoing
 * what the ground mask already spared.
 */
struct Screen {
    Image<Color::RGBA8888> img;
    std::vector<uint8_t> kept;

    explicit Screen(Haio::Size size)
        : img{size.width, size.height, std::vector<uint8_t>(static_cast<size_t>(size.width) * size.height * 4, 0)},
          kept(static_cast<size_t>(size.width) * size.height, 0) {}

    /**
     * the chunk scaled to the target by nearest neighbour, and painted only where the
     * glyph of the command has a dot: every pixel for a solid tint, the first column,
     * row or pixel of every 2x2 cell for the interlaced ones.
     */
    template <typename At>
    void paint(const Rect& target, int stepX, int stepY, const At& colourAt) {
        for (int y = 0; y < target.height; y += stepY) {
            const int row = target.y + y;
            if (row < 0 || row >= img.height) continue;
            for (int x = 0; x < target.width; x += stepX) {
                const int column = target.x + x;
                if (column < 0 || column >= img.width) continue;
                const auto pixel = static_cast<size_t>(row) * img.width + column;
                if (kept[pixel]) continue;
                colourAt(x, y, img.data.data() + pixel * 4);
            }
        }
    }

    void tint(const Rect& target, const Image<Color::RGBA8888>& chunk, int stepX, int stepY) {
        paint(target, stepX, stepY, [&](int x, int y, uint8_t* out) {
            const auto sx = static_cast<size_t>(x) * chunk.width / target.width;
            const auto sy = static_cast<size_t>(y) * chunk.height / target.height;
            std::copy_n(chunk.data.data() + (sy * chunk.width + sx) * 4, 4, out);
        });
    }

    void fill(const Rect& target, const std::array<uint8_t, 4>& rgba) {
        paint(target, 1, 1, [&](int, int, uint8_t* out) { std::copy(rgba.begin(), rgba.end(), out); });
    }

    void mask(const Rect& target, const Image<Color::RGBA8888>& bits) {
        for (int y = 0; y < target.height; y++) {
            const int row = target.y + y;
            if (row < 0 || row >= img.height) continue;
            for (int x = 0; x < target.width; x++) {
                const int column = target.x + x;
                if (column < 0 || column >= img.width) continue;
                const auto sx = static_cast<size_t>(x) * bits.width / target.width;
                const auto sy = static_cast<size_t>(y) * bits.height / target.height;
                if (bits.data[(sy * bits.width + sx) * 4] == 0) kept[static_cast<size_t>(row) * img.width + column] = 1;
            }
        }
    }
};

}

namespace Haio::Codecs {

/**
 * @addtogroup decode
 * @{
 */
/**
 * the whole canvas, every command run in the order the archive holds them. blit and
 * commit only differ in when the television shows the page, so here they are the
 * same; what comes out is the picture once everything has landed.
 *
 * the chunks may be in any format haio reads, as the standard allows, plus the
 * 4:2:0 y4m the encoder writes.
 */
template <>
Result<Image<Color::RGBA8888>> Decode<Format::ZCIS, Color::RGBA8888>(const Blob& blob) {
    HAIO_TRY(members, membersOf(blob.data));
    if (members.empty() || members.front().name != indexMemberName) HAIO_FAIL(InvalidInput, "zcis without an index");
    HAIO_TRY(size, canvasOf(members.front()));

    Screen canvas(size);
    for (size_t at = 1; at < members.size(); at++) {
        const auto& member = members[at];
        const std::string_view name(member.name);
        if (name.find('.') != 10) HAIO_FAIL(InvalidInput, "malformed zcis member name: " + member.name);

        const auto command = static_cast<char>(name[1] | 0x20);
        if (command == 'f') {
            const auto colour = colourOf(name.substr(2, 8));
            const auto numbers = numbersOf(member.body);
            if (!colour || numbers.size() % 4 != 0) HAIO_FAIL(InvalidInput, "malformed zcis fill: " + member.name);
            for (size_t n = 0; n < numbers.size(); n += 4) {
                canvas.fill(Rect{static_cast<int>(numbers[n]), static_cast<int>(numbers[n + 1]),
                                 static_cast<int>(numbers[n + 2]), static_cast<int>(numbers[n + 3])}, *colour);
            }
            continue;
        }

        const auto x = base62Pair(name.substr(2, 2));
        const auto y = base62Pair(name.substr(4, 2));
        const auto width = base62Pair(name.substr(6, 2));
        const auto height = base62Pair(name.substr(8, 2));
        if (!x || !y || !width || !height) HAIO_FAIL(InvalidInput, "malformed zcis member name: " + member.name);
        const Rect target{*x, *y, *width, *height};
        if (target.width == 0 || target.height == 0) continue;

        if (command < 'a' || command > 'e') HAIO_FAIL(InvalidInput, "unknown zcis command: " + member.name);
        HAIO_TRY(chunk, decodeChunk(member));
        if (chunk.width <= 0 || chunk.height <= 0) HAIO_FAIL(InvalidInput, "empty zcis chunk: " + member.name);

        switch (command) {
            case 'a': canvas.tint(target, chunk, 1, 1); break;
            case 'b': canvas.tint(target, chunk, 2, 1); break;
            case 'c': canvas.tint(target, chunk, 1, 2); break;
            case 'd': canvas.tint(target, chunk, 2, 2); break;
            case 'e': canvas.mask(target, chunk); break;
        }
    }
    return std::move(canvas.img);
}
/** @} */

}
