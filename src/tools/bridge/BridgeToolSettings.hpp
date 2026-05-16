#pragma once
#include "tools/ToolParameter.hpp"
#include "BridgeApproachGeometry.hpp"

struct BridgeToolSettings {
    BridgeToolSettings();
    BridgeToolSettings(const BridgeToolSettings&) = delete;
    BridgeToolSettings& operator=(const BridgeToolSettings&) = delete;

    TypedParameter<float> height {
        .value = 275.0f,
        .minValue = 10.0f,
        .maxValue = 2000.0f,
        .step = 5.0f
    };

    TypedParameter<float> grade {
        .value = 12.0f,
        .minValue = 2.0f,
        .maxValue = 50.0f,
        .step = 1.0f
    };

    TypedParameter<int> widthTiles {
        .value = 1,
        .minValue = 1,
        .maxValue = 32,
        .step = 1
    };

    bool showHeightMarkers = true;
    BridgeApproachGeometry::ApproachSideMode sideMode = BridgeApproachGeometry::ApproachSideMode::Both;

    ToolParameterSet parameters;

    void CycleSideMode(int32_t delta);
};
