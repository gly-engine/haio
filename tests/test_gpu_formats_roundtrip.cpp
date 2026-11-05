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

/** every gpu container, so a colour is added to the list in one place rather than four */
template <Haio::Color P>
void testEvery(const Haio::Image<P>& img) {
    testContainer<Haio::Format::KTX>(img);
    testContainer<Haio::Format::KTX2>(img);
    testContainer<Haio::Format::PVR>(img);
    testContainer<Haio::Format::DDS>(img);
}

}

auto main() -> int {
    try {
        const auto rgba = makeImage(8, 8);
        const auto rgb565 = unwrap(Haio::Codecs::Convert<Haio::Color::RGBA8888, Haio::Color::RGB565>(rgba));
        const auto rgb565Rgba = unwrap(Haio::Codecs::Convert<Haio::Color::RGB565, Haio::Color::RGBA8888>(rgb565));

        require(psnr(rgba, rgb565Rgba) > 40.0, "rgb565 psnr is too low");

        testEvery(rgba);
        testEvery(rgb565);
        testEvery(unwrap(Haio::Codecs::Convert<Haio::Color::RGBA8888, Haio::Color::ETC1>(rgba)));
        testEvery(unwrap(Haio::Codecs::Convert<Haio::Color::RGBA8888, Haio::Color::ETC2>(rgba)));
        testEvery(unwrap(Haio::Codecs::Convert<Haio::Color::RGBA8888, Haio::Color::ETC2A>(rgba)));
        testEvery(unwrap(Haio::Codecs::Convert<Haio::Color::RGBA8888, Haio::Color::EAC_R11>(rgba)));
        testEvery(unwrap(Haio::Codecs::Convert<Haio::Color::RGBA8888, Haio::Color::EAC_RG11>(rgba)));

        // a block is eight bytes, or sixteen once there is an alpha or a second channel
        require(Haio::sizeOf(Haio::Color::ETC2, {8, 8}) == 32, "etc2 is eight bytes a block");
        require(Haio::sizeOf(Haio::Color::ETC2A, {8, 8}) == 64, "etc2a is sixteen bytes a block");
        require(Haio::sizeOf(Haio::Color::EAC_R11, {8, 8}) == 32, "eac r11 is eight bytes a block");
        require(Haio::sizeOf(Haio::Color::EAC_RG11, {8, 8}) == 64, "eac rg11 is sixteen bytes a block");

        // and the sides round up to whole blocks rather than being refused
        require(Haio::sizeOf(Haio::Color::ETC2, {7, 5}) == 32, "etc2 rounds up to whole blocks");

        // dds carries no etc at all, and those are pairs that were never declared
        static_assert(!Haio::Codecs::Encodable<Haio::Format::DDS, Haio::Color::ETC1>);
        static_assert(!Haio::Codecs::Encodable<Haio::Format::DDS, Haio::Color::ETC2>);
        static_assert(Haio::Codecs::Encodable<Haio::Format::KTX2, Haio::Color::ETC2A>);

        /**
         * the gpu containers hold several colours and name no favourite, so a
         * conversion into one without a -pix_fmt is refused rather than guessed at.
         */
        require(!Haio::encodeColorFor(Haio::Format::KTX), "a ktx must be asked which colour");
        require(!Haio::encodeColorFor(Haio::Format::KTX2), "a ktx2 must be asked which colour");
        require(!Haio::encodeColorFor(Haio::Format::PVR), "a pvr must be asked which colour");
        require(!Haio::encodeColorFor(Haio::Format::DDS), "a dds must be asked which colour");
        require(Haio::encodeColorFor(Haio::Format::PNG) == Haio::Color::RGBA8888, "a png declares its own");

        return 0;
    } catch (const std::exception& err) {
        std::cerr << err.what() << "\n";
        return 1;
    }
}
