#pragma once

#include <string>

#include "tools/ToolParameter.hpp"
#include "FlattenOperation.hpp"

struct FlattenSettings {
    FlattenSettings();

    TypedParameter<float> explicitHeight{
        .value = 250.0f,
        .minValue = -300.0f,
        .maxValue = 3000.0f,
        .step = 5.0f
    };

    TypedParameter<float> deltaHeight{
        .value = 7.5f,
        .minValue = -200.0f,
        .maxValue = 200.0f,
        .step = 2.5f
    };

    FlattenHeightMode mode{FlattenHeightMode::Explicit};
    ToolParameterSet parameters;

    void CycleMode(int32_t delta) noexcept;
    void FlipDeltaSign() noexcept;
    [[nodiscard]] const char* ModeLabel() const noexcept;
    [[nodiscard]] std::string ValueLabel() const;
};
