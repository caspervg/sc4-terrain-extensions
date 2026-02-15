#include "BridgeToolPanel.hpp"

#include "imgui.h"
#include "public/cIGZImGuiService.h"

BridgeToolPanel::BridgeToolPanel(TerrainExtensionsDllDirector* director, cIGZImGuiService* imgui)
	: ImGuiPanel()
	, pDirector(director)
	, pImguiService(imgui)
	, bOpen(false) {}


void BridgeToolPanel::OnRender() {
	ImGui::Begin("Bridge Approach Tool");

	ImGui::Text("Visual Options:");
	ImGui::Checkbox("Show Height Markers", &gShowHeightMarkers);
	ImGui::Checkbox("Show Grade Colors", &gShowGradeColors);
	ImGui::Checkbox("Show Grid Snap", &gShowGrid);

	ImGui::Separator();
	ImGui::Text("Color Legend:");
	ImGui::TextColored(ImVec4(0, 1, 0, 1), "Grade OK (< 80%%)");
	ImGui::TextColored(ImVec4(1, 0.67f, 0, 1), "Grade Warning (80-100%%)");
	ImGui::TextColored(ImVec4(1, 0, 0, 1), "Grade Exceeded (> 100%%)");

	ImGui::End();
}

void BridgeToolPanel::SetOpen(const bool open) {
	bOpen = open;
}
