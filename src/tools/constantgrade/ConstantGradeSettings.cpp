#include "ConstantGradeSettings.hpp"

#include <format>

ConstantGradeSettings::ConstantGradeSettings() {
    parameters.Add(gradePercent.Describe(
        "Grade", "%",
        {.alt = true, .shift = false, .ctrl = false}
    ));
}

std::string ConstantGradeSettings::ValueLabel() const {
    return std::format("grade {:.1f}%", gradePercent.value);
}
