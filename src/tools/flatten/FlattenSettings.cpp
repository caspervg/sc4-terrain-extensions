#include "FlattenSettings.hpp"

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
        .name = "Mode",
        .unit = "",
        .scrollBinding = {.alt = false, .shift = false, .ctrl = true},
        .GetAsFloat = []() { return 0.0f; },
        .GetDisplayValue = [this]() { return std::string(ModeLabel()); },
        .AdjustByDelta = [this](const int32_t delta) { CycleMode(delta); }
    });
}

void FlattenSettings::AdjustPrimaryValue(const int32_t delta) noexcept {
    if (mode == FlattenHeightMode::Delta) {
        deltaHeight.Adjust(delta);
        return;
    }

    explicitHeight.Adjust(delta);
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

const char* FlattenSettings::ModeLabel() const noexcept {
    return FlattenOperation::ModeName(mode);
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
