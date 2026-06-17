#pragma once

#include "haio.hpp"

#include <boost/asio/awaitable.hpp>

#include <filesystem>
#include <map>

namespace Haio::Cdn {

struct BucketConfig {
    std::string name;
    std::string type = "file";
    std::filesystem::path root = ".";
    std::string host;
    std::string prefix;
    std::string endpoint;
    std::map<std::string, std::string> values;
};

struct Config {
    std::string host = "0.0.0.0";
    unsigned short port = 8080;
    std::map<std::string, BucketConfig> buckets;
};

Config loadConfig(const std::filesystem::path& path, std::filesystem::path defaultRoot = ".");
boost::asio::awaitable<Blob> fetchBucket(const Config& config, std::string bucket, std::string path);
boost::asio::awaitable<void> runServer(Config config);

}
