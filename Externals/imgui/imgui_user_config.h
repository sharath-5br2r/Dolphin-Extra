#pragma once

#include "Common/Assert.h"

#define IM_ASSERT(_EXPR) ASSERT(_EXPR)

#define IMGUI_DISABLE_DEMO_WINDOWS

// Rich presence includes emoji and other codepoints beyond the Basic
// Multilingual Plane.
#define IMGUI_USE_WCHAR32
