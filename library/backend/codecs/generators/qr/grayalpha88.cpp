#include <haio/codecs/generators/qr.hpp>

#include "../zint.hpp"

#include <optional>

namespace Haio::Codecs {

/**
 * a qr code, or a data matrix when -format says so, with -margin modules around it
 * and a hole in the middle when -hole asks for one. a hole is data that is not
 * there, so a qr code with one is written at level h -- the most it can lose, about
 * 30% -- unless qr:level says.
 */
template <>
Result<Image<Color::GRAYALPHA88>> Generate<Brush::Qr, Color::GRAYALPHA88>(std::string_view words, const Settings& settings) {
    auto format = Matrix::QrCode;
    if (const auto* named = settingNamed(settings, matrixFormat.name())) format = *Stages::enumNamed<Matrix>(named->value);

    const auto* level = settingNamed(settings, qrLevel.name());
    const auto* asked = settingNamed(settings, qrHole.name());
    if (format != Matrix::QrCode && (level || asked)) {
        HAIO_FAIL(InvalidInput, std::string(level ? "qr:level" : "-hole") + " is for a qr code, and this is a data matrix "
                                    + Stages::quoted((level ? level : asked)->value));
    }

    HAIO_TRY(margin, settingInt(settings, qrMargin));

    if (format == Matrix::DataMatrix) {
        return Zint::paint(BARCODE_DATAMATRIX, words, settings, [](zint_symbol&) { return std::optional<Error>{}; }, margin);
    }

    std::optional<Size> hole;
    if (asked) {
        Size wanted;
        if (!String::tryGetSize(asked->value, wanted)) HAIO_FAIL(InvalidInput, "invalid argument for option `-hole': " + asked->value);
        hole = wanted;
    }

    return Zint::paint(BARCODE_QRCODE, words, settings, [&](zint_symbol& symbol) {
        // zint counts the levels from one, l to h
        if (level) symbol.option_1 = static_cast<int>(*Stages::enumNamed<QrLevel>(level->value)) + 1;
        else if (hole) symbol.option_1 = static_cast<int>(QrLevel::H) + 1;
        return std::optional<Error>{};
    }, margin, hole);
}

}
