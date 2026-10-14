#pragma once

#include "haio/implementation/error.hpp"
#include "haio/implementation/image.hpp"
#include "haio/implementation/language/token.hpp"

#include <chrono>
#include <optional>
#include <vector>

/** the tokens run: each one hands its picture to the next, and the last is a file */
namespace Haio::Implementation::Language {

/**
 * a deadline of none lets it run as long as it takes.
 *
 * it is checked between stages rather than inside them, so the bound is really "one
 * more stage after the clock runs out". that is enough for what it defends against: a
 * query naming a thousand operations is stopped after the first one or two, and a
 * single stage is already bounded by max_size.
 */
Result<Blob> runPipeline(Blob input, const Pipeline& pipeline,
                         std::optional<std::chrono::steady_clock::time_point> deadline = std::nullopt);

/**
 * the same, with a picture for every source: each Decode takes the next input and
 * puts it on a stack, transforms change the pictures of the parenthesis they are in,
 * and a merge such as Composite turns two into one. the encode wants exactly one
 * left, the way a line with one output does.
 */
Result<Blob> runPipeline(std::vector<Blob> inputs, const Pipeline& pipeline,
                         std::optional<std::chrono::steady_clock::time_point> deadline = std::nullopt);

}
