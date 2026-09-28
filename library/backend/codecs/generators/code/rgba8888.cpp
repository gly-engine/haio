#include <haio/codecs/generators/code.hpp>

#include "../zint.hpp"

#include <algorithm>
#include <optional>

namespace Haio::Codecs {
namespace {

/** how many digits an ean or a upc holds, with its check digit and without */
std::optional<Error> digitsFor(std::string_view words, std::string_view what, size_t without) {
    const auto digits = words.substr(0, words.find('+'));   // an add-on after a plus is its own
    if (digits.size() == without || digits.size() == without + 1) return std::nullopt;
    return Error{ErrorCode::InvalidInput, std::string(what) + " takes " + std::to_string(without) + " digits, or "
                                              + std::to_string(without + 1) + " with its check digit "
                                              + Stages::quoted(words)};
}

}

/**
 * a barcode: code 128 unless -format names another. an ean-8 and an ean-13 are one
 * symbology to zint, told apart by how many digits there are, so the digits are
 * counted here to make -format ean8 mean ean-8 rather than whatever the digits say.
 */
template <>
Result<Image<Color::RGBA8888>> Generate<Brush::Code, Color::RGBA8888>(std::string_view words, const Settings& settings) {
    auto format = Barcode::Code128;
    if (const auto* named = settingNamed(settings, barcodeFormat.name())) format = *Stages::enumNamed<Barcode>(named->value);

    int symbology = BARCODE_CODE128;
    switch (format) {
        case Barcode::Code128: symbology = BARCODE_CODE128; break;
        case Barcode::Code39: symbology = BARCODE_CODE39; break;
        case Barcode::Codabar: symbology = BARCODE_CODABAR; break;
        case Barcode::Ean13:
            if (auto wrong = digitsFor(words, "ean-13", 12)) return std::unexpected(*wrong);
            symbology = BARCODE_EANX;
            break;
        case Barcode::Ean8:
            if (auto wrong = digitsFor(words, "ean-8", 7)) return std::unexpected(*wrong);
            symbology = BARCODE_EANX;
            break;
        case Barcode::UpcA:
            if (auto wrong = digitsFor(words, "upc-a", 11)) return std::unexpected(*wrong);
            symbology = BARCODE_UPCA;
            break;
    }
    return Zint::paint(symbology, words, settings, [](zint_symbol&) { return std::optional<Error>{}; });
}

}
