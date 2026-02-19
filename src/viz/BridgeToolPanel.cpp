#include "BridgeToolPanel.hpp"
#include "imgui.h"
#include "public/cIGZImGuiService.h"
#include "tools/bridge/BridgeToolSettings.hpp"

BridgeToolPanel::BridgeToolPanel(BridgeToolSettings& settings)
	: ImGuiPanel()
	  , settings_(settings)
	  , bOpen(false) {}


void BridgeToolPanel::OnRender() {
	ImGui::Begin("Bridge Approach Tool");

	ImGui::Text("Options:");
	ImGui::Checkbox("Show Height Markers", &settings_.showHeightMarkers);

	ImGui::End();
}

void BridgeToolPanel::SetOpen(const bool open) {
	bOpen = open;
}
