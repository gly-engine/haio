#include <haio_cdn.hpp>
#include <haio_convert.hpp>

#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/signal_set.hpp>

#include <filesystem>
#include <iostream>

namespace {

std::string consumeValue(int& i, int argc, char* argv[], std::string_view arg, std::string_view name) {
    const std::string prefix = std::string(name) + "=";
    if (arg.starts_with(prefix)) return std::string(arg.substr(prefix.size()));
    if (i + 1 >= argc) throw std::runtime_error("missing value for " + std::string(name));
    return argv[++i];
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
              << "  haio convert <input> [filters] <output>\n"
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
        if (command == "convert") return Haio::Convert::runCli(argc - 1, argv + 1);
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
