#pragma once

#include <cstdint>

struct ConstantGradeDragState {
    int32_t startX{0};
    int32_t startZ{0};
    int32_t currentX{0};
    int32_t currentZ{0};
    bool snapAngle{false};
};
