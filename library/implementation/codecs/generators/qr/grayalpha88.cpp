#include <haio/generated/codecs/classes.hpp>
#include <haio/implementation/codecs/generators/qr.hpp>

#include "../zint.hpp"

#include <optional>

namespace Haio::Implementation::Codecs {

/**
 * a qr code, or a data matrix when -format says so, with -margin modules around it
 * and a hole in the middle when -hole asks for one. a hole is data that is not
 * there, so a qr code with one is written at level h -- the most it can lose, about
 * 30% -- unless qr:level says.
 */
CodecRes<Color::GRAYALPHA88> Generate<Brush::Qr, Color::GRAYALPHA88>::operator()(std::string_view words, const Settings& settings) const {
    auto format = Generate<>::Matrix::QrCode;
    if (const auto* named = Util::settingNamed(settings, matrixFormat.name())) format = *Util::enumNamed<Generate<>::Matrix>(named->value);

    const auto* level = Util::settingNamed(settings, qrLevel.name());
    const auto* asked = Util::settingNamed(settings, qrHole.name());
    if (format != Generate<>::Matrix::QrCode && (level || asked)) {
        HAIO_FAIL(InvalidInput, std::string(level ? "qr:level" : "-hole") + " is for a qr code, and this is a data matrix "
                                    + Util::quoted((level ? level : asked)->value));
    }

    HAIO_TRY(margin, Util::settingInt(settings, qrMargin));

    if (format == Generate<>::Matrix::DataMatrix) {
        return Util::getZintImage(BARCODE_DATAMATRIX, words, settings, [](zint_symbol&) { return std::optional<Error>{}; }, margin);
    }

    std::optional<Size> hole;
    if (asked) {
        Size wanted;
        if (!Util::tryGetSize(asked->value, wanted)) HAIO_FAIL(InvalidInput, "invalid argument for option `-hole': " + asked->value);
        hole = wanted;
    }

    return Util::getZintImage(BARCODE_QRCODE, words, settings, [&](zint_symbol& symbol) {
        // zint counts the levels from one, l to h
        if (level) symbol.option_1 = static_cast<int>(*Util::enumNamed<Generate<>::QrLevel>(level->value)) + 1;
        else if (hole) symbol.option_1 = static_cast<int>(Generate<>::QrLevel::H) + 1;
        return std::optional<Error>{};
    }, margin, hole);
}

}
