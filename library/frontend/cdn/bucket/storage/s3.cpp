#include <internal/bucket.hpp>

#include <boost/asio/use_awaitable.hpp>
#include <boost/url/parse.hpp>
#include <boost/url/url.hpp>

#include <wolfssl/options.h>
#include <wolfssl/wolfcrypt/hmac.h>
#include <wolfssl/wolfcrypt/sha256.h>

#include <array>
#include <stdexcept>
#include <cstdlib>
#include <ctime>
#include <iostream>
#include <string>
#include <vector>

namespace asio = boost::asio;
namespace urls = boost::urls;

namespace {

using Digest = std::array<uint8_t, WC_SHA256_DIGEST_SIZE>;

std::string toHex(const uint8_t* bytes, size_t size) {
    static constexpr char hex[] = "0123456789abcdef";
    std::string out(size * 2, '0');
    for (size_t at = 0; at < size; at++) {
        out[at * 2] = hex[bytes[at] >> 4];
        out[at * 2 + 1] = hex[bytes[at] & 0x0F];
    }
    return out;
}

Digest sha256(std::string_view text) {
    Digest out{};
    wc_Sha256 state;
    if (wc_InitSha256(&state) != 0) throw std::runtime_error("cannot start sha256");
    wc_Sha256Update(&state, reinterpret_cast<const uint8_t*>(text.data()), static_cast<word32>(text.size()));
    wc_Sha256Final(&state, out.data());
    wc_Sha256Free(&state);
    return out;
}

Digest hmac(const uint8_t* key, size_t keySize, std::string_view text) {
    Digest out{};
    Hmac state;
    if (wc_HmacInit(&state, nullptr, INVALID_DEVID) != 0) throw std::runtime_error("cannot start hmac");
    wc_HmacSetKey(&state, WC_SHA256, key, static_cast<word32>(keySize));
    wc_HmacUpdate(&state, reinterpret_cast<const uint8_t*>(text.data()), static_cast<word32>(text.size()));
    wc_HmacFinal(&state, out.data());
    wc_HmacFree(&state);
    return out;
}

Digest hmac(const Digest& key, std::string_view text) {
    return hmac(key.data(), key.size(), text);
}

std::string env(const char* name) {
    const char* value = std::getenv(name);
    return value ? value : std::string{};
}

/**
 * every byte outside the unreserved set is escaped, and "/" stays a separator.
 *
 * this has to agree with the signature exactly: a path escaped one way here and
 * another way in the canonical request signs something the server never sees, and the
 * answer is a 403 that says nothing about which of the two was wrong.
 */
std::string encodePath(std::string_view path) {
    static constexpr char hex[] = "0123456789ABCDEF";
    std::string out;
    out.reserve(path.size());

    for (const unsigned char c : path) {
        const bool plain = (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9')
                        || c == '-' || c == '_' || c == '.' || c == '~' || c == '/';
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

struct Stamp {
    std::string date;      // 20240115
    std::string moment;    // 20240115T103000Z
};

Stamp nowUtc() {
    const auto clock = std::time(nullptr);
    std::tm parts{};
    gmtime_r(&clock, &parts);

    char date[16];
    char moment[32];
    std::strftime(date, sizeof(date), "%Y%m%d", &parts);
    std::strftime(moment, sizeof(moment), "%Y%m%dT%H%M%SZ", &parts);
    return Stamp{date, moment};
}

}

namespace Haio::Cdn::Bucket {

std::string awsSignatureV4(std::string_view secret, std::string_view date, std::string_view region,
                           std::string_view service, std::string_view stringToSign) {
    const std::string seed = "AWS4" + std::string(secret);
    const auto signing =
        hmac(hmac(hmac(hmac(reinterpret_cast<const uint8_t*>(seed.data()), seed.size(), date),
                       region), service), "aws4_request");
    return toHex(hmac(signing, stringToSign).data(), WC_SHA256_DIGEST_SIZE);
}

/**
 * signature version 4, which is what turns an https request into an s3 one.
 *
 * the credentials are read from the environment on every request rather than kept,
 * so rotating them does not need a restart, and nothing secret is ever written in the
 * config file. without them the request goes out unsigned, which is exactly right for
 * a public bucket and gives a plain 403 from aws for a private one.
 */
asio::awaitable<Blob> fetchS3(const BucketConfig& bucket, std::string path) {
    const auto endpoint = urls::parse_uri_reference(bucket.url);
    if (!endpoint) {
        std::cerr << "invalid s3 endpoint: " << bucket.url << "\n";
        throw Failure(ErrorCode::Internal, "this bucket is misconfigured");
    }

    const std::string host{endpoint->has_port() ? std::string(endpoint->host()) + ":" + std::string(endpoint->port())
                                                : std::string(endpoint->host())};

    // "?region=" told the config which region this is; it is no part of the object
    std::string prefix{endpoint->path()};
    while (!prefix.empty() && prefix.back() == '/') prefix.pop_back();

    auto resource = prefix + (path.empty() || path.front() == '/' ? "" : "/") + path;
    if (resource.empty() || resource.front() != '/') resource.insert(resource.begin(), '/');

    const auto canonicalPath = encodePath(resource);
    const auto target = "https://" + host + canonicalPath;

    // the config says it; the environment is only asked when the config stays quiet
    const auto key = bucket.accessKey.empty() ? env("AWS_ACCESS_KEY_ID") : bucket.accessKey;
    const auto secret = bucket.secretKey.empty() ? env("AWS_SECRET_ACCESS_KEY") : bucket.secretKey;
    if (key.empty() || secret.empty()) {
        // a public bucket needs no signature; a private one will say so itself
        co_return co_await fetchUrlWith(target, std::move(path), {});
    }

    const auto stamp = nowUtc();
    const auto token = bucket.sessionToken.empty() ? env("AWS_SESSION_TOKEN") : bucket.sessionToken;

    // a GET carries no body, and the hash of nothing is still part of what is signed
    const auto payload = toHex(sha256("").data(), WC_SHA256_DIGEST_SIZE);

    std::string signedHeaders = "host;x-amz-content-sha256;x-amz-date";
    std::string canonicalHeaders = "host:" + host + "\n"
                                 + "x-amz-content-sha256:" + payload + "\n"
                                 + "x-amz-date:" + stamp.moment + "\n";
    if (!token.empty()) {
        // the list and the block have to stay in the same order, and it is alphabetical
        signedHeaders += ";x-amz-security-token";
        canonicalHeaders += "x-amz-security-token:" + token + "\n";
    }

    const auto canonicalRequest = "GET\n" + canonicalPath + "\n\n" + canonicalHeaders + "\n"
                                + signedHeaders + "\n" + payload;

    const auto scope = stamp.date + "/" + bucket.region + "/s3/aws4_request";
    const auto toSign = "AWS4-HMAC-SHA256\n" + stamp.moment + "\n" + scope + "\n"
                      + toHex(sha256(canonicalRequest).data(), WC_SHA256_DIGEST_SIZE);

    const auto signature = awsSignatureV4(secret, stamp.date, bucket.region, "s3", toSign);

    Headers headers{
        {"x-amz-content-sha256", payload},
        {"x-amz-date", stamp.moment},
        {"authorization", "AWS4-HMAC-SHA256 Credential=" + key + "/" + scope
                        + ", SignedHeaders=" + signedHeaders + ", Signature=" + signature},
    };
    if (!token.empty()) headers.emplace("x-amz-security-token", token);

    co_return co_await fetchUrlWith(target, std::move(path), std::move(headers));
}

}
