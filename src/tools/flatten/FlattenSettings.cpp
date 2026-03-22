#include "FlattenSettings.hpp"

#include <sstream>

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
    constexpr int kModeCount = 5;
    int modeIndex = static_cast<int>(mode);
    modeIndex += (delta > 0) ? 1 : -1;

    if (modeIndex < 0) {
        modeIndex = kModeCount - 1;
    } else if (modeIndex >= kModeCount) {
        modeIndex = 0;
    }

    mode = static_cast<FlattenHeightMode>(modeIndex);
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
    } else {
        text << explicitHeight.value << "m";
    }
    return text.str();
}
