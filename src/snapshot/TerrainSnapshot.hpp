#pragma once
#include <chrono>
#include <cstdint>
#include <string>
#include <vector>

struct TerrainSnapshot {
    uint32_t id{0}; // Stable, unique per capture; 0 = invalid
    std::string name;
    std::string description;
    std::chrono::system_clock::time_point timestamp;
    uint32_t vertexCountX{0}; // CellCountX + 1
    uint32_t vertexCountZ{0}; // CellCountZ + 1
    std::vector<float> heights; // Dense, row-major [z * vertexCountX + x]

    [[nodiscard]] float GetHeight(int x, int z) const {
        return heights[static_cast<size_t>(z) * vertexCountX + x];
    }

    void SetHeight(int x, int z, float h) {
        heights[static_cast<size_t>(z) * vertexCountX + x] = h;
    }

    [[nodiscard]] bool IsValidVertex(int x, int z) const {
        return x >= 0 && z >= 0
            && static_cast<uint32_t>(x) < vertexCountX
            && static_cast<uint32_t>(z) < vertexCountZ;
    }
};
