#include <haio_atlas.hpp>

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <numeric>
#include <tuple>

namespace Haio::Atlas {
namespace {

int alignUp(int value) {
    return (value + alignment - 1) / alignment * alignment;
}

/** the pictures laid left to right at this width, wrapping when the row runs out */
Layout fill(std::span<const Size> sizes, std::span<const size_t> order, int width) {
    Layout out;
    out.rects.resize(sizes.size());

    int x = 0;
    int y = 0;
    int shelf = 0;
    int widest = 0;
    for (const auto at : order) {
        if (x + sizes[at].width > width && x > 0) {
            y += shelf;
            x = 0;
            shelf = 0;
        }
        out.rects[at] = Rect{x, y, sizes[at].width, sizes[at].height};
        x += alignUp(sizes[at].width);
        widest = std::max(widest, x);
        shelf = std::max(shelf, alignUp(sizes[at].height));
    }

    out.canvas = Size{widest, y + shelf};
    return out;
}

}

Result<Layout> shelf(std::span<const Size> sizes) {
    if (sizes.empty()) HAIO_FAIL(InvalidInput, "nothing to pack");
    for (const auto& size : sizes) {
        if (size.width <= 0 || size.height <= 0) HAIO_FAIL(InvalidInput, "a picture with no pixels cannot be packed");
    }

    // tallest first, so a low one never opens a shelf that a tall one then has to leave
    std::vector<size_t> order(sizes.size());
    std::iota(order.begin(), order.end(), 0u);
    std::ranges::stable_sort(order, [&](size_t a, size_t b) { return sizes[a].height > sizes[b].height; });

    /**
     * every width worth trying is a sum of the widths of some of the pictures, so the
     * running total as they are laid out is where a good one is found. guessing one
     * instead costs more than trying them all: there are as many as there are
     * pictures, and laying them out is cheap.
     */
    std::vector<int> widths;
    int running = 0;
    for (const auto at : order) {
        running += alignUp(sizes[at].width);
        widths.push_back(running);
    }

    /**
     * the shortest longest side, and the smallest canvas among those.
     *
     * squarer beats tighter on purpose. a strip one picture wide often wastes nothing
     * at all and is still the wrong answer: a container states a limit per side, so
     * the long way round reaches it while the same area laid square does not, and
     * whatever reads the atlas back walks it in two directions rather than one.
     */
    const auto score = [](const Layout& laid) {
        const auto w = static_cast<size_t>(laid.canvas.width);
        const auto h = static_cast<size_t>(laid.canvas.height);
        // and wider over taller when even those are equal, so what is read back grows
        // along the rows it is stored in rather than away from them
        return std::tuple{std::max(w, h), w * h, h};
    };

    Layout best;
    for (const auto width : widths) {
        auto laid = fill(sizes, order, width);
        if (best.rects.empty() || score(laid) < score(best)) best = std::move(laid);
    }
    return best;
}

}
