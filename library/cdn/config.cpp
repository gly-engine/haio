#include <haio_cdn.hpp>

#include <algorithm>
#include <cctype>
#include <fstream>
#include <sstream>

namespace {

std::string trim(std::string value) {
    auto is_space = [](unsigned char c) { return std::isspace(c); };
    value.erase(value.begin(), std::find_if_not(value.begin(), value.end(), is_space));
    value.erase(std::find_if_not(value.rbegin(), value.rend(), is_space).base(), value.end());
    return value;
}

std::string unquote(std::string value) {
    value = trim(std::move(value));
    if (value.size() >= 2 && ((value.front() == '"' && value.back() == '"') || (value.front() == '\'' && value.back() == '\''))) {
        return value.substr(1, value.size() - 2);
    }
    return value;
}

}

namespace Haio::Cdn {

Config loadConfig(const std::filesystem::path& path, std::filesystem::path defaultRoot) {
    Config config;
    config.buckets["file"] = BucketConfig{.name = "file", .type = "file", .root = std::move(defaultRoot)};

    if (path.empty() || !std::filesystem::exists(path)) return config;

    std::ifstream in(path);
    if (!in) throw std::runtime_error("cannot open config: " + path.string());

    BucketConfig* current = nullptr;
    std::string line;
    while (std::getline(in, line)) {
        if (const auto comment = line.find('#'); comment != std::string::npos) line.resize(comment);
        line = trim(std::move(line));
        if (line.empty()) continue;

        if (line.front() == '[' && line.back() == ']') {
            const auto section = line.substr(1, line.size() - 2);
            constexpr std::string_view prefix = "bucket.";
            if (section.starts_with(prefix)) {
                const auto name = section.substr(prefix.size());
                auto& bucket = config.buckets[name];
                bucket.name = name;
                current = &bucket;
            } else {
                current = nullptr;
            }
            continue;
        }

        const auto eq = line.find('=');
        if (eq == std::string::npos || !current) continue;

        const auto key = trim(line.substr(0, eq));
        const auto value = unquote(line.substr(eq + 1));
        current->values[key] = value;

        if (key == "type") current->type = value;
        else if (key == "root" || key == "path") current->root = value;
        else if (key == "host") current->host = value;
        else if (key == "prefix") current->prefix = value;
        else if (key == "endpoint" || key == "base_url" || key == "url") current->endpoint = value;
    }

    return config;
}

}
