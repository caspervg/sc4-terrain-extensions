#pragma once
#include <cstdint>

enum class ControlStateId : uint32_t {
    Inactive        = 0,
    Hovering        = 1,
    Selecting       = 2,
    Executing       = 3,
    ToolSpecific    = 0x1000
};
