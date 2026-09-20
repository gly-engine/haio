#pragma once

#include "haio.hpp"

#include <boost/asio/awaitable.hpp>

#include <filesystem>
#include <map>
#include <string_view>

namespace Haio::Cdn {

/**
 * a bucket is described entirely by the scheme of its endpoint:
 *
 *   file://relative/dir     file:///absolute/dir
 *   http://host/prefix      https://host/prefix      s3://host/bucket
 *   https://\*               open, the request names the host
 *   //\*                     open, the request names the scheme and the host
 */
struct BucketConfig {
    std::string name;
    std::string endpoint;

    // derived from endpoint while the config loads, so a bad one fails at startup
    std::string scheme;
    bool open = false;
    std::filesystem::path root;

    std::map<std::string, std::string> values;
};

struct Config {
    std::string host = "0.0.0.0";
    unsigned short port = 8080;
    std::map<std::string, BucketConfig> buckets;
};

Config parseConfig(std::string_view text);
Config loadConfig(const std::filesystem::path& path);
boost::asio::awaitable<Result<Blob>> fetchBucket(const Config& config, std::string bucket, std::string path);
boost::asio::awaitable<void> runServer(Config config);

}
