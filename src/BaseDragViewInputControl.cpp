#pragma once
#include "cISTETerrain.h"
#include "cSC4BaseViewInputControl.h"
#include "cISC4View3DWin.h"
#include "cRZAutoRefCount.h"
#include "cRZBaseString.h"
#include "Logger.h"
#include <memory>
#include <functional>
#include <map>

// Base class for all drag-based terrain tools
class BaseDragViewInputControl : public cSC4BaseViewInputControl {
public:
	// Parameter types that can be adjusted with scroll wheel
	enum class ParameterType {
		Primary,    // Normal scroll (e.g., height)
		Secondary,  // Alt + scroll (e.g., grade)
		Tertiary,   // Ctrl + Alt + scroll (e.g., width)
		Fine,       // Ctrl + scroll (fine adjustment of primary)
		Coarse      // Shift + scroll (coarse adjustment of primary)
	};

	// Parameter definition
	struct Parameter {
		std::string name;
		float value;
		float minValue;
		float maxValue;
		float step;
		std::string unit;

		// Default constructor for std::map compatibility
		Parameter() : name(""), value(0.0f), minValue(0.0f), maxValue(0.0f), step(0.0f), unit("") {}

		Parameter(const std::string& n, float v, float min, float max, float s, const std::string& u = "")
			: name(n), value(v), minValue(min), maxValue(max), step(s), unit(u) {
		}
	};

	// Callbacks for tool-specific behavior
	using DragStartCallback = std::function<void(int32_t tileX, int32_t tileZ)>;
	using DragUpdateCallback = std::function<void(int32_t startX, int32_t startZ, int32_t currentX, int32_t currentZ)>;
	using DragFinishCallback = std::function<void(int32_t startX, int32_t startZ, int32_t endX, int32_t endZ)>;
	using ValidateCallback = std::function<bool(int32_t startX, int32_t startZ, int32_t endX, int32_t endZ, std::string& errorMsg)>;

private:
	cISC4View3DWin* mpView3DWin;
	cIGZWin* mpWindow;
	cISTETerrain* mpTerrain;

	Logger* mpLogger;

	// Drag state
	bool mIsDragging;
	bool mShowingFeedback;
	int32_t mStartTileX, mStartTileZ;
	int32_t mCurrentTileX, mCurrentTileZ;
	int32_t mLastFeedbackX, mLastFeedbackY;

	// Current modifier state (cached from last event)
	uint32_t mCurrentModifiers;

	// Tool configuration
	std::string mToolName;
	std::string mToolDescription;
	std::map<ParameterType, Parameter> mParameters;

	// Callbacks
	DragStartCallback mOnDragStart;
	DragUpdateCallback mOnDragUpdate;
	DragFinishCallback mOnDragFinish;
	ValidateCallback mOnValidate;

	// Visual feedback IDs
	uint32_t mPrimaryTextID;
	uint32_t mSecondaryTextID;

public:
	BaseDragViewInputControl(uint32_t controlID, cISTETerrain* pTerrain, cIGZWin* pWindow, cISC4View3DWin* pView3DWin,
		const std::string& toolName, const std::string& toolDescription)
		: cSC4BaseViewInputControl(controlID)
		, mpView3DWin(pView3DWin)
		, mpWindow(pWindow)
		, mpTerrain(pTerrain)
		, mIsDragging(false)
		, mShowingFeedback(false)
		, mStartTileX(0), mStartTileZ(0)
		, mCurrentTileX(0), mCurrentTileZ(0)
		, mLastFeedbackX(0), mLastFeedbackY(0)
		, mCurrentModifiers(0)
		, mToolName(toolName)
		, mToolDescription(toolDescription)
		, mPrimaryTextID(controlID + 100)
		, mSecondaryTextID(controlID + 101)
	{
		mpLogger = &Logger::GetInstance();
		mpLogger->WriteLineFormatted(LogLevel::Info, "GenericDragViewInputControl created: %s", toolName.c_str());
	}

	// Configuration methods
	void SetParameter(ParameterType type, const Parameter& param) {
		mParameters[type] = param;
	}

	void SetDragStartCallback(DragStartCallback callback) { mOnDragStart = callback; }
	void SetDragUpdateCallback(DragUpdateCallback callback) { mOnDragUpdate = callback; }
	void SetDragFinishCallback(DragFinishCallback callback) { mOnDragFinish = callback; }
	void SetValidateCallback(ValidateCallback callback) { mOnValidate = callback; }

	// Parameter access
	float GetParameterValue(ParameterType type) const {
		auto it = mParameters.find(type);
		return (it != mParameters.end()) ? it->second.value : 0.0f;
	}

	bool SetParameterValue(ParameterType type, float value) {
		auto it = mParameters.find(type);
		if (it != mParameters.end()) {
			float clampedValue = std::max(it->second.minValue,
				std::min(value, it->second.maxValue));
			if (clampedValue != it->second.value) {
				it->second.value = clampedValue;
				return true;
			}
		}
		return false;
	}

	bool Shutdown() override {
		cSC4BaseViewInputControl::Shutdown();

		mpLogger->WriteLineFormatted(LogLevel::Info, "%s drag mode shutdown requested", mToolName.c_str());

		if (mIsDragging) {
			CancelDrag();
		}
		ClearAllVisualFeedback();
		mpLogger->WriteLineFormatted(LogLevel::Info, "%s drag mode shutdown", mToolName.c_str());
		return true;
	}

	bool OnKeyDown(int32_t vkCode, uint32_t modifiers) override {
		mpLogger->WriteLineFormatted(LogLevel::Info, "OnKeyDown: keyCode=0x%X, modifiers=0x%X", vkCode, modifiers);

		mCurrentModifiers = modifiers;
		switch (vkCode) {
			case 0x1B: if (mIsDragging) { CancelDrag(); return true; } break; // ESC
		}
		return false;
	}

	bool OnMouseDownL(int32_t screenX, int32_t screenZ, uint32_t modifiers) override {
		if (!IsOnTop()) return false;
		
		int32_t tileX, tileZ;
		if (ScreenToTileCoordinates(screenX, static_cast<int32_t>(screenZ), tileX, tileZ)) {
			StartDrag(tileX, tileZ, screenX, static_cast<int32_t>(screenZ));
			return true;
		}
		return false;
	}

	bool OnMouseDownR(int32_t x, int32_t z, uint32_t modifiers) override {
		if (!IsOnTop()) return false;
		
		if (mIsDragging) {
			CancelDrag();
			return true;
		}
		return false;
	}

	bool OnMouseUpL(int32_t screenX, int32_t screenZ, uint32_t modifiers) override {
		if (!IsOnTop()) return false;
		
		if (mIsDragging) {
			int32_t tileX, tileZ;
			if (ScreenToTileCoordinates(screenX, static_cast<int32_t>(screenZ), tileX, tileZ)) {
				FinishDrag(tileX, tileZ);
				return true;
			}
		}
		return false;
	}

	bool OnMouseMove(int32_t screenX, int32_t screenZ, uint32_t modifiers) override {
		if (!IsOnTop()) return false;
		
		mCurrentModifiers = modifiers;
		mLastFeedbackX = screenX;
		mLastFeedbackY = screenZ;

		if (mIsDragging) {
			// Only update if we can safely convert coordinates
			int32_t tileX, tileZ;
			if (ScreenToTileCoordinates(screenX, screenZ, tileX, tileZ)) {
				UpdateDrag(tileX, tileZ, screenX, screenZ);
			}
		}
		else {
			ShowParameterFeedback(screenX, screenZ);
		}
		
		return true;
	}

	bool OnMouseWheel(int32_t screenX, int32_t screenZ, uint32_t modifiers, int32_t wheelDelta) override {
		if (!IsOnTop()) return false;
		
		mpLogger->WriteLineFormatted(LogLevel::Info, "OnMouseWheel: wheelDelta=%d, screenX=%d, screenZ=%d, modifiers=%x", wheelDelta, screenX, screenZ, modifiers);

		if (!initialized) return false;
		if (wheelDelta == 0) return false;

		mCurrentModifiers = modifiers;
		ParameterType targetParam = GetScrollTargetParameter(modifiers);
		auto it = mParameters.find(targetParam);

		if (it != mParameters.end()) {
			float adjustment = (wheelDelta > 0 ? 1.0f : -1.0f) * it->second.step;
			float newValue = it->second.value + adjustment;

			if (SetParameterValue(targetParam, newValue)) {
				mpLogger->WriteLineFormatted(LogLevel::Info,
					"%s adjusted to %.2f%s", it->second.name.c_str(),
					it->second.value, it->second.unit.c_str());

				ShowParameterFeedback(screenX, screenZ);
				return true;
			}
		}

		return false;
	}

	bool OnMouseExit() override {
		mpLogger->WriteLineFormatted(LogLevel::Info, "OnMouseExit");

		if (mIsDragging) {
			CancelDrag();
		}
		return true;
	}

private:
	ParameterType GetScrollTargetParameter(uint32_t modifiers) {
		bool ctrlPressed = (modifiers & 0x8) != 0;  // MK_CONTROL
		bool shiftPressed = (modifiers & 0x4) != 0; // MK_SHIFT
		bool altPressed = (modifiers & 0x20) != 0;  // MK_ALT (if supported by SC4)
		
		if (ctrlPressed && altPressed) return ParameterType::Tertiary;
		if (altPressed) return ParameterType::Secondary;
		if (shiftPressed) return ParameterType::Coarse;
		if (ctrlPressed) return ParameterType::Fine;
		return ParameterType::Primary;
	}

	void StartDrag(int32_t tileX, int32_t tileZ, int32_t screenX, int32_t screenY) {
		mIsDragging = true;
		mStartTileX = tileX;
		mStartTileZ = tileZ;
		mCurrentTileX = tileX;
		mCurrentTileZ = tileZ;

		if (mOnDragStart) {
			mOnDragStart(tileX, tileZ);
		}

		ShowDragFeedback(screenX, screenY);
	}

	void UpdateDrag(int32_t tileX, int32_t tileZ, int32_t screenX, int32_t screenY) {
		if (mCurrentTileX != tileX || mCurrentTileZ != tileZ) {
			mCurrentTileX = tileX;
			mCurrentTileZ = tileZ;

			if (mOnDragUpdate) {
				mOnDragUpdate(mStartTileX, mStartTileZ, tileX, tileZ);
			}

			ShowDragFeedback(screenX, screenY);
		}
	}

	void FinishDrag(int32_t tileX, int32_t tileZ) {
		mIsDragging = false;
		ClearDragFeedback();

		// Validate the operation
		std::string errorMsg;
		if (mOnValidate && !mOnValidate(mStartTileX, mStartTileZ, tileX, tileZ, errorMsg)) {
			mpLogger->WriteLineFormatted(LogLevel::Info, "%s validation failed: %s",
				mToolName.c_str(), errorMsg.c_str());
			ShowErrorFeedback(errorMsg);
			return;
		}

		if (mOnDragFinish) {
			mOnDragFinish(mStartTileX, mStartTileZ, tileX, tileZ);
		}

		ShowCompletionFeedback();
	}

	void CancelDrag() {
		mIsDragging = false;
		ClearDragFeedback();
		mpLogger->WriteLineFormatted(LogLevel::Info, "%s drag cancelled", mToolName.c_str());
		ShowParameterFeedback(mLastFeedbackX, mLastFeedbackY);
	}

	// Visual feedback methods
	void ShowInitialFeedback() {
		if (!mpView3DWin) return;

		cRZBaseString titleText;
		titleText.Sprintf("%s Active", mToolName.c_str());

		cRZBaseString instructionText;
		instructionText.Sprintf("%s\nScroll: Adjust parameters | Click and drag to use tool",
			mToolDescription.c_str());

		mpView3DWin->SetCursorText(mSecondaryTextID, 0, &instructionText, &titleText, 0);
		mShowingFeedback = true;
	}

	void ShowParameterFeedback(int32_t screenX, int32_t screenY) {
		if (!mpView3DWin) return;

		cRZBaseString paramText = BuildParameterString();
		cRZBaseString hintText = BuildHintString();

		mpView3DWin->SetCursorText(mPrimaryTextID, 0, &paramText, &hintText, 0);
		mShowingFeedback = true;
	}

	void ShowDragFeedback(int32_t screenX, int32_t screenY) {
		if (!mpView3DWin) return;

		cRZBaseString dragText = BuildDragString();
		cRZBaseString detailText = BuildDragDetailString();

		mpView3DWin->SetCursorText(mPrimaryTextID, 0, &detailText, &dragText, 0);
		
		cRZBaseString cancelText("Release to execute | Right-click to cancel");
		cRZBaseString hintText = BuildHintString();
		mpView3DWin->SetCursorText(mSecondaryTextID, 0, &hintText, &cancelText, 0);
		mShowingFeedback = true;
	}

	void ShowErrorFeedback(const std::string& error) {
		if (!mpView3DWin) return;

		cRZBaseString errorText;
		errorText.Sprintf("%s Error", mToolName.c_str());

		cRZBaseString errorDetailText(error.c_str());
		mpView3DWin->SetCursorText(mPrimaryTextID, 0, &errorText, &errorDetailText, 0);
	}

	void ShowCompletionFeedback() {
		if (!mpView3DWin) return;

		cRZBaseString completionText;
		completionText.Sprintf("%s Completed!", mToolName.c_str());

		cRZBaseString paramText = BuildParameterString();
		mpView3DWin->SetCursorText(mPrimaryTextID, 0, &paramText, &completionText, 0);
	}

	void ClearDragFeedback() {
		if (mpView3DWin) {
			mpView3DWin->ClearCursorText(mSecondaryTextID);
		}
	}

	void ClearAllVisualFeedback() {
		if (mpView3DWin) {
			mpView3DWin->ClearCursorText(mPrimaryTextID);
			mpView3DWin->ClearCursorText(mSecondaryTextID);
		}
		mShowingFeedback = false;
	}

	// String builders
	cRZBaseString BuildParameterString() {
		cRZBaseString result;
		bool first = true;

		for (const auto& pair : mParameters) {
			if (pair.first == ParameterType::Fine || pair.first == ParameterType::Coarse) {
				continue; // Skip fine/coarse as they're just modifiers
			}

			if (!first) result.Append(cRZBaseString(" | "));

			cRZBaseString paramStr;
			paramStr.Sprintf("%s: %.1f%s", pair.second.name.c_str(),
				pair.second.value, pair.second.unit.c_str());
			result.Append(paramStr);
			first = false;
		}

		return result;
	}

	cRZBaseString BuildHintString() {
		ParameterType target = GetScrollTargetParameter(mCurrentModifiers);
		auto it = mParameters.find(target);

		cRZBaseString hint;
		if (it != mParameters.end()) {
			bool ctrlPressed = (mCurrentModifiers & 0x8) != 0;
			bool shiftPressed = (mCurrentModifiers & 0x4) != 0;
			bool altPressed = (mCurrentModifiers & 0x20) != 0;
			
			const char* modifier = "";
			if (ctrlPressed && altPressed) modifier = "Ctrl+Alt+";
			else if (altPressed) modifier = "Alt+";
			else if (shiftPressed) modifier = "Shift+";
			else if (ctrlPressed) modifier = "Ctrl+";

			hint.Sprintf("%sScroll: %s", modifier, it->second.name.c_str());
		}
		else {
			hint.Sprintf("Scroll: Adjust parameters");
		}

		return hint;
	}

	cRZBaseString BuildDragString() {
		int32_t dx = mCurrentTileX - mStartTileX;
		int32_t dz = mCurrentTileZ - mStartTileZ;
		float distance = std::sqrt(static_cast<float>(dx * dx + dz * dz));

		cRZBaseString result;
		result.Sprintf("%s: %.1f tiles | %s", mToolName.c_str(), distance,
			BuildParameterString().Data());

		return result;
	}

	cRZBaseString BuildDragDetailString() {
		cRZBaseString result;
		result.Sprintf("Start: (%d,%d) | End: (%d,%d)",
			mStartTileX, mStartTileZ, mCurrentTileX, mCurrentTileZ);

		return result;
	}

	bool ScreenToTileCoordinates(int32_t screenX, int32_t screenY, int32_t& tileX, int32_t& tileZ) {
		if (!mpView3DWin || !mpTerrain) {
			mpLogger->WriteLineFormatted(LogLevel::Error, "ScreenToTileCoordinates: null pointers - View3D:%p Terrain:%p", mpView3DWin, mpTerrain);
			return false;
		}

		float worldCoords[3] = { 0.0f, 0.0f, 0.0f };
		bool terrainQueryState = mpView3DWin->GetTerrainQueryEnabled();
		
		mpLogger->WriteLineFormatted(LogLevel::Info, "ScreenToTileCoordinates: calling PickTerrain(%d,%d)", screenX, screenY);

		bool pickResult = false;
		pickResult = mpView3DWin->PickTerrain(screenX, screenY, worldCoords, terrainQueryState);

		if (pickResult) {
			tileX = static_cast<int32_t>(worldCoords[0] / 16.0f);
			tileZ = static_cast<int32_t>(worldCoords[2] / 16.0f);

			uint32_t maxX = mpTerrain->CellCountX();
			uint32_t maxZ = mpTerrain->CellCountZ();

			tileX = std::max(0, std::min(tileX, static_cast<int32_t>(maxX - 1)));
			tileZ = std::max(0, std::min(tileZ, static_cast<int32_t>(maxZ - 1)));

			mpLogger->WriteLineFormatted(LogLevel::Info, "ScreenToTileCoordinates: success - tile(%d,%d)", tileX, tileZ);
			return true;
		}

		mpLogger->WriteLineFormatted(LogLevel::Info, "ScreenToTileCoordinates: PickTerrain failed");
		return false;
	}
};