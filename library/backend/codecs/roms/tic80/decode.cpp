#include <haio/internal/codecs/roms/tic80.hpp>
#include <haio/codecs/rom.hpp>
#include <haio_codec.hpp>
#include <haio_convert.hpp>

namespace Haio::Codecs {

/**
 * @addtogroup decode
 * @{
 */

/** the chunk -define tic80:read names, or the cover when it names none */
template <>
Result<Image<Color::RGBA8888>> Decode<Format::ROM, Color::RGBA8888>(const Blob& blob, const Settings& settings) {
    HAIO_TRY(sectors, Tic80::sectorsOf(blob.data));

    const auto* setting = settingNamed(settings, tic80Read.name());
    if (!setting || setting->value.empty()) {
        const auto* shown = Tic80::cover(sectors);
        if (!shown) {
            HAIO_FAIL(InvalidInput, "this cartridge keeps no cover; name a chunk with -define "
                                    + std::string(tic80Read.name()) + ", one of " + Roms::listed(sectors));
        }
        return Tic80::draw(blob.data, sectors, *shown);
    }

    const auto* wanted = Roms::named(sectors, setting->value);
    if (!wanted) {
        HAIO_FAIL(InvalidInput, "no " + setting->value + " in this cartridge; it holds " + Roms::listed(sectors));
    }
    if (!wanted->drawable) {
        HAIO_FAIL(InvalidInput, setting->value + " is a part of the cartridge that is not a picture; "
                                "the ones that draw are " + Roms::drawable(sectors));
    }
    return Tic80::draw(blob.data, sectors, *wanted);
}

/** @} */

}
