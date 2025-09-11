#pragma once
#include <vector>
#include <unordered_map>
#include <cstdint>

// Shared blueprint data structures
struct CapturedNetworkPiece {
    int relX = 0;
    int relZ = 0;
    uint32_t pieceId = 0;
    uint8_t rotation = 0;
    uint8_t flip = 0;
    uint8_t variation = 0;
    uint32_t networkType = 0; // cISC4NetworkOccupant::eNetworkType
    bool isIntersection = false;
};

struct CapturedBlueprint {
    int originX = 0;
    int originZ = 0;
    int width = 0;
    int height = 0;
    std::vector<int32_t> zoneTypes;              // row-major (z * width + x)
    std::unordered_map<int32_t,int> zoneCounts;  // enum integral -> count
    std::vector<CapturedNetworkPiece> networkPieces;
    bool IsValid() const { return width > 0 && height > 0 && (int)zoneTypes.size() == width * height; }
};

extern CapturedBlueprint g_LastCapturedBlueprint;
