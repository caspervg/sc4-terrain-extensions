#pragma once

#include <cstdint>
#include <string>

#include "tools/ToolParameter.hpp"

enum class ConstantGradeShapeMode : uint8_t {
    Rectangle,
    Line,
};

struct ConstantGradeSettings {
    ConstantGradeSettings();

    TypedParameter<float> gradePercent{
        .value = 7.5f,
        .minValue = -25.0f,
        .maxValue = 25.0f,
        .step = 0.5f
    };
    TypedParameter<int32_t> falloffTiles{
        .value = 3,
        .minValue = 0,
        .maxValue = 16,
        .step = 1
    };
    TypedParameter<int32_t> lineWidthTiles{
        .value = 1,
        .minValue = 1,
        .maxValue = 16,
        .step = 1
    };

    ConstantGradeShapeMode shape{ConstantGradeShapeMode::Rectangle};

    ToolParameterSet parameters;

    [[nodiscard]] bool IsLineMode() const noexcept { return shape == ConstantGradeShapeMode::Line; }
    void CycleShape(int32_t delta) noexcept;
    [[nodiscard]] const char* ShapeLabel() const noexcept;
    [[nodiscard]] std::string LineWidthLabel() const;
    [[nodiscard]] std::string FalloffLabel() const;
    [[nodiscard]] std::string ValueLabel() const;
};
