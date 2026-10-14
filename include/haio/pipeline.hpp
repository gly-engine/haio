#pragma once

#include "haio/implementation/chain.hpp"

/**
 * the verbs as objects, and the names they are specialised on, so a pipeline is
 * written short. a program can call this namespace Haio inside its own functions:
 *
 * @code{.cpp}
 * #include <haio/pipeline.hpp>
 *
 * #include <fstream>
 *
 * int main() {
 *     namespace Haio = ::Haio::Pipeline;
 *     const auto toPng = Haio::Decode<> | Haio::Encode<Haio::PNG, Haio::RGBA8888>;
 *
 *     auto input = std::ifstream("photo.jpg", std::ios::binary);
 *     auto output = std::ofstream("photo.png", std::ios::binary);
 *     const auto result = input >> toPng >> output;
 *     return result ? 0 : 1;
 * }
 * @endcode
 *
 * @note not at the global scope, where Haio is already the library's. inside that function
 * the rest of the library is still there as ::Haio, so a blob is ::Haio::Implementation::Blob.
 */
namespace Haio::Pipeline {

template <auto... P> inline constexpr Implementation::Codecs::Detect<P...> Detect;
template <auto... P> inline constexpr Implementation::Codecs::Decode<P...> Decode;
template <auto... P> inline constexpr Implementation::Codecs::Encode<P...> Encode;
template <auto... P> inline constexpr Implementation::Codecs::Generate<P...> Generate;
template <auto... P> inline constexpr Implementation::Codecs::Convert<P...> Convert;
template <auto... P> inline constexpr Implementation::Codecs::Move<P...> Move;
template <auto... P> inline constexpr Implementation::Transforms::Transform<P...> Transform;
template <auto... P> inline constexpr Implementation::Transforms::Blend<P...> Blend;

using enum Format;
using enum Color;

/**
 * ## blend
 * @code{.cpp}
 * const auto mixed = Haio::Blend<Haio::Multiply, Haio::RGBA8888, Haio::RGBA8888>(*image, *small, 0, 0);
 * @endcode
 */
using enum Implementation::Transforms::Compose;

/**
 * ## xc
 * @code{.cpp}
 * const auto canvas = Haio::Generate<Haio::Xc, Haio::RGBA8888>("tomato", {{"size", "64x64"}});
 * const auto png = Haio::Encode<Haio::PNG, Haio::RGBA8888>(*canvas, {});
 * @endcode
 *
 * ## null
 * @code{.cpp}
 * const auto clear = Haio::Generate<Haio::Null, Haio::RGBA8888>("", {{"size", "64x64"}});
 * @endcode
 *
 * ## gradient
 * @code{.cpp}
 * const auto canvas = Haio::Generate<Haio::Gradient, Haio::RGBA8888>(
 *     "red-blue", {{"size", "64x64"}, {"gradient:direction", "east"}});
 * @endcode
 *
 * ## radial-gradient
 * @code{.cpp}
 * const auto canvas = Haio::Generate<Haio::RadialGradient, Haio::RGBA8888>("white-black", {{"size", "64x64"}});
 * @endcode
 *
 * ## plasma
 * @code{.cpp}
 * const auto canvas = Haio::Generate<Haio::Plasma, Haio::RGBA8888>("", {{"size", "64x64"}, {"seed", "3"}});
 * @endcode
 *
 * ## hald
 * @code{.cpp}
 * const auto toPng = Haio::Generate<Haio::Hald, Haio::RGBA8888> | Haio::Encode<Haio::PNG, Haio::RGBA8888>;
 * const auto png = toPng("4");
 * @endcode
 *
 * ## code
 * @code{.cpp}
 * const auto toPng = Haio::Generate<Haio::Code, Haio::GRAYALPHA88> | Haio::Encode<Haio::PNG, Haio::RGBA8888>;
 * const auto png = toPng("12345");
 * @endcode
 *
 * ## qr
 * @code{.cpp}
 * const auto toPng = Haio::Generate<Haio::Qr, Haio::GRAYALPHA88> | Haio::Encode<Haio::PNG, Haio::RGBA8888>;
 * const auto png = toPng("http://pudim.com.br");
 * @endcode
 *
 * ## text
 * @code{.cpp}
 * const auto toPng = Haio::Generate<Haio::Text, Haio::GRAY8> | Haio::Encode<Haio::PNG, Haio::RGBA8888>;
 * const auto png = toPng("hi");
 * @endcode
 */
using enum Implementation::Codecs::Brush;

/**
 * ## crop
 * @code{.cpp}
 * const auto image = Haio::Decode<Haio::PNG, Haio::RGBA8888>(blob);
 * const auto part = Haio::Transform<Haio::Crop, Haio::RGBA8888>(*image, {0, 0, 32, 32});
 * @endcode
 *
 * ## resize
 * @code{.cpp}
 * const auto small = Haio::Transform<Haio::Resize, Haio::RGBA8888>(*image, {64, 64}, Haio::Point);
 * @endcode
 *
 * ## negate
 * @code{.cpp}
 * const auto negative = Haio::Transform<Haio::Negate, Haio::RGBA8888>(*image);
 * @endcode
 *
 * ## radius
 * @code{.cpp}
 * const auto rounded = Haio::Transform<Haio::Radius, Haio::RGBA8888>(*image, 8);
 * @endcode
 *
 * ## composite
 * @code{.cpp}
 * const auto laid = Haio::Transform<Haio::Composite, Haio::RGBA8888>(*image, *small, Haio::Over, Haio::Center, 0, 0);
 * @endcode
 */
using enum Implementation::Transforms::Operation;
using enum Implementation::Transforms::Transform<>::ResizeFilter;
using enum Implementation::Transforms::Transform<>::Gravity;

}
