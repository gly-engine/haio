#include <haio/generated/codecs/classes.hpp>
#include <haio/implementation/codecs.hpp>
#include <haio/util.hpp>

namespace Haio::Implementation::WIP {

/** every layer inside is a ppm, so rgba is what the encoder takes in */
template <> struct DefaultColor<Format::ZCIS> { static constexpr Color value = Color::RGBA8888; };

}

namespace Haio::Implementation::Codecs {

/**
 * @todo zcis has no decoder at all, so a .zcis round trip is impossible. it also
 * cannot say which colour is inside without reading the member names.
 */
bool Detect<Format::ZCIS, Color::RGBA8888>::operator()(Bytes data) const {
    return Util::matches(data, "!<arch>\n");
}
}
