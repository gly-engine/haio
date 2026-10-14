#include <haio/generated/codecs/classes.hpp>
#include <haio/implementation/codecs.hpp>
#include <haio/util.hpp>

namespace {

constexpr std::array<uint8_t, 6> signature87a = {
    'G', 'I', 'F', '8', '7', 'a'
};

constexpr std::array<uint8_t, 6> signature89a = {
    'G', 'I', 'F', '8', '9', 'a'
};

bool headerSays(Haio::Implementation::Bytes data) {
    return Haio::Util::matches(data, signature87a)
        || Haio::Util::matches(data, signature89a);
}

}

namespace Haio::Implementation::WIP {

/** what a gif holds when nobody asks for something else */
template <> struct DefaultColor<Format::GIF> { static constexpr Color value = Color::RGBA8888; };

}

namespace Haio::Implementation::Codecs {

bool Detect<Format::GIF, Color::RGBA8888>::operator()(Bytes data) const {
    return headerSays(data);
}

/**
 * GIF has a palette rather than an RGB colour type, but the decoder expands
 * it directly to RGB888.
 */
bool Detect<Format::GIF, Color::RGB888>::operator()(Bytes data) const {
    return headerSays(data);
}
}
