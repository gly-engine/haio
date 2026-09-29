#include <haio/grammar.hpp>
#include <haio/parser.hpp>
#include <haio_source.hpp>

#include <algorithm>
#include <cctype>
#include <stdexcept>
#include <vector>

namespace Haio::Parser {
namespace {

using Stages::Kind;
using Stages::Stage;

std::string lower(std::string_view value) {
    std::string out(value);
    std::ranges::transform(out, out.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return out;
}

std::optional<Format> knownFormat(std::string_view name) {
    const auto format = formatFromName(name);
    if (format == Format::RAW && !lower(name).starts_with("raw")) return std::nullopt;
    return format;
}

Format formatFromPathOrRaw(std::string_view path) {
    try {
        return formatFromExtension(path);
    } catch (const std::exception&) {
        return Format::RAW;
    }
}

/** "png:out.bin" or "xc:white": a prefix the build knows, and what follows it */
struct Prefixed {
    std::string prefix;
    std::string value;
};

std::optional<Prefixed> prefixOf(std::string_view word) {
    const auto colon = word.find(':');
    if (colon == std::string_view::npos || colon == 0) return std::nullopt;
    const auto prefix = word.substr(0, colon);
    if (prefix.find_first_of("/\\") != std::string_view::npos) return std::nullopt;
    return Prefixed{std::string(prefix), std::string(word.substr(colon + 1))};
}

/** an option read and not used yet, which the next stage may or may not want */
struct Pending {
    std::string spelling;
    std::string name;
    std::string value;
};

class Reader {
public:
    explicit Reader(std::span<const Word> words) : words_(words) {}

    Parsed run() {
        if (words_.size() <= 1) return fail("missing an image filename", words_.empty() ? "" : words_.front().text);

        for (at_ = 0; at_ < words_.size(); at_++) {
            if (!word(words_[at_])) return std::move(command_);
        }

        // the line is over and something is still waiting to be used, which it was
        // written in the belief that it would be
        if (!pending_.empty()) {
            unused(pending_.front(), "option is not followed by anything that takes it");
            return std::move(command_);
        }
        if (scopes_.size() > 1) return fail("unbalanced parenthesis", "(");
        if (sources_ == 0) return fail("no images defined");
        if (!hasOutput_) return fail("missing an image filename", words_.back().text);
        return std::move(command_);
    }

private:
    std::span<const Word> words_;
    size_t at_ = 0;
    Parsed command_;
    std::vector<Pending> pending_;
    bool hasOutput_ = false;

    /** every source so far, read or painted */
    size_t sources_ = 0;

    /**
     * how many pictures each open parenthesis holds, the line itself first.
     *
     * this is the stack the pipeline will keep, counted rather than held: every
     * source adds one, a merge turns two into one, and a ")" hands what it made to
     * the parenthesis around it.
     */
    std::vector<size_t> scopes_{0};

    /**
     * the reason and the word to blame, put together the way imagemagick puts them:
     * "unrecognized option `-wat'". the word is also kept on its own, for whoever
     * wants to point at it.
     */
    bool refuse(std::string reason, std::string token = {}) {
        if (!token.empty()) reason += " " + Stages::quoted(token);
        return refuseWhole(std::move(reason), std::move(token));
    }

    /** a message that already says it all, as the validators and the stages write them */
    bool refuseWhole(std::string message, std::string token) {
        command_.error = Error{std::move(message), std::move(token)};
        return false;
    }

    Parsed fail(std::string reason, std::string token = {}) {
        refuse(std::move(reason), std::move(token));
        return std::move(command_);
    }

    /**
     * whether anything at all answers to this name: a transform's option, the
     * output's, a codec's. it tells "unrecognized option", which is a typo, from an
     * option that exists and was written before the wrong stage.
     */
    static bool known(std::string_view name) {
        const auto in = [&](std::span<const Stages::Option> options) {
            return std::ranges::any_of(options, [&](const Stages::Option& o) {
                return std::ranges::find(o.spellings, name) != o.spellings.end();
            });
        };
        if (name == Grammar::define.name()) return true;
        if (in(Grammar::output.options)) return true;
        for (const auto* stage : Grammar::stages) {
            if (in(stage->options)) return true;
        }
        for (const auto& codec : Grammar::codecs) {
            if (in(codec.reads->decode) || in(codec.reads->encode)) return true;
        }
        for (const auto& brush : Grammar::brushes) {
            if (in(brush.draws->options)) return true;
        }
        return false;
    }

    /** a source or the output as a message names it: by its format, or as written when it has none */
    static std::string contextOf(Format format, std::string_view word) {
        return format == Format::RAW ? Stages::quoted(word) : std::string(formatName(format));
    }

    /** an option nobody took, worded by whether anybody could have */
    bool unused(const Pending& waiting, std::string_view reason) {
        if (!known(waiting.name)) return refuse("unrecognized option", waiting.spelling);
        return refuse(std::string(reason), waiting.spelling);
    }

    /** a named word is a stage when a transform answers to it, and an option when none does */
    bool word(const Word& word) {
        switch (word.kind) {
            case Word::Kind::Open: return open();
            case Word::Kind::Close: return close();
            case Word::Kind::Output: return output(word.text);
            case Word::Kind::Plain: return source(word.text);
            case Word::Kind::Named: break;
        }
        if (const auto* stage = Grammar::stageNamed(Kind::Transform, word.name)) return transform(*stage, word);
        return option(word);
    }

    /**
     * what came attached to it, or the next word along, whatever that word is: a
     * value may start with a dash, as -geometry -3+4 does, and a value that swallows
     * the output is the mistake the missing output will then report.
     */
    std::optional<std::string> valueOf(const Word& named) {
        if (named.value) return named.value;
        if (at_ + 1 >= words_.size()) return std::nullopt;
        return words_[++at_].text;
    }

    bool option(const Word& dashed) {
        if (dashed.name.empty()) return refuse("unrecognized option", dashed.spelling);

        auto value = valueOf(dashed);
        if (!value) return refuse("missing an argument", dashed.spelling);

        pending_.push_back(Pending{dashed.spelling, dashed.name, *std::move(value)});
        return true;
    }

    /**
     * a parenthesis: what comes inside it sees only the pictures made inside it, and
     * at the ")" they join the stack around it. options do not cross it either way,
     * since nothing on the other side could be what they were written for.
     */
    bool open() {
        constexpr size_t deepest = 8;
        if (!pending_.empty()) {
            return unused(pending_.front(), "option cannot cross a parenthesis");
        }
        if (scopes_.size() > deepest) {
            return refuse("parentheses nested more than " + std::to_string(deepest) + " deep", "(");
        }
        scopes_.push_back(0);
        command_.steps.push_back(Tokens::Open());
        return true;
    }

    bool close() {
        if (scopes_.size() == 1) return refuse("unbalanced parenthesis", ")");
        if (!pending_.empty()) {
            return unused(pending_.front(), "option cannot cross a parenthesis");
        }
        if (scopes_.back() == 0) return refuse("no images defined", ")");

        const auto made = scopes_.back();
        scopes_.pop_back();
        scopes_.back() += made;
        command_.steps.push_back(Tokens::Close());
        return true;
    }

    /**
     * the options waiting for this stage, handed over and cleared.
     *
     * one it does not know is the mistake, and so is a required one that is missing,
     * and so is one said twice that is not meant to be: either way the line does not
     * do what whoever wrote it thought it did.
     */
    /**
     * the context is what the stage is called in a message: "resize" for a
     * transform, the format for a source or the output -- "for png" says more than
     * "for out.png" about why -quality was not taken there.
     */
    std::optional<std::vector<Stages::Taken>> take(const Stage& stage, std::string_view context,
                                                   std::string_view spelled) {
        const auto forContext = " for " + std::string(context);
        std::vector<Stages::Taken> taken;
        for (auto& waiting : pending_) {
            const auto* option = stage.option(waiting.name);
            if (!option) {
                // the one thing imagemagick cannot say: which stage it was not an option of
                refuseWhole("unrecognized option " + Stages::quoted(waiting.spelling) + forContext, waiting.spelling);
                return std::nullopt;
            }
            const auto same = std::ranges::find(taken, option, &Stages::Taken::option);
            if (!option->repeats && same != taken.end()) {
                refuseWhole("option given twice " + Stages::quoted(waiting.spelling) + forContext, waiting.spelling);
                return std::nullopt;
            }
            // what the value has to look like is declared with the option, so it is
            // checked here, before anything has been read or decoded
            if (const auto why = Stages::refusal(*option, waiting.value)) {
                refuseWhole(*why, waiting.value);
                return std::nullopt;
            }
            taken.push_back(Stages::Taken{option, std::move(waiting.spelling), std::move(waiting.value)});
        }
        pending_.clear();

        for (const auto& option : stage.options) {
            if (!option.required || std::ranges::find(taken, &option, &Stages::Taken::option) != taken.end()) continue;
            refuseWhole("missing required option " + Stages::quoted("-" + std::string(option.name())) + forContext,
                        std::string(spelled));
            return std::nullopt;
        }
        return taken;
    }

    /** the stage's own build, found by the same reflection that found the stage */
    static Stages::Built build(const Stage& stage, std::string_view value, const Stages::Given& given) {
        Stages::Built out = std::unexpected(Stages::Refusal{"this stage cannot be built", std::string(stage.name())});
        template for (constexpr auto member : Grammar::Detail::stageMembers()) {
            if (&stage == &[:member:]) out = Stages::build<&[:member:]>(value, given);
        }
        return out;
    }

    /**
     * a transform changes the picture on top of the stack, and a merge wants a picture
     * and one to three over it in its parenthesis, and leaves one.
     */
    bool transform(const Stage& stage, const Word& dashed) {
        // a stage that takes no value, as -composite, reads no word after it
        std::optional<std::string> value = std::string{};
        if (!stage.takes.empty()) value = valueOf(dashed);
        if (!value) return refuse("missing an argument", dashed.spelling);

        auto& pictures = scopes_.back();
        if (pictures == 0) return refuse("no images defined", dashed.spelling);
        if (stage.merges && pictures == 1) return refuse("image sequence is required", dashed.spelling);
        if (stage.merges && pictures > 4) return refuse("image sequence is too long", dashed.spelling);

        const auto taken = take(stage, stage.name(), dashed.spelling);
        if (!taken) return false;

        constexpr size_t maxSteps = 64;
        if (command_.steps.size() >= maxSteps) return refuse("too many operations", dashed.spelling);

        auto token = build(stage, *value, Stages::Given{*taken});
        if (!token) return refuseWhole(std::move(token.error().message), std::move(token.error().token));
        command_.steps.push_back(*std::move(token));
        if (stage.merges) pictures = 1;
        return true;
    }

    /** the option waiting under this name, before any stage has taken it */
    const Pending* waiting(const Stages::Option& option) const {
        for (const auto& one : pending_) {
            if (std::ranges::find(option.spellings, std::string_view{one.name}) != option.spellings.end()) return &one;
        }
        return nullptr;
    }

    /**
     * the input or the output, whose options are its own plus whatever the codec of
     * that format declared. that is only known once the format is, so the stage is
     * put together here rather than declared.
     */
    struct Codec {
        std::vector<Stages::Option> options;
        Stage stage;
    };

    static Codec codecStage(const Stage& base, std::span<const Stages::Option> reads) {
        Codec out{{base.options.begin(), base.options.end()}, base};
        out.options.push_back(Grammar::define);
        for (const auto& option : reads) {
            // a name with a colon is png's own, and reached through -define alone
            if (option.spellings[0].find(':') == std::string_view::npos) out.options.push_back(option);
        }
        out.stage.options = out.options;
        return out;
    }

    /**
     * what the codec is handed: the options it declared, by their own names, and each
     * -define whose key it declared too. a -define it does not know is refused, where
     * imagemagick would ignore it, because it was written to change something.
     */
    std::optional<Settings> settingsOf(const std::vector<Stages::Taken>& taken, std::span<const Stages::Option> reads,
                                       std::string_view context) {
        Settings out;
        for (const auto& one : taken) {
            if (one.option->spellings[0] != Grammar::define.spellings[0]) {
                if (std::ranges::find(reads, one.option->spellings[0],
                                      [](const Stages::Option& o) { return o.spellings[0]; }) != reads.end()) {
                    out.push_back(Setting{std::string(one.option->spellings[0]), one.value});
                }
                continue;
            }

            const auto equals = one.value.find('=');
            const auto key = std::string_view{one.value}.substr(0, equals);
            const auto declared = std::ranges::find_if(reads, [&](const Stages::Option& o) {
                return o.spellings[0].find(':') != std::string_view::npos && o.spellings[0] == key;
            });
            if (declared == reads.end()) {
                refuseWhole("unrecognized define " + Stages::quoted(one.value) + " for " + std::string(context),
                            one.value);
                return std::nullopt;
            }
            if (equals == std::string::npos) {
                refuseWhole("invalid argument for option `-define': " + one.value, one.value);
                return std::nullopt;
            }
            // each key once: -define may repeat, but one key said twice would leave the
            // codec to pick which of the two was meant
            if (settingNamed(out, key)) {
                refuseWhole("define given twice " + Stages::quoted(one.value) + " for " + std::string(context),
                            one.value);
                return std::nullopt;
            }
            const auto value = one.value.substr(equals + 1);
            if (const auto why = Stages::refusal(*declared, value)) {
                // a name out of a list says which list, as it does for any other option
                const bool listed = declared->shape == Stages::Shape::Name && !declared->called.empty();
                refuseWhole(listed ? *why : "invalid argument for option `-define': " + one.value, one.value);
                return std::nullopt;
            }
            out.push_back(Setting{std::string(key), value});
        }
        return out;
    }

    /**
     * a source, which puts one picture on the stack.
     *
     * a prefix that names a brush is painted rather than read -- "xc:white",
     * "gradient:red-blue" -- and takes that brush's options. anything else is a file:
     * a path on this machine, an http url, or "-", maybe with a format written in
     * front of it.
     */
    bool source(std::string_view word) {
        const auto prefixed = prefixOf(word);
        if (prefixed) {
            if (const auto brush = brushNamed(prefixed->prefix)) return paint(*brush, prefixed->value, word);
        }

        Input input;
        input.path = std::string(word);
        if (prefixed && knownFormat(prefixed->prefix)) {
            if (prefixed->value.empty()) return refuse("missing an image filename", std::string(word));
            input.path = prefixed->value;
            input.formatName = prefixed->prefix;
            input.format = *knownFormat(prefixed->prefix);
        }
        if (input.formatName.empty()) input.format = formatFromPathOrRaw(input.path);

        const auto& reads = readsOf(input.format);
        const auto codec = codecStage(Grammar::input, reads.decode);
        const auto taken = take(codec.stage, contextOf(input.format, word), word);
        if (!taken) return false;

        auto settings = settingsOf(*taken, reads.decode, contextOf(input.format, word));
        if (!settings) return false;
        input.settings = *std::move(settings);

        command_.steps.push_back(Tokens::Source(Source::isRemoteUri(input.path) ? "url" : "file", input.path));
        command_.steps.push_back(Tokens::Decode(input.format, input.settings));
        command_.inputs.push_back(std::move(input));
        scopes_.back()++;
        sources_++;
        return true;
    }

    /** a picture a brush paints, which is a source like any other and reads no input */
    bool paint(Brush brush, std::string_view words, std::string_view word) {
        const auto options = Grammar::optionsOf(brush);
        const auto context = std::string(brushName(brush));
        const auto codec = codecStage(Grammar::input, options);
        const auto taken = take(codec.stage, context, word);
        if (!taken) return false;

        auto settings = settingsOf(*taken, options, context);
        if (!settings) return false;

        command_.steps.push_back(Tokens::Generate(brush, std::string(words), *std::move(settings)));
        scopes_.back()++;
        sources_++;
        return true;
    }

    /**
     * the last word, and the options written right before it.
     *
     * a prefix on the path wins over -format, and -format over the extension: the
     * closer to the path it is written, the more on purpose it was. the format has to
     * be settled before the options are taken, because which ones there are depends
     * on it.
     */
    bool output(std::string_view word) {
        if (scopes_.size() > 1) return refuse("unbalanced parenthesis", std::string(word));
        if (scopes_.back() == 0) return refuse("no images defined", std::string(word));
        // imagemagick would write one file per picture; haio writes one, so the rest
        // have to have been joined into it
        if (scopes_.back() > 1) {
            return refuse(std::to_string(scopes_.back()) + " images and one output, join them with -composite",
                          std::string(word));
        }
        hasOutput_ = true;

        command_.outputPath = std::string(word);
        command_.outputFormat = formatFromPathOrRaw(word);

        if (const auto* format = waiting(Grammar::outputFormat)) {
            const auto known = knownFormat(format->value);
            if (!known) return refuse("no encode delegate for this image format", format->value);
            command_.outputFormat = *known;
            command_.outputFormatName = format->value;
        }

        if (const auto prefixed = prefixOf(word); prefixed && knownFormat(prefixed->prefix)) {
            if (prefixed->value.empty()) return refuse("missing an image filename", std::string(word));
            command_.outputPath = prefixed->value;
            command_.outputFormatName = prefixed->prefix;
            command_.outputFormat = *knownFormat(prefixed->prefix);
        }

        const auto& reads = readsOf(command_.outputFormat);
        const auto codec = codecStage(Grammar::output, reads.encode);
        const auto taken = take(codec.stage, contextOf(command_.outputFormat, word), word);
        if (!taken) return false;

        /**
         * the colour stored inside the output, which is not the same question as the
         * container: a tga holds any of six and a ktx2 any of three. a -pix_fmt written
         * last is the one, the way it is for ffmpeg.
         */
        if (!command_.steps.empty() && command_.steps.back().kind == TokenKind::PixFmt) {
            command_.outputColor = command_.steps.back().color;
        }

        auto settings = settingsOf(*taken, reads.encode, contextOf(command_.outputFormat, word));
        if (!settings) return false;
        command_.outputSettings = *std::move(settings);

        command_.outputIsStdout = command_.outputPath == "-";
        return true;
    }
};

} // namespace

Parsed parse(std::span<const Word> words) {
    return Reader(words).run();
}

} // namespace Haio::Parser
