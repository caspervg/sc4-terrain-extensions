#pragma once
#include <algorithm>
#include <chrono>
#include <cstdint>
#include <string>
#include <vector>

#include "TerrainSnapshot.hpp"

class cISTETerrain;

class SnapshotManager {
public:
    static constexpr size_t kMaxSnapshots = 10;

    bool Capture(cISTETerrain* terrain, const std::string& name, const std::string& desc = "");
    void RestoreFull(int index, cISTETerrain* terrain);
    void RestoreRegion(int index, cISTETerrain* terrain, int minX, int minZ, int maxX, int maxZ);
    void Remove(int index);
    void Clear();

    [[nodiscard]] const TerrainSnapshot* Get(int index) const;
    [[nodiscard]] size_t Count() const { return snapshots_.size(); }
    [[nodiscard]] bool IsFull() const { return snapshots_.size() >= kMaxSnapshots; }

    void SetPreviewIndex(int index) { previewIndex_ = index; }
    [[nodiscard]] int GetPreviewIndex() const { return previewIndex_; }

private:
    std::vector<TerrainSnapshot> snapshots_;
    int previewIndex_ = -1;
};
