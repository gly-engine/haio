#pragma once

#include <array>
#include <span>
#include <string_view>

/**
 * what a stage or a codec reads off a line, as data: the grammar prints it, the
 * usage explains it and the parser checks against it, and none of them needs to
 * link anything to do so.
 */
namespace Haio::Implementation {

/**
 * what an option's value has to look like, checked the moment the line is read.
 *
 * it is data rather than a function so the grammar and the usage can say it too:
 * "from 1 to 100", "one of point", "WxH". the few shapes that come up over and over
 * are here once; anything stranger is Any, and the stage that reads it says no.
 */
enum class OptionShape {
    Any,       /**< whatever the stage makes of it */
    Integer,   /**< a whole number from least to most */
    Size,      /**< WxH, both sides positive */
    Name,      /**< one of names, in any case */
};

/**
 * a setting written before the stage that uses it, as in "-filter bayer -palette cga".
 *
 * options are not known to the parser: any "-name value" that is not a stage is kept
 * until the next stage, and that stage says which ones it wants. whatever it does not
 * want is an error, because it was written in the belief that it would do something.
 */
struct Option {
    /** without the dash; the first is the one the grammar and the usage show */
    std::array<std::string_view, 2> spellings;

    /** the rule its value has to match, in the grammar */
    std::string_view takes;

    /** a stage refuses to run without it, as a qr code refuses without a -size */
    bool required = false;

    /** it may be written more than once, as -define is */
    bool repeats = false;

    std::string_view help;

    OptionShape shape = OptionShape::Any;

    /** Integer: the bounds, both included */
    int least = 0;
    int most = 0;

    /** Name: every name it answers to, from namesOf */
    std::span<const char* const> names = {};

    /** what it is when nobody says, as it would be written; empty when that is nothing */
    std::string_view fallback = {};

    /**
     * Name: what a name of this list is, for the message when one is not, which is
     * how imagemagick words it: "unrecognized image filter `cubicz'".
     */
    std::string_view called = {};

    constexpr std::string_view name() const { return spellings[0]; }
};

}
