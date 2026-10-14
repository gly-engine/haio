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
// usage: gen_scaffold <output.hpp> [--formats haio/domain.hpp]
//                     [--namespace Haio::Implementation::Codecs]
//                     [--include haio/implementation/codecs.hpp]... [source.cpp...]

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

/**
 * "Decode<Format::PNG, ...", "Transform<Operation::Crop, ...", "Generate<Brush::Xc, ..." or
 * "Blend<Compose::Multiply, ...":
 * a name, specialised on one of the enums. any name will do, so a new transform needs no line here.
 */
bool isEntryPoint(std::string_view named) {
    const auto open = named.find('<');
    if (open == 0 || open == std::string_view::npos) return false;
    if (!std::ranges::all_of(named.substr(0, open), [](char c) { return std::isalnum(static_cast<unsigned char>(c)) || c == '_'; })) {
        return false;
    }
    const auto rest = named.substr(open + 1);
    return rest.starts_with("Format::") || rest.starts_with("Color::") || rest.starts_with("Brush::")
        || rest.starts_with("Compose::") || rest.starts_with("Operation::");
}

/**
 * where the entry point starts, when the line is a definition of one: it starts at the
 * first column with its return type, whatever that is -- Result<Image<Color::X>>,
 * bool, or an alias such as BlendRes -- and the entry point is the first word after a
 * space outside any angle brackets.
 */
size_t entryOf(std::string_view line) {
    if (line.empty() || !std::isalpha(static_cast<unsigned char>(line[0]))) return std::string_view::npos;
    if (line.starts_with("template") || line.starts_with("using") || line.starts_with("namespace")) return std::string_view::npos;

    int depth = 0;
    for (size_t at = 0; at < line.size(); at++) {
        const char c = line[at];
        if (c == '<') depth++;
        else if (c == '>') depth--;
        else if (c == '(' && depth == 0) break;
        else if (c == ' ' && depth == 0 && isEntryPoint(line.substr(at + 1))) return at + 1;
    }
    return std::string_view::npos;
}

/**
 * "Result<Image> Decode<Format::PNG>(const Image& i) {" -> the same line as a declaration,
 * for the verbs that are still function templates.
 */
std::string declarationOf(std::string_view line) {
    if (line.starts_with("constexpr ")) line.remove_prefix(10);
    const auto entry = entryOf(line);
    if (entry == std::string_view::npos) return {};
    if (line.find("::operator()", entry) != std::string_view::npos) return {};

    const auto close = line.rfind(')');
    if (close == std::string_view::npos) return {};

    return "template <> " + std::string(line.substr(0, close + 1)) + ";";
}

/** a verb that is a class: one specialisation, and the calls it answers */
struct Shell {
    std::string name;
    std::vector<std::string> members;
};

/**
 * "bool Detect<Format::PNG, Color::RGBA8888>::operator()(Bytes data) const {" -> the
 * specialisation it belongs to, and that call as a member declaration. the class
 * itself is written here, since a concept can only see one that is complete: every
 * one of them derives from the verb with no arguments, Detect<>, which is what makes
 * it one of the family.
 */
bool memberOf(std::string_view line, std::vector<Shell>& shells) {
    const auto entry = entryOf(line);
    if (entry == std::string_view::npos) return false;
    const auto call = line.find("::operator()", entry);
    if (call == std::string_view::npos) return false;

    const auto close = line.rfind(')');
    if (close == std::string_view::npos || close < call) return false;
    const auto qualifiers = line.substr(close + 1, line.find('{', close) - close - 1);

    std::string member = std::string(line.substr(0, entry)) + std::string(line.substr(call + 2, close + 1 - call - 2));
    if (qualifiers.find("const") != std::string_view::npos) member += " const";
    member += ";";

    const std::string name{line.substr(entry, call - entry)};
    auto shell = std::ranges::find(shells, name, &Shell::name);
    if (shell == shells.end()) shell = shells.insert(shells.end(), Shell{name, {}});
    shell->members.push_back(std::move(member));
    return true;
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

/**
 * only rewrite when something actually changed, so a codec edit that adds no
 * capability does not force every translation unit to recompile
 */
bool writeIfChanged(const std::filesystem::path& output, const std::string& text) {
    std::ifstream existing(output);
    const std::string current{std::istreambuf_iterator<char>(existing), std::istreambuf_iterator<char>()};
    if (current == text) return true;

    if (output.has_parent_path()) {
        std::error_code ec;
        std::filesystem::create_directories(output.parent_path(), ec);
    }

    std::ofstream out(output);
    if (!out) {
        std::cerr << "gen_scaffold: cannot write " << output.string() << '\n';
        return false;
    }
    out << text;
    return static_cast<bool>(out);
}

}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "usage: gen_scaffold <output.hpp> [--formats haio/domain.hpp] [--namespace ns]"
                     " [--defaults ns] [--classes classes.hpp name] [--include header]... [source.cpp...]\n";
        return 1;
    }

    std::vector<std::string> sources;
    std::vector<std::string> includes;
    std::string formatsHeader;
    std::string space = "Haio::Implementation::Codecs";
    std::string defaultsSpace;
    std::string classesPath;
    std::string classesName;
    for (int i = 2; i < argc; i++) {
        const std::string_view arg{argv[i]};
        if (arg == "--classes" && i + 2 < argc) {
            classesPath = argv[++i];
            classesName = argv[++i];
        } else if (arg == "--defaults" && i + 1 < argc) {
            defaultsSpace = argv[++i];
        } else if (arg == "--formats" && i + 1 < argc) {
            formatsHeader = argv[++i];
        } else if (arg == "--namespace" && i + 1 < argc) {
            space = argv[++i];
        } else if (arg == "--include" && i + 1 < argc) {
            includes.emplace_back(argv[++i]);
        } else {
            sources.emplace_back(argv[i]);
        }
    }
    if (includes.empty()) includes.emplace_back("haio/implementation/codecs.hpp");

    std::ostringstream generated;
    generated << "#pragma once\n\n"
              << "// generated by scripts/gen_scaffold.cpp -- do not edit\n\n";
    for (const auto& header : includes) generated << "#include <" << header << ">\n";
    generated << "\n#include <string_view>\n";

    if (!formatsHeader.empty()) {
        const auto mimes = tableOf(formatsHeader, "@mime");
        const auto extensions = tableOf(formatsHeader, "@ext");
        if (mimes.empty() || extensions.empty()) return 1;

        generated << "\nnamespace Haio::Implementation::Trash {\n"
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

    // the verbs live apart from the vocabulary; see haio/implementation/codecs.hpp
    generated << "\nnamespace " << space << " {\n";

    std::vector<Shell> shells;
    std::vector<std::string> defaults;
    for (const auto& path : sources) {
        std::ifstream source(path);
        if (!source) {
            std::cerr << "gen_scaffold: cannot read " << path << '\n';
            return 1;
        }

        std::vector<std::string> declarations;
        for (std::string line; std::getline(source, line);) {
            // DefaultColor is a value rather than a call, so its definition is copied whole,
            // into the namespace it lives in when that is not the verbs'
            if (line.starts_with("template <> struct DefaultColor<")) {
                (defaultsSpace.empty() ? declarations : defaults).push_back(line);
                continue;
            }
            if (memberOf(line, shells)) continue;
            if (auto declaration = declarationOf(line); !declaration.empty()) {
                declarations.push_back(std::move(declaration));
            }
        }
        if (declarations.empty()) continue;

        generated << '\n';
        for (const auto& declaration : declarations) generated << declaration << '\n';
    }

    generated << "\n}\n";

    if (!defaults.empty()) {
        generated << "\nnamespace " << defaultsSpace << " {\n\n";
        for (const auto& line : defaults) generated << line << '\n';
        generated << "\n}\n";
    }

    // a class may take its calls from more than one source, so they are written after
    // every source is read, and apart: a source defines its operator() out of line, so
    // it has to see its class, and it cannot see the rest of this header, which copies
    // the DefaultColor that very source defines
    std::ostringstream classes;
    for (const auto& shell : shells) {
        const auto family = shell.name.substr(0, shell.name.find('<')) + "<>";
        classes << "\ntemplate <> class " << shell.name << " final : public " << family << " {\n"
                << "public:\n";
        for (const auto& member : shell.members) classes << "    " << member << '\n';
        classes << "};\n";
    }

    if (!classesPath.empty()) {
        std::ostringstream header;
        header << "#pragma once\n\n"
               << "// generated by scripts/gen_scaffold.cpp -- do not edit\n\n";
        for (const auto& named : includes) header << "#include <" << named << ">\n";
        header << "\nnamespace " << space << " {\n" << classes.str() << "\n}\n";
        if (!writeIfChanged(classesPath, header.str())) return 1;
        generated << "\n#include <" << classesName << ">\n";
    } else if (!shells.empty()) {
        generated << "\nnamespace " << space << " {\n" << classes.str() << "\n}\n";
    }

    return writeIfChanged(argv[1], generated.str()) ? 0 : 1;
}
