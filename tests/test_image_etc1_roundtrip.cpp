#include <haio.hpp>

#include <cmath>
#include <cstdint>
#include <exception>
#include <iostream>
#include <tuple>

namespace {

Haio::Image makeImage(int width, int height, auto pixel) {
    std::vector<uint8_t> data(static_cast<size_t>(width) * static_cast<size_t>(height) * 4);
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            const auto [r, g, b, a] = pixel(x, y);
            const auto offset = (static_cast<size_t>(y) * static_cast<size_t>(width) + static_cast<size_t>(x)) * 4;
            data[offset + 0] = r;
            data[offset + 1] = g;
            data[offset + 2] = b;
            data[offset + 3] = a;
        }
    }
    return Haio::Image{Haio::Format::RGBA8888, width, height, std::move(data)};
}

bool hasOpaqueAlpha(const Haio::Image& img) {
    for (size_t i = 3; i < img.data.size(); i += 4) {
        if (img.data[i] != 255) return false;
    }
    return true;
}

double psnr(const Haio::Image& a, const Haio::Image& b) {
    double mse = 0.0;
    size_t count = 0;
    for (size_t i = 0; i < a.data.size(); i += 4) {
        for (int c = 0; c < 3; c++) {
            const double diff = static_cast<double>(a.data[i + c]) - static_cast<double>(b.data[i + c]);
            mse += diff * diff;
            count++;
        }
    }
    mse /= static_cast<double>(count);
    return mse == 0.0 ? 99.0 : 10.0 * std::log10((255.0 * 255.0) / mse);
}

bool throwsRuntimeError(auto func) {
    try {
        func();
        return false;
    } catch (const std::runtime_error&) {
        return true;
    }
}

void require(bool condition, const char* message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

} // namespace

auto main() -> int {
    try {
        auto encode = Haio::Encode<Haio::Format::ETC1>();
        auto decode = Haio::Decode<Haio::Format::ETC1>();

        auto solid = makeImage(4, 4, [](int, int) {
            return std::tuple<uint8_t, uint8_t, uint8_t, uint8_t>{255, 0, 0, 255};
        });
        auto solidEtc1 = encode(solid);
        auto solidRoundtrip = decode(solidEtc1);
        require(solidEtc1.type == Haio::Format::ETC1, "solid encode should output etc1");
        require(solidEtc1.data.size() == 8, "solid 4x4 etc1 should be one block");
        require(solidRoundtrip.type == Haio::Format::RGBA8888, "solid decode should output rgba8888");
        require(solidRoundtrip.width == 4 && solidRoundtrip.height == 4, "solid roundtrip should keep dimensions");
        require(hasOpaqueAlpha(solidRoundtrip), "solid roundtrip alpha should be opaque");

        auto gradient = makeImage(16, 16, [](int x, int y) {
            return std::tuple<uint8_t, uint8_t, uint8_t, uint8_t>{
                static_cast<uint8_t>(32 + x * 8),
                static_cast<uint8_t>(48 + y * 8),
                static_cast<uint8_t>(96 + (x + y) * 2),
                255
            };
        });
        auto gradientRoundtrip = decode(encode(gradient));
        const double gradientPsnr = psnr(gradient, gradientRoundtrip);
        std::cout << "gradient psnr: " << gradientPsnr << " db\n";
        require(gradientPsnr > 30.0, "gradient psnr is too low");

        auto odd = makeImage(5, 7, [](int x, int y) {
            return std::tuple<uint8_t, uint8_t, uint8_t, uint8_t>{
                static_cast<uint8_t>(40 + x * 20),
                static_cast<uint8_t>(60 + y * 15),
                static_cast<uint8_t>(120 + x * 3 + y * 2),
                255
            };
        });
        auto oddEtc1 = encode(odd);
        auto oddRoundtrip = decode(oddEtc1);
        require(oddEtc1.data.size() == 2 * 2 * 8, "5x7 etc1 should be four blocks");
        require(oddRoundtrip.width == 5 && oddRoundtrip.height == 7, "odd roundtrip should keep logical dimensions");

        require(throwsRuntimeError([&] {
            decode(Haio::Image{Haio::Format::ETC1, 0, 4, std::vector<uint8_t>(8)});
        }), "decode with zero width should throw");
        require(throwsRuntimeError([&] {
            decode(Haio::Image{Haio::Format::ETC1, 4, 4, std::vector<uint8_t>(7)});
        }), "decode with invalid data size should throw");
        require(throwsRuntimeError([&] {
            encode(Haio::Image{Haio::Format::RGB888, 4, 4, std::vector<uint8_t>(4 * 4 * 3)});
        }), "encode with invalid format should throw");

        return 0;
    } catch (const std::exception& err) {
        std::cerr << err.what() << "\n";
        return 1;
    }
}
