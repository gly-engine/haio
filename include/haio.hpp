#pragma once

#include "haio_codec.hpp"
#include "haio_common.hpp"
#include "haio_convert.hpp"
#include "haio_formats.hpp"
#include "haio_iwindow.hpp"
#include "haio_object.hpp"
#include "haio_pipeline.hpp"

namespace Haio {

std::unique_ptr<IWindow> CreateWindow(const char* title, int width, int height);

}

// last: they ask the codecs what they can do, so those must be declared by now
#include "haio_registry.hpp"
#include "haio_pipe.hpp"
