#include <haio.hpp>

#include <cmath>
#include <cstdint>
#include <exception>
#include <iostream>

namespace {

Haio::Image makeImage(int width, int height) {
    std::vector<uint8_t> data(static_cast<size_t>(width) * static_cast<size_t>(height) * 4);
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            const auto off = (static_cast<size_t>(y) * static_cast<size_t>(width) + static_cast<size_t>(x)) * 4;
            data[off + 0] = static_cast<uint8_t>(x * 31 + y * 7);
            data[off + 1] = static_cast<uint8_t>(x * 5 + y * 23);
            data[off + 2] = static_cast<uint8_t>(x * 11 + y * 13);
            data[off + 3] = 255;
        }
    }
    return Haio::Image{Haio::Format::RGBA8888, width, height, std::move(data)};
}

double psnr(const Haio::Image& a, const Haio::Image& b) {
    double mse = 0.0;
    size_t count = 0;
    for (size_t i = 0; i < a.data.size(); i += 4) {
        for (int c = 0; c < 3; c++) {
            const double d = static_cast<double>(a.data[i + c]) - static_cast<double>(b.data[i + c]);
            mse += d * d;
            count++;
        }
    }
    mse /= static_cast<double>(count);
    return mse == 0.0 ? 99.0 : 10.0 * std::log10((255.0 * 255.0) / mse);
}

void require(bool ok, const char* message) {
    if (!ok) throw std::runtime_error(message);
}

template <Haio::Format Container>
void testContainer(const Haio::Image& img) {
    auto container = Haio::Encode<Container>()(img);
    auto restored = Haio::Decode<Container>()(container);
    require(restored.type == img.type, "container format mismatch");
    require(restored.width == img.width && restored.height == img.height, "container dimensions mismatch");
    require(restored.data == img.data, "container payload mismatch");
}

}

auto main() -> int {
    try {
        const auto rgba = makeImage(8, 8);
        const auto rgb565 = Haio::Encode<Haio::Format::RGB565>()(rgba);
        const auto rgb565Rgba = Haio::Decode<Haio::Format::RGB565>()(rgb565);
        const auto etc1 = Haio::Encode<Haio::Format::ETC1>()(rgba);

        require(psnr(rgba, rgb565Rgba) > 40.0, "rgb565 psnr is too low");

        testContainer<Haio::Format::KTX>(rgba);
        testContainer<Haio::Format::KTX>(rgb565);
        testContainer<Haio::Format::KTX>(etc1);

        testContainer<Haio::Format::PVR>(rgba);
        testContainer<Haio::Format::PVR>(rgb565);
        testContainer<Haio::Format::PVR>(etc1);

        testContainer<Haio::Format::DDS>(rgba);
        testContainer<Haio::Format::DDS>(rgb565);

        testContainer<Haio::Format::KTX2>(rgba);
        testContainer<Haio::Format::KTX2>(rgb565);
        testContainer<Haio::Format::KTX2>(etc1);

        bool ddsRejectedEtc1 = false;
        try {
            (void)Haio::Encode<Haio::Format::DDS>()(etc1);
        } catch (const std::runtime_error&) {
            ddsRejectedEtc1 = true;
        }
        require(ddsRejectedEtc1, "dds should reject etc1");

        return 0;
    } catch (const std::exception& err) {
        std::cerr << err.what() << "\n";
        return 1;
    }
}
