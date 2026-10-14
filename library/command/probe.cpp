#include <haio/implementation/registry.hpp>
#include <haio/implementation/source.hpp>

#include <filesystem>
#include <iostream>
#include <string>

namespace Haio {

using namespace Implementation;

int commandProbe(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "usage: haio probe <file>...\n";
        return 1;
    }

    int failures = 0;
    for (int i = 1; i < argc; i++) {
        const std::filesystem::path path = argv[i];
        const auto read = Source::readFile(path);
        if (!read) {
            std::cerr << path.string() << ": cannot open\n";
            failures++;
            continue;
        }

        const auto& data = *read;
        const auto found = Implementation::Codecs::Detect<>{}(data);

        std::cout << path.string() << ": ";
        if (!found) {
            std::cout << "unrecognised";
        } else {
            std::cout << Implementation::formatName(found.format) << ' ' << Implementation::colorName(found.color);
            const Blob blob{found.format, found.color, std::string(Implementation::contentTypeFor(found.format)), path.string(), data};
            if (const auto image = Implementation::decode(blob); image) {
                std::cout << ' ' << image->width << 'x' << image->height;
            }
            // without a decoder the name is all we can report
        }
        std::cout << ", " << data.size() << " bytes";

        const auto fromName = Implementation::formatFromExtension(path.string());
        if (fromName != Format::RAW && fromName != found.format) {
            std::cout << "  [extension says " << Implementation::formatName(fromName) << ']';
            failures++;
        }
        std::cout << '\n';
    }

    return failures == 0 ? 0 : 1;
}

}
