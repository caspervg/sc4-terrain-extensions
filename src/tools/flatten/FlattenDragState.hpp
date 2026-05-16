#pragma once

struct FlattenDragState {
    int startX{};
    int startZ{};
    int currentX{};
    int currentZ{};
    int pickedReferenceTileX{};
    int pickedReferenceTileZ{};
    float pickedReferenceAverageHeight{};
    bool hasPickedReferenceTile{};
    bool selectionCommitted{};

    void ClearPickedReferenceTile() noexcept {
        pickedReferenceTileX = 0;
        pickedReferenceTileZ = 0;
        pickedReferenceAverageHeight = 0.0f;
        hasPickedReferenceTile = false;
    }
};
