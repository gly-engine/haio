#include <haio_platform.hpp>

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <iterator>
#include <system_error>

namespace Haio::Platform {
namespace {

namespace fs = std::filesystem;

bool wild(std::string_view text) {
    return text.find('*') != std::string_view::npos;
}

bool same(char a, char b, bool keepCase) {
    if (keepCase) return a == b;
    return std::tolower(static_cast<unsigned char>(a)) == std::tolower(static_cast<unsigned char>(b));
}

/**
 * a path relative to where the walk started, against what is left of the pattern
 * after it. ** may eat slashes and * may not, and a ** followed by a slash
 * may also stand for no folder at all, so src/ ** /a.png finds src/a.png too.
 */
bool matches(std::string_view pattern, std::string_view path, bool keepCase) {
    if (pattern.empty()) return path.empty();

    if (pattern.starts_with("**")) {
        auto rest = pattern.substr(2);
        if (rest.starts_with('/') && matches(rest.substr(1), path, keepCase)) return true;
        for (size_t at = 0; at <= path.size(); at++) {
            if (matches(rest, path.substr(at), keepCase)) return true;
        }
        return false;
    }
    if (pattern.front() == '*') {
        for (size_t at = 0; at <= path.size(); at++) {
            if (matches(pattern.substr(1), path.substr(at), keepCase)) return true;
            if (at < path.size() && path[at] == '/') break;
        }
        return false;
    }
    if (path.empty()) return false;
    return same(pattern.front(), path.front(), keepCase) && matches(pattern.substr(1), path.substr(1), keepCase);
}

}

std::vector<std::string> glob(std::string_view pattern) {
    if (const auto bar = pattern.find('|'); bar != std::string_view::npos) {
        const auto dot = pattern.rfind('.', bar);
        const auto head = dot == std::string_view::npos ? std::string_view{} : pattern.substr(0, dot + 1);
        auto tails = pattern.substr(head.size());

        std::vector<std::string> out;
        while (true) {
            const auto end = tails.find('|');
            auto found = glob(std::string(head) + std::string(tails.substr(0, end)));
            out.insert(out.end(), std::make_move_iterator(found.begin()), std::make_move_iterator(found.end()));
            if (end == std::string_view::npos) break;
            tails.remove_prefix(end + 1);
        }
        std::ranges::sort(out);
        out.erase(std::ranges::unique(out).begin(), out.end());
        return out;
    }

    std::string spelled(pattern);
    std::ranges::replace(spelled, '\\', '/');
    if (const auto home = homeFolder(); !home.empty() && (spelled == "~" || spelled.starts_with("~/"))) {
        spelled = home + spelled.substr(1);
    }

    // the folders before the first wildcard are walked from, not matched
    std::string root;
    std::string_view rest = spelled;
    while (true) {
        const auto slash = rest.find('/');
        if (slash == std::string_view::npos || wild(rest.substr(0, slash))) break;
        root += std::string(rest.substr(0, slash + 1));
        rest.remove_prefix(slash + 1);
    }

    const auto keepCase = namesKeepCase();
    const auto deep = rest.find("**") != std::string_view::npos;
    const auto levels = static_cast<int>(std::ranges::count(rest, '/'));
    const fs::path start = root.empty() ? fs::path(".") : fs::path(root);

    std::vector<std::string> out;
    std::error_code problem;
    fs::recursive_directory_iterator it(start, fs::directory_options::skip_permission_denied, problem);
    for (; !problem && it != fs::recursive_directory_iterator(); it.increment(problem)) {
        const auto name = it->path().filename().string();
        if (name.starts_with('.')) {
            if (it->is_directory(problem)) it.disable_recursion_pending();
            continue;
        }
        if (it->is_directory(problem)) {
            if (!deep && it.depth() >= levels) it.disable_recursion_pending();
            continue;
        }
        if (!it->is_regular_file(problem)) continue;

        const auto relative = it->path().lexically_relative(start).generic_string();
        if (matches(rest, relative, keepCase)) out.push_back(root + relative);
    }
    std::ranges::sort(out);
    return out;
}

}
