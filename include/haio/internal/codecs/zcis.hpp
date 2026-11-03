#pragma once

#include "haio_codec.hpp"

#include <span>

/**
 * what the zcis encoder and the zcis packer share: the .ar wrapper, the index that
 * says where each picture is cut from the canvas, and the two interlaced layers that
 * make the canvas arrive progressively.
 *
 * one picture is the packer's case with a single cut covering everything, so there is
 * one writer and not two.
 */
namespace Haio::Codecs::Zcis {

Result<Blob> writeCanvas(const Image<Color::RGBA8888>& canvas, std::span<const Rect> cuts);

}
