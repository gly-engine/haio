#include <haio_convert.hpp>

#include <array>
#include <filesystem>
#include <fstream>
#include <iostream>

namespace Haio::Convert {
namespace {

constexpr std::array<uint8_t, 8> pngMagic = {0x89, 'P', 'N', 'G', '\r', '\n', 0x1a, '\n'};

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

void printError(const Error& error) {
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

Format detectMagic(const std::vector<uint8_t>& data) {
    if (data.size() >= pngMagic.size() && std::equal(pngMagic.begin(), pngMagic.end(), data.begin())) {
        return Format::PNG;
    }
    return Format::RAW;
}

Blob readInputBlob(Command& command) {
    Blob blob;
    blob.path = command.inputPath;
    blob.format = command.inputFormat;
    blob.contentType = contentTypeFor(blob.format);
    blob.data = command.inputPath == "-" ? readStream(std::cin) : readFile(command.inputPath);

    if (command.inputFormatName.empty()) {
        if (const auto magic = detectMagic(blob.data); magic != Format::RAW) {
            command.inputFormat = magic;
            blob.format = magic;
            blob.contentType = contentTypeFor(magic);
        }
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
        auto output = runPipeline(std::move(input), pipeline);
        writeOutput(command, output.data);
        return 0;
    } catch (const std::exception& err) {
        std::cerr << "[error] " << err.what() << '\n';
        return 1;
    }
}

} // namespace Haio::Convert
