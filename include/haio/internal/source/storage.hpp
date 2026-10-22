#pragma once

#include <haio_platform.hpp>
#include <haio_source.hpp>

#include <string>
#include <string_view>

/**
 * one file per way of reaching bytes, the way the codecs do it. the dispatcher in
 * source.cpp picks by the scheme resolveOrigin settled, and each fetcher only knows
 * its own protocol.
 *
 * every one of them answers with a Result, the way the codecs do: a failure is a
 * value that says why, and nothing between here and the caller has to catch it.
 */
namespace Haio::Source {

/** file:// origins: a directory on this machine, and never a step above it */
Result<Blob> fetchFile(const Origin& origin, std::string path);

/** http and https origins, open or with a fixed endpoint */
Task<Result<Blob>> fetchHttp(const Origin& origin, std::string path);

/**
 * the same fetch, with headers somebody else worked out. the platform does the
 * asking; this decides whether the answer was a picture.
 */
Task<Result<Blob>> fetchUrlWith(std::string url, std::string pathForFormat, Platform::Headers headers);

/**
 * s3 origins, which are https with a signature.
 *
 * the credentials come from the origin, or else from the environment the way every
 * other aws tool reads them. the region only ever comes from the origin.
 */
Task<Result<Blob>> fetchS3(const Origin& origin, std::string path);

/** whichever of the three the scheme names */
Task<Result<Blob>> fetchAny(const Origin& origin, std::string path);

/**
 * the signature part of sigv4, on its own so it can be checked against the vectors
 * aws publishes. the key is derived over date, region and service and then used over
 * the string to sign; getting the last step wrong yields something that looks like a
 * signature and is refused exactly like a wrong secret.
 */
std::string awsSignatureV4(std::string_view secret, std::string_view date, std::string_view region,
                           std::string_view service, std::string_view stringToSign);

}
