#include <haio_cdn.hpp>

#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/signal_set.hpp>

#include <filesystem>
#include <fstream>
#include <iostream>

namespace {

std::vector<uint8_t> readFile(const std::filesystem::path& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) throw std::runtime_error("cannot open input: " + path.string());
    return {(std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>()};
}

void writeFile(const std::filesystem::path& path, const std::vector<uint8_t>& data) {
    std::ofstream out(path, std::ios::binary);
    if (!out) throw std::runtime_error("cannot open output: " + path.string());
    out.write(reinterpret_cast<const char*>(data.data()), static_cast<std::streamsize>(data.size()));
}

std::string consumeValue(int& i, int argc, char* argv[], std::string_view arg, std::string_view name) {
    const std::string prefix = std::string(name) + "=";
    if (arg.starts_with(prefix)) return std::string(arg.substr(prefix.size()));
    if (i + 1 >= argc) throw std::runtime_error("missing value for " + std::string(name));
    return argv[++i];
}

int convertCommand(int argc, char* argv[]) {
    if (argc < 3) {
        std::cerr << "usage: haio convert <input> <output> [--crop x,y,w,h] [--size wxh] [--radius r] [--format fmt]\n";
        return 1;
    }

    const std::filesystem::path inputPath = argv[1];
    const std::filesystem::path outputPath = argv[2];
    std::string query;
    auto appendQuery = [&](std::string key, std::string value) {
        if (!query.empty()) query += '&';
        query += std::move(key) + '=' + std::move(value);
    };

    bool explicitFormat = false;
    for (int i = 3; i < argc; i++) {
        const std::string_view arg = argv[i];
        if (arg == "--crop" || arg.starts_with("--crop=")) appendQuery("crop", consumeValue(i, argc, argv, arg, "--crop"));
        else if (arg == "--size" || arg.starts_with("--size=")) appendQuery("size", consumeValue(i, argc, argv, arg, "--size"));
        else if (arg == "--resize" || arg.starts_with("--resize=")) appendQuery("resize", consumeValue(i, argc, argv, arg, "--resize"));
        else if (arg == "--radius" || arg.starts_with("--radius=")) appendQuery("radius", consumeValue(i, argc, argv, arg, "--radius"));
        else if (arg == "--format" || arg.starts_with("--format=")) {
            appendQuery("format", consumeValue(i, argc, argv, arg, "--format"));
            explicitFormat = true;
        } else {
            throw std::runtime_error("unknown convert option: " + std::string(arg));
        }
    }

    Haio::Blob input;
    input.path = inputPath.string();
    input.format = Haio::formatFromExtension(input.path);
    input.contentType = Haio::contentTypeFor(input.format);
    input.data = readFile(inputPath);

    Haio::Pipeline pipeline;
    pipeline |= Haio::Tokens::Source("file", input.path);
    for (auto token : Haio::parseQueryTokens(query)) pipeline |= std::move(token);
    if (!explicitFormat) pipeline |= Haio::Tokens::Encode(Haio::formatFromExtension(outputPath.string()));

    const auto output = Haio::runPipeline(std::move(input), pipeline);
    writeFile(outputPath, output.data);
    return 0;
}

int cdnCommand(int argc, char* argv[]) {
    std::filesystem::path configPath;
    std::filesystem::path root = ".";
    auto config = Haio::Cdn::loadConfig({}, root);

    for (int i = 1; i < argc; i++) {
        const std::string_view arg = argv[i];
        if (arg == "--port" || arg.starts_with("--port=")) config.port = static_cast<unsigned short>(std::stoi(consumeValue(i, argc, argv, arg, "--port")));
        else if (arg == "--host" || arg.starts_with("--host=")) config.host = consumeValue(i, argc, argv, arg, "--host");
        else if (arg == "--config" || arg.starts_with("--config=")) configPath = consumeValue(i, argc, argv, arg, "--config");
        else if (arg == "--root" || arg.starts_with("--root=")) root = consumeValue(i, argc, argv, arg, "--root");
        else throw std::runtime_error("unknown cdn option: " + std::string(arg));
    }

    if (!configPath.empty()) {
        const auto host = config.host;
        const auto port = config.port;
        config = Haio::Cdn::loadConfig(configPath, root);
        config.host = host;
        config.port = port;
    } else {
        config.buckets["file"].root = root;
    }

    boost::asio::io_context io;
    boost::asio::signal_set signals(io, SIGINT, SIGTERM);
    signals.async_wait([&](auto, auto) { io.stop(); });
    boost::asio::co_spawn(io, Haio::Cdn::runServer(std::move(config)), boost::asio::detached);
    io.run();
    return 0;
}

void printHelp() {
    std::cout << "usage:\n"
              << "  haio convert <input> <output> [--crop x,y,w,h] [--size wxh] [--radius r] [--format fmt]\n"
              << "  haio cdn [--host 0.0.0.0] [--port 8080] [--root .] [--config haio.toml]\n";
}

}

auto main(int argc, char* argv[]) -> int {
    try {
        if (argc < 2) {
            printHelp();
            return 1;
        }

        const std::string_view command = argv[1];
        if (command == "convert") return convertCommand(argc - 1, argv + 1);
        if (command == "cdn") return cdnCommand(argc - 1, argv + 1);
        if (command == "help" || command == "--help" || command == "-h") {
            printHelp();
            return 0;
        }

        std::cerr << "unknown command: " << command << "\n";
        printHelp();
        return 1;
    } catch (const std::exception& err) {
        std::cerr << "error: " << err.what() << "\n";
        return 1;
    }
}
