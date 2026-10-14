#pragma once

#include <concepts>

namespace Haio::Util {

using namespace Implementation;

/**
 * a verb's specialisation that is one of its family: Detect<Format::PNG,
 * Color::RGBA8888> derives from Detect<>, and a pair nobody declared is the empty
 * primary, which does not. the family itself is not one of its members.
 */
template <typename T, typename Family>
concept MemberOf = !std::same_as<T, Family> && std::derived_from<T, Family>;

}
