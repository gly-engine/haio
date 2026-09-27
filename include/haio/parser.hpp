#pragma once

#include "haio.hpp"

#include <optional>
#include <span>
#include <string>
#include <vector>

/**
 * what a line of stages means, whoever wrote it.
 *
 * the command line and the cdn say the same things in different spellings: "-resize
 * 8x8" and "resize=8x8", "(" and "begin". each front end has a tokenizer of its own
 * that cuts its spelling into Words, and everything from there -- which word is a
 * stage and which an option, what a stage takes, the stack of pictures, the codecs'
 * settings -- is decided here, once, for all of them.
 */
namespace Haio::Parser {

/** one word, as a front end's tokenizer cut it; what it means is the parser's to say */
struct Word {
    enum class Kind {
        Plain,    /**< a source: a path, a url, "-", or a canvas such as xc:white */
        Named,    /**< a stage or an option, told apart by the name: "-resize", "--filter=bayer" */
        Open,     /**< a parenthesis: "(" on the command line, "begin" in a query */
        Close,    /**< and its end: ")" or "end" */
        Output,   /**< where the one picture left goes */
    };

    Kind kind = Kind::Plain;

    /** the whole word as it was written, which is also what it is as another's value */
    std::string text;

    /** Named: how it was spelled, without any attached value, for the messages */
    std::string spelling;

    /** Named: the name alone, without the dashes or whatever else the spelling wraps it in */
    std::string name;

    /** Named: a value that came attached to it, after an = or in the query */
    std::optional<std::string> value;
};

struct Error {
    std::string message;
    std::string token;

    explicit operator bool() const noexcept { return !message.empty(); }
};

/**
 * one source: a file, a url, stdin, or something a codec draws such as xc:white.
 * each one puts a picture on the stack, in the order they were written.
 */
struct Input {
    std::string path;
    std::string formatName;
    Format format = Format::RAW;

    /** drawn by its codec rather than read, and the path is what to draw */
    bool drawn = false;

    /** what its codec reads, as the options before it wrote them */
    Settings settings;
};

/**
 * what the words asked for.
 *
 * the steps are pipeline tokens already: a Decode for each input, in order, and each
 * transform, parenthesis and merge where it was written. each stage builds its own,
 * so there is no second kind of token here that has to be translated field by field.
 */
struct Parsed {
    Error error;

    std::vector<Input> inputs;
    std::vector<Token> steps;

    bool outputIsStdout = false;
    std::string outputPath;
    std::string outputFormatName;
    Format outputFormat = Format::RAW;
    Settings outputSettings;

    /** which colour to store inside the output, when -pix_fmt named one */
    std::optional<Color> outputColor;
    std::string outputColorName;
};

Parsed parse(std::span<const Word> words);

/**
 * the pipeline the words describe, ready for runPipeline with one blob per input.
 * it is refused when the words were, or when a format nobody recognised is still
 * standing where the pipeline needs a real one.
 */
std::expected<Pipeline, Error> build(const Parsed& parsed);

}
