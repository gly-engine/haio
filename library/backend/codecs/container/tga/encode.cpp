#include "header.hpp"

#include <haio_codec.hpp>
#include <haio_codecs.hpp>
#include <haio_convert.hpp>

#include <algorithm>

namespace {

using namespace Haio;

/** mapped, truecolor or grey: the three the format has, and none of them compressed */
constexpr uint8_t imageTypeOf(Color colour) {
    if (colour == Color::PALETTE) return 1;
    if (colour == Color::GRAY8) return 3;
    return 2;
}

/**
 * how many of the sixteen bits the file claims are an alpha.
 *
 * it is the only thing that tells a reader apart the two sixteen bit colours and the
 * only thing that tells it a thirty two bit picture is not padded, so it is written
 * from the colour rather than left at zero the way a lot of writers leave it.
 */
constexpr uint8_t attributeBitsOf(Color colour) {
    if (colour == Color::BGRA8888) return 8;
    if (colour == Color::RGBA5551) return 1;
    return 0;
}

void appendU16LE(std::vector<uint8_t>& out, uint16_t value) {
    out.push_back(static_cast<uint8_t>(value));
    out.push_back(static_cast<uint8_t>(value >> 8));
}

/** haio's rgba5551 rotated back one bit, which is the format's own A1R5G5B5 */
void appendRotated(std::vector<uint8_t>& out, Bytes pixels) {
    for (size_t at = 0; at + 1 < pixels.size(); at += 2) {
        const auto packed = static_cast<uint16_t>(pixels[at] | (pixels[at + 1] << 8));
        appendU16LE(out, static_cast<uint16_t>((packed >> 1) | (packed << 15)));
    }
}

/**
 * one body for every colour the container can hold.
 *
 * everything it writes is uncompressed and starts at the top left, which is the one
 * corner every reader agrees on. the picture is already in the layout the file wants
 * for all but rgba5551, so this is a header, a colour map when there is one, and a
 * copy.
 *
 * @todo the runs are not written. tga's run length encoding is trivial and would pay
 * for itself on flat art, which is most of what gets saved as a tga; a plain file is
 * simply the version that every reader takes.
 */
template <Color P>
Result<Blob> encodeTga(Image<P> img) {
    const Size size{img.width, img.height};
    HAIO_TRY(expected, sizeOf(P, size));
    if (img.data.size() != expected) HAIO_FAIL(InvalidInput, "tga payload does not match its size");
    if (img.width > 0xffff || img.height > 0xffff) {
        HAIO_FAIL(InvalidInput, "a tga says its size in two bytes, and this picture is wider than that reaches");
    }

    std::vector<uint32_t> entries;
    if constexpr (P == Color::PALETTE) {
        entries = img.entries;
        if (entries.empty()) HAIO_FAIL(InvalidInput, "this picture is indices with no palette to write beside them");
        if (entries.size() > 256) HAIO_FAIL(InvalidInput, "a tga colour map here is reached by one byte, so it holds at most 256 colours");
        if (const auto highest = std::ranges::max(img.data); highest >= entries.size()) {
            HAIO_FAIL(InvalidInput, "the picture points at colour " + std::to_string(highest)
                                        + " and the palette has " + std::to_string(entries.size()));
        }
    }

    std::vector<uint8_t> out;
    out.reserve(Codecs::Tga::headerBytes + entries.size() * 3 + img.data.size() + Codecs::Tga::footerBytes);

    out.push_back(0);                                   // no id field
    out.push_back(entries.empty() ? 0 : 1);
    out.push_back(imageTypeOf(P));
    appendU16LE(out, 0);                                // the map starts at its first colour
    appendU16LE(out, static_cast<uint16_t>(entries.size()));
    out.push_back(entries.empty() ? 0 : 24);
    appendU16LE(out, 0);                                // where it sits on a screen is
    appendU16LE(out, 0);                                // not part of the picture
    appendU16LE(out, static_cast<uint16_t>(img.width));
    appendU16LE(out, static_cast<uint16_t>(img.height));
    out.push_back(static_cast<uint8_t>(strideOf(P) * 8));
    // bit five is the origin: haio writes top left, so nothing downstream has to flip
    out.push_back(static_cast<uint8_t>(0x20 | attributeBitsOf(P)));

    /**
     * the map goes out as three bytes an entry.
     *
     * a palette here is 0x00RRGGBB and its top byte is not read anywhere: the
     * conversion back to full colour forces every pixel opaque. writing a four byte
     * map would put an alpha in the file that haio never had, and the first tool to
     * read it back would find a palette of invisible colours.
     */
    for (const auto colour : entries) {
        out.push_back(static_cast<uint8_t>(colour));
        out.push_back(static_cast<uint8_t>(colour >> 8));
        out.push_back(static_cast<uint8_t>(colour >> 16));
    }

    if constexpr (P == Color::RGBA5551) {
        appendRotated(out, img.data);
    } else {
        out.insert(out.end(), img.data.begin(), img.data.end());
    }

    // the footer is what makes a tga recognisable at all, so haio's own files carry
    // one even though nothing else in them needs the two offsets it holds
    for (int i = 0; i < 8; i++) out.push_back(0);
    out.insert(out.end(), Codecs::Tga::footerSignature.begin(), Codecs::Tga::footerSignature.end());
    out.push_back(0);

    return Blob{Format::TGA, P, "image/x-tga", {}, std::move(out)};
}

}

namespace Haio::Codecs {

/**
 * @addtogroup encode
 * @{
 */
/** everything a picture arrives with, which is why this is what a bare tga means */
template <>
Result<Blob> Encode<Format::TGA, Color::BGRA8888>(Image<Color::BGRA8888> img) {
    return encodeTga<Color::BGRA8888>(std::move(img));
}
/** @} */

/**
 * @addtogroup encode
 * @{
 */
template <>
Result<Blob> Encode<Format::TGA, Color::BGR888>(Image<Color::BGR888> img) {
    return encodeTga<Color::BGR888>(std::move(img));
}
/** @} */

/**
 * @addtogroup encode
 * @{
 */
template <>
Result<Blob> Encode<Format::TGA, Color::RGBA5551>(Image<Color::RGBA5551> img) {
    return encodeTga<Color::RGBA5551>(std::move(img));
}
/** @} */

/**
 * @addtogroup encode
 * @{
 */
template <>
Result<Blob> Encode<Format::TGA, Color::RGB555>(Image<Color::RGB555> img) {
    return encodeTga<Color::RGB555>(std::move(img));
}
/** @} */

/**
 * @addtogroup encode
 * @{
 */
/**
 * the indices as they are, with their own colours beside them.
 *
 * this is the one container haio has that stores a palette rather than expanding it,
 * so "-filter bayer -palete cga out.tga" comes out as one byte a pixel instead of
 * four holding sixteen distinct values.
 */
template <>
Result<Blob> Encode<Format::TGA, Color::PALETTE>(Image<Color::PALETTE> img) {
    return encodeTga<Color::PALETTE>(std::move(img));
}
/** @} */

/**
 * @addtogroup encode
 * @{
 */
/**
 * one byte a pixel and no colour at all. nothing converts full colour down to grey
 * yet, so this is reached by a typed pipe from something already grey, such as a
 * netpbm P5.
 */
template <>
Result<Blob> Encode<Format::TGA, Color::GRAY8>(Image<Color::GRAY8> img) {
    return encodeTga<Color::GRAY8>(std::move(img));
}
/** @} */

}
