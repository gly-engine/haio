// Scans sources and declares whatever each one specialises.
//
// A declaration is all the concepts need in order to answer whether a format can be
// detected, decoded or encoded, or whether a colour can be cropped or resized; the
// bodies stay in their own translation unit and the linker joins them. A codec or a
// transform left out of the build is therefore never declared, so it loses its
// capability with no list for anyone to forget to update.
//
// The same pass reads the @mime tags off the Format enumerators, so the mime lookup
// is generated from the enum rather than kept as a second table that can drift from it.
//
// usage: gen_scaffold <output.hpp> [--formats haio_formats.hpp] [--namespace Haio::Codecs]
//                     [--include haio_codec.hpp]... [source.cpp...]

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

namespace {

constexpr std::string_view returnTypes[] = {"bool ", "Result<Blob> ", "Result<void> ", "Result<Image<"};

/**
 * "Decode<Format::PNG, ...", "Crop<Color::RGBA8888>(" or "Generate<Brush::Xc, ...":
 * a name, specialised on one of the enums. any name will do, so a new transform needs no line here.
 */
bool isEntryPoint(std::string_view named) {
    const auto open = named.find('<');
    if (open == 0 || open == std::string_view::npos) return false;
    if (!std::ranges::all_of(named.substr(0, open), [](char c) { return std::isalnum(static_cast<unsigned char>(c)) || c == '_'; })) {
        return false;
    }
    const auto rest = named.substr(open + 1);
    return rest.starts_with("Format::") || rest.starts_with("Color::") || rest.starts_with("Brush::");
}

/** "Result<Image> Decode<Format::PNG>(const Image& i) {" -> the same line as a declaration */
std::string declarationOf(std::string_view line) {
    if (line.starts_with("constexpr ")) line.remove_prefix(10);

    const auto* returns = std::ranges::find_if(returnTypes, [line](std::string_view t) { return line.starts_with(t); });
    if (returns == std::end(returnTypes)) return {};

    // Result<Image<Color::X>> puts the colour before the name, so the entry point is
    // wherever the "<" of the specialisation is rather than a fixed offset
    const auto named = *returns == "Result<Image<"
        ? std::string_view{line}.substr(line.find("> ") + 2)
        : line.substr(returns->size());
    if (!isEntryPoint(named)) return {};

    const auto close = line.rfind(')');
    if (close == std::string_view::npos) return {};

    return "template <> " + std::string(line.substr(0, close + 1)) + ";";
}

/** the tokens after a tag, up to the end of the comment */
void collectTagged(std::string_view line, std::string_view tag, std::vector<std::string>& into) {
    auto rest = line.substr(line.find(tag) + tag.size());
    if (const auto close = rest.find("*/"); close != std::string_view::npos) rest = rest.substr(0, close);

    while (!rest.empty()) {
        const auto start = rest.find_first_not_of(" \t");
        if (start == std::string_view::npos) break;
        rest.remove_prefix(start);
        const auto end = rest.find_first_of(" \t");
        into.emplace_back(rest.substr(0, end));
        if (end == std::string_view::npos) break;
        rest.remove_prefix(end);
    }
}

/** an enumerator is a bare name and a comma, which is all this file ever writes */
std::string enumeratorOf(std::string_view line) {
    const auto start = line.find_first_not_of(" \t");
    if (start == std::string_view::npos) return {};
    line.remove_prefix(start);

    const auto comma = line.find(',');
    if (comma == std::string_view::npos) return {};
    const auto name = line.substr(0, comma);
    if (name.empty() || name.find_first_of(" \t/*=(") != std::string_view::npos) return {};

    return std::string(name);
}

/** Format::PNG -> "image/png" or "png", straight off the enumerator that declares it */
std::string tableOf(const std::string& path, std::string_view tag) {
    std::ifstream header(path);
    if (!header) {
        std::cerr << "gen_scaffold: cannot read " << path << '\n';
        return {};
    }

    std::ostringstream table;
    std::vector<std::string> pending;
    bool inFormat = false;

    for (std::string line; std::getline(header, line);) {
        if (line.find("enum class Format") != std::string::npos) {
            inFormat = true;
            continue;
        }
        if (!inFormat) continue;
        if (line.find('}') != std::string::npos) break;

        if (line.find(tag) != std::string::npos) {
            collectTagged(line, tag, pending);
            continue;
        }
        if (const auto name = enumeratorOf(line); !name.empty()) {
            for (const auto& value : pending) {
                table << "    {Format::" << name << ", \"" << value << "\"},\n";
            }
            pending.clear();
        }
    }
    return table.str();
}

}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "usage: gen_scaffold <output.hpp> [--formats haio_formats.hpp] [--namespace ns]"
                     " [--include header]... [source.cpp...]\n";
        return 1;
    }

    std::vector<std::string> sources;
    std::vector<std::string> includes;
    std::string formatsHeader;
    std::string space = "Haio::Codecs";
    for (int i = 2; i < argc; i++) {
        const std::string_view arg{argv[i]};
        if (arg == "--formats" && i + 1 < argc) {
            formatsHeader = argv[++i];
        } else if (arg == "--namespace" && i + 1 < argc) {
            space = argv[++i];
        } else if (arg == "--include" && i + 1 < argc) {
            includes.emplace_back(argv[++i]);
        } else {
            sources.emplace_back(argv[i]);
        }
    }
    if (includes.empty()) includes.emplace_back("haio_codec.hpp");

    std::ostringstream generated;
    generated << "#pragma once\n\n"
              << "// generated by scripts/gen_scaffold.cpp -- do not edit\n\n";
    for (const auto& header : includes) generated << "#include <" << header << ">\n";
    generated << "\n#include <string_view>\n";

    if (!formatsHeader.empty()) {
        const auto mimes = tableOf(formatsHeader, "@mime");
        const auto extensions = tableOf(formatsHeader, "@ext");
        if (mimes.empty() || extensions.empty()) return 1;

        generated << "\nnamespace Haio {\n"
                  << "\n/** every mime type the Format enum names; the first of a format is the one haio sends */\n"
                  << "struct FormatMime {\n"
                  << "    Format format;\n"
                  << "    std::string_view mime;\n"
                  << "};\n\n"
                  << "inline constexpr FormatMime formatMimes[] = {\n"
                  << mimes
                  << "};\n"
                  << "\n/** every extension the Format enum names; the first of a format is the one haio writes */\n"
                  << "struct FormatExtension {\n"
                  << "    Format format;\n"
                  << "    std::string_view extension;\n"
                  << "};\n\n"
                  << "inline constexpr FormatExtension formatExtensions[] = {\n"
                  << extensions
                  << "};\n\n}\n";
    }

    // the verbs live apart from the vocabulary; see haio_codec.hpp
    generated << "\nnamespace " << space << " {\n";

    for (const auto& path : sources) {
        std::ifstream source(path);
        if (!source) {
            std::cerr << "gen_scaffold: cannot read " << path << '\n';
            return 1;
        }

        std::vector<std::string> declarations;
        for (std::string line; std::getline(source, line);) {
            // DefaultColor is a value rather than a call, so its definition is copied whole
            if (line.starts_with("template <> struct DefaultColor<")) {
                declarations.push_back(line);
                continue;
            }
            if (auto declaration = declarationOf(line); !declaration.empty()) {
                declarations.push_back(std::move(declaration));
            }
        }
        if (declarations.empty()) continue;

        generated << '\n';
        for (const auto& declaration : declarations) generated << declaration << '\n';
    }
    generated << "\n}\n";

    // only rewrite when something actually changed, so a codec edit that adds no
    // capability does not force every translation unit to recompile
    std::ifstream existing(argv[1]);
    const std::string current{std::istreambuf_iterator<char>(existing), std::istreambuf_iterator<char>()};
    if (current == generated.str()) return 0;

    const std::filesystem::path output = argv[1];
    if (output.has_parent_path()) {
        std::error_code ec;
        std::filesystem::create_directories(output.parent_path(), ec);
    }

    std::ofstream out(output);
    if (!out) {
        std::cerr << "gen_scaffold: cannot write " << output.string() << '\n';
        return 1;
    }
    out << generated.str();
    return out ? 0 : 1;
}
