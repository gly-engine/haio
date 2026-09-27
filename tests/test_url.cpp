#include <haio_url.hpp>

#include <iostream>
#include <string>

using Haio::Url;

namespace {

int failures = 0;

void check(bool ok, const std::string& what) {
    if (ok) return;
    std::cerr << "fail: " << what << '\n';
    failures++;
}

void testParts() {
    const auto url = Url::parse("HTTPS://user:p%40ss@host.example:8443/a/b%20c.png?x=1&y=two#frag");
    check(url.has_value(), "a full url parses");
    if (!url) return;
    check(url->scheme == "https", "the scheme is lowercased");
    check(url->user() == "user" && url->password() == "p@ss", "the userinfo decodes");
    check(url->host == "host.example" && url->port == "8443", "host and port split");
    check(url->path == "/a/b%20c.png", "the path stays encoded");
    check(url->decodedPath() == "/a/b c.png", "and decodes when asked");
    check(url->query == "x=1&y=two", "the query stays as written");
    check(url->target() == "/a/b%20c.png?x=1&y=two", "the target carries path and query, not the fragment");
    check(url->authority() == "host.example:8443", "the authority is what a Host header carries");
    check(url->str() == "https://user:p%40ss@host.example:8443/a/b%20c.png?x=1&y=two", "it writes back out without the fragment");
}

void testShapes() {
    const auto open = Url::parse("//*");
    check(open && open->scheme.empty() && open->hasAuthority && open->host == "*", "an open bucket has no scheme and a * host");

    const auto star = Url::parse("https://*");
    check(star && star->host == "*" && star->path.empty(), "https://* is a * host");

    const auto file = Url::parse("file:///srv/img");
    check(file && file->host.empty() && file->path == "/srv/img", "file:/// has an empty host");

    const auto relative = Url::parse("file://assets/sub");
    check(relative && relative->host == "assets" && relative->path == "/sub", "file://dir keeps the dir as the host");

    const auto v6 = Url::parse("http://[::1]:8080/x");
    check(v6 && v6->host == "[::1]" && v6->port == "8080" && v6->hostName() == "::1", "an ipv6 host keeps its colons out of the port");

    const auto origin = Url::parse("/cdn/b/a%2Fb.png?resize=1x1");
    check(origin && origin->scheme.empty() && !origin->hasAuthority, "an origin form target has neither scheme nor host");
    if (origin) {
        const auto segments = origin->segments();
        check(segments.size() == 3 && segments[2] == "a/b.png", "segments decode, escaped slashes included");
    }
    const auto trailing = Url::parse("/a/b/");
    check(trailing && trailing->segments().size() == 3 && trailing->segments()[2].empty(), "a trailing slash is an empty segment");

    const auto region = Url::parse("s3://minio:9000/bucket?region=us-east-1");
    check(region && region->params().size() == 1 && region->params()[0].second == "us-east-1", "params come out as pairs");

    const auto plus = Url::parse("/?name=hello+world&flag");
    check(plus && plus->params(true)[0].second == "hello world", "plus reads as a space when asked");
    check(plus && plus->params(true)[1].first == "flag" && plus->params(true)[1].second.empty(), "a key without = has no value");
}

void testRefused() {
    check(!Url::parse("http://host/a b"), "a space is refused");
    check(!Url::parse("http://host/%zz"), "a broken escape is refused");
    check(!Url::parse("http://host/%2"), "a cut escape is refused");
    check(!Url::parse("http://host:80a/"), "a port that is not a number is refused");
    check(!Url::parse("http://[::1/"), "an unclosed ipv6 host is refused");
}

void testSetPath() {
    auto url = *Url::parse("https://host/prefix?keep=1");
    url.setPath("/prefix/my file#1.png");
    check(url.str() == "https://host/prefix/my%20file%231.png?keep=1", "setPath escapes and leaves the query alone");

    auto bare = *Url::parse("https://host");
    bare.setPath("x.png");
    check(bare.path == "/x.png", "a path after an authority always starts at /");
}

/** the examples rfc 3986 5.4 lists, which is what a redirect is resolved by */
void testResolve() {
    const auto base = *Url::parse("http://a/b/c/d;p?q");
    const auto to = [&](std::string_view ref) {
        const auto out = base.resolve(ref);
        return out ? out->str() : std::string("<none>");
    };

    check(to("g") == "http://a/b/c/g", "g");
    check(to("./g") == "http://a/b/c/g", "./g");
    check(to("g/") == "http://a/b/c/g/", "g/");
    check(to("/g") == "http://a/g", "/g");
    check(to("//g") == "http://g", "//g");
    check(to("?y") == "http://a/b/c/d;p?y", "?y");
    check(to("g?y") == "http://a/b/c/g?y", "g?y");
    check(to("") == "http://a/b/c/d;p?q", "empty");
    check(to(".") == "http://a/b/c/", ".");
    check(to("..") == "http://a/b/", "..");
    check(to("../g") == "http://a/b/g", "../g");
    check(to("../../g") == "http://a/g", "../../g");
    check(to("../../../g") == "http://a/g", "../../../g");
    check(to("/./g") == "http://a/g", "/./g");
    check(to("g/../h") == "http://a/b/c/h", "g/../h");
    check(to("https://other/x") == "https://other/x", "an absolute location replaces everything");
}

}

auto main() -> int {
    testParts();
    testShapes();
    testRefused();
    testSetPath();
    testResolve();

    if (failures == 0) std::cout << "url: ok\n";
    return failures == 0 ? 0 : 1;
}
