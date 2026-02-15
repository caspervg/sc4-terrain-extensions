#pragma once
#include "BaseDragViewInputControl.cpp"
#include "tools/BridgeApproachTool.cpp"
#include "viz/BridgeApproachVisualizer.hpp"
#include "viz/BridgeToolPanel.hpp"  // for gShowHeightMarkers, gShowGradeColors, gShowGrid
#include <optional>

static constexpr uint32_t kBridgeDragInputControlID = 0xA20FD559;
static constexpr uint32_t kBridgeDragInputCursorID = 0xD9B4FFAA;

class BridgeDragViewInputControl : public BaseDragViewInputControl {
private:
	struct BridgeDragPlacement {
		bool isHorizontal = true;
		float bridgeLength = 0.0f;
		float bridgeWidth = 1.0f;
		int32_t bridgeStartX = 0;
		int32_t bridgeStartZ = 0;
		int32_t bridgeEndX = 0;
		int32_t bridgeEndZ = 0;
	};

	std::unique_ptr<BridgeApproachTool> mpBridgeTool;
	SC4Rect<uint32_t> mLastMarkedRect;

public:
	BridgeDragViewInputControl(cISTETerrain* pTerrain, cIGZWin* pWindow, cISC4View3DWin* pView3DWin)
		: BaseDragViewInputControl(kBridgeDragInputControlID, kBridgeDragInputCursorID, pTerrain, pWindow, pView3DWin,
			"Bridge Approach Tool",
			"Drag from bridge start to end to create approaches")
	{
		mpBridgeTool = std::make_unique<BridgeApproachTool>(pTerrain);
		mLastMarkedRect = SC4Rect<uint32_t>(0, 0, 0, 0);

		SetParameter(ParameterType::First, Parameter("Height", 275.0f, 10.0f, 500.0f, 5.0f, "m"));
		SetParameter(ParameterType::Second, Parameter("Grade", 12.0f, 3.0f, 50.0f, 1.0f, "%"));

		SetDragStartCallback([this](int32_t startX, int32_t startZ) {
			LOG_TRACE("Drag start: ({}, {})", startX, startZ);
			this->mLastMarkedRect = SC4Rect<uint32_t>(
				startX, startZ, startX, startZ
			);
			MarkSelected(this->mLastMarkedRect, cISTETerrain::eHilightColorType::Blue, 1);
		});

		SetDragUpdateCallback([this](int32_t startX, int32_t startZ, int32_t currentX, int32_t currentZ) {
			LOG_TRACE("Drag update: start=({},{}), current=({},{})", startX, startZ, currentX, currentZ);

			UpdateVisualPreview(startX, startZ, currentX, currentZ);

			this->mLastMarkedRect = SC4Rect<uint32_t>(
				std::min(startX, currentX), std::min(startZ, currentZ),
				std::max(startX, currentX), std::max(startZ, currentZ));
			auto selectionColor = cISTETerrain::eHilightColorType::Green;
			if (!IsValidSelection()) {
				selectionColor = cISTETerrain::eHilightColorType::Red;
			}
			MarkSelected(this->mLastMarkedRect, selectionColor, 1);
		});

		SetDragCancelCallback([this](int32_t startX, int32_t startZ, int32_t currentX, int32_t currentZ) {
			LOG_TRACE("Drag cancelled: start=({},{}), current=({},{})", startX, startZ, currentX, currentZ);
			ClearCurrentSelections();
			gBridgeVisualizer.ClearAll();
		});

		SetDragFinishCallback([this](int32_t startX, int32_t startZ, int32_t endX, int32_t endZ) {
			float height = GetParameterValue(ParameterType::First);
			float grade = GetParameterValue(ParameterType::Second);

			auto placement = ComputeBridgePlacement(startX, startZ, endX, endZ);
			if (!placement.has_value()) {
				LOG_INFO("Start and end points are identical, cannot create bridge");
				return;
			}

			if (placement->bridgeLength < 3.0f) {
				LOG_INFO("Bridge too short - minimum 3 tiles required (current: {:.1f} tiles)", placement->bridgeLength);
				return;
			}

			LOG_DEBUG("Creating bridge approaches from ({},{}) to ({},{}), height: {:.2f}, grade: {:.1f}%, width: {:.1f} tiles, orientation: {}",
				placement->bridgeStartX, placement->bridgeStartZ, placement->bridgeEndX, placement->bridgeEndZ, height, grade, placement->bridgeWidth,
				placement->isHorizontal ? "horizontal" : "vertical");

			mpBridgeTool->CreateBridgeApproaches(placement->bridgeStartX, placement->bridgeStartZ, placement->bridgeEndX, placement->bridgeEndZ,
				height, -1.0f, grade, placement->bridgeWidth, false);
			ClearCurrentSelections();
			gBridgeVisualizer.ClearAll();
		});

		SetValidateCallback([this](int32_t startX, int32_t startZ, int32_t endX, int32_t endZ, std::string& error) {
			if (startX == endX && startZ == endZ) {
				error = "Start and end points are identical";
				return false;
			}

			auto placement = ComputeBridgePlacement(startX, startZ, endX, endZ);
			if (!placement.has_value()) {
				error = "Start and end points are identical";
				return false;
			}

			if (placement->bridgeLength < 3.0f) {
				error = "Bridge too short - minimum 3 tiles required";
				return false;
			}

			return true;
		});
	}

private:
	std::optional<BridgeDragPlacement> ComputeBridgePlacement(int32_t startX, int32_t startZ, int32_t endX, int32_t endZ) const {
		const int32_t dragDx = endX - startX;
		const int32_t dragDz = endZ - startZ;

		if (dragDx == 0 && dragDz == 0) {
			return std::nullopt;
		}

		BridgeDragPlacement placement;
		placement.isHorizontal = abs(dragDx) >= abs(dragDz);

		if (placement.isHorizontal) {
			const int32_t minZ = std::min(startZ, endZ);
			const int32_t maxZ = std::max(startZ, endZ);
			placement.bridgeLength = static_cast<float>(abs(dragDx));
			placement.bridgeWidth = static_cast<float>((maxZ - minZ) + 1);
			placement.bridgeStartX = std::min(startX, endX);
			placement.bridgeEndX = std::max(startX, endX);
			placement.bridgeStartZ = minZ + ((maxZ - minZ) / 2);
			placement.bridgeEndZ = placement.bridgeStartZ;
		}
		else {
			const int32_t minX = std::min(startX, endX);
			const int32_t maxX = std::max(startX, endX);
			placement.bridgeLength = static_cast<float>(abs(dragDz));
			placement.bridgeWidth = static_cast<float>((maxX - minX) + 1);
			placement.bridgeStartZ = std::min(startZ, endZ);
			placement.bridgeEndZ = std::max(startZ, endZ);
			placement.bridgeStartX = minX + ((maxX - minX) / 2);
			placement.bridgeEndX = placement.bridgeStartX;
		}

		return placement;
	}

	void UpdateVisualPreview(int32_t startX, int32_t startZ, int32_t endX, int32_t endZ) {
		float height = GetParameterValue(ParameterType::First);
		float grade = GetParameterValue(ParameterType::Second);
		auto placement = ComputeBridgePlacement(startX, startZ, endX, endZ);
		if (!placement.has_value()) {
			gBridgeVisualizer.ClearAll();
			return;
		}

		float width = placement->bridgeWidth;

		bool isValid = IsValidSelection(startX, startZ, endX, endZ);

		// Always build the approach preview (main surface)
		gBridgeVisualizer.BuildApproachPreview(
			mpTerrain, placement->bridgeStartX, placement->bridgeStartZ, placement->bridgeEndX, placement->bridgeEndZ, height, grade, width, isValid
		);

		// Height markers — use the global panel toggle
		if (gShowHeightMarkers) {
			gBridgeVisualizer.BuildHeightMarkers(
				mpTerrain, placement->bridgeStartX, placement->bridgeStartZ, placement->bridgeEndX, placement->bridgeEndZ, height, grade, width
			);
		}
	}

	bool IsValidSelection(uint32_t startX, uint32_t startZ, uint32_t endX, uint32_t endZ) {
		if (startX >= mpTerrain->CellCountX() || startZ >= mpTerrain->CellCountZ() ||
			endX >= mpTerrain->CellCountX() || endZ >= mpTerrain->CellCountZ()) {
			return false;
		}

		int32_t dragDx = endX - startX;
		int32_t dragDz = endZ - startZ;

		if (dragDx == 0 && dragDz == 0) {
			return false;
		}

		bool isHorizontalBridge = abs(dragDx) >= abs(dragDz);
		float bridgeLength = isHorizontalBridge ? static_cast<float>(abs(dragDx)) : static_cast<float>(abs(dragDz));

		if (bridgeLength < 3.0f) {
			return false;
		}

		return true;
	}

	bool IsValidSelection(SC4Rect<uint32_t> rect) {
		return IsValidSelection(rect.topLeftX, rect.topLeftY, rect.bottomRightX, rect.bottomRightY);
	}

	bool IsValidSelection() {
		return IsValidSelection(mLastMarkedRect);
	}
};
