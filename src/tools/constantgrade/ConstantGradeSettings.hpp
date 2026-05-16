#pragma once

#include <string>

#include "tools/ToolParameter.hpp"

struct ConstantGradeSettings {
    ConstantGradeSettings();

    TypedParameter<float> gradePercent{
        .value = 7.5f,
        .minValue = -25.0f,
        .maxValue = 25.0f,
        .step = 0.5f
    };
    TypedParameter<int> sideSmoothing{
        .value = 1,
        .minValue = 0,
        .maxValue = 1,
        .step = 1
    };

    ToolParameterSet parameters;

    [[nodiscard]] bool IsSideSmoothingEnabled() const noexcept { return sideSmoothing.value != 0; }
    [[nodiscard]] std::string ValueLabel() const;
};
