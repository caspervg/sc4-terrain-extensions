#include "BridgeToolSettings.hpp"

namespace {

constexpr int kSideModeCount = 4;

}

BridgeToolSettings::BridgeToolSettings() {
	parameters.Add(height.Describe(
		"Height", "m",
		{.alt = true, .shift = false, .ctrl = false}
	));
	parameters.Add(grade.Describe(
		"Grade", "%",
		{.alt = false, .shift = true, .ctrl = false}
	));
	parameters.Add(ParameterDescriptor{
		.name = "Side",
		.unit = "",
		.scrollBinding = {.alt = false, .shift = false, .ctrl = true},
		.GetAsFloat = []() { return 0.0f; },
		.GetDisplayValue = [this]() {
			return std::string(BridgeApproachGeometry::GetApproachSideModeName(sideMode));
		},
		.AdjustByDelta = [this](const int32_t delta) { CycleSideMode(delta); }
	});
}

void BridgeToolSettings::CycleSideMode(const int32_t delta) {
	int value = static_cast<int>(sideMode);
	value += (delta > 0) ? 1 : -1;

	if (value < 0) {
		value = kSideModeCount - 1;
	}
	else if (value >= kSideModeCount) {
		value = 0;
	}

	sideMode = static_cast<BridgeApproachGeometry::ApproachSideMode>(value);
}
