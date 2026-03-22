#include "ConstantGradeSettings.hpp"

#include <format>

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
}

std::string ConstantGradeSettings::ValueLabel() const {
    return std::format(
        "grade {:.1f}% | smoothing {}",
        gradePercent.value,
        IsSideSmoothingEnabled() ? "on" : "off");
}
