#pragma once

#include <optional>
#include <string>

#include "ConstantGradeDragState.hpp"
#include "ConstantGradeOperation.hpp"

struct ConstantGradeSettings;

[[nodiscard]] std::optional<ConstantGradeRequest> BuildConstantGradeRequest(
    const ConstantGradeDragState& dragState,
    const ConstantGradeSettings& settings);

[[nodiscard]] std::string DescribeConstantGradeDirection(const ConstantGradeRequest& request);
