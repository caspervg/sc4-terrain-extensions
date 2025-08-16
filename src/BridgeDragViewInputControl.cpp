#pragma once
#include "BaseDragViewInputControl.cpp"
#include "BridgeApproachTool.cpp"

static constexpr uint32_t kBridgeDragInputControlID = 0xA20FD559;

class BridgeDragViewInputControl : public BaseDragViewInputControl {
private:
	std::unique_ptr<BridgeApproachTool> mpBridgeTool;
	SC4Rect<uint32_t> mLastMarkedRect;
public:
	BridgeDragViewInputControl(cISTETerrain* pTerrain, cIGZWin* pWindow, cISC4View3DWin* pView3DWin)
		: BaseDragViewInputControl(kBridgeDragInputControlID, pTerrain, pWindow, pView3DWin,
			"Bridge Approach Tool",
			"Drag from bridge start to end to create approaches")
	{
		mpBridgeTool = std::make_unique<BridgeApproachTool>(pTerrain);
		mLastMarkedRect = SC4Rect<uint32_t>(0, 0, 0, 0);

		SetCursor(0xa16f1463);

		mpLogger->WriteLineFormatted(LogLevel::Info, "BridgeDragViewInputControl %x : %x : %x", pTerrain, pWindow, pView3DWin);

		SetParameter(ParameterType::First, Parameter("Height", 275.0f, 10.0f, 500.0f, 5.0f, "m"));
		SetParameter(ParameterType::Second, Parameter("Grade", 12.0f, 3.0f, 50.0f, 1.0f, "%"));
		SetParameter(ParameterType::Third, Parameter("Width", 2.0f, 1.0f, 10.0f, 1.0f, " tiles"));

		SetDragStartCallback([this](int32_t startX, int32_t startZ) {
			mpLogger->WriteLineFormatted(LogLevel::Trace, "Drag start: (%d,%d)", startX, startZ);
			this->mLastMarkedRect = SC4Rect<uint32_t>(
				startX, startZ, startX, startZ
			);
			MarkSelected(this->mLastMarkedRect, cISTETerrain::eHilightColorType::Blue, 1);
		});

		SetDragUpdateCallback([this](int32_t startX, int32_t startZ, int32_t currentX, int32_t currentZ) {
			Logger::GetInstance().WriteLineFormatted(LogLevel::Trace, "Drag update: start=(%d,%d), current=(%d,%d)", startX, startZ, currentX, currentZ);

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
			Logger::GetInstance().WriteLineFormatted(LogLevel::Trace, "Drag cancelled: start=(%d,%d), current=(%d,%d)", startX, startZ, currentX, currentZ);
			ClearCurrentSelections();
		});

		SetDragFinishCallback([this](int32_t startX, int32_t startZ, int32_t endX, int32_t endZ) {
			float height = GetParameterValue(ParameterType::First);
			float grade = GetParameterValue(ParameterType::Second);
			float width = GetParameterValue(ParameterType::Third);


			int32_t dx = endX - startX;
			int32_t dz = endZ - startZ;
			float distance = std::sqrt(static_cast<float>(dx * dx + dz * dz));

			if (distance < 3.0f) {
				Logger::GetInstance().WriteLine(LogLevel::Error, "Bridge too short - minimum 3 tiles required");
				return;
			}
			if (dx != 0 && dz != 0) {
				Logger::GetInstance().WriteLine(LogLevel::Error, "Bridge must be drawn horizontally or vertically");
				return;
			}
			if (dx == 0 && dz == 0) {
				Logger::GetInstance().WriteLine(LogLevel::Error, "Start and end points are identical, cannot create bridge");
				return;
			}

			Logger::GetInstance().WriteLineFormatted(LogLevel::Info,
				"Creating bridge approaches from (%d,%d) to (%d,%d), height: %.2f, grade: %.1f%%, width: %.1f tiles",
				startX, startZ, endX, endZ, height, grade, width);
			mpBridgeTool->CreateBridgeApproaches(startX, startZ, endX, endZ,
				height, -1.0f, grade, width);
			ClearCurrentSelections();
			});

		SetValidateCallback([this](int32_t startX, int32_t startZ, int32_t endX, int32_t endZ, std::string& error) {
			if (startX == endX && startZ == endZ) {
				error = "Start and end points are identical";
				return false;
			}

			int32_t dx = endX - startX;
			int32_t dz = endZ - startZ;
			float distance = std::sqrt(static_cast<float>(dx * dx + dz * dz));

			if (distance < 3.0f) {
				error = "Bridge too short - minimum 3 tiles required";
				return false;
			}

			// Ensure bridge is either horizontal or vertical
			if (dx != 0 && dz != 0) {
				error = "Bridge must be drawn horizontally or vertically";
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

		int32_t dx = endX - startX;
		int32_t dz = endZ - startZ;
		float distance = std::sqrt(static_cast<float>(dx * dx + dz * dz));
		if ((dx != 0 && dz != 0) || distance < 3.0f || (dx == 0 && dz == 0)) {
			// Invalid selection: must be horizontal or vertical, and at least 3 tiles long
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