#include <haio/generated/codecs/classes.hpp>
#include <haio/implementation/codecs/generators/code.hpp>

#include "../zint.hpp"

#include <algorithm>
#include <optional>

namespace Haio::Implementation::Codecs {
namespace {

/** how many digits an ean or a upc holds, with its check digit and without */
std::optional<Error> digitsFor(std::string_view words, std::string_view what, size_t without) {
    const auto digits = words.substr(0, words.find('+'));   // an add-on after a plus is its own
    if (digits.size() == without || digits.size() == without + 1) return std::nullopt;
    return Error{ErrorCode::InvalidInput, std::string(what) + " takes " + std::to_string(without) + " digits, or "
                                              + std::to_string(without + 1) + " with its check digit "
                                              + Util::quoted(words)};
}

}

/**
 * a barcode: code 128 unless -format names another. an ean-8 and an ean-13 are one
 * symbology to zint, told apart by how many digits there are, so the digits are
 * counted here to make -format ean8 mean ean-8 rather than whatever the digits say.
 */
CodecRes<Color::GRAYALPHA88> Generate<Brush::Code, Color::GRAYALPHA88>::operator()(std::string_view words, const Settings& settings) const {
    auto format = Generate<>::Barcode::Code128;
    if (const auto* named = Util::settingNamed(settings, barcodeFormat.name())) format = *Util::enumNamed<Generate<>::Barcode>(named->value);

    int symbology = BARCODE_CODE128;
    switch (format) {
        case Generate<>::Barcode::Code128: symbology = BARCODE_CODE128; break;
        case Generate<>::Barcode::Code39: symbology = BARCODE_CODE39; break;
        case Generate<>::Barcode::Codabar: symbology = BARCODE_CODABAR; break;
        case Generate<>::Barcode::Ean13:
            if (auto wrong = digitsFor(words, "ean-13", 12)) return std::unexpected(*wrong);
            symbology = BARCODE_EANX;
            break;
        case Generate<>::Barcode::Ean8:
            if (auto wrong = digitsFor(words, "ean-8", 7)) return std::unexpected(*wrong);
            symbology = BARCODE_EANX;
            break;
        case Generate<>::Barcode::UpcA:
            if (auto wrong = digitsFor(words, "upc-a", 11)) return std::unexpected(*wrong);
            symbology = BARCODE_UPCA;
            break;
    }
    return Util::getZintImage(symbology, words, settings, [](zint_symbol&) { return std::optional<Error>{}; });
}

}
