#pragma once

#include "fast/interpreter.h"
#include "fast/backends/gfx_opengl.h"
#include "fast/backends/gfx_window_manager_api.h"

namespace Fast {
void GfxSetInstance(std::shared_ptr<Interpreter> gfx);
}
