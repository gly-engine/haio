#include <haio/codecs/pcx.hpp>
#include <haio/internal/codecs/pcx.hpp>

#include <haio_codec.hpp>
#include <haio_convert.hpp>

namespace Haio::Codecs {

namespace {

void little16(std::vector<uint8_t>& out, size_t at, int value) {
    out[at] = static_cast<uint8_t>(value & 0xFF);
    out[at + 1] = static_cast<uint8_t>((value >> 8) & 0xFF);
}

/**
 * the run length, one scanline at a time.
 *
 * a run never crosses into the next row even when the bytes would allow it: readers
 * of the day decoded a row at a time and some of them still do, and the saving on a
 * boundary is one byte per row.
 *
 * a literal is only a literal below 0xc0, because the top two bits are what marks a
 * count. a lone 0xff therefore costs two bytes, which is the format's own worst case
 * and the reason pcx grows on noise.
 */
void packRow(const uint8_t* row, size_t length, std::vector<uint8_t>& out) {
    size_t at = 0;
    while (at < length) {
        const auto value = row[at];
        size_t run = 1;
        while (at + run < length && row[at + run] == value && run < 63) run++;

        if (run > 1 || value >= 0xC0) {
            out.push_back(static_cast<uint8_t>(0xC0 | run));
            out.push_back(value);
        } else {
            out.push_back(value);
        }
        at += run;
    }
}

/** the row as the header says it is kept: packed, or simply there */
void writeRow(const uint8_t* row, size_t length, PcxCompression how, std::vector<uint8_t>& out) {
    if (how == PcxCompression::Rle) {
        packRow(row, length, out);
        return;
    }
    out.insert(out.end(), row, row + length);
}

/** the choice, read once per picture rather than once per row */
PcxCompression compressionOf(const Settings& settings) {
    if (const auto* named = settingNamed(settings, pcxCompression.name())) {
        if (const auto chosen = Stages::enumNamed<PcxCompression>(named->value)) return *chosen;
    }
    return PcxCompression::Rle;
}

/**
 * the hundred and twenty eight byte header, filled for the only two shapes haio
 * writes: one eight bit plane of indices, or three of colour.
 */
Result<std::vector<uint8_t>> header(int width, int height, uint8_t planes, uint16_t bytesPerLine,
                                    std::span<const uint32_t> ega, PcxCompression how) {
    if (width <= 0 || height <= 0) HAIO_FAIL(InvalidInput, "pcx needs positive dimensions");
    if (width > 0xFFFF || height > 0xFFFF) HAIO_FAIL(InvalidInput, "a pcx says its size in sixteen bits");

    std::vector<uint8_t> out(Pcx::headerBytes, 0);
    out[0] = Pcx::magic;
    out[1] = 5;  // the version that has a 256 colour palette at the end
    out[2] = how == PcxCompression::Rle ? 1 : 0;
    out[3] = 8;
    little16(out, 4, 0);
    little16(out, 6, 0);
    little16(out, 8, width - 1);
    little16(out, 10, height - 1);
    little16(out, 12, 72);
    little16(out, 14, 72);
    out[65] = planes;
    little16(out, 66, bytesPerLine);
    little16(out, 68, 1);  // colour, as opposed to the greyscale claim nobody honours

    // the sixteen colour header palette, which a reader older than version 5 is all
    // it has. writing the start of the real palette there costs nothing and is what
    // every writer did
    for (size_t index = 0; index < Pcx::egaEntries && index < ega.size(); index++) {
        const auto at = Pcx::egaOffset + index * 3;
        out[at + 0] = static_cast<uint8_t>((ega[index] >> 16) & 0xFF);
        out[at + 1] = static_cast<uint8_t>((ega[index] >> 8) & 0xFF);
        out[at + 2] = static_cast<uint8_t>(ega[index] & 0xFF);
    }
    return out;
}

}

/**
 * @addtogroup encode
 * @{
 */
/**
 * one index a pixel, as 256 colours in a single eight bit plane with the palette
 * glued to the end. a picture with fewer colours than that is written at eight bits
 * anyway: packing it to four would halve a file that the run length already
 * flattens, and would cost every reader older than the packing.
 */
template <>
Result<Blob> Encode<Format::PCX, Color::PALETTE>(Image<Color::PALETTE> img, const Settings& settings) {
    const auto how = compressionOf(settings);
    const Size size{img.width, img.height};
    HAIO_TRY(incoming, sizeOf(Color::PALETTE, size));
    if (img.data.size() != incoming) HAIO_FAIL(InvalidInput, "invalid palette image for pcx encode");
    if (img.entries.empty()) HAIO_FAIL(InvalidInput, "a palette picture needs colours to index");
    if (img.entries.size() > 256) HAIO_FAIL(InvalidInput, "a pcx palette holds 256 colours");

    for (const auto index : img.data) {
        if (index >= img.entries.size()) HAIO_FAIL(InvalidInput, "image indexes a colour its palette does not have");
    }

    // the format pads every row to an even byte, and the padding is not pixels
    const auto bytesPerLine = static_cast<uint16_t>(img.width + (img.width % 2));
    HAIO_TRY(out, header(img.width, img.height, 1, bytesPerLine, img.entries, how));

    std::vector<uint8_t> row(bytesPerLine, 0);
    for (int y = 0; y < img.height; y++) {
        const auto* pixels = img.data.data() + static_cast<size_t>(y) * img.width;
        std::copy(pixels, pixels + img.width, row.begin());
        writeRow(row.data(), row.size(), how, out);
    }

    out.push_back(Pcx::vgaMarker);
    for (size_t index = 0; index < 256; index++) {
        const auto colour = index < img.entries.size() ? img.entries[index] : 0xFF000000u;
        out.push_back(static_cast<uint8_t>((colour >> 16) & 0xFF));
        out.push_back(static_cast<uint8_t>((colour >> 8) & 0xFF));
        out.push_back(static_cast<uint8_t>(colour & 0xFF));
    }

    return Blob{Format::PCX, Color::PALETTE, "image/x-pcx", {}, std::move(out)};
}
/** @} */

/**
 * @addtogroup encode
 * @{
 */
/**
 * three planes of eight bits. the rows are not pixels here but a row of reds, then
 * one of greens, then one of blues, which is what makes the run length worth
 * anything on a photograph: a gradient that changes every pixel in rgb still holds
 * still for a while in one channel.
 */
template <>
Result<Blob> Encode<Format::PCX, Color::RGB888>(Image<Color::RGB888> img, const Settings& settings) {
    const auto how = compressionOf(settings);
    const Size size{img.width, img.height};
    HAIO_TRY(incoming, sizeOf(Color::RGB888, size));
    if (img.data.size() != incoming) HAIO_FAIL(InvalidInput, "invalid rgb888 image for pcx encode");

    const auto bytesPerLine = static_cast<uint16_t>(img.width + (img.width % 2));
    HAIO_TRY(out, header(img.width, img.height, 3, bytesPerLine, {}, how));

    std::vector<uint8_t> plane(bytesPerLine, 0);
    for (int y = 0; y < img.height; y++) {
        const auto* pixels = img.data.data() + static_cast<size_t>(y) * img.width * 3;
        for (int channel = 0; channel < 3; channel++) {
            for (int x = 0; x < img.width; x++) plane[x] = pixels[x * 3 + channel];
            writeRow(plane.data(), plane.size(), how, out);
        }
    }

    return Blob{Format::PCX, Color::RGB888, "image/x-pcx", {}, std::move(out)};
}
/** @} */

}
