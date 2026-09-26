#include <internal/bucket.hpp>

#include <iostream>
#include <string>

using namespace Haio::Cdn::Bucket;

namespace {

int failures = 0;

void check(bool ok, const std::string& what) {
    if (ok) return;
    std::cerr << "fail: " << what << '\n';
    failures++;
}

}

auto main() -> int {
    /**
     * the example aws publishes for signature version 4. it is worth checking against
     * rather than trusting: a signature that is wrong compiles, sends, and comes back
     * as a 403 that says nothing about which of the five steps was the bad one.
     */
    {
        const std::string secret = "wJalrXUtnFEMI/K7MDENG+bPxRfiCYEXAMPLEKEY";
        const std::string toSign =
            "AWS4-HMAC-SHA256\n"
            "20150830T123600Z\n"
            "20150830/us-east-1/iam/aws4_request\n"
            "f536975d06c0309214f805bb90ccff089219ecd68b2577efef23edd43b7e1a59";

        const auto signature = awsSignatureV4(secret, "20150830", "us-east-1", "iam", toSign);
        check(signature == "5d672d79c15b13162d9279b0855cfba6789a8edb4c82c400e06b5924a6f2b5d7",
              "the published aws example signs to its published signature, got " + signature);
        check(signature.size() == 64, "a signature is a sha256 in hex");
    }

    // a different secret, date, region or service must not sign the same
    {
        const std::string toSign = "AWS4-HMAC-SHA256\n20150830T123600Z\nscope\nhash";
        const auto base = awsSignatureV4("secret", "20150830", "us-east-1", "s3", toSign);
        check(base != awsSignatureV4("other", "20150830", "us-east-1", "s3", toSign), "the secret is part of it");
        check(base != awsSignatureV4("secret", "20150831", "us-east-1", "s3", toSign), "the date is part of it");
        check(base != awsSignatureV4("secret", "20150830", "eu-west-1", "s3", toSign), "the region is part of it");
        check(base != awsSignatureV4("secret", "20150830", "us-east-1", "iam", toSign), "the service is part of it");
        check(base != awsSignatureV4("secret", "20150830", "us-east-1", "s3", toSign + "x"), "the request is part of it");
        check(base == awsSignatureV4("secret", "20150830", "us-east-1", "s3", toSign), "the same inputs sign the same");
    }

    if (failures == 0) std::cout << "cdn s3: ok\n";
    return failures == 0 ? 0 : 1;
}
