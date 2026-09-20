#include <haio_cdn.hpp>
#include <haio_convert.hpp>

#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/signal_set.hpp>

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>

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
        const auto format = Haio::formatFromMagic(data);

        std::cout << path.string() << ": ";
        if (format == Haio::Format::RAW) {
            const auto foreign = Haio::describeForeignMagic(data);
            std::cout << (foreign.empty() ? "unrecognised" : foreign);
            if (!foreign.empty()) std::cout << " (haio cannot decode it)";
        } else {
            std::cout << Haio::formatName(format);
            try {
                const Haio::Blob blob{format, std::string(Haio::contentTypeFor(format)), path.string(), data};
                const auto image = Haio::decodeBlob(blob, format);
                std::cout << ' ' << image.width << 'x' << image.height;
            } catch (const std::exception&) {
                // no decoder for this one, the name is all we can report
            }
        }
        std::cout << ", " << data.size() << " bytes";

        Haio::Format fromName = Haio::Format::RAW;
        try {
            fromName = Haio::formatFromExtension(path.string());
        } catch (const std::exception&) {
        }
        if (fromName != Haio::Format::RAW && fromName != format) {
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
              << "  [bucket.assets]\n"
              << "  endpoint = \"file://assets\"\n";
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
}
