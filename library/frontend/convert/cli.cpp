#include <haio_cli.hpp>
#include <haio/grammar.hpp>
#include <haio_platform.hpp>
#include <haio_source.hpp>
#include <haio_url.hpp>

#include <algorithm>

#include <fstream>
#include <iostream>
#include <memory>
#include <utility>
#include <vector>

namespace Haio::Cli {
namespace {

/**
 * the stages, off the same declarations the parser reads and the grammar documents,
 * each with the options it takes written right under it, and then every codec that
 * reads a setting of its own.
 */
void printUsage() {
    std::cerr << "usage:\n"
              << "  haio convert input.png [transforms] output.ppm\n"
              << "  haio convert png:- [transforms] ppm:-\n"
              << "  haio convert -size 64x64 xc:white [transforms] output.png\n"
              << "  haio convert base.png layer.png -gravity center -composite output.png\n"
              << "  haio convert xc:blue \\( -size 8x8 xc:red -radius 4 \\) -composite output.png\n"
              << "  haio convert https://host/input.png [transforms] output.ppm\n"
              << "  haio convert foo.ipk/data.tar.gz/assets/icon.png [transforms] output.ppm\n";

    std::vector<std::pair<std::string, std::string>> lines;
    const auto optionLine = [](const Stages::Option& option) {
        const auto colon = option.spellings[0].find(':') != std::string_view::npos;
        return (colon ? "  -define " + std::string(option.spellings[0]) + "=" : "  -" + std::string(option.spellings[0]) + " ")
             + std::string(option.takes) + (option.required ? " (required)" : "");
    };
    const auto withOptions = [&](std::string line, std::string_view help, std::span<const Stages::Option> options) {
        lines.emplace_back(std::move(line), std::string(help));
        for (const auto& option : options) lines.emplace_back(optionLine(option), Stages::describe(option));
    };

    lines.emplace_back("\ntransforms:", "");
    for (const auto* stage : Grammar::stages) {
        if (stage->kind != Stages::Kind::Transform) continue;
        auto spelled = "-" + std::string(stage->name());
        if (!stage->takes.empty()) spelled += " " + std::string(stage->takes);
        withOptions(std::move(spelled), stage->help, stage->options);
    }

    lines.emplace_back("\noutput:", "");
    withOptions(std::string(Grammar::output.takes), Grammar::output.help, Grammar::output.options);
    lines.emplace_back(optionLine(Grammar::define), Stages::describe(Grammar::define));

    lines.emplace_back("\ncodecs:", "");
    for (const auto& codec : Grammar::codecs) {
        if (!codec.reads->decode.empty()) withOptions(std::string(codec.format) + " input", "reading", codec.reads->decode);
        if (!codec.reads->encode.empty()) {
            withOptions(std::string(codec.format) + " output", "writing", codec.reads->encode);
        }
    }

    // the brushes, which paint a picture where a file would be read
    lines.emplace_back("\nbrushes:", "");
    for (const auto& brush : Grammar::brushes) {
        auto line = std::string(brush.name) + ":" + std::string(brush.draws->takes);
        withOptions(std::move(line), "painted, not read", brush.draws->options);
    }

    size_t width = 0;
    for (const auto& [line, help] : lines) {
        if (!help.empty()) width = std::max(width, line.size());
    }
    for (const auto& [line, help] : lines) {
        if (help.empty()) {
            std::cerr << line << '\n';
            continue;
        }
        std::cerr << "  " << line << std::string(width - line.size() + 2, ' ') << help << '\n';
    }

    std::cerr << "\nan option goes right before the stage that uses it, and takes its value after\n"
                 "a space or an =; generate_ebnf prints the whole grammar.\n";
}

/** "haio: unrecognized option `-wat'", the way imagemagick prints the same mistake */
void printError(std::string_view message) {
    std::cerr << "haio: " << message << '\n';
}

std::vector<uint8_t> readStream(std::istream& in) {
    return {std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
}

/**
 * a url is read the way the cdn reads a bucket, through Haio::Source. this is the one
 * place the command line waits on the platform's loop, and it waits for nothing else.
 */
Blob fetchInput(const std::string& uri) {
    auto fetched = Platform::blockOn(Source::fetchUri(uri));
    if (!fetched) throw std::runtime_error("unable to open image " + Stages::quoted(uri) + ": " + fetched.error().message);
    return *std::move(fetched);
}

/**
 * no allow switch here, unlike the cdn: the archive is the caller's own. the limit on
 * one extracted file is generous for the same reason, and the ratio a zip bomb needs
 * is refused either way.
 */
Source::ArchiveOptions archiveOptions() {
    return Source::ArchiveOptions{.maxEntry = size_t{1} << 30};
}

Blob unwrap(Result<Blob> found, const std::string& path) {
    if (!found) throw std::runtime_error("unable to open image " + Stages::quoted(path) + ": " + found.error().message);
    return *std::move(found);
}

/**
 * "https://host/foo.ipk/data.tar.gz/assets/icon.png" fetches foo.ipk, query and all,
 * and digs the rest out of it. the split is on the path the url spells, so an escaped
 * slash inside a name stays part of that name.
 */
Blob readRemote(const std::string& uri) {
    auto url = Url::parse(uri);
    const auto inside = url ? Source::splitArchivePath(url->path) : std::nullopt;
    if (!inside) return fetchInput(uri);

    url->path = inside->archive;
    auto archive = fetchInput(url->str());
    return unwrap(Source::readInsideArchive(std::move(archive.data), Percent::decode(inside->archive),
                                            Percent::decode(inside->inside), archiveOptions()),
                  uri);
}

/**
 * a file that is there is read as it is, so a directory that happens to be called
 * something.zip still works; only a path that names nothing on the disk is tried as
 * an archive and a name inside it.
 */
Blob readLocal(const std::string& path) {
    if (auto data = Source::readFile(path)) return Source::blobFrom(*std::move(data), path);

    if (const auto inside = Source::splitArchivePath(path)) {
        auto archive = Source::readFile(inside->archive);
        if (!archive) throw std::runtime_error("unable to open image " + Stages::quoted(inside->archive) + ": No such file or directory");
        return unwrap(Source::readInsideArchive(*std::move(archive), inside->archive, inside->inside, archiveOptions()), path);
    }
    throw std::runtime_error("unable to open image " + Stages::quoted(path) + ": No such file or directory");
}

Blob readInput(const std::string& path) {
    if (path == "-") return Source::blobFrom(readStream(std::cin), path);
    if (path.starts_with("s3://")) {
        // a bucket needs somewhere to write its region and its keys, and only the
        // cdn's config has one; a presigned https url reads fine from here
        throw std::runtime_error("unable to open image " + Stages::quoted(path)
                                 + ": s3 is read by the cdn only, use a presigned https url instead");
    }
    return Source::isRemoteUri(path) ? readRemote(path) : readLocal(path);
}

/**
 * the input, with its format settled. blobFrom already asked the bytes, then the
 * content type, then the name; what is left is a prefix written on purpose, which is
 * honoured, and a name that turned out wrong, which is worth a warning.
 */
Blob readInputBlob(Input& input) {
    auto blob = readInput(input.path);
    const auto named = input.format;
    // only what the bytes themselves say is worth contradicting a name over
    const auto found = Detect(blob.data);

    if (!input.formatName.empty()) {
        if (found && found.format != named) {
            std::cerr << "haio: " << Stages::quoted(input.path) << " is " << formatName(found.format)
                      << ", not " << formatName(named) << "; decoding as " << formatName(named) << " anyway\n";
            blob.color = Blob{}.color;
        }
        blob.format = named;
        blob.contentType = contentTypeFor(named);
        return blob;
    }

    if (found && named != Format::RAW && found.format != named) {
        std::cerr << "haio: " << Stages::quoted(input.path) << " is " << formatName(found.format)
                  << ", not " << formatName(named) << "; decoding as " << formatName(found.format) << '\n';
    }
    input.format = blob.format;
    return blob;
}

void writeFile(const std::string& path, const std::vector<uint8_t>& data) {
    std::ofstream out(path, std::ios::binary);
    if (!out) throw std::runtime_error("unable to open image " + Stages::quoted(path) + " for writing");
    out.write(reinterpret_cast<const char*>(data.data()), static_cast<std::streamsize>(data.size()));
    if (!out) throw std::runtime_error("unable to write image " + Stages::quoted(path));
}

void writeOutput(const Command& command, const std::vector<uint8_t>& data) {
    if (command.outputIsStdout) {
        std::cout.write(reinterpret_cast<const char*>(data.data()), static_cast<std::streamsize>(data.size()));
        if (!std::cout) throw std::runtime_error("unable to write image `-'");
        return;
    }
    writeFile(command.outputPath, data);
}

/** a source the shell left alone because it was quoted, which haio expands itself */
bool isPattern(const std::string& path) {
    return path != "-" && !Source::isRemoteUri(path) && path.find_first_of("*|") != std::string::npos;
}

std::vector<std::string> filesMatching(const std::string& pattern) {
    auto out = Platform::glob(pattern);
    if (out.empty()) throw std::runtime_error("no file matches " + Stages::quoted(pattern));
    return out;
}

/**
 * a line of text about a picture instead of the picture, for print:, which is how
 * "'*.png' print:'%t\t%G\n'" lists every file and its size for sort or awk.
 */
int print(const std::string& format, const Written& written, size_t index, size_t count) {
    const auto text = expandEscapes(format, written, index, count);
    if (!text) {
        printError(text.error().message);
        return 1;
    }
    std::cout << *text << std::flush;
    return 0;
}

/** the output, written once, with its escapes filled in from the one picture when it has any */
int once(Command& command, const std::optional<std::string>& printing) {
    // every source read before anything runs, so a missing file stops the line
    // before a single picture is decoded
    std::vector<Blob> inputs;
    for (auto& input : command.inputs) inputs.push_back(readInputBlob(input));
    const auto pipeline = buildPipeline(command);

    if (!printing && (command.outputIsStdout || !hasEscapes(command.outputPath))) {
        const auto output = runPipeline(std::move(inputs), pipeline);
        if (!output) {
            printError(output.error().message);
            return 1;
        }
        writeOutput(command, output->data);
        return 0;
    }

    const auto written = runPipelineWritten(std::move(inputs), pipeline);
    if (!written) {
        printError(written.error().message);
        return 1;
    }
    if (printing) return print(*printing, *written, 0, 1);

    const auto named = expandEscapes(command.outputPath, *written, 0, 1);
    if (!named) {
        printError(named.error().message);
        return 1;
    }
    writeFile(*named, written->blob.data);
    return 0;
}

/**
 * the whole line once for every file the pattern matches, the way mogrify runs, so a
 * transform reaches each of them and only one is ever held at a time. the other
 * sources are read once and handed to every run, so a logo laid over each is
 * fetched once. a file that fails stops the ones after it, and two that would land
 * on the same name stop the second before it overwrites the first.
 */
int eachFile(Command& command, size_t pattern, const std::optional<std::string>& printing) {
    const auto files = filesMatching(command.inputs[pattern].path);

    std::vector<Blob> fixed(command.inputs.size());
    for (size_t at = 0; at < command.inputs.size(); at++) {
        if (at != pattern) fixed[at] = readInputBlob(command.inputs[at]);
    }

    std::vector<std::string> names;
    for (size_t file = 0; file < files.size(); file++) {
        // built again for every file, since '**.png|jpeg' says what it reads only once it is a file
        auto line = command;
        auto& one = line.inputs[pattern];
        one.path = files[file];
        if (one.formatName.empty()) one.format = formatFromExtension(files[file]);
        const auto pipeline = buildPipeline(line);

        std::vector<Blob> inputs = fixed;
        inputs[pattern] = readInputBlob(one);

        const auto written = runPipelineWritten(std::move(inputs), pipeline);
        if (!written) {
            printError(Stages::quoted(files[file]) + ": " + written.error().message);
            return 1;
        }
        if (printing) {
            if (print(*printing, *written, file, files.size()) != 0) return 1;
            continue;
        }
        auto named = expandEscapes(command.outputPath, *written, file, files.size());
        if (!named) {
            printError(named.error().message);
            return 1;
        }
        if (std::ranges::contains(names, *named)) {
            printError(Stages::quoted(files[file]) + " would be written to " + Stages::quoted(*named) + ", which an earlier file already was");
            return 1;
        }
        writeFile(*named, written->blob.data);
        names.push_back(*std::move(named));
    }
    return 0;
}

}

int runCli(int argc, char* argv[]) {
    if (argc <= 2) {
        printUsage();
        return 1;
    }

    /**
     * print: is not a container but a line of text about every picture, so the line
     * is read as if it wrote the bare pixels to stdout, and those go unwritten.
     */
    std::vector<std::string> args(argv, argv + argc);
    std::optional<std::string> printing;
    if (args.back().starts_with("print:")) {
        printing = unescapeText(std::string_view{args.back()}.substr(6));
        args.back() = "raw:-";
    }

    auto command = Parser::parse(wordsOf(args));
    if (command.error) {
        printError(command.error.message);
        // the shell already spread an unquoted pattern into several sources
        if (namesEach(argv[argc - 1])) printError("quote the pattern, as '*.png', so haio runs the line once for each file");
        return 1;
    }

    try {
        std::vector<size_t> patterns;
        for (size_t at = 0; at < command.inputs.size(); at++) {
            if (isPattern(command.inputs[at].path)) patterns.push_back(at);
        }
        if (patterns.size() > 1) {
            printError("only one source may be a pattern");
            return 1;
        }
        if (patterns.empty()) return once(command, printing);

        // every file is a run of its own, so one name for all of them would only keep the last
        if (!printing && (command.outputIsStdout || !namesEach(command.outputPath))) {
            printError("a pattern runs the line once for each file, so the output has to be named after each, as %t.png");
            return 1;
        }
        return eachFile(command, patterns.front(), printing);
    } catch (const std::exception& err) {
        printError(err.what());
        return 1;
    }
}

} // namespace Haio::Cli
