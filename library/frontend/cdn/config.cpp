#include <haio_cdn.hpp>

#include <boost/url/parse.hpp>
#include <boost/url/url.hpp>

#include <algorithm>
#include <cctype>
#include <fstream>
#include <iostream>
#include <sstream>
#include <chrono>
#include <cstdlib>
#include <initializer_list>
#include <stdexcept>

namespace urls = boost::urls;

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
    const auto host = std::string(url.host());
    const auto path = std::string(url.path());

    if (host.empty()) {
        if (path.empty() || path == "/") {
            throw std::runtime_error("file bucket \"" + name + "\" has no directory in its url");
        }
        return std::filesystem::path(path);
    }
    return std::filesystem::path(host + path);
}

std::string hostRegion(const std::string& host);

/**
 * most explicit first: what the url says outright, then what an amazon host implies,
 * then the environment every other aws tool reads.
 *
 * it is settled once, while the config loads, so a bucket whose region nobody can
 * work out fails at startup. guessing it wrong would instead sign a request that
 * comes back as a plain 403, which says nothing about the region being the problem.
 */
std::string regionOf(const urls::url_view_base& url) {
    for (const auto param : url.params()) {
        if (param.key == "region" && !param.value.empty()) return param.value;
    }

    if (const auto found = hostRegion(std::string(url.host())); !found.empty()) return found;

    const char* fromEnv = std::getenv("AWS_DEFAULT_REGION");
    return fromEnv ? fromEnv : std::string{};
}

/** s3.eu-west-1.amazonaws.com, and bucket.s3.eu-west-1.amazonaws.com, both name it */
std::string hostRegion(const std::string& host) {
    const auto marker = host.find(".s3.");
    const auto from = marker != std::string::npos ? marker + 4
                    : host.starts_with("s3.")     ? size_t{3}
                                                  : std::string::npos;
    if (from == std::string::npos) return {};

    const auto rest = host.substr(from);
    const auto dot = rest.find('.');
    if (dot == std::string::npos) return {};

    const auto candidate = rest.substr(0, dot);
    return candidate == "amazonaws" ? std::string{} : candidate;
}

void applyUrl(Haio::Cdn::BucketConfig& bucket) {
    if (bucket.url.empty()) {
        throw std::runtime_error("bucket \"" + bucket.name + "\" has no url");
    }

    const auto parsed = urls::parse_uri_reference(bucket.url);
    if (!parsed) {
        throw std::runtime_error("bucket \"" + bucket.name + "\" has an invalid url: " + bucket.url);
    }

    bucket.scheme = std::string(parsed->scheme());
    bucket.open = parsed->host() == "*";

    if (bucket.scheme == "file") {
        if (bucket.open) {
            throw std::runtime_error("a file bucket cannot be open: " + bucket.url);
        }
        bucket.root = fileRoot(*parsed, bucket.name);
        return;
    }

    if (bucket.scheme == "s3") {
        // a signature is made for one host, so there is nothing sensible to sign for
        // a bucket whose host arrives with the request
        if (bucket.open) throw std::runtime_error("an s3 bucket cannot be open: " + bucket.url);

        bucket.region = regionOf(*parsed);
        if (bucket.region.empty()) {
            throw std::runtime_error("cannot tell the region of s3 bucket \"" + bucket.name
                                     + "\"; name it in the host, as in s3.eu-west-1.amazonaws.com, "
                                       "or add ?region=<name> to its url, or set AWS_DEFAULT_REGION");
        }
        return;
    }
    if (bucket.scheme == "http" || bucket.scheme == "https") return;

    // "//\*" carries no scheme on purpose: the request supplies it
    if (bucket.scheme.empty()) {
        if (!bucket.open) {
            throw std::runtime_error("bucket \"" + bucket.name + "\" needs a scheme in its url: " + bucket.url);
        }
        return;
    }

    throw std::runtime_error("unsupported url scheme \"" + bucket.scheme + "\" in bucket \"" + bucket.name + "\"");
}

/**
 * a key nobody reads is almost always a typo, and silently ignoring it is how a cache
 * ends up switched off without anybody noticing. so an unknown key stops the server
 * at startup and says what it could have meant.
 */
std::runtime_error unknownKey(const std::string& where, const std::string& key, std::initializer_list<std::string_view> known) {
    std::string message = "unknown key \"" + key + "\" in " + where + "; it takes ";
    for (auto it = known.begin(); it != known.end(); ++it) {
        if (it != known.begin()) message += it + 1 == known.end() ? " and " : ", ";
        message += std::string(*it);
    }
    return std::runtime_error(message);
}

bool parseFlag(const std::string& key, const std::string& value) {
    if (value == "true" || value == "yes" || value == "1") return true;
    if (value == "false" || value == "no" || value == "0") return false;
    throw std::runtime_error(key + " must be true or false, got: " + value);
}

/** a count, a number of seconds, a number of megabytes: no units to misspell */
size_t parseCount(const std::string& key, const std::string& value) {
    try {
        const auto number = std::stoll(value);
        if (number < 0) throw std::runtime_error("negative");
        return static_cast<size_t>(number);
    } catch (const std::exception&) {
        throw std::runtime_error(key + " must be a whole number, got: " + value);
    }
}

/** file://dir, redis://host, or nothing at all, which keeps entries in this process */
void checkCacheUrl(const std::string& value) {
    const auto parsed = urls::parse_uri_reference(value);
    if (!parsed) throw std::runtime_error("cache url is not a url: " + value);

    const auto scheme = std::string(parsed->scheme());
    if (scheme != "file" && scheme != "redis" && scheme != "rediss") {
        throw std::runtime_error("unsupported cache scheme \"" + scheme + "\"; it takes file, redis and rediss");
    }
}

enum class Scope { Root, Cdn, Security, Cache, Bucket, Other };

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
            if (section == "cdn") {
                current = nullptr;
                scope = Scope::Cdn;
            } else if (section == "security") {
                current = nullptr;
                scope = Scope::Security;
            } else if (section == "cache") {
                current = nullptr;
                scope = Scope::Cache;
            } else if (section.starts_with(prefix)) {
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

        if (scope == Scope::Cdn) {
            config.declared.insert("cdn." + key);
            if (key == "host") config.host = value;
            else if (key == "port") config.port = parsePort(value);
            else throw unknownKey("cdn", key, {"host", "port"});
            continue;
        }

        if (scope == Scope::Security) {
            config.declared.insert("security." + key);
            if (key == "timeout") config.security.timeout = std::chrono::seconds(parseCount(key, value));
            else if (key == "max_size_pixel") config.security.maxSizePixel = static_cast<int>(parseCount(key, value));
            else if (key == "max_size_percent") config.security.maxSizePercent = static_cast<int>(parseCount(key, value));
            else if (key == "max_cache_entries_by_ip") config.security.maxCacheEntriesByIp = parseCount(key, value);
            else if (key == "max_requests_by_ip") config.security.maxRequestsByIp = parseCount(key, value);
            else if (key == "allow_unzip") config.security.allowUnzip = parseFlag(key, value);
            else if (key == "max_unzip") config.security.maxUnzip = parseCount(key, value) * 1024 * 1024;
            else throw unknownKey("security", key, {"timeout", "max_size_pixel", "max_size_percent",
                                                   "max_cache_entries_by_ip", "max_requests_by_ip",
                                                   "allow_unzip", "max_unzip"});
            continue;
        }

        if (scope == Scope::Cache) {
            config.declared.insert("cache." + key);
            if (key == "ttl") config.cache.ttl = std::chrono::seconds(parseCount(key, value));
            else if (key == "max_usage") config.cache.maxUsage = parseCount(key, value) * 1024 * 1024;
            else if (key == "url") { checkCacheUrl(value); config.cache.url = value; }
            else throw unknownKey("cache", key, {"ttl", "max_usage", "url"});
            continue;
        }

        // everything lives in a table now, so a key before the first one is a config
        // written against an older haio rather than something to guess at
        if (scope == Scope::Root) {
            throw std::runtime_error("\"" + key + "\" is outside any table; settings live under "
                                     "[cdn], [security], [cache] or [bucket.<name>]");
        }

        if (scope != Scope::Bucket || !current) continue;

        if (key == "url") current->url = value;
        else if (key == "access_key") current->accessKey = value;
        else if (key == "secret_key") current->secretKey = value;
        else if (key == "session_token") current->sessionToken = value;
        else throw unknownKey("bucket \"" + current->name + "\"", key,
                              {"url", "access_key", "secret_key", "session_token"});
    }

    for (auto& [name, bucket] : config.buckets) applyUrl(bucket);
    return config;
}

Config loadConfig(const std::filesystem::path& path) {
    std::ifstream in(path);
    if (!in) throw std::runtime_error("cannot open config: " + path.string());

    /**
     * the file may hold an s3 secret, so anybody who can read it can sign as this
     * server. the check is a warning rather than a refusal: whose machine this is, and
     * who else is on it, is not haio's call to make.
     */
    if (std::error_code ec; true) {
        using std::filesystem::perms;
        const auto mode = std::filesystem::status(path, ec).permissions();
        if (!ec && (mode & (perms::group_read | perms::others_read)) != perms::none) {
            std::cerr << "warning: " << path.string() << " can be read by more than its owner, "
                         "and a bucket's secret_key would be readable with it\n";
        }
    }

    std::ostringstream text;
    text << in.rdbuf();
    return parseConfig(text.str());
}

}
