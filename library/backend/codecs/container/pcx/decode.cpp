#include <haio/internal/codecs/pcx.hpp>

#include <haio_codec.hpp>

namespace Haio::Codecs {

namespace {

using Pcx::Header;

/**
 * the run length pcx uses: a byte with its top two bits set is a count of the byte
 * after it, and anything else is itself. the count is six bits, so a run is at most
 * 63 and a literal above 0xbf has to be written as a run of one.
 *
 * the whole picture is unpacked at once rather than a row at a time because the
 * rows are a fixed width here: a file that stops early is truncated, and one that
 * runs long is lying about its size, and both are worth saying out loud.
 */
Result<std::vector<uint8_t>> unpack(Bytes data, const Header& header) {
    const auto wanted = header.rowBytes() * static_cast<size_t>(header.height);
    if (wanted == 0) HAIO_FAIL(InvalidInput, "pcx has no pixels");

    const auto start = Pcx::headerBytes;
    if (data.size() <= start) HAIO_FAIL(InvalidInput, "pcx has a header and nothing else");

    // the 256 colour palette is not pixels, so it is not there to be unpacked
    auto limit = data.size();
    if (Pcx::hasVgaTrailer(data, header)) limit -= Pcx::vgaTrailerBytes;

    std::vector<uint8_t> out;
    out.reserve(wanted);

    if (header.encoding == 0) {
        if (limit - start < wanted) HAIO_FAIL(InvalidInput, "truncated pcx pixels");
        out.assign(data.begin() + static_cast<ptrdiff_t>(start),
                   data.begin() + static_cast<ptrdiff_t>(start + wanted));
        return out;
    }

    size_t at = start;
    while (out.size() < wanted) {
        if (at >= limit) HAIO_FAIL(InvalidInput, "truncated pcx pixels");
        const auto byte = data[at++];
        if ((byte & 0xC0) == 0xC0) {
            const size_t run = byte & 0x3F;
            if (run == 0 || at >= limit) HAIO_FAIL(InvalidInput, "invalid pcx run");
            // a run that would spill past the last row is the file disagreeing with
            // its own dimensions, not something to silently trim
            if (out.size() + run > wanted) HAIO_FAIL(InvalidInput, "pcx run runs past the last row");
            out.insert(out.end(), run, data[at++]);
        } else {
            out.push_back(byte);
        }
    }
    return out;
}

/** one pixel's worth of a plane, with the leftmost pixel in the top bits */
uint8_t sample(const uint8_t* row, int x, uint8_t bits) {
    if (bits == 8) return row[x];
    const auto perByte = 8 / bits;
    const auto byte = row[x / perByte];
    const auto shift = (perByte - 1 - (x % perByte)) * bits;
    return static_cast<uint8_t>((byte >> shift) & ((1u << bits) - 1u));
}

/**
 * the planes of one row, folded back into one index a pixel. plane zero holds the
 * low bits, which is the order ega wired its four one bit planes in and the order
 * every pcx has used since.
 */
void foldPlanes(const uint8_t* row, const Header& header, uint8_t* out) {
    for (int x = 0; x < header.width; x++) {
        unsigned index = 0;
        for (int plane = 0; plane < header.planes; plane++) {
            const auto* data = row + static_cast<size_t>(plane) * header.bytesPerLine;
            index |= static_cast<unsigned>(sample(data, x, header.bits)) << (plane * header.bits);
        }
        out[x] = static_cast<uint8_t>(index);
    }
}

uint32_t rgb(uint8_t r, uint8_t g, uint8_t b) {
    return 0xFF000000u | (static_cast<uint32_t>(r) << 16) | (static_cast<uint32_t>(g) << 8) | b;
}

/**
 * where the colours come from: the trailer for 256, the header for the rest.
 *
 * a one bit picture whose header palette says nothing -- which is most of them,
 * since the writers of the day left it zeroed -- is black and white, because a
 * palette of two identical colours is not a picture anybody meant to make.
 */
std::vector<uint32_t> readPalette(Bytes data, const Header& header) {
    std::vector<uint32_t> entries;

    if (Pcx::hasVgaTrailer(data, header)) {
        const auto at = data.size() - Pcx::vgaTrailerBytes + 1;
        entries.reserve(256);
        for (size_t index = 0; index < 256; index++) {
            const auto byte = at + index * 3;
            entries.push_back(rgb(data[byte], data[byte + 1], data[byte + 2]));
        }
        return entries;
    }

    const auto colors = header.colors();
    entries.reserve(colors);
    for (size_t index = 0; index < colors && index < Pcx::egaEntries; index++) {
        const auto byte = Pcx::egaOffset + index * 3;
        entries.push_back(rgb(data[byte], data[byte + 1], data[byte + 2]));
    }
    while (entries.size() < colors) entries.push_back(0xFF000000u);

    const bool blank = entries.size() == 2 && entries[0] == entries[1];
    if (colors == 2 && (blank || entries[1] == 0xFF000000u)) {
        entries[0] = 0xFF000000u;
        entries[1] = 0xFFFFFFFFu;
    }
    return entries;
}

}

/**
 * @addtogroup decode
 * @{
 */
/**
 * every pcx that indexes, at any of the depths it indexes at, handed back as one
 * index a pixel. a four bit picture does not stay four bits here: haio's palette
 * colour is a byte an index everywhere, so that a crop or a resize can address a
 * pixel without knowing how the file packed it.
 */
template <>
Result<Image<Color::PALETTE>> Decode<Format::PCX, Color::PALETTE>(const Blob& blob) {
    const auto header = Pcx::readHeader(blob.data);
    if (!header) HAIO_FAIL(InvalidInput, "not a pcx");

    const auto colour = Pcx::colorOf(*header);
    if (!colour || *colour != Color::PALETTE) {
        HAIO_FAIL(InvalidInput, "this pcx is " + std::to_string(header->bits) + " bits across "
                                    + std::to_string(header->planes) + " planes, which is not an indexed picture");
    }

    HAIO_TRY(rows, unpack(blob.data, *header));

    std::vector<uint8_t> pixels(static_cast<size_t>(header->width) * static_cast<size_t>(header->height));
    for (int y = 0; y < header->height; y++) {
        foldPlanes(rows.data() + static_cast<size_t>(y) * header->rowBytes(), *header,
                   pixels.data() + static_cast<size_t>(y) * header->width);
    }

    Image<Color::PALETTE> out{header->width, header->height, std::move(pixels)};
    out.entries = readPalette(blob.data, *header);

    for (const auto index : out.data) {
        if (index >= out.entries.size()) HAIO_FAIL(InvalidInput, "pcx indexes a colour its palette does not have");
    }
    return out;
}
/** @} */

/**
 * @addtogroup decode
 * @{
 */
/** three planes of eight bits, which is the one pcx that carries colour per pixel */
template <>
Result<Image<Color::RGB888>> Decode<Format::PCX, Color::RGB888>(const Blob& blob) {
    const auto header = Pcx::readHeader(blob.data);
    if (!header) HAIO_FAIL(InvalidInput, "not a pcx");

    const auto colour = Pcx::colorOf(*header);
    if (!colour || *colour != Color::RGB888) HAIO_FAIL(InvalidInput, "this pcx is not twenty four bit colour");

    HAIO_TRY(rows, unpack(blob.data, *header));

    std::vector<uint8_t> pixels(static_cast<size_t>(header->width) * static_cast<size_t>(header->height) * 3);
    for (int y = 0; y < header->height; y++) {
        const auto* row = rows.data() + static_cast<size_t>(y) * header->rowBytes();
        auto* out = pixels.data() + static_cast<size_t>(y) * header->width * 3;
        for (int x = 0; x < header->width; x++) {
            out[x * 3 + 0] = row[x];
            out[x * 3 + 1] = row[static_cast<size_t>(header->bytesPerLine) + x];
            out[x * 3 + 2] = row[static_cast<size_t>(header->bytesPerLine) * 2 + x];
        }
    }
    return Image<Color::RGB888>{header->width, header->height, std::move(pixels)};
}
/** @} */

}
