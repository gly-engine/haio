#include <haio_cli.hpp>

#include <filesystem>
#include <fstream>
#include <iostream>

namespace Haio::Cli {
namespace {

void printUsage() {
    std::cerr << "usage:\n"
              << "  haio convert input.png [filters] output.ppm\n"
              << "  haio convert png:- [filters] ppm:-\n"
              << "\nfilters:\n"
              << "  -crop [wxh+x+y]       crop using imagemagick-style geometry\n"
              << "  --crop x,y,w,h        crop using explicit rectangle\n"
              << "  --size wxh            resize image\n"
              << "  --resize wxh          resize image\n"
              << "  --radius r            round image corners\n"
              << "  --format fmt          override output format\n"
              << "  -fx expr              parse expression token, unsupported by backend for now\n";
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

std::vector<uint8_t> readFile(const std::filesystem::path& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) throw std::runtime_error("could not open input: " + path.string());
    return readStream(in);
}

Blob readInputBlob(Command& command) {
    Blob blob;
    blob.path = command.inputPath;
    blob.format = command.inputFormat;
    blob.contentType = contentTypeFor(blob.format);
    blob.data = command.inputPath == "-" ? readStream(std::cin) : readFile(command.inputPath);

    const auto found = Detect(blob.data);
    const bool explicitFormat = !command.inputFormatName.empty();

    if (found && found.format != command.inputFormat) {
        std::cerr << "warning: " << command.inputPath << " is " << formatName(found.format)
                  << ", not " << formatName(command.inputFormat);
        if (explicitFormat) {
            // the prefix was asked for on purpose, so it is honoured and only flagged
            std::cerr << "; decoding as " << formatName(command.inputFormat) << " anyway\n";
        } else {
            std::cerr << "; decoding as " << formatName(found.format) << '\n';
            command.inputFormat = found.format;
            blob.format = found.format;
            blob.color = found.color;
            blob.contentType = contentTypeFor(found.format);
        }
    } else if (found) {
        blob.color = found.color;
    }

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
