#include <haio_codec.hpp>
#include <haio_codecs.hpp>
#include <haio_convert.hpp>

namespace {

/**
 * a pattern table: four kilobytes, which is 256 tiles, which is 128 by 128 pixels.
 *
 * this is the unit people mean by a chr bank when they look at one, and it is why
 * every tool that shows chr data shows a 128 by 128 square.
 */
constexpr size_t bankBytes = 4096;
constexpr int bankSide = 128;

}

namespace Haio::Codecs {

/**
 * every pattern table in the cartridge, stacked one below another.
 *
 * the header says where they start: sixteen bytes of it, then a trainer if there is
 * one, then the program itself, and only then the pattern data. picking which bank
 * to look at is left to the caller, because by then it is a crop.
 *
 * @todo a cartridge with no chr rom keeps its patterns in ram and builds them as it
 * runs, so there is nothing in the file to show. those are refused rather than drawn
 * as an empty square.
 */
template <>
Result<Image<Color::CHR_NES>> Decode<Format::ROM, Color::CHR_NES>(const Blob& blob) {
    const auto& data = blob.data;
    if (data.size() < 16) HAIO_FAIL(InvalidInput, "this file is too short to be a rom");

    const size_t programBanks = data[4];
    const size_t patternBanks = static_cast<size_t>(data[5]) * 2;   // 8k of chr is two tables
    const bool hasTrainer = (data[6] & 4) != 0;

    if (patternBanks == 0) {
        HAIO_FAIL(UnsupportedFormat, "this cartridge builds its patterns as it runs, so the file holds none");
    }

    const auto from = 16 + (hasTrainer ? 512u : 0u) + programBanks * 16384;
    if (from + patternBanks * bankBytes > data.size()) {
        HAIO_FAIL(InvalidInput, "the rom header promises more pattern data than the file holds");
    }

    std::vector<uint8_t> out(data.begin() + from, data.begin() + from + patternBanks * bankBytes);
    return Image<Color::CHR_NES>{bankSide, bankSide * static_cast<int>(patternBanks), std::move(out)};
}

}
