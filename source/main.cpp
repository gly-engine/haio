#include <haio.hpp>

#include <exception>
#include <iostream>
#include <string_view>

#ifdef HAIO_USE_PROFILER
#include <gperftools/profiler.h>
#endif

namespace {

void printHelp() {
    std::cout << "usage:\n"
              << "  haio convert <input> [transforms] <output>   input may be an http url\n"
              << "                                            or a file inside an archive: foo.ipk/data.tar.gz/icon.png\n"
              << "  haio cdn [config.toml]        without a file, reads HAIO_CDN_TOML\n"
              << "  haio probe <file>...          report what the bytes actually are\n"
              << "\nconfig.toml:\n"
              << "  [cdn]\n"
              << "  host = \"0.0.0.0\"\n"
              << "  port = 8080\n"
              << "\n"
              << "  [cache]\n"
              << "  ttl = 3600                      seconds; leave [cache] out to keep nothing\n"
              << "  url = \"redis://127.0.0.1:6379\"   also file://dir; without it, in memory\n"
              << "\n"
              << "  [bucket.assets]\n"
              << "  url = \"file://assets\"           also https://host/prefix and s3://host/bucket\n";
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
        if (command == "convert") return Haio::commandConvert(argc - 1, argv + 1);
        if (command == "cdn") return Haio::commandCdn(argc - 1, argv + 1);
        if (command == "probe") return Haio::commandProbe(argc - 1, argv + 1);
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
