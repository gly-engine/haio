#pragma once

#include "haio_object.hpp"

#include <string_view>

namespace Haio::String {

int getInt(std::string_view value);
Size getSize(std::string_view value);

/**
 * a size written as a share of what came in: "30%", or "30pct" where a percent sign
 * cannot go.
 *
 * the url form exists because "%" opens an escape sequence: "?resize=30%" is not a
 * request for thirty percent, it is a request the server has to reject as a broken
 * escape. "pct" says the same thing and survives being a url.
 *
 * it answers zero when the text is not a share at all, since zero percent of a
 * picture is nothing and could never have been meant.
 */
int getPercent(std::string_view value);
Rect getRect(std::string_view value);
bool tryGetSize(std::string_view value, Size& out);
bool tryGetRect(std::string_view value, Rect& out);
bool tryGetCropGeometry(std::string_view value, Rect& out);

}
