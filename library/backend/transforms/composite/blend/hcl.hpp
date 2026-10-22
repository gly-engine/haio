#pragma once

#include <algorithm>
#include <array>
#include <cmath>

namespace Haio::Transforms::Hcl {

/**
 * hue, chroma and luma as imagemagick 6 splits a colour for -compose hue and its
 * kin: a hue from 0 to 1 around the hexagon, the spread between the largest channel
 * and the smallest, and rec.601 luma.
 */
struct Split {
    double hue = 0;
    double chroma = 0;
    double luma = 0;
};

inline Split of(const std::array<double, 4>& rgb) {
    const double r = rgb[0], g = rgb[1], b = rgb[2];
    const double max = std::max({r, g, b});
    const double chroma = max - std::min({r, g, b});
    double h = 0;
    if (chroma != 0) {
        if (r == max) h = std::fmod((g - b) / chroma + 6.0, 6.0);
        else if (g == max) h = (b - r) / chroma + 2.0;
        else h = (r - g) / chroma + 4.0;
    }
    return {h / 6.0, chroma, 0.298839 * r + 0.586811 * g + 0.114350 * b};
}

/** and back, luma first: whatever the hue and chroma make is lifted until it has that luma */
inline std::array<double, 3> rgbOf(const Split& split) {
    const double h = 6.0 * split.hue;
    const double c = split.chroma;
    const double x = c * (1.0 - std::fabs(std::fmod(h, 2.0) - 1.0));
    double r = 0, g = 0, b = 0;
    if (h < 1) r = c, g = x;
    else if (h < 2) r = x, g = c;
    else if (h < 3) g = c, b = x;
    else if (h < 4) g = x, b = c;
    else if (h < 5) r = x, b = c;
    else if (h < 6) r = c, b = x;
    const double m = split.luma - (0.298839 * r + 0.586811 * g + 0.114350 * b);
    return {r + m, g + m, b + m};
}

/**
 * the frame every hcl blend shares: where src is clear dst stays, where dst is clear
 * src goes in whole, and otherwise the mixed colour takes whichever alpha is larger
 * -- no blending by alpha at all, which is imagemagick 6's way with these.
 */
template <typename Mix>
std::array<double, 4> blend(const std::array<double, 4>& s, const std::array<double, 4>& d, Mix&& mix) {
    if (s[3] == 0) return d;
    if (d[3] == 0) return s;
    const auto rgb = rgbOf(mix(of(s), of(d)));
    return {rgb[0], rgb[1], rgb[2], std::max(s[3], d[3])};
}

}
