#include "FlattenSettings.hpp"

#include <sstream>

FlattenSettings::FlattenSettings() {
    parameters.Add(explicitHeight.Describe(
        "Height", "m",
        {.alt = true, .shift = false, .ctrl = false}
    ));
    parameters.Add(deltaHeight.Describe(
        "Delta", "m",
        {.alt = false, .shift = false, .ctrl = true}
    ));
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

void FlattenSettings::FlipDeltaSign() noexcept {
    deltaHeight.value = -deltaHeight.value;
    deltaHeight.Validate();
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
