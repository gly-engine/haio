#include <haio_convert.hpp>

#include <cstring>

#if defined(__x86_64__) || defined(__i386__)
#define HAIO_X86 1
#include <immintrin.h>
#endif

namespace {

void packScalar(const uint8_t* src, uint8_t* dst, size_t pixels) {
    while (pixels--) {
        dst[0] = src[0];
        dst[1] = src[1];
        dst[2] = src[2];
        src += 4;
        dst += 3;
    }
}

#if HAIO_X86

/**
 * twelve bytes out of a sixteen byte register: eight with one store and four with a
 * memcpy, because there is no twelve byte store and writing sixteen would run past
 * the end of the last block.
 */
inline void store12(uint8_t* dst, __m128i packed) {
    _mm_storel_epi64(reinterpret_cast<__m128i*>(dst), packed);
    const auto tail = static_cast<uint32_t>(_mm_cvtsi128_si32(_mm_srli_si128(packed, 8)));
    std::memcpy(dst + 8, &tail, sizeof(tail));
}

__attribute__((target("avx2")))
void packAvx2(const uint8_t* src, uint8_t* dst, size_t pixels) {
    const __m256i shuffle = _mm256_setr_epi8(
         0,  1,  2,  4,  5,  6,  8,  9, 10, 12, 13, 14, -1, -1, -1, -1,
         0,  1,  2,  4,  5,  6,  8,  9, 10, 12, 13, 14, -1, -1, -1, -1);

    const size_t wide = pixels & ~size_t{7};
    for (size_t at = 0; at < wide; at += 8) {
        const __m256i loaded = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(src));
        const __m256i packed = _mm256_shuffle_epi8(loaded, shuffle);

        // the shuffle works on each 128 bit lane on its own, so the halves land apart
        store12(dst, _mm256_castsi256_si128(packed));
        store12(dst + 12, _mm256_extracti128_si256(packed, 1));

        src += 32;
        dst += 24;
    }
    packScalar(src, dst, pixels - wide);
}

__attribute__((target("ssse3")))
void packSsse3(const uint8_t* src, uint8_t* dst, size_t pixels) {
    const __m128i shuffle = _mm_setr_epi8(0, 1, 2, 4, 5, 6, 8, 9, 10, 12, 13, 14, -1, -1, -1, -1);

    const size_t wide = pixels & ~size_t{3};
    for (size_t at = 0; at < wide; at += 4) {
        store12(dst, _mm_shuffle_epi8(_mm_loadu_si128(reinterpret_cast<const __m128i*>(src)), shuffle));
        src += 16;
        dst += 12;
    }
    packScalar(src, dst, pixels - wide);
}

#endif

/**
 * ported from BufferPush43 in the c branch, with two of its bugs left behind: it
 * rounded capacity with "& ~32", which clears one bit instead of aligning, and it
 * reported an error for an empty input that its callers treated as fatal.
 */
void pack(const uint8_t* src, uint8_t* dst, size_t pixels) {
#if HAIO_X86
    if (__builtin_cpu_supports("avx2")) return packAvx2(src, dst, pixels);
    if (__builtin_cpu_supports("ssse3")) return packSsse3(src, dst, pixels);
#endif
    packScalar(src, dst, pixels);
}

}

namespace Haio::Codecs {

/**
 * @addtogroup move
 * @{
 */
/** drop the alpha channel */
template <>
Result<void> Move<Color::RGBA8888, Color::RGB888>(Bytes src, std::span<uint8_t> dst, Size size) {
    const auto pixels = static_cast<size_t>(size.width) * static_cast<size_t>(size.height);
    if (src.size() < pixels * 4 || dst.size() < pixels * 3) {
        HAIO_FAIL(InvalidInput, "rgba8888 to rgb888 got a buffer that is too small");
    }
    if (pixels == 0) return {};

    pack(src.data(), dst.data(), pixels);
    return {};
}
/** @} */

/**
 * @addtogroup convert
 * @{
 */
template <>
Result<Image<Color::RGB888>> Convert<Color::RGBA8888, Color::RGB888>(Image<Color::RGBA8888> src) {
    return convertVia<Color::RGBA8888, Color::RGB888>(std::move(src));
}
/** @} */

}
