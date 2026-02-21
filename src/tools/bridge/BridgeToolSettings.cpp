#include "BridgeToolSettings.hpp"

BridgeToolSettings::BridgeToolSettings() {
	parameters.Add(height.Describe(
		"Height", "m",
		{.alt = true, .shift = false, .ctrl = false}
	));
	parameters.Add(grade.Describe(
		"Grade", "%",
		{.alt = true, .shift = true, .ctrl = false}
	));
}
