#include <haio_cdn.hpp>

#include <boost/url/parse.hpp>
#include <boost/url/url.hpp>

#include <algorithm>
#include <cctype>
#include <fstream>
#include <sstream>
#include <stdexcept>

namespace urls = boost::urls;

namespace {

std::string toString(std::string_view value) {
    return {value.begin(), value.end()};
}

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

unsigned short parsePort(const std::string& value) {
    try {
        const auto number = std::stoi(value);
        if (number > 0 && number <= 65535) return static_cast<unsigned short>(number);
    } catch (const std::exception&) {
    }
    throw std::runtime_error("port must be a number between 1 and 65535, got: " + value);
}

/** file://relative keeps the authority as the first path segment, file:///absolute has none */
std::filesystem::path fileRoot(const urls::url_view_base& url, const std::string& name) {
    const auto host = toString(url.host());
    const auto path = toString(url.path());

    if (host.empty()) {
        if (path.empty() || path == "/") {
            throw std::runtime_error("file bucket \"" + name + "\" has no directory in its endpoint");
        }
        return std::filesystem::path(path);
    }
    return std::filesystem::path(host + path);
}

void applyEndpoint(Haio::Cdn::BucketConfig& bucket) {
    if (bucket.endpoint.empty()) {
        throw std::runtime_error("bucket \"" + bucket.name + "\" has no endpoint");
    }

    const auto parsed = urls::parse_uri_reference(bucket.endpoint);
    if (!parsed) {
        throw std::runtime_error("bucket \"" + bucket.name + "\" has an invalid endpoint: " + bucket.endpoint);
    }

    bucket.scheme = toString(parsed->scheme());
    bucket.open = parsed->host() == "*";

    if (bucket.scheme == "file") {
        if (bucket.open) {
            throw std::runtime_error("a file bucket cannot be open: " + bucket.endpoint);
        }
        bucket.root = fileRoot(*parsed, bucket.name);
        return;
    }

    if (bucket.scheme == "http" || bucket.scheme == "https" || bucket.scheme == "s3") return;

    // "//\*" carries no scheme on purpose: the request supplies it
    if (bucket.scheme.empty()) {
        if (!bucket.open) {
            throw std::runtime_error("bucket \"" + bucket.name + "\" needs a scheme in its endpoint: " + bucket.endpoint);
        }
        return;
    }

    throw std::runtime_error("unsupported endpoint scheme \"" + bucket.scheme + "\" in bucket \"" + bucket.name + "\"");
}

enum class Scope { Root, Bucket, Other };

}

namespace Haio::Cdn {

Config parseConfig(std::string_view text) {
    Config config;
    std::istringstream in{std::string(text)};

    Scope scope = Scope::Root;
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
                scope = Scope::Bucket;
            } else {
                current = nullptr;
                scope = Scope::Other;
            }
            continue;
        }

        const auto eq = line.find('=');
        if (eq == std::string::npos) continue;

        const auto key = trim(line.substr(0, eq));
        const auto value = unquote(line.substr(eq + 1));

        if (scope == Scope::Root) {
            if (key == "host") config.host = value;
            else if (key == "port") config.port = parsePort(value);
            continue;
        }

        if (scope != Scope::Bucket || !current) continue;

        current->values[key] = value;
        if (key == "endpoint" || key == "base_url" || key == "url") current->endpoint = value;
    }

    for (auto& [name, bucket] : config.buckets) applyEndpoint(bucket);
    return config;
}

Config loadConfig(const std::filesystem::path& path) {
    std::ifstream in(path);
    if (!in) throw std::runtime_error("cannot open config: " + path.string());

    std::ostringstream text;
    text << in.rdbuf();
    return parseConfig(text.str());
}

}
