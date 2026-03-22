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

    ToolParameterSet parameters;

    [[nodiscard]] std::string ValueLabel() const;
};
