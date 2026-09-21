#include <haio.hpp>

#include <cmath>
#include <iostream>
#include <stdexcept>
#include <vector>

namespace {

void require(bool ok, const char* message) {
    if (!ok) throw std::runtime_error(message);
}

/** unwraps on the spot: anything failing here is the test failing */
template <typename T>
T unwrap(Haio::Result<T> result) {
    if (!result) throw std::runtime_error(result.error().message);
    return *std::move(result);
}

Haio::Image<Haio::Color::RGBA8888> makeImage(int width, int height) {
    std::vector<uint8_t> data(static_cast<size_t>(width) * static_cast<size_t>(height) * 4);
    for (size_t i = 0; i < data.size() / 4; i++) {
        data[i * 4 + 0] = static_cast<uint8_t>((i * 37) % 256);
        data[i * 4 + 1] = static_cast<uint8_t>((i * 59) % 256);
        data[i * 4 + 2] = static_cast<uint8_t>((i * 83) % 256);
        data[i * 4 + 3] = 255;
    }
    return Haio::Image<Haio::Color::RGBA8888>{width, height, std::move(data)};
}

double psnr(const Haio::Image<Haio::Color::RGBA8888>& a, const Haio::Image<Haio::Color::RGBA8888>& b) {
    double sum = 0;
    for (size_t i = 0; i < a.data.size(); i++) {
        const double diff = static_cast<double>(a.data[i]) - static_cast<double>(b.data[i]);
        sum += diff * diff;
    }
    const double mse = sum / static_cast<double>(a.data.size());
    return mse == 0 ? 99.0 : 10.0 * std::log10(255.0 * 255.0 / mse);
}

/**
 * a container switched off with -DHAIO_CODEC_<NAME>=OFF has nothing to round trip,
 * and one that never accepted this colour never had a pair to begin with.
 */
template <Haio::Format Container, Haio::Color P>
void testContainer(const Haio::Image<P>& img) {
    if constexpr (Haio::Codecs::Encodable<Container, P> && Haio::Codecs::Decodable<Container, P>) {
        const auto container = unwrap(Haio::Codecs::Encode<Container, P>(Haio::Image<P>{img}));
        const auto restored = unwrap(Haio::Codecs::Decode<Container, P>(container));
        require(restored.width == img.width && restored.height == img.height, "container dimensions mismatch");
        require(restored.data == img.data, "container payload mismatch");
    }
}

}

auto main() -> int {
    try {
        const auto rgba = makeImage(8, 8);
        const auto rgb565 = unwrap(Haio::Codecs::Convert<Haio::Color::RGBA8888, Haio::Color::RGB565>(rgba));
        const auto rgb565Rgba = unwrap(Haio::Codecs::Convert<Haio::Color::RGB565, Haio::Color::RGBA8888>(rgb565));
        const auto etc1 = unwrap(Haio::Codecs::Convert<Haio::Color::RGBA8888, Haio::Color::ETC1>(rgba));

        require(psnr(rgba, rgb565Rgba) > 40.0, "rgb565 psnr is too low");

        testContainer<Haio::Format::KTX>(rgba);
        testContainer<Haio::Format::KTX>(rgb565);
        testContainer<Haio::Format::KTX>(etc1);

        testContainer<Haio::Format::KTX2>(rgba);
        testContainer<Haio::Format::KTX2>(rgb565);
        testContainer<Haio::Format::KTX2>(etc1);

        testContainer<Haio::Format::PVR>(rgba);
        testContainer<Haio::Format::PVR>(rgb565);
        testContainer<Haio::Format::PVR>(etc1);

        testContainer<Haio::Format::DDS>(rgba);
        testContainer<Haio::Format::DDS>(rgb565);

        // dds carries no etc1, and that is now a pair that was never declared
        static_assert(!Haio::Codecs::Encodable<Haio::Format::DDS, Haio::Color::ETC1>);
        static_assert(Haio::Codecs::Encodable<Haio::Format::KTX2, Haio::Color::ETC1>);

        return 0;
    } catch (const std::exception& err) {
        std::cerr << err.what() << "\n";
        return 1;
    }
}
