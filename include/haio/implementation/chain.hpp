#pragma once

// the registry first: it includes the generated codecs before anything asks them what
// they can do, since a concept checked before they are declared answers "no"
#include "haio/implementation/registry.hpp"
#include "haio/implementation/transforms/composite.hpp"
#include "haio/implementation/transforms/crop.hpp"
#include "haio/implementation/transforms/negate.hpp"
#include "haio/implementation/transforms/radius.hpp"
#include "haio/implementation/transforms/resize.hpp"

#include <haio/generated/transforms.hpp>

#include <concepts>
#include <exception>
#include <istream>
#include <iterator>
#include <ostream>
#include <type_traits>
#include <utility>
#include <vector>

/**
 * the verbs composed: one stage's answer handed to the next, checked when the chain
 * is built rather than when it runs. a stage is any member of a family, and chaining
 * two short circuits: the second never runs if the first failed, and the error rides
 * through untouched. what the first hands back is taken to what the second is handed,
 * so a Decode<> that found a jpeg in yuv420 reaches an encode of rgba8888 through
 * Convert<>.
 */
namespace Haio::Implementation::Codecs {

template <typename M> struct FirstArg;
template <typename R, typename C, typename A, typename... Rest>
struct FirstArg<R (C::*)(A, Rest...) const> {
    using type = std::remove_cvref_t<A>;
};

/** what a stage is handed, read off its call: one with a single operator() only */
template <typename T> using InputOf = typename FirstArg<decltype(&T::operator())>::type;

/** a stage called with what it was handed, and no settings when it only has the form that reads them */
template <typename S, typename In>
auto callStage(const S& stage, In&& in) {
    if constexpr (std::invocable<const S&, In>) return stage(std::forward<In>(in));
    else return stage(std::forward<In>(in), Settings{});
}

template <typename T> struct ColourOf;
template <Color P> struct ColourOf<Image<P>> {
    static constexpr Color value = P;
};

/** what one stage handed back, taken to what the next one is handed */
template <typename Want, typename Have>
Result<Want> adaptStage(Have have) {
    if constexpr (std::same_as<Want, Have>) {
        return have;
    } else if constexpr (std::same_as<Want, AnyImage>) {
        return AnyImage{ColourOf<Have>::value, have.width, have.height, std::move(have.data)};
    } else if constexpr (std::same_as<Have, AnyImage>) {
        HAIO_TRY(moved, Convert<>{}(std::move(have), ColourOf<Want>::value));
        return Want{moved.width, moved.height, std::move(moved.data)};
    } else {
        return adaptStage<Want>(AnyImage{ColourOf<Have>::value, have.width, have.height, std::move(have.data)});
    }
}

/** any member of a family, which is what may be chained */
template <typename T>
concept Stage = std::derived_from<T, Detect<>> || std::derived_from<T, Decode<>>
    || std::derived_from<T, Encode<>> || std::derived_from<T, Generate<>>
    || std::derived_from<T, Convert<>> || std::derived_from<T, Move<>>
    || std::derived_from<T, Transforms::Transform<>> || std::derived_from<T, Transforms::Blend<>>;

/** two stages, the second handed what the first answered */
template <typename First, typename Second>
class Chain {
public:
    constexpr Chain(First first, Second second) : first_(first), second_(second) {}

    auto operator()(const auto& in) const {
        using Want = InputOf<Second>;
        using Out = decltype(callStage(second_, std::declval<Want>()));

        auto middle = callStage(first_, in);
        if (!middle) return Out{std::unexpected(middle.error())};
        auto adapted = adaptStage<Want>(*std::move(middle));
        if (!adapted) return Out{std::unexpected(adapted.error())};
        return callStage(second_, *std::move(adapted));
    }

private:
    First first_;
    Second second_;
};

template <typename T> inline constexpr bool isChain = false;
template <typename A, typename B> inline constexpr bool isChain<Chain<A, B>> = true;

/** a stage, or stages already chained, which is what may be chained again */
template <typename T> concept Chainable = Stage<T> || isChain<T>;

template <typename A, typename B>
    requires Chainable<A> && Stage<B> && requires { typename InputOf<B>; }
constexpr Chain<A, B> operator|(const A& first, const B& second) {
    return {first, second};
}

/** the bytes of a stream handed to a chain, as a file of undecided format */
template <typename S>
    requires Chainable<S>
auto operator>>(std::istream& in, const S& stage) {
    using Out = decltype(callStage(stage, std::declval<const Blob&>()));
    if (!in) return Out{std::unexpected(Error{ErrorCode::Io, "could not read input"})};

    // a directory opens as a stream without complaint and only fails on the first
    // read, so the read itself has to be guarded and not just the open
    Blob blob{Format::RAW, Color::RGBA8888, {}, {}, {}};
    try {
        blob.data.assign(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
    } catch (const std::exception&) {
        return Out{std::unexpected(Error{ErrorCode::Io, "could not read input"})};
    }
    if (in.bad()) return Out{std::unexpected(Error{ErrorCode::Io, "failed while reading input"})};

    return callStage(stage, blob);
}

/** and a chain's file written out, with the Result surviving: input >> chain >> output */
inline Result<void> operator>>(const Result<Blob>& result,
                                               std::ostream& out) {
    if (!result) return std::unexpected(result.error());

    out.write(reinterpret_cast<const char*>(result->data.data()), static_cast<std::streamsize>(result->data.size()));
    if (!out) return std::unexpected(Error{ErrorCode::Io, "could not write output"});
    return {};
}

}

// argument dependent lookup looks where a stage's class lives, and a Result<Blob> is
// the implementation's: these name the | and the >> above there, and define nothing
namespace Haio::Implementation {

using Codecs::operator>>;

}

namespace Haio::Implementation::Transforms {

using Codecs::operator|;
using Codecs::operator>>;

}
