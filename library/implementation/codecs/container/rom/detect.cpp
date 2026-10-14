#include <haio/generated/codecs/classes.hpp>
#include <haio/implementation/codecs.hpp>

namespace Haio::Implementation::WIP {

/** a rom holds pattern data, and pattern data is what haio can draw */
template <> struct DefaultColor<Format::ROM> { static constexpr Color value = Color::CHR_NES; };

}

namespace Haio::Implementation::Codecs {


/** the ines header, which every nes rom on the internet starts with */
bool Detect<Format::ROM, Color::CHR_NES>::operator()(Bytes data) const {
    return data.size() >= 16 && data[0] == 'N' && data[1] == 'E' && data[2] == 'S' && data[3] == 0x1A;
}

}
