#include <haio_cdn.hpp>
#include <haio_cli.hpp>

#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/signal_set.hpp>

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>

#ifdef HAIO_USE_PROFILER
#include <gperftools/profiler.h>
#endif

namespace {

int cdnCommand(int argc, char* argv[]) {
    std::filesystem::path configPath;

    for (int i = 1; i < argc; i++) {
        const std::string_view arg = argv[i];
        if (arg.starts_with('-')) throw std::runtime_error("unknown cdn option: " + std::string(arg));
        if (!configPath.empty()) throw std::runtime_error("cdn takes a single config file");
        configPath = arg;
    }

    Haio::Cdn::Config config;
    if (!configPath.empty()) {
        config = Haio::Cdn::loadConfig(configPath);
    } else if (const char* inlineToml = std::getenv("HAIO_CDN_TOML")) {
        config = Haio::Cdn::parseConfig(inlineToml);
    } else {
        throw std::runtime_error("cdn needs a config file, or HAIO_CDN_TOML holding the config itself");
    }

    boost::asio::io_context io;
    boost::asio::signal_set signals(io, SIGINT, SIGTERM);
    signals.async_wait([&](auto, auto) { io.stop(); });
    boost::asio::co_spawn(io, Haio::Cdn::runServer(std::move(config)), boost::asio::detached);
    io.run();
    return 0;
}

int probeCommand(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "usage: haio probe <file>...\n";
        return 1;
    }

    int failures = 0;
    for (int i = 1; i < argc; i++) {
        const std::filesystem::path path = argv[i];
        std::ifstream in(path, std::ios::binary);
        if (!in) {
            std::cerr << path.string() << ": cannot open\n";
            failures++;
            continue;
        }

        const std::vector<uint8_t> data((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
        const auto found = Haio::Detect(data);

        std::cout << path.string() << ": ";
        if (!found) {
            std::cout << "unrecognised";
        } else {
            std::cout << Haio::formatName(found.format) << ' ' << Haio::colorName(found.color);
            const Haio::Blob blob{found.format, found.color, std::string(Haio::contentTypeFor(found.format)), path.string(), data};
            if (const auto image = Haio::Decode(blob); image) {
                std::cout << ' ' << image->width << 'x' << image->height;
            }
            // without a decoder the name is all we can report
        }
        std::cout << ", " << data.size() << " bytes";

        const auto fromName = Haio::formatFromExtension(path.string());
        if (fromName != Haio::Format::RAW && fromName != found.format) {
            std::cout << "  [extension says " << Haio::formatName(fromName) << ']';
            failures++;
        }
        std::cout << '\n';
    }

    return failures == 0 ? 0 : 1;
}

void printHelp() {
    std::cout << "usage:\n"
              << "  haio convert <input> [filters] <output>\n"
              << "  haio cdn [config.toml]        without a file, reads HAIO_CDN_TOML\n"
              << "  haio probe <file>...          report what the bytes actually are\n"
              << "\nconfig.toml:\n"
              << "  host = \"0.0.0.0\"\n"
              << "  port = 8080\n"
              << "\n"
              << "  cache = \"mem://?ttl=1h&max=5mb\"   also file://dir and redis://host\n"
              << "\n"
              << "  [bucket.assets]\n"
              << "  url = \"file://assets\"\n";
}

}

auto main(int argc, char* argv[]) -> int {
#ifdef HAIO_USE_PROFILER
    ProfilerStart("haio.prof");
#endif
    try {
        if (argc < 2) {
            printHelp();
            return 1;
        }

        const std::string_view command = argv[1];
        if (command == "convert") return Haio::Cli::runCli(argc - 1, argv + 1);
        if (command == "cdn") return cdnCommand(argc - 1, argv + 1);
        if (command == "probe") return probeCommand(argc - 1, argv + 1);
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
#ifdef HAIO_USE_PROFILER
    ProfilerStop();
#endif
}
