#include <haio/generated/codecs.hpp>

#include <haio/implementation/codecs.hpp>
#include <haio/util/reflection.hpp>

namespace Haio::Implementation::Codecs {

/**
 * every pair asked in turn, so "which payload is inside" needs no reader of its own.
 * the generated header is included first, since a concept checked before the pairs
 * are declared answers "no" for the rest of the build.
 */
Found Detect<>::operator()(Bytes data) const {
    Found found;
    HAIO_FOR_EACH_FORMAT(f) {
        constexpr Format format = std::meta::extract<Format>(f);
        HAIO_FOR_EACH_COLOR(c) {
            constexpr Color color = std::meta::extract<Color>(c);
            if constexpr (Detectable<format, color>) {
                if (!found && Detect<format, color>{}(data)) found = Found{format, color};
            }
        }
    }
    return found;
}

}
