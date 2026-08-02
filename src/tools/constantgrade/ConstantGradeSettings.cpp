#include "ConstantGradeSettings.hpp"

#include <array>
#include <format>

namespace {
constexpr std::array kShapeCycleOrder{
    ConstantGradeShapeMode::Rectangle,
    ConstantGradeShapeMode::Line,
};
}

ConstantGradeSettings::ConstantGradeSettings() {
    parameters.Add(gradePercent.Describe(
        "Grade", "%",
        {.alt = true, .shift = false, .ctrl = false}
    ));

    auto falloffDescriptor = falloffTiles.Describe(
        "Edge falloff", "",
        {.alt = false, .shift = false, .ctrl = true}
    );
    falloffDescriptor.GetDisplayValue = [this]() { return FalloffLabel(); };
    parameters.Add(std::move(falloffDescriptor));

    auto widthDescriptor = lineWidthTiles.Describe(
        "Line width", "",
        {.alt = false, .shift = true, .ctrl = false}
    );
    widthDescriptor.GetDisplayValue = [this]() { return LineWidthLabel(); };
    parameters.Add(std::move(widthDescriptor));
}

void ConstantGradeSettings::CycleShape(const int32_t delta) noexcept {
    const auto count = static_cast<int32_t>(kShapeCycleOrder.size());
    int32_t index = 0;
    for (int32_t i = 0; i < count; ++i) {
        if (kShapeCycleOrder[static_cast<size_t>(i)] == shape) {
            index = i;
            break;
        }
    }

    index = ((index + delta) % count + count) % count;
    shape = kShapeCycleOrder[static_cast<size_t>(index)];
}

const char* ConstantGradeSettings::ShapeLabel() const noexcept {
    return IsLineMode() ? "line" : "rectangle";
}

std::string ConstantGradeSettings::LineWidthLabel() const {
    if (!IsLineMode()) {
        return "n/a";
    }
    return std::format("{} tiles", lineWidthTiles.value);
}

std::string ConstantGradeSettings::FalloffLabel() const {
    if (falloffTiles.value <= 0) {
        return "hard edges";
    }
    return std::format("{} tiles", falloffTiles.value);
}

std::string ConstantGradeSettings::ValueLabel() const {
    return std::format(
        "grade {:.1f}% | falloff {} | shape {}",
        gradePercent.value,
        FalloffLabel(),
        ShapeLabel());
}
