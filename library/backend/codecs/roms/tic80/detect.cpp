#include <haio/internal/codecs/roms/tic80.hpp>
#include <haio_codec.hpp>

namespace Haio::Codecs {

/**
 * @addtogroup detect
 * @{
 */
/** the colour a cartridge is detected as is which console it came from */
template <>
bool Detect<Format::ROM, Color::RGBA8888>(Bytes data) {
    return Tic80::isCartridge(data);
}
/** @} */

}
