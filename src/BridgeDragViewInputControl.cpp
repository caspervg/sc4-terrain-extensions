#pragma once
#include "GenericDragViewInputControl.cpp"
#include "BridgeApproachTool.cpp"

static constexpr uint32_t kBridgeDragInputControlID = 0xA20FD559;

class BridgeDragViewInputControl : public GenericDragViewInputControl {
private:
	std::unique_ptr<BridgeApproachTool> mpBridgeTool;

public:
	BridgeDragViewInputControl(cISTETerrain* pTerrain, cIGZWin* pWindow, cISC4View3DWin* pView3DWin)
		: GenericDragViewInputControl(kBridgeDragInputControlID, pTerrain, pWindow, pView3DWin,
			"Bridge Approach Tool",
			"Drag from bridge start to end to create approaches")
	{
		mpBridgeTool = std::make_unique<BridgeApproachTool>(pTerrain);

		Logger& logger = Logger::GetInstance();
		logger.WriteLineFormatted(LogLevel::Info, "BridgeDragViewInputControl %x : %x : %x", pTerrain, pWindow, pView3DWin);

		// Configure parameters
		SetParameter(ParameterType::Primary, Parameter("Height", 50.0f, 10.0f, 500.0f, 2.0f, "m"));
		SetParameter(ParameterType::Fine, Parameter("Height", 50.0f, 10.0f, 500.0f, 0.5f, "m"));
		SetParameter(ParameterType::Coarse, Parameter("Height", 50.0f, 10.0f, 500.0f, 10.0f, "m"));
		SetParameter(ParameterType::Secondary, Parameter("Grade", 6.0f, 1.0f, 25.0f, 0.5f, "%"));
		SetParameter(ParameterType::Tertiary, Parameter("Width", 2.0f, 1.0f, 10.0f, 0.25f, " tiles"));

		// Set up callbacks
		SetDragStartCallback([this](int32_t x, int32_t z) {
			// Could add visual preview here
			});

		SetDragUpdateCallback([this](int32_t startX, int32_t startZ, int32_t currentX, int32_t currentZ) {
			// Could update visual preview here
			});

		SetDragFinishCallback([this](int32_t startX, int32_t startZ, int32_t endX, int32_t endZ) {
			float height = GetParameterValue(ParameterType::Primary);
			float grade = GetParameterValue(ParameterType::Secondary);
			float width = GetParameterValue(ParameterType::Tertiary);

			mpBridgeTool->CreateBridgeApproaches(startX, startZ, endX, endZ,
				height, -1.0f, grade, width);
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

			return true;
			});
	}
};