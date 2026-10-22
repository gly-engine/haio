// Runs the examples a comment asks for, so the docs show what haio really draws.
//
// Doxygen hands every source to this filter and reads back what it prints. A line
// holding "@haio{convert} a.png b.png -composite png:-" has ./build/bin/haio convert
// run with those words, its output saved as build/docs/assets/<sha1 of the command>.png,
// and the tag replaced by an inline image of it, which IMAGE_PATH finds there. Tags
// on one line, or on lines one under the other, are pictures side by side.
//
// The line count never changes, so doxygen's warnings still point at the right line.
// A picture already there and newer than the haio that drew it is kept.
//
// Doxygen looks through IMAGE_PATH before it filters anything, so a picture drawn by
// the filter is one it has already missed. Built as doxygen_images the same code only
// draws, every tag under the directories it is given, and runs before doxygen does.
//
// usage: doxygen_filter <source>          (INPUT_FILTER in the Doxyfile)
//        doxygen_images [directory...]    (include and library when none is given)

#include <spawn.h>
#include <sys/wait.h>
#include <fcntl.h>

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

extern char** environ;

namespace {

constexpr std::string_view tag = "@haio{";
constexpr std::string_view haio = "./build/bin/haio";
constexpr std::string_view assets = "build/docs/assets";

std::string sha1(std::string_view text) {
    const auto rotl = [](uint32_t v, int n) { return (v << n) | (v >> (32 - n)); };
    std::array<uint32_t, 5> h{0x67452301, 0xEFCDAB89, 0x98BADCFE, 0x10325476, 0xC3D2E1F0};

    std::string message(text);
    const uint64_t bits = static_cast<uint64_t>(message.size()) * 8;
    message += static_cast<char>(0x80);
    while (message.size() % 64 != 56) message += '\0';
    for (int i = 7; i >= 0; i--) message += static_cast<char>(bits >> (i * 8));

    for (size_t chunk = 0; chunk < message.size(); chunk += 64) {
        std::array<uint32_t, 80> w{};
        for (int i = 0; i < 16; i++) {
            for (int b = 0; b < 4; b++) w[i] = (w[i] << 8) | static_cast<uint8_t>(message[chunk + i * 4 + b]);
        }
        for (int i = 16; i < 80; i++) w[i] = rotl(w[i - 3] ^ w[i - 8] ^ w[i - 14] ^ w[i - 16], 1);

        auto [a, b, c, d, e] = h;
        for (int i = 0; i < 80; i++) {
            const auto [f, k] = i < 20 ? std::pair{(b & c) | (~b & d), 0x5A827999u}
                              : i < 40 ? std::pair{b ^ c ^ d, 0x6ED9EBA1u}
                              : i < 60 ? std::pair{(b & c) | (b & d) | (c & d), 0x8F1BBCDCu}
                                       : std::pair{b ^ c ^ d, 0xCA62C1D6u};
            const uint32_t next = rotl(a, 5) + f + e + k + w[i];
            e = d;
            d = c;
            c = rotl(b, 30);
            b = a;
            a = next;
        }
        h[0] += a;
        h[1] += b;
        h[2] += c;
        h[3] += d;
        h[4] += e;
    }

    std::string out;
    char hex[9];
    for (const auto word : h) {
        std::snprintf(hex, sizeof(hex), "%08x", word);
        out += hex;
    }
    return out;
}

/** the words of a command, split on spaces, with '...' and "..." kept whole */
std::vector<std::string> wordsOf(std::string_view command) {
    std::vector<std::string> words;
    std::string word;
    bool inWord = false;
    char quote = 0;
    for (const char c : command) {
        if (quote) {
            if (c == quote) quote = 0;
            else word += c;
        } else if (c == '\'' || c == '"') {
            quote = c;
            inWord = true;
        } else if (c == ' ' || c == '\t') {
            if (inWord) words.push_back(std::move(word));
            word.clear();
            inWord = false;
        } else {
            word += c;
            inWord = true;
        }
    }
    if (inWord) words.push_back(std::move(word));
    return words;
}

/** the command run straight, not through a shell, with what it prints going into the file */
bool run(const std::vector<std::string>& words, const std::filesystem::path& into) {
    std::vector<char*> argv;
    for (const auto& word : words) argv.push_back(const_cast<char*>(word.c_str()));
    argv.push_back(nullptr);

    posix_spawn_file_actions_t actions;
    posix_spawn_file_actions_init(&actions);
    // doxygen reads the filter's stdout as the source, so the child's goes to the file; its stderr is doxygen's
    posix_spawn_file_actions_addopen(&actions, 1, into.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);

    pid_t child = 0;
    const int failed = posix_spawn(&child, argv[0], &actions, nullptr, argv.data(), environ);
    posix_spawn_file_actions_destroy(&actions);
    if (failed != 0) return false;

    int status = 0;
    waitpid(child, &status, 0);
    return WIFEXITED(status) && WEXITSTATUS(status) == 0;
}

/** the picture a command draws, drawn again only when the haio that drew it has changed since */
std::optional<std::string> pictureOf(std::string_view command, const std::string& source, size_t line) {
    const auto words = wordsOf(command);
    const auto name = sha1(command) + ".png";
    const auto path = std::filesystem::path(assets) / name;
    std::error_code ec;
    std::filesystem::create_directories(assets, ec);

    // drawn already, and by this haio or one that is not there to draw it again
    if (std::filesystem::exists(path, ec)
        && (!std::filesystem::exists(words[0], ec)
            || std::filesystem::last_write_time(path, ec) > std::filesystem::last_write_time(words[0], ec))) {
        return "@image{inline} html " + name + " \"\"";
    }
    if (!run(words, path)) {
        std::filesystem::remove(path, ec);
        std::cerr << source << ":" << line << ": warning: @haio failed: " << command << "\n";
        return std::nullopt;
    }
    return "@image{inline} html " + name + " \"\"";
}

/** the commands of every tag on a line, and where the first one starts */
std::vector<std::string> commandsOf(std::string_view text, size_t& first) {
    std::vector<std::string> commands;
    first = text.find(tag);
    for (auto at = first; at != std::string_view::npos; at = text.find(tag, at + 1)) {
        const auto shut = text.find('}', at);
        if (shut == std::string_view::npos) break;

        // "@haio{convert} a.png ..." is "./build/bin/haio convert a.png ..."
        const auto verb = text.substr(at + tag.size(), shut - at - tag.size());
        auto words = text.substr(shut + 1, std::min(text.find(tag, shut), text.find("*/", shut)) - shut - 1);
        while (!words.empty() && words.front() == ' ') words.remove_prefix(1);
        while (!words.empty() && words.back() == ' ') words.remove_suffix(1);
        commands.push_back(std::string(haio) + " " + std::string(verb) + " " + std::string(words));
    }
    return commands;
}

/** a comment line holding tags and nothing else: " * @haio{convert} ..." */
bool onlyTags(std::string_view text, size_t first) {
    return first != std::string_view::npos
        && text.substr(0, first).find_first_not_of(" \t*/") == std::string_view::npos
        && text.find("*/") == std::string_view::npos;
}

}

/** the source with every tag drawn and replaced by its pictures */
void filter(std::istream& source, std::ostream& out, const std::string& name) {
    /**
     * lines of tags one under the other are one row of pictures: every picture is drawn
     * first, then the row goes on the first line and the rest keep only their "*", so
     * the line count is what it was.
     */
    std::vector<std::string> row;
    std::vector<std::string> prefixes;
    size_t rowStart = 0;
    const auto flush = [&] {
        if (prefixes.empty()) return;
        std::string pictures;
        for (const auto& command : row) {
            if (const auto picture = pictureOf(command, name, rowStart)) pictures += (pictures.empty() ? "" : " ") + *picture;
        }
        out << prefixes.front() << pictures << '\n';
        for (size_t i = 1; i < prefixes.size(); i++) {
            auto bare = prefixes[i];
            while (!bare.empty() && bare.back() == ' ') bare.pop_back();
            out << bare << '\n';
        }
        row.clear();
        prefixes.clear();
    };

    size_t number = 0;
    for (std::string line; std::getline(source, line);) {
        number++;
        size_t first = 0;
        auto commands = commandsOf(line, first);
        if (onlyTags(line, first) && !commands.empty()) {
            if (prefixes.empty()) rowStart = number;
            prefixes.push_back(line.substr(0, first));
            for (auto& command : commands) row.push_back(std::move(command));
            continue;
        }
        flush();
        if (commands.empty()) {
            out << line << '\n';
            continue;
        }

        // tags sharing a line with other words are drawn where they stand
        std::string drawn = line.substr(0, first);
        for (const auto& command : commands) {
            if (const auto picture = pictureOf(command, name, number)) drawn += *picture + " ";
        }
        const auto close = line.find("*/", first);
        out << drawn << (close == std::string::npos ? "" : line.substr(close)) << '\n';
    }
    flush();
}

int main(int argc, char* argv[]) {
    if (std::filesystem::path(argv[0]).filename() == "doxygen_images") {
        std::vector<std::string> roots(argv + 1, argv + argc);
        if (roots.empty()) roots = {"include", "library"};
        std::ostream nowhere(nullptr);
        for (const auto& root : roots) {
            for (const auto& entry : std::filesystem::recursive_directory_iterator(root)) {
                const auto extension = entry.path().extension();
                if (!entry.is_regular_file() || (extension != ".cpp" && extension != ".hpp")) continue;
                std::ifstream source(entry.path());
                filter(source, nowhere, entry.path().string());
            }
        }
        return 0;
    }

    if (argc != 2) {
        std::cerr << "usage: doxygen_filter <source>\n";
        return 1;
    }
    std::ifstream source(argv[1]);
    if (!source) {
        std::cerr << "doxygen_filter: cannot read " << argv[1] << '\n';
        return 1;
    }
    filter(source, std::cout, argv[1]);
    return 0;
}
