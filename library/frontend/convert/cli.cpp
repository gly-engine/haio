#include <haio_cli.hpp>
#include <haio_cli_grammar.hpp>
#include <haio_platform.hpp>
#include <haio_source.hpp>
#include <haio_url.hpp>

#include <algorithm>

#include <fstream>
#include <iostream>

namespace Haio::Cli {
namespace {

/**
 * the options, off the same table the parser reads and the grammar documents.
 *
 * it used to be a third copy of the list, which is why it was missing -filter, -limit
 * and -palete entirely and offered a --size the parser did not take.
 */
void printUsage() {
    std::cerr << "usage:\n"
              << "  haio convert input.png [filters] output.ppm\n"
              << "  haio convert png:- [filters] ppm:-\n"
              << "  haio convert https://host/input.png [filters] output.ppm\n"
              << "  haio convert foo.ipk/data.tar.gz/assets/icon.png [filters] output.ppm\n"
              << "\nfilters:\n";

    const auto spelled = [](const Lexer::Option& option) {
        auto out = std::string(option.spellings[0]);
        if (option.args == 0) return out;
        out += option.optional ? " [" + std::string(option.takes) + "]" : " " + std::string(option.takes);
        return out;
    };

    size_t width = 0;
    for (const auto& option : Lexer::options) {
        if (option.filter) width = std::max(width, spelled(option).size());
    }

    for (const auto& option : Lexer::options) {
        if (!option.filter) continue;
        const auto line = spelled(option);
        std::cerr << "  " << line << std::string(width - line.size() + 2, ' ') << option.help << '\n';
    }

    std::cerr << "\nevery option also takes its value after an =, and several answer to more than\n"
                 "one spelling; docs/convert-ebnf.md lists them all.\n";
}

void printError(const ParseError& error) {
    if (!error) return;
    std::cerr << "[error] " << error.message;
    if (!error.token.empty()) std::cerr << ": " << error.token;
    std::cerr << '\n';
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
    if (!fetched) throw std::runtime_error("could not fetch " + uri + ": " + fetched.error().message);
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
    if (!found) throw std::runtime_error(found.error().message + ": " + path);
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
        if (!archive) throw std::runtime_error("could not read input: " + inside->archive);
        return unwrap(Source::readInsideArchive(*std::move(archive), inside->archive, inside->inside, archiveOptions()), path);
    }
    throw std::runtime_error("could not read input: " + path);
}

Blob readInput(const std::string& path) {
    if (path == "-") return Source::blobFrom(readStream(std::cin), path);
    if (path.starts_with("s3://")) {
        // a bucket needs somewhere to write its region and its keys, and only the
        // cdn's config has one; a presigned https url reads fine from here
        throw std::runtime_error("s3 urls are read by the cdn only; use a presigned https url instead: " + path);
    }
    return Source::isRemoteUri(path) ? readRemote(path) : readLocal(path);
}

/**
 * the input, with its format settled. blobFrom already asked the bytes, then the
 * content type, then the name; what is left is a prefix written on purpose, which is
 * honoured, and a name that turned out wrong, which is worth a warning.
 */
Blob readInputBlob(Command& command) {
    auto blob = readInput(command.inputPath);
    const auto named = command.inputFormat;
    // only what the bytes themselves say is worth contradicting a name over
    const auto found = Detect(blob.data);

    if (!command.inputFormatName.empty()) {
        if (found && found.format != named) {
            std::cerr << "warning: " << command.inputPath << " is " << formatName(found.format)
                      << ", not " << formatName(named) << "; decoding as " << formatName(named) << " anyway\n";
            blob.color = Blob{}.color;
        }
        blob.format = named;
        blob.contentType = contentTypeFor(named);
        return blob;
    }

    if (found && named != Format::RAW && found.format != named) {
        std::cerr << "warning: " << command.inputPath << " is " << formatName(found.format)
                  << ", not " << formatName(named) << "; decoding as " << formatName(found.format) << '\n';
    }
    command.inputFormat = blob.format;
    return blob;
}

void writeOutput(const Command& command, const std::vector<uint8_t>& data) {
    if (command.outputIsStdout) {
        std::cout.write(reinterpret_cast<const char*>(data.data()), static_cast<std::streamsize>(data.size()));
        if (!std::cout) throw std::runtime_error("could not write output: stdout");
        return;
    }

    std::ofstream out(command.outputPath, std::ios::binary);
    if (!out) throw std::runtime_error("could not open output: " + command.outputPath);
    out.write(reinterpret_cast<const char*>(data.data()), static_cast<std::streamsize>(data.size()));
    if (!out) throw std::runtime_error("could not write output: " + command.outputPath);
}

}

int runCli(int argc, char* argv[]) {
    if (argc <= 2) {
        printUsage();
        return 1;
    }

    auto command = parseArgs(argc, argv);
    if (command.error) {
        printError(command.error);
        return 1;
    }

    try {
        if (command.hasGenerator) {
            (void)buildPipeline(command);
        }

        auto input = readInputBlob(command);
        const auto pipeline = buildPipeline(command);
        const auto output = runPipeline(std::move(input), pipeline);
        if (!output) {
            std::cerr << "[error] " << output.error().message << '\n';
            return 1;
        }

        writeOutput(command, output->data);
        return 0;
    } catch (const std::exception& err) {
        std::cerr << "[error] " << err.what() << '\n';
        return 1;
    }
}

} // namespace Haio::Cli
