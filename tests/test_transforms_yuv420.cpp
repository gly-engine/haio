#include <haio.hpp>
#include <haio/internal/codecs/yuv.hpp>
#include <haio/transforms/blur.hpp>
#include <haio/transforms/resize.hpp>

#include <cassert>
#include <cstdlib>
#include <vector>

namespace {

using Haio::Color;
using Haio::Image;
using Haio::Size;
namespace Yuv420 = Haio::Codecs::Yuv420;

/** a 4:2:0 picture whose every sample says where it is, with the spare luma filled the way libjpeg fills it */
Image<Color::YUV420> makeYuv(int width, int height) {
    const auto planes = Yuv420::planesOf(Size{width, height});
    std::vector<uint8_t> data(planes.bytes, 0);
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) data[static_cast<size_t>(y) * planes.lumaWidth + x] = static_cast<uint8_t>(x * 7 + y * 13);
    }
    Yuv420::repeatEdges(data, Size{width, height}, planes);
    for (int y = 0; y < planes.chromaHeight; y++) {
        for (int x = 0; x < planes.chromaWidth; x++) {
            const auto at = static_cast<size_t>(y) * planes.chromaWidth + x;
            data[planes.luma + at] = static_cast<uint8_t>(40 + x * 11 + y);
            data[planes.luma + planes.chroma + at] = static_cast<uint8_t>(200 - x - y * 9);
        }
    }
    return Image<Color::YUV420>{width, height, std::move(data)};
}

uint8_t lumaAt(const Image<Color::YUV420>& image, int x, int y) {
    return image.data[static_cast<size_t>(y) * Yuv420::planesOf(Size{image.width, image.height}).lumaWidth + x];
}

uint8_t chromaAt(const Image<Color::YUV420>& image, size_t plane, int x, int y) {
    const auto planes = Yuv420::planesOf(Size{image.width, image.height});
    return image.data[planes.luma + plane * planes.chroma + static_cast<size_t>(y) * planes.chromaWidth + x];
}

/** the spare luma column and row of an odd side are still a copy of their neighbour */
void assertEdgesRepeated(const Image<Color::YUV420>& image) {
    const auto planes = Yuv420::planesOf(Size{image.width, image.height});
    if (planes.lumaWidth > image.width) {
        for (int y = 0; y < image.height; y++) assert(lumaAt(image, image.width, y) == lumaAt(image, image.width - 1, y));
    }
    if (planes.lumaHeight > image.height) {
        for (int x = 0; x < planes.lumaWidth; x++) assert(lumaAt(image, x, image.height) == lumaAt(image, x, image.height - 1));
    }
}

void testResizeToTheSameSizeChangesNothing() {
    for (const auto& size : {Size{8, 6}, Size{7, 5}, Size{1, 1}, Size{3, 2}}) {
        const auto source = makeYuv(size.width, size.height);
        const auto resized = Haio::Transforms::Resize<Color::YUV420>(source, size, Haio::ResizeFilter::Point);
        assert(resized && resized->width == size.width && resized->height == size.height);
        assert(resized->data == source.data);
    }
}

void testResizeDoublesEveryPlane() {
    const auto source = makeYuv(6, 4);
    const auto resized = Haio::Transforms::Resize<Color::YUV420>(source, Size{12, 8}, Haio::ResizeFilter::Point);
    assert(resized);
    for (int y = 0; y < 8; y++) {
        for (int x = 0; x < 12; x++) assert(lumaAt(*resized, x, y) == lumaAt(source, x / 2, y / 2));
    }
    for (size_t plane = 0; plane < 2; plane++) {
        for (int y = 0; y < 4; y++) {
            for (int x = 0; x < 6; x++) assert(chromaAt(*resized, plane, x, y) == chromaAt(source, plane, x / 2, y / 2));
        }
    }
}

void testResizeKeepsChromaWithItsLuma() {
    // halving: every output pixel is the top left of a source block, so its colour is that block's
    const auto source = makeYuv(8, 8);
    const auto resized = Haio::Transforms::Resize<Color::YUV420>(source, Size{4, 4}, Haio::ResizeFilter::Point);
    assert(resized);
    for (int y = 0; y < 4; y++) {
        for (int x = 0; x < 4; x++) assert(lumaAt(*resized, x, y) == lumaAt(source, x * 2, y * 2));
    }
    for (size_t plane = 0; plane < 2; plane++) {
        for (int y = 0; y < 2; y++) {
            for (int x = 0; x < 2; x++) assert(chromaAt(*resized, plane, x, y) == chromaAt(source, plane, x * 2, y * 2));
        }
    }
}

void testResizeToOddSidesFillsTheSpareLuma() {
    for (const auto& size : {Size{5, 3}, Size{9, 9}, Size{1, 7}, Size{4, 5}}) {
        const auto resized = Haio::Transforms::Resize<Color::YUV420>(makeYuv(10, 8), size, Haio::ResizeFilter::Point);
        assert(resized && resized->width == size.width && resized->height == size.height);
        assert(resized->data.size() == Yuv420::planesOf(size).bytes);
        assertEdgesRepeated(*resized);
    }
}

void testResizeWorksOutAMissingSide() {
    const auto wide = Haio::Transforms::Resize<Color::YUV420>(makeYuv(20, 10), Size{10, 0}, Haio::ResizeFilter::Point);
    assert(wide && wide->width == 10 && wide->height == 5);
    const auto tall = Haio::Transforms::Resize<Color::YUV420>(makeYuv(20, 10), Size{0, 4}, Haio::ResizeFilter::Point);
    assert(tall && tall->width == 8 && tall->height == 4);
}

void testResizeRefusesNonsense() {
    assert((!Haio::Transforms::Resize<Color::YUV420>(makeYuv(4, 4), Size{0, 0}, Haio::ResizeFilter::Point)));
    assert((!Haio::Transforms::Resize<Color::YUV420>(Image<Color::YUV420>{4, 4, std::vector<uint8_t>(3)}, Size{2, 2},
                                                     Haio::ResizeFilter::Point)));
}

void testBlurOfZeroChangesNothing() {
    const auto source = makeYuv(7, 5);
    const auto blurred = Haio::Transforms::Blur<Color::YUV420>(source, 0);
    assert(blurred && blurred->data == source.data);
}

void testBlurKeepsAFlatPictureFlat() {
    const auto planes = Yuv420::planesOf(Size{9, 7});
    std::vector<uint8_t> data(planes.bytes);
    std::fill_n(data.begin(), planes.luma, uint8_t{90});
    std::fill_n(data.begin() + static_cast<std::ptrdiff_t>(planes.luma), planes.chroma, uint8_t{60});
    std::fill_n(data.begin() + static_cast<std::ptrdiff_t>(planes.luma + planes.chroma), planes.chroma, uint8_t{170});
    const auto blurred = Haio::Transforms::Blur<Color::YUV420>(Image<Color::YUV420>{9, 7, data}, 4);
    assert(blurred && blurred->data == data);
}

void testBlurSoftensAnEdgeInEveryPlane() {
    // left half dark, right half bright, in luma and in both chroma planes
    const Size size{16, 8};
    const auto planes = Yuv420::planesOf(size);
    std::vector<uint8_t> data(planes.bytes);
    for (int y = 0; y < 8; y++) {
        for (int x = 0; x < 16; x++) data[static_cast<size_t>(y) * planes.lumaWidth + x] = x < 8 ? 0 : 240;
    }
    for (size_t plane = 0; plane < 2; plane++) {
        for (int y = 0; y < planes.chromaHeight; y++) {
            for (int x = 0; x < planes.chromaWidth; x++) {
                data[planes.luma + plane * planes.chroma + static_cast<size_t>(y) * planes.chromaWidth + x] = x < 4 ? 20 : 220;
            }
        }
    }
    const auto blurred = Haio::Transforms::Blur<Color::YUV420>(Image<Color::YUV420>{16, 8, data}, 2);
    assert(blurred);

    // the far ends keep their colour, the edge turns into a ramp that only climbs
    assert(lumaAt(*blurred, 0, 4) < 10 && lumaAt(*blurred, 15, 4) > 230);
    assert(lumaAt(*blurred, 7, 4) > 0 && lumaAt(*blurred, 8, 4) < 240);
    for (int x = 1; x < 16; x++) assert(lumaAt(*blurred, x, 4) >= lumaAt(*blurred, x - 1, 4));
    for (size_t plane = 0; plane < 2; plane++) {
        assert(chromaAt(*blurred, plane, 3, 2) > 20 && chromaAt(*blurred, plane, 4, 2) < 220);
    }
}

void testBlurKeepsTheSpareLumaACopy() {
    const auto blurred = Haio::Transforms::Blur<Color::YUV420>(makeYuv(9, 7), 3);
    assert(blurred);
    assertEdgesRepeated(*blurred);
}

void testWiring() {
    static_assert(Haio::Transforms::Resizable<Color::YUV420>);
    static_assert(Haio::Transforms::Blurrable<Color::YUV420>);
    static_assert(Haio::Transforms::Blurrable<Color::RGBA8888>);
}

}

int main() {
    testResizeToTheSameSizeChangesNothing();
    testResizeDoublesEveryPlane();
    testResizeKeepsChromaWithItsLuma();
    testResizeToOddSidesFillsTheSpareLuma();
    testResizeWorksOutAMissingSide();
    testResizeRefusesNonsense();
    testBlurOfZeroChangesNothing();
    testBlurKeepsAFlatPictureFlat();
    testBlurSoftensAnEdgeInEveryPlane();
    testBlurKeepsTheSpareLumaACopy();
    testWiring();
    return 0;
}
