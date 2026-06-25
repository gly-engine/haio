#pragma once

#include "haio_object.hpp"

#include <string_view>

namespace Haio::String {

int getInt(std::string_view value);
Size getSize(std::string_view value);
Rect getRect(std::string_view value);
bool tryGetSize(std::string_view value, Size& out);
bool tryGetRect(std::string_view value, Rect& out);
bool tryGetCropGeometry(std::string_view value, Rect& out);

}
