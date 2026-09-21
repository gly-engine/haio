#include <bucket/bucket.hpp>

#include <boost/beast/core.hpp>

#include <boost/system/system_error.hpp>

namespace beast = boost::beast;

namespace Haio::Cdn {

/**
 * picks a fetcher by the scheme the config already parsed, and turns whatever it
 * throws into a Result, so nothing above this line has to catch anything.
 */
boost::asio::awaitable<Result<Blob>> fetchBucket(const Config& config, Bucket::ZipArchives& archives, std::string bucketName, std::string path) {
    try {
        const auto& security = config.security;
        const auto it = config.buckets.find(bucketName);
        if (it == config.buckets.end()) {
            co_return std::unexpected(Error{ErrorCode::NotFound, "unknown bucket: " + bucketName});
        }

        const auto& bucket = it->second;

        if (auto zipped = Bucket::splitZipPath(path)) {
            if (!security.allowUnzip) {
                co_return std::unexpected(Error{ErrorCode::InvalidInput,
                                                "this path names a file inside a zip, and allow_unzip is off"});
            }
            co_return co_await Bucket::fetchInsideZip(bucket, *zipped, security, archives);
        }

        if (bucket.scheme == "file") co_return Bucket::fetchFile(bucket, std::move(path));
        co_return co_await Bucket::fetchHttp(bucket, std::move(path));
    } catch (const Bucket::Failure& err) {
        co_return std::unexpected(Error{err.code, err.what()});
    } catch (const boost::system::system_error& err) {
        // beast reports its own deadline as a plain system error
        const bool timedOut = err.code() == beast::error::timeout;
        co_return std::unexpected(Error{timedOut ? ErrorCode::Timeout : ErrorCode::Upstream,
                                        timedOut ? "upstream timed out" : "upstream unreachable"});
    } catch (const std::exception& err) {
        co_return std::unexpected(Error{ErrorCode::Internal, err.what()});
    }
}

}
