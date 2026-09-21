#include <haio.hpp>
#include <haio_codecs.hpp>

#include <iostream>
#include <string>

namespace {

int failures = 0;

void check(bool ok, const std::string& what) {
    if (ok) return;
    std::cerr << "fail: " << what << '\n';
    failures++;
}

}

auto main() -> int {
    // the table is generated from the @mime tags, so the two directions cannot drift:
    // whatever a format sends must be a spelling that resolves back to it
    HAIO_FOR_EACH_FORMAT(e) {
        constexpr Haio::Format format = std::meta::extract<Haio::Format>(e);
        const auto name = std::string(Haio::formatName(format));
        const auto mime = Haio::contentTypeFor(format);

        if (format == Haio::Format::RAW) {
            check(mime == "application/octet-stream", "raw has no mime type of its own");
            continue;
        }
        check(mime != "application/octet-stream", name + " names a mime type");
        check(Haio::formatFromContentType(mime) == format, name + " resolves back from what it sends");
    }

    // every alias resolves too, not just the one a format sends
    for (const auto& entry : Haio::formatMimes) {
        check(Haio::formatFromContentType(entry.mime) == entry.format,
              std::string(entry.mime) + " resolves to its format");
    }

    // the parameters and the casing a real header carries
    check(Haio::formatFromContentType("image/png; charset=binary") == Haio::Format::PNG,
          "a content type with parameters still resolves");
    check(Haio::formatFromContentType("  IMAGE/PNG  ") == Haio::Format::PNG,
          "a content type resolves whatever its casing and padding");
    check(Haio::formatFromContentType("application/octet-stream") == Haio::Format::RAW,
          "an unknown content type resolves to raw");

    // the alias exists because it is accepted but never sent
    check(Haio::contentTypeFor(Haio::Format::PPM) == "image/x-portable-pixmap",
          "the first tag is the one haio sends");
    check(Haio::formatFromContentType("image/x-portable-anymap") == Haio::Format::PPM,
          "a later tag is accepted");

    if (failures == 0) std::cout << "format mime: ok\n";
    return failures == 0 ? 0 : 1;
}
