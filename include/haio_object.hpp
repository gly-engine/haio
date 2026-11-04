#pragma once

#include "haio_formats.hpp"

#include <cstddef>
#include <optional>
#include <string>
#include <vector>

namespace Haio {

struct Rect {
    int x = 0;
    int y = 0;
    int width = 0;
    int height = 0;
};

struct Size {
    int width = 0;
    int height = 0;
};

/** where one picture was laid on a bigger one, and what it was called before it was */
struct Cut {
    Rect rect;
    std::string name;
};

/**
 * what a picture knows about itself besides its pixels, carried beside it on the
 * stack for as long as the line runs and never written anywhere by itself.
 *
 * a container that can say these things picks them up at the end, and one that
 * cannot leaves them behind. there is one of each kind: writing one again replaces
 * it rather than piling a second beside it.
 */
struct Metadata {
    /** what the answer is called, from the file it came out of */
    std::optional<std::string> name;

    /** the pictures laid on this one, so a container that keeps an index can cut them back out */
    std::vector<Cut> cuts;

    /** the file it was read from, as it was before anything was done to it: what it was, its bytes, its size */
    std::optional<Format> magick;
    size_t bytes = 0;
    std::optional<Size> original;
};

}
