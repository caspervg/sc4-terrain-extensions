#include "FlattenSettings.hpp"

#include <algorithm>
#include <array>
#include <sstream>

namespace {
constexpr std::array kModeCycleOrder{
    FlattenHeightMode::ReferenceTileAverage,
    FlattenHeightMode::Delta,
    FlattenHeightMode::Explicit,
    FlattenHeightMode::Average,
    FlattenHeightMode::Minimum,
    FlattenHeightMode::Maximum
};

constexpr std::array kShapeCycleOrder{
    FlattenShapeMode::Rectangle,
    FlattenShapeMode::LineMask
};

int32_t NormalizeThickness(const int32_t value, const int32_t direction) noexcept {
    int32_t next = std::clamp(value, -9, 9);
    if (next == 0) {
        next += direction >= 0 ? 1 : -1;
    }
    return std::clamp(next, -9, 9);
}
}

FlattenSettings::FlattenSettings() {
    parameters.Add(ParameterDescriptor{
        .name = "Value",
        .unit = "",
        .scrollBinding = {.alt = true, .shift = false, .ctrl = false},
        .GetAsFloat = []() { return 0.0f; },
        .GetDisplayValue = [this]() { return ValueLabel(); },
        .AdjustByDelta = [this](const int32_t delta) { AdjustPrimaryValue(delta); }
    });
    parameters.Add(ParameterDescriptor{
        .name = "Thickness",
        .unit = "",
        .scrollBinding = {.alt = false, .shift = true, .ctrl = false},
        .GetAsFloat = [this]() { return static_cast<float>(lineThickness.value); },
        .GetDisplayValue = [this]() { return ThicknessLabel(); },
        .AdjustByDelta = [this](const int32_t delta) { AdjustLineThickness(delta); }
    });
}

void FlattenSettings::AdjustPrimaryValue(const int32_t delta) noexcept {
    if (mode == FlattenHeightMode::Delta) {
        deltaHeight.Adjust(delta);
        return;
    }

    explicitHeight.Adjust(delta);
}

void FlattenSettings::AdjustLineThickness(const int32_t delta) noexcept {
    const int direction = delta > 0 ? 1 : -1;
    lineThickness.value = NormalizeThickness(lineThickness.value + direction, direction);
}

void FlattenSettings::CycleMode(const int32_t delta) noexcept {
    auto it = std::find(kModeCycleOrder.begin(), kModeCycleOrder.end(), mode);
    int modeIndex = it != kModeCycleOrder.end()
        ? static_cast<int>(std::distance(kModeCycleOrder.begin(), it))
        : 0;
    modeIndex += (delta > 0) ? 1 : -1;

    if (modeIndex < 0) {
        modeIndex = static_cast<int>(kModeCycleOrder.size()) - 1;
    } else if (modeIndex >= static_cast<int>(kModeCycleOrder.size())) {
        modeIndex = 0;
    }

    mode = kModeCycleOrder[modeIndex];
}

void FlattenSettings::CycleShape(const int32_t delta) noexcept {
    auto it = std::find(kShapeCycleOrder.begin(), kShapeCycleOrder.end(), shape);
    int shapeIndex = it != kShapeCycleOrder.end()
        ? static_cast<int>(std::distance(kShapeCycleOrder.begin(), it))
        : 0;
    shapeIndex += (delta > 0) ? 1 : -1;

    if (shapeIndex < 0) {
        shapeIndex = static_cast<int>(kShapeCycleOrder.size()) - 1;
    } else if (shapeIndex >= static_cast<int>(kShapeCycleOrder.size())) {
        shapeIndex = 0;
    }

    shape = kShapeCycleOrder[shapeIndex];
}

const char* FlattenSettings::ModeLabel() const noexcept {
    return FlattenOperation::ModeName(mode);
}

const char* FlattenSettings::ShapeLabel() const noexcept {
    return FlattenOperation::ShapeName(shape);
}

std::string FlattenSettings::ValueLabel() const {
    std::ostringstream text;
    if (mode == FlattenHeightMode::Delta) {
        if (deltaHeight.value > 0.0f) {
            text << '+';
        }
        text << deltaHeight.value << "m";
    } else if (mode == FlattenHeightMode::Explicit) {
        text << explicitHeight.value << "m";
    } else {
        text << "auto";
    }
    return text.str();
}

std::string FlattenSettings::ThicknessLabel() const {
    if (shape != FlattenShapeMode::LineMask) {
        return "n/a";
    }

    std::ostringstream text;
    if (lineThickness.value > 0) {
        text << '+';
    }
    text << lineThickness.value;
    return text.str();
}
