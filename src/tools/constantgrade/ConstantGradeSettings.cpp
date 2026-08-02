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

    auto smoothingDescriptor = sideSmoothing.Describe(
        "Side smoothing", "",
        {.alt = false, .shift = false, .ctrl = true}
    );
    smoothingDescriptor.GetDisplayValue = [this]() {
        return IsSideSmoothingEnabled() ? std::string("on") : std::string("off");
    };
    parameters.Add(std::move(smoothingDescriptor));

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

std::string ConstantGradeSettings::ValueLabel() const {
    return std::format(
        "grade {:.1f}% | smoothing {} | shape {}",
        gradePercent.value,
        IsSideSmoothingEnabled() ? "on" : "off",
        ShapeLabel());
}
