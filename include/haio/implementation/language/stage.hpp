#pragma once

#include "haio/implementation/option.hpp"
#include "haio/util/option.hpp"
#include "haio/util/reflection.hpp"

#include <array>
#include <expected>
#include <span>
#include <string>
#include <string_view>

/**
 * a stage is one thing the convert line can do: read a picture, change one, or write
 * one out. a transform's stage is declared in include/haio/implementation/language/stages/,
 * and nothing else has to list it; reading and writing belong to the codecs, which
 * declare what they take in include/haio/implementation/codecs/.
 *
 * the declaration is only data -- spellings, what it takes, which options it reads --
 * so the grammar can be printed from it without linking anything. turning the words
 * into a Token is build<&stage>, declared beside it and defined in its own source,
 * which only the parser ever instantiates.
 */
namespace Haio::Implementation::Language {

struct Token;

}

namespace Haio::Implementation::LanguageStage {

enum class Kind {
    Input,       /**< a file, a url, stdin, or a canvas a codec draws such as xc:white */
    Transform,   /**< "-resize 8x8": between the source and the output */
    Output,      /**< the last word, and the one stage there is always exactly one of */
};

struct Stage {
    Kind kind = Kind::Transform;

    /** without the dash; the first is the canonical one */
    std::array<std::string_view, 2> spellings;

    /** its name in the grammar */
    std::string_view rule;

    /** the rule its own value has to match; empty for one that takes none, as -composite */
    std::string_view takes;

    std::span<const Option> options = {};

    std::string_view help;

    /**
     * it joins the picture from a parenthesis into the one before it, as -composite
     * does. a parenthesis is only ever closed into one of these: imagemagick would
     * keep both pictures and write two, which haio does not do.
     */
    bool merges = false;

    constexpr std::string_view name() const { return spellings[0]; }

    constexpr const Option* option(std::string_view spelled) const {
        for (const auto& option : options) {
            for (const auto spelling : option.spellings) {
                if (!spelling.empty() && spelling == spelled) return &option;
            }
        }
        return nullptr;
    }
};

/** an option a stage took, in the spelling it arrived as */
struct Taken {
    const Option* option = nullptr;
    std::string spelling;
    std::string value;
};

/** the options handed to one stage, looked up by their canonical name */
struct Given {
    std::span<const Taken> taken;

    const Taken* find(std::string_view name) const {
        for (const auto& one : taken) {
            if (one.option->spellings[0] == name) return &one;
        }
        return nullptr;
    }
};

/** why a stage would not build, and the word that is to blame */
struct Refusal {
    std::string message;
    std::string token;
};

using Built = std::expected<Language::Token, Refusal>;

/**
 * the words of one stage, turned into what the pipeline runs.
 *
 * every stage declares its own specialisation beside its declaration; the primary is
 * deleted so that a stage somebody forgot to give one fails to compile in the parser
 * rather than at a user.
 */
template <const Stage* S>
Built build(std::string_view value, const Given& given) = delete;

}
