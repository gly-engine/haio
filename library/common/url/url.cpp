#include <haio_url.hpp>

#include <algorithm>
#include <cctype>

namespace {

bool isAlpha(char c) {
    return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z');
}

bool isDigit(char c) {
    return c >= '0' && c <= '9';
}

int hexValue(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

/**
 * the characters no part of a url may carry as they are. everything else is left to
 * the part that holds it, which is as strict as haio needs: a url with a space in it
 * is refused here rather than reaching a server that reads it differently.
 */
bool forbidden(unsigned char c) {
    if (c <= 0x20 || c >= 0x7F) return true;
    switch (c) {
        case '"': case '<': case '>': case '\\': case '^': case '`': case '{': case '|': case '}':
            return true;
        default:
            return false;
    }
}

bool wellFormed(std::string_view text) {
    for (size_t at = 0; at < text.size(); at++) {
        const auto c = static_cast<unsigned char>(text[at]);
        if (forbidden(c)) return false;
        if (c != '%') continue;
        if (at + 2 >= text.size()) return false;
        if (hexValue(text[at + 1]) < 0 || hexValue(text[at + 2]) < 0) return false;
        at += 2;
    }
    return true;
}

/** a scheme is a letter and then letters, digits, "+", "-" and "."; then a ":" */
size_t schemeLength(std::string_view text) {
    if (text.empty() || !isAlpha(text.front())) return 0;
    for (size_t at = 1; at < text.size(); at++) {
        const char c = text[at];
        if (c == ':') return at;
        if (!isAlpha(c) && !isDigit(c) && c != '+' && c != '-' && c != '.') return 0;
    }
    return 0;
}

/** rfc 3986 5.2.4, the part of resolving that turns "a/b/../c" into "a/c" */
std::string removeDotSegments(std::string_view input) {
    std::string output;
    while (!input.empty()) {
        if (input.starts_with("../")) {
            input.remove_prefix(3);
        } else if (input.starts_with("./")) {
            input.remove_prefix(2);
        } else if (input.starts_with("/./")) {
            input.remove_prefix(2);
        } else if (input == "/.") {
            input = "/";
        } else if (input.starts_with("/../") || input == "/..") {
            input = input.size() == 3 ? std::string_view("/") : input.substr(3);
            const auto slash = output.find_last_of('/');
            output.resize(slash == std::string::npos ? 0 : slash);
        } else if (input == "." || input == "..") {
            input = {};
        } else {
            const auto next = input.find('/', input.front() == '/' ? 1 : 0);
            const auto segment = input.substr(0, next);
            output.append(segment);
            input.remove_prefix(segment.size());
        }
    }
    return output;
}

}

namespace Haio {

namespace Percent {

std::string decode(std::string_view text, bool plusAsSpace) {
    std::string out;
    out.reserve(text.size());
    for (size_t at = 0; at < text.size(); at++) {
        const char c = text[at];
        if (c == '+' && plusAsSpace) {
            out.push_back(' ');
            continue;
        }
        if (c == '%' && at + 2 < text.size()) {
            const int high = hexValue(text[at + 1]);
            const int low = hexValue(text[at + 2]);
            if (high >= 0 && low >= 0) {
                out.push_back(static_cast<char>(high * 16 + low));
                at += 2;
                continue;
            }
        }
        out.push_back(c);
    }
    return out;
}

std::string encodePath(std::string_view text) {
    static constexpr char hex[] = "0123456789ABCDEF";
    std::string out;
    out.reserve(text.size());
    for (const unsigned char c : text) {
        const bool plain = isAlpha(static_cast<char>(c)) || isDigit(static_cast<char>(c))
                        || std::string_view("-._~!$&'()*+,;=:@/").find(static_cast<char>(c)) != std::string_view::npos;
        if (plain) {
            out.push_back(static_cast<char>(c));
        } else {
            out.push_back('%');
            out.push_back(hex[c >> 4]);
            out.push_back(hex[c & 0x0F]);
        }
    }
    return out;
}

}

std::optional<Url> Url::parse(std::string_view text) {
    if (const auto hash = text.find('#'); hash != std::string_view::npos) text = text.substr(0, hash);
    if (!wellFormed(text)) return std::nullopt;

    Url url;
    if (const auto length = schemeLength(text); length > 0) {
        url.scheme = std::string(text.substr(0, length));
        std::ranges::transform(url.scheme, url.scheme.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        text.remove_prefix(length + 1);
    }

    if (text.starts_with("//")) {
        text.remove_prefix(2);
        url.hasAuthority = true;

        const auto end = text.find_first_of("/?");
        auto authority = text.substr(0, end);
        text = end == std::string_view::npos ? std::string_view{} : text.substr(end);

        if (const auto at = authority.rfind('@'); at != std::string_view::npos) {
            url.userinfo = std::string(authority.substr(0, at));
            authority.remove_prefix(at + 1);
        }

        // an ipv6 address has colons of its own, so the port is only what follows "]"
        size_t portFrom = std::string_view::npos;
        if (authority.starts_with('[')) {
            const auto close = authority.find(']');
            if (close == std::string_view::npos) return std::nullopt;
            if (close + 1 < authority.size()) {
                if (authority[close + 1] != ':') return std::nullopt;
                portFrom = close + 1;
            }
        } else {
            portFrom = authority.find(':');
        }

        if (portFrom != std::string_view::npos) {
            const auto port = authority.substr(portFrom + 1);
            if (!std::ranges::all_of(port, isDigit)) return std::nullopt;
            url.port = std::string(port);
            authority = authority.substr(0, portFrom);
        }
        url.host = std::string(authority);
    }

    const auto question = text.find('?');
    url.path = std::string(text.substr(0, question));
    if (question != std::string_view::npos) {
        url.hasQuery = true;
        url.query = std::string(text.substr(question + 1));
    }
    return url;
}

std::optional<Url> Url::resolve(std::string_view reference) const {
    const auto parsed = parse(reference);
    if (!parsed) return std::nullopt;
    const auto& ref = *parsed;

    Url out;
    if (!ref.scheme.empty()) {
        out = ref;
        out.path = removeDotSegments(ref.path);
        return out;
    }

    out.scheme = scheme;
    if (ref.hasAuthority) {
        out.hasAuthority = true;
        out.userinfo = ref.userinfo;
        out.host = ref.host;
        out.port = ref.port;
        out.path = removeDotSegments(ref.path);
        out.hasQuery = ref.hasQuery;
        out.query = ref.query;
        return out;
    }

    out.hasAuthority = hasAuthority;
    out.userinfo = userinfo;
    out.host = host;
    out.port = port;

    if (ref.path.empty()) {
        out.path = path;
        out.hasQuery = ref.hasQuery || hasQuery;
        out.query = ref.hasQuery ? ref.query : query;
        return out;
    }

    if (ref.path.starts_with('/')) {
        out.path = removeDotSegments(ref.path);
    } else if (hasAuthority && path.empty()) {
        out.path = removeDotSegments("/" + ref.path);
    } else {
        const auto slash = path.find_last_of('/');
        const auto base = slash == std::string::npos ? std::string{} : path.substr(0, slash + 1);
        out.path = removeDotSegments(base + ref.path);
    }
    out.hasQuery = ref.hasQuery;
    out.query = ref.query;
    return out;
}

std::string Url::user() const {
    return Percent::decode(userinfo.substr(0, userinfo.find(':')));
}

std::string Url::password() const {
    const auto colon = userinfo.find(':');
    return colon == std::string::npos ? std::string{} : Percent::decode(userinfo.substr(colon + 1));
}

std::string Url::hostName() const {
    if (host.size() >= 2 && host.front() == '[' && host.back() == ']') return host.substr(1, host.size() - 2);
    return Percent::decode(host);
}

std::string Url::authority() const {
    return port.empty() ? host : host + ":" + port;
}

std::string Url::target() const {
    auto out = path.empty() ? std::string("/") : path;
    if (hasQuery) out += "?" + query;
    return out;
}

std::string Url::decodedPath() const {
    return Percent::decode(path);
}

void Url::setPath(std::string_view decoded) {
    path = Percent::encodePath(decoded);
    // with an authority in front, a path that does not start at "/" would run into it
    if (hasAuthority && !path.empty() && path.front() != '/') path.insert(path.begin(), '/');
}

std::vector<std::string> Url::segments() const {
    std::vector<std::string> out;
    std::string_view rest = path;
    if (rest.starts_with('/')) rest.remove_prefix(1);
    if (rest.empty()) return out;

    while (true) {
        const auto slash = rest.find('/');
        out.push_back(Percent::decode(rest.substr(0, slash)));
        if (slash == std::string_view::npos) break;
        rest.remove_prefix(slash + 1);
    }
    return out;
}

std::vector<std::pair<std::string, std::string>> Url::params(bool plusAsSpace) const {
    std::vector<std::pair<std::string, std::string>> out;
    if (!hasQuery || query.empty()) return out;

    std::string_view rest = query;
    while (true) {
        const auto amp = rest.find('&');
        const auto item = rest.substr(0, amp);
        const auto equals = item.find('=');
        out.emplace_back(Percent::decode(item.substr(0, equals), plusAsSpace),
                         equals == std::string_view::npos ? std::string{} : Percent::decode(item.substr(equals + 1), plusAsSpace));
        if (amp == std::string_view::npos) break;
        rest.remove_prefix(amp + 1);
    }
    return out;
}

std::string Url::str() const {
    std::string out;
    if (!scheme.empty()) out += scheme + ":";
    if (hasAuthority) {
        out += "//";
        if (!userinfo.empty()) out += userinfo + "@";
        out += authority();
    }
    out += path;
    if (hasQuery) out += "?" + query;
    return out;
}

}
