#pragma once

#include "haio_registry.hpp"

#include <fstream>
#include <iterator>
#include <utility>

/**
 * lazily composed conversions, checked when the chain is built rather than when it
 * runs. a stage is a callable returning Result<T>, and chaining two short circuits:
 * the second never runs if the first failed, and the error rides through untouched.
 *
 *     const auto pipeline = Decode | Convert<Color::ETC1> | Encode<Format::KTX2>;
 *     const auto result = pipeline(bytes);
 *
 * the names repeat those in Haio::Codecs on purpose: a stage is a value that
 * composes, not a function that runs, so Pipe::Encode<Format::KTX2> is a thing you
 * hold and Codecs::Encode<Format::KTX2, Color::ETC1> is a thing you call.
 */
namespace Haio::Pipe {

template <typename Fn>
struct Stage {
    Fn fn;

    constexpr auto operator()(auto&& in) const {
        return fn(std::forward<decltype(in)>(in));
    }
};

template <typename A, typename B>
constexpr auto operator|(Stage<A> first, Stage<B> second) {
    return Stage{[first, second](auto&& in) {
        auto middle = first(std::forward<decltype(in)>(in));
        using Out = decltype(second(*std::move(middle)));
        if (!middle) return Out{std::unexpected(middle.error())};
        return second(*std::move(middle));
    }};
}

namespace Detail {

inline Blob asBlob(const Blob& in) { return in; }

/** anything byte shaped enters the pipeline as a file of undecided format */
inline Blob asBlob(const auto& bytes) {
    return Blob{Format::RAW, Color::RGBA8888, {}, {}, {bytes.begin(), bytes.end()}};
}

}

/**
 * the bytes say what they are, nobody has to know beforehand.
 *
 * @todo the output is pinned to rgba8888 because a pipeline is typed and the input
 * is not. a jpeg is natively yuv, so asking for y4m through here would go through
 * rgb for nothing; Codecs::Decode<Format::JPEG, Color::YUV420> is the way around it.
 */
inline constexpr auto Decode = Stage{[](const auto& in) -> Result<Image<Color::RGBA8888>> {
    return Haio::Decode(Detail::asBlob(in));
}};

/** the target colour is named, the source is deduced from whatever arrives */
template <Color To>
inline constexpr auto Convert = Stage{[]<Color From>(Image<From> src) -> Result<Image<To>>
    requires Codecs::Convertible<From, To> {
    return Codecs::Convert<From, To>(std::move(src));
}};

/** the container is named, the colour comes from the image in hand */
template <Format F>
inline constexpr auto Encode = Stage{[]<Color P>(Image<P> src) -> Result<Blob>
    requires Codecs::Encodable<F, P> {
    return Codecs::Encode<F, P>(std::move(src));
}};

/** the result type follows whatever the chain ends in, image or blob */
template <typename Fn>
auto operator>>(std::istream& in, const Stage<Fn>& stage) {
    using Out = decltype(stage(std::declval<const std::vector<uint8_t>&>()));

    if (!in) return Out{std::unexpected(Error{ErrorCode::Io, "could not read input"})};

    // a directory opens as a stream without complaint and only fails on the first
    // read, so the read itself has to be guarded and not just the open
    std::vector<uint8_t> data;
    try {
        data.assign(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
    } catch (const std::exception&) {
        return Out{std::unexpected(Error{ErrorCode::Io, "could not read input"})};
    }
    if (in.bad()) return Out{std::unexpected(Error{ErrorCode::Io, "failed while reading input"})};

    return stage(data);
}

}

namespace Haio {

/** the sugar: input >> pipeline >> output, with the Result surviving the chain */
inline Result<void> operator>>(const Result<Blob>& result, std::ostream& out) {
    if (!result) return std::unexpected(result.error());

    out.write(reinterpret_cast<const char*>(result->data.data()),
              static_cast<std::streamsize>(result->data.size()));
    if (!out) return std::unexpected(Error{ErrorCode::Io, "could not write output"});
    return {};
}

}
