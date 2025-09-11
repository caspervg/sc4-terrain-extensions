#pragma once
#include "BaseDragViewInputControl.cpp"
#include "tools/BridgeApproachTool.cpp"

static constexpr uint32_t kBridgeDragInputControlID = 0xA20FD559;
static constexpr uint32_t kBridgeDragInputCursorID = 0xD9B4FFAA;

class BridgeDragViewInputControl : public BaseDragViewInputControl {
private:
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
		});

		SetDragFinishCallback([this](int32_t startX, int32_t startZ, int32_t endX, int32_t endZ) {
			float height = GetParameterValue(ParameterType::First);
			float grade = GetParameterValue(ParameterType::Second);

			// Calculate drag direction and derive bridge orientation and dimensions
			const int32_t dragDx = endX - startX;
			const int32_t dragDz = endZ - startZ;
			
			if (dragDx == 0 && dragDz == 0) {
				LOG_INFO("Start and end points are identical, cannot create bridge");
				return;
			}

			// Determine bridge orientation from drag direction
			const bool isHorizontalBridge = abs(dragDx) >= abs(dragDz);  // >= handles perfect diagonal -> horizontal
			
			float bridgeLength, bridgeWidth;
			int32_t bridgeStartX, bridgeStartZ, bridgeEndX, bridgeEndZ;
			
			if (isHorizontalBridge) {
				// Bridge runs horizontally (East-West)
				bridgeLength = static_cast<float>(abs(dragDx));
				bridgeWidth = static_cast<float>(abs(dragDz));
				bridgeStartX = std::min(startX, endX);
				bridgeEndX = std::max(startX, endX);
				bridgeStartZ = (startZ + endZ) / 2;  // Center of width
				bridgeEndZ = bridgeStartZ;
			} else {
				// Bridge runs vertically (North-South)
				bridgeLength = static_cast<float>(abs(dragDz));
				bridgeWidth = static_cast<float>(abs(dragDx));
				bridgeStartZ = std::min(startZ, endZ);
				bridgeEndZ = std::max(startZ, endZ);
				bridgeStartX = (startX + endX) / 2;  // Center of width
				bridgeEndX = bridgeStartX;
			}

			if (bridgeLength < 3.0f) {
				LOG_INFO("Bridge too short - minimum 3 tiles required (current: {:.1f} tiles)", bridgeLength);
				return;
			}

			LOG_DEBUG("Creating bridge approaches from ({},{}) to ({},{}), height: {:.2f}, grade: {:.1f}%, width: {:.1f} tiles, orientation: {}",
				bridgeStartX, bridgeStartZ, bridgeEndX, bridgeEndZ, height, grade, bridgeWidth, 
				isHorizontalBridge ? "horizontal" : "vertical");
				
			mpBridgeTool->CreateBridgeApproaches(bridgeStartX, bridgeStartZ, bridgeEndX, bridgeEndZ,
				height, -1.0f, grade, bridgeWidth, false);  // Explicit: no tapering for rectangular drags
			ClearCurrentSelections();
			});

		SetValidateCallback([this](int32_t startX, int32_t startZ, int32_t endX, int32_t endZ, std::string& error) {
			if (startX == endX && startZ == endZ) {
				error = "Start and end points are identical";
				return false;
			}

			// Calculate bridge length from drag direction
			int32_t dragDx = endX - startX;
			int32_t dragDz = endZ - startZ;
			bool isHorizontalBridge = abs(dragDx) >= abs(dragDz);
			float bridgeLength = isHorizontalBridge ? static_cast<float>(abs(dragDx)) : static_cast<float>(abs(dragDz));

			if (bridgeLength < 3.0f) {
				error = "Bridge too short - minimum 3 tiles required";
				return false;
			}

			return true;
			});
	}

private:
	bool IsValidSelection(uint32_t startX, uint32_t startZ, uint32_t endX, uint32_t endZ) {
		if (startX < 0 || startZ < 0 || endX < 0 || endZ < 0 ||
			startX >= mpTerrain->CellCountX() || startZ >= mpTerrain->CellCountZ() ||
			endX >= mpTerrain->CellCountX() || endZ >= mpTerrain->CellCountZ()) {
			return false;
		}

		// Same logic as validation callback: check bridge length from dominant dimension
		int32_t dragDx = endX - startX;
		int32_t dragDz = endZ - startZ;
		
		if (dragDx == 0 && dragDz == 0) {
			return false;  // Start and end are identical
		}
		
		bool isHorizontalBridge = abs(dragDx) >= abs(dragDz);
		float bridgeLength = isHorizontalBridge ? static_cast<float>(abs(dragDx)) : static_cast<float>(abs(dragDz));
		
		if (bridgeLength < 3.0f) {
			return false;  // Bridge must be at least 3 tiles long
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