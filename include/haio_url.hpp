#pragma once

#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

/**
 * a url, taken apart the way rfc 3986 does it and no further.
 *
 * it used to be boost.url, which is a fine library and a compiled one: every place
 * that looked at a scheme or a query pulled it in, and it was the last piece of boost
 * outside the platform besides spirit. what haio asks of a url is small -- the parts,
 * the query as pairs, the path as segments, and resolving a redirect -- so it is here
 * instead, and compiles wherever the rest of haio does.
 *
 * the parts are kept encoded, exactly as written, and decoded only when asked. that
 * is the one rule that keeps a signed s3 path and the path the server sees the same.
 */
namespace Haio {

struct Url {
    /** lowercase, empty for a reference such as "//host/path" or "/path" */
    std::string scheme;
    bool hasAuthority = false;
    std::string userinfo;
    /** as written, brackets included for an ipv6 address; may be "*" */
    std::string host;
    /** digits only, empty when the url names none */
    std::string port;
    std::string path;
    bool hasQuery = false;
    std::string query;

    /**
     * an absolute url or any reference to one. the fragment is dropped, since nothing
     * haio fetches or serves ever sees it. nothing is returned for a character that
     * cannot appear in a url, a broken escape, or a port that is not a number.
     */
    static std::optional<Url> parse(std::string_view text);

    /** where a reference such as a redirect's location leads from here, rfc 3986 5.2 */
    std::optional<Url> resolve(std::string_view reference) const;

    std::string user() const;
    std::string password() const;

    /** the host without ipv6 brackets, which is what a resolver wants */
    std::string hostName() const;

    /** host[:port], which is what a Host header carries */
    std::string authority() const;

    /** the path, "/" when there is none, and the query: what a request line carries */
    std::string target() const;

    std::string decodedPath() const;

    /** replaces the path with one written in plain text, escaping what needs it */
    void setPath(std::string_view decoded);

    /** the path split on "/", each segment decoded; "/a/b/" is {"a", "b", ""} */
    std::vector<std::string> segments() const;

    /** the query as decoded pairs, in order; a key without "=" has an empty value */
    std::vector<std::pair<std::string, std::string>> params(bool plusAsSpace = false) const;

    /** the whole url written back out */
    std::string str() const;
};

namespace Percent {

/** "%2F" as "/"; an escape that is not one is left as it was written */
std::string decode(std::string_view text, bool plusAsSpace = false);

/** everything a path may not carry as it is, escaped; "/" stays a separator */
std::string encodePath(std::string_view text);

}

}
