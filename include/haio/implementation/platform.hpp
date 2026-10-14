#pragma once

/**
 * everything haio asks of the machine: the loop, http both ways, and the Task they
 * are awaited with. the interfaces say what, and one directory under
 * library/implementation/platform says how.
 */
#include "haio/implementation/platform/interfaces/http.hpp"
#include "haio/implementation/platform/interfaces/runtime.hpp"
#include "haio/implementation/platform/task.hpp"
