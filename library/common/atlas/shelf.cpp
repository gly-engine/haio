#include <haio_atlas.hpp>

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <numeric>
#include <string>
#include <tuple>

namespace Haio::Atlas {
namespace {

int roundUp(int value, int quantum) {
    return (value + quantum - 1) / quantum * quantum;
}

/** the pictures laid left to right at this width, wrapping when the row runs out */
Layout fill(std::span<const Size> sizes, std::span<const size_t> order, int width, const Spread& spread) {
    Layout out;
    out.rects.resize(sizes.size());

    int x = 0;
    int y = 0;
    int shelf = 0;
    int widest = 0;
    for (const auto at : order) {
        if (x + sizes[at].width > width && x > 0) {
            y += shelf + spread.gap.height;
            x = 0;
            shelf = 0;
        }
        out.rects[at] = Rect{x, y, sizes[at].width, sizes[at].height};
        const auto room = roundUp(sizes[at].width, spread.quantum.width);
        widest = std::max(widest, x + room);
        x += room + spread.gap.width;
        shelf = std::max(shelf, roundUp(sizes[at].height, spread.quantum.height));
    }

    out.canvas = Size{widest, y + shelf};
    return out;
}

std::optional<Error> checkSizes(std::span<const Size> sizes, const Spread& spread) {
    if (sizes.empty()) return Error{ErrorCode::InvalidInput, "nothing to pack"};
    for (const auto& size : sizes) {
        if (size.width <= 0 || size.height <= 0) return Error{ErrorCode::InvalidInput, "a picture with no pixels cannot be packed"};
    }
    if (spread.quantum.width <= 0 || spread.quantum.height <= 0 || spread.gap.width < 0 || spread.gap.height < 0) {
        return Error{ErrorCode::InvalidInput, "a grid cell is rounded to a positive multiple, with no negative gap"};
    }
    if (spread.quantum.width > largest || spread.quantum.height > largest || spread.gap.width > largest
        || spread.gap.height > largest) {
        return Error{ErrorCode::InvalidInput, "a grid cell or gap is past the " + std::to_string(largest) + " a side a layout may reach"};
    }
    return std::nullopt;
}

Result<Layout> bounded(Layout laid) {
    if (laid.canvas.width > largest || laid.canvas.height > largest) {
        return std::unexpected(Error{ErrorCode::InvalidInput,
                                     "a canvas of " + std::to_string(laid.canvas.width) + "x" + std::to_string(laid.canvas.height)
                                         + " is past the " + std::to_string(largest) + " a side a layout may reach"});
    }
    return laid;
}

}

Result<Layout> shelf(std::span<const Size> sizes, Spread spread) {
    if (auto failed = checkSizes(sizes, spread)) return std::unexpected(*failed);

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
        running += roundUp(sizes[at].width, spread.quantum.width);
        widths.push_back(running);
        running += spread.gap.width;
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
        auto laid = fill(sizes, order, width, spread);
        if (best.rects.empty() || score(laid) < score(best)) best = std::move(laid);
    }
    return bounded(std::move(best));
}

Result<Layout> shelf(std::span<const Size> sizes) {
    return shelf(sizes, Spread{{alignment, alignment}, {}});
}

Result<Layout> grid(std::span<const Size> sizes, Size cells, Spread spread) {
    if (auto failed = checkSizes(sizes, spread)) return std::unexpected(*failed);
    if (cells.width < 0 || cells.height < 0 || (cells.width == 0 && cells.height == 0)) {
        return std::unexpected(Error{ErrorCode::InvalidInput, "a grid needs its columns, its rows, or both"});
    }

    const auto count = static_cast<int>(sizes.size());
    const auto columns = cells.width > 0 ? cells.width : (count + cells.height - 1) / cells.height;
    const auto rows = (count + columns - 1) / columns;
    if (cells.height > 0 && rows > cells.height) {
        return std::unexpected(Error{ErrorCode::InvalidInput,
                                     std::to_string(count) + " pictures do not fit in a " + std::to_string(columns) + " by "
                                         + std::to_string(cells.height) + " grid"});
    }

    Size cell;
    for (const auto& size : sizes) {
        cell.width = std::max(cell.width, size.width);
        cell.height = std::max(cell.height, size.height);
    }
    cell = Size{roundUp(cell.width, spread.quantum.width), roundUp(cell.height, spread.quantum.height)};

    const auto across = static_cast<int64_t>(columns) * (cell.width + spread.gap.width) - spread.gap.width;
    const auto down = static_cast<int64_t>(rows) * (cell.height + spread.gap.height) - spread.gap.height;
    if (across > largest || down > largest) {
        return std::unexpected(Error{ErrorCode::InvalidInput,
                                     "a grid of " + std::to_string(across) + "x" + std::to_string(down)
                                         + " is past the " + std::to_string(largest) + " a side a layout may reach"});
    }

    Layout out;
    out.canvas = Size{static_cast<int>(across), static_cast<int>(down)};
    out.rects.reserve(sizes.size());
    for (int at = 0; at < count; at++) {
        out.rects.push_back(Rect{at % columns * (cell.width + spread.gap.width), at / columns * (cell.height + spread.gap.height),
                                 cell.width, cell.height});
    }
    return out;
}

}
