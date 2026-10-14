#pragma once

/**
 * everything, for a caller that would rather not pick: the vocabulary in
 * include/haio/domain/, the verbs and how they chain in include/haio/pipeline.hpp, and
 * the commands the binary runs. a source inside haio includes what it uses instead.
 */

#include "haio/domain.hpp"
#include "haio/implementation/wip/dither.hpp"
#include "haio/implementation/error.hpp"
#include "haio/implementation/geometry.hpp"
#include "haio/implementation/image.hpp"
#include "haio/implementation/option.hpp"

// the registry includes the generated codecs before anything asks them what they can
// do, since a concept evaluated before they are declared caches the answer "no"
#include "haio/implementation/registry.hpp"
#include "haio/implementation/wip/palette.hpp"
#include "haio/pipeline.hpp"


namespace Haio {

int commandConvert(int argc, char* argv[]);
int commandCdn(int argc, char* argv[]);
int commandProbe(int argc, char* argv[]);

}
