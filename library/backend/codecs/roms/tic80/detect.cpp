#include <haio/internal/codecs/roms/tic80.hpp>
#include <haio_codec.hpp>

namespace Haio::Codecs {

/**
 * @addtogroup detect
 * @{
 */

/**
 * a tic-80 cartridge, which answers in rgba8888 because that is what comes out of
 * it: its chunks are four bits a pixel and its colours are in a chunk of their own,
 * so there is nothing here anybody else could hold.
 *
 * the nes rom beside this one answers in chr_nes instead, which is how one Format
 * carries two consoles: the colour a cartridge is detected as is which console it
 * came from, and nothing outside these files has to know that.
 */
template <>
bool Detect<Format::ROM, Color::RGBA8888>(Bytes data) {
    return Tic80::isCartridge(data);
}

/** @} */

}
