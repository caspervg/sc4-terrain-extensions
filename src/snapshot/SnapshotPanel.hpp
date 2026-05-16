#pragma once
#include <functional>
#include <optional>
#include <string>
#include <chrono>

#include "public/ImGuiPanel.h"

class SnapshotManager;
class SnapshotPreviewRenderer;
class cISTETerrain;

class SnapshotPanel final : public ImGuiPanel {
public:
    using PartialRestoreCallback = std::function<void(int snapshotIndex)>;
    using DeferredAction = std::function<void()>;

    SnapshotPanel(SnapshotManager& mgr,
                  cISTETerrain* terrain,
                  SnapshotPreviewRenderer& renderer,
                  PartialRestoreCallback onPartialRestore);

    void OnRender() override;
    void OnUpdate() override;

    void SetTerrain(cISTETerrain* terrain) { terrain_ = terrain; }

    static constexpr uint32_t kPanelId = 0x7E5A9B01;

private:
    void RenderCaptureSection_();
    void RenderSnapshotList_();
    void RenderSnapshotActions_(int index);
    void UpdatePreview_(int index);
    void DeferAction_(DeferredAction action);
    std::string FormatTimestamp_(const std::chrono::system_clock::time_point& tp) const;

    SnapshotManager& mgr_;
    cISTETerrain* terrain_;
    SnapshotPreviewRenderer& renderer_;
    PartialRestoreCallback onPartialRestore_;

    char nameBuffer_[64]{};
    char descBuffer_[128]{};
    int confirmDeleteIndex_ = -1;
    int confirmRestoreIndex_ = -1;

    std::optional<DeferredAction> pendingAction_;
};
