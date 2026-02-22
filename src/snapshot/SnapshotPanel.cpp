#include "SnapshotPanel.hpp"

#include "imgui.h"
#include "SnapshotManager.hpp"
#include "SnapshotPreviewRenderer.hpp"
#include "cISTETerrain.h"
#include "utils/Logger.h"

#include <ctime>

SnapshotPanel::SnapshotPanel(
	SnapshotManager& mgr,
	cISTETerrain* terrain,
	SnapshotPreviewRenderer& renderer,
	PartialRestoreCallback onPartialRestore)
	: mgr_(mgr)
	, terrain_(terrain)
	, renderer_(renderer)
	, onPartialRestore_(std::move(onPartialRestore))
{
}

void SnapshotPanel::OnUpdate() {
	if (pendingAction_) {
		auto action = std::move(*pendingAction_);
		pendingAction_.reset();
		action();
	}
}

void SnapshotPanel::OnRender() {
	ImGui::SetNextWindowSize(ImVec2(280, 0), ImGuiCond_FirstUseEver);

	if (!ImGui::Begin("Snapshots", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
		ImGui::End();
		return;
	}

	RenderCaptureSection_();
	RenderSnapshotList_();

	ImGui::End();
}

void SnapshotPanel::RenderCaptureSection_() {
	ImGui::PushItemWidth(160);
	ImGui::InputTextWithHint("##name", "Name", nameBuffer_, sizeof(nameBuffer_));
	ImGui::PopItemWidth();
	ImGui::SameLine();

	const bool canCapture = !mgr_.IsFull() && terrain_ && nameBuffer_[0] != '\0';
	if (!canCapture) ImGui::BeginDisabled();
	if (ImGui::Button("Save")) {
		if (mgr_.Capture(terrain_, nameBuffer_, descBuffer_)) {
			nameBuffer_[0] = '\0';
			descBuffer_[0] = '\0';
		}
	}
	if (!canCapture) ImGui::EndDisabled();

	if (mgr_.IsFull()) {
		ImGui::SameLine();
		ImGui::TextColored(ImVec4(1, 0.6f, 0.2f, 1), "(full)");
	}

	ImGui::Separator();
}

void SnapshotPanel::RenderSnapshotList_() {
	if (mgr_.Count() == 0) {
		ImGui::TextDisabled("No snapshots yet.");
		return;
	}

	for (int i = 0; i < static_cast<int>(mgr_.Count()); ++i) {
		ImGui::PushID(i);
		RenderSnapshotActions_(i);
		ImGui::PopID();
	}
}

void SnapshotPanel::RenderSnapshotActions_(int index) {
	const auto* snap = mgr_.Get(index);
	if (!snap) return;

	const bool isPreview = (mgr_.GetPreviewIndex() == index);

	// Row: name + time, then action buttons below
	ImGui::TextUnformatted(snap->name.c_str());
	ImGui::SameLine();
	ImGui::TextDisabled("%s", FormatTimestamp_(snap->timestamp).c_str());

	// Action buttons on one line
	if (isPreview) {
		ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.3f, 0.5f, 0.3f, 1.0f));
		if (ImGui::SmallButton("Preview")) {
			mgr_.SetPreviewIndex(-1);
			renderer_.ClearAll();
		}
		ImGui::PopStyleColor();
	} else {
		if (ImGui::SmallButton("Preview")) {
			UpdatePreview_(index);
		}
	}

	ImGui::SameLine();

	if (confirmRestoreIndex_ == index) {
		ImGui::SmallButton("Restore?");
		ImGui::SameLine();
		if (ImGui::SmallButton("Y")) {
			const int idx = index;
			DeferAction_([this, idx]() {
				mgr_.RestoreFull(idx, terrain_);
				if (mgr_.GetPreviewIndex() >= 0)
					UpdatePreview_(mgr_.GetPreviewIndex());
			});
			confirmRestoreIndex_ = -1;
		}
		ImGui::SameLine();
		if (ImGui::SmallButton("N")) {
			confirmRestoreIndex_ = -1;
		}
	} else {
		if (ImGui::SmallButton("Restore")) {
			confirmRestoreIndex_ = index;
			confirmDeleteIndex_ = -1;
		}
	}

	ImGui::SameLine();

	if (ImGui::SmallButton("Region")) {
		if (onPartialRestore_) {
			const int idx = index;
			DeferAction_([this, idx]() {
				onPartialRestore_(idx);
			});
		}
	}

	ImGui::SameLine();

	if (confirmDeleteIndex_ == index) {
		ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1, 0.4f, 0.4f, 1));
		ImGui::SmallButton("X?");
		ImGui::PopStyleColor();
		ImGui::SameLine();
		if (ImGui::SmallButton("Y##d")) {
			mgr_.Remove(index);
			confirmDeleteIndex_ = -1;
			confirmRestoreIndex_ = -1;
			if (mgr_.GetPreviewIndex() >= 0 && terrain_)
				UpdatePreview_(mgr_.GetPreviewIndex());
		}
		ImGui::SameLine();
		if (ImGui::SmallButton("N##d")) {
			confirmDeleteIndex_ = -1;
		}
	} else {
		ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.6f, 0.6f, 0.6f, 1));
		if (ImGui::SmallButton("X")) {
			confirmDeleteIndex_ = index;
			confirmRestoreIndex_ = -1;
		}
		ImGui::PopStyleColor();
	}

	if (index < static_cast<int>(mgr_.Count()) - 1)
		ImGui::Separator();
}

void SnapshotPanel::UpdatePreview_(int index) {
	mgr_.SetPreviewIndex(index);
	const auto* snap = mgr_.Get(index);
	if (snap && terrain_) {
		renderer_.Rebuild(*snap, terrain_);
	}
}

void SnapshotPanel::DeferAction_(DeferredAction action) {
	pendingAction_ = std::move(action);
}

std::string SnapshotPanel::FormatTimestamp_(
	const std::chrono::system_clock::time_point& tp) const
{
	const auto time = std::chrono::system_clock::to_time_t(tp);
	std::tm tm{};
	localtime_s(&tm, &time);

	char buf[32];
	std::strftime(buf, sizeof(buf), "%H:%M:%S", &tm);
	return buf;
}
